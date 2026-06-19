# BHT/BHTE Production Readiness Tasks

Last updated: 2026-06-19

This repository is currently a compileable, partially runnable dual-chain
prototype being actively hardened into a verifiable node system. It is not yet a
BTC/ETH-grade public mainnet.

For the current working narrative, read [CURRENT_WORK.md](CURRENT_WORK.md).
That file explains what we are doing now: moving BHT/BHTE from demonstration
paths toward locally replayable, proof-bearing, reorg-aware infrastructure. The
checklist below is the engineering backlog needed before the project can be
treated as globally operable infrastructure.

## Current Honest Status

- BHTE has a persistent JSON-RPC development node with txpool, block production,
  receipts/logs, state-root anchoring, bridge events, withdrawal records, simple
  reorg handling, and peer sync endpoints.
- BHTE `eth_getProof` now returns account/storage trie proofs derived from the
  current state trie instead of all-zero placeholders, and mined blocks persist
  state, receipt, and storage trie commits at the mined block height.
- BHTE can now produce and locally verify transaction receipt trie proofs through
  `bhte_getReceiptProof` and `bhte_verifyReceiptProof`; receipt and block
  `logsBloom` values are populated from actual logs instead of zero placeholders.
- BHTE can now produce and locally verify single-log proofs through
  `bhte_getLogProof` and `bhte_verifyLogProof`, binding bridge event log content
  to a verified receipt proof.
- BHTE can now list persisted trie commits, read individual persisted trie nodes,
  and verify receipts directly against the local trie-node database through
  `bhte_getTrieCommits`, `bhte_getTrieNode`, and `bhte_verifyReceiptInTrie`.
- BHTE now persists per-block state snapshots for balances, nonces, code, and
  storage, so `eth_getProof`/`bhte_getProof` can answer historical account and
  storage proofs for retained snapshots instead of always using latest state.
- BHTE reorg handling now restores balances/nonces/code/storage from the
  pre-reorg snapshot, removes orphaned receipts/logs/snapshots/trie commits, and
  requeues transactions from removed blocks back into the pending pool.
- BHTE peer sync can now fetch a bounded range of remote canonical blocks,
  validate number/parent hash/state root/receipt root/transaction root, replay
  the block transactions through the local BHTE state machine, rebuild
  state/receipt/log bloom roots, and import only blocks whose commitments match.
- Peer-imported BHTE blocks that replay successfully now persist complete local
  state snapshots and can answer proof RPCs. Incomplete `peer-summary` snapshots
  are still recognized and proof-disabled for older or manually injected data.
- BHTE now exposes `bhte_replayChain`, an audit-style deterministic replay check
  for the retained canonical chain. It starts from the genesis snapshot, replays
  each block through the current state machine, compares state/transaction/
  receipt/log commitments, and restores the live node state afterward.
- `bhte_validateBlock` now performs execution-level candidate validation instead
  of header-only checks: it restores the parent snapshot, replays the candidate
  block, compares state/transaction/receipt/log commitments, and restores live
  node state without importing the block.
- BHTE peer records now include basic scoring, failure counts, and temporary
  bans. Repeated invalid sync attempts reduce score to zero and short-circuit
  further sync attempts until the ban expires.
- BHTE now maintains a block index with height, parent hash, cumulative weight,
  validation status, source, and canonical flags. `bhte_forkChoiceStatus`
  exposes canonical head, best known head, known block count, and current tips.
- BHTE now writes an independent `bhte_chain.json` chain database for blocks,
  canonical mappings, and block index data. This is the first split away from
  the monolithic state JSON; account state and snapshots still need a production
  database/trie backend.
- BHTE does not yet implement a full Ethereum execution layer: opcode execution,
  gas/state transition rules, contract storage semantics, MPT proof generation,
  and consensus/P2P are still incomplete.
- BHC is not yet a Bitcoin Core-equivalent L1 node. It has chain parameters,
  ProgPoW/ML-DSA components, scripts, and standalone smoke tests, but still
  needs production P2P, mempool, UTXO/index databases, mining, RPC coverage,
  reorg handling, and long-running validation.
- The bridge has stronger SPV/Merkle/nBits validation and a compileable
  settlement contract, but it is not yet a trust-minimized production bridge.
- BHTWalletPro builds and starts with Qt runtime deployment, but the wallet
  database, HD wallet, transaction construction/signing, history sync, backup,
  and hardware-wallet workflows are not production-complete.
- BHTE Go ML-DSA and the zkEVM precompile are real implementations with local
  tests, but still need NIST ACVP/KAT validation and external review.
- BHC C++ has an unintegrated pure C++ FIPS 204 implementation candidate in
  `BHC/src/crypto/mldsa_fips204.h`. It must be rerun through the standalone
  self-test on a host that allows new crypto executables before replacing the
  current non-liboqs fallback.

## P0: Consensus and Execution

- Implement BHC production L1 node components: peer discovery, version/verack,
  inventory relay, block/tx relay, ban scoring, headers-first sync, UTXO set,
  block index, mempool policy, mining template, difficulty adjustment, and RPC.
- Implement BHTE production consensus: block proposal, validation, fork choice,
  persistent canonical chain database, peer discovery, peer scoring, snap/header
  sync, and deterministic validator/miner configuration. Current status:
  bounded peer block sync with local transaction replay and root verification
  exists for the current simplified state machine; candidate block replay
  validation exists through `bhte_validateBlock`; basic peer scoring/temporary
  banning exists; a block-index/fork-choice status skeleton exists; chain data
  now persists separately to `bhte_chain.json`. Automatic non-canonical branch
  adoption, durable peer reputation, validator/miner networking, and
  Ethereum-compatible execution are still pending.
- Replace BHTE selector-level contract simulation with full EVM state
  transitions: opcode execution, gas accounting, refunds, CALL/CREATE/SELFDESTRUCT
  semantics, storage/account trie updates, bloom filters, and receipt roots.
- Persist BHTE account/storage trie nodes in a real database and expose standard
  MPT proof generation/verification for accounts, storage slots, receipts, and
  logs. Current status: current-state account/storage proof generation exists;
  transaction receipt proof generation and local verification exists; historical
  trie commit lookup, node retrieval, and receipt verification from persisted
  nodes exists. Single-log proof helpers exist as local receipt-proof plus
  log-content verification. Historical account/storage proof snapshots exist for
  retained locally executed and successfully replayed peer blocks. Blocks,
  canonical mappings, and block index now also persist to `bhte_chain.json`.
  Incomplete peer summaries remain proof-disabled. Remaining gaps: real database
  storage beyond JSON files, pruning-safe node retention, state-sync for nodes
  joining far behind, and validation of replacement branch state during deep
  reorgs.
- Add deterministic replay tests that rebuild chain state from genesis and
  compare state roots across nodes. Current status: single-node canonical replay
  verification exists through `bhte_replayChain`; cross-node replay and full EVM
  compatibility vectors are still pending.

## P0: Bridge and Cross-Chain Safety

- Run a real BHC L1 watcher that scans canonical BHC blocks, tracks reorgs, and
  produces confirmed deposit/anchor proofs.
- Run a real BHTE watcher that scans bridge contract logs and drives withdrawal
  finalization after the challenge period.
- Implement on-chain/light-client verification for BHC headers or a clearly
  defined validator set with slashing/challenge rules.
- Implement BHT L1 withdrawal broadcast with fee estimation, nonce/UTXO
  management, idempotent retry, conflict detection, and reorg rollback.
- Add bridge accounting invariants: total locked BHT, total minted BHTE, pending
  withdrawals, challenged withdrawals, and completed withdrawals must reconcile.
- Add end-to-end bridge tests for deposit, mint, withdraw, challenge, finalize,
  L1 reorg, L2 reorg, duplicate proof, and invalid proof flows.

## P0: Cryptography and Keys

- Validate Go ML-DSA and C++ ML-DSA against official NIST ACVP/KAT vectors.
- Integrate the verified C++ FIPS 204 ML-DSA fallback into `BHC/src/crypto` and
  `BHTWalletPro`, or require liboqs at build/runtime with matching FIPS 204
  parameter sizes.
- Align all Level3/Level5 sizes to FIPS 204 across BHC, BHTE, wallet, tests, and
  documentation: ML-DSA-65 signature size is 3309 bytes, ML-DSA-87 is 4627 bytes.
- Add constant-time checks, memory-zeroing audits, fuzz tests, and negative test
  vectors for malformed keys/signatures.

## P1: Wallet Production Features

- Replace placeholder wallet DB methods with encrypted persistent storage,
  schema migrations, backup/restore, wallet locking, and corruption recovery.
- Implement HD seed generation/import/export, address derivation, key rotation,
  watch-only accounts, multisig, and descriptor-style metadata.
- Implement BHT UTXO transaction construction: coin selection, change output,
  fee estimation, signing, PSBT-like export/import, and broadcast.
- Implement BHTE transaction construction: nonce tracking, gas estimation, chain
  ID enforcement, contract calls, receipt polling, and history sync.
- Add transaction history indexing, confirmations, replaced/dropped transaction
  detection, bridge status tracking, and rescan support.
- Add developer mode diagnostics without exposing unsafe private-key or signing
  controls by default.

## P1: Operations, Security, and CI

- Add RPC authentication, TLS guidance, CORS restrictions, rate limits, request
  size limits, and safe defaults for public/private interfaces.
- Add structured logs, metrics, health checks, pprof/debug gating, and alerting
  hooks for nodes, bridge services, and wallet backend services.
- Add CI for Windows/Linux builds, Go tests, C++ tests, Solidity compile/tests,
  static analysis, formatting, dependency vulnerability checks, and artifact
  signing.
- Add fuzz/property tests for blocks, transactions, scripts, RPC parsers, bridge
  proofs, trie proofs, and wallet transaction builders.
- Run long-lived multi-node testnets with network partitions, restarts, reorgs,
  corrupted DBs, and bridge stress tests before any public launch.
- Commission external security audits for consensus, bridge, cryptography, and
  wallet key management.

## P2: Compatibility and Ecosystem

- Define stable BHT and BHTE RPC specifications and compatibility tests.
- Provide block explorer/indexer APIs for balances, transactions, logs, receipts,
  bridge events, and finality status.
- Provide reproducible builds, signed releases, chain configuration files,
  genesis generation, seed nodes, faucets/testnet tools, and operator docs.
- Add SDK examples for deposits, withdrawals, BHTE contract deployment, wallet
  integration, and ML-DSA signature verification.
