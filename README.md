# BHC + BHTE Project Status

Last verified: 2026-06-14

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

The Go package structure has been repaired enough for all packages to compile, and tests pass using the Windows-safe script:

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

1. Re-run `BHC/tools/mldsa_fips204_selftest.cpp` on a host with an antivirus exclusion, then wire the verified implementation into `BHC/src/crypto/mldsa.cpp` and `BHTWalletPro/src/crypto/mldsa_signer.cpp`.
2. Run NIST ACVP/KAT vectors against both Go and C++ ML-DSA before treating the implementation as certified.
3. Run `BHTE/run_tests.ps1` in CI or a clean Windows shell and preserve the output.
4. Add focused tests for BHTE state channels, AuxPoW, SPV bridge, and settlement contract behavior.
5. Exercise BHTWalletPro manually: create/open wallet, send dialog validation, receive address generation, transaction history, Layer 2 connection settings.
6. Run an external security audit before any public deployment.
