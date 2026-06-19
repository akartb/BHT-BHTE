# BHC + BHTE Project Status

Last verified: 2026-06-19

## Current Work

This repository is currently in the engineering-hardening phase: the project is
being moved from a demonstrable BHC/BHTE dual-chain prototype toward a node
system that can verify its own state, replay synced blocks, recover from reorgs,
and expose auditable proofs.

It is not yet a BTC/ETH-grade public mainnet. The active work is focused on
turning placeholder or summary-only paths into locally verifiable paths,
especially inside BHTE.

Read the current Chinese working note first:

- [CURRENT_WORK.md](CURRENT_WORK.md)

That document explains what we are doing now, why the project should be treated
as a hardening prototype rather than a production network, and what must still
be built before global operation.

## Overview

This workspace contains a dual-layer blockchain/payment stack:

- `BHC`: Layer 1 C++ core prototype with ProgPoW, ML-DSA, LWMA difficulty adjustment, hybrid signatures, and L2 withdrawal/anchor support.
- `BHTE`: Go Layer 2 prototype with kernel opcodes, gas regulation, BHC observer/bridge pieces, state channels, merge-mining modules, SPV bridge, wallet integration, and settlement contracts.
- `BHTWalletPro`: Qt/C++ desktop wallet for BHC/BHTE with wallet core, ML-DSA signer, Layer 2 client, hardware wallet integration, and Bitcoin Core style GUI.
- `bitcoin-*`, `go-ethereum-master`, `vcpkg`: upstream source/dependency work areas and local build support.

## Verified Status

### BHC

`BHC/standalone_test.exe` runs successfully.

Verified command:

```powershell
cd D:\work0\BHT\BHC
.\standalone_test.exe
```

Observed result:

- ProgPoW hash calculation succeeds.
- The standalone ML-DSA smoke path signs and verifies, but it is still the simplified standalone fallback, not production FIPS 204 validation.
- A current run completed successfully in this environment.

### BHTE

BHTE is now the most actively hardened part of the workspace. It is a
persistent JSON-RPC development node with txpool, local block production,
receipt/log indexing, trie proof helpers, historical state snapshots, reorg
rollback, and peer block replay validation for the current simplified execution
model.

The Go package structure compiles, and tests pass using the Windows-safe script:

```powershell
cd D:\work0\BHT\BHTE
.\run_tests.ps1
```

Why the script exists:

- `go test ./...` currently passes most packages but can fail to execute generated test binaries from Go's deep temporary build path with `Access is denied` on this Windows machine.
- The script compiles each package that has tests with `go test -c -o .testbin/...` and then executes the stable test binary.

Key fixes made:

- Moved independent consensus modules into subpackages:
  - `consensus/bhc/proxy`
  - `consensus/bhc/coordinator`
  - `consensus/bhc/channelnetwork`
  - `consensus/bhc/anchor`
- Corrected local VM imports to use `bhte/core/vm` instead of geth's upstream `core/vm`.
- Added a local `OpCode` type and public gas-regulator test accessor.
- Replaced overflowing `uint64` defaults such as `1000000 * 1e18`.
- Fixed ML-DSA fixed-size serialization/signature output and strengthened the hash fallback so tampered signatures are rejected.
- Replaced a local absolute import path in the ML-DSA integration test.
- Added account/storage proof generation from state trie snapshots.
- Added receipt/log proof generation and local verification.
- Added trie commit/node persistence APIs for proof inspection.
- Added reorg rollback for balances, nonces, code, storage, receipts, logs,
  snapshots, trie commits, and pending transactions.
- Added peer block sync that replays remote block transactions locally, rebuilds
  state/receipt/log commitments, and imports only matching blocks.
- Added `bhte_replayChain` for audit-style canonical chain replay from the
  genesis snapshot without mutating live node state.
- Upgraded `bhte_validateBlock` from header-only checks to candidate block
  replay validation against the parent state snapshot.
- Added basic peer scoring and temporary bans for repeated invalid sync attempts.
- Added a BHTE block index and `bhte_forkChoiceStatus` so the node can report
  canonical head, best known head, cumulative weight, known blocks, and tips.
- Added `bhte_chain.json` as a separate chain database file for blocks,
  canonical mappings, and block index data, while keeping old state JSON
  compatibility.
- Added `bhte_state_db.json` as a separate state database file for accounts,
  balances, nonces, code, storage, and state snapshots.

### BHTWalletPro

The wallet builds successfully with the project script:

```powershell
cd D:\work0\BHT\BHTWalletPro
.\build.bat
```

Verified artifact:

```text
D:\work0\BHT\BHTWalletPro\build\bin\bht-wallet.exe
```

Runtime deployment is handled by `build.bat` after compilation:

- `windeployqt` copies Qt runtime DLLs and plugins, including `platforms/qwindows.dll`.
- PostgreSQL OpenSSL runtime DLLs (`libssl-3-x64.dll`, `libcrypto-3-x64.dll`) are copied into `build/bin`.

Native startup smoke test passed: the process opened a window titled `BHT Wallet Pro`.

Recent UI/functionality expansion:

- Added a Bitcoin Core style multi-page workspace with left navigation.
- Added pages for portfolio overview, transactions, address book, BHTE bridge, BHT/BHTE nodes, security, hardware wallet, settings, and optional developer console.
- Added runtime language selection for the wallet workspace: English and Simplified Chinese.
- Added developer mode toggle; developer console and diagnostics are hidden until enabled.
- Improved BHT/BHTE dual-chain affordances in send/receive flows, address validation, bridge page, node page, and settings.

## Current Risk Notes

- BHTE's Go ML-DSA (`BHTE/mldsa`) is now a genuine FIPS 204 lattice implementation (real SHAKE128/256 via `crypto/sha3`, negacyclic NTT validated against a schoolbook convolution, rejection-sampling sign, hint-based verify) rather than the previous hash-based stub. It is self-consistent and interoperable within BHTE, but it has **not** yet been validated against NIST ACVP known-answer test vectors, so it should not be treated as certified before that step. Signature sizes are now the correct FIPS 204 values (ML-DSA-65 = 3309, ML-DSA-87 = 4627 bytes).
- The BHTE zkEVM ML-DSA precompile (`BHTE/core/vm/contracts_mldsa.go`) now performs real ML-DSA-65 verification through the `mldsa` package instead of the previous 4-byte magic-prefix check.
- The BHTE SPV bridge (`BHTE/spv/spv_bridge.go`) now uses correct cryptography: the compact `nBits` target decode follows the Bitcoin formula, and Merkle proofs are verified over raw little-endian bytes with double-SHA256 (previously it concatenated ASCII hex and used a wrong target formula). Unit tests cover these without a live node.
- The Solidity settlement contract (`BHTE/contracts/BHTEsettlement.sol`) now compiles (it previously contained HTML-escaped `=>` and a wrong OpenZeppelin v5 import path) and performs **native on-chain ML-DSA verification** via the zkEVM precompile at `0x...0100`, with the owner-managed commitment retained only as an explicit fallback for stock EVMs. Anchor submission is gated by a real multi-validator ECDSA proof check instead of `return true`. Verified to compile with solc 0.8.24 + OpenZeppelin 5.0.2.
- BHC's C++ ML-DSA still has two paths: a real liboqs-backed path (`USE_MLDSA`) and a non-liboqs fallback that is NOT real ML-DSA. A complete pure-C++ FIPS 204 ML-DSA (Keccak/SHAKE + keygen/sign/verify, ported from the verified Go implementation) now exists as `BHC/tools/mldsa_fips204_selftest.cpp` and compiles cleanly under MSVC, but it has not yet replaced the fallback in `src/crypto/mldsa.cpp`: this machine's antivirus blocks or removes freshly built crypto executables, so the post-fix self-test cannot be re-run here. Rechecked on 2026-06-14 with `D:\11` MSVC: compilation produced `.testbin/mldsa_selftest.exe`, then execution failed with `Access is denied`. Integrate it only after re-running the self-test on a host with an AV exclusion, and reconcile sizes to FIPS 204 (ML-DSA-65 = 3309, ML-DSA-87 = 4627).
- `BHC/tools/standalone_test.cpp` was fixed to compile on stock MSVC: the non-portable `unsigned __int256` proof-of-work check was replaced with a correct, portable compact-target comparison. Verified: it compiles and runs (`ProgPoW` + ML-DSA-stub smoke pass).
- `go test ./...` still hits a Windows temporary-executable permission issue for two packages; use `BHTE/run_tests.ps1` for now.
- `BHTWalletPro` has a build artifact and startup smoke pass, but still needs interactive GUI workflow testing.

## Recommended Next Steps

1. Replace BHTE's simplified selector-level execution with a fuller
   EVM/state-transition implementation.
2. Move BHTE state, trie nodes, receipts, and chain indexes from JSON files into
   a production-style database layout.
3. Replace RPC pull-style peer sync with real P2P, fork choice, peer scoring,
   and state/header sync.
4. Implement BHC L1 watcher, BHTE bridge event watcher, withdrawal broadcast,
   and reorg-aware bridge accounting.
5. Re-run `BHC/tools/mldsa_fips204_selftest.cpp` on a host with an antivirus
   exclusion, then wire the verified implementation into `BHC/src/crypto` and
   `BHTWalletPro`.
6. Run NIST ACVP/KAT vectors against both Go and C++ ML-DSA before treating the
   implementation as certified.
7. Exercise BHTWalletPro manually and replace placeholder wallet DB/HD/transaction
   builder paths before any real-fund use.
8. Run long-lived multi-node testnets and external security audits before any
   public deployment.
