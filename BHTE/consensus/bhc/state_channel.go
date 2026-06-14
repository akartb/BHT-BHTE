// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// Payment Channel Management for BHTE Layer 2

package bhc

import (
	"crypto/rand"
	"crypto/sha256"
	"encoding/binary"
	"fmt"
	"sync"
	"time"

	"github.com/ethereum/go-ethereum/common"
	"github.com/ethereum/go-ethereum/crypto"
)

const (
	ChannelTimeout    = 24 * time.Hour
	ChallengePeriod   = 7 * 24 * time.Hour
	MaxChannelAmount  = uint64(1_000_000_000_000_000_000)
	MaxChannelBalance = uint64(1_000_000_000_000_000_000)
)

type ChannelState uint8

const (
	ChannelStateOpen ChannelState = iota
	ChannelStateClosing
	ChannelStateClosed
	ChannelStateDisputed
)

func (s ChannelState) String() string {
	switch s {
	case ChannelStateOpen:
		return "Open"
	case ChannelStateClosing:
		return "Closing"
	case ChannelStateClosed:
		return "Closed"
	case ChannelStateDisputed:
		return "Disputed"
	default:
		return "Unknown"
	}
}

type PaymentChannel struct {
	ChannelID     common.Hash
	Participants  [2]common.Address
	Balance       [2]uint64
	Sequence      uint64
	State         ChannelState
	OpenTime      time.Time
	CloseTime     time.Time
	ChallengeEnd  time.Time
	DisputeNonce  uint64
	TotalSent     [2]uint64
	TotalReceived [2]uint64
}

type ChannelManager struct {
	channels      map[common.Hash]*PaymentChannel
	byParticipant map[common.Address][]common.Hash
	byState       map[ChannelState][]common.Hash
	mu            sync.RWMutex
}

func NewChannelManager() *ChannelManager {
	return &ChannelManager{
		channels:      make(map[common.Hash]*PaymentChannel),
		byParticipant: make(map[common.Address][]common.Hash),
		byState:       make(map[ChannelState][]common.Hash),
	}
}

func (m *ChannelManager) OpenChannel(p1, p2 common.Address, initialBalance uint64) (*PaymentChannel, error) {
	if p1 == p2 {
		return nil, fmt.Errorf("cannot create channel with same participant")
	}

	if initialBalance > MaxChannelAmount {
		return nil, fmt.Errorf("initial balance exceeds maximum: %d > %d", initialBalance, MaxChannelAmount)
	}

	channelID := generateChannelID()

	channel := &PaymentChannel{
		ChannelID:    channelID,
		Participants: [2]common.Address{p1, p2},
		Balance:      [2]uint64{initialBalance, 0},
		Sequence:     0,
		State:        ChannelStateOpen,
		OpenTime:     time.Now(),
	}

	m.mu.Lock()
	m.channels[channelID] = channel
	m.byParticipant[p1] = append(m.byParticipant[p1], channelID)
	m.byParticipant[p2] = append(m.byParticipant[p2], channelID)
	m.byState[ChannelStateOpen] = append(m.byState[ChannelStateOpen], channelID)
	m.mu.Unlock()

	return channel, nil
}

func (m *ChannelManager) GetChannel(channelID common.Hash) (*PaymentChannel, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	channel, exists := m.channels[channelID]
	if !exists {
		return nil, fmt.Errorf("channel not found: %s", channelID.Hex())
	}

	return channel, nil
}

func (m *ChannelManager) UpdateBalance(channelID common.Hash, newBalance [2]uint64, seq uint64, signature []byte) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	channel, exists := m.channels[channelID]
	if !exists {
		return fmt.Errorf("channel not found")
	}

	if channel.State != ChannelStateOpen {
		return fmt.Errorf("channel not open: %s", channel.State.String())
	}

	if seq <= channel.Sequence {
		return fmt.Errorf("invalid sequence: %d <= %d", seq, channel.Sequence)
	}

	totalOld := channel.Balance[0] + channel.Balance[1]
	totalNew := newBalance[0] + newBalance[1]

	if totalNew != totalOld {
		return fmt.Errorf("balance mismatch: total changed from %d to %d", totalOld, totalNew)
	}

	for i, bal := range newBalance {
		if bal > MaxChannelBalance {
			return fmt.Errorf("balance %d exceeds maximum: %d", i, MaxChannelBalance)
		}
	}

	if !m.verifyChannelSignature(channel, newBalance, seq, signature) {
		return fmt.Errorf("invalid signature")
	}

	channel.Balance = newBalance
	channel.Sequence = seq

	return nil
}

func (m *ChannelManager) CloseChannel(channelID common.Hash, initiator uint8) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	channel, exists := m.channels[channelID]
	if !exists {
		return fmt.Errorf("channel not found")
	}

	if channel.State != ChannelStateOpen {
		return fmt.Errorf("channel not open: %s", channel.State.String())
	}

	if initiator > 1 {
		return fmt.Errorf("invalid initiator: %d", initiator)
	}

	channel.State = ChannelStateClosing
	channel.CloseTime = time.Now()
	channel.ChallengeEnd = time.Now().Add(ChallengePeriod)

	m.updateStateIndex(channelID, ChannelStateOpen, ChannelStateClosing)

	return nil
}

func (m *ChannelManager) ConfirmClose(channelID common.Hash) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	channel, exists := m.channels[channelID]
	if !exists {
		return fmt.Errorf("channel not found")
	}

	if channel.State != ChannelStateClosing {
		return fmt.Errorf("channel not closing: %s", channel.State.String())
	}

	if time.Now().Before(channel.ChallengeEnd) {
		return fmt.Errorf("challenge period not ended")
	}

	channel.State = ChannelStateClosed

	m.updateStateIndex(channelID, ChannelStateClosing, ChannelStateClosed)

	return nil
}

func (m *ChannelManager) DisputeChannel(channelID common.Hash) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	channel, exists := m.channels[channelID]
	if !exists {
		return fmt.Errorf("channel not found")
	}

	if channel.State == ChannelStateClosed {
		return fmt.Errorf("channel already closed")
	}

	channel.State = ChannelStateDisputed
	channel.DisputeNonce++

	if channel.State == ChannelStateClosing {
		m.updateStateIndex(channelID, ChannelStateClosing, ChannelStateDisputed)
	} else if channel.State == ChannelStateOpen {
		m.updateStateIndex(channelID, ChannelStateOpen, ChannelStateDisputed)
	}

	return nil
}

func (m *ChannelManager) GetChannelsByParticipant(addr common.Address) []*PaymentChannel {
	m.mu.RLock()
	defer m.mu.RUnlock()

	channelIDs := m.byParticipant[addr]
	channels := make([]*PaymentChannel, 0, len(channelIDs))

	for _, id := range channelIDs {
		if ch, exists := m.channels[id]; exists {
			channels = append(channels, ch)
		}
	}

	return channels
}

func (m *ChannelManager) GetChannelsByState(state ChannelState) []*PaymentChannel {
	m.mu.RLock()
	defer m.mu.RUnlock()

	channelIDs := m.byState[state]
	channels := make([]*PaymentChannel, 0, len(channelIDs))

	for _, id := range channelIDs {
		if ch, exists := m.channels[id]; exists {
			channels = append(channels, ch)
		}
	}

	return channels
}

func (m *ChannelManager) GetOpenChannels() []*PaymentChannel {
	return m.GetChannelsByState(ChannelStateOpen)
}

func (m *ChannelManager) GetAllChannels() []*PaymentChannel {
	m.mu.RLock()
	defer m.mu.RUnlock()

	channels := make([]*PaymentChannel, 0, len(m.channels))
	for _, ch := range m.channels {
		channels = append(channels, ch)
	}

	return channels
}

func (m *ChannelManager) verifyChannelSignature(channel *PaymentChannel, newBalance [2]uint64, seq uint64, signature []byte) bool {
	if len(signature) == 0 {
		return false
	}

	if len(signature) < 64 {
		return false
	}

	if len(channel.Participants) != 2 {
		return false
	}

	sigData := fmt.Sprintf("%s%d%d%d", channel.ChannelID.Hex(), newBalance[0], newBalance[1], seq)
	hash := sha256.Sum256([]byte(sigData))

	signerPubKey, err := crypto.SigToPub(hash[:], signature)
	if err != nil {
		return false
	}

	signerAddr := crypto.PubkeyToAddress(*signerPubKey)

	if signerAddr != channel.Participants[0] && signerAddr != channel.Participants[1] {
		return false
	}

	return true
}

func (m *ChannelManager) updateStateIndex(channelID common.Hash, from, to ChannelState) {
	oldList := m.byState[from]
	newList := make([]common.Hash, 0, len(oldList))
	for _, id := range oldList {
		if id != channelID {
			newList = append(newList, id)
		}
	}
	m.byState[from] = newList

	m.byState[to] = append(m.byState[to], channelID)
}

func generateChannelID() common.Hash {
	var id common.Hash
	rand.Read(id[:])
	binary.BigEndian.PutUint64(id[24:], uint64(time.Now().UnixNano()))
	return id
}

type ChannelBalance struct {
	Participant   common.Address
	Balance       uint64
	TotalSent     uint64
	TotalReceived uint64
}

func (c *PaymentChannel) GetBalance(addr common.Address) (uint64, error) {
	if c.Participants[0] == addr {
		return c.Balance[0], nil
	}
	if c.Participants[1] == addr {
		return c.Balance[1], nil
	}
	return 0, fmt.Errorf("address not a participant")
}

func (c *PaymentChannel) GetParticipantIndex(addr common.Address) (uint8, error) {
	if c.Participants[0] == addr {
		return 0, nil
	}
	if c.Participants[1] == addr {
		return 1, nil
	}
	return 0, fmt.Errorf("address not a participant")
}

func (c *PaymentChannel) IsClosed() bool {
	return c.State == ChannelStateClosed
}

func (c *PaymentChannel) IsOpen() bool {
	return c.State == ChannelStateOpen
}

func (c *PaymentChannel) CanClose() bool {
	return c.State == ChannelStateOpen || c.State == ChannelStateClosing
}
