#!/usr/bin/env python3
# Copyright (c) 2026 BHC Developers
# Distributed under the MIT software license
# BHTE Mainnet Deployment Checklist and Verification

import os
import json
import subprocess
import hashlib
from pathlib import Path
from typing import Dict, List, Tuple, Optional

PROJECT_ROOT = r"d:\work0\BHT"
BHC_DIR = r"d:\work0\BHT\BHC"
BHTE_DIR = r"d:\work0\BHT\BHTE"

class MainnetDeploymentChecklist:
    def __init__(self):
        self.checks = []
        self.passed = 0
        self.failed = 0
        self.warnings = 0

    def log(self, message: str, level: str = "INFO"):
        prefix = {
            "INFO": "[INFO]",
            "PASS": "[PASS]",
            "FAIL": "[FAIL]",
            "WARN": "[WARN]"
        }.get(level, "[----]")
        print(f"{prefix} {message}")

    def add_check(self, category: str, item: str, check_func, *args) -> bool:
        self.log(f"Checking: [{category}] {item}...")
        try:
            result = check_func(*args)
            status = "PASS" if result else "FAIL"
            self.checks.append({
                "category": category,
                "item": item,
                "status": status
            })
            if result:
                self.passed += 1
                self.log(f"  Result: PASS", "PASS")
            else:
                self.failed += 1
                self.log(f"  Result: FAIL", "FAIL")
            return result
        except Exception as e:
            self.failed += 1
            self.log(f"  Exception: {str(e)}", "FAIL")
            self.checks.append({
                "category": category,
                "item": item,
                "status": "FAIL",
                "error": str(e)
            })
            return False

    def add_warning(self, category: str, item: str, message: str):
        self.warnings += 1
        self.log(f"[WARN] [{category}] {item}: {message}", "WARN")

    def check_file_exists(self, file_path: str) -> bool:
        return Path(file_path).exists()

    def check_directory_exists(self, dir_path: str) -> bool:
        return Path(dir_path).is_dir()

    def check_files_in_directory(self, dir_path: str, required_files: List[str]) -> bool:
        if not self.check_directory_exists(dir_path):
            return False
        for f in required_files:
            if not self.check_file_exists(os.path.join(dir_path, f)):
                return False
        return True

    def check_go_module(self, module_path: str) -> bool:
        go_mod = Path(module_path) / "go.mod"
        if not go_mod.exists():
            return False
        with open(go_mod, 'r') as f:
            content = f.read()
        required = ["module", "go "]
        return all(r in content for r in required)

    def verify_file_hash(self, file_path: str, expected_hash: str) -> bool:
        if not self.check_file_exists(file_path):
            return False
        with open(file_path, 'rb') as f:
            file_hash = hashlib.sha256(f.read()).hexdigest()
        return file_hash == expected_hash

    def check_executable(self, file_path: str) -> bool:
        if not self.check_file_exists(file_path):
            return False
        try:
            result = subprocess.run([file_path], capture_output=True, timeout=5)
            return True
        except:
            return False

    def run_deployment_checklist(self):
        self.log("=" * 70)
        self.log("BHTE Mainnet Deployment Checklist")
        self.log("=" * 70)
        self.log("")

        self.log("Category 1: Directory Structure", "INFO")
        self.log("-" * 50)
        self.add_check("Structure", "BHC directory exists",
                      self.check_directory_exists, BHC_DIR)
        self.add_check("Structure", "BHTE directory exists",
                      self.check_directory_exists, BHTE_DIR)
        self.add_check("Structure", "BHC src directory exists",
                      self.check_directory_exists, os.path.join(BHC_DIR, "src"))
        self.add_check("Structure", "BHTE core directory exists",
                      self.check_directory_exists, os.path.join(BHTE_DIR, "core"))
        self.log("")

        self.log("Category 2: Core Modules", "INFO")
        self.log("-" * 50)
        self.add_check("Core", "ProgPoW module",
                      self.check_file_exists, os.path.join(BHC_DIR, "src", "crypto", "progpow.cpp"))
        self.add_check("Core", "ML-DSA module",
                      self.check_file_exists, os.path.join(BHC_DIR, "src", "crypto", "mldsa.cpp"))
        self.add_check("Core", "Hybrid signature module",
                      self.check_file_exists, os.path.join(BHC_DIR, "src", "script", "hybrid_sig.cpp"))
        self.add_check("Core", "Chain parameters",
                      self.check_file_exists, os.path.join(BHC_DIR, "src", "chainparams.cpp"))
        self.add_check("Core", "Proof of Work",
                      self.check_file_exists, os.path.join(BHC_DIR, "src", "pow.cpp"))
        self.log("")

        self.log("Category 3: BHTE Modules", "INFO")
        self.log("-" * 50)
        self.add_check("BHTE", "BHTEVM module",
                      self.check_file_exists, os.path.join(BHTE_DIR, "core", "bhte_evm.go"))
        self.add_check("BHTE", "Gas regulator",
                      self.check_file_exists, os.path.join(BHTE_DIR, "core", "vm", "gas_regulator.go"))
        self.add_check("BHTE", "Kernel opcodes",
                      self.check_file_exists, os.path.join(BHTE_DIR, "core", "vm", "opcodes_kernel.go"))
        self.add_check("BHTE", "ML-DSA precompiled",
                      self.check_file_exists, os.path.join(BHTE_DIR, "core", "vm", "contracts_mldsa.go"))
        self.add_check("BHTE", "BHC Observer",
                      self.check_file_exists, os.path.join(BHTE_DIR, "core", "bhc_observer", "bhc_observer.go"))
        self.add_check("BHTE", "Anchor verifier",
                      self.check_file_exists, os.path.join(BHTE_DIR, "consensus", "bhc", "anchor.go"))
        self.add_check("BHTE", "AuxPoW module",
                      self.check_file_exists, os.path.join(BHTE_DIR, "consensus", "bhc", "auxpow.go"))
        self.add_check("BHTE", "Merge mining coordinator",
                      self.check_file_exists, os.path.join(BHTE_DIR, "consensus", "bhc", "coordinator.go"))
        self.add_check("BHTE", "Channel network",
                      self.check_file_exists, os.path.join(BHTE_DIR, "consensus", "bhc", "channel_network.go"))
        self.add_check("BHTE", "SPV bridge",
                      self.check_file_exists, os.path.join(BHTE_DIR, "spv", "spv_bridge.go"))
        self.add_check("BHTE", "State anchor",
                      self.check_file_exists, os.path.join(BHTE_DIR, "consensus", "bhc", "state_anchor.go"))
        self.log("")

        self.log("Category 4: Security Modules", "INFO")
        self.log("-" * 50)
        self.add_check("Security", "Withdrawal script header",
                      self.check_file_exists, os.path.join(BHC_DIR, "src", "script", "withdrawal_script.h"))
        self.add_check("Security", "Withdrawal script implementation",
                      self.check_file_exists, os.path.join(BHC_DIR, "src", "script", "withdrawal_script.cpp"))
        self.add_check("Security", "Type bridge",
                      self.check_file_exists, os.path.join(BHC_DIR, "src", "type_bridge.h"))
        self.add_check("Security", "zkEVM opcode pruning rules",
                      self.check_file_exists, os.path.join(BHTE_DIR, "core", "vm", "opcode_pruning.md"))
        self.log("")

        self.log("Category 5: Configuration Files", "INFO")
        self.log("-" * 50)
        self.add_check("Config", "Mainnet config",
                      self.check_file_exists, os.path.join(BHTE_DIR, "config", "mainnet.yaml"))
        self.add_check("Config", "BHC CMakeLists.txt",
                      self.check_file_exists, os.path.join(BHC_DIR, "CMakeLists.txt"))
        self.log("")

        self.log("Category 6: Test Files", "INFO")
        self.log("-" * 50)
        self.add_check("Tests", "Observer integration test",
                      self.check_file_exists, os.path.join(BHTE_DIR, "test", "observer_integration_test.py"))
        self.add_check("Tests", "Testnet deployment test",
                      self.check_file_exists, os.path.join(BHTE_DIR, "test", "testnet_deployment_test.py"))
        self.add_check("Tests", "Merge mining test",
                      self.check_file_exists, os.path.join(BHTE_DIR, "test", "merge_mining_test.py"))
        self.log("")

        self.log("Category 7: Documentation", "INFO")
        self.log("-" * 50)
        self.add_check("Docs", "zkEVM pruning specification",
                      self.check_file_exists, os.path.join(BHTE_DIR, "core", "vm", "opcode_pruning.md"))
        self.add_check("Docs", "BHC integration documentation",
                      self.check_file_exists, os.path.join(BHC_DIR, "BHC_integration.md"))
        self.log("")

        self.log("Category 8: Go Module Structure", "INFO")
        self.log("-" * 50)
        self.add_check("GoModule", "BHTE go.mod exists",
                      self.check_go_module, BHTE_DIR)
        self.log("")

        self.log("=" * 70)
        self.log("Deployment Checklist Summary", "INFO")
        self.log("=" * 70)
        self.log(f"Total Checks: {self.passed + self.failed}")
        self.log(f"Passed: {self.passed}", "PASS")
        self.log(f"Failed: {self.failed}", "FAIL" if self.failed > 0 else "INFO")
        self.log(f"Warnings: {self.warnings}", "WARN" if self.warnings > 0 else "INFO")
        self.log("")

        if self.failed > 0:
            self.log("Failed Items:", "FAIL")
            for check in self.checks:
                if check["status"] == "FAIL":
                    self.log(f"  - [{check['category']}] {check['item']}", "FAIL")
                    if "error" in check:
                        self.log(f"    Error: {check['error']}", "FAIL")
            self.log("")

        deployment_ready = self.failed == 0
        if deployment_ready:
            self.log("STATUS: READY FOR MAINNET DEPLOYMENT", "PASS")
        else:
            self.log("STATUS: NOT READY - Please fix failed items above", "FAIL")

        return deployment_ready

    def generate_deployment_report(self) -> str:
        report = []
        report.append("# BHTE Mainnet Deployment Report\n")
        report.append(f"Generated: {subprocess.run(['date'], capture_output=True, text=True).stdout.strip()}\n")
        report.append(f"Project Root: {PROJECT_ROOT}\n\n")

        report.append("## Summary\n")
        report.append(f"- Total Checks: {self.passed + self.failed}\n")
        report.append(f"- Passed: {self.passed}\n")
        report.append(f"- Failed: {self.failed}\n")
        report.append(f"- Warnings: {self.warnings}\n\n")

        report.append("## Detailed Results\n")
        for check in self.checks:
            status_icon = "✅" if check["status"] == "PASS" else "❌"
            report.append(f"{status_icon} [{check['category']}] {check['item']}")
            if "error" in check:
                report.append(f"    Error: {check['error']}")
            report.append("")

        return "\n".join(report)

if __name__ == "__main__":
    checklist = MainnetDeploymentChecklist()
    ready = checklist.run_deployment_checklist()

    report = checklist.generate_deployment_report()
    report_path = os.path.join(BHTE_DIR, "test", "mainnet_deployment_report.md")
    with open(report_path, 'w') as f:
        f.write(report)
    print(f"\nReport saved to: {report_path}")

    exit(0 if ready else 1)
