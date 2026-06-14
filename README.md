# BHC + BHTE Project Status

Last verified: 2026-06-13

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
- ML-DSA signing and verification succeeds.
- Performance smoke result was about 338k ProgPoW hashes/sec and 19 microseconds per ML-DSA sign+verify cycle in the current environment.

### BHTE

The Go package structure has been repaired enough for all packages to compile, and tests pass using the Windows-safe script:

```powershell
cd D:\work0\BHT\BHTE
.\run_tests.ps1
```

Why the script exists:

- `go test ./...` currently passes most packages but fails to execute generated test binaries for `bhte/mldsa` and `bhte/test` from Go's deep temporary build path with `Access is denied` on this Windows machine.
- The same packages pass when the test binaries are emitted to a stable project path with `go test -o`.

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
- BHC's C++ ML-DSA still has two paths: a real liboqs-backed path (`USE_MLDSA`) and a non-liboqs fallback that is NOT real ML-DSA. Builds without liboqs (e.g. `standalone_test.exe`) use the fallback.
- The Solidity settlement contract uses an owner-managed verified commitment mechanism for ML-DSA acceptance. That is safer than length-only validation, but it is not the same as native on-chain cryptographic verification.
- `go test ./...` still hits a Windows temporary-executable permission issue for two packages; use `BHTE/run_tests.ps1` for now.
- `BHTWalletPro` has a build artifact and startup smoke pass, but still needs interactive GUI workflow testing.

## Recommended Next Steps

1. Run `BHTE/run_tests.ps1` in CI or a clean Windows shell and preserve the output.
2. Add focused tests for BHTE state channels, AuxPoW, SPV bridge, and settlement contract behavior.
3. Exercise BHTWalletPro manually: create/open wallet, send dialog validation, receive address generation, transaction history, Layer 2 connection settings.
4. Replace prototype ML-DSA fallback code with liboqs-backed production verification before mainnet.
5. Run an external security audit before any public deployment.
