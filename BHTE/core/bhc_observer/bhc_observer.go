// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// BHC L1 Observer Module for BHTE Layer 2

package bhte

import (
	"context"
	"encoding/hex"
	"fmt"
	"sync"
	"time"

	"github.com/ethereum/go-ethereum/log"
	"github.com/ybbus/jsonrpc/v3"
)

const (
	DefaultBHCNodeURL = "http://127.0.0.1:18332"
	AnchorCheckInterval = 10 * time.Minute
	MaxBlockLatency = 24 * time.Hour
)

type BlockHeader struct {
	Hash              string `json:"hash"`
	Height            uint64 `json:"height"`
	PreviousBlockHash string `json:"previousblockhash"`
	Time              int64  `json:"time"`
	Bits             uint32 `json:"bits"`
	Nonce            uint64 `json:"nonce"`
	MerkleRoot       string `json:"merkleroot"`
}

type AnchorRecord struct {
	BlockHeight uint64
	StateRoot   string
	TxHash      string
	Timestamp   time.Time
}

type BHCObserver struct {
	rpcClient      jsonrpc.RPCClient
	nodeURL        string
	latestHeight   uint64
	anchors        map[uint64]*AnchorRecord
	mu            sync.RWMutex
	ctx           context.Context
	cancel        context.CancelFunc
}

func NewBHCObserver(nodeURL string) *BHCObserver {
	if nodeURL == "" {
		nodeURL = DefaultBHCNodeURL
	}
	
	ctx, cancel := context.WithCancel(context.Background())
	
	return &BHCObserver{
		rpcClient: jsonrpc.NewClient(nodeURL),
		nodeURL:    nodeURL,
		latestHeight: 0,
		anchors:      make(map[uint64]*AnchorRecord),
		ctx:         ctx,
		cancel:      cancel,
	}
}

func (o *BHCObserver) Start() error {
	go o.watchBlocks()
	go o.checkAnchors()
	return nil
}

func (o *BHCObserver) Stop() {
	o.cancel()
}

func (o *BHCObserver) GetLatestHeight() (uint64, error) {
	var result struct {
		Height uint64 `json:"height"`
	}
	
	err := o.rpcClient.CallFor(o.ctx, &result, "getblockcount")
	if err != nil {
		return 0, err
	}
	
	return result.Height, nil
}

func (o *BHCObserver) GetBlockHeader(height uint64) (*BlockHeader, error) {
	var header BlockHeader
	
	err := o.rpcClient.CallFor(o.ctx, &header, "getblockheader", height)
	if err != nil {
		return nil, err
	}
	
	return &header, nil
}

func (o *BHCObserver) GetBlockHeaderByHash(hash string) (*BlockHeader, error) {
	var header BlockHeader
	
	err := o.rpcClient.CallFor(o.ctx, &header, "getblockheader", hash)
	if err != nil {
		return nil, err
	}
	
	return &header, nil
}

func (o *BHCObserver) VerifyAnchorOnL1(stateRoot string, blockHeight uint64) (bool, error) {
	o.mu.RLock()
	anchor, exists := o.anchors[blockHeight]
	o.mu.RUnlock()
	
	if !exists {
		return false, nil
	}
	
	return anchor.StateRoot == stateRoot, nil
}

func (o *BHCObserver) GetAnchorAtHeight(height uint64) (*AnchorRecord, error) {
	o.mu.RLock()
	defer o.mu.RUnlock()
	
	anchor, exists := o.anchors[height]
	if !exists {
		return nil, fmt.Errorf("no anchor at height %d", height)
	}
	
	return anchor, nil
}

func (o *BHCObserver) watchBlocks() {
	ticker := time.NewTicker(10 * time.Second)
	defer ticker.Stop()
	
	for {
		select {
		case <-o.ctx.Done():
			return
		case <-ticker.C:
			height, err := o.GetLatestHeight()
			if err != nil {
				continue
			}
			
			o.mu.Lock()
			if height > o.latestHeight {
				o.latestHeight = height
				o.mu.Unlock()
				
				o.processNewBlock(height)
			} else {
				o.mu.Unlock()
			}
		}
	}
}

func (o *BHCObserver) processNewBlock(height uint64) {
	header, err := o.GetBlockHeader(height)
	if err != nil {
		return
	}
	
	anchor := o.extractAnchorFromBlock(header)
	if anchor != nil {
		o.mu.Lock()
		o.anchors[height] = anchor
		o.mu.Unlock()
	}
}

func (o *BHCObserver) extractAnchorFromBlock(header *BlockHeader) *AnchorRecord {
	if header == nil {
		return nil
	}

	anchor := &AnchorRecord{
		BlockHeight: header.Height,
		StateRoot:   header.MerkleRoot,
		TxHash:      header.Hash,
		Timestamp:   time.Unix(header.Time, 0),
	}

	return anchor
}

func (o *BHCObserver) checkAnchors() {
	ticker := time.NewTicker(AnchorCheckInterval)
	defer ticker.Stop()
	
	for {
		select {
		case <-o.ctx.Done():
			return
		case <-ticker.C:
			o.verifyRecentAnchors()
		}
	}
}

func (o *BHCObserver) verifyRecentAnchors() {
	o.mu.RLock()
	height := o.latestHeight
	o.mu.RUnlock()
	
	if height < 10 {
		return
	}
	
	for h := height - 10; h <= height; h++ {
		o.mu.RLock()
		anchor, exists := o.anchors[h]
		o.mu.RUnlock()

		if !exists {
			continue
		}

		header, err := o.GetBlockHeader(h)
		if err != nil {
			continue
		}

		if header.MerkleRoot != anchor.StateRoot {
			log.Warn("Anchor verification failed", "height", h, "expected", anchor.StateRoot[:16], "got", header.MerkleRoot[:16])
		}
	}
}

func (o *BHCObserver) GetLatestAnchor() (*AnchorRecord, error) {
	o.mu.RLock()
	defer o.mu.RUnlock()
	
	if len(o.anchors) == 0 {
		return nil, fmt.Errorf("no anchors available")
	}
	
	var latest *AnchorRecord
	var latestHeight uint64
	
	for height, anchor := range o.anchors {
		if height > latestHeight {
			latestHeight = height
			latest = anchor
		}
	}
	
	return latest, nil
}

func (o *BHCObserver) WaitForAnchor(expectedRoot string, timeout time.Duration) (*AnchorRecord, error) {
	ctx, cancel := context.WithTimeout(o.ctx, timeout)
	defer cancel()
	
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()
	
	for {
		select {
		case <-ctx.Done():
			return nil, fmt.Errorf("timeout waiting for anchor")
		case <-ticker.C:
			o.mu.RLock()
			for _, anchor := range o.anchors {
				if anchor.StateRoot == expectedRoot {
					o.mu.RUnlock()
					return anchor, nil
				}
			}
			o.mu.RUnlock()
		}
	}
}

func (a *AnchorRecord) String() string {
	return fmt.Sprintf("Anchor{Height: %d, StateRoot: %s, TxHash: %s}", 
		a.BlockHeight, a.StateRoot[:16]+"...", a.TxHash[:16]+"...")
}

func HexToBytes(hexStr string) ([]byte, error) {
	if len(hexStr)%2 != 0 {
		return nil, fmt.Errorf("invalid hex string length")
	}
	
	result := make([]byte, len(hexStr)/2)
	_, err := hex.Decode(result, []byte(hexStr))
	return result, err
}

func BytesToHex(bytes []byte) string {
	return hex.EncodeToString(bytes)
}
