#!/usr/bin/env python3
# Copyright (c) 2026 BHC Developers
# Distributed under the MIT software license
# BHC + BHTE 钱包与挖矿模拟测试
# 说明: 当前代码库包含核心算法，需要完整节点实现才能真挖矿

import os
import sys
import time
import json
import hashlib
import secrets
from pathlib import Path
from typing import Optional, Dict, Any

BHTC_EXE = r"d:\work0\BHT\BHC\standalone_test.exe"

class BHCSimulator:
    def __init__(self):
        self.wallets = {}
        self.balances = {}
        self.mining_rewards = 0

    def log(self, message: str, level: str = "INFO"):
        prefix = {
            "INFO": "[INFO]",
            "PASS": "[PASS]",
            "FAIL": "[FAIL]",
            "WARN": "[WARN]"
        }.get(level, "[----]")
        print(f"{prefix} {message}")

    def generate_wallet(self) -> Dict[str, str]:
        self.log("生成新钱包...")

        private_key = secrets.token_hex(32)
        public_key = hashlib.sha256(private_key.encode()).hexdigest()
        address = hashlib.sha256(public_key.encode()).hexdigest()[:40]

        address_checksum = "0x" + address

        self.wallets[address_checksum] = {
            "private_key": private_key,
            "public_key": public_key,
            "created_at": time.time()
        }
        self.balances[address_checksum] = 0

        self.log(f"钱包地址: {address_checksum}", "PASS")
        self.log(f"私钥 (Hex): {private_key[:16]}...", "WARN")

        return {
            "address": address_checksum,
            "private_key": private_key,
            "public_key": public_key
        }

    def get_balance(self, address: str) -> int:
        return self.balances.get(address, 0)

    def simulate_mining(self, address: str, blocks: int = 1) -> Dict[str, Any]:
        self.log(f"模拟挖矿 {blocks} 个区块...")

        base_reward = 6.25

        for i in range(blocks):
            block_reward = base_reward * (0.5 ** (self.mining_rewards // 210000))

            self.balances[address] = self.balances.get(address, 0) + int(block_reward * 1e8)
            self.mining_rewards += 1

            self.log(f"  区块 {self.mining_rewards}: 奖励 {block_reward} BHC", "PASS")

        total_balance = self.get_balance(address)
        self.log(f"总余额: {total_balance / 1e8:.8f} BHC", "PASS")

        return {
            "blocks_mined": blocks,
            "reward_per_block": base_reward,
            "total_reward": blocks * base_reward,
            "new_balance": total_balance
        }

    def transfer(self, from_addr: str, to_addr: str, amount: int) -> bool:
        if self.balances.get(from_addr, 0) < amount:
            self.log(f"余额不足: {self.balances.get(from_addr, 0) / 1e8} < {amount / 1e8}", "FAIL")
            return False

        self.balances[from_addr] -= amount
        self.balances[to_addr] = self.balances.get(to_addr, 0) + amount

        self.log(f"转账成功: {amount / 1e8} BHC", "PASS")
        self.log(f"  From: {from_addr[:20]}...", "INFO")
        self.log(f"  To:   {to_addr[:20]}...", "INFO")

        return True

    def test_standalone_executable(self) -> bool:
        self.log("测试 standalone_test.exe...")

        exe_path = Path(BHTC_EXE)
        if not exe_path.exists():
            self.log(f"未找到: {BHTC_EXE}", "FAIL")
            return False

        try:
            import subprocess
            result = subprocess.run(
                [BHTC_EXE],
                capture_output=True,
                text=True,
                timeout=30,
                cwd=str(exe_path.parent)
            )

            if result.returncode == 0:
                self.log("standalone_test.exe 执行成功", "PASS")

                if "ProgPoW" in result.stdout or "ML-DSA" in result.stdout:
                    self.log("  包含 ProgPoW 和 ML-DSA 算法", "PASS")

                return True
            else:
                self.log(f"执行失败: {result.stderr[:200]}", "FAIL")
                return False

        except subprocess.TimeoutExpired:
            self.log("执行超时", "FAIL")
            return False
        except Exception as e:
            self.log(f"错误: {str(e)}", "FAIL")
            return False

    def run_full_test(self):
        self.log("=" * 60)
        self.log("BHC + BHTE 钱包与挖矿模拟测试")
        self.log("=" * 60)
        self.log("")
        self.log("注意: 当前为模拟测试，完整节点实现后支持真挖矿")
        self.log("")

        self.log("步骤 1: 测试核心算法", "INFO")
        self.log("-" * 40)
        self.test_standalone_executable()
        self.log("")

        self.log("步骤 2: 创建钱包", "INFO")
        self.log("-" * 40)
        wallet = self.generate_wallet()
        wallet_addr = wallet["address"]
        self.log("")

        self.log("步骤 3: 模拟挖矿获得 BHC", "INFO")
        self.log("-" * 40)
        mining_result = self.simulate_mining(wallet_addr, blocks=3)
        self.log("")

        self.log("步骤 4: 模拟转账到另一个钱包", "INFO")
        self.log("-" * 40)
        wallet2 = self.generate_wallet()
        self.transfer(wallet_addr, wallet2["address"], int(1 * 1e8))
        self.log("")

        self.log("步骤 5: 最终余额确认", "INFO")
        self.log("-" * 40)
        self.log(f"钱包1 ({wallet_addr[:20]}...): {self.get_balance(wallet_addr) / 1e8:.8f} BHC", "PASS")
        self.log(f"钱包2 ({wallet2['address'][:20]}...): {self.get_balance(wallet2['address']) / 1e8:.8f} BHC", "PASS")
        self.log("")

        self.log("=" * 60)
        self.log("模拟测试完成!", "PASS")
        self.log("")
        self.log("下一步: 需要完整节点实现才能真挖矿")
        self.log("  1. BHC 节点 - 处理 ProgPoW 挖矿")
        self.log("  2. BHTE 节点 - 处理 L2 交易")
        self.log("  3. RPC 服务 - 暴露钱包 API")
        self.log("=" * 60)

        return True

if __name__ == "__main__":
    simulator = BHCSimulator()
    simulator.run_full_test()
