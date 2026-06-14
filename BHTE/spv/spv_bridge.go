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
	"strings"
	"sync"
	"time"

	"github.com/ethereum/go-ethereum/common"
	"github.com/ybbus/jsonrpc/v3"
)

const (
	SPVHeaderCacheSize = 10000
	SPVProofValidDays = 7
	MaxHeaderBatchSize = 2000
	MaxMerkleProofHashes = 1024
	DefaultMinConfirmations = 3
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
	var result uint64

	err := c.rpcClient.CallFor(c.ctx, &result, "getblockcount")
	if err != nil {
		return 0, fmt.Errorf("failed to get block count: %w", err)
	}

	return result, nil
}

func (c *BHCSPVClient) GetBlockHash(height uint64) (string, error) {
	var hash string
	if err := c.rpcClient.CallFor(c.ctx, &hash, "getblockhash", height); err != nil {
		return "", fmt.Errorf("failed to get block hash at height %d: %w", height, err)
	}
	if hash == "" {
		return "", fmt.Errorf("empty block hash at height %d", height)
	}
	return hash, nil
}

func (c *BHCSPVClient) GetBlockHeader(height uint64) (*BHCHeader, error) {
	c.mu.RLock()
	if header, exists := c.headerCache[height]; exists {
		c.mu.RUnlock()
		return header, nil
	}
	c.mu.RUnlock()

	var header BHCHeader
	blockHash, hashErr := c.GetBlockHash(height)
	err := hashErr
	if hashErr == nil {
		err = c.rpcClient.CallFor(c.ctx, &header, "getblockheader", blockHash, true)
		if err != nil {
			err = c.rpcClient.CallFor(c.ctx, &header, "getblockheader", blockHash)
		}
	}
	if err != nil {
		err = c.rpcClient.CallFor(c.ctx, &header, "getblockheader", height)
	}
	if err != nil {
		return nil, fmt.Errorf("failed to get block header: %w", err)
	}
	if header.Hash == "" && blockHash != "" {
		header.Hash = blockHash
	}
	if header.Height == 0 {
		header.Height = height
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
	err := c.rpcClient.CallFor(c.ctx, &header, "getblockheader", hash, true)
	if err != nil {
		err = c.rpcClient.CallFor(c.ctx, &header, "getblockheader", hash)
	}
	if err != nil {
		return nil, fmt.Errorf("failed to get block header: %w", err)
	}
	if header.Hash == "" {
		header.Hash = hash
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
	target := c.decodeBits(header.Bits)
	if target.Sign() <= 0 {
		return false
	}

	// header.Hash is in display (big-endian) order, which equals the numeric
	// value of the proof-of-work hash. Reject malformed hashes.
	raw, err := hex.DecodeString(header.Hash)
	if err != nil || len(raw) != 32 {
		return false
	}
	headerNum := new(big.Int).SetBytes(raw)

	return headerNum.Cmp(target) <= 0
}

// decodeBits converts a Bitcoin-style compact "nBits" encoding into the full
// 256-bit target: target = mantissa * 256^(exponent-3).
func (c *BHCSPVClient) decodeBits(bits uint32) *big.Int {
	exponent := bits >> 24
	mantissa := big.NewInt(int64(bits & 0x007FFFFF))

	// The 0x00800000 sign bit is never set for valid targets; treat as positive.
	if exponent <= 3 {
		return new(big.Int).Rsh(mantissa, uint(8*(3-exponent)))
	}
	return new(big.Int).Lsh(mantissa, uint(8*(exponent-3)))
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
		BlockHash      string   `json:"block_hash"`
		BlockHashAlt   string   `json:"blockhash"`
		Proof          []string `json:"proof"`
		MerkleBranch   []string `json:"merkle_branch"`
		ProofFlags     []bool   `json:"flags"`
		ProofFlagsAlt  []bool   `json:"proof_flags"`
		Transaction    string   `json:"tx"`
		TransactionAlt string   `json:"transaction"`
		TxID           string   `json:"txid"`
	}

	blockHash, hashErr := c.GetBlockHash(blockHeight)
	err := hashErr
	if hashErr == nil {
		err = c.rpcClient.CallFor(c.ctx, &result, "gettxoutproof", []string{txHash}, blockHash)
	}
	if err != nil {
		err = c.rpcClient.CallFor(c.ctx, &result, "gettxoutproof", []string{txHash}, blockHeight)
	}
	if err != nil {
		return nil, fmt.Errorf("failed to get merkle proof: %w", err)
	}

	if result.BlockHash != "" {
		blockHash = result.BlockHash
	}
	if result.BlockHashAlt != "" {
		blockHash = result.BlockHashAlt
	}

	proofHashes := result.Proof
	if len(proofHashes) == 0 {
		proofHashes = result.MerkleBranch
	}
	proofFlags := result.ProofFlags
	if len(proofFlags) == 0 {
		proofFlags = result.ProofFlagsAlt
	}
	txResult := txHash
	if result.Transaction != "" {
		txResult = result.Transaction
	}
	if result.TransactionAlt != "" {
		txResult = result.TransactionAlt
	}
	if result.TxID != "" {
		txResult = result.TxID
	}

	return &MerkleProof{
		BlockHash:    blockHash,
		Transaction:  txResult,
		ProofHashes:  proofHashes,
		ProofFlags:   proofFlags,
		HeaderHeight: blockHeight,
	}, nil
}

func (c *BHCSPVClient) VerifyMerkleProof(proof *MerkleProof) (bool, error) {
	if proof == nil {
		return false, fmt.Errorf("nil merkle proof")
	}
	if len(proof.ProofHashes) == 0 && proof.Transaction == "" {
		return false, fmt.Errorf("empty merkle proof")
	}
	if len(proof.ProofHashes) > MaxMerkleProofHashes {
		return false, fmt.Errorf("merkle proof too large: %d hashes", len(proof.ProofHashes))
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

	if proof.Transaction == "" {
		return false, fmt.Errorf("missing transaction id")
	}

	computedMerkle, err := c.computeMerkleRoot(proof.Transaction, proof.ProofHashes, proof.ProofFlags)
	if err != nil {
		return false, fmt.Errorf("failed to compute merkle root: %w", err)
	}

	if !strings.EqualFold(computedMerkle, header.MerkleRoot) {
		return false, fmt.Errorf("merkle root mismatch: computed %s, header %s", computedMerkle, header.MerkleRoot)
	}

	return true, nil
}

func (c *BHCSPVClient) VerifyTransactionProof(txHash string, blockHeight uint64, minConfirmations uint64) (bool, error) {
	if txHash == "" {
		return false, fmt.Errorf("missing transaction hash")
	}
	if minConfirmations == 0 {
		minConfirmations = DefaultMinConfirmations
	}

	latestHeight, err := c.GetLatestHeight()
	if err != nil {
		return false, err
	}
	if latestHeight < blockHeight {
		return false, fmt.Errorf("block height %d is above latest height %d", blockHeight, latestHeight)
	}
	confirmations := latestHeight - blockHeight + 1
	if confirmations < minConfirmations {
		return false, fmt.Errorf("insufficient confirmations: have %d, need %d", confirmations, minConfirmations)
	}

	proof, err := c.GetMerkleProof(txHash, blockHeight)
	if err != nil {
		return false, err
	}

	return c.VerifyMerkleProof(proof)
}

// computeMerkleRoot folds a Bitcoin-style merkle branch from a leaf (txHash) up
// to the root. All hashes are hex in display (reversed) byte order; folding is
// performed in internal byte order using double-SHA256, and the result is
// returned in display order so it can be compared with a block's merkleroot.
//
// proofFlags[i] == true means the sibling at level i is the right node (the
// running hash is the left child); false means the sibling is the left node.
func (c *BHCSPVClient) computeMerkleRoot(txHash string, proofHashes []string, proofFlags []bool) (string, error) {
	if len(proofHashes) != len(proofFlags) {
		return "", fmt.Errorf("proof hashes/flags length mismatch")
	}

	cur, err := decodeReversed(txHash)
	if err != nil {
		return "", fmt.Errorf("invalid transaction id: %w", err)
	}

	for i, sibHex := range proofHashes {
		sib, err := decodeReversed(sibHex)
		if err != nil {
			return "", fmt.Errorf("invalid proof hash %d: %w", i, err)
		}
		if proofFlags[i] {
			cur = sha256d(append(append([]byte{}, cur...), sib...)) // H(cur || sib)
		} else {
			cur = sha256d(append(append([]byte{}, sib...), cur...)) // H(sib || cur)
		}
	}

	return encodeReversed(cur), nil
}

// sha256d returns the double SHA-256 of the input bytes.
func sha256d(input []byte) []byte {
	h1 := sha256.Sum256(input)
	h2 := sha256.Sum256(h1[:])
	return h2[:]
}

// decodeReversed decodes a display-order hex hash into internal byte order.
func decodeReversed(s string) ([]byte, error) {
	b, err := hex.DecodeString(s)
	if err != nil {
		return nil, err
	}
	for i, j := 0, len(b)-1; i < j; i, j = i+1, j-1 {
		b[i], b[j] = b[j], b[i]
	}
	return b, nil
}

// encodeReversed encodes internal byte order back to display-order hex.
func encodeReversed(b []byte) string {
	r := make([]byte, len(b))
	for i := range b {
		r[i] = b[len(b)-1-i]
	}
	return hex.EncodeToString(r)
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
