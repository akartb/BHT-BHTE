// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// Merge Mining Coordinator - Orchestrates AuxPoW between BHC and BHTE

package coordinator

import (
	"context"
	"fmt"
	"math/big"
	"sync"
	"time"

	"github.com/ethereum/go-ethereum/common"
	"github.com/ethereum/go-ethereum/core/types"
	"github.com/ethereum/go-ethereum/crypto"
	"github.com/ethereum/go-ethereum/log"

	"bhte/consensus/bhc"
	proxy "bhte/consensus/bhc/proxy"
)

const (
	MaxMergeMiningDelay          = 30 * time.Second
	CoordinatedBlocks            = 16
	MinPoWDifficulty             = 1000
	MaxMinerRegistrationsPerHour = 10
	MinSharesPerHourForActive    = 0
)

type MergeMiningCoordinator struct {
	auxVerifier         *proxy.AuxPoWVerifier
	anchorVerifier      *bhc.AnchorVerifier
	bhteHeaderChan      chan *types.Header
	coordinationMu      sync.RWMutex
	pendingBlocks       map[common.Hash]*CoordinatedBlock
	activeMiners        map[string]*MinerStatus
	minersMu            sync.RWMutex
	ctx                 context.Context
	cancel              context.CancelFunc
	enabled             bool
	powThreshold        *big.Int
	registrationLimiter map[string]*RegistrationAttempt
	limiterMu           sync.RWMutex
}

type CoordinatedBlock struct {
	BHTEHeader     *types.Header
	BHCAuxData     *proxy.AuxPoWData
	BHCBlockHash   common.Hash
	CreatedAt      time.Time
	MinedAt        time.Time
	SubmitAttempts int
}

type MinerStatus struct {
	MinerID              string
	LastShareTime        time.Time
	SharesPerHour        float64
	TotalShares          uint64
	VerifiedPoW          *big.Int
	RegisteredAt         time.Time
	LastPoWCheck         time.Time
	RegistrationFailures uint64
	Active               bool
}

func NewMergeMiningCoordinator(
	bhcNodeURL string,
	anchorVerifier *bhc.AnchorVerifier,
) *MergeMiningCoordinator {
	ctx, cancel := context.WithCancel(context.Background())

	coordinator := &MergeMiningCoordinator{
		auxVerifier:         proxy.NewAuxPoWVerifier(bhcNodeURL),
		anchorVerifier:      anchorVerifier,
		bhteHeaderChan:      make(chan *types.Header, 100),
		pendingBlocks:       make(map[common.Hash]*CoordinatedBlock),
		activeMiners:        make(map[string]*MinerStatus),
		ctx:                 ctx,
		cancel:              cancel,
		enabled:             true,
		powThreshold:        big.NewInt(MinPoWDifficulty),
		registrationLimiter: make(map[string]*RegistrationAttempt),
	}

	return coordinator
}

func (c *MergeMiningCoordinator) Start() error {
	if err := c.auxVerifier.Start(); err != nil {
		return fmt.Errorf("failed to start AuxPoW verifier: %w", err)
	}

	go c.coordinateBlocks()
	go c.cleanupPendingBlocks()
	go c.monitorMinerHealth()

	log.Info("Merge Mining Coordinator started")

	return nil
}

func (c *MergeMiningCoordinator) Stop() {
	c.cancel()
	c.enabled = false

	if c.auxVerifier != nil {
		c.auxVerifier.Stop()
	}

	close(c.bhteHeaderChan)

	log.Info("Merge Mining Coordinator stopped")
}

func (c *MergeMiningCoordinator) SubmitBHTEHeader(header *types.Header) error {
	if !c.enabled {
		return fmt.Errorf("merge mining is disabled")
	}

	select {
	case c.bhteHeaderChan <- header:
		return nil
	case <-time.After(MaxMergeMiningDelay):
		return fmt.Errorf("timeout submitting BHTX header")
	}
}

func (c *MergeMiningCoordinator) coordinateBlocks() {
	for {
		select {
		case <-c.ctx.Done():
			return
		case header := <-c.bhteHeaderChan:
			c.processBHTEHeader(header)
		}
	}
}

func (c *MergeMiningCoordinator) processBHTEHeader(header *types.Header) {
	c.coordinationMu.Lock()
	defer c.coordinationMu.Unlock()

	blockHash := header.Hash()

	if _, exists := c.pendingBlocks[blockHash]; exists {
		log.Debug("Block already pending coordination", "hash", blockHash.Hex())
		return
	}

	coordinated := &CoordinatedBlock{
		BHTEHeader: header,
		CreatedAt:  time.Now(),
	}

	bhcBlockHash, err := c.findMatchingBHCBlock(header)
	if err != nil {
		log.Debug("No matching BHC block found", "error", err)
	} else {
		coordinated.BHCBlockHash = bhcBlockHash
		coordinated.BHCAuxData, _ = c.auxVerifier.GetAuxBlockForBHTTBlock(bhcBlockHash)
	}

	c.pendingBlocks[blockHash] = coordinated

	c.scheduleCoordination(blockHash)

	log.Info("BHTX block queued for merge mining",
		"hash", blockHash.Hex(),
		"bhc_hash", coordinated.BHCBlockHash.Hex())
}

func (c *MergeMiningCoordinator) findMatchingBHCBlock(bhteHeader *types.Header) (common.Hash, error) {
	currentHeight, err := c.auxVerifier.GetLatestBHCHeight()
	if err != nil {
		return common.Hash{}, err
	}

	bhteDifficulty := bhteHeader.Difficulty
	targetDifficulty := new(big.Int).Div(bhteDifficulty, big.NewInt(1000))

	for i := uint64(0); i < uint64(MaxMergeMiningDelay/time.Second); i++ {
		checkHeight := currentHeight - i
		if checkHeight < 1 {
			break
		}

		blockHash, err := c.auxVerifier.GetBHCBlockHash(checkHeight)
		if err != nil {
			continue
		}

		if c.isValidAuxProof(blockHash, targetDifficulty) {
			return blockHash, nil
		}
	}

	return common.Hash{}, fmt.Errorf("no valid BHC block found")
}

func (c *MergeMiningCoordinator) isValidAuxProof(blockHash common.Hash, targetDifficulty *big.Int) bool {
	combinedHash := common.BytesToHash(append(
		blockHash.Bytes(),
		targetDifficulty.Bytes()...,
	))

	hashInt := new(big.Int).SetBytes(combinedHash.Bytes())
	divisor := new(big.Int).Div(targetDifficulty, big.NewInt(1000000))

	return hashInt.Cmp(divisor) < 0
}

func (c *MergeMiningCoordinator) scheduleCoordination(blockHash common.Hash) {
	go func() {
		timeout := time.After(MaxMergeMiningDelay)

		for {
			select {
			case <-c.ctx.Done():
				return
			case <-timeout:
				c.finalizeCoordination(blockHash)
				return
			}
		}
	}()
}

func (c *MergeMiningCoordinator) finalizeCoordination(blockHash common.Hash) {
	c.coordinationMu.Lock()
	defer c.coordinationMu.Unlock()

	coordinated, exists := c.pendingBlocks[blockHash]
	if !exists {
		return
	}

	if coordinated.MinedAt.IsZero() {
		coordinated.MinedAt = time.Now()
	}

	coordinated.SubmitAttempts++

	if coordinated.SubmitAttempts >= 3 {
		delete(c.pendingBlocks, blockHash)
		log.Warn("Block coordination failed after max attempts",
			"hash", blockHash.Hex(),
			"attempts", coordinated.SubmitAttempts)
		return
	}

	if coordinated.BHCAuxData != nil {
		log.Info("Merge mining coordination complete",
			"bhte_hash", blockHash.Hex(),
			"bhc_hash", coordinated.BHCBlockHash.Hex(),
			"attempts", coordinated.SubmitAttempts)
	}

	delete(c.pendingBlocks, blockHash)
}

func (c *MergeMiningCoordinator) cleanupPendingBlocks() {
	ticker := time.NewTicker(1 * time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-c.ctx.Done():
			return
		case <-ticker.C:
			c.removeStalePendingBlocks()
		}
	}
}

func (c *MergeMiningCoordinator) removeStalePendingBlocks() {
	c.coordinationMu.Lock()
	defer c.coordinationMu.Unlock()

	now := time.Now()
	for hash, block := range c.pendingBlocks {
		if now.Sub(block.CreatedAt) > MaxMergeMiningDelay*2 {
			delete(c.pendingBlocks, hash)
			log.Debug("Removed stale pending block", "hash", hash.Hex())
		}
	}
}

func (c *MergeMiningCoordinator) registerMiner(minerID string, minerPubKey []byte) error {
	c.limiterMu.Lock()
	defer c.limiterMu.Unlock()

	now := time.Now()
	if attempt, exists := c.registrationLimiter[minerID]; exists {
		if now.Sub(attempt.FirstSeen) < time.Hour {
			if attempt.Count >= MaxMinerRegistrationsPerHour {
				return fmt.Errorf("miner registration rate limited: max %d per hour", MaxMinerRegistrationsPerHour)
			}
			attempt.Count++
		} else {
			attempt.Count = 1
			attempt.FirstSeen = now
		}
	} else {
		c.registrationLimiter[minerID] = &RegistrationAttempt{
			Count:     1,
			FirstSeen: now,
		}
	}

	verifiedPoW, err := c.verifyMinerPoW(minerPubKey)
	if err != nil {
		c.minersMu.Lock()
		if miner, exists := c.activeMiners[minerID]; exists {
			miner.RegistrationFailures++
		}
		c.minersMu.Unlock()
		return fmt.Errorf("PoW verification failed: %w", err)
	}

	if verifiedPoW.Cmp(c.powThreshold) < 0 {
		return fmt.Errorf("insufficient PoW: have %s, need %s", verifiedPoW.String(), c.powThreshold.String())
	}

	c.minersMu.Lock()
	defer c.minersMu.Unlock()

	c.activeMiners[minerID] = &MinerStatus{
		MinerID:              minerID,
		LastShareTime:        time.Now(),
		SharesPerHour:        0,
		TotalShares:          0,
		VerifiedPoW:          verifiedPoW,
		RegisteredAt:         now,
		LastPoWCheck:         now,
		RegistrationFailures: 0,
		Active:               true,
	}

	log.Info("Miner registered for merge mining", "miner_id", minerID, "verified_pow", verifiedPoW.String())
	return nil
}

func (c *MergeMiningCoordinator) verifyMinerPoW(minerPubKey []byte) (*big.Int, error) {
	hash := crypto.Keccak256Hash(minerPubKey)

	hashBig := new(big.Int).SetBytes(hash.Bytes())

	target := new(big.Int).Div(c.powThreshold, big.NewInt(1000))

	if hashBig.Cmp(target) >= 0 {
		return big.NewInt(0), fmt.Errorf("hash does not meet difficulty target")
	}

	return hashBig, nil
}

func (c *MergeMiningCoordinator) verifyShareDifficulty(shareHash common.Hash, targetDiff *big.Int) bool {
	hashBig := new(big.Int).SetBytes(shareHash.Bytes())

	maxHash := new(big.Int).Sub(c.powThreshold, big.NewInt(1))

	if hashBig.Cmp(maxHash) > 0 {
		return false
	}

	requiredZeros := 0
	targetBytes := targetDiff.Bytes()
	for i := range targetBytes {
		if targetBytes[i] == 0 {
			requiredZeros += 2
		} else if targetBytes[i] == 1 {
			break
		} else {
			break
		}
	}

	hashBytes := shareHash.Bytes()
	leadingZeros := 0
	for _, b := range hashBytes {
		if b == 0 {
			leadingZeros += 2
		} else {
			break
		}
	}

	return leadingZeros >= requiredZeros
}

func (c *MergeMiningCoordinator) setDifficulty(newDiff *big.Int) {
	c.coordinationMu.Lock()
	defer c.coordinationMu.Unlock()

	if newDiff.Cmp(big.NewInt(MinPoWDifficulty)) < 0 {
		newDiff = big.NewInt(MinPoWDifficulty)
	}

	c.powThreshold = newDiff
	log.Info("Updated merge mining difficulty", "new_threshold", newDiff.String())
}

func (c *MergeMiningCoordinator) verifyShare(minerID string, shareDiff *big.Int) bool {
	c.minersMu.RLock()
	miner, exists := c.activeMiners[minerID]
	c.minersMu.RUnlock()

	if !exists || !miner.Active {
		return false
	}

	if miner.VerifiedPoW == nil {
		return false
	}

	requiredDiff := new(big.Int).Div(miner.VerifiedPoW, big.NewInt(100))

	return shareDiff.Cmp(requiredDiff) >= 0
}

func (c *MergeMiningCoordinator) isMinerAllowed(minerID string) bool {
	c.minersMu.RLock()
	defer c.minersMu.RUnlock()

	miner, exists := c.activeMiners[minerID]
	if !exists {
		return false
	}

	if !miner.Active {
		return false
	}

	if miner.RegistrationFailures >= 3 {
		return false
	}

	if miner.SharesPerHour < MinSharesPerHourForActive {
		return false
	}

	return true
}

func (c *MergeMiningCoordinator) submitShare(minerID string, blockHash common.Hash, shareDiff *big.Int) error {
	c.minersMu.Lock()
	defer c.minersMu.Unlock()

	miner, exists := c.activeMiners[minerID]
	if !exists {
		return fmt.Errorf("unknown miner: %s", minerID)
	}

	if !c.verifyShare(minerID, shareDiff) {
		return fmt.Errorf("share does not meet PoW threshold")
	}

	miner.LastShareTime = time.Now()
	miner.TotalShares++

	elapsed := time.Since(miner.RegisteredAt)
	if elapsed > 0 {
		hours := elapsed.Hours()
		if hours > 0 {
			miner.SharesPerHour = float64(miner.TotalShares) / hours
		}
	}

	c.coordinationMu.RLock()
	coordinated, exists := c.pendingBlocks[blockHash]
	c.coordinationMu.RUnlock()

	if !exists {
		return nil
	}

	if shareDiff.Cmp(coordinated.BHTEHeader.Difficulty) >= 0 {
		c.coordinationMu.Lock()
		coordinated.MinedAt = time.Now()
		c.coordinationMu.Unlock()

		log.Info("Valid share submitted for merge mining",
			"miner", minerID,
			"block", blockHash.Hex())

		c.finalizeCoordination(blockHash)
	}

	return nil
}

func (c *MergeMiningCoordinator) monitorMinerHealth() {
	ticker := time.NewTicker(5 * time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-c.ctx.Done():
			return
		case <-ticker.C:
			c.checkMinerHealth()
		}
	}
}

func (c *MergeMiningCoordinator) checkMinerHealth() {
	c.minersMu.Lock()
	defer c.minersMu.Unlock()

	now := time.Now()
	staleThreshold := 30 * time.Minute

	for id, miner := range c.activeMiners {
		if now.Sub(miner.LastShareTime) > staleThreshold {
			miner.Active = false
			log.Warn("Miner went stale", "miner_id", id)
		}
	}
}

func (c *MergeMiningCoordinator) GetStats() *CoordinatorStats {
	c.coordinationMu.RLock()
	defer c.coordinationMu.RUnlock()

	c.minersMu.RLock()
	defer c.minersMu.RUnlock()

	activeCount := 0
	for _, miner := range c.activeMiners {
		if miner.Active {
			activeCount++
		}
	}

	return &CoordinatorStats{
		PendingBlocks: len(c.pendingBlocks),
		ActiveMiners:  activeCount,
		TotalMiners:   len(c.activeMiners),
		Enabled:       c.enabled,
	}
}

type RegistrationAttempt struct {
	Count     int
	FirstSeen time.Time
}

type CoordinatorStats struct {
	PendingBlocks int  `json:"pendingBlocks"`
	ActiveMiners  int  `json:"activeMiners"`
	TotalMiners   int  `json:"totalMiners"`
	Enabled       bool `json:"enabled"`
}
