// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// SPV Bridge - Simplified Payment Verification for Cross-Layer Communication
// Enables BHTE to verify BHC (L1) state without running a full node

package spv

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"math/big"
	"sync"
	"time"

	"github.com/ethereum/go-ethereum/common"
	"github.com/ybbus/jsonrpc/v3"
)

const (
	SPVHeaderCacheSize = 10000
	SPVProofValidDays = 7
	MaxHeaderBatchSize = 2000
)

type BHCSPVClient struct {
	rpcClient       jsonrpc.RPCClient
	ctx            context.Context
	cancel         context.CancelFunc
	headerCache    map[uint64]*BHCHeader
	hashIndex      map[common.Hash]uint64
	mu             sync.RWMutex
	nodeURL        string
	lastSyncTime   time.Time
}

type BHCHeader struct {
	Height         uint64 `json:"height"`
	Hash           string `json:"hash"`
	Version        int64  `json:"version"`
	PreviousHash   string `json:"previousblockhash"`
	MerkleRoot     string `json:"merkleroot"`
	Time           int64  `json:"time"`
	Bits           uint32 `json:"bits"`
	Nonce          uint64 `json:"nonce"`
	Confirmations  uint64 `json:"confirmations"`
	ChainWork      string `json:"chainwork"`
}

type SPVProof struct {
	Header        *BHCHeader `json:"header"`
	ProofType     string     `json:"proof_type"`
	TargetHash    string     `json:"target_hash"`
	ProofData     []string   `json:"proof_data"`
	ChainLength   uint64     `json:"chain_length"`
	ValidFrom     int64      `json:"valid_from"`
	ValidUntil    int64      `json:"valid_until"`
}

type MerkleProof struct {
	BlockHash     string   `json:"block_hash"`
	Transaction   string   `json:"transaction"`
	ProofHashes   []string `json:"proof_hashes"`
	ProofFlags    []bool   `json:"proof_flags"`
	HeaderHeight  uint64   `json:"header_height"`
}

type AddressProof struct {
	Address       string   `json:"address"`
	Balance       uint64   `json:"balance"`
	UTXOCount     uint32   `json:"utxo_count"`
	ProofHashes   []string `json:"proof_hashes"`
	HeaderHeight  uint64   `json:"header_height"`
	HeaderHash    string   `json:"header_hash"`
}

func NewBHCSPVClient(nodeURL string) *BHCSPVClient {
	if nodeURL == "" {
		nodeURL = "http://127.0.0.1:18332"
	}

	ctx, cancel := context.WithCancel(context.Background())

	return &BHCSPVClient{
		rpcClient:   jsonrpc.NewClient(nodeURL),
		ctx:        ctx,
		cancel:     cancel,
		headerCache: make(map[uint64]*BHCHeader),
		hashIndex:  make(map[common.Hash]uint64),
		nodeURL:    nodeURL,
	}
}

func (c *BHCSPVClient) Start() error {
	go c.syncHeadersPeriodically()
	return nil
}

func (c *BHCSPVClient) Stop() {
	c.cancel()
}

func (c *BHCSPVClient) GetLatestHeight() (uint64, error) {
	var result struct {
		Height uint64 `json:"height"`
	}

	err := c.rpcClient.CallFor(c.ctx, &result, "getblockcount")
	if err != nil {
		return 0, fmt.Errorf("failed to get block count: %w", err)
	}

	return result.Height, nil
}

func (c *BHCSPVClient) GetBlockHeader(height uint64) (*BHCHeader, error) {
	c.mu.RLock()
	if header, exists := c.headerCache[height]; exists {
		c.mu.RUnlock()
		return header, nil
	}
	c.mu.RUnlock()

	var header BHCHeader
	err := c.rpcClient.CallFor(c.ctx, &header, "getblockheader", height)
	if err != nil {
		return nil, fmt.Errorf("failed to get block header: %w", err)
	}

	c.mu.Lock()
	c.headerCache[height] = &header
	c.hashIndex[common.HexToHash(header.Hash)] = height
	c.mu.Unlock()

	return &header, nil
}

func (c *BHCSPVClient) GetBlockHeaderByHash(hash string) (*BHCHeader, error) {
	hashHash := common.HexToHash(hash)

	c.mu.RLock()
	if height, exists := c.hashIndex[hashHash]; exists {
		if header, exists := c.headerCache[height]; exists {
			c.mu.RUnlock()
			return header, nil
		}
	}
	c.mu.RUnlock()

	var header BHCHeader
	err := c.rpcClient.CallFor(c.ctx, &header, "getblockheader", hash)
	if err != nil {
		return nil, fmt.Errorf("failed to get block header: %w", err)
	}

	c.mu.Lock()
	c.headerCache[header.Height] = &header
	c.hashIndex[hashHash] = header.Height
	c.mu.Unlock()

	return &header, nil
}

func (c *BHCSPVClient) GetHeaderChain(startHeight, endHeight uint64) ([]*BHCHeader, error) {
	if endHeight < startHeight {
		return nil, fmt.Errorf("end height must be >= start height")
	}

	if endHeight-startHeight > MaxHeaderBatchSize {
		endHeight = startHeight + MaxHeaderBatchSize
	}

	headers := make([]*BHCHeader, 0, endHeight-startHeight+1)

	for h := startHeight; h <= endHeight; h++ {
		header, err := c.GetBlockHeader(h)
		if err != nil {
			return nil, err
		}
		headers = append(headers, header)
	}

	return headers, nil
}

func (c *BHCSPVClient) VerifyHeaderChain(headers []*BHCHeader) (bool, error) {
	if len(headers) < 1 {
		return false, fmt.Errorf("header chain too short")
	}

	for i := 1; i < len(headers); i++ {
		prevHeader := headers[i-1]
		currHeader := headers[i]

		if currHeader.PreviousHash != prevHeader.Hash {
			return false, fmt.Errorf("header chain broken at height %d: previous hash mismatch", currHeader.Height)
		}

		if currHeader.Height != prevHeader.Height+1 {
			return false, fmt.Errorf("header chain broken at height %d: height gap", currHeader.Height)
		}

		if !c.verifyProofOfWork(currHeader) {
			return false, fmt.Errorf("header chain broken at height %d: insufficient proof of work", currHeader.Height)
		}
	}

	return true, nil
}

func (c *BHCSPVClient) verifyProofOfWork(header *BHCHeader) bool {
	targetDifficulty := c.decodeBits(header.Bits)

	headerHash := common.HexToHash(header.Hash)
	headerNum := new(big.Int).SetBytes(headerHash.Bytes())

	return headerNum.Cmp(targetDifficulty) <= 0
}

func (c *BHCSPVClient) decodeBits(bits uint32) *big.Int {
	exponent := uint(bits >> 24)
	mantissa := bits & 0x007FFFFF

	limit := new(big.Int).Lsh(big.NewInt(1), 256-exponent)
	result := new(big.Int).Mul(big.NewInt(int64(mantissa)), limit)

	return result
}

func (c *BHCSPVClient) GenerateHeaderProof(startHeight, endHeight uint64) (*SPVProof, error) {
	headers, err := c.GetHeaderChain(startHeight, endHeight)
	if err != nil {
		return nil, err
	}

	valid, err := c.VerifyHeaderChain(headers)
	if err != nil || !valid {
		return nil, fmt.Errorf("header chain verification failed")
	}

	headerHashes := make([]string, len(headers))
	for i, h := range headers {
		headerHashes[i] = h.Hash
	}

	return &SPVProof{
		Header:      headers[len(headers)-1],
		ProofType:   "header_chain",
		TargetHash:  headers[len(headers)-1].Hash,
		ProofData:   headerHashes,
		ChainLength: uint64(len(headers)),
		ValidFrom:   headers[0].Time,
		ValidUntil:  headers[len(headers)-1].Time + int64(SPVProofValidDays*24*60*60),
	}, nil
}

func (c *BHCSPVClient) GetMerkleProof(txHash string, blockHeight uint64) (*MerkleProof, error) {
	var result struct {
		BlockHash   string   `json:"block_hash"`
		Proof       []string `json:"proof"`
		ProofFlags  []bool   `json:"flags"`
		Transaction string   `json:"tx"`
	}

	err := c.rpcClient.CallFor(c.ctx, &result, "gettxoutproof", []string{txHash}, blockHeight)
	if err != nil {
		return nil, fmt.Errorf("failed to get merkle proof: %w", err)
	}

	txResult := ""
	if len(result.Proof) > 0 {
		txResult = result.Proof[0]
	}

	return &MerkleProof{
		BlockHash:    result.BlockHash,
		Transaction:  txResult,
		ProofHashes:  result.Proof,
		ProofFlags:   result.ProofFlags,
		HeaderHeight: blockHeight,
	}, nil
}

func (c *BHCSPVClient) VerifyMerkleProof(proof *MerkleProof) (bool, error) {
	if len(proof.ProofHashes) == 0 && proof.Transaction == "" {
		return false, fmt.Errorf("empty merkle proof")
	}

	if proof.BlockHash == "" {
		return false, fmt.Errorf("missing block hash")
	}

	header, err := c.GetBlockHeader(proof.HeaderHeight)
	if err != nil {
		return false, err
	}

	if header.Hash != proof.BlockHash {
		return false, fmt.Errorf("block hash mismatch")
	}

	txHash := proof.Transaction
	if txHash == "" && len(proof.ProofHashes) > 0 {
		txHash = proof.ProofHashes[0]
	}

	computedMerkle, err := c.computeMerkleRoot(proof.ProofHashes, proof.ProofFlags, txHash)
	if err != nil {
		return false, fmt.Errorf("failed to compute merkle root: %w", err)
	}

	if computedMerkle != header.MerkleRoot {
		return false, fmt.Errorf("merkle root mismatch: computed %s, header %s", computedMerkle, header.MerkleRoot)
	}

	return true, nil
}

func (c *BHCSPVClient) computeMerkleRoot(proofHashes []string, proofFlags []bool, txHash string) (string, error) {
	if len(proofHashes) == 0 && txHash == "" {
		return "", fmt.Errorf("empty merkle proof")
	}

	if txHash == "" && len(proofHashes) > 0 {
		txHash = proofHashes[0]
	}

	hashes := []string{txHash}

	for i := 0; i < len(proofHashes) && i < len(proofFlags); i++ {
		var left, right string

		if proofFlags[i] {
			left = hashes[len(hashes)-1]
			right = proofHashes[i]
		} else {
			if len(hashes) > 1 {
				left = hashes[len(hashes)-1]
				hashes = hashes[:len(hashes)-1]
			} else {
				left = hashes[0]
			}

			if i < len(proofHashes) {
				right = proofHashes[i]
			} else {
				right = left
			}
		}

		combined := left + right
		newHash := c.sha256d(combined)
		hashes = append(hashes, newHash)
	}

	if len(hashes) == 0 {
		return "", fmt.Errorf("failed to compute merkle root")
	}

	return hashes[len(hashes)-1], nil
}

func (c *BHCSPVClient) sha256d(input string) string {
	h1 := sha256.Sum256([]byte(input))
	h2 := sha256.Sum256(h1[:])
	return hex.EncodeToString(h2[:])
}

func (c *BHCSPVClient) GetAddressProof(address string, minHeight uint64) (*AddressProof, error) {
	var result struct {
		Balance uint64 `json:"balance"`
		UTXOs   []struct {
			TxHash string `json:"tx_hash"`
			VOut   uint32 `json:"vout"`
			Amount uint64 `json:"amount"`
		} `json:"utxos"`
	}

	err := c.rpcClient.CallFor(c.ctx, &result, "getaddressutxos", map[string]interface{}{
		"addresses": []string{address},
		"start":     minHeight,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to get address UTXOs: %w", err)
	}

	proofHashes := make([]string, len(result.UTXOs))
	for i, utxo := range result.UTXOs {
		proof, err := c.GetMerkleProof(utxo.TxHash, minHeight)
		if err != nil {
			continue
		}
		proofHashes[i] = proof.BlockHash
	}

	var headerHash string
	if len(proofHashes) > 0 {
		header, _ := c.GetBlockHeader(minHeight)
		if header != nil {
			headerHash = header.Hash
		}
	}

	return &AddressProof{
		Address:      address,
		Balance:      result.Balance,
		UTXOCount:    uint32(len(result.UTXOs)),
		ProofHashes: proofHashes,
		HeaderHeight: minHeight,
		HeaderHash:   headerHash,
	}, nil
}

func (c *BHCSPVClient) syncHeadersPeriodically() {
	ticker := time.NewTicker(10 * time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-c.ctx.Done():
			return
		case <-ticker.C:
			c.performHeaderSync()
		}
	}
}

func (c *BHCSPVClient) performHeaderSync() {
	latestHeight, err := c.GetLatestHeight()
	if err != nil {
		return
	}

	c.mu.RLock()
	cachedHeight := uint64(0)
	for height := range c.headerCache {
		if height > cachedHeight {
			cachedHeight = height
		}
	}
	c.mu.RUnlock()

	if latestHeight > cachedHeight {
		startHeight := cachedHeight + 1
		if startHeight == 0 {
			startHeight = 1
		}

		headers, err := c.GetHeaderChain(startHeight, latestHeight)
		if err != nil {
			return
		}

		c.mu.Lock()
		for _, h := range headers {
			c.headerCache[h.Height] = h
			c.hashIndex[common.HexToHash(h.Hash)] = h.Height
		}
		c.lastSyncTime = time.Now()
		c.mu.Unlock()
	}
}

func (c *BHCSPVClient) GetCachedHeader(height uint64) (*BHCHeader, bool) {
	c.mu.RLock()
	defer c.mu.RUnlock()

	header, exists := c.headerCache[height]
	return header, exists
}

func (c *BHCSPVClient) GetCacheStats() (int, time.Time) {
	c.mu.RLock()
	defer c.mu.RUnlock()

	return len(c.headerCache), c.lastSyncTime
}

func (c *BHCSPVClient) VerifyAnchor(stateRoot string, blockHeight uint64) (bool, error) {
	header, err := c.GetBlockHeader(blockHeight)
	if err != nil {
		return false, err
	}

	if header.MerkleRoot != stateRoot {
		return false, fmt.Errorf("state root mismatch: expected %s, got %s", stateRoot, header.MerkleRoot)
	}

	return true, nil
}

func (c *BHCSPVClient) GetConfirmations(txHash string) (uint64, error) {
	var result struct {
		Confirmations uint64 `json:"confirmations"`
		BlockHash    string `json:"blockhash"`
		BlockHeight  uint64 `json:"blockheight"`
	}

	err := c.rpcClient.CallFor(c.ctx, &result, "getrawtransaction", txHash, true)
	if err != nil {
		return 0, fmt.Errorf("failed to get transaction: %w", err)
	}

	return result.Confirmations, nil
}

func (c *BHCSPVClient) MarshalJSON() ([]byte, error) {
	c.mu.RLock()
	defer c.mu.RUnlock()

	type headerJSON struct {
		Height   uint64 `json:"height"`
		Hash     string `json:"hash"`
		Time     int64  `json:"time"`
	}

	result := struct {
		NodeURL     string               `json:"nodeURL"`
		CacheSize  int                  `json:"cacheSize"`
		LastSync   time.Time            `json:"lastSync"`
		Headers    map[string]headerJSON `json:"headers"`
	}{
		NodeURL:    c.nodeURL,
		CacheSize:  len(c.headerCache),
		LastSync:   c.lastSyncTime,
		Headers:    make(map[string]headerJSON),
	}

	for height, header := range c.headerCache {
		result.Headers[fmt.Sprintf("%d", height)] = headerJSON{
			Height: header.Height,
			Hash:   header.Hash,
			Time:   header.Time,
		}
	}

	return json.Marshal(result)
}
