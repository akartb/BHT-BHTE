# BHT Wallet Pro

基于 Bitcoin Core 架构的专业加密货币钱包应用，支持 BHT 和 BHTE。

## 项目概述

BHT Wallet Pro 是一款基于 Bitcoin Core 架构开发的去中心化钱包应用，集成了最新的后量子密码学技术和 Layer 2 解决方案。

## 当前可用进度

- 已修复 Windows Qt 运行时部署，`build\bin\bht-wallet.exe` 可直接启动。
- 已提供 Bitcoin Core 风格的钱包工作台：概览、交易、地址、BHTE 跨链、BHT/BHTE 节点、安全、硬件钱包、设置、开发者控制台。
- 已支持英文 / 简体中文运行时切换和开发者模式入口。
- 已接入真实节点 RPC 配置：
  - BHT 使用 Bitcoin-style JSON-RPC：`getblockchaininfo`、`getnetworkinfo`、`getbalance`、`sendtoaddress`、`sendrawtransaction`、`gettxoutproof`。
  - BHTE 使用 Ethereum-style JSON-RPC：`eth_blockNumber`、`net_peerCount`、`eth_accounts`、`eth_getBalance`、`eth_sendTransaction`、`eth_sendRawTransaction`、`eth_getProof`。
- 概览页和节点页会同步 RPC 连接状态、高度、节点数和余额；设置页可保存 BHT RPC 用户名 / 密码、BHTE RPC endpoint、BHTE 默认账户。
- 已加入生产化接口层：
  - BHT 存入证明校验：`verifytxoutproof` + `getrawtransaction` 确认数检查。
  - BHTE 账户证明校验：解析 `eth_getProof` 结构并校验账户、balance、storage hash。
  - 托管节点进程：可配置 BHT/BHTE 可执行文件、数据目录、启动参数，并从节点页启动/停止。
  - 交易历史索引：BHT 使用 `listtransactions`；BHTE 默认调用扩展索引 RPC `bhte_getTransactionsByAddress`，可在设置中调整 method。
  - 构建/签名/广播：BHT 使用 `createrawtransaction`、`fundrawtransaction`、`signrawtransactionwithwallet`、`sendrawtransaction`；BHTE 使用 `eth_estimateGas`、`eth_getTransactionCount`、`eth_chainId`、`eth_signTransaction`、`eth_sendRawTransaction`。

后续仍需继续完善链端本身：BHTE 地址历史索引器 RPC、桥接合约 ABI/地址配置、排序器提现请求、合约事件扫描、轻客户端状态根验证、BHT/BHTE 节点安装打包和生产级错误恢复。

## 核心功能

### 1. ML-DSA 后量子签名
- 基于 liboqs 库的 Dilithium 签名算法
- 支持 Level 2/3/5 三个安全级别
- 混合签名模式（ECDSA + ML-DSA）

### 2. Layer 2 BHTE 集成
- 完整的 Layer 2 BHTE 节点连接
- 跨链存款/取款功能
- 实时交易状态监控

### 3. Send 转账功能
- 支持 BHC 和 BHTE 双币种
- 智能费率估算
- Coin Control 高级功能
- RBF (Replace-By-Fee) 支持

### 4. 硬件钱包支持
- Ledger 设备集成
- Trezor 设备集成
- 安全的离线签名

## 技术架构

```
BHTWalletPro/
├── src/
│   ├── wallet/          # 钱包核心
│   │   ├── wallet.cpp   # 钱包实现
│   │   ├── walletdb.cpp # 数据库
│   │   └── crypter.cpp  # 加密模块
│   ├── crypto/          # 加密算法
│   │   ├── mldsa_signer.cpp    # ML-DSA 签名
│   │   ├── sha256.cpp   # SHA-256
│   │   ├── ripemd160.cpp # RIPEMD-160
│   │   └── secp256k1.cpp # 椭圆曲线
│   ├── layer2/          # Layer 2 集成
│   │   └── layer2_client.cpp
│   ├── hardware/        # 硬件钱包
│   │   └── hardware_wallet.cpp
│   ├── config/          # 网络配置
│   │   └── networkconfig.cpp
│   └── qt/              # Qt GUI
│       ├── main.cpp              # 入口
│       ├── bitcoingui.cpp        # 主界面
│       ├── clientmodel.cpp       # 客户端模型
│       ├── optionsmodel.cpp      # 选项模型
│       ├── walletmodel.cpp       # 钱包模型
│       ├── transactionview.cpp   # 交易历史
│       ├── addressbookpage.cpp   # 地址簿
│       ├── overviewpage.cpp      # 概览页面
│       ├── sendcoinsdialog.cpp   # 发送对话框
│       └── receivecoinsdialog.cpp # 接收对话框
└── CMakeLists.txt       # 构建配置
```

## 构建要求

- CMake 3.22+
- Qt 6.0+
- OpenSSL 3.0+
- C++20 编译器
- Visual Studio 2022 / GCC 11+

## 构建步骤

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake .. -DBUILD_GUI=ON -DUSE_MLDSA=ON -DUSE_NATIVE_HID=ON

# 编译
cmake --build . --config Release

# 安装
cmake --install .
```

## 开发计划

- [x] 核心钱包架构
- [x] ML-DSA 后量子签名
- [x] Layer 2 BHTE 集成
- [x] 硬件钱包 HID 支持
- [x] Qt GUI 完善
- [x] 交易历史模块
- [x] 地址簿管理
- [x] 测试网络支持
- [x] 多语言支持
- [x] BHT / BHTE RPC 状态与广播入口
- [x] 生产级跨链桥证明验证接口
- [x] 节点进程生命周期管理接口
- [x] 交易历史 RPC 索引接口
- [x] 原始交易构建 / 签名 / 广播接口
- [ ] BHTE 索引器服务实现
- [ ] 桥接合约事件扫描与提现执行
- [ ] 移动端适配

## 安全特性

1. **后量子签名**: 抗量子计算攻击
2. **BIP-39 助记词**: 安全的密钥备份
3. **BIP-32/44 HD**: 分层确定性钱包
4. **硬件签名**: 私钥永不触网
5. **多重签名**: 支持 2-of-3 等多重签名方案

## 许可证

MIT License

## 联系方式

- Website: https://bht.org
- Email: dev@bht.org

## 致谢

本项目基于以下开源项目：

- [Bitcoin Core](https://github.com/bitcoin/bitcoin)
- [liboqs](https://github.com/open-quantum-safe/liboqs)
- [secp256k1](https://github.com/bitcoin-core/secp256k1)
- [Qt Framework](https://www.qt.io/)
