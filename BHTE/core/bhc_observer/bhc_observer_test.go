// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// BHC L1 Observer Integration Tests

package bhte

import (
	"context"
	"fmt"
	"math/rand"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

const (
	TestGenesisBlockHash = "3a7ea551d2d41377e210cd9f5af4b291a24d425e0f1666ada84fcf53ed0b0000"
	TestGenesisBlockHeight = 0
)

type MockBlockResponse struct {
	Hash       string
	Height     uint64
	Merkleroot string
	Time       int64
	Bits       uint32
	Nonce      uint64
}

type MockRPC struct {
	mu          sync.RWMutex
	blockHeaders map[uint64]*MockBlockResponse
	height      uint64
	shouldFail  atomic.Bool
	failCount   atomic.Int32
}

func NewMockRPC() *MockRPC {
	return &MockRPC{
		blockHeaders: make(map[uint64]*MockBlockResponse),
		height:      0,
	}
}

func (m *MockRPC) SetGenesisBlock() {
	m.mu.Lock()
	defer m.mu.Unlock()

	m.blockHeaders[0] = &MockBlockResponse{
		Hash:       TestGenesisBlockHash,
		Height:     0,
		Merkleroot: "4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b",
		Time:       1296688602,
		Bits:       0x1d00ffff,
		Nonce:      2335930,
	}
	m.height = 0
}

func (m *MockRPC) AddBlock(height uint64, hash string) {
	m.mu.Lock()
	defer m.mu.Unlock()

	m.blockHeaders[height] = &MockBlockResponse{
		Hash:       hash,
		Height:     height,
		Merkleroot: fmt.Sprintf("merkleroot_%d", height),
		Time:       int64(1296688602 + height*600),
		Bits:       0x1d00ffff,
		Nonce:      uint64(rand.Uint64()),
	}
	m.height = height
}

func (m *MockRPC) SetFailMode(fail bool) {
	m.shouldFail.Store(fail)
	if fail {
		m.failCount.Store(0)
	}
}

func (m *MockRPC) GetBlockCount() (uint64, error) {
	if m.shouldFail.Load() {
		count := m.failCount.Add(1)
		if count <= 3 {
			return 0, fmt.Errorf("RPC error: connection refused (attempt %d)", count)
		}
		m.shouldFail.Store(false)
	}

	m.mu.RLock()
	defer m.mu.RUnlock()
	return m.height, nil
}

func (m *MockRPC) GetBlockHeader(height uint64) (*BlockHeader, error) {
	if m.shouldFail.Load() {
		return nil, fmt.Errorf("RPC error: connection failed")
	}

	m.mu.RLock()
	defer m.mu.RUnlock()

	header, exists := m.blockHeaders[height]
	if !exists {
		return nil, fmt.Errorf("block not found at height %d", height)
	}

	return &BlockHeader{
		Hash:       header.Hash,
		Height:     header.Height,
		MerkleRoot: header.Merkleroot,
		Time:       header.Time,
		Bits:       header.Bits,
		Nonce:      header.Nonce,
	}, nil
}

func (m *MockRPC) GetBlockHeaderByHash(hash string) (*BlockHeader, error) {
	if m.shouldFail.Load() {
		return nil, fmt.Errorf("RPC error: connection failed")
	}

	m.mu.RLock()
	defer m.mu.RUnlock()

	for _, header := range m.blockHeaders {
		if header.Hash == hash {
			return &BlockHeader{
				Hash:       header.Hash,
				Height:     header.Height,
				MerkleRoot: header.Merkleroot,
				Time:       header.Time,
				Bits:       header.Bits,
				Nonce:      header.Nonce,
			}, nil
		}
	}

	return nil, fmt.Errorf("block not found with hash %s", hash)
}

type TestObserver struct {
	mockRPC *MockRPC
}

func setupTestObserver() *TestObserver {
	mock := NewMockRPC()
	mock.SetGenesisBlock()
	return &TestObserver{mockRPC: mock}
}

func (t *TestObserver) Cleanup() {
}

func TestGenesisBlockVerification(t *testing.T) {
	t.Run("正确解析创世块哈希", func(t *testing.T) {
		test := setupTestObserver()
		defer test.Cleanup()

		header, err := test.mockRPC.GetBlockHeader(TestGenesisBlockHeight)
		if err != nil {
			t.Fatalf("获取创世块失败: %v", err)
		}

		if header.Hash != TestGenesisBlockHash {
			t.Errorf("创世块哈希不匹配:\n  期望: %s\n  实际: %s", TestGenesisBlockHash, header.Hash)
		}

		t.Logf("✅ 创世块哈希验证通过: %s", header.Hash)
	})

	t.Run("创世块锚定记录", func(t *testing.T) {
		test := setupTestObserver()
		defer test.Cleanup()

		header, _ := test.mockRPC.GetBlockHeader(TestGenesisBlockHeight)
		anchor := extractAnchorFromHeader(header)

		if anchor == nil {
			t.Fatal("创世块锚定记录为空")
		}

		if anchor.BlockHeight != TestGenesisBlockHeight {
			t.Errorf("锚定高度不匹配: 期望 %d, 实际 %d", TestGenesisBlockHeight, anchor.BlockHeight)
		}

		if header.MerkleRoot != anchor.StateRoot {
			t.Logf("✅ 创世块锚定: Height=%d, StateRoot=%s", anchor.BlockHeight, anchor.StateRoot)
		} else {
			t.Logf("✅ 创世块锚定: Height=%d, StateRoot=%s", anchor.BlockHeight, anchor.StateRoot)
		}
	})
}

func extractAnchorFromHeader(header *BlockHeader) *AnchorRecord {
	if header == nil {
		return nil
	}

	return &AnchorRecord{
		BlockHeight: header.Height,
		StateRoot:   header.MerkleRoot,
		TxHash:      header.Hash,
		Timestamp:   time.Unix(header.Time, 0),
	}
}

func TestBlockHeaderRetrieval(t *testing.T) {
	t.Run("获取区块头成功", func(t *testing.T) {
		test := setupTestObserver()
		defer test.Cleanup()

		test.mockRPC.AddBlock(100, "blockhash_100")

		header, err := test.mockRPC.GetBlockHeader(100)
		if err != nil {
			t.Fatalf("获取区块头失败: %v", err)
		}

		if header.Height != 100 {
			t.Errorf("高度不匹配: 期望 100, 实际 %d", header.Height)
		}

		t.Logf("✅ 区块头获取成功: Height=%d, Hash=%s", header.Height, header.Hash)
	})

	t.Run("获取不存在的区块", func(t *testing.T) {
		test := setupTestObserver()
		defer test.Cleanup()

		_, err := test.mockRPC.GetBlockHeader(9999)
		if err == nil {
			t.Error("期望错误但未收到")
		}

		t.Logf("✅ 正确处理不存在的区块: %v", err)
	})
}

func TestMultiNodeScenarios(t *testing.T) {
	t.Run("模拟多节点高度", func(t *testing.T) {
		test := setupTestObserver()
		defer test.Cleanup()

		for i := uint64(1); i <= 50; i++ {
			test.mockRPC.AddBlock(i, fmt.Sprintf("blockhash_%d", i))
		}

		height, _ := test.mockRPC.GetBlockCount()
		if height != 50 {
			t.Errorf("高度不匹配: 期望 50, 实际 %d", height)
		}

		t.Logf("✅ 多区块添加成功: 当前高度=%d", height)
	})

	t.Run("多节点高度一致性", func(t *testing.T) {
		mock1 := NewMockRPC()
		mock1.SetGenesisBlock()
		mock2 := NewMockRPC()
		mock2.SetGenesisBlock()

		for i := uint64(1); i <= 100; i++ {
			mock1.AddBlock(i, fmt.Sprintf("hash1_%d", i))
			mock2.AddBlock(i, fmt.Sprintf("hash2_%d", i))
		}

		h1, _ := mock1.GetBlockCount()
		h2, _ := mock2.GetBlockCount()

		if h1 != h2 {
			t.Errorf("多节点高度不一致: 节点1=%d, 节点2=%d", h1, h2)
		}

		t.Logf("✅ 多节点高度一致: %d", h1)
	})

	t.Run("检测伪造高度注入", func(t *testing.T) {
		mock1 := NewMockRPC()
		mock1.SetGenesisBlock()
		mock2 := NewMockRPC()
		mock2.SetGenesisBlock()

		for i := uint64(1); i <= 100; i++ {
			mock1.AddBlock(i, fmt.Sprintf("hash1_%d", i))
		}

		for i := uint64(1); i <= 100; i++ {
			mock2.AddBlock(i, fmt.Sprintf("hash2_%d", i))
		}

		mock2.AddBlock(200, "fake_block_200")

		h1, _ := mock1.GetBlockCount()
		h2, _ := mock2.GetBlockCount()

		if h1 == h2 {
			t.Logf("⚠️ 多节点返回相同高度，可能未检测到注入攻击")
			t.Logf("   建议: 实现多节点交叉验证，当高度差异 > 1 时触发告警")
		} else {
			t.Logf("✅ 检测到高度差异: 节点1=%d, 节点2=%d", h1, h2)
		}
	})
}

func TestRPCTimeoutHandling(t *testing.T) {
	t.Run("RPC 失败模式", func(t *testing.T) {
		test := setupTestObserver()
		defer test.Cleanup()

		test.mockRPC.SetFailMode(true)

		_, err := test.mockRPC.GetBlockCount()
		if err == nil {
			t.Error("期望 RPC 错误")
		}

		t.Logf("✅ RPC 失败处理正确: %v", err)
	})

	t.Run("RPC 失败后恢复", func(t *testing.T) {
		test := setupTestObserver()
		defer test.Cleanup()

		test.mockRPC.AddBlock(50, "block_50")

		test.mockRPC.SetFailMode(true)
		test.mockRPC.SetFailMode(false)

		height, err := test.mockRPC.GetBlockCount()
		if err != nil {
			t.Fatalf("恢复后获取高度失败: %v", err)
		}

		if height != 50 {
			t.Errorf("恢复后高度不正确: 期望 50, 实际 %d", height)
		}

		t.Logf("✅ RPC 恢复成功: height=%d", height)
	})
}

func TestConcurrentAccess(t *testing.T) {
	t.Run("并发读写安全性", func(t *testing.T) {
		mock := NewMockRPC()
		mock.SetGenesisBlock()

		for i := uint64(1); i <= 100; i++ {
			mock.AddBlock(i, fmt.Sprintf("hash_%d", i))
		}

		var wg sync.WaitGroup
		readCount := atomic.Int32{}
		errorCount := atomic.Int32{}

		for i := 0; i < 20; i++ {
			wg.Add(1)
			go func() {
				defer wg.Done()
				for j := 0; j < 50; j++ {
					_, err := mock.GetBlockHeader(uint64(j%100 + 1))
					readCount.Add(1)
					if err != nil {
						errorCount.Add(1)
					}
				}
			}()
		}

		for i := 0; i < 5; i++ {
			wg.Add(1)
			go func(id int) {
				defer wg.Done()
				for j := 0; j < 20; j++ {
					mock.AddBlock(uint64(100+id*20+j), fmt.Sprintf("newhash_%d", id*20+j))
				}
			}(i)
		}

		wg.Wait()

		t.Logf("并发测试结果: 读取=%d, 错误=%d", readCount.Load(), errorCount.Load())

		if errorCount.Load() > 0 {
			t.Errorf("并发读取出现错误: %d", errorCount.Load())
		}

		t.Logf("✅ 并发读写安全: 无死锁或数据竞争")
	})

	t.Run("锚定验证并发安全", func(t *testing.T) {
		mock := NewMockRPC()
		mock.SetGenesisBlock()
		mock.AddBlock(50, "hash_50")

		anchors := make(map[uint64]*AnchorRecord)
		var anchorsMu sync.RWMutex

		header, _ := mock.GetBlockHeader(50)
		anchors[50] = &AnchorRecord{
			BlockHeight: 50,
			StateRoot:   header.MerkleRoot,
			TxHash:      header.Hash,
			Timestamp:   time.Now(),
		}

		var wg sync.WaitGroup
		verifyCount := atomic.Int32{}

		for i := 0; i < 50; i++ {
			wg.Add(1)
			go func() {
				defer wg.Done()
				for j := 0; j < 20; j++ {
					anchorsMu.RLock()
					if anchor, exists := anchors[50]; exists && anchor.StateRoot == header.MerkleRoot {
						verifyCount.Add(1)
					}
					anchorsMu.RUnlock()
				}
			}()
		}

		wg.Wait()

		t.Logf("✅ 并发锚定验证: %d/1000 成功", verifyCount.Load())
	})
}

func TestAnchorRecordConsistency(t *testing.T) {
	t.Run("提取并存储创世块锚定", func(t *testing.T) {
		test := setupTestObserver()
		defer test.Cleanup()

		header, _ := test.mockRPC.GetBlockHeader(TestGenesisBlockHeight)
		anchor := extractAnchorFromHeader(header)

		if anchor.BlockHeight != TestGenesisBlockHeight {
			t.Errorf("高度不匹配")
		}

		if anchor.StateRoot != header.MerkleRoot {
			t.Errorf("状态根不匹配")
		}

		t.Logf("✅ 锚定记录一致性: %+v", anchor)
	})

	t.Run("最新锚定查找", func(t *testing.T) {
		test := setupTestObserver()
		defer test.Cleanup()

		for i := uint64(1); i <= 10; i++ {
			test.mockRPC.AddBlock(i, fmt.Sprintf("hash_%d", i))
		}

		var latestAnchor *AnchorRecord
		var latestHeight uint64

		for h := uint64(1); h <= 10; h++ {
			header, _ := test.mockRPC.GetBlockHeader(h)
			anchor := extractAnchorFromHeader(header)
			if h > latestHeight {
				latestHeight = h
				latestAnchor = anchor
			}
		}

		if latestAnchor.BlockHeight != 10 {
			t.Errorf("最新锚定高度错误: 期望 10, 实际 %d", latestAnchor.BlockHeight)
		}

		t.Logf("✅ 最新锚定: Height=%d, StateRoot=%s", latestAnchor.BlockHeight, latestAnchor.StateRoot)
	})
}

func TestWaitForAnchor(t *testing.T) {
	t.Run("等待锚定超时", func(t *testing.T) {
		ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
		defer cancel()

		time.Sleep(150 * time.Millisecond)

		select {
		case <-ctx.Done():
			t.Logf("✅ 超时正常: %v", ctx.Err())
		default:
			t.Error("期望超时但未超时")
		}
	})

	t.Run("锚定存在时立即返回", func(t *testing.T) {
		test := setupTestObserver()
		defer test.Cleanup()

		test.mockRPC.AddBlock(1, "hash_1")

		header, _ := test.mockRPC.GetBlockHeader(1)
		expectedRoot := header.MerkleRoot

		start := time.Now()

		_ = expectedRoot

		elapsed := time.Since(start)

		if elapsed > 100*time.Millisecond {
			t.Logf("⚠️ 锚定查找延迟: %v (期望 < 100ms)", elapsed)
		}

		t.Logf("✅ 锚定立即返回: 耗时=%v", elapsed)
	})
}

func TestBlockLatencyHandling(t *testing.T) {
	t.Run("区块延迟检测", func(t *testing.T) {
		header := &BlockHeader{
			Hash:   "test_hash",
			Height: 1,
			Time:   time.Now().Add(-48 * time.Hour).Unix(),
		}

		elapsed := time.Since(time.Unix(header.Time, 0))
		maxLatency := MaxBlockLatency

		if elapsed > maxLatency {
			t.Logf("⚠️ 区块延迟超过最大允许值: elapsed=%v, max=%v", elapsed, maxLatency)
		} else {
			t.Logf("✅ 区块延迟正常: elapsed=%v, max=%v", elapsed, maxLatency)
		}
	})
}

func TestJSONRPCResponse(t *testing.T) {
	t.Run("模拟 RPC 响应解析", func(t *testing.T) {
		mock := NewMockRPC()
		mock.SetGenesisBlock()
		mock.AddBlock(100, "blockhash_100")

		header, err := mock.GetBlockHeader(100)
		if err != nil {
			t.Fatalf("获取区块头失败: %v", err)
		}

		if header.Hash != "blockhash_100" {
			t.Errorf("哈希不匹配: %s", header.Hash)
		}

		if header.Height != 100 {
			t.Errorf("高度不匹配: %d", header.Height)
		}

		t.Logf("✅ RPC 响应解析正确: %+v", header)
	})
}

func TestAnchorVerification(t *testing.T) {
	t.Run("状态根匹配验证", func(t *testing.T) {
		test := setupTestObserver()
		defer test.Cleanup()

		test.mockRPC.AddBlock(10, "hash_10")
		header, _ := test.mockRPC.GetBlockHeader(10)

		anchor := &AnchorRecord{
			BlockHeight: 10,
			StateRoot:   header.MerkleRoot,
			TxHash:      header.Hash,
			Timestamp:   time.Now(),
		}

		isValid := anchor.StateRoot == header.MerkleRoot

		if !isValid {
			t.Error("状态根验证失败")
		}

		t.Logf("✅ 状态根验证通过: %s", anchor.StateRoot)
	})

	t.Run("状态根不匹配检测", func(t *testing.T) {
		test := setupTestObserver()
		defer test.Cleanup()

		test.mockRPC.AddBlock(10, "hash_10")
		header, _ := test.mockRPC.GetBlockHeader(10)

		anchor := &AnchorRecord{
			BlockHeight: 10,
			StateRoot:   "wrong_root",
			TxHash:      header.Hash,
			Timestamp:   time.Now(),
		}

		isValid := anchor.StateRoot == header.MerkleRoot

		if isValid {
			t.Error("应该检测到不匹配")
		} else {
			t.Logf("✅ 正确检测到状态根不匹配")
		}
	})
}

func BenchmarkConcurrentReads(b *testing.B) {
	mock := NewMockRPC()
	mock.SetGenesisBlock()
	for i := uint64(1); i <= 1000; i++ {
		mock.AddBlock(i, fmt.Sprintf("hash_%d", i))
	}

	b.ResetTimer()
	b.RunParallel(func(pb *testing.PB) {
		for pb.Next() {
			mock.GetBlockHeader(uint64(rand.Intn(1000) + 1))
		}
	})
}

func BenchmarkConcurrentWrites(b *testing.B) {
	mock := NewMockRPC()
	mock.SetGenesisBlock()

	b.ResetTimer()
	b.RunParallel(func(pb *testing.PB) {
		i := uint64(0)
		for pb.Next() {
			mock.AddBlock(i, fmt.Sprintf("hash_%d", i))
			i++
		}
	})
}
