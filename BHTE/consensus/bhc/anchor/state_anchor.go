// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// State Root Anchoring Module - Cross-Layer State Synchronization
// Enables BHTE to anchor its state roots to BHC (L1) for trustless verification

package anchor

import (
	"context"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"sync"
	"time"

	"github.com/ethereum/go-ethereum/common"
	"github.com/ethereum/go-ethereum/log"
	"github.com/ybbus/jsonrpc/v3"
)

const (
	DefaultAnchorInterval    = 10 * time.Minute
	MinBHCConfirmations      = 6
	ChallengePeriod          = 7 * 24 * time.Hour
	EmergencyChallengePeriod = 24 * time.Hour
	MaxAnchorDelay           = 15 * time.Minute
)

type AnchorStatus uint8

const (
	AnchorStatusPending AnchorStatus = iota
	AnchorStatusSubmitted
	AnchorStatusConfirmed
	AnchorStatusChallenged
	AnchorStatusFinalized
)

func (s AnchorStatus) String() string {
	switch s {
	case AnchorStatusPending:
		return "Pending"
	case AnchorStatusSubmitted:
		return "Submitted"
	case AnchorStatusConfirmed:
		return "Confirmed"
	case AnchorStatusChallenged:
		return "Challenged"
	case AnchorStatusFinalized:
		return "Finalized"
	default:
		return "Unknown"
	}
}

type StateRoot struct {
	BlockHeight uint64
	BlockHash   common.Hash
	StateRoot   common.Hash
	Timestamp   time.Time
	ParentHash  common.Hash
}

type L1Anchor struct {
	AnchorID        common.Hash
	StateRoot       *StateRoot
	Status          AnchorStatus
	BHCHeight       uint64
	BHCTxHash       string
	Confirmations   uint64
	SubmittedAt     time.Time
	ConfirmedAt     time.Time
	ChallengeEndsAt time.Time
	FraudProof      []byte
}

type AnchorSubmission struct {
	StateRoot      common.Hash
	BlockHeight    uint64
	Fee            uint64
	OP_RETURN_Data []byte
}

type AnchorVerifier struct {
	bhcRPCClient jsonrpc.RPCClient
	anchors      map[common.Hash]*L1Anchor
	byHeight     map[uint64][]common.Hash
	stateRoots   map[uint64]*StateRoot
	pendingSubs  map[common.Hash]*AnchorSubmission
	mu           sync.RWMutex
	ctx          context.Context
	cancel       context.CancelFunc
	nodeURL      string
	interval     time.Duration
	isRunning    bool
}

func NewAnchorVerifier(bhcNodeURL string) *AnchorVerifier {
	if bhcNodeURL == "" {
		bhcNodeURL = "http://127.0.0.1:18332"
	}

	ctx, cancel := context.WithCancel(context.Background())

	return &AnchorVerifier{
		bhcRPCClient: jsonrpc.NewClient(bhcNodeURL),
		anchors:      make(map[common.Hash]*L1Anchor),
		byHeight:     make(map[uint64][]common.Hash),
		stateRoots:   make(map[uint64]*StateRoot),
		pendingSubs:  make(map[common.Hash]*AnchorSubmission),
		nodeURL:      bhcNodeURL,
		interval:     DefaultAnchorInterval,
		ctx:          ctx,
		cancel:       cancel,
	}
}

func (v *AnchorVerifier) Start() error {
	if v.isRunning {
		return fmt.Errorf("anchor verifier already running")
	}

	v.isRunning = true

	go v.anchorSyncLoop()
	go v.confirmationChecker()
	go v.challengeChecker()

	log.Info("Anchor verifier started", "node_url", v.nodeURL)

	return nil
}

func (v *AnchorVerifier) Stop() {
	v.cancel()
	v.isRunning = false
	log.Info("Anchor verifier stopped")
}

func (v *AnchorVerifier) RecordStateRoot(stateRoot *StateRoot) error {
	if stateRoot == nil {
		return fmt.Errorf("nil state root")
	}

	v.mu.Lock()
	defer v.mu.Unlock()

	v.stateRoots[stateRoot.BlockHeight] = stateRoot

	return nil
}

func (v *AnchorVerifier) GenerateAnchorSubmission(height uint64, fee uint64) (*AnchorSubmission, error) {
	v.mu.RLock()
	stateRoot, exists := v.stateRoots[height]
	v.mu.RUnlock()

	if !exists {
		return nil, fmt.Errorf("state root not found for height %d", height)
	}

	opReturnData := v.buildOPReturnData(stateRoot)

	submission := &AnchorSubmission{
		StateRoot:      stateRoot.StateRoot,
		BlockHeight:    height,
		Fee:            fee,
		OP_RETURN_Data: opReturnData,
	}

	v.mu.Lock()
	v.pendingSubs[stateRoot.StateRoot] = submission
	v.mu.Unlock()

	return submission, nil
}

func (v *AnchorVerifier) buildOPReturnData(stateRoot *StateRoot) []byte {
	data := make([]byte, 0)

	data = append(data, []byte("BHTE")...)

	version := byte(1)
	data = append(data, version)

	heightBytes := make([]byte, 8)
	binary.BigEndian.PutUint64(heightBytes, stateRoot.BlockHeight)
	data = append(data, heightBytes...)

	data = append(data, stateRoot.StateRoot.Bytes()...)

	blockHashBytes := stateRoot.BlockHash.Bytes()
	data = append(data, blockHashBytes[:16]...)

	return data
}

func (v *AnchorVerifier) SubmitAnchorToL1(submission *AnchorSubmission, coinbaseAddr string) (string, error) {
	if submission == nil {
		return "", fmt.Errorf("nil submission")
	}

	opReturnHex := hex.EncodeToString(submission.OP_RETURN_Data)

	var txHash string
	err := v.bhcRPCClient.CallFor(v.ctx, &txHash, "sendrawtransaction", map[string]interface{}{
		"coinbase":  coinbaseAddr,
		"op_return": opReturnHex,
	})

	if err != nil {
		return "", fmt.Errorf("failed to submit anchor: %w", err)
	}

	stateRootHash := submission.StateRoot

	v.mu.Lock()
	if _, exists := v.pendingSubs[stateRootHash]; exists {
		v.anchors[stateRootHash] = &L1Anchor{
			AnchorID:    stateRootHash,
			StateRoot:   v.stateRoots[submission.BlockHeight],
			Status:      AnchorStatusSubmitted,
			BHCTxHash:   txHash,
			SubmittedAt: time.Now(),
		}
		v.byHeight[submission.BlockHeight] = append(v.byHeight[submission.BlockHeight], stateRootHash)
		delete(v.pendingSubs, stateRootHash)
	}
	v.mu.Unlock()

	log.Info("Anchor submitted to BHC", "tx_hash", txHash, "state_root", stateRootHash.Hex())

	return txHash, nil
}

func (v *AnchorVerifier) GetAnchorForStateRoot(stateRoot common.Hash) (*L1Anchor, bool) {
	v.mu.RLock()
	defer v.mu.RUnlock()

	anchor, exists := v.anchors[stateRoot]
	return anchor, exists
}

func (v *AnchorVerifier) GetLatestConfirmedAnchor() (*L1Anchor, error) {
	v.mu.RLock()
	defer v.mu.RUnlock()

	var latest *L1Anchor
	var latestHeight uint64

	for _, anchor := range v.anchors {
		if anchor.Status == AnchorStatusConfirmed || anchor.Status == AnchorStatusFinalized {
			if anchor.StateRoot != nil && anchor.StateRoot.BlockHeight > latestHeight {
				latestHeight = anchor.StateRoot.BlockHeight
				latest = anchor
			}
		}
	}

	if latest == nil {
		return nil, fmt.Errorf("no confirmed anchors found")
	}

	return latest, nil
}

func (v *AnchorVerifier) GetAnchorByHeight(height uint64) ([]*L1Anchor, error) {
	v.mu.RLock()
	defer v.mu.RUnlock()

	anchorHashes, exists := v.byHeight[height]
	if !exists {
		return nil, fmt.Errorf("no anchors at height %d", height)
	}

	anchors := make([]*L1Anchor, 0, len(anchorHashes))
	for _, hash := range anchorHashes {
		if anchor, exists := v.anchors[hash]; exists {
			anchors = append(anchors, anchor)
		}
	}

	return anchors, nil
}

func (v *AnchorVerifier) VerifyAnchorProof(stateRoot common.Hash, merkleProof []common.Hash, leafIndex uint64) (bool, error) {
	v.mu.RLock()
	anchor, exists := v.anchors[stateRoot]
	v.mu.RUnlock()

	if !exists {
		return false, fmt.Errorf("anchor not found for state root")
	}

	if anchor.Status != AnchorStatusConfirmed && anchor.Status != AnchorStatusFinalized {
		return false, fmt.Errorf("anchor not yet confirmed")
	}

	header, err := v.getBHCBlockHeader(anchor.BHCHeight)
	if err != nil {
		return false, err
	}

	computedMerkleRoot := v.computeMerkleRoot(merkleProof, stateRoot, leafIndex)
	actualMerkleRoot := header.MerkleRoot

	return computedMerkleRoot == actualMerkleRoot, nil
}

func (v *AnchorVerifier) computeMerkleRoot(proof []common.Hash, leaf common.Hash, leafIndex uint64) string {
	current := leaf.Bytes()

	for i, sibling := range proof {
		if ((leafIndex >> i) & 1) == 0 {
			current = v.concatHash(current, sibling.Bytes())
		} else {
			current = v.concatHash(sibling.Bytes(), current)
		}
	}

	return hex.EncodeToString(current)
}

func (v *AnchorVerifier) concatHash(left, right []byte) []byte {
	combined := make([]byte, 0, len(left)+len(right))
	combined = append(combined, left...)
	combined = append(combined, right...)
	return v.sha256d(combined)
}

func (v *AnchorVerifier) sha256d(data []byte) []byte {
	h1 := sha256.Sum256(data)
	h2 := sha256.Sum256(h1[:])
	return h2[:]
}

func (v *AnchorVerifier) ChallengeAnchor(stateRoot common.Hash, fraudProof []byte) error {
	v.mu.Lock()
	defer v.mu.Unlock()

	anchor, exists := v.anchors[stateRoot]
	if !exists {
		return fmt.Errorf("anchor not found")
	}

	if anchor.Status == AnchorStatusFinalized {
		return fmt.Errorf("anchor already finalized")
	}

	anchor.Status = AnchorStatusChallenged
	anchor.FraudProof = fraudProof
	anchor.ChallengeEndsAt = time.Now().Add(ChallengePeriod)

	log.Warn("Anchor challenged", "state_root", stateRoot.Hex(), "challenge_ends", anchor.ChallengeEndsAt)

	return nil
}

func (v *AnchorVerifier) FinalizeAnchor(stateRoot common.Hash) error {
	v.mu.Lock()
	defer v.mu.Unlock()

	anchor, exists := v.anchors[stateRoot]
	if !exists {
		return fmt.Errorf("anchor not found")
	}

	if anchor.Status == AnchorStatusChallenged {
		if time.Now().Before(anchor.ChallengeEndsAt) {
			return fmt.Errorf("challenge period not yet ended")
		}
	}

	anchor.Status = AnchorStatusFinalized
	anchor.ConfirmedAt = time.Now()

	log.Info("Anchor finalized", "state_root", stateRoot.Hex())

	return nil
}

func (v *AnchorVerifier) anchorSyncLoop() {
	ticker := time.NewTicker(v.interval)
	defer ticker.Stop()

	for {
		select {
		case <-v.ctx.Done():
			return
		case <-ticker.C:
			v.syncPendingAnchors()
		}
	}
}

func (v *AnchorVerifier) syncPendingAnchors() {
	v.mu.RLock()
	pendingCount := len(v.pendingSubs)
	v.mu.RUnlock()

	if pendingCount == 0 {
		return
	}

	log.Debug("Syncing pending anchors", "count", pendingCount)
}

func (v *AnchorVerifier) confirmationChecker() {
	ticker := time.NewTicker(1 * time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-v.ctx.Done():
			return
		case <-ticker.C:
			v.checkConfirmations()
		}
	}
}

func (v *AnchorVerifier) checkConfirmations() {
	v.mu.Lock()
	defer v.mu.Unlock()

	for _, anchor := range v.anchors {
		if anchor.Status != AnchorStatusSubmitted {
			continue
		}

		confirmations, err := v.getTxConfirmations(anchor.BHCTxHash)
		if err != nil {
			log.Debug("Failed to get confirmations", "tx", anchor.BHCTxHash, "error", err)
			continue
		}

		anchor.Confirmations = confirmations

		if confirmations >= MinBHCConfirmations {
			anchor.Status = AnchorStatusConfirmed
			log.Info("Anchor confirmed", "state_root", anchor.AnchorID.Hex(), "confirmations", confirmations)
		}
	}
}

func (v *AnchorVerifier) challengeChecker() {
	ticker := time.NewTicker(5 * time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-v.ctx.Done():
			return
		case <-ticker.C:
			v.checkChallenges()
		}
	}
}

func (v *AnchorVerifier) checkChallenges() {
	v.mu.Lock()
	defer v.mu.Unlock()

	now := time.Now()

	for _, anchor := range v.anchors {
		if anchor.Status == AnchorStatusChallenged && now.After(anchor.ChallengeEndsAt) {
			log.Warn("Challenge period ended without resolution", "state_root", anchor.AnchorID.Hex())
		}
	}
}

func (v *AnchorVerifier) getBHCBlockHeader(height uint64) (*BHCBlockHeader, error) {
	var header BHCBlockHeader
	err := v.bhcRPCClient.CallFor(v.ctx, &header, "getblockheader", height)
	if err != nil {
		return nil, fmt.Errorf("failed to get BHC block header: %w", err)
	}
	return &header, nil
}

func (v *AnchorVerifier) getTxConfirmations(txHash string) (uint64, error) {
	var result struct {
		Confirmations uint64 `json:"confirmations"`
	}

	err := v.bhcRPCClient.CallFor(v.ctx, &result, "gettransaction", txHash)
	if err != nil {
		return 0, err
	}

	return result.Confirmations, nil
}

func (v *AnchorVerifier) GetStats() *AnchorStats {
	v.mu.RLock()
	defer v.mu.RUnlock()

	var pending, submitted, confirmed, challenged, finalized uint64

	for _, anchor := range v.anchors {
		switch anchor.Status {
		case AnchorStatusPending:
			pending++
		case AnchorStatusSubmitted:
			submitted++
		case AnchorStatusConfirmed:
			confirmed++
		case AnchorStatusChallenged:
			challenged++
		case AnchorStatusFinalized:
			finalized++
		}
	}

	return &AnchorStats{
		TotalAnchors: uint64(len(v.anchors)),
		Pending:      pending,
		Submitted:    submitted,
		Confirmed:    confirmed,
		Challenged:   challenged,
		Finalized:    finalized,
		IsRunning:    v.isRunning,
	}
}

type BHCBlockHeader struct {
	Hash          string `json:"hash"`
	Height        uint64 `json:"height"`
	MerkleRoot    string `json:"merkleroot"`
	Time          int64  `json:"time"`
	Confirmations uint64 `json:"confirmations"`
}

type AnchorStats struct {
	TotalAnchors uint64 `json:"totalAnchors"`
	Pending      uint64 `json:"pending"`
	Submitted    uint64 `json:"submitted"`
	Confirmed    uint64 `json:"confirmed"`
	Challenged   uint64 `json:"challenged"`
	Finalized    uint64 `json:"finalized"`
	IsRunning    bool   `json:"isRunning"`
}
