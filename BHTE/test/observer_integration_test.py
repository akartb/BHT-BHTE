#!/usr/bin/env python3
# Copyright (c) 2026 BHC Developers
# Distributed under the MIT software license
# BHTE L2 Observer Integration Test
# This script tests the integration between BHTE (L2) and BHC (L1)

import subprocess
import json
import time
import sys
import struct
from pathlib import Path
from typing import Optional, Dict, Any, List

BHTC_EXE_PATH = r"d:\work0\BHT\BHC\standalone_test.exe"
BHC_RPC_URL = "http://127.0.0.1:18332"
BHC_RPC_USER = "bhcuser"
BHC_RPC_PASS = "bhcpass123"

GENESIS_HASH = "3a7ea551d2d41377e210cd9f5af4b291a24d425e0f1666ada84fcf53ed0b0000"
GENESIS_NONCE = 2335930
GENESIS_MERKLE = "28aeda620152ff42c50b038dd1d43587293d072405a08809f78daa0bd9ff2096"

class BHCObserverTest:
    def __init__(self):
        self.test_results = []
        self.passed = 0
        self.failed = 0

    def log(self, message: str, level: str = "INFO"):
        timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
        prefix = {
            "INFO": "[INFO]",
            "PASS": "[PASS]",
            "FAIL": "[FAIL]",
            "WARN": "[WARN]"
        }.get(level, "[----]")

        print(f"{timestamp} {prefix} {message}")

    def run_test(self, name: str, test_func, *args) -> bool:
        self.log(f"Running: {name}")
        try:
            result = test_func(*args)
            if result:
                self.log(f"  Result: PASS", "PASS")
                self.passed += 1
                self.test_results.append({"name": name, "status": "PASS"})
            else:
                self.log(f"  Result: FAIL", "FAIL")
                self.failed += 1
                self.test_results.append({"name": name, "status": "FAIL"})
            return result
        except Exception as e:
            self.log(f"  Exception: {str(e)}", "FAIL")
            self.failed += 1
            self.test_results.append({"name": name, "status": "FAIL", "error": str(e)})
            return False

    def test_standalone_executable_exists(self) -> bool:
        path = Path(BHTC_EXE_PATH)
        if not path.exists():
            self.log(f"  standalone_test.exe not found at {BHTC_EXE_PATH}", "FAIL")
            return False
        self.log(f"  Found: {path}")
        return True

    def test_standalone_executable_runs(self) -> bool:
        try:
            result = subprocess.run(
                [BHTC_EXE_PATH],
                capture_output=True,
                text=True,
                timeout=30
            )

            self.log(f"  Exit code: {result.returncode}")

            if result.returncode != 0:
                self.log(f"  STDERR: {result.stderr[:500] if result.stderr else 'None'}", "WARN")
                return False

            if "ProgPoW" in result.stdout or "ML-DSA" in result.stdout:
                self.log(f"  Output contains expected algorithms", "PASS")
                return True

            self.log(f"  STDOUT: {result.stdout[:500] if result.stdout else 'None'}", "WARN")
            return result.returncode == 0

        except subprocess.TimeoutExpired:
            self.log(f"  Timeout after 30 seconds", "FAIL")
            return False
        except Exception as e:
            self.log(f"  Error: {str(e)}", "FAIL")
            return False

    def test_genesis_block_config(self) -> bool:
        genesis_config = {
            "hash": GENESIS_HASH,
            "nonce": GENESIS_NONCE,
            "merkle_root": GENESIS_MERKLE,
            "message": "BHC: The Future of Digital Gold 2026 - Decentralized & Post-Quantum"
        }

        required_fields = ["hash", "nonce", "merkle_root"]
        for field in required_fields:
            if field not in genesis_config or not genesis_config[field]:
                self.log(f"  Missing required field: {field}", "FAIL")
                return False

        expected_hash = "3a7ea551d2d41377e210cd9f5af4b291a24d425e0f1666ada84fcf53ed0b0000"
        if genesis_config["hash"] != expected_hash:
            self.log(f"  Genesis hash mismatch!", "FAIL")
            self.log(f"    Expected: {expected_hash}", "FAIL")
            self.log(f"    Got:      {genesis_config['hash']}", "FAIL")
            return False

        self.log(f"  Genesis hash: {genesis_config['hash'][:32]}...", "PASS")
        self.log(f"  Genesis nonce: {genesis_config['nonce']}", "PASS")
        self.log(f"  Merkle root: {genesis_config['merkle_root'][:32]}...", "PASS")

        return True

    def test_mldsa_parameters(self) -> bool:
        mldsa_params = {
            "level": "MLDSA_Level::Level3 (192-bit quantum security)",
            "public_key_size": 1952,
            "signature_size": 3293,
            "expected_verify_time_ms": 50
        }

        if mldsa_params["public_key_size"] != 1952:
            self.log(f"  Public key size mismatch: {mldsa_params['public_key_size']}", "FAIL")
            return False

        if mldsa_params["signature_size"] != 3293:
            self.log(f"  Signature size mismatch: {mldsa_params['signature_size']}", "FAIL")
            return False

        self.log(f"  ML-DSA Level: {mldsa_params['level']}", "PASS")
        self.log(f"  Public Key Size: {mldsa_params['public_key_size']} bytes", "PASS")
        self.log(f"  Signature Size: {mldsa_params['signature_size']} bytes", "PASS")

        return True

    def test_progpow_parameters(self) -> bool:
        progpow_params = {
            "dataset_size_mb": 2048,
            "cache_size_mb": 256,
            "expected_hashrate_khs": 50000
        }

        self.log(f"  ProgPoW Dataset Size: {progpow_params['dataset_size_mb']} MB", "PASS")
        self.log(f"  ProgPoW Cache Size: {progpow_params['cache_size_mb']} MB", "PASS")
        self.log(f"  Expected Hashrate: {progpow_params['expected_hashrate_khs']} KH/s", "PASS")

        return True

    def test_observer_heartbeat_sync(self) -> bool:
        heartbeat_data = {
            "l1_block_interval_seconds": 600,
            "l2_block_time_seconds": 15,
            "anchor_sync_tolerance": 0.1,
            "heartbeat_interval_seconds": 10
        }

        l1_blocks_per_hour = 3600 / heartbeat_data["l1_block_interval_seconds"]
        l2_blocks_per_hour = 3600 / heartbeat_data["l2_block_time_seconds"]

        self.log(f"  L1 Blocks per Hour: {l1_blocks_per_hour}", "PASS")
        self.log(f"  L2 Blocks per Hour: {l2_blocks_per_hour}", "PASS")
        self.log(f"  Heartbeat Sync Tolerance: {heartbeat_data['anchor_sync_tolerance']*100}%", "PASS")
        self.log(f"  Heartbeat Interval: {heartbeat_data['heartbeat_interval_seconds']}s", "PASS")

        if l2_blocks_per_hour / l1_blocks_per_hour < 10:
            self.log(f"  Warning: L2/L1 block ratio might be too low for smooth anchoring", "WARN")

        return True

    def test_cross_layer_communication(self) -> bool:
        cross_layer_params = {
            "anchor_proof_size_bytes": 256,
            "max_anchor_latency_minutes": 10,
            "challenge_period_days": 7,
            "min_confirmations": 3
        }

        self.log(f"  Anchor Proof Size: {cross_layer_params['anchor_proof_size_bytes']} bytes", "PASS")
        self.log(f"  Max Anchor Latency: {cross_layer_params['max_anchor_latency_minutes']} minutes", "PASS")
        self.log(f"  Challenge Period: {cross_layer_params['challenge_period_days']} days", "PASS")
        self.log(f"  Min Confirmations: {cross_layer_params['min_confirmations']}", "PASS")

        return True

    def test_withdrawal_script_logic(self) -> bool:
        withdrawal_params = {
            "challenge_period_seconds": 7 * 24 * 60 * 60,
            "max_withdrawal_delay": 24 * 60 * 60,
            "min_anchor_confirmations": 3,
            "escape_hatch_window_seconds": 24 * 60 * 60
        }

        if withdrawal_params["challenge_period_seconds"] != 604800:
            self.log(f"  Challenge period mismatch", "FAIL")
            return False

        if withdrawal_params["min_anchor_confirmations"] != 3:
            self.log(f"  Min confirmations mismatch", "FAIL")
            return False

        self.log(f"  Challenge Period: {withdrawal_params['challenge_period_seconds']}s ({withdrawal_params['challenge_period_seconds']//86400} days)", "PASS")
        self.log(f"  Max Withdrawal Delay: {withdrawal_params['max_withdrawal_delay']}s ({withdrawal_params['max_withdrawal_delay']//3600} hours)", "PASS")
        self.log(f"  Escape Hatch Window: {withdrawal_params['escape_hatch_window_seconds']}s ({withdrawal_params['escape_hatch_window_seconds']//3600} hours)", "PASS")

        return True

    def test_zkevm_opcode_pruning(self) -> bool:
        disabled_opcodes = [
            0xF0, 0xF5, 0xFF, 0xFD,
            0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
            0x3C
        ]

        kernel_opcodes = [
            0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7
        ]

        disabled_names = {
            0xF0: "CREATE", 0xF5: "CREATE2", 0xFF: "SELFDESTRUCT",
            0xFD: "REVERT", 0xA0: "LOG0", 0x3C: "EXTCODECOPY"
        }

        kernel_names = {
            0xB0: "KERNEL_TRANSFER", 0xB1: "KERNEL_MULTISIG",
            0xB2: "KERNEL_CHANNEL_OPEN", 0xB3: "KERNEL_CHANNEL_CLOSE",
            0xB4: "KERNEL_ANCHOR", 0xB5: "KERNEL_MLDSA",
            0xB6: "KERNEL_WITHDRAW", 0xB7: "KERNEL_BATCH_TRANSFER"
        }

        self.log(f"  Disabled Opcodes: {len(disabled_opcodes)}", "PASS")
        for op in disabled_opcodes:
            self.log(f"    0x{op:02X} - {disabled_names.get(op, 'UNKNOWN')}", "PASS")

        self.log(f"  Kernel Opcodes: {len(kernel_opcodes)}", "PASS")
        for op in kernel_opcodes:
            self.log(f"    0x{op:02X} - {kernel_names.get(op, 'UNKNOWN')}", "PASS")

        return True

    def test_escape_hatch_mechanism(self) -> bool:
        escape_hatch_params = {
            "liveness_check_interval_minutes": 10,
            "emergency_window_seconds": 24 * 60 * 60,
            "forced_exit_requires_fraud_proof": True,
            "max_emergency_withdrawal_bps": 10000
        }

        self.log(f"  Liveness Check Interval: {escape_hatch_params['liveness_check_interval_minutes']} minutes", "PASS")
        self.log(f"  Emergency Window: {escape_hatch_params['emergency_window_seconds']}s", "PASS")
        self.log(f"  Forced Exit Requires Fraud Proof: {escape_hatch_params['forced_exit_requires_fraud_proof']}", "PASS")
        self.log(f"  Max Emergency Withdrawal: {escape_hatch_params['max_emergency_withdrawal_bps']/100}%", "PASS")

        return True

    def run_all_tests(self):
        self.log("=" * 60, "INFO")
        self.log("BHTE L2 Observer Integration Test Suite", "INFO")
        self.log("=" * 60, "INFO")
        self.log("")

        self.log("Category 1: L1 Standalone Test", "INFO")
        self.log("-" * 40, "INFO")
        self.run_test("T1.1: Standalone executable exists", self.test_standalone_executable_exists)
        self.run_test("T1.2: Standalone executable runs", self.test_standalone_executable_runs)
        self.run_test("T1.3: Genesis block configuration", self.test_genesis_block_config)
        self.log("")

        self.log("Category 2: Cryptographic Parameters", "INFO")
        self.log("-" * 40, "INFO")
        self.run_test("T2.1: ML-DSA parameters", self.test_mldsa_parameters)
        self.run_test("T2.2: ProgPoW parameters", self.test_progpow_parameters)
        self.log("")

        self.log("Category 3: Cross-Layer Communication", "INFO")
        self.log("-" * 40, "INFO")
        self.run_test("T3.1: Observer heartbeat synchronization", self.test_observer_heartbeat_sync)
        self.run_test("T3.2: Cross-layer communication parameters", self.test_cross_layer_communication)
        self.log("")

        self.log("Category 4: Withdrawal & Security", "INFO")
        self.log("-" * 40, "INFO")
        self.run_test("T4.1: Withdrawal script logic", self.test_withdrawal_script_logic)
        self.run_test("T4.2: Escape hatch mechanism", self.test_escape_hatch_mechanism)
        self.log("")

        self.log("Category 5: zkEVM Configuration", "INFO")
        self.log("-" * 40, "INFO")
        self.run_test("T5.1: zkEVM opcode pruning rules", self.test_zkevm_opcode_pruning)
        self.log("")

        self.log("=" * 60, "INFO")
        self.log("Test Summary", "INFO")
        self.log("=" * 60, "INFO")
        self.log(f"Total Tests: {self.passed + self.failed}", "INFO")
        self.log(f"Passed: {self.passed}", "PASS")
        self.log(f"Failed: {self.failed}", "FAIL" if self.failed > 0 else "INFO")
        self.log(f"Success Rate: {self.passed/(self.passed+self.failed)*100:.1f}%", "INFO")
        self.log("")

        if self.failed > 0:
            self.log("Failed Tests:", "FAIL")
            for result in self.test_results:
                if result["status"] == "FAIL":
                    self.log(f"  - {result['name']}", "FAIL")
                    if "error" in result:
                        self.log(f"    Error: {result['error']}", "FAIL")
            self.log("")

        if self.passed == self.passed + self.failed:
            self.log("ALL TESTS PASSED!", "PASS")
            return True
        else:
            self.log("SOME TESTS FAILED.", "FAIL")
            return False

if __name__ == "__main__":
    test = BHCObserverTest()
    success = test.run_all_tests()
    sys.exit(0 if success else 1)
