// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// BHC Anchor Verifier for BHTE Layer 2

package bhc

import (
	"context"
	"encoding/json"
	"fmt"
	"sync"
	"time"

	"github.com/ethereum/go-ethereum/common"
	"github.com/ybbus/jsonrpc/v3"
)

const (
	AnchorCheckInterval = 10 * time.Minute
	MinConfirmations    = 3
	MaxAnchorLatency    = 24 * time.Hour
)

type AnchorRecord struct {
	BlockHeight uint64
	StateRoot   common.Hash
	TxHash      common.Hash
	Timestamp   time.Time
	Verified    bool
}

type BHCBlockHeader struct {
	Hash         string `json:"hash"`
	Height       uint64 `json:"height"`
	PreviousHash string `json:"previousblockhash"`
	Time         int64  `json:"time"`
	Bits         uint32 `json:"bits"`
	Nonce        uint64 `json:"nonce"`
	MerkleRoot   string `json:"merkleroot"`
}

type AnchorVerifier struct {
	bhcRPCClient jsonrpc.RPCClient
	nodeURL      string
	anchors      map[uint64]*AnchorRecord
	latestHeight uint64
	mu           sync.RWMutex
	ctx          context.Context
	cancel       context.CancelFunc
}

func NewAnchorVerifier(bhcNodeURL string) *AnchorVerifier {
	if bhcNodeURL == "" {
		bhcNodeURL = "http://127.0.0.1:18332"
	}

	ctx, cancel := context.WithCancel(context.Background())

	return &AnchorVerifier{
		bhcRPCClient: jsonrpc.NewClient(bhcNodeURL),
		nodeURL:       bhcNodeURL,
		latestHeight:  0,
		anchors:       make(map[uint64]*AnchorRecord),
		ctx:          ctx,
		cancel:       cancel,
	}
}

func (v *AnchorVerifier) Start() {
	go v.syncAnchors()
	go v.checkAnchors()
}

func (v *AnchorVerifier) Stop() {
	v.cancel()
}

func (v *AnchorVerifier) GetLatestHeight() (uint64, error) {
	var result struct {
		Height uint64 `json:"height"`
	}

	err := v.bhcRPCClient.CallFor(v.ctx, &result, "getblockcount")
	if err != nil {
		return 0, fmt.Errorf("failed to get block count: %w", err)
	}

	return result.Height, nil
}

func (v *AnchorVerifier) GetBlockHeader(height uint64) (*BHCBlockHeader, error) {
	var header BHCBlockHeader

	err := v.bhcRPCClient.CallFor(v.ctx, &header, "getblockheader", height)
	if err != nil {
		return nil, fmt.Errorf("failed to get block header: %w", err)
	}

	return &header, nil
}

func (v *AnchorVerifier) GetBlockHeaderByHash(hash string) (*BHCBlockHeader, error) {
	var header BHCBlockHeader

	err := v.bhcRPCClient.CallFor(v.ctx, &header, "getblockheader", hash)
	if err != nil {
		return nil, fmt.Errorf("failed to get block header: %w", err)
	}

	return &header, nil
}

func (v *AnchorVerifier) VerifyAnchor(stateRoot common.Hash, blockHeight uint64) (bool, error) {
	v.mu.RLock()
	defer v.mu.RUnlock()

	anchor, exists := v.anchors[blockHeight]
	if !exists {
		return false, fmt.Errorf("no anchor at height %d", blockHeight)
	}

	if !anchor.Verified {
		return false, fmt.Errorf("anchor at height %d not verified", blockHeight)
	}

	latest := v.latestHeight
	if latest-anchor.BlockHeight < MinConfirmations {
		return false, fmt.Errorf("insufficient confirmations: have %d, need %d", latest-anchor.BlockHeight, MinConfirmations)
	}

	return anchor.StateRoot == stateRoot, nil
}

func (v *AnchorVerifier) GetAnchor(blockHeight uint64) (*AnchorRecord, error) {
	v.mu.RLock()
	defer v.mu.RUnlock()

	anchor, exists := v.anchors[blockHeight]
	if !exists {
		return nil, fmt.Errorf("no anchor at height %d", blockHeight)
	}

	return anchor, nil
}

func (v *AnchorVerifier) syncAnchors() {
	ticker := time.NewTicker(AnchorCheckInterval)
	defer ticker.Stop()

	for {
		select {
		case <-v.ctx.Done():
			return
		case <-ticker.C:
			v.fetchLatestAnchors()
		}
	}
}

func (v *AnchorVerifier) fetchLatestAnchors() {
	height, err := v.GetLatestHeight()
	if err != nil {
		fmt.Printf("failed to get latest height: %v\n", err)
		return
	}

	v.mu.Lock()
	v.latestHeight = height
	v.mu.Unlock()

	startHeight := uint64(0)
	v.mu.RLock()
	if v.latestHeight > 0 {
		startHeight = v.latestHeight - MinConfirmations
	}
	v.mu.RUnlock()

	for h := startHeight; h <= height; h++ {
		v.fetchAnchorAtHeight(h)
	}
}

func (v *AnchorVerifier) fetchAnchorAtHeight(height uint64) error {
	header, err := v.GetBlockHeader(height)
	if err != nil {
		return err
	}

	anchor := v.extractAnchorFromHeader(header)

	v.mu.Lock()
	v.anchors[height] = anchor
	v.mu.Unlock()

	return nil
}

func (v *AnchorVerifier) extractAnchorFromHeader(header *BHCBlockHeader) *AnchorRecord {
	if header == nil {
		return nil
	}

	stateRoot := common.HexToHash(header.MerkleRoot)
	txHash := common.HexToHash(header.Hash)

	return &AnchorRecord{
		BlockHeight: header.Height,
		StateRoot:   stateRoot,
		TxHash:      txHash,
		Timestamp:   time.Unix(header.Time, 0),
		Verified:    true,
	}
}

func (v *AnchorVerifier) checkAnchors() {
	ticker := time.NewTicker(AnchorCheckInterval)
	defer ticker.Stop()

	for {
		select {
		case <-v.ctx.Done():
			return
		case <-ticker.C:
			v.verifyRecentAnchors()
		}
	}
}

func (v *AnchorVerifier) verifyRecentAnchors() {
	v.mu.RLock()
	height := v.latestHeight
	v.mu.RUnlock()

	if height < MinConfirmations {
		return
	}

	for h := height - MinConfirmations; h <= height; h++ {
		v.mu.RLock()
		anchor, exists := v.anchors[h]
		v.mu.RUnlock()

		if !exists {
			continue
		}

		header, err := v.GetBlockHeader(h)
		if err != nil {
			continue
		}

		if header.MerkleRoot != anchor.StateRoot.Hex() {
			v.mu.Lock()
			anchor.Verified = false
			v.mu.Unlock()
		}
	}
}

func (v *AnchorVerifier) GetLatestAnchor() (*AnchorRecord, error) {
	v.mu.RLock()
	defer v.mu.RUnlock()

	if len(v.anchors) == 0 {
		return nil, fmt.Errorf("no anchors available")
	}

	var latest *AnchorRecord
	var latestHeight uint64

	for height, anchor := range v.anchors {
		if height > latestHeight {
			latestHeight = height
			latest = anchor
		}
	}

	return latest, nil
}

func (v *AnchorVerifier) WaitForAnchor(expectedRoot common.Hash, timeout time.Duration) (*AnchorRecord, error) {
	ctx, cancel := context.WithTimeout(v.ctx, timeout)
	defer cancel()

	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return nil, fmt.Errorf("timeout waiting for anchor")
		case <-ticker.C:
			v.mu.RLock()
			for _, anchor := range v.anchors {
				if anchor.StateRoot == expectedRoot {
					v.mu.RUnlock()
					return anchor, nil
				}
			}
			v.mu.RUnlock()
		}
	}
}

func (v *AnchorVerifier) MarshalJSON() ([]byte, error) {
	v.mu.RLock()
	defer v.mu.RUnlock()

	type AnchorRecordJSON struct {
		BlockHeight uint64 `json:"blockHeight"`
		StateRoot   string `json:"stateRoot"`
		TxHash      string `json:"txHash"`
		Timestamp   int64  `json:"timestamp"`
		Verified    bool   `json:"verified"`
	}

	records := make(map[string]AnchorRecordJSON)
	for height, anchor := range v.anchors {
		records[fmt.Sprintf("%d", height)] = AnchorRecordJSON{
			BlockHeight: anchor.BlockHeight,
			StateRoot:   anchor.StateRoot.Hex(),
			TxHash:      anchor.TxHash.Hex(),
			Timestamp:   anchor.Timestamp.Unix(),
			Verified:    anchor.Verified,
		}
	}

	return json.Marshal(records)
}
