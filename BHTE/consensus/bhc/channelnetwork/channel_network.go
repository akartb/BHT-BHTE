// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// State Channel Network - Multi-hop HTLC and Network Topology for BHTE

package channelnetwork

import (
	"crypto/rand"
	"crypto/sha256"
	"encoding/binary"
	"fmt"
	"sync"
	"time"

	"github.com/ethereum/go-ethereum/common"
	"github.com/ethereum/go-ethereum/log"
)

const (
	HTLCTimeoutDefault = 24 * time.Hour
	HTLCTimeoutMin     = 1 * time.Hour
	HTLCTimeoutMax     = 7 * 24 * time.Hour
	MaxHops            = 5
	MinChannelCapacity = 1000000
)

type HTLCState uint8

const (
	HTLCStatePending HTLCState = iota
	HTLCStateOpen
	HTLCStateSettled
	HTLCStateExpired
	HTLCStateRefunded
)

func (s HTLCState) String() string {
	switch s {
	case HTLCStatePending:
		return "Pending"
	case HTLCStateOpen:
		return "Open"
	case HTLCStateSettled:
		return "Settled"
	case HTLCStateExpired:
		return "Expired"
	case HTLCStateRefunded:
		return "Refunded"
	default:
		return "Unknown"
	}
}

type HTLC struct {
	HTLCID       common.Hash
	ChannelID    common.Hash
	Sender       common.Address
	Receiver     common.Address
	Amount       uint64
	HashLock     [32]byte
	Preimage     [32]byte
	State        HTLCState
	Timeout      time.Time
	CreatedAt    time.Time
	SettledAt    time.Time
	Fee          uint64
	HopAddresses []common.Address
}

type HTLCManager struct {
	htlcs      map[common.Hash]*HTLC
	byChannel  map[common.Hash][]common.Hash
	byHashLock map[[32]byte]common.Hash
	htlcMu     sync.RWMutex
}

func NewHTLCManager() *HTLCManager {
	return &HTLCManager{
		htlcs:      make(map[common.Hash]*HTLC),
		byChannel:  make(map[common.Hash][]common.Hash),
		byHashLock: make(map[[32]byte]common.Hash),
	}
}

func (m *HTLCManager) CreateHTLC(channelID common.Hash, sender, receiver common.Address,
	amount uint64, timeout time.Duration, hashLock [32]byte, fee uint64) (*HTLC, error) {

	if timeout < HTLCTimeoutMin {
		return nil, fmt.Errorf("timeout too short: minimum %v", HTLCTimeoutMin)
	}
	if timeout > HTLCTimeoutMax {
		return nil, fmt.Errorf("timeout too long: maximum %v", HTLCTimeoutMax)
	}

	htlcID := generateHTLCID()

	htlc := &HTLC{
		HTLCID:    htlcID,
		ChannelID: channelID,
		Sender:    sender,
		Receiver:  receiver,
		Amount:    amount,
		HashLock:  hashLock,
		Preimage:  [32]byte{},
		State:     HTLCStatePending,
		Timeout:   time.Now().Add(timeout),
		Fee:       fee,
		CreatedAt: time.Now(),
	}

	m.htlcMu.Lock()
	m.htlcs[htlcID] = htlc
	m.byChannel[channelID] = append(m.byChannel[channelID], htlcID)
	m.byHashLock[hashLock] = htlcID
	m.htlcMu.Unlock()

	log.Info("HTLC created",
		"id", htlcID.Hex(),
		"channel", channelID.Hex(),
		"amount", amount,
		"timeout", timeout)

	return htlc, nil
}

func (m *HTLCManager) OpenHTLC(htlcID common.Hash, preimage [32]byte) error {
	m.htlcMu.Lock()
	defer m.htlcMu.Unlock()

	htlc, exists := m.htlcs[htlcID]
	if !exists {
		return fmt.Errorf("HTLC not found")
	}

	if htlc.State != HTLCStatePending {
		return fmt.Errorf("HTLC not pending: %s", htlc.State.String())
	}

	if time.Now().After(htlc.Timeout) {
		htlc.State = HTLCStateExpired
		return fmt.Errorf("HTLC expired")
	}

	hashLock := sha256.Sum256(preimage[:])
	if hashLock != htlc.HashLock {
		return fmt.Errorf("preimage does not match hash lock")
	}

	htlc.Preimage = preimage
	htlc.State = HTLCStateOpen
	htlc.SettledAt = time.Now()

	log.Info("HTLC opened", "id", htlcID.Hex())

	return nil
}

func (m *HTLCManager) SettleHTLC(htlcID common.Hash) error {
	m.htlcMu.Lock()
	defer m.htlcMu.Unlock()

	htlc, exists := m.htlcs[htlcID]
	if !exists {
		return fmt.Errorf("HTLC not found")
	}

	if htlc.State != HTLCStateOpen {
		return fmt.Errorf("HTLC not open: %s", htlc.State.String())
	}

	htlc.State = HTLCStateSettled

	log.Info("HTLC settled", "id", htlcID.Hex())

	return nil
}

func (m *HTLCManager) RefundHTLC(htlcID common.Hash) error {
	m.htlcMu.Lock()
	defer m.htlcMu.Unlock()

	htlc, exists := m.htlcs[htlcID]
	if !exists {
		return fmt.Errorf("HTLC not found")
	}

	if htlc.State != HTLCStatePending {
		return fmt.Errorf("HTLC not pending: %s", htlc.State.String())
	}

	if time.Now().Before(htlc.Timeout) {
		return fmt.Errorf("HTLC not yet expired")
	}

	htlc.State = HTLCStateRefunded

	log.Info("HTLC refunded", "id", htlcID.Hex())

	return nil
}

func (m *HTLCManager) ExpireHTLCs() {
	m.htlcMu.Lock()
	defer m.htlcMu.Unlock()

	now := time.Now()
	for id, htlc := range m.htlcs {
		if htlc.State == HTLCStatePending && now.After(htlc.Timeout) {
			htlc.State = HTLCStateExpired
			log.Info("HTLC expired", "id", id.Hex())
		}
	}
}

func (m *HTLCManager) GetHTLC(htlcID common.Hash) (*HTLC, error) {
	m.htlcMu.RLock()
	defer m.htlcMu.RUnlock()

	htlc, exists := m.htlcs[htlcID]
	if !exists {
		return nil, fmt.Errorf("HTLC not found")
	}

	return htlc, nil
}

func (m *HTLCManager) GetHTLCByHashLock(hashLock [32]byte) (*HTLC, error) {
	m.htlcMu.RLock()
	defer m.htlcMu.RUnlock()

	htlcID, exists := m.byHashLock[hashLock]
	if !exists {
		return nil, fmt.Errorf("HTLC not found for hash lock")
	}

	return m.htlcs[htlcID], nil
}

func (m *HTLCManager) GetHTLCsByChannel(channelID common.Hash) []*HTLC {
	m.htlcMu.RLock()
	defer m.htlcMu.RUnlock()

	htlcIDs := m.byChannel[channelID]
	htlcs := make([]*HTLC, 0, len(htlcIDs))

	for _, id := range htlcIDs {
		if htlc, exists := m.htlcs[id]; exists {
			htlcs = append(htlcs, htlc)
		}
	}

	return htlcs
}

func (m *HTLCManager) GetPendingHTLCs() []*HTLC {
	m.htlcMu.RLock()
	defer m.htlcMu.RUnlock()

	htlcs := make([]*HTLC, 0)
	for _, htlc := range m.htlcs {
		if htlc.State == HTLCStatePending {
			htlcs = append(htlcs, htlc)
		}
	}

	return htlcs
}

func (m *HTLCManager) GetOpenHTLCs() []*HTLC {
	m.htlcMu.RLock()
	defer m.htlcMu.RUnlock()

	htlcs := make([]*HTLC, 0)
	for _, htlc := range m.htlcs {
		if htlc.State == HTLCStateOpen {
			htlcs = append(htlcs, htlc)
		}
	}

	return htlcs
}

func (m *HTLCManager) GetChannelCapacity(channelID common.Hash, participant common.Address) (uint64, error) {
	m.htlcMu.RLock()
	defer m.htlcMu.RUnlock()

	htlcIDs := m.byChannel[channelID]
	var lockedAmount uint64

	for _, id := range htlcIDs {
		if htlc, exists := m.htlcs[id]; exists {
			if htlc.State == HTLCStatePending || htlc.State == HTLCStateOpen {
				if htlc.Sender == participant {
					lockedAmount += htlc.Amount
				}
			}
		}
	}

	return lockedAmount, nil
}

func generateHTLCID() common.Hash {
	var id common.Hash
	rand.Read(id[:])
	binary.BigEndian.PutUint64(id[24:], uint64(time.Now().UnixNano()))
	return id
}

func GenerateHashLock(preimage [32]byte) [32]byte {
	return sha256.Sum256(preimage[:])
}

type NetworkTopology struct {
	nodes      map[common.Address]*NetworkNode
	channels   map[common.Hash]*NetworkChannel
	topologyMu sync.RWMutex
	lastUpdate time.Time
}

type NetworkNode struct {
	Address           common.Address
	PublicKey         []byte
	Channels          []common.Hash
	TotalCapacity     uint64
	AvailableCapacity uint64
	LastSeen          time.Time
	Score             float64
	Region            string
}

type NetworkChannel struct {
	ChannelID      common.Hash
	Node1          common.Address
	Node2          common.Address
	Capacity       uint64
	Available      uint64
	HTLCCount      uint32
	AvgHTLCSize    uint64
	Uptime         time.Duration
	Latency        time.Duration
	LastUpdateTime time.Time
}

func NewNetworkTopology() *NetworkTopology {
	return &NetworkTopology{
		nodes:    make(map[common.Address]*NetworkNode),
		channels: make(map[common.Hash]*NetworkChannel),
	}
}

func (nt *NetworkTopology) RegisterNode(address common.Address, publicKey []byte, region string) {
	nt.topologyMu.Lock()
	defer nt.topologyMu.Unlock()

	node := &NetworkNode{
		Address:   address,
		PublicKey: publicKey,
		Channels:  []common.Hash{},
		LastSeen:  time.Now(),
		Score:     100.0,
		Region:    region,
	}

	nt.nodes[address] = node
	nt.lastUpdate = time.Now()

	log.Info("Network node registered", "address", address.Hex(), "region", region)
}

func (nt *NetworkTopology) AddChannel(channelID common.Hash, node1, node2 common.Address, capacity uint64) {
	nt.topologyMu.Lock()
	defer nt.topologyMu.Unlock()

	channel := &NetworkChannel{
		ChannelID:      channelID,
		Node1:          node1,
		Node2:          node2,
		Capacity:       capacity,
		Available:      capacity,
		HTLCCount:      0,
		AvgHTLCSize:    0,
		Uptime:         0,
		LastUpdateTime: time.Now(),
	}

	nt.channels[channelID] = channel

	if node, exists := nt.nodes[node1]; exists {
		node.Channels = append(node.Channels, channelID)
		node.TotalCapacity += capacity
		node.AvailableCapacity += capacity
	}

	if node, exists := nt.nodes[node2]; exists {
		node.Channels = append(node.Channels, channelID)
		node.TotalCapacity += capacity
		node.AvailableCapacity += capacity
	}

	nt.lastUpdate = time.Now()
}

func (nt *NetworkTopology) FindPath(sender, receiver common.Address, amount uint64) ([]common.Address, error) {
	nt.topologyMu.RLock()
	defer nt.topologyMu.RUnlock()

	if _, exists := nt.nodes[sender]; !exists {
		return nil, fmt.Errorf("sender not in network")
	}
	if _, exists := nt.nodes[receiver]; !exists {
		return nil, fmt.Errorf("receiver not in network")
	}

	path := nt.dijkstraPath(sender, receiver, amount)
	if len(path) == 0 || len(path) > MaxHops+2 {
		return nil, fmt.Errorf("no path found with available capacity")
	}

	return path, nil
}

func (nt *NetworkTopology) dijkstraPath(sender, receiver common.Address, amount uint64) []common.Address {
	type pathState struct {
		node     common.Address
		path     []common.Address
		capacity uint64
	}

	visited := make(map[common.Address]bool)
	heap := []pathState{{sender, []common.Address{sender}, amount}}

	for len(heap) > 0 {
		current := heap[0]
		heap = heap[1:]

		if visited[current.node] {
			continue
		}
		visited[current.node] = true

		if current.node == receiver {
			return current.path
		}

		node, exists := nt.nodes[current.node]
		if !exists {
			continue
		}

		for _, channelID := range node.Channels {
			channel, exists := nt.channels[channelID]
			if !exists || channel.Available < amount {
				continue
			}

			var neighbor common.Address
			if channel.Node1 == current.node {
				neighbor = channel.Node2
			} else {
				neighbor = channel.Node1
			}

			if visited[neighbor] {
				continue
			}

			newPath := make([]common.Address, len(current.path))
			copy(newPath, current.path)
			newPath = append(newPath, neighbor)

			newCapacity := channel.Available
			if newCapacity > current.capacity {
				newCapacity = current.capacity
			}

			heap = append(heap, pathState{neighbor, newPath, newCapacity})
		}
	}

	return nil
}

func (nt *NetworkTopology) UpdateChannelCapacity(channelID common.Hash, newAvailable uint64) {
	nt.topologyMu.Lock()
	defer nt.topologyMu.Unlock()

	channel, exists := nt.channels[channelID]
	if !exists {
		return
	}

	channel.Available = newAvailable
	channel.LastUpdateTime = time.Now()
	nt.lastUpdate = time.Now()
}

func (nt *NetworkTopology) GetNodeStats(address common.Address) (*NodeStats, error) {
	nt.topologyMu.RLock()
	defer nt.topologyMu.RUnlock()

	node, exists := nt.nodes[address]
	if !exists {
		return nil, fmt.Errorf("node not found")
	}

	var totalHTLCs uint32
	var avgLatency time.Duration

	for _, channelID := range node.Channels {
		if channel, exists := nt.channels[channelID]; exists {
			totalHTLCs += channel.HTLCCount
			avgLatency += channel.Latency
		}
	}

	if len(node.Channels) > 0 {
		avgLatency /= time.Duration(len(node.Channels))
	}

	return &NodeStats{
		Address:           node.Address,
		ChannelCount:      uint32(len(node.Channels)),
		TotalCapacity:     node.TotalCapacity,
		AvailableCapacity: node.AvailableCapacity,
		TotalHTLCs:        totalHTLCs,
		AvgLatency:        avgLatency,
		Uptime:            time.Since(node.LastSeen),
		Score:             node.Score,
		LastSeen:          node.LastSeen,
	}, nil
}

type NodeStats struct {
	Address           common.Address
	ChannelCount      uint32
	TotalCapacity     uint64
	AvailableCapacity uint64
	TotalHTLCs        uint32
	AvgLatency        time.Duration
	Uptime            time.Duration
	Score             float64
	LastSeen          time.Time
}

func (nt *NetworkTopology) GetNetworkStats() *NetworkStats {
	nt.topologyMu.RLock()
	defer nt.topologyMu.RUnlock()

	var totalCapacity, availableCapacity uint64
	var totalChannels, activeHTLCs uint32

	for _, channel := range nt.channels {
		totalCapacity += channel.Capacity
		availableCapacity += channel.Available
		totalChannels++
		activeHTLCs += channel.HTLCCount
	}

	return &NetworkStats{
		NodeCount:             uint32(len(nt.nodes)),
		ChannelCount:          totalChannels,
		TotalCapacity:         totalCapacity,
		AvailableCapacity:     availableCapacity,
		ActiveHTLCs:           activeHTLCs,
		AvgChannelUtilization: float64(totalCapacity-availableCapacity) / float64(totalCapacity),
		LastUpdate:            nt.lastUpdate,
	}
}

type NetworkStats struct {
	NodeCount             uint32
	ChannelCount          uint32
	TotalCapacity         uint64
	AvailableCapacity     uint64
	ActiveHTLCs           uint32
	AvgChannelUtilization float64
	LastUpdate            time.Time
}
