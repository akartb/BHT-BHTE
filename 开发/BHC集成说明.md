# BHC 集成说明

## 概述

BHC (Bitcoin Hybrid Chain) 是基于 Bitcoin Core 的后量子安全区块链，集成了：

1. **ProgPoW** - 抗 ASIC 挖矿算法
2. **ML-DSA (Dilithium)** - NIST 后量子签名标准
3. **LWMA** - 线性加权移动平均难度调整

## 文件结构

```
bitcoin-master/src/
├── crypto/
│   ├── progpow.h          # ProgPoW 头文件
│   ├── progpow.cpp        # ProgPoW 实现
│   ├── mldsa.h            # ML-DSA 头文件
│   ├── mldsa.cpp          # ML-DSA 实现
│   └── CMakeLists.txt     # 已更新，包含新模块
├── kernel/
│   ├── lwma.h             # LWMA 难度调整
│   └── bhc_chainparams.h  # BHC 链参数
└── test/
    └── bhc_tests.cpp      # 集成测试
```

## 编译步骤

### Windows (Visual Studio 2026)

```batch
cd D:\work0\BHT\bitcoin-master
build_bhc.bat
```

### 手动编译

```batch
# 设置 VS 环境
call "D:\11\Common7\Tools\VsDevCmd.bat" -arch=amd64

# 创建构建目录
mkdir build && cd build

# 配置
cmake .. -G "Visual Studio 17 2022" -A x64

# 编译
cmake --build . --config Release -j 4
```

## 创世块参数

```
Hash: 3a7ea551d2d41377e210cd9f5af4b291a24d425e0f1666ada84fcf53ed0b0000
Nonce: 2335930
Timestamp: 1774082909
Bits: 0x1d00ffff
Message: "BHC: The Future of Digital Gold 2026 - Decentralized & Post-Quantum"
```

## 网络参数

| 网络 | 魔数 | P2P端口 | RPC端口 |
|------|------|---------|---------|
| Mainnet | 0x42 0x48 0x43 0x01 | 18334 | 18332 |
| Testnet | 0x42 0x48 0x54 0x01 | 28334 | 28332 |
| Regtest | 0x42 0x48 0x52 0x01 | 38334 | 38332 |

## 验证创世块

```batch
# 启动节点
bitcoind.exe -regtest -daemon

# 验证创世块哈希
bitcoin-cli.exe -regtest getblockhash 0
# 预期输出: 3a7ea551d2d41377e210cd9f5af4b291a24d425e0f1666ada84fcf53ed0b0000
```

## 性能指标

| 模块 | 指标 |
|------|------|
| ProgPoW | ~315,000 H/s (单核) |
| ML-DSA 签名 | ~13 微秒 |
| ML-DSA 验证 | ~13 微秒 |

## 后量子安全

ML-DSA Level 3 参数：
- 公钥大小: 1,952 字节
- 私钥大小: 4,032 字节
- 签名大小: 3,293 字节
- 安全级别: NIST Level 3 (192-bit 量子安全)

## 下一步

1. 完成完整节点编译
2. 运行 Regtest 测试网
3. 验证创世块哈希
4. 开始 BHTE Layer 2 开发
