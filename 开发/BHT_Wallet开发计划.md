# BHT Wallet 开发计划

## 项目概述

**项目名称**: BHT Wallet  
**定位**: 类似 Bitcoin Core 的全功能加密货币钱包应用程序  
**技术栈**: C++20 + Qt6 + CMake  
**目标平台**: Windows (优先), Linux, macOS

---

## 核心需求

### 1. 用户界面
- ✅ GUI 图形界面 (Qt6)
- ✅ CLI 命令行界面
- ✅ 双模式支持

### 2. 节点模式
- ✅ 完整节点 (Full Node) - 下载完整区块链
- ✅ SPV 轻钱包 - 不存储区块链，依赖第三方验证
- ✅ 用户可切换模式

### 3. 核心功能
- ✅ ML-DSA 抗量子签名 (ECDSA + ML-DSA 双重签名)
- ✅ Layer 2 (BHTE) 集成
- ✅ 多币种支持 (BHC, BHTE, BTC 等)
- ✅ 硬件钱包集成 (Ledger, Trezor)

---

## 项目结构

```
BHTWallet/
├── src/
│   ├── core/                    # 核心模块
│   │   ├── wallet.cpp/h         # 钱包核心逻辑
│   │   ├── keystore.cpp/h       # 密钥存储
│   │   ├── transaction.cpp/h    # 交易处理
│   │   ├── mldsa_signer.cpp/h   # ML-DSA 签名器
│   │   └── hd_wallet.cpp/h      # HD 分层确定性钱包
│   │
│   ├── node/                    # 节点模块
│   │   ├── full_node.cpp/h      # 完整节点
│   │   ├── spv_node.cpp/h       # SPV 轻节点
│   │   ├── sync.cpp/h           # 区块同步
│   │   └── mempool.cpp/h        # 内存池
│   │
│   ├── layer2/                  # Layer 2 模块
│   │   ├── bridge.cpp/h         # L1-L2 桥接
│   │   ├── channel.cpp/h        # 支付通道
│   │   └── settlement.cpp/h     # 结算逻辑
│   │
│   ├── hardware/                # 硬件钱包
│   │   ├── device.cpp/h         # 设备抽象层
│   │   ├── ledger.cpp/h         # Ledger 支持
│   │   └── trezor.cpp/h         # Trezor 支持
│   │
│   ├── gui/                     # Qt GUI
│   │   ├── mainwindow.cpp/h     # 主窗口
│   │   ├── overview.cpp/h       # 概览页
│   │   ├── send.cpp/h           # 发送页
│   │   ├── receive.cpp/h        # 接收页
│   │   ├── transactions.cpp/h   # 交易历史
│   │   ├── settings.cpp/h       # 设置页
│   │   └── console.cpp/h        # 控制台
│   │
│   ├── cli/                     # CLI 接口
│   │   ├── bht-wallet.cpp       # CLI 主程序
│   │   └── commands.cpp/h       # 命令处理
│   │
│   ├── rpc/                     # RPC 服务
│   │   ├── server.cpp/h         # RPC 服务器
│   │   └── client.cpp/h         # RPC 客户端
│   │
│   └── util/                    # 工具模块
│       ├── crypto.cpp/h         # 加密工具
│       ├── logging.cpp/h        # 日志系统
│       └── config.cpp/h         # 配置管理
│
├── resources/                   # 资源文件
│   ├── icons/                   # 图标
│   ├── translations/            # 多语言翻译
│   └── styles/                  # 样式表
│
├── tests/                       # 测试
│   ├── test_wallet.cpp
│   ├── test_mldsa.cpp
│   └── test_integration.cpp
│
├── CMakeLists.txt               # CMake 配置
├── README.md                    # 项目说明
└── LICENSE                      # 许可证
```

---

## 开发阶段

### Phase 1: 核心基础 (第 1-4 周)

| 任务 | 说明 | 状态 |
|------|------|------|
| 项目结构创建 | 创建目录和 CMake 配置 | ⏳ 待开始 |
| 密钥管理模块 | 密钥生成、存储、导入导出 | ⏳ 待开始 |
| ML-DSA 签名器 | 集成 liboqs 实现 ML-DSA | ⏳ 待开始 |
| HD 钱包 | BIP-39/BIP-32/BIP-44 支持 | ⏳ 待开始 |
| 交易构建 | 交易创建、签名、序列化 | ⏳ 待开始 |

### Phase 2: 节点功能 (第 5-8 周)

| 任务 | 说明 | 状态 |
|------|------|------|
| 完整节点 | 区块下载、验证、存储 | ⏳ 待开始 |
| SPV 轻节点 | 区块头同步、Merkle 证明 | ⏳ 待开始 |
| P2P 网络 | 节点发现、连接、消息传递 | ⏳ 待开始 |
| 内存池 | 未确认交易管理 | ⏳ 待开始 |

### Phase 3: GUI 界面 (第 9-12 周)

| 任务 | 说明 | 状态 |
|------|------|------|
| 主窗口框架 | Qt 主窗口、菜单、工具栏 | ⏳ 待开始 |
| 概览页 | 余额、最近交易、网络状态 | ⏳ 待开始 |
| 发送/接收 | 转账界面、地址生成 | ⏳ 待开始 |
| 交易历史 | 交易列表、详情、搜索 | ⏳ 待开始 |
| 设置页 | 网络、安全、显示设置 | ⏳ 待开始 |

### Phase 4: 高级功能 (第 13-16 周)

| 任务 | 说明 | 状态 |
|------|------|------|
| Layer 2 集成 | BHTE 资产管理、跨层转账 | ⏳ 待开始 |
| 硬件钱包 | Ledger/Trezor 支持 | ⏳ 待开始 |
| 多币种 | BHC/BHTE/BTC 多资产 | ⏳ 待开始 |
| CLI 接口 | 命令行钱包操作 | ⏳ 待开始 |

### Phase 5: 测试与发布 (第 17-20 周)

| 任务 | 说明 | 状态 |
|------|------|------|
| 单元测试 | 核心模块测试覆盖 | ⏳ 待开始 |
| 集成测试 | 端到端功能测试 | ⏳ 待开始 |
| 安全审计 | 代码安全审查 | ⏳ 待开始 |
| 打包发布 | Windows 安装程序 | ⏳ 待开始 |

---

## 技术规格

### 依赖库

| 库 | 版本 | 用途 |
|---|------|------|
| Qt | 6.5+ | GUI 框架 |
| liboqs | 0.8+ | ML-DSA 抗量子签名 |
| OpenSSL | 3.0+ | 加密原语 |
| Boost | 1.80+ | 工具库 |
| libevent | 2.1+ | 事件驱动 |
| SQLite | 3.40+ | 数据库存储 |
| ZeroMQ | 4.3+ | 消息队列 |

### 编译环境

| 平台 | 编译器 | 构建工具 |
|------|--------|----------|
| Windows | MSVC 2022 | CMake 4.2+ |
| Linux | GCC 11+ / Clang 14+ | CMake 3.22+ |
| macOS | Xcode 14+ | CMake 3.22+ |

---

## 安全设计

### 密钥安全
- 私钥加密存储 (AES-256-GCM)
- 内存中密钥保护 (mlock)
- 钱包密码 + 可选硬件密钥

### 签名安全
- ECDSA (secp256k1) 传统签名
- ML-DSA (Dilithium3) 抗量子签名
- Hybrid 双重签名模式

### 网络安全
- TLS 加密通信
- 证书固定 (Certificate Pinning)
- RPC 认证

---

## 文件清单

### 核心文件 (优先开发)

1. `src/core/wallet.h/cpp` - 钱包核心
2. `src/core/keystore.h/cpp` - 密钥存储
3. `src/core/mldsa_signer.h/cpp` - ML-DSA 签名
4. `src/core/hd_wallet.h/cpp` - HD 钱包
5. `src/core/transaction.h/cpp` - 交易处理

### GUI 文件

1. `src/gui/mainwindow.h/cpp` - 主窗口
2. `src/gui/overview.h/cpp` - 概览页
3. `src/gui/send.h/cpp` - 发送页
4. `src/gui/receive.h/cpp` - 接收页
5. `src/gui/transactions.h/cpp` - 交易历史

### 节点文件

1. `src/node/full_node.h/cpp` - 完整节点
2. `src/node/spv_node.h/cpp` - SPV 节点
3. `src/node/sync.h/cpp` - 同步逻辑

---

## 下一步行动

1. ✅ 创建项目目录结构
2. ⏳ 创建 CMakeLists.txt 构建配置
3. ⏳ 实现钱包核心模块
4. ⏳ 实现 ML-DSA 签名器
5. ⏳ 实现 Qt GUI 框架

---

*创建时间: 2026-03-27*
*预计完成: 2026-08-15*
