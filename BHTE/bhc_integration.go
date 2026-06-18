// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// BHC Integration Layer for BHTE

package bhc

import (
	"context"
	"encoding/json"
	"fmt"
	"net/url"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/ethereum/go-ethereum/common"
	"github.com/ybbus/jsonrpc/v3"
)

func getEnv(key, defaultVal string) string {
	if val := os.Getenv(key); val != "" {
		return val
	}
	return defaultVal
}

const (
	DefaultBHCNodeRPC = "http://127.0.0.1:18332"
	MaxRetries        = 3
	RetryDelay        = time.Second
)

type BHCIntegration struct {
	rpcClient jsonrpc.RPCClient
	config    *IntegrationConfig
	mu        sync.RWMutex
	ctx       context.Context
	cancel    context.CancelFunc
}

type IntegrationConfig struct {
	NodeURL     string
	RPCUser     string
	RPCPassword string
	Timeout     time.Duration
	RetryCount  int
	RetryDelay  time.Duration
}

type RPCError struct {
	Code    int    `json:"code"`
	Message string `json:"message"`
}

type GetBlockCountResult struct {
	Height int64 `json:"height"`
}

type GetBlockHeaderResult struct {
	Hash          string `json:"hash"`
	Height        uint64 `json:"height"`
	Confirmations uint64 `json:"confirmations"`
	PreviousHash  string `json:"previousblockhash"`
	Time          int64  `json:"time"`
	Bits          uint32 `json:"bits"`
	Nonce         uint64 `json:"nonce"`
	Merkleroot    string `json:"merkleroot"`
}

func (h *GetBlockHeaderResult) UnmarshalJSON(data []byte) error {
	type headerJSON struct {
		Hash          string          `json:"hash"`
		Height        uint64          `json:"height"`
		Confirmations uint64          `json:"confirmations"`
		PreviousHash  string          `json:"previousblockhash"`
		Time          int64           `json:"time"`
		Bits          json.RawMessage `json:"bits"`
		Nonce         uint64          `json:"nonce"`
		Merkleroot    string          `json:"merkleroot"`
	}

	var raw headerJSON
	if err := json.Unmarshal(data, &raw); err != nil {
		return err
	}

	bits, err := parseCompactBits(raw.Bits)
	if err != nil {
		return err
	}

	*h = GetBlockHeaderResult{
		Hash:          raw.Hash,
		Height:        raw.Height,
		Confirmations: raw.Confirmations,
		PreviousHash:  raw.PreviousHash,
		Time:          raw.Time,
		Bits:          bits,
		Nonce:         raw.Nonce,
		Merkleroot:    raw.Merkleroot,
	}
	return nil
}

func parseCompactBits(raw json.RawMessage) (uint32, error) {
	if len(raw) == 0 || string(raw) == "null" {
		return 0, nil
	}

	var bitString string
	if err := json.Unmarshal(raw, &bitString); err == nil {
		bitString = strings.TrimSpace(bitString)
		bitString = strings.TrimPrefix(strings.TrimPrefix(bitString, "0x"), "0X")
		if bitString == "" {
			return 0, nil
		}
		value, err := strconv.ParseUint(bitString, 16, 32)
		if err != nil {
			return 0, fmt.Errorf("invalid compact bits %q: %w", bitString, err)
		}
		return uint32(value), nil
	}

	var bitNumber uint64
	if err := json.Unmarshal(raw, &bitNumber); err != nil {
		return 0, fmt.Errorf("invalid compact bits: %w", err)
	}
	if bitNumber > uint64(^uint32(0)) {
		return 0, fmt.Errorf("compact bits value overflows uint32: %d", bitNumber)
	}
	return uint32(bitNumber), nil
}

type GetRawTransactionResult struct {
	Hex      string `json:"hex"`
	Txid     string `json:"txid"`
	Hash     string `json:"hash"`
	Version  int    `json:"version"`
	Locktime uint   `json:"locktime"`
	Vin      []struct {
		Txid      string `json:"txid"`
		Vout      uint   `json:"vout"`
		ScriptSig struct {
			Asm string `json:"asm"`
			Hex string `json:"hex"`
		} `json:"scriptSig"`
		Sequence uint64 `json:"sequence"`
	} `json:"vin"`
	Vout []struct {
		Value        float64 `json:"value"`
		N            uint    `json:"n"`
		ScriptPubKey struct {
			Asm       string   `json:"asm"`
			Hex       string   `json:"hex"`
			Type      string   `json:"type"`
			Addresses []string `json:"addresses"`
		} `json:"scriptPubKey"`
	} `json:"vout"`
}

func DefaultIntegrationConfig() *IntegrationConfig {
	return &IntegrationConfig{
		NodeURL:     getEnv("BHC_NODE_URL", DefaultBHCNodeRPC),
		RPCUser:     getEnv("BHC_RPC_USER", ""),
		RPCPassword: getEnv("BHC_RPC_PASSWORD", ""),
		Timeout:     30 * time.Second,
		RetryCount:  MaxRetries,
		RetryDelay:  RetryDelay,
	}
}

func rpcURLWithCredentials(rawURL, user, password string) string {
	if user == "" {
		return rawURL
	}

	parsed, err := url.Parse(rawURL)
	if err != nil || parsed.User != nil {
		return rawURL
	}

	parsed.User = url.UserPassword(user, password)
	return parsed.String()
}

func NewBHCIntegration(config *IntegrationConfig) (*BHCIntegration, error) {
	if config == nil {
		config = DefaultIntegrationConfig()
	}

	ctx, cancel := context.WithCancel(context.Background())

	client := jsonrpc.NewClient(rpcURLWithCredentials(config.NodeURL, config.RPCUser, config.RPCPassword))

	return &BHCIntegration{
		rpcClient: client,
		config:    config,
		ctx:       ctx,
		cancel:    cancel,
	}, nil
}

func (b *BHCIntegration) Start() error {
	height, err := b.GetBlockCount()
	if err != nil {
		return fmt.Errorf("failed to connect to BHC node: %w", err)
	}

	fmt.Printf("BHC Integration started - connected to block %d\n", height)
	return nil
}

func (b *BHCIntegration) Stop() {
	b.cancel()
}

func (b *BHCIntegration) GetBlockCount() (int64, error) {
	ctx, cancel := context.WithTimeout(b.ctx, b.config.Timeout)
	defer cancel()

	var raw json.RawMessage
	err := b.rpcClient.CallFor(ctx, &raw, "getblockcount")
	if err != nil {
		return 0, fmt.Errorf("getblockcount failed: %w", err)
	}

	var height int64
	if err := json.Unmarshal(raw, &height); err == nil {
		return height, nil
	}

	var result GetBlockCountResult
	if err := json.Unmarshal(raw, &result); err != nil {
		return 0, fmt.Errorf("getblockcount returned invalid result %s: %w", string(raw), err)
	}
	return result.Height, nil
}

func (b *BHCIntegration) GetBlockHash(height uint64) (string, error) {
	ctx, cancel := context.WithTimeout(b.ctx, b.config.Timeout)
	defer cancel()

	var hash string
	if err := b.rpcClient.CallFor(ctx, &hash, "getblockhash", height); err != nil {
		return "", fmt.Errorf("getblockhash failed at height %d: %w", height, err)
	}
	if hash == "" {
		return "", fmt.Errorf("getblockhash returned empty hash at height %d", height)
	}
	return hash, nil
}

func (b *BHCIntegration) GetBlockHeader(height uint64) (*GetBlockHeaderResult, error) {
	hash, err := b.GetBlockHash(height)
	if err != nil {
		return nil, err
	}

	return b.GetBlockHeaderByHash(hash)
}

func (b *BHCIntegration) GetBlockHeaderByHash(hash string) (*GetBlockHeaderResult, error) {
	ctx, cancel := context.WithTimeout(b.ctx, b.config.Timeout)
	defer cancel()

	var result GetBlockHeaderResult
	err := b.rpcClient.CallFor(ctx, &result, "getblockheader", hash, true)
	if err != nil {
		err = b.rpcClient.CallFor(ctx, &result, "getblockheader", hash)
	}
	if err != nil {
		return nil, fmt.Errorf("getblockheader failed: %w", err)
	}
	if result.Hash == "" {
		result.Hash = hash
	}

	return &result, nil
}

func (b *BHCIntegration) GetRawTransaction(txid string) (*GetRawTransactionResult, error) {
	ctx, cancel := context.WithTimeout(b.ctx, b.config.Timeout)
	defer cancel()

	var result GetRawTransactionResult
	err := b.rpcClient.CallFor(ctx, &result, "getrawtransaction", txid, true)
	if err != nil {
		return nil, fmt.Errorf("getrawtransaction failed: %w", err)
	}

	return &result, nil
}

func (b *BHCIntegration) GetStateRoot(height uint64) (common.Hash, error) {
	header, err := b.GetBlockHeader(height)
	if err != nil {
		return common.Hash{}, err
	}

	return common.HexToHash(header.Merkleroot), nil
}

func (b *BHCIntegration) VerifyAnchor(blockHeight uint64, expectedRoot common.Hash) (bool, error) {
	header, err := b.GetBlockHeader(blockHeight)
	if err != nil {
		return false, err
	}

	actualRoot := common.HexToHash(header.Merkleroot)
	return actualRoot == expectedRoot, nil
}

func (b *BHCIntegration) GetConfirmations(height uint64) (uint64, error) {
	currentHeight, err := b.GetBlockCount()
	if err != nil {
		return 0, err
	}

	if height > uint64(currentHeight) {
		return 0, fmt.Errorf("block height %d is in the future", height)
	}

	return uint64(currentHeight) - height + 1, nil
}

func (b *BHCIntegration) IsAnchorReady(blockHeight uint64, minConfirmations uint64) (bool, error) {
	confirmations, err := b.GetConfirmations(blockHeight)
	if err != nil {
		return false, err
	}

	return confirmations >= minConfirmations, nil
}

func (b *BHCIntegration) GetLatestBlockHash() (common.Hash, error) {
	height, err := b.GetBlockCount()
	if err != nil {
		return common.Hash{}, err
	}

	header, err := b.GetBlockHeader(uint64(height))
	if err != nil {
		return common.Hash{}, err
	}

	return common.HexToHash(header.Hash), nil
}

func (b *BHCIntegration) GetBlockTime(height uint64) (time.Time, error) {
	header, err := b.GetBlockHeader(height)
	if err != nil {
		return time.Time{}, err
	}

	return time.Unix(header.Time, 0), nil
}

func (b *BHCIntegration) EstimateNextBlockTime(currentHeight uint64) (time.Time, error) {
	if currentHeight < 10 {
		return time.Now().Add(10 * time.Minute), nil
	}

	startHeight := currentHeight - 9
	headers := make([]*GetBlockHeaderResult, 0, 10)

	for h := startHeight; h <= currentHeight; h++ {
		header, err := b.GetBlockHeader(h)
		if err != nil {
			return time.Time{}, err
		}
		headers = append(headers, header)
	}

	if len(headers) < 2 {
		return time.Now().Add(10 * time.Minute), nil
	}

	var totalInterval int64
	for i := 1; i < len(headers); i++ {
		interval := headers[i].Time - headers[i-1].Time
		if interval < 0 {
			interval = 600
		}
		if interval > 3600 {
			interval = 600
		}
		totalInterval += interval
	}

	avgInterval := totalInterval / int64(len(headers)-1)
	nextTime := headers[len(headers)-1].Time + avgInterval

	return time.Unix(nextTime, 0), nil
}

func (b *BHCIntegration) HealthCheck() error {
	ctx, cancel := context.WithTimeout(b.ctx, 5*time.Second)
	defer cancel()

	var result json.RawMessage
	err := b.rpcClient.CallFor(ctx, &result, "getblockcount")
	if err != nil {
		return fmt.Errorf("health check failed: %w", err)
	}

	return nil
}

func (b *BHCIntegration) String() string {
	b.mu.RLock()
	defer b.mu.RUnlock()

	return fmt.Sprintf("BHCIntegration{NodeURL: %s, Timeout: %s}",
		b.config.NodeURL, b.config.Timeout)
}

func (b *BHCIntegration) MarshalJSON() ([]byte, error) {
	b.mu.RLock()
	defer b.mu.RUnlock()

	type ConfigJSON struct {
		NodeURL    string `json:"nodeURL"`
		Timeout    string `json:"timeout"`
		RetryCount int    `json:"retryCount"`
	}

	return json.Marshal(ConfigJSON{
		NodeURL:    b.config.NodeURL,
		Timeout:    b.config.Timeout.String(),
		RetryCount: b.config.RetryCount,
	})
}
