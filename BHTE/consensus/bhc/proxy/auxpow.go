// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// BHTE-Proxy: AuxPoW (Auxiliary Proof of Work) Module for Merge Mining
// This module enables BHTE to leverage BHC's mining infrastructure

package proxy

import (
	"context"
	"crypto/sha256"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"sync"
	"time"

	"github.com/ethereum/go-ethereum/common"
	"github.com/ybbus/jsonrpc/v3"
)

const (
	AuxPoWCheckInterval = 10 * time.Second
	MaxAuxChainDepth    = 144
	MinBHCConfirmations = 6
)

type AuxPoWData struct {
	BlockHash    common.Hash   `json:"block_hash"`
	ParentHash   common.Hash   `json:"parent_hash"`
	Coinbase     []byte        `json:"coinbase"`
	MerkleBranch []common.Hash `json:"merkle_branch"`
	ChainIndex   uint32        `json:"chain_index"`
	ProofBits    []byte        `json:"proof_bits"`
	BHCHeight    uint64        `json:"bhc_height"`
	Timestamp    time.Time     `json:"timestamp"`
}

type BHCAuxBlock struct {
	Hash          common.Hash `json:"hash"`
	Height        uint64      `json:"height"`
	Confirmations uint64      `json:"confirmations"`
	Coinbase      []byte      `json:"coinbase"`
	IsAuxBlock    bool        `json:"is_aux_block"`
}

type AuxPoWVerifier struct {
	bhcRPCClient    jsonrpc.RPCClient
	auxBlocks       map[common.Hash]*AuxPoWData
	validatedBlocks map[common.Hash]bool
	mu              sync.RWMutex
	ctx             context.Context
	cancel          context.CancelFunc
	nodeURL         string
}

func NewAuxPoWVerifier(bhcNodeURL string) *AuxPoWVerifier {
	if bhcNodeURL == "" {
		bhcNodeURL = "http://127.0.0.1:18332"
	}

	ctx, cancel := context.WithCancel(context.Background())

	return &AuxPoWVerifier{
		bhcRPCClient:    jsonrpc.NewClient(bhcNodeURL),
		auxBlocks:       make(map[common.Hash]*AuxPoWData),
		validatedBlocks: make(map[common.Hash]bool),
		nodeURL:         bhcNodeURL,
		ctx:             ctx,
		cancel:          cancel,
	}
}

func (v *AuxPoWVerifier) Start() error {
	go v.watchBHCBlocks()
	go v.cleanupOldAuxBlocks()
	return nil
}

func (v *AuxPoWVerifier) Stop() {
	v.cancel()
}

func (v *AuxPoWVerifier) GetLatestBHCHeight() (uint64, error) {
	var result struct {
		Height uint64 `json:"height"`
	}

	err := v.bhcRPCClient.CallFor(v.ctx, &result, "getblockcount")
	if err != nil {
		return 0, fmt.Errorf("failed to get BHC block count: %w", err)
	}

	return result.Height, nil
}

func (v *AuxPoWVerifier) GetBHCBlockHash(height uint64) (common.Hash, error) {
	var result struct {
		Hash string `json:"hash"`
	}

	err := v.bhcRPCClient.CallFor(v.ctx, &result, "getblockhash", height)
	if err != nil {
		return common.Hash{}, fmt.Errorf("failed to get block hash: %w", err)
	}

	return common.HexToHash(result.Hash), nil
}

func (v *AuxPoWVerifier) GetAuxPoWData(auxBlockHash common.Hash) (*AuxPoWData, error) {
	var result struct {
		Hash          string `json:"hash"`
		Height        uint64 `json:"height"`
		Confirmations uint64 `json:"confirmations"`
		Coinbase      string `json:"coinbase"`
		ChainIndex    uint32 `json:"chain_index"`
		ProofBits     string `json:"proof_bits"`
	}

	err := v.bhcRPCClient.CallFor(v.ctx, &result, "getauxblock", auxBlockHash.Hex())
	if err != nil {
		return nil, fmt.Errorf("failed to get AuxPoW data: %w", err)
	}

	coinbase, err := hexToBytes(result.Coinbase)
	if err != nil {
		return nil, fmt.Errorf("failed to decode coinbase: %w", err)
	}

	proofBits, err := hexToBytes(result.ProofBits)
	if err != nil {
		return nil, fmt.Errorf("failed to decode proof bits: %w", err)
	}

	return &AuxPoWData{
		BlockHash:    common.HexToHash(result.Hash),
		ParentHash:   common.Hash{},
		Coinbase:     coinbase,
		MerkleBranch: []common.Hash{},
		ChainIndex:   result.ChainIndex,
		ProofBits:    proofBits,
		BHCHeight:    result.Height,
		Timestamp:    time.Now(),
	}, nil
}

func (v *AuxPoWVerifier) GetAuxBlocks() ([]*AuxPoWData, error) {
	var result []struct {
		Hash          string `json:"hash"`
		Height        uint64 `json:"height"`
		Confirmations uint64 `json:"confirmations"`
		Coinbase      string `json:"coinbase"`
	}

	err := v.bhcRPCClient.CallFor(v.ctx, &result, "getauxblocks")
	if err != nil {
		return nil, fmt.Errorf("failed to get aux blocks: %w", err)
	}

	auxBlocks := make([]*AuxPoWData, 0, len(result))
	for _, block := range result {
		coinbase, _ := hexToBytes(block.Coinbase)
		auxBlocks = append(auxBlocks, &AuxPoWData{
			BlockHash: common.HexToHash(block.Hash),
			BHCHeight: block.Height,
			Coinbase:  coinbase,
			Timestamp: time.Now(),
		})
	}

	return auxBlocks, nil
}

func (v *AuxPoWVerifier) SubmitAuxBlock(auxBlock *AuxPoWData) (common.Hash, error) {
	coinbaseHex := bytesToHex(auxBlock.Coinbase)
	proofBitsHex := bytesToHex(auxBlock.ProofBits)

	var result struct {
		Hash       string `json:"hash"`
		ParentHash string `json:"parent_hash"`
	}

	err := v.bhcRPCClient.CallFor(v.ctx, &result, "createauxblock", coinbaseHex)
	if err != nil {
		return common.Hash{}, fmt.Errorf("failed to create aux block: %w", err)
	}

	_, err = v.bhcRPCClient.Call(v.ctx, "auxproof", map[string]interface{}{
		"block_hash":  result.Hash,
		"proof_bits":  proofBitsHex,
		"chain_index": auxBlock.ChainIndex,
	})
	if err != nil {
		return common.Hash{}, fmt.Errorf("failed to submit aux proof: %w", err)
	}

	return common.HexToHash(result.Hash), nil
}

func (v *AuxPoWVerifier) VerifyAuxPoW(auxData *AuxPoWData) (bool, error) {
	v.mu.RLock()
	if v.validatedBlocks[auxData.BlockHash] {
		v.mu.RUnlock()
		return true, nil
	}
	v.mu.RUnlock()

	if auxData.BHCHeight < MinBHCConfirmations {
		return false, fmt.Errorf("insufficient BHC confirmations: need %d, got %d",
			MinBHCConfirmations, auxData.BHCHeight)
	}

	currentHeight, err := v.GetLatestBHCHeight()
	if err != nil {
		return false, err
	}

	if currentHeight-auxData.BHCHeight < MinBHCConfirmations {
		return false, fmt.Errorf("BHC block not yet confirmed: height %d, current %d",
			auxData.BHCHeight, currentHeight)
	}

	blockHash, err := v.GetBHCBlockHash(auxData.BHCHeight)
	if err != nil {
		return false, err
	}

	if blockHash != auxData.ParentHash && auxData.ParentHash != (common.Hash{}) {
		return false, fmt.Errorf("parent hash mismatch")
	}

	if err := v.verifyCoinbase(auxData); err != nil {
		return false, fmt.Errorf("coinbase verification failed: %w", err)
	}

	if err := v.verifyMerkleBranch(auxData); err != nil {
		return false, fmt.Errorf("merkle branch verification failed: %w", err)
	}

	v.mu.Lock()
	v.validatedBlocks[auxData.BlockHash] = true
	v.auxBlocks[auxData.BlockHash] = auxData
	v.mu.Unlock()

	return true, nil
}

func (v *AuxPoWVerifier) verifyCoinbase(auxData *AuxPoWData) error {
	if len(auxData.Coinbase) == 0 {
		return fmt.Errorf("empty coinbase")
	}

	if len(auxData.Coinbase) < 20 {
		return fmt.Errorf("coinbase too short")
	}

	if len(auxData.MerkleBranch) == 0 && len(auxData.ProofBits) == 0 {
		return fmt.Errorf("missing merkle proof data")
	}

	return nil
}

func (v *AuxPoWVerifier) verifyMerkleBranch(auxData *AuxPoWData) error {
	if len(auxData.MerkleBranch) == 0 {
		return fmt.Errorf("empty merkle branch")
	}

	firstHash := sha256.Sum256(auxData.Coinbase)
	coinbaseHash := sha256.Sum256(firstHash[:])

	currentHash := coinbaseHash
	for i, branchHash := range auxData.MerkleBranch {
		if i%2 == 0 {
			combined := make([]byte, 64)
			copy(combined[:32], currentHash[:])
			copy(combined[32:], branchHash[:])
			currentHash = sha256.Sum256(combined)
		} else {
			combined := make([]byte, 64)
			copy(combined[:32], branchHash[:])
			copy(combined[32:], currentHash[:])
			currentHash = sha256.Sum256(combined)
		}
	}

	if auxData.ChainIndex != 0 && len(auxData.ProofBits) > 0 {
		if !v.checkProofBits(currentHash[:], auxData.ProofBits) {
			return fmt.Errorf("proof bits verification failed")
		}
	}

	return nil
}

func (v *AuxPoWVerifier) checkProofBits(hash []byte, proofBits []byte) bool {
	if len(proofBits) < 4 {
		return false
	}

	targetDifficulty := binary.BigEndian.Uint32(proofBits[:4])

	diffBits := binary.BigEndian.Uint32(hash[:4])

	return diffBits <= targetDifficulty
}

func (v *AuxPoWVerifier) GetAuxBlockForBHTTBlock(bhcBlockHash common.Hash) (*AuxPoWData, error) {
	auxBlocks, err := v.GetAuxBlocks()
	if err != nil {
		return nil, err
	}

	for _, aux := range auxBlocks {
		if _, err := v.GetBHCBlockHash(aux.BHCHeight); err != nil {
			continue
		}

		auxParentResponse, err := v.getAuxParent(aux.BlockHash)
		if err != nil {
			continue
		}

		if common.HexToHash(auxParentResponse) == bhcBlockHash {
			return aux, nil
		}
	}

	return nil, fmt.Errorf("no aux block found for BHC block %s", bhcBlockHash.Hex())
}

func (v *AuxPoWVerifier) getAuxParent(auxHash common.Hash) (string, error) {
	var result struct {
		ParentHash string `json:"parent_hash"`
	}

	err := v.bhcRPCClient.CallFor(v.ctx, &result, "getauxblock", auxHash.Hex())
	if err != nil {
		return "", err
	}

	return result.ParentHash, nil
}

func (v *AuxPoWVerifier) watchBHCBlocks() {
	ticker := time.NewTicker(AuxPoWCheckInterval)
	defer ticker.Stop()

	var lastHeight uint64

	for {
		select {
		case <-v.ctx.Done():
			return
		case <-ticker.C:
			height, err := v.GetLatestBHCHeight()
			if err != nil {
				continue
			}

			if height > lastHeight {
				lastHeight = height
				v.processNewBHCBlock(height)
			}
		}
	}
}

func (v *AuxPoWVerifier) processNewBHCBlock(height uint64) {
	if _, err := v.GetBHCBlockHash(height); err != nil {
		return
	}

	v.mu.Lock()
	defer v.mu.Unlock()

	v.cleanupOldBlocksLocked(height)
}

func (v *AuxPoWVerifier) cleanupOldBlocksLocked(currentHeight uint64) {
	for hash, data := range v.auxBlocks {
		if currentHeight-data.BHCHeight > MaxAuxChainDepth {
			delete(v.auxBlocks, hash)
			delete(v.validatedBlocks, hash)
		}
	}
}

func (v *AuxPoWVerifier) cleanupOldAuxBlocks() {
	ticker := time.NewTicker(5 * time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-v.ctx.Done():
			return
		case <-ticker.C:
			v.mu.Lock()
			height, _ := v.GetLatestBHCHeight()
			v.cleanupOldBlocksLocked(height)
			v.mu.Unlock()
		}
	}
}

func (v *AuxPoWVerifier) GetValidatedAuxBlock(hash common.Hash) (*AuxPoWData, bool) {
	v.mu.RLock()
	defer v.mu.RUnlock()

	data, ok := v.auxBlocks[hash]
	return data, ok
}

func (v *AuxPoWVerifier) IsAuxBlockValidated(hash common.Hash) bool {
	v.mu.RLock()
	defer v.mu.RUnlock()

	return v.validatedBlocks[hash]
}

func (v *AuxPoWVerifier) GetAllValidatedAuxBlocks() []*AuxPoWData {
	v.mu.RLock()
	defer v.mu.RUnlock()

	result := make([]*AuxPoWData, 0, len(v.auxBlocks))
	for _, data := range v.auxBlocks {
		result = append(result, data)
	}

	return result
}

func hexToBytes(hex string) ([]byte, error) {
	if len(hex)%2 != 0 {
		hex = "0" + hex
	}

	result := make([]byte, len(hex)/2)
	for i := 0; i < len(hex)/2; i++ {
		var b byte
		_, err := fmt.Sscanf(hex[i*2:i*2+2], "%02x", &b)
		if err != nil {
			return nil, err
		}
		result[i] = b
	}

	return result, nil
}

func bytesToHex(data []byte) string {
	const hexChars = "0123456789abcdef"
	result := make([]byte, len(data)*2)
	for i, b := range data {
		result[i*2] = hexChars[b>>4]
		result[i*2+1] = hexChars[b&0x0f]
	}
	return string(result)
}

func (v *AuxPoWVerifier) MarshalJSON() ([]byte, error) {
	v.mu.RLock()
	defer v.mu.RUnlock()

	type AuxBlockJSON struct {
		BlockHash  string `json:"blockHash"`
		ParentHash string `json:"parentHash"`
		BHCHeight  uint64 `json:"bhcHeight"`
		ChainIndex uint32 `json:"chainIndex"`
		Timestamp  int64  `json:"timestamp"`
		Validated  bool   `json:"validated"`
	}

	result := make(map[string]AuxBlockJSON)
	for hash, data := range v.auxBlocks {
		result[hash.Hex()] = AuxBlockJSON{
			BlockHash:  data.BlockHash.Hex(),
			ParentHash: data.ParentHash.Hex(),
			BHCHeight:  data.BHCHeight,
			ChainIndex: data.ChainIndex,
			Timestamp:  data.Timestamp.Unix(),
			Validated:  v.validatedBlocks[data.BlockHash],
		}
	}

	return json.Marshal(result)
}
