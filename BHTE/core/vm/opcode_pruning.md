# BHTE zkEVM 操作码裁剪规范 v1.0

## 1. 概述

本规范定义了 BHTE (Layer 2) 中 zkEVM 操作码的裁剪策略，确保支付交易永不拥堵，同时保持与以太坊 EVM 的最大兼容性。

### 1.1 设计原则

| 原则 | 说明 |
|------|------|
| **支付优先** | 内核态操作码永远优先执行，不受 Gas 价格影响 |
| **防拥堵** | 高复杂度操作码被禁用或严格限流 |
| **抗量子** | ML-DSA 签名验证必须高效集成 |
| **可扩展** | 用户态合约可以部署但受 Gas 调节器控制 |

---

## 2. 操作码分类

### 2.1 永久允许列表（内核态）

这些操作码构成支付核心，永不拥堵：

| 操作码 | Opcode | Gas 成本 | 说明 |
|--------|--------|----------|------|
| STOP | 0x00 | 0 | 终止执行 |
| ADD | 0x01 | 3 | 整数加法 |
| MUL | 0x02 | 5 | 整数乘法 |
| SUB | 0x03 | 3 | 整数减法 |
| DIV | 0x05 | 5 | 整数除法 |
| SDIV | 0x06 | 5 | 有符号整数除法 |
| MOD | 0x07 | 5 | 模运算 |
| SMOD | 0x08 | 5 | 有符号模运算 |
| ADDMOD | 0x09 | 8 | 加法后模运算 |
| MULMOD | 0x0A | 8 | 乘法后模运算 |
| EXP | 0x0B | 10 | 指数运算（base^exp） |
| SIGNEXTEND | 0x0C | 5 | 符号扩展 |
| LT | 0x10 | 3 | 小于比较 |
| GT | 0x11 | 3 | 大于比较 |
| SLT | 0x12 | 3 | 有符号小于比较 |
| SGT | 0x13 | 3 | 有符号大于比较 |
| EQ | 0x14 | 3 | 相等比较 |
| ISZERO | 0x15 | 3 | 零值检测 |
| AND | 0x16 | 3 | 按位与 |
| OR | 0x17 | 3 | 按位或 |
| XOR | 0x18 | 3 | 按位异或 |
| NOT | 0x19 | 3 | 按位非 |
| BYTE | 0x1A | 3 | 字节提取 |
| SHL | 0x1B | 3 | 左移 |
| SHR | 0x1C | 3 | 右移 |
| SAR | 0x1D | 3 | 算术右移 |
| SHA3 | 0x20 | 30 | Keccak-256 哈希 |
| KECCAK256 | 0x20 | 30 | Keccak-256（别名） |
| ADDRESS | 0x30 | 2 | 获取当前合约地址 |
| ORIGIN | 0x32 | 2 | 获取交易 origin |
| CALLER | 0x33 | 2 | 获取调用者地址 |
| CALLVALUE | 0x34 | 2 | 获取调用值 |
| CALLDATALOAD | 0x35 | 3 | 加载调用数据 |
| CALLDATASIZE | 0x36 | 2 | 获取调用数据大小 |
| CALLDATACOPY | 0x37 | 3 | 复制调用数据 |
| CODESIZE | 0x38 | 2 | 获取代码大小 |
| CODECOPY | 0x39 | 3 | 复制代码 |
| GASPRICE | 0x3A | 2 | 获取 Gas 价格 |
| EXTCODESIZE | 0x3B | 2 | 获取外部代码大小 |
| EXTCODEHASH | 0x3F | 2 | 获取外部代码哈希 |
| BLOCKHASH | 0x40 | 20 | 获取区块哈希 |
| COINBASE | 0x41 | 2 | 获取区块奖励地址 |
| TIMESTAMP | 0x42 | 2 | 获取区块时间戳 |
| NUMBER | 0x43 | 2 | 获取区块编号 |
| DIFFICULTY | 0x44 | 2 | 获取区块难度 |
| GASLIMIT | 0x45 | 2 | 获取 Gas 上限 |
| CHAINID | 0x46 | 2 | 获取链 ID |
| SELFBALANCE | 0x47 | 2 | 获取自身余额 |
| BASEFEE | 0x48 | 2 | 获取基础费用 |
| POP | 0x50 | 2 | 弹出堆栈 |
| MLOAD | 0x51 | 3 | 内存加载 |
| MSTORE | 0x52 | 3 | 内存存储 |
| MSTORE8 | 0x53 | 3 | 内存字节存储 |
| SLOAD | 0x54 | 100 | 存储加载（优化） |
| SSTORE | 0x55 | 100 | 存储存储（优化） |
| JUMP | 0x56 | 8 | 无条件跳转 |
| JUMPI | 0x57 | 8 | 条件跳转 |
| PC | 0x58 | 2 | 程序计数器 |
| MSIZE | 0x59 | 2 | 内存大小 |
| GAS | 0x5A | 2 | 剩余 Gas |
| JUMPDEST | 0x5B | 1 | 跳转目标 |

### 2.2 BHTE 专用内核态操作码

这些是 BHC 特有的支付操作码：

| 操作码 | Opcode | Gas 成本 | 说明 |
|--------|--------|----------|------|
| KERNEL_TRANSFER | 0xB0 | 5000 | 原生资产转账 |
| KERNEL_MULTISIG | 0xB1 | 15000 | 多签验证 |
| KERNEL_CHANNEL_OPEN | 0xB2 | 20000 | 开启支付通道 |
| KERNEL_CHANNEL_CLOSE | 0xB3 | 15000 | 关闭支付通道 |
| KERNEL_ANCHOR | 0xB4 | 5000 | BHC 锚定验证 |
| KERNEL_MLDSA | 0xB5 | 50000 | ML-DSA 签名验证 |
| KERNEL_WITHDRAW | 0xB6 | 10000 | L1 提款 |
| KERNEL_BATCH_TRANSFER | 0xB7 | 8000 | 批量转账 |

### 2.3 禁用操作码列表

以下操作码在 BHTE 中**永久禁用**：

| 操作码 | Opcode | 禁用原因 |
|--------|--------|----------|
| CREATE | 0xF0 | 防止无限循环创建合约 |
| CREATE2 | 0xF5 | 防止确定性合约创建 |
| CALL | 0xF1 | 使用 CALLWITHVALUE 代替 |
| CALLCODE | 0xF4 | 安全隐患 |
| DELEGATECALL | 0xF4 | 安全隐患 |
| STATICCALL | 0xFA | 使用特许调用代替 |
| SELFDESTRUCT | 0xFF | 防止合约销毁攻击 |
| REVERT | 0xFD | 使用 INVALID 代替 |
| LOG0 | 0xA0 | 禁用事件日志 |
| LOG1 | 0xA1 | 禁用事件日志 |
| LOG2 | 0xA2 | 禁用事件日志 |
| LOG3 | 0xA3 | 禁用事件日志 |
| LOG4 | 0xA4 | 禁用事件日志 |
| EXTCODECOPY | 0x3C | 禁用外部代码复制 |
| BLOCKBLOBBASEFEE | 0x48 | 特定于 EIP-4844，BHTE 不需要 |
| MCOPY | 0x5E | 禁用内存复制 |

### 2.4 限流操作码列表

以下操作码在用户态下受限：

| 操作码 | Opcode | 限流条件 | 最大 Gas |
|--------|--------|----------|----------|
| EXP | 0x0B | 仅限小额指数 | 100000 |
| KECCAK256 | 0x20 | 限速 10 TPS/合约 | 50000 |
| LOG* | 0xA0-0xA4 | 完全禁用 | N/A |
| CREATE* | 0xF0, 0xF5 | 禁用 | N/A |
| CALL | 0xF1 | 仅限内核态代理 | 2300 |

---

## 3. 双态执行环境

### 3.1 内核态 (Kernel Space)

**定义**: 高优先级支付操作执行环境

```
┌─────────────────────────────────────────────────────────────┐
│                      内核态执行环境                          │
├─────────────────────────────────────────────────────────────┤
│  ✓ 所有 KERNEL_* 操作码                                    │
│  ✓ 基本算术和逻辑操作 (0x01-0x1D)                          │
│  ✓ 存储读写 (SLOAD, SSTORE)                               │
│  ✓ 区块信息访问 (BLOCKHASH, TIMESTAMP, NUMBER)              │
│  ✓ ML-DSA 验证 (KERNEL_MLDSA)                             │
│  ✓ 原生转账 (KERNEL_TRANSFER)                             │
├─────────────────────────────────────────────────────────────┤
│  Gas 成本: 基准的 25%                                     │
│  优先级: 最高                                             │
│  拥堵保护: 无视用户态拥堵                                   │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 用户态 (User Space)

**定义**: 用户合约执行环境，受 Gas 调节器控制

```
┌─────────────────────────────────────────────────────────────┐
│                      用户态执行环境                          │
├─────────────────────────────────────────────────────────────┤
│  ✓ 允许的算术和逻辑操作                                    │
│  ✓ 基本的存储读写                                          │
│  ✓ 合法的外部调用                                          │
│  ✓ 有限的区块信息访问                                       │
├─────────────────────────────────────────────────────────────┤
│  Gas 成本: 动态调整（基准的 100%-1600%）                    │
│  优先级: 低                                               │
│  拥堵保护: 非内核态操作 > 30% 时自动抬升 Gas                │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. Gas 调节器规范

### 4.1 动态调节算法

```go
func (r *GasRegulator) UpdateCongestion() {
    total := r.totalOps.Load()
    if total == 0 {
        return
    }

    nonKernel := r.nonKernelOps.Load()
    nonKernelRatio := float64(nonKernel) / float64(total)

    // 触发条件：非内核态操作占比 > 30%
    if nonKernelRatio > 0.3 {
        // 计算新的 Gas 乘数
        // 最高可达基准的 16 倍
        newMultiplier := uint64(nonKernelRatio * 100 / 0.3)
        if newMultiplier > r.maxMultiplier*100 {
            newMultiplier = r.maxMultiplier * 100
        }
        r.userGasMultiplier.Store(newMultiplier)
        r.congestionLevel.Store(uint64(nonKernelRatio * 100))
    } else {
        // 恢复正常
        r.userGasMultiplier.Store(100)
        r.congestionLevel.Store(0)
    }

    // 重置计数器
    r.totalOps.Store(0)
    r.nonKernelOps.Store(0)
}
```

### 4.2 Gas 成本对照表

| 操作类型 | 基准 Gas | 内核态 Gas | 用户态 Gas（正常） | 用户态 Gas（拥堵） |
|----------|---------|------------|--------------------|--------------------|
| 转账 (Transfer) | 21000 | 5250 | 21000 | 33600 |
| 多签 (MultiSig) | 50000 | 12500 | 50000 | 80000 |
| ML-DSA 验证 | 50000 | 50000 | N/A | N/A |
| 存储写入 (SSTORE) | 100 | 100 | 100 | 160 |
| 区块哈希 (BLOCKHASH) | 20 | 20 | 20 | 32 |
| 合约调用 (CALL) | 2300 | 2300 | N/A | N/A |

---

## 5. ML-DSA 集成规范

### 5.1 预编译合约地址

| 地址 | 合约 | 说明 |
|------|------|------|
| 0x0000000000000000000000000000000000000100 | MLDSAPrecompile | ML-DSA 签名验证 |

### 5.2 输入格式

```
| 32 字节消息哈希 | 1952 字节公钥 | 3293 字节签名 |
|----------------|---------------|----------------|
| message (32B)  | publicKey (1952B) | signature (3293B) |
```

### 5.3 输出格式

```
| 32 字节返回值 (成功=1, 失败=0) |
|-------------------------------|
| result (32B)                  |
```

### 5.4 Gas 成本

| 操作 | Gas 成本 | 说明 |
|------|---------|------|
| ML-DSA 验证 | 50000 | Level 3 (192-bit 量子安全) |

---

## 6. 安全性规范

### 6.1 禁止的操作组合

| 组合 | 风险 | 缓解措施 |
|------|------|----------|
| CALL + 自毁 | 重入攻击 | 使用内核态代理 |
| CREATE + SSTORE | 无限存储膨胀 | 完全禁用 CREATE |
| LOG + CALL | 日志注入 | 完全禁用 LOG |
| EXTCODECOPY + CALL | 代码 injection | 使用只读视图 |

### 6.2 异常处理

当执行被禁止的操作码时：

```go
func (evm *EVM) ExecuteOp(op OpCode) error {
    switch op {
    case OP_CREATE, OP_CREATE2:
        return ErrOpCodeDisabled // "CREATE/CREATE2 is disabled in BHTE"
    case OP_SELFDESTRUCT:
        return ErrOpCodeDisabled // "SELFDESTRUCT is disabled in BHTE"
    case OP_LOG0, OP_LOG1, OP_LOG2, OP_LOG3, OP_LOG4:
        return ErrOpCodeDisabled // "LOG is disabled in BHTE"
    // ... 其他禁用操作码
    }
}
```

---

## 7. 性能目标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 基础交易确认 | 10-15 秒 | 链上交易 |
| 状态通道支付 | < 100ms | 链下即时支付 |
| 峰值 TPS | ≥ 10000 | 内核态交易 |
| ML-DSA 验证 | < 50ms | 预编译合约 |
| 锚定同步延迟 | < 10 分钟 | BHC 状态根 |

---

## 8. 实施检查清单

### 8.1 禁用操作码验证

- [ ] CREATE (0xF0) 返回 ErrOpCodeDisabled
- [ ] CREATE2 (0xF5) 返回 ErrOpCodeDisabled
- [ ] SELFDESTRUCT (0xFF) 返回 ErrOpCodeDisabled
- [ ] LOG0-4 (0xA0-0xA4) 返回 ErrOpCodeDisabled
- [ ] REVERT (0xFD) 被替换为 INVALID
- [ ] EXTCODECOPY (0x3C) 返回 ErrOpCodeDisabled

### 8.2 内核态验证

- [ ] KERNEL_TRANSFER (0xB0) 正确执行原生转账
- [ ] KERNEL_MLDSA (0xB5) 正确验证 ML-DSA 签名
- [ ] KERNEL_ANCHOR (0xB4) 正确验证 BHC 锚定
- [ ] Gas 消耗为基准的 25%

### 8.3 Gas 调节器验证

- [ ] 非内核态操作 > 30% 时自动抬升 Gas
- [ ] 拥堵解除后 Gas 恢复正常
- [ ] 最大乘数不超过 16x

---

*最后更新: 2026-03-22*
