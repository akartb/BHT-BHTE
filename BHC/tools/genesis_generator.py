#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BHC Genesis Block Generator
Generates the genesis block for BHC network using ProgPoW algorithm

Copyright (c) 2026 BHC Developers
"""

import struct
import time
import hashlib
import json
from typing import Tuple, Optional

try:
    from bhc_crypto import progpow_hash
    HAS_PROGPOW = True
except ImportError:
    HAS_PROGPOW = False
    print("Warning: bhc_crypto module not found, using SHA256 fallback for testing")


class GenesisBlockGenerator:
    """BHC Genesis Block Generator"""
    
    def __init__(self, 
                 timestamp: int,
                 psz_timestamp: str,
                 n_bits: int,
                 n_version: int = 1,
                 block_reward: int = 50 * 100000000):
        self.timestamp = timestamp
        self.psz_timestamp = psz_timestamp
        self.n_bits = n_bits
        self.n_version = n_version
        self.block_reward = block_reward
        
        self.nonce = 0
        self.hash = None
        self.merkle_root = None
        
    def _create_coinbase_tx(self) -> bytes:
        psz_bytes = self.psz_timestamp.encode('utf-8')
        
        version = struct.pack("<I", 1)
        input_count = struct.pack("<B", 1)
        prev_output = b'\x00' * 32
        prev_output_index = struct.pack("<I", 0xFFFFFFFF)
        script_len = struct.pack("<B", len(psz_bytes) + 5)
        script_sig = b'\x04\xff\xff\x00\x1d\x01\x04' + psz_bytes
        sequence = struct.pack("<I", 0xFFFFFFFF)
        output_count = struct.pack("<B", 1)
        reward = struct.pack("<Q", self.block_reward)
        pubkey_script = bytes([
            0x41,
            0x04, 0x67, 0x8a, 0xfd, 0xa0, 0x1c, 0x26, 0x8b,
            0x34, 0x5c, 0x9f, 0x8c, 0x5e, 0x6d, 0x7c, 0x9e,
            0x4a, 0x5c, 0x8b, 0x6d, 0x9e, 0x4a, 0x5c, 0x8b,
            0x34, 0x5c, 0x9f, 0x8c, 0x5e, 0x6d, 0x7c, 0x9e,
            0x4a, 0x5c, 0x8b, 0x6d, 0x9e, 0x4a, 0x5c, 0x8b,
            0x34, 0x5c, 0x9f, 0x8c, 0x5e, 0x6d, 0x7c, 0x9e,
            0x4a, 0x5c, 0x8b, 0x6d, 0x9e, 0x4a, 0x5c, 0x8b,
            0xac
        ])
        pubkey_script_len = struct.pack("<B", len(pubkey_script))
        locktime = struct.pack("<I", 0)
        
        tx = (version + input_count + prev_output + prev_output_index + 
              script_len + script_sig + sequence + output_count + 
              reward + pubkey_script_len + pubkey_script + locktime)
        
        return tx
    
    def _calculate_merkle_root(self, tx: bytes) -> bytes:
        tx_hash = hashlib.sha256(hashlib.sha256(tx).digest()).digest()
        return tx_hash
    
    def _create_block_header(self, merkle_root: bytes, nonce: int) -> bytes:
        prev_block = b'\x00' * 32
        
        header = struct.pack(
            "<I32s32sIII",
            self.n_version,
            prev_block,
            merkle_root,
            self.timestamp,
            self.n_bits,
            nonce
        )
        
        return header
    
    def _hash_block(self, header: bytes) -> bytes:
        if HAS_PROGPOW:
            return progpow_hash(header, 0)
        else:
            return hashlib.sha256(hashlib.sha256(header).digest()).digest()
    
    def _check_pow(self, block_hash: bytes) -> bool:
        target = self._bits_to_target(self.n_bits)
        hash_int = int.from_bytes(block_hash, 'little')
        return hash_int < target
    
    def _bits_to_target(self, n_bits: int) -> int:
        exponent = n_bits >> 24
        mantissa = n_bits & 0x007fffff
        
        if exponent <= 3:
            target = mantissa >> (8 * (3 - exponent))
        else:
            target = mantissa << (8 * (exponent - 3))
        
        return target
    
    def mine(self, max_nonce: int = 100000000, progress_interval: int = 10000) -> Tuple[int, bytes, bytes]:
        coinbase_tx = self._create_coinbase_tx()
        self.merkle_root = self._calculate_merkle_root(coinbase_tx)
        
        print(f"Starting genesis block mining...")
        print(f"Timestamp: {self.timestamp}")
        print(f"Message: {self.psz_timestamp}")
        print(f"Difficulty bits: 0x{self.n_bits:08x}")
        print(f"Merkle root: {self.merkle_root.hex()}")
        print()
        
        start_time = time.time()
        
        for nonce in range(max_nonce):
            header = self._create_block_header(self.merkle_root, nonce)
            block_hash = self._hash_block(header)
            
            if self._check_pow(block_hash):
                self.nonce = nonce
                self.hash = block_hash
                
                elapsed = time.time() - start_time
                print(f"\n{'='*60}")
                print(f"GENESIS BLOCK FOUND!")
                print(f"{'='*60}")
                print(f"Nonce: {nonce}")
                print(f"Hash: {block_hash.hex()}")
                print(f"Merkle Root: {self.merkle_root.hex()}")
                print(f"Time elapsed: {elapsed:.2f} seconds")
                print(f"Hashes per second: {nonce/elapsed:.2f}")
                print(f"{'='*60}")
                
                return nonce, block_hash, self.merkle_root
            
            if nonce > 0 and nonce % progress_interval == 0:
                elapsed = time.time() - start_time
                print(f"Tried {nonce:,} nonces... ({nonce/elapsed:.0f} H/s)", end='\r')
        
        print(f"\nFailed to find genesis block within {max_nonce:,} attempts")
        return -1, b'', b''
    
    def generate_code(self, output_file: Optional[str] = None) -> str:
        if self.hash is None:
            return "Error: Genesis block not yet mined"
        
        code = f'''// BHC Genesis Block Parameters
// Auto-generated by genesis_generator.py

// Genesis Block Hash
consensus.hashGenesisBlock = uint256S("{self.hash.hex()}");

// Genesis Block Nonce
genesis.nNonce = {self.nonce};

// Genesis Block Timestamp
genesis.nTime = {self.timestamp};

// Genesis Block Merkle Root
genesis.hashMerkleRoot = uint256S("{self.merkle_root.hex()}");

// Genesis Block Message
const char* pszTimestamp = "{self.psz_timestamp}";
'''
        
        if output_file:
            with open(output_file, 'w') as f:
                f.write(code)
            print(f"Code written to {output_file}")
        
        return code
    
    def to_json(self) -> dict:
        return {
            "hash": self.hash.hex() if self.hash else None,
            "nonce": self.nonce,
            "timestamp": self.timestamp,
            "message": self.psz_timestamp,
            "merkle_root": self.merkle_root.hex() if self.merkle_root else None,
            "bits": self.n_bits,
            "version": self.n_version,
            "reward": self.block_reward
        }


def main():
    print("="*60)
    print("BHC Genesis Block Generator")
    print("="*60)
    print()
    
    timestamp = int(time.time())
    message = "BHC: The Future of Digital Gold 2026 - Decentralized & Post-Quantum"
    n_bits = 0x1e0ffff0
    
    generator = GenesisBlockGenerator(
        timestamp=timestamp,
        psz_timestamp=message,
        n_bits=n_bits
    )
    
    nonce, block_hash, merkle_root = generator.mine()
    
    if nonce >= 0:
        print("\nGenerated C++ Code:")
        print("-"*60)
        print(generator.generate_code())
        print("-"*60)
        
        print("\nJSON Export:")
        print(json.dumps(generator.to_json(), indent=2))


if __name__ == "__main__":
    main()
