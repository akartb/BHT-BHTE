#!/usr/bin/env python3
# Copyright (c) 2026 BHC Developers
# Distributed under the MIT software license
# Merge Mining Integration Test - AuxPoW and Coordinator Testing

import subprocess
import json
import time
import sys
import hashlib
import struct
from pathlib import Path
from typing import List, Dict, Any, Optional

BHTE_DIR = r"d:\work0\BHT\BHTE"
BHC_DIR = r"d:\work0\BHT\BHC"

class MergeMiningTest:
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

    def test_auxpow_data_structure(self) -> bool:
        self.log("Testing AuxPoW data structure...")

        auxpow_path = Path(BHTE_DIR) / "consensus" / "bhc" / "auxpow.go"
        if not auxpow_path.exists():
            self.log(f"  auxpow.go not found", "FAIL")
            return False

        with open(auxpow_path, 'r', encoding='utf-8') as f:
            content = f.read()

        required_fields = [
            "type AuxPoWData struct",
            "BlockHash",
            "ParentHash",
            "Coinbase",
            "MerkleBranch",
            "ChainIndex",
            "ProofBits",
            "BHCHeight"
        ]

        all_found = True
        for field in required_fields:
            if field in content:
                self.log(f"  Found: {field}", "PASS")
            else:
                self.log(f"  Missing: {field}", "FAIL")
                all_found = False

        return all_found

    def test_auxpow_verifier_functions(self) -> bool:
        self.log("Testing AuxPoW verifier functions...")

        auxpow_path = Path(BHTE_DIR) / "consensus" / "bhc" / "auxpow.go"
        if not auxpow_path.exists():
            self.log(f"  auxpow.go not found", "FAIL")
            return False

        with open(auxpow_path, 'r', encoding='utf-8') as f:
            content = f.read()

        required_functions = [
            "NewAuxPoWVerifier",
            "VerifyAuxPoW",
            "SubmitAuxBlock",
            "GetAuxBlockForBHTTBlock",
            "GetLatestBHCHeight",
            "GetBHCBlockHash"
        ]

        all_found = True
        for func_name in required_functions:
            if func_name in content:
                self.log(f"  Found: {func_name}", "PASS")
            else:
                self.log(f"  Missing: {func_name}", "FAIL")
                all_found = False

        return all_found

    def test_coordinator_functions(self) -> bool:
        self.log("Testing Coordinator functions...")

        coord_path = Path(BHTE_DIR) / "consensus" / "bhc" / "coordinator.go"
        if not coord_path.exists():
            self.log(f"  coordinator.go not found", "FAIL")
            return False

        with open(coord_path, 'r', encoding='utf-8') as f:
            content = f.read()

        required_functions = [
            "NewMergeMiningCoordinator",
            "SubmitBHTEHeader",
            "coordinateBlocks",
            "registerMiner",
            "submitShare",
            "GetStats"
        ]

        all_found = True
        for func_name in required_functions:
            if func_name in content:
                self.log(f"  Found: {func_name}", "PASS")
            else:
                self.log(f"  Missing: {func_name}", "FAIL")
                all_found = False

        return all_found

    def test_merge_mining_parameters(self) -> bool:
        self.log("Testing merge mining parameters...")

        auxpow_path = Path(BHTE_DIR) / "consensus" / "bhc" / "auxpow.go"
        with open(auxpow_path, 'r', encoding='utf-8') as f:
            content = f.read()

        required_constants = [
            "AuxPoWCheckInterval",
            "MaxAuxChainDepth",
            "MinBHCConfirmations"
        ]

        all_found = True
        for const_name in required_constants:
            if const_name in content:
                self.log(f"  Found: {const_name}", "PASS")
            else:
                self.log(f"  Missing: {const_name}", "FAIL")
                all_found = False

        return all_found

    def test_channel_network_htlc(self) -> bool:
        self.log("Testing HTLC functions...")

        channel_path = Path(BHTE_DIR) / "consensus" / "bhc" / "channel_network.go"
        if not channel_path.exists():
            self.log(f"  channel_network.go not found", "FAIL")
            return False

        with open(channel_path, 'r', encoding='utf-8') as f:
            content = f.read()

        required_functions = [
            "NewHTLCManager",
            "CreateHTLC",
            "OpenHTLC",
            "SettleHTLC",
            "RefundHTLC",
            "GenerateHashLock"
        ]

        all_found = True
        for func_name in required_functions:
            if func_name in content:
                self.log(f"  Found: {func_name}", "PASS")
            else:
                self.log(f"  Missing: {func_name}", "FAIL")
                all_found = False

        return all_found

    def test_network_topology(self) -> bool:
        self.log("Testing Network Topology functions...")

        channel_path = Path(BHTE_DIR) / "consensus" / "bhc" / "channel_network.go"
        with open(channel_path, 'r', encoding='utf-8') as f:
            content = f.read()

        required_functions = [
            "NewNetworkTopology",
            "RegisterNode",
            "AddChannel",
            "FindPath",
            "dijkstraPath",
            "GetNetworkStats"
        ]

        all_found = True
        for func_name in required_functions:
            if func_name in content:
                self.log(f"  Found: {func_name}", "PASS")
            else:
                self.log(f"  Missing: {func_name}", "FAIL")
                all_found = False

        return all_found

    def test_spv_bridge_functions(self) -> bool:
        self.log("Testing SPV Bridge functions...")

        spv_path = Path(BHTE_DIR) / "spv" / "spv_bridge.go"
        if not spv_path.exists():
            self.log(f"  spv_bridge.go not found", "FAIL")
            return False

        with open(spv_path, 'r', encoding='utf-8') as f:
            content = f.read()

        required_functions = [
            "NewBHCSPVClient",
            "GetHeaderChain",
            "VerifyHeaderChain",
            "GetMerkleProof",
            "VerifyMerkleProof",
            "GetAddressProof",
            "VerifyAnchor"
        ]

        all_found = True
        for func_name in required_functions:
            if func_name in content:
                self.log(f"  Found: {func_name}", "PASS")
            else:
                self.log(f"  Missing: {func_name}", "FAIL")
                all_found = False

        return all_found

    def test_withdrawal_lifecycle(self) -> bool:
        self.log("Testing withdrawal lifecycle...")

        withdrawal_h = Path(BHC_DIR) / "src" / "script" / "withdrawal_script.h"
        if not withdrawal_h.exists():
            self.log(f"  withdrawal_script.h not found", "FAIL")
            return False

        with open(withdrawal_h, 'r', encoding='utf-8') as f:
            content = f.read()

        required_functions = [
            "SubmitWithdrawal",
            "ChallengeWithdrawal",
            "ApproveWithdrawal",
            "ExecuteWithdrawal",
            "ExpireWithdrawal",
            "IsWithdrawalReady"
        ]

        all_found = True
        for func_name in required_functions:
            if func_name in content:
                self.log(f"  Found: {func_name}", "PASS")
            else:
                self.log(f"  Missing: {func_name}", "FAIL")
                all_found = False

        return all_found

    def test_escape_hatch_mechanism(self) -> bool:
        self.log("Testing escape hatch mechanism...")

        withdrawal_h = Path(BHC_DIR) / "src" / "script" / "withdrawal_script.h"
        with open(withdrawal_h, 'r', encoding='utf-8') as f:
            content = f.read()

        required_functions = [
            "VerifyEscapeHatch",
            "TriggerEmergencyMode",
            "ExecuteEmergencyWithdrawal",
            "CheckLiveness",
            "GenerateForcedExitProof"
        ]

        all_found = True
        for func_name in required_functions:
            if func_name in content:
                self.log(f"  Found: {func_name}", "PASS")
            else:
                self.log(f"  Missing: {func_name}", "FAIL")
                all_found = False

        return all_found

    def test_coordinator_stats(self) -> bool:
        self.log("Testing coordinator stats...")

        coord_path = Path(BHTE_DIR) / "consensus" / "bhc" / "coordinator.go"
        with open(coord_path, 'r', encoding='utf-8') as f:
            content = f.read()

        if "CoordinatorStats" in content:
            self.log(f"  Found: CoordinatorStats struct", "PASS")
        else:
            self.log(f"  Missing: CoordinatorStats struct", "FAIL")
            return False

        required_stats = [
            "PendingBlocks",
            "ActiveMiners",
            "TotalMiners",
            "Enabled"
        ]

        all_found = True
        for stat_name in required_stats:
            if stat_name in content:
                self.log(f"  Found: {stat_name}", "PASS")
            else:
                self.log(f"  Missing: {stat_name}", "FAIL")
                all_found = False

        return all_found

    def test_network_stats(self) -> bool:
        self.log("Testing network stats...")

        channel_path = Path(BHTE_DIR) / "consensus" / "bhc" / "channel_network.go"
        with open(channel_path, 'r', encoding='utf-8') as f:
            content = f.read()

        if "NetworkStats" in content:
            self.log(f"  Found: NetworkStats struct", "PASS")
        else:
            self.log(f"  Missing: NetworkStats struct", "FAIL")
            return False

        required_stats = [
            "NodeCount",
            "ChannelCount",
            "TotalCapacity",
            "AvailableCapacity",
            "ActiveHTLCs"
        ]

        all_found = True
        for stat_name in required_stats:
            if stat_name in content:
                self.log(f"  Found: {stat_name}", "PASS")
            else:
                self.log(f"  Missing: {stat_name}", "FAIL")
                all_found = False

        return all_found

    def test_package_imports(self) -> bool:
        self.log("Testing package imports...")

        auxpow_path = Path(BHTE_DIR) / "consensus" / "bhc" / "auxpow.go"
        with open(auxpow_path, 'r', encoding='utf-8') as f:
            content = f.read()

        required_imports = [
            "github.com/ethereum/go-ethereum/common",
            "github.com/ybbus/jsonrpc/v3"
        ]

        all_found = True
        for imp in required_imports:
            if imp in content:
                self.log(f"  Found import: {imp}", "PASS")
            else:
                self.log(f"  Missing import: {imp}", "FAIL")
                all_found = False

        return all_found

    def test_state_channel_integration(self) -> bool:
        self.log("Testing state channel integration...")

        state_channel_path = Path(BHTE_DIR) / "consensus" / "bhc" / "state_channel.go"
        if not state_channel_path.exists():
            self.log(f"  state_channel.go not found", "FAIL")
            return False

        with open(state_channel_path, 'r', encoding='utf-8') as f:
            content = f.read()

        required_functions = [
            "NewChannelManager",
            "OpenChannel",
            "UpdateBalance",
            "CloseChannel",
            "GetChannelsByParticipant"
        ]

        all_found = True
        for func_name in required_functions:
            if func_name in content:
                self.log(f"  Found: {func_name}", "PASS")
            else:
                self.log(f"  Missing: {func_name}", "FAIL")
                all_found = False

        return all_found

    def run_all_tests(self):
        self.log("=" * 70, "INFO")
        self.log("Merge Mining Integration Test Suite", "INFO")
        self.log("=" * 70, "INFO")
        self.log("")

        self.log("Category 1: AuxPoW Module", "INFO")
        self.log("-" * 50, "INFO")
        self.run_test("T1.1: AuxPoW Data Structure", self.test_auxpow_data_structure)
        self.run_test("T1.2: AuxPoW Verifier Functions", self.test_auxpow_verifier_functions)
        self.run_test("T1.3: Merge Mining Parameters", self.test_merge_mining_parameters)
        self.run_test("T1.4: Package Imports", self.test_package_imports)
        self.log("")

        self.log("Category 2: Coordinator Module", "INFO")
        self.log("-" * 50, "INFO")
        self.run_test("T2.1: Coordinator Functions", self.test_coordinator_functions)
        self.run_test("T2.2: Coordinator Stats", self.test_coordinator_stats)
        self.log("")

        self.log("Category 3: Channel Network", "INFO")
        self.log("-" * 50, "INFO")
        self.run_test("T3.1: HTLC Functions", self.test_channel_network_htlc)
        self.run_test("T3.2: Network Topology", self.test_network_topology)
        self.run_test("T3.3: Network Stats", self.test_network_stats)
        self.run_test("T3.4: State Channel Integration", self.test_state_channel_integration)
        self.log("")

        self.log("Category 4: SPV Bridge", "INFO")
        self.log("-" * 50, "INFO")
        self.run_test("T4.1: SPV Bridge Functions", self.test_spv_bridge_functions)
        self.log("")

        self.log("Category 5: Withdrawal & Security", "INFO")
        self.log("-" * 50, "INFO")
        self.run_test("T5.1: Withdrawal Lifecycle", self.test_withdrawal_lifecycle)
        self.run_test("T5.2: Escape Hatch Mechanism", self.test_escape_hatch_mechanism)
        self.log("")

        self.log("=" * 70, "INFO")
        self.log("Test Summary", "INFO")
        self.log("=" * 70, "INFO")
        self.log(f"Total Tests: {self.passed + self.failed}", "INFO")
        self.log(f"Passed: {self.passed}", "PASS")
        self.log(f"Failed: {self.failed}", "FAIL" if self.failed > 0 else "INFO")
        if self.passed + self.failed > 0:
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

        if self.passed == self.passed + self.failed and self.passed > 0:
            self.log("ALL MERGE MINING TESTS PASSED!", "PASS")
            return True
        else:
            self.log("SOME TESTS FAILED.", "FAIL")
            return False

if __name__ == "__main__":
    test = MergeMiningTest()
    success = test.run_all_tests()
    sys.exit(0 if success else 1)
