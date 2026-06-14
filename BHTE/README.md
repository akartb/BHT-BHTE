# BHTE zkEVM Layered Architecture

## Overview

BHTE (Layer 2) zkEVM is a payment-optimized Layer 2 scaling solution built on modified Go-Ethereum with dual-state execution environment.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    BHTE zkEVM Architecture                   │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐   │
│  │              Kernel Space (Priority: Highest)         │   │
│  │  • Native Transfer (OP_KERNEL_TRANSFER)            │   │
│  │  • Multi-Signature (OP_KERNEL_MULTISIG)            │   │
│  │  • State Channel Open/Close                         │   │
│  │  • BHC Anchor Verification (OP_KERNEL_ANCHOR)       │   │
│  │  • ML-DSA Verify (OP_KERNEL_MLDSA)                  │   │
│  │  • Batch Transfer (OP_KERNEL_BATCH_TRANSFER)         │   │
│  │  Gas Cost: 75% discount | Never congested           │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              User Space (Priority: Normal)          │   │
│  │  • Custom Contracts                                 │   │
│  │  • DeFi Protocols                                   │   │
│  │  • NFT Marketplace                                  │   │
│  │  Gas: Dynamic pricing with 16x max multiplier        │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Module Structure

```
BHTE/
├── bhc_integration.go          # BHC L1 RPC integration
├── contracts/
│   └── BHTEsettlement.sol     # L2 settlement contract
├── core/
│   ├── config.go              # BHTE configuration
│   ├── bhte_evm.go            # Modified EVM with dual-state
│   └── vm/
│       ├── opcodes_kernel.go  # Kernel opcodes (0xB0-0xB7)
│       ├── gas_regulator.go   # Dynamic gas pricing
│       └── contracts_mldsa.go # ML-DSA precompiled contract
└── consensus/
    └── bhc/
        ├── anchor.go          # BHC state root verifier
        └── state_channel.go   # Payment channel manager
```

## Kernel Opcodes

| Opcode | Value | Description | Gas Discount |
|--------|-------|-------------|---------------|
| OP_KERNEL_TRANSFER | 0xB0 | Native transfer | 75% |
| OP_KERNEL_MULTISIG | 0xB1 | Multi-signature verify | 75% |
| OP_KERNEL_CHANNEL_OPEN | 0xB2 | Open state channel | 75% |
| OP_KERNEL_CHANNEL_CLOSE | 0xB3 | Close state channel | 75% |
| OP_KERNEL_ANCHOR | 0xB4 | Verify BHC anchor | 50% |
| OP_KERNEL_MLDSA | 0xB5 | ML-DSA signature verify | 50% |
| OP_KERNEL_WITHDRAW | 0xB6 | L1 withdrawal | 75% |
| OP_KERNEL_BATCH_TRANSFER | 0xB7 | Batch transfers | 80% |

## Gas Regulator

The gas regulator dynamically adjusts pricing based on network congestion:

- Tracks kernel vs user ops ratio
- When user ops > 30% of total: triggers congestion mode
- Max multiplier: 16x for non-kernel operations
- Kernel operations always at 25% base gas price

## BHC Integration

BHTE connects to BHC L1 via:

- JSON-RPC client (default: http://127.0.0.1:18332)
- Anchors L2 state roots every 10 minutes
- 3 confirmations required for anchor finality
- 7-day challenge period for withdrawals

## ML-DSA Precompile

Address: `0x0000000000000000000000000000000000000100`

Input format:
- 32 bytes: message hash
- 1952 bytes: public key
- 3293 bytes: signature

Gas cost: 50,000

## State Channels

Payment channels enable instant, off-chain transfers:

- 24-hour timeout
- 7-day challenge period
- Up to 1,000,000 BHC per channel
- Balance updates require sequence number increment
- Optional signature verification

## Configuration

```go
config := bhc.DefaultConfig()
// or
config := bhc.TestnetConfig()
// or
config := bhc.DevConfig()
```

## Build & Run

```bash
cd BHTE
go build -o bhte ./cmd/geth

./bhte --datadir ~/.bhte \
    --http.addr 127.0.0.1 \
    --http.port 8545 \
    --account 0x0000000000000000000000000000000000000100 \
    --bht.rpc http://127.0.0.1:18332
```

The development node now exposes a wallet-compatible JSON-RPC surface, persists
chain state in `bhte_state.json` under the selected data directory, and keeps
explicit txpool, block, receipt, log, bridge, withdrawal, and anchor indexes.
State roots are calculated with a go-ethereum trie over RLP-encoded account
state, and receipt roots are calculated with a trie over RLP-encoded receipts.
Trie node commits are persisted in `bhte_trie_nodes.json`, keyed by root and
height, so state and receipt roots can be audited after restart.

Supported RPC methods:

- `web3_clientVersion`
- `eth_chainId`
- `eth_blockNumber`
- `eth_getBlockByNumber`
- `eth_getBlockByHash`
- `net_peerCount`
- `admin_addPeer`
- `admin_peers`
- `bhte_syncPeer`
- `bhte_consensusStatus`
- `bhte_validateBlock`
- `eth_accounts`
- `eth_getBalance`
- `eth_getTransactionCount`
- `eth_getCode`
- `eth_getStorageAt`
- `eth_call`
- `eth_getTransactionByHash`
- `eth_getTransactionReceipt`
- `eth_getLogs`
- `eth_estimateGas`
- `eth_getProof`
- `eth_sendTransaction`
- `eth_signTransaction`
- `eth_sendRawTransaction`
- `txpool_status`
- `txpool_content`
- `bhte_mineBlock`
- `bhte_getTransactionsByAddress`
- `bhte_getBridgeEvents`
- `bhte_submitBridgeEvent`
- `bhte_initiateWithdrawal`
- `bhte_processWithdrawal`
- `bhte_getWithdrawal`
- `bhte_submitAnchor`
- `bhte_verifyStateRoot`
- `bhte_handleReorg`
- `bhte_securityStatus`
- `bhte_getTrieCommit`
- `bhte_getProof`

`bhte_getTransactionsByAddress` is the wallet history index endpoint used by
BHT Wallet Pro. Production deployments should replace the in-process state file
with the canonical chain database / event indexer while keeping this RPC
contract stable.

By default the node automines each submitted transaction into a block. Pass
`--no-automine` to keep transactions in `txpool_content` and produce blocks
manually with `bhte_mineBlock`.

Production safety defaults:

- Strict mode is enabled by default.
- Transactions are rejected on invalid sender, receiver, nonce, gas limit, or insufficient balance.
- Withdrawals cannot be processed until their challenge period has elapsed.
- BHT L1 withdrawal processing calls BHT `sendtoaddress` through `--bht.rpc`.
- `--dev-insecure` disables strict challenge / L1 failure behavior for local smoke tests only.
- `bhte_handleReorg` marks logs as removed and rolls back canonical block metadata from the supplied height.

P2P and consensus surface:

- `admin_addPeer` stores peer endpoints.
- `bhte_syncPeer` fetches peer height through JSON-RPC.
- `bhte_consensusStatus` reports engine, best hash, finalized height, peers and state root.
- `bhte_validateBlock` validates parent linkage for imported block metadata.

EVM execution surface:

- Native value transfer updates account balances and nonces.
- Contract creation stores deployed bytecode at the deterministic sender/nonce address.
- Bridge calls are decoded by ABI selector and emit canonical logs.
- `eth_call`, `eth_getCode`, and `eth_getStorageAt` expose contract state for wallet/indexer integration.
