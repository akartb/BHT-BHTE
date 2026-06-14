#!/usr/bin/env python3
# Copyright (c) 2026 BHC Developers
# Security Audit Script - Automated Security Checks

import os
import re
from pathlib import Path
from typing import List, Tuple, Dict

PROJECT_ROOT = r"d:\work0\BHT"
BHC_DIR = r"d:\work0\BHT\BHC"
BHTE_DIR = r"d:\work0\BHT\BHTE"

class SecurityAuditor:
    def __init__(self):
        self.findings = []
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

    def check_hardcoded_secrets(self, file_path: str) -> List[str]:
        secrets = []
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            patterns = [
                (r'password\s*=\s*["\'][^"\']{8,}["\']', 'hardcoded password'),
                (r'api[_-]?key\s*=\s*["\'][^"\']{16,}["\']', 'hardcoded API key'),
                (r'secret\s*=\s*["\'][^"\']{16,}["\']', 'hardcoded secret'),
                (r'token\s*=\s*["\'][^"\']{16,}["\']', 'hardcoded token'),
            ]

            for pattern, desc in patterns:
                matches = re.findall(pattern, content, re.IGNORECASE)
                if matches:
                    secrets.append(f"{desc}: {len(matches)} occurrences")
        except Exception as e:
            secrets.append(f"Error reading file: {str(e)}")
        return secrets

    def check_buffer_overflow(self, file_path: str) -> List[str]:
        issues = []
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()

            dangerous_functions = [
                'strcpy', 'strcat', 'sprintf', 'gets',
                'scanf', 'fscanf', 'vsprintf'
            ]

            for i, line in enumerate(lines, 1):
                for func in dangerous_functions:
                    if f'{func}(' in line and '//' not in line.split(func)[0]:
                        issues.append(f"Line {i}: potentially unsafe {func}")
                        break
        except Exception as e:
            issues.append(f"Error reading file: {str(e)}")
        return issues

    def check_sql_injection(self, file_path: str) -> List[str]:
        issues = []
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            dangerous_patterns = [
                (r'execute\s*\([^)]*\+', 'string concatenation in SQL execute'),
                (r'query\s*\([^)]*\+', 'string concatenation in SQL query'),
                (r'raw\s*\([^)]*\+', 'raw query with concatenation'),
            ]

            for pattern, desc in dangerous_patterns:
                matches = re.findall(pattern, content, re.IGNORECASE)
                if matches:
                    issues.append(f"{desc}: {len(matches)} occurrences")
        except Exception as e:
            issues.append(f"Error reading file: {str(e)}")
        return issues

    def check_input_validation(self, file_path: str) -> List[str]:
        issues = []
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            if 'recv' in content or 'read' in content:
                validation_patterns = [
                    r'if\s*\(\s*len\s*[<>=]',
                    r'if\s*\(\s*size\s*[<>=]',
                    r'assert\s*\(',
                    r'panic\s*\(',
                ]

                has_validation = any(re.search(p, content) for p in validation_patterns)
                if not has_validation:
                    issues.append("No obvious input validation found")
        except Exception as e:
            issues.append(f"Error reading file: {str(e)}")
        return issues

    def check_crypto_usage(self, file_path: str) -> List[str]:
        issues = []
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            weak_crypto = [
                (r'MD5', 'MD5 is cryptographically weak'),
                (r'SHA1', 'SHA1 is cryptographically weak'),
                (r'RC4', 'RC4 is cryptographically weak'),
            ]

            for pattern, desc in weak_crypto:
                if re.search(pattern, content):
                    issues.append(desc)
        except Exception as e:
            issues.append(f"Error reading file: {str(e)}")
        return issues

    def audit_file(self, file_path: str, category: str):
        self.log(f"Auditing: {file_path}...")

        secrets = self.check_hardcoded_secrets(file_path)
        if secrets:
            for finding in secrets:
                self.log(f"  [WARN] Hardcoded secret: {finding}", "WARN")
                self.warnings += 1

        if file_path.endswith('.cpp') or file_path.endswith('.c'):
            overflow_issues = self.check_buffer_overflow(file_path)
            if overflow_issues:
                for issue in overflow_issues:
                    self.log(f"  [WARN] Buffer safety: {issue}", "WARN")
                    self.warnings += 1

        if file_path.endswith('.py'):
            sql_issues = self.check_sql_injection(file_path)
            if sql_issues:
                for issue in sql_issues:
                    self.log(f"  [WARN] SQL injection risk: {issue}", "WARN")
                    self.warnings += 1

        crypto_issues = self.check_crypto_usage(file_path)
        if crypto_issues:
            for issue in crypto_issues:
                self.log(f"  [FAIL] Weak crypto: {issue}", "FAIL")
                self.failed += 1
                self.findings.append({
                    "file": file_path,
                    "category": category,
                    "severity": "HIGH",
                    "issue": issue
                })
        else:
            self.log(f"  Result: PASS", "PASS")
            self.passed += 1

    def audit_go_file(self, file_path: str, category: str):
        self.log(f"Auditing: {file_path}...")

        secrets = self.check_hardcoded_secrets(file_path)
        if secrets:
            for finding in secrets:
                self.log(f"  [WARN] Hardcoded secret: {finding}", "WARN")
                self.warnings += 1

        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()

            crypto_issues = self.check_crypto_usage(file_path)
            if crypto_issues:
                for issue in crypto_issues:
                    self.log(f"  [FAIL] Weak crypto: {issue}", "FAIL")
                    self.failed += 1
            else:
                self.log(f"  Result: PASS", "PASS")
                self.passed += 1

        except Exception as e:
            self.log(f"  Error: {str(e)}", "FAIL")
            self.failed += 1

    def run_audit(self):
        self.log("=" * 70)
        self.log("BHTE Security Audit")
        self.log("=" * 70)
        self.log("")

        self.log("Category 1: BHC Cryptographic Modules", "INFO")
        self.log("-" * 50)

        crypto_files = [
            (os.path.join(BHC_DIR, "src", "crypto", "progpow.cpp"), "ProgPoW"),
            (os.path.join(BHC_DIR, "src", "crypto", "mldsa.cpp"), "ML-DSA"),
            (os.path.join(BHC_DIR, "src", "script", "hybrid_sig.cpp"), "Hybrid Signature"),
            (os.path.join(BHC_DIR, "src", "pow.cpp"), "Proof of Work"),
            (os.path.join(BHC_DIR, "src", "type_bridge.h"), "Type Bridge"),
        ]

        for file_path, name in crypto_files:
            if os.path.exists(file_path):
                self.audit_file(file_path, f"Crypto/{name}")
            else:
                self.log(f"  [FAIL] File not found: {file_path}", "FAIL")
                self.failed += 1
        self.log("")

        self.log("Category 2: BHC Script Security", "INFO")
        self.log("-" * 50)

        script_files = [
            (os.path.join(BHC_DIR, "src", "script", "withdrawal_script.cpp"), "Withdrawal"),
        ]

        for file_path, name in script_files:
            if os.path.exists(file_path):
                self.audit_file(file_path, f"Script/{name}")
            else:
                self.log(f"  [FAIL] File not found: {file_path}", "FAIL")
                self.failed += 1
        self.log("")

        self.log("Category 3: BHTE Go Modules", "INFO")
        self.log("-" * 50)

        go_files = [
            (os.path.join(BHTE_DIR, "core", "bhc_observer", "bhc_observer.go"), "Observer"),
            (os.path.join(BHTE_DIR, "consensus", "bhc", "anchor.go"), "Anchor"),
            (os.path.join(BHTE_DIR, "consensus", "bhc", "auxpow.go"), "AuxPoW"),
            (os.path.join(BHTE_DIR, "consensus", "bhc", "coordinator.go"), "Coordinator"),
            (os.path.join(BHTE_DIR, "consensus", "bhc", "channel_network.go"), "Channel Network"),
            (os.path.join(BHTE_DIR, "spv", "spv_bridge.go"), "SPV Bridge"),
            (os.path.join(BHTE_DIR, "consensus", "bhc", "state_anchor.go"), "State Anchor"),
        ]

        for file_path, name in go_files:
            if os.path.exists(file_path):
                self.audit_go_file(file_path, f"Go/{name}")
            else:
                self.log(f"  [FAIL] File not found: {file_path}", "FAIL")
                self.failed += 1
        self.log("")

        self.log("Category 4: Python Test Scripts", "INFO")
        self.log("-" * 50)

        py_files = [
            (os.path.join(BHTE_DIR, "test", "observer_integration_test.py"), "Observer Test"),
            (os.path.join(BHTE_DIR, "test", "testnet_deployment_test.py"), "Deployment Test"),
            (os.path.join(BHTE_DIR, "test", "merge_mining_test.py"), "Merge Mining Test"),
        ]

        for file_path, name in py_files:
            if os.path.exists(file_path):
                self.audit_file(file_path, f"Python/{name}")
            else:
                self.log(f"  [FAIL] File not found: {file_path}", "FAIL")
                self.failed += 1
        self.log("")

        self.log("=" * 70)
        self.log("Security Audit Summary", "INFO")
        self.log("=" * 70)
        self.log(f"Total Checks: {self.passed + self.failed}")
        self.log(f"Passed: {self.passed}", "PASS")
        self.log(f"Filed: {self.failed}", "FAIL" if self.failed > 0 else "INFO")
        self.log(f"Warnings: {self.warnings}", "WARN" if self.warnings > 0 else "INFO")
        self.log("")

        if self.findings:
            self.log("High Severity Findings:", "FAIL")
            for finding in self.findings:
                self.log(f"  - [{finding['category']}] {finding['issue']}", "FAIL")
            self.log("")

        if self.failed == 0:
            self.log("STATUS: SECURITY AUDIT PASSED", "PASS")
        else:
            self.log("STATUS: SECURITY ISSUES FOUND - Review findings above", "FAIL")

        return self.failed == 0

if __name__ == "__main__":
    auditor = SecurityAuditor()
    passed = auditor.run_audit()
    exit(0 if passed else 1)
