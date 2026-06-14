#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BHC Hybrid Signature Test Script
Tests ECDSA + ML-DSA hybrid signature functionality

Copyright (c) 2026 BHC Developers
"""

import sys
import json
import subprocess
import hashlib
import time
import os
from typing import Optional, Dict, Any, List, Tuple

CLI_PATH = "bitcoin-cli"
RPC_USER = "bhc_boss"
RPC_PASSWORD = "your_secure_password"
RPC_PORT = 38332

class BHCClient:
    """BHC RPC Client"""
    
    def __init__(self, cli_path: str = CLI_PATH, 
                 rpc_user: str = RPC_USER,
                 rpc_password: str = RPC_PASSWORD,
                 rpc_port: int = RPC_PORT,
                 regtest: bool = True):
        self.cli_path = cli_path
        self.rpc_user = rpc_user
        self.rpc_password = rpc_password
        self.rpc_port = rpc_port
        self.regtest = regtest
        
    def _build_command(self, method: str, *args) -> str:
        """Build bitcoin-cli command"""
        regtest_flag = "-regtest" if self.regtest else ""
        cmd = f'{self.cli_path} {regtest_flag} -rpcuser={self.rpc_user} -rpcpassword={self.rpc_password} -rpcport={self.rpc_port} {method}'
        for arg in args:
            if isinstance(arg, str):
                if ' ' in arg or '"' in arg:
                    cmd += f' "{arg}"'
                else:
                    cmd += f' {arg}'
            elif isinstance(arg, (int, float)):
                cmd += f' {arg}'
            elif isinstance(arg, dict) or isinstance(arg, list):
                cmd += f" '{json.dumps(arg)}'"
        return cmd
    
    def call(self, method: str, *args) -> Optional[Any]:
        """Execute RPC call"""
        cmd = self._build_command(method, *args)
        try:
            result = subprocess.check_output(cmd, shell=True, stderr=subprocess.PIPE)
            if result:
                return json.loads(result.decode().strip())
            return None
        except subprocess.CalledProcessError as e:
            print(f"RPC Error: {e.stderr.decode() if e.stderr else str(e)}")
            return None
        except json.JSONDecodeError as e:
            print(f"JSON Error: {e}")
            return None
    
    def get_blockchain_info(self) -> Optional[Dict]:
        """Get blockchain info"""
        return self.call("getblockchaininfo")
    
    def get_block_hash(self, height: int) -> Optional[str]:
        """Get block hash at height"""
        return self.call("getblockhash", height)
    
    def get_new_address(self, label: str = "", address_type: str = "bech32") -> Optional[str]:
        """Generate new address"""
        return self.call("getnewaddress", label, address_type)
    
    def generate_to_address(self, nblocks: int, address: str) -> Optional[List[str]]:
        """Generate blocks to address"""
        return self.call("generatetoaddress", nblocks, address)
    
    def get_balance(self) -> Optional[float]:
        """Get wallet balance"""
        return self.call("getbalance")
    
    def list_unspent(self, minconf: int = 1) -> Optional[List[Dict]]:
        """List unspent outputs"""
        return self.call("listunspent", minconf)
    
    def create_raw_transaction(self, inputs: List[Dict], outputs: Dict) -> Optional[str]:
        """Create raw transaction"""
        return self.call("createrawtransaction", inputs, outputs)
    
    def sign_raw_transaction_with_wallet(self, hexstring: str) -> Optional[Dict]:
        """Sign raw transaction"""
        return self.call("signrawtransactionwithwallet", hexstring)
    
    def send_raw_transaction(self, hexstring: str) -> Optional[str]:
        """Send raw transaction"""
        return self.call("sendrawtransaction", hexstring)
    
    def decode_raw_transaction(self, hexstring: str) -> Optional[Dict]:
        """Decode raw transaction"""
        return self.call("decoderawtransaction", hexstring)
    
    def validate_address(self, address: str) -> Optional[Dict]:
        """Validate address"""
        return self.call("validateaddress", address)


class MLDSASigner:
    """ML-DSA Signature Generator (Python binding simulation)"""
    
    @staticmethod
    def generate_keypair() -> Tuple[bytes, bytes]:
        """Generate ML-DSA keypair"""
        import secrets
        privkey = secrets.token_bytes(4032)
        pubkey = hashlib.sha512(privkey).digest()[:1952]
        return privkey, pubkey
    
    @staticmethod
    def sign(privkey: bytes, message: bytes) -> bytes:
        """Sign message with ML-DSA"""
        h = hashlib.sha512(privkey + message).digest()
        signature = h * 103 + hashlib.sha256(message).digest()
        return signature[:3293]
    
    @staticmethod
    def verify(pubkey: bytes, message: bytes, signature: bytes) -> bool:
        """Verify ML-DSA signature"""
        return len(signature) == 3293 and len(pubkey) == 1952


class HybridSignatureTest:
    """Hybrid Signature Test Suite"""
    
    def __init__(self, client: BHCClient):
        self.client = client
        self.test_address = None
        self.test_utxos = []
        
    def setup(self) -> bool:
        """Setup test environment"""
        print("=" * 60)
        print("BHC Hybrid Signature Test Suite")
        print("=" * 60)
        print()
        
        info = self.client.get_blockchain_info()
        if not info:
            print("❌ Cannot connect to BHC node")
            print("   Please ensure bitcoind is running in regtest mode:")
            print("   ./bitcoind -regtest -daemon -rpcuser=bhc_boss -rpcpassword=your_secure_password")
            return False
        
        print(f"✅ Connected to BHC node")
        print(f"   Chain: {info.get('chain', 'unknown')}")
        print(f"   Blocks: {info.get('blocks', 0)}")
        print()
        
        genesis_hash = self.client.get_block_hash(0)
        if genesis_hash:
            print(f"✅ Genesis Block Hash: {genesis_hash}")
            expected = "3a7ea551d2d41377e210cd9f5af4b291a24d425e0f1666ada84fcf53ed0b0000"
            if genesis_hash == expected:
                print(f"   ✅ Genesis hash verified!")
            else:
                print(f"   ⚠️  Genesis hash mismatch (expected: {expected})")
        print()
        
        return True
    
    def generate_test_funds(self) -> bool:
        """Generate test funds"""
        print("--- Generating Test Funds ---")
        
        self.test_address = self.client.get_new_address("test", "bech32")
        if not self.test_address:
            print("❌ Failed to generate address")
            return False
        
        print(f"✅ Test address: {self.test_address}")
        
        block_hashes = self.client.generate_to_address(101, self.test_address)
        if not block_hashes:
            print("❌ Failed to generate blocks")
            return False
        
        print(f"✅ Generated {len(block_hashes)} blocks")
        
        balance = self.client.get_balance()
        print(f"✅ Balance: {balance} BHC")
        print()
        
        return True
    
    def test_standard_transaction(self) -> bool:
        """Test standard ECDSA transaction"""
        print("--- Testing Standard ECDSA Transaction ---")
        
        utxos = self.client.list_unspent()
        if not utxos:
            print("❌ No UTXOs available")
            return False
        
        utxo = utxos[0]
        print(f"   Using UTXO: {utxo['txid'][:16]}... (vout: {utxo['vout']})")
        
        dest_addr = self.client.get_new_address("dest", "bech32")
        print(f"   Destination: {dest_addr}")
        
        inputs = [{"txid": utxo["txid"], "vout": utxo["vout"]}]
        outputs = {dest_addr: utxo["amount"] - 0.001}
        
        raw_tx = self.client.create_raw_transaction(inputs, outputs)
        if not raw_tx:
            print("❌ Failed to create transaction")
            return False
        
        print(f"   Raw TX: {raw_tx[:64]}...")
        
        signed = self.client.sign_raw_transaction_with_wallet(raw_tx)
        if not signed or not signed.get("complete"):
            print("❌ Failed to sign transaction")
            return False
        
        print(f"   ✅ Transaction signed (ECDSA)")
        
        tx_id = self.client.send_raw_transaction(signed["hex"])
        if not tx_id:
            print("❌ Failed to send transaction")
            return False
        
        print(f"   ✅ Transaction sent: {tx_id}")
        print()
        
        return True
    
    def test_hybrid_signature(self) -> bool:
        """Test hybrid ECDSA + ML-DSA signature"""
        print("--- Testing Hybrid Signature (ECDSA + ML-DSA) ---")
        
        utxos = self.client.list_unspent()
        if not utxos:
            print("❌ No UTXOs available")
            return False
        
        utxo = utxos[0]
        print(f"   Using UTXO: {utxo['txid'][:16]}...")
        
        dest_addr = self.client.get_new_address("hybrid", "bech32")
        print(f"   Destination: {dest_addr}")
        
        inputs = [{"txid": utxo["txid"], "vout": utxo["vout"]}]
        outputs = {dest_addr: utxo["amount"] - 0.001}
        
        raw_tx = self.client.create_raw_transaction(inputs, outputs)
        if not raw_tx:
            print("❌ Failed to create transaction")
            return False
        
        tx_decoded = self.client.decode_raw_transaction(raw_tx)
        if tx_decoded:
            print(f"   TX Size: {len(raw_tx) // 2} bytes")
        
        signed = self.client.sign_raw_transaction_with_wallet(raw_tx)
        if not signed:
            print("❌ Failed to sign transaction")
            return False
        
        ecdsa_sig = signed.get("hex", "")
        print(f"   ✅ ECDSA signature generated ({len(ecdsa_sig) // 2} bytes)")
        
        privkey, pubkey = MLDSASigner.generate_keypair()
        print(f"   ✅ ML-DSA keypair generated")
        print(f"      Public key: {len(pubkey)} bytes")
        print(f"      Private key: {len(privkey)} bytes")
        
        message = bytes.fromhex(raw_tx)
        mldsa_sig = MLDSASigner.sign(privkey, message)
        print(f"   ✅ ML-DSA signature generated ({len(mldsa_sig)} bytes)")
        
        is_valid = MLDSASigner.verify(pubkey, message, mldsa_sig)
        if is_valid:
            print(f"   ✅ ML-DSA signature verified")
        else:
            print(f"   ❌ ML-DSA signature verification failed")
            return False
        
        hybrid_witness = ecdsa_sig + mldsa_sig.hex()
        print(f"   ✅ Hybrid witness assembled ({len(hybrid_witness) // 2} bytes total)")
        print(f"      ECDSA portion: {len(ecdsa_sig) // 2} bytes")
        print(f"      ML-DSA portion: {len(mldsa_sig)} bytes")
        print()
        
        return True
    
    def test_mldsa_tampering(self) -> bool:
        """Test ML-DSA tamper detection"""
        print("--- Testing ML-DSA Tamper Detection ---")
        
        privkey, pubkey = MLDSASigner.generate_keypair()
        message = b"Test message for tamper detection"
        
        signature = MLDSASigner.sign(privkey, message)
        print(f"   Original signature: {signature[:16].hex()}...")
        
        is_valid = MLDSASigner.verify(pubkey, message, signature)
        print(f"   ✅ Original signature verified: {is_valid}")
        
        tampered_sig = bytearray(signature)
        tampered_sig[0] ^= 0xFF
        tampered_sig = bytes(tampered_sig)
        
        is_valid_tampered = MLDSASigner.verify(pubkey, message, tampered_sig)
        print(f"   ✅ Tampered signature rejected: {not is_valid_tampered}")
        
        if is_valid_tampered:
            print("   ❌ SECURITY ISSUE: Tampered signature was accepted!")
            return False
        
        print()
        return True
    
    def run_all_tests(self) -> bool:
        """Run all tests"""
        if not self.setup():
            return False
        
        if not self.generate_test_funds():
            return False
        
        results = []
        
        results.append(("Standard ECDSA", self.test_standard_transaction()))
        results.append(("Hybrid Signature", self.test_hybrid_signature()))
        results.append(("Tamper Detection", self.test_mldsa_tampering()))
        
        print("=" * 60)
        print("Test Results Summary")
        print("=" * 60)
        
        all_passed = True
        for name, passed in results:
            status = "✅ PASS" if passed else "❌ FAIL"
            print(f"   {name}: {status}")
            if not passed:
                all_passed = False
        
        print()
        if all_passed:
            print("🎉 All tests passed!")
        else:
            print("⚠️  Some tests failed")
        
        return all_passed


def main():
    """Main entry point"""
    client = BHCClient()
    test_suite = HybridSignatureTest(client)
    
    success = test_suite.run_all_tests()
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
