# BHT/BHTE 当前工作说明

Last updated: 2026-06-19

## 当前属于什么状态

这个仓库现在处于“双链原型向可验证节点系统过渡”的工程阶段。

更直白地说：它已经不是只有界面和概念代码的演示项目，BHTE 这边已经开始具备真实节点应有的一些关键性质，例如交易池、出块、receipt/log、trie proof、历史状态快照、reorg 回滚，以及 peer 区块执行重放校验。但它也还不是 BTC/ETH 那种可以公开运行、承载真实资产、允许全球节点长期参与的生产级主网。

当前项目最准确的定位是：

- BHTE 是一个可持久化、可测试、正在被逐步硬化的 L2 开发节点。
- BHC 是一个具备链参数、PoW/ML-DSA/脚本雏形和测试工具的 L1 原型，还不是 Bitcoin Core 等价节点。
- 双链桥已经从纯占位逻辑推进到 SPV/Merkle/nBits 校验、settlement 合约、receipt/log proof 等方向，但还不是完整去信任桥。
- 钱包已经能构建和启动，界面和双链入口更完整，但钱包内核还需要 HD、加密数据库、真实交易构建签名、历史索引和恢复流程。
- ML-DSA 已经有 Go 侧真实实现和 zkEVM precompile 测试，但仍需要官方 KAT/ACVP、C++ fallback 端到端验证和外部审计。

## 我们正在做的事

我们当前的核心工作不是写宣传页，也不是简单补 UI，而是把 BHT/BHTE 从“可演示”推进到“可验证、可同步、可恢复、可审计”的节点系统。

近期重点集中在 BHTE，因为它是双链体系里目前最容易形成闭环验证的部分：

1. 把状态从 JSON 模拟推进到 trie/proof 可验证结构。
2. 把 receipt/log 从占位字段推进到可以生成、持久化、验证的索引。
3. 把 reorg 从简单改高度推进到能恢复余额、nonce、code、storage、receipt、log、trie commit 和 pending tx。
4. 把 peer sync 从“相信远端区块摘要”推进到“本地重放远端交易，重建 stateRoot/receiptRoot/logsBloom，匹配后才导入”。
5. 把本地 canonical chain 从“存着一串区块”推进到“可以从 genesis 审计式重放，确认每个区块承诺能复现”。
6. 把候选区块验证从 header-only 字段检查推进到临时重放父状态、校验 stateRoot/receiptRoot/logsBloom，并且不污染节点状态。
7. 给 peer sync 加入基础评分和临时封禁，避免坏 peer 反复提供无效区块时被无限重试。
8. 引入 block index/fork-choice status 骨架，从“只看本地高度”推进到“跟踪已知块、累计权重、canonical head 和候选 tips”。
9. 把 blocks/canonical/block index 从单体状态文件里拆出独立 `bhte_chain.json`，开始向正式 chain database 布局迁移。
10. 把文档从乐观项目介绍改成诚实工程说明，明确哪些链路已经能测，哪些仍是生产级缺口。

这意味着我们现在做的是节点可信度建设：每增加一个功能，都尽量配套测试、失败回滚、root 校验和文档说明。

## 最近已经推进到哪里

BHTE 当前已经具备以下实质进展：

- 本地区块执行会生成 state trie root、receipt trie root 和 logs bloom。
- `eth_getProof`/`bhte_getProof` 可以基于当前或历史 snapshot 返回 account/storage proof。
- `bhte_getReceiptProof`、`bhte_verifyReceiptProof`、`bhte_getLogProof`、`bhte_verifyLogProof` 已经形成 receipt/log 本地证明链。
- trie commit 和 trie node 可以持久化并通过 RPC 查询。
- reorg 会回滚状态、索引和交易池，而不是只改 canonical hash。
- peer 同步区块时会验证 number、parent、tx root、receipt root、state root，并执行交易重放；root 不匹配会拒绝导入并撤销副作用。
- `bhte_replayChain` 可以对本地 canonical blocks 做审计式重放，从 genesis snapshot 开始复现 stateRoot、transactionsRoot、receiptsRoot 和 logsBloom，并且不会改变节点当前状态。
- `bhte_validateBlock` 现在会对候选区块做执行级验证：从父块完整 snapshot 临时重放交易，比较 stateRoot、transactionsRoot、receiptsRoot、logsBloom，然后恢复原节点状态。
- peer 记录现在包含 score/failures/bannedUntil；同步失败会扣分，重复提供无效区块会被临时封禁，成功同步会恢复健康分数。
- `bhte_forkChoiceStatus` 现在会返回 canonical head、best known head、known block 数、累计权重和 tips；目前仍是 dev fork-choice 骨架，还没有自动采用非 canonical 分支。
- 节点现在会写入独立 `bhte_chain.json`，用于持久化 blocks、canonical map 和 block index；加载时会优先恢复这部分链数据库，同时保留旧状态文件兼容。

这些工作让 BHTE 从“接口看起来像链”向“节点能自己验证链数据”靠近了一步。

## 还不是生产级的原因

离 BTC/ETH 级可运营主网仍差几个硬条件：

- BHTE 还没有完整 Ethereum EVM opcode/state transition/gas/storage 语义。
- BHTE 还没有真实生产 P2P、fork choice、peer scoring、长期 chain database 和 snap/state sync。
- BHC 还缺完整 L1 节点框架：P2P、mempool、UTXO DB、block index、mining、RPC、reorg、长期运行验证。
- 桥还缺 BHC L1 watcher、BHTE 合约事件 watcher、提现广播、挑战自动化、双向 reorg 处理和桥资产守恒审计。
- 钱包还缺生产钱包内核：HD seed、加密 DB、交易构建签名、找零、手续费、恢复、历史索引。
- ML-DSA 还缺官方向量验证、C++ fallback 落地、常数时间审计和第三方安全审计。

## 当前下一步

下一阶段应继续沿着“能独立验证，不盲信远端”的路线推进：

1. BHTE：把当前简化执行模型替换为更完整的 EVM/state transition。
2. BHTE：把 JSON 文件状态迁移到更正式的链数据库和 trie 节点数据库。
3. BHTE：实现真正 P2P/fork choice/peer scoring，而不是 RPC 拉块式同步。
4. Bridge：实现真实 BHC watcher、BHTE event watcher、提现执行和 reorg 回滚闭环。
5. BHC：补完整 L1 节点骨架，尤其 UTXO、mempool、block index、P2P 和 RPC。
6. Wallet：补 HD、加密 DB、真实 BHT/BHTE 交易构建签名和历史同步。

一句话总结：我们现在正在把 BHT/BHTE 的关键链路从“能展示”改造成“能被节点自己验证”，这仍是生产级主网之前的中间阶段，但方向已经从原型展示转向真实节点工程。
