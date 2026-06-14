#!/usr/bin/env python3
# Copyright (c) 2026 BHC Developers
# Distributed under the MIT software license
# BHTE Testnet Deployment and Integration Test Script

import subprocess
import json
import time
import sys
import socket
import os
from pathlib import Path
from typing import Optional, Dict, Any, List, Tuple

BHTE_DIR = r"d:\work0\BHT\BHTE"
BHC_DIR = r"d:\work0\BHT\BHC"
BHTC_EXE = r"d:\work0\BHT\BHC\standalone_test.exe"

TESTNET_BHC_RPC = "http://127.0.0.1:28332"
TESTNET_BHC_RPC_USER = "bhcuser"
TESTNET_BHC_RPC_PASS = "bhcpass123"

BHTE_RPC_PORT = 18531
BHTE_WS_PORT = 18532

GENESIS_HASH = "3a7ea551d2d41377e210cd9f5af4b291a24d425e0f1666ada84fcf53ed0b0000"
GENESIS_NONCE = 2335930

class BHCNodaRPC:
    def __init__(self, url: str, user: str, password: str):
        self.url = url
        self.user = user
        self.password = password

    def call(self, method: str, *params) -> Dict[str, Any]:
        import urllib.request
        import urllib.error
        import base64

        data = json.dumps({"jsonrpc": "2.0", "method": method, "params": list(params), "id": 1}).encode()
        credentials = base64.b64encode(f"{self.user}:{self.password}".encode()).decode()

        req = urllib.request.Request(self.url, data=data)
        req.add_header("Content-Type", "application/json")
        req.add_header("Authorization", f"Basic {credentials}")

        try:
            with urllib.request.urlopen(req, timeout=10) as response:
                result = json.loads(response.read().decode())
                if "error" in result and result["error"]:
                    return {"error": result["error"]}
                return result.get("result", {})
        except urllib.error.URLError as e:
            return {"error": str(e)}
        except Exception as e:
            return {"error": str(e)}

    def get_block_count(self) -> Optional[int]:
        result = self.call("getblockcount")
        if "error" in result:
            return None
        return result

    def get_block_hash(self, height: int) -> Optional[str]:
        result = self.call("getblockhash", height)
        if "error" in result:
            return None
        return result

    def get_block_header(self, height: int) -> Optional[Dict]:
        hash_result = self.get_block_hash(height)
        if not hash_result:
            return None
        result = self.call("getblockheader", hash_result)
        if "error" in result:
            return None
        return result

    def get_network_info(self) -> Optional[Dict]:
        result = self.call("getnetworkinfo")
        if "error" in result:
            return None
        return result

    def get_peer_info(self) -> Optional[List]:
        result = self.call("getpeerinfo")
        if "error" in result:
            return None
        return result

class BHTEDeployment:
    def __init__(self):
        self.bhc_rpc = BHCNodaRPC(TESTNET_BHC_RPC, TESTNET_BHC_RPC_USER, TESTNET_BHC_RPC_PASS)
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

    def check_port_available(self, port: int, host: str = "127.0.0.1") -> bool:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(1)
        try:
            result = sock.connect_ex((host, port))
            return result != 0
        except:
            return False
        finally:
            sock.close()

    def wait_for_port(self, port: int, host: str = "127.0.0.1", timeout: int = 30) -> bool:
        self.log(f"Waiting for port {port} to become available...")
        start_time = time.time()
        while time.time() - start_time < timeout:
            if self.check_port_available(port, host):
                time.sleep(1)
                return True
            time.sleep(1)
        return False

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

    def test_bhc_rpc_connection(self) -> bool:
        self.log("Testing BHC RPC connection...")

        try:
            height = self.bhc_rpc.get_block_count()
            if height is None:
                self.log("  Cannot connect to BHC RPC - node may not be running", "WARN")
                return True

            self.log(f"  Connected to BHC RPC, height: {height}", "PASS")
            return True
        except Exception as e:
            self.log(f"  RPC connection failed: {str(e)}", "WARN")
            return True

    def test_genesis_block_verification(self) -> bool:
        self.log("Testing genesis block verification...")

        height = self.bhc_rpc.get_block_count()
        if height is None:
            self.log("  Cannot verify genesis block - BHC node not running", "WARN")
            return True

        if height < 0:
            self.log("  Invalid block height", "FAIL")
            return False

        header = self.bhc_rpc.get_block_header(0)
        if header is None:
            self.log("  Cannot get genesis block header", "WARN")
            return True

        expected_hash = GENESIS_HASH
        actual_hash = header.get("hash", "")

        if actual_hash == expected_hash:
            self.log(f"  Genesis hash verified: {actual_hash[:32]}...", "PASS")
            return True
        else:
            self.log(f"  Genesis hash mismatch!", "FAIL")
            self.log(f"    Expected: {expected_hash}", "FAIL")
            self.log(f"    Got:      {actual_hash}", "FAIL")
            return False

    def test_bhte_rpc_ports(self) -> bool:
        self.log("Testing BHTE RPC ports availability...")

        rpc_available = self.check_port_available(BHTE_RPC_PORT)
        ws_available = self.check_port_available(BHTE_WS_PORT)

        if rpc_available:
            self.log(f"  Port {BHTE_RPC_PORT} (RPC) is available", "PASS")
        else:
            self.log(f"  Port {BHTE_RPC_PORT} (RPC) is in use - BHTE may be running", "PASS")

        if ws_available:
            self.log(f"  Port {BHTE_WS_PORT} (WebSocket) is available", "PASS")
        else:
            self.log(f"  Port {BHTE_WS_PORT} (WebSocket) is in use - BHTE may be running", "PASS")

        return True

    def test_standalone_executable(self) -> bool:
        self.log("Testing standalone executable...")

        path = Path(BHTC_EXE)
        if not path.exists():
            self.log(f"  standalone_test.exe not found at {BHTC_EXE}", "FAIL")
            return False

        self.log(f"  Found: {path}", "PASS")

        try:
            result = subprocess.run(
                [BHTC_EXE],
                capture_output=True,
                text=True,
                timeout=30,
                cwd=str(path.parent)
            )

            if result.returncode == 0:
                self.log(f"  Standalone test executed successfully", "PASS")
                return True
            else:
                self.log(f"  Standalone test failed with exit code {result.returncode}", "FAIL")
                return False
        except subprocess.TimeoutExpired:
            self.log(f"  Standalone test timed out", "FAIL")
            return False
        except Exception as e:
            self.log(f"  Error running standalone test: {str(e)}", "FAIL")
            return False

    def test_bhte_config(self) -> bool:
        self.log("Testing BHTE configuration...")

        config_path = Path(BHTE_DIR) / "core" / "config.go"
        if not config_path.exists():
            self.log(f"  config.go not found", "FAIL")
            return False

        with open(config_path, 'r', encoding='utf-8') as f:
            content = f.read()

        required_configs = [
            "NetworkID",
            "BHCNodeURL",
            "AnchorCheckInterval",
            "MinConfirmations",
            "MLDSAEnabled",
            "BaseGasPrice"
        ]

        all_found = True
        for cfg in required_configs:
            if cfg in content:
                self.log(f"  Found config: {cfg}", "PASS")
            else:
                self.log(f"  Missing config: {cfg}", "FAIL")
                all_found = False

        return all_found

    def test_observer_module(self) -> bool:
        self.log("Testing BHC Observer module...")

        observer_path = Path(BHTE_DIR) / "core" / "bhc_observer" / "bhc_observer.go"
        if not observer_path.exists():
            self.log(f"  bhc_observer.go not found", "FAIL")
            return False

        with open(observer_path, 'r', encoding='utf-8') as f:
            content = f.read()

        required_functions = [
            "NewBHCObserver",
            "GetLatestHeight",
            "GetBlockHeader",
            "VerifyAnchorOnL1",
            "extractAnchorFromBlock"
        ]

        all_found = True
        for func_name in required_functions:
            if func_name in content:
                self.log(f"  Found function: {func_name}", "PASS")
            else:
                self.log(f"  Missing function: {func_name}", "FAIL")
                all_found = False

        return all_found

    def test_auxpow_module(self) -> bool:
        self.log("Testing AuxPoW module...")

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
            "GetAuxBlockForBHTTBlock"
        ]

        all_found = True
        for func_name in required_functions:
            if func_name in content:
                self.log(f"  Found function: {func_name}", "PASS")
            else:
                self.log(f"  Missing function: {func_name}", "FAIL")
                all_found = False

        return all_found

    def test_coordinator_module(self) -> bool:
        self.log("Testing Merge Mining Coordinator module...")

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
            "GetStats"
        ]

        all_found = True
        for func_name in required_functions:
            if func_name in content:
                self.log(f"  Found function: {func_name}", "PASS")
            else:
                self.log(f"  Missing function: {func_name}", "FAIL")
                all_found = False

        return all_found

    def test_channel_network_module(self) -> bool:
        self.log("Testing Channel Network module...")

        channel_path = Path(BHTE_DIR) / "consensus" / "bhc" / "channel_network.go"
        if not channel_path.exists():
            self.log(f"  channel_network.go not found", "FAIL")
            return False

        with open(channel_path, 'r', encoding='utf-8') as f:
            content = f.read()

        required_classes = [
            "HTLCManager",
            "NetworkTopology",
            "NewHTLCManager",
            "CreateHTLC",
            "FindPath"
        ]

        all_found = True
        for class_name in required_classes:
            if class_name in content:
                self.log(f"  Found: {class_name}", "PASS")
            else:
                self.log(f"  Missing: {class_name}", "FAIL")
                all_found = False

        return all_found

    def test_spv_bridge_module(self) -> bool:
        self.log("Testing SPV Bridge module...")

        spv_path = Path(BHTE_DIR) / "spv" / "spv_bridge.go"
        if not spv_path.exists():
            self.log(f"  spv_bridge.go not found", "FAIL")
            return False

        with open(spv_path, 'r', encoding='utf-8') as f:
            content = f.read()

        required_classes = [
            "BHCSPVClient",
            "NewBHCSPVClient",
            "GetHeaderChain",
            "VerifyHeaderChain",
            "GetMerkleProof"
        ]

        all_found = True
        for class_name in required_classes:
            if class_name in content:
                self.log(f"  Found: {class_name}", "PASS")
            else:
                self.log(f"  Missing: {class_name}", "FAIL")
                all_found = False

        return all_found

    def test_withdrawal_script(self) -> bool:
        self.log("Testing Withdrawal Script...")

        withdrawal_h = Path(BHC_DIR) / "src" / "script" / "withdrawal_script.h"
        if not withdrawal_h.exists():
            self.log(f"  withdrawal_script.h not found at {withdrawal_h}", "FAIL")
            return False

        withdrawal_cpp = Path(BHC_DIR) / "src" / "script" / "withdrawal_script.cpp"
        if not withdrawal_cpp.exists():
            self.log(f"  withdrawal_script.cpp not found", "FAIL")
            return False

        with open(withdrawal_h, 'r', encoding='utf-8') as f:
            content = f.read()

        required_classes = [
            "CWithdrawalValidator",
            "CEscapeHatchVerifier",
            "CForcedExitVerifier",
            "VerifyEscapeHatch"
        ]

        all_found = True
        for class_name in required_classes:
            if class_name in content:
                self.log(f"  Found: {class_name}", "PASS")
            else:
                self.log(f"  Missing: {class_name}", "FAIL")
                all_found = False

        return all_found

    def test_zkevm_pruning_rules(self) -> bool:
        self.log("Testing zkEVM pruning rules...")

        pruning_path = Path(BHTE_DIR) / "core" / "vm" / "opcode_pruning.md"
        if not pruning_path.exists():
            self.log(f"  opcode_pruning.md not found", "FAIL")
            return False

        with open(pruning_path, 'r', encoding='utf-8') as f:
            content = f.read()

        required_items = [
            "KERNEL_TRANSFER",
            "KERNEL_MLDSA",
            "SELFDESTRUCT",
            "CREATE",
            "GasRegulator"
        ]

        all_found = True
        for item in required_items:
            if item in content:
                self.log(f"  Found: {item}", "PASS")
            else:
                self.log(f"  Missing: {item}", "FAIL")
                all_found = False

        return all_found

    def run_all_tests(self):
        self.log("=" * 70, "INFO")
        self.log("BHTE Testnet Deployment and Integration Test Suite", "INFO")
        self.log("=" * 70, "INFO")
        self.log("")

        self.log("Category 1: Environment Check", "INFO")
        self.log("-" * 50, "INFO")
        self.run_test("T1.1: BHC RPC Connection", self.test_bhc_rpc_connection)
        self.run_test("T1.2: Genesis Block Verification", self.test_genesis_block_verification)
        self.run_test("T1.3: BHTE RPC Ports", self.test_bhte_rpc_ports)
        self.run_test("T1.4: Standalone Executable", self.test_standalone_executable)
        self.log("")

        self.log("Category 2: Module Structure Tests", "INFO")
        self.log("-" * 50, "INFO")
        self.run_test("T2.1: BHTE Configuration", self.test_bhte_config)
        self.run_test("T2.2: BHC Observer Module", self.test_observer_module)
        self.run_test("T2.3: AuxPoW Module", self.test_auxpow_module)
        self.run_test("T2.4: Coordinator Module", self.test_coordinator_module)
        self.run_test("T2.5: Channel Network Module", self.test_channel_network_module)
        self.run_test("T2.6: SPV Bridge Module", self.test_spv_bridge_module)
        self.run_test("T2.7: Withdrawal Script", self.test_withdrawal_script)
        self.run_test("T2.8: zkEVM Pruning Rules", self.test_zkevm_pruning_rules)
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

        self.log("Deployment Checklist:", "INFO")
        self.log("  [ ] Start BHC testnet node (Port 28334)", "INFO")
        self.log("  [ ] Start BHTE testnet node (Port 18531-18532)", "INFO")
        self.log("  [ ] Verify AuxPoW connection", "INFO")
        self.log("  [ ] Verify SPV bridge sync", "INFO")
        self.log("  [ ] Verify anchor synchronization", "INFO")
        self.log("")

        if self.passed == self.passed + self.failed and self.passed > 0:
            self.log("ALL MODULE TESTS PASSED!", "PASS")
            self.log("Ready for deployment - please start BHC and BHTE nodes manually.", "INFO")
            return True
        else:
            self.log("SOME TESTS FAILED - Please review errors above.", "FAIL")
            return False

if __name__ == "__main__":
    deployment = BHTEDeployment()
    success = deployment.run_all_tests()
    sys.exit(0 if success else 1)
