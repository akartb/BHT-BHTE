# BHTE zkEVM 裁剪规范 v1.0

## 📋 概述

BHTE (Layer 2) 的 zkEVM 是基于 Go-Ethereum 裁剪的专用支付引擎。本规范定义了如何修改 Geth 执行引擎，使其能原生识别来自 BHC 锚定的混合签名。

---

## 🎯 设计目标

| 目标 | 说明 |
|------|------|
| **支付优先** | 核心支付功能永久保留，禁止高拥堵合约 |
| **BHC锚定** | 原生支持 BHC 状态根验证和 ML-DSA 签名 |
| **低延迟** | 基础交易 10-15 秒确认，状态通道毫秒级 |
| **高吞吐** | 峰值 TPS ≥ 10000 |
| **模块化** | 扩展功能独立于核心支付层 |

---

## 🏗️ 架构设计

### 双态执行环境 (Dual-State Execution)

```
┌─────────────────────────────────────────────────────────────┐
│                    BHTE zkEVM 架构                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              内核态 (Kernel Space)                   │   │
│  │  ┌─────────────────────────────────────────────┐   │   │
│  │  │ • 转账 (Transfer)                            │   │   │
│  │  │ • 多签 (MultiSig)                            │   │   │
│  │  │ • 状态通道开启/关闭 (Channel Open/Close)      │   │   │
│  │  │ • BHC锚定验证 (Anchor Verification)          │   │   │
│  │  │ • ML-DSA签名验证 (Post-Quantum Verify)       │   │   │
│  │  └─────────────────────────────────────────────┘   │   │
│  │  优先级: 最高 | Gas成本: 最低 | 永不拥堵            │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              用户态 (User Space)                     │   │
│  │  ┌─────────────────────────────────────────────┐   │   │
│  │  │ • 自定义合约 (Custom Contracts)              │   │   │
│  │  │ • DeFi 协议                                  │   │   │
│  │  │ • NFT 市场                                   │   │   │
│  │  │ • DAO 治理                                   │   │   │
│  │  └─────────────────────────────────────────────┘   │   │
│  │  优先级: 较低 | Gas成本: 动态调整 | 拥堵时限流      │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 📁 文件修改清单

### 1. 核心执行层修改

| 文件路径 | 修改内容 | 优先级 |
|----------|----------|--------|
| `core/vm/evm.go` | 内核态/用户态分离 | 🔴 高 |
| `core/vm/gas.go` | Gas 波动调节器 | 🔴 高 |
| `core/vm/operations_acl.go` | 操作码访问控制 | 🔴 高 |
| `core/vm/contracts.go` | 预编译合约 | 🔴 高 |
| `core/state/transition.go` | 状态转换逻辑 | 🟡 中 |
| `core/types/transaction.go` | 交易类型扩展 | 🟡 中 |

### 2. 共识层修改

| 文件路径 | 修改内容 | 优先级 |
|----------|----------|--------|
| `consensus/bhc/anchor.go` | BHC 锚定验证 | 🔴 高 |
| `consensus/bhc/mldsa.go` | ML-DSA 预编译 | 🔴 高 |
| `consensus/bhc/validator.go` | 混合签名验证 | 🔴 高 |
| `consensus/bhc/state_channel.go` | 状态通道管理 | 🟡 中 |

### 3. 网络层修改

| 文件路径 | 修改内容 | 优先级 |
|----------|----------|--------|
| `eth/downloader/bhc_sync.go` | BHC 区块同步 | 🟡 中 |
| `p2p/peer_bhc.go` | BHC 节点发现 | 🟡 中 |
| `rpc/bhc_api.go` | BHC RPC 接口 | 🟡 中 |

---

## 🔧 核心代码实现

### 1. 内核态操作码定义

```go
// core/vm/opcodes_kernel.go

package vm

const (
    // 内核态专用操作码 (0xB0 - 0xBF)
    OP_KERNEL_TRANSFER     OpCode = 0xB0  // 原生转账
    OP_KERNEL_MULTISIG     OpCode = 0xB1  // 多签验证
    OP_KERNEL_CHANNEL_OPEN OpCode = 0xB2  // 开启状态通道
    OP_KERNEL_CHANNEL_CLOSE OpCode = 0xB3 // 关闭状态通道
    OP_KERNEL_ANCHOR       OpCode = 0xB4  // BHC锚定验证
    OP_KERNEL_MLDSA        OpCode = 0xB5  // ML-DSA签名验证
    OP_KERNEL_WITHDRAW     OpCode = 0xB6  // L1提款
)

var KernelOpcodes = map[OpCode]bool{
    OP_KERNEL_TRANSFER:     true,
    OP_KERNEL_MULTISIG:     true,
    OP_KERNEL_CHANNEL_OPEN: true,
    OP_KERNEL_CHANNEL_CLOSE:true,
    OP_KERNEL_ANCHOR:       true,
    OP_KERNEL_MLDSA:        true,
    OP_KERNEL_WITHDRAW:     true,
}

func IsKernelOpcode(op OpCode) bool {
    return KernelOpcodes[op]
}
```

### 2. Gas 波动调节器

```go
// core/vm/gas_regulator.go

package vm

import (
    "math"
    "sync/atomic"
)

type GasRegulator struct {
    baseGasPrice      uint64
    kernelGasPrice    uint64
    userGasMultiplier atomic.Uint64
    congestionLevel   atomic.Uint64
    maxMultiplier     uint64
}

func NewGasRegulator(baseGas uint64) *GasRegulator {
    return &GasRegulator{
        baseGasPrice:   baseGas,
        kernelGasPrice: baseGas / 4,  // 内核态 1/4 价格
        maxMultiplier:  16,            // 最大 16 倍
    }
}

func (r *GasRegulator) CalculateGas(op OpCode, baseGas uint64) uint64 {
    if IsKernelOpcode(op) {
        return r.kernelGasPrice * baseGas / r.baseGasPrice
    }
    
    multiplier := r.userGasMultiplier.Load()
    if multiplier < 100 {
        multiplier = 100  // 最小 1x
    }
    
    return baseGas * multiplier / 100
}

func (r *GasRegulator) UpdateCongestion(nonKernelRatio float64) {
    // 如果非内核态操作超过 30%，提高用户态 Gas
    if nonKernelRatio > 0.3 {
        newMultiplier := uint64(nonKernelRatio * 100 / 0.3)
        if newMultiplier > r.maxMultiplier*100 {
            newMultiplier = r.maxMultiplier * 100
        }
        r.userGasMultiplier.Store(newMultiplier)
        r.congestionLevel.Store(uint64(nonKernelRatio * 100))
    } else {
        r.userGasMultiplier.Store(100)  // 恢复正常
        r.congestionLevel.Store(0)
    }
}

func (r *GasRegulator) GetCongestionLevel() uint64 {
    return r.congestionLevel.Load()
}
```

### 3. ML-DSA 预编译合约

```go
// core/vm/contracts_mldsa.go

package vm

import (
    "errors"
    "github.com/ethereum/go-ethereum/common"
)

const (
    MLDSAVerifyGas        uint64 = 50000   // ML-DSA 验证 Gas
    MLDSASignatureSize    int    = 3293    // Level 3 签名大小
    MLDSAPublicKeySize    int    = 1952    // Level 3 公钥大小
    MLDSAMessageSize      int    = 32      // 消息哈希大小
)

var (
    ErrMLDSAInvalidSignatureLength = errors.New("invalid ML-DSA signature length")
    ErrMLDSAInvalidPublicKeyLength = errors.New("invalid ML-DSA public key length")
    ErrMLDSAVerificationFailed     = errors.New("ML-DSA verification failed")
)

type MLDSAPrecompile struct{}

func (c *MLDSAPrecompile) RequiredGas(input []byte) uint64 {
    return MLDSAVerifyGas
}

func (c *MLDSAPrecompile) Run(input []byte) ([]byte, error) {
    // 输入格式: [32字节消息][1952字节公钥][3293字节签名]
    expectedLen := MLDSAMessageSize + MLDSAPublicKeySize + MLDSASignatureSize
    if len(input) < expectedLen {
        return nil, ErrMLDSAInvalidSignatureLength
    }
    
    message := input[:MLDSAMessageSize]
    pubkey := input[MLDSAMessageSize : MLDSAMessageSize+MLDSAPublicKeySize]
    signature := input[MLDSAMessageSize+MLDSAPublicKeySize:]
    
    // 调用 ML-DSA 验证
    valid := verifyMLDSA(message, pubkey, signature)
    if !valid {
        return nil, ErrMLDSAVerificationFailed
    }
    
    // 返回 true (32字节)
    result := make([]byte, 32)
    result[31] = 1
    return result, nil
}

func verifyMLDSA(message, pubkey, signature []byte) bool {
    // 这里调用实际的 ML-DSA 验证库
    // 可以通过 CGO 调用 C++ 实现或使用纯 Go 实现
    return true  // 占位符
}

// 注册预编译合约
func init() {
    // 地址 0x000...0100 用于 ML-DSA 验证
    PrecompiledContractsBerlin[common.BytesToAddress([]byte{0x00, 0x01})] = &MLDSAPrecompile{}
}
```

### 4. BHC 锚定验证器

```go
// consensus/bhc/anchor.go

package bhc

import (
    "context"
    "encoding/hex"
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

type AnchorVerifier struct {
    bhcRPCClient jsonrpc.RPCClient
    anchors      map[uint64]*AnchorRecord
    latestHeight uint64
    mu           sync.RWMutex
}

type AnchorRecord struct {
    BlockHeight uint64
    StateRoot   common.Hash
    TxHash      common.Hash
    Timestamp   time.Time
    Verified    bool
}

func NewAnchorVerifier(bhcNodeURL string) *AnchorVerifier {
    return &AnchorVerifier{
        bhcRPCClient: jsonrpc.NewClient(bhcNodeURL),
        anchors:      make(map[uint64]*AnchorRecord),
    }
}

func (v *AnchorVerifier) VerifyAnchor(stateRoot common.Hash, blockHeight uint64) (bool, error) {
    v.mu.RLock()
    anchor, exists := v.anchors[blockHeight]
    v.mu.RUnlock()
    
    if !exists {
        return false, fmt.Errorf("no anchor at height %d", blockHeight)
    }
    
    // 检查确认数
    v.mu.RLock()
    latest := v.latestHeight
    v.mu.RUnlock()
    
    if latest-anchor.BlockHeight < MinConfirmations {
        return false, fmt.Errorf("insufficient confirmations")
    }
    
    return anchor.StateRoot == stateRoot, nil
}

func (v *AnchorVerifier) SyncAnchors(ctx context.Context) error {
    ticker := time.NewTicker(AnchorCheckInterval)
    defer ticker.Stop()
    
    for {
        select {
        case <-ctx.Done():
            return ctx.Err()
        case <-ticker.C:
            if err := v.fetchLatestAnchors(); err != nil {
                // 记录错误但继续运行
                fmt.Printf("anchor sync error: %v\n", err)
            }
        }
    }
}

func (v *AnchorVerifier) fetchLatestAnchors() error {
    // 从 BHC 节点获取最新区块
    var result struct {
        Height uint64 `json:"height"`
    }
    
    err := v.bhcRPCClient.CallFor(context.Background(), &result, "getblockcount")
    if err != nil {
        return err
    }
    
    v.mu.Lock()
    v.latestHeight = result.Height
    v.mu.Unlock()
    
    return nil
}
```

### 5. 状态通道管理

```go
// consensus/bhc/state_channel.go

package bhc

import (
    "crypto/rand"
    "encoding/binary"
    "sync"
    "time"
    
    "github.com/ethereum/go-ethereum/common"
)

const (
    ChannelTimeout      = 24 * time.Hour
    ChallengePeriod     = 7 * 24 * time.Hour
    MaxChannelAmount    = 1000000  // 最大通道金额
)

type ChannelState uint8

const (
    ChannelStateOpen   ChannelState = iota
    ChannelStateClosing
    ChannelStateClosed
    ChannelStateDisputed
)

type PaymentChannel struct {
    ChannelID       common.Hash
    Participants    [2]common.Address
    Balance         [2]uint64
    Sequence        uint64
    State           ChannelState
    OpenTime        time.Time
    CloseTime       time.Time
    ChallengeEnd    time.Time
    DisputeNonce    uint64
}

type ChannelManager struct {
    channels    map[common.Hash]*PaymentChannel
    byParticipant map[common.Address][]common.Hash
    mu          sync.RWMutex
}

func NewChannelManager() *ChannelManager {
    return &ChannelManager{
        channels:      make(map[common.Hash]*PaymentChannel),
        byParticipant: make(map[common.Address][]common.Hash),
    }
}

func (m *ChannelManager) OpenChannel(p1, p2 common.Address, amount uint64) (*PaymentChannel, error) {
    if amount > MaxChannelAmount {
        return nil, fmt.Errorf("amount exceeds maximum")
    }
    
    channelID := generateChannelID()
    
    channel := &PaymentChannel{
        ChannelID:    channelID,
        Participants: [2]common.Address{p1, p2},
        Balance:      [2]uint64{amount, 0},
        Sequence:     0,
        State:        ChannelStateOpen,
        OpenTime:     time.Now(),
    }
    
    m.mu.Lock()
    m.channels[channelID] = channel
    m.byParticipant[p1] = append(m.byParticipant[p1], channelID)
    m.byParticipant[p2] = append(m.byParticipant[p2], channelID)
    m.mu.Unlock()
    
    return channel, nil
}

func (m *ChannelManager) UpdateBalance(channelID common.Hash, newBalance [2]uint64, seq uint64) error {
    m.mu.Lock()
    defer m.mu.Unlock()
    
    channel, exists := m.channels[channelID]
    if !exists {
        return fmt.Errorf("channel not found")
    }
    
    if channel.State != ChannelStateOpen {
        return fmt.Errorf("channel not open")
    }
    
    if seq <= channel.Sequence {
        return fmt.Errorf("invalid sequence")
    }
    
    totalOld := channel.Balance[0] + channel.Balance[1]
    totalNew := newBalance[0] + newBalance[1]
    
    if totalNew != totalOld {
        return fmt.Errorf("balance mismatch")
    }
    
    channel.Balance = newBalance
    channel.Sequence = seq
    
    return nil
}

func (m *ChannelManager) CloseChannel(channelID common.Hash) error {
    m.mu.Lock()
    defer m.mu.Unlock()
    
    channel, exists := m.channels[channelID]
    if !exists {
        return fmt.Errorf("channel not found")
    }
    
    channel.State = ChannelStateClosing
    channel.CloseTime = time.Now()
    channel.ChallengeEnd = time.Now().Add(ChallengePeriod)
    
    return nil
}

func generateChannelID() common.Hash {
    var id common.Hash
    rand.Read(id[:])
    binary.BigEndian.PutUint64(id[24:], uint64(time.Now().UnixNano()))
    return id
}
```

---

## 📊 性能指标

### 目标性能

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 基础交易确认 | 10-15秒 | 链上交易 |
| 状态通道支付 | <100ms | 链下即时支付 |
| 峰值 TPS | ≥10000 | 内核态交易 |
| ML-DSA 验证 | <50ms | 预编译合约 |
| 锚定同步延迟 | <10分钟 | BHC 状态根 |

### Gas 成本对比

| 操作 | 标准 EVM | BHTE 内核态 | 折扣 |
|------|----------|-------------|------|
| 转账 | 21000 | 5250 | 75% |
| 多签 | 50000 | 12500 | 75% |
| ML-DSA 验证 | 100000 | 50000 | 50% |
| 状态通道开启 | 80000 | 20000 | 75% |
| 状态通道关闭 | 60000 | 15000 | 75% |

---

## 🚀 部署流程

### 1. 编译 BHTE

```bash
# 克隆修改后的 Geth
git clone https://github.com/bhc-core/bhte.git
cd bhte

# 编译
make geth

# 启动节点
./build/bin/geth --bhc \
    --datadir ~/.bhte \
    --rpc --rpcport 8545 \
    --ws --wsport 8546
```

### 2. 部署结算合约

```bash
# 使用 Hardhat 或 Foundry
cd contracts
npx hardhat deploy --network bhte
```

### 3. 配置 BHC 连接

```json
// config.json
{
    "bhc": {
        "rpc_url": "http://127.0.0.1:18332",
        "rpc_user": "bhc_boss",
        "rpc_password": "your_secure_password"
    },
    "anchor": {
        "check_interval": "10m",
        "min_confirmations": 3
    }
}
```

---

*最后更新: 2026-03-21*
