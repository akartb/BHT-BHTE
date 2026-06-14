// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license

package test

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"testing"

	"bhte/mldsa"
)

// TestCompleteMLDSAFlow tests the complete ML-DSA flow from key generation to verification
func TestCompleteMLDSAFlow(t *testing.T) {
	fmt.Println("=== Testing Complete ML-DSA Flow ===")

	// 1. Key Generation
	fmt.Println("\n1. Testing Key Generation...")
	m := mldsa.New(mldsa.MLDSA65)
	pk, sk, err := m.GenerateKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate key pair: %v", err)
	}
	fmt.Println("✓ Key generation successful")

	// 2. Message Preparation
	fmt.Println("\n2. Preparing Test Message...")
	message := make([]byte, mldsa.MLDSA44HashSize)
	rand.Read(message)
	fmt.Printf("✓ Message prepared: %s...\n", hex.EncodeToString(message[:16]))

	// 3. Signing
	fmt.Println("\n3. Testing Signing...")
	signature, err := m.Sign(sk, message)
	if err != nil {
		t.Fatalf("Failed to sign message: %v", err)
	}
	fmt.Printf("✓ Signature generated (size: %d bytes)\n", len(signature))

	// 4. Verification
	fmt.Println("\n4. Testing Verification...")
	err = m.Verify(pk, message, signature)
	if err != nil {
		t.Fatalf("Failed to verify signature: %v", err)
	}
	fmt.Println("✓ Signature verification successful")

	// 5. Key Serialization
	fmt.Println("\n5. Testing Key Serialization...")
	pkBytes := m.PublicKeyToBytes(pk)
	skBytes := m.SecretKeyToBytes(sk)
	fmt.Printf("✓ Public key serialized (size: %d bytes)\n", len(pkBytes))
	fmt.Printf("✓ Secret key serialized (size: %d bytes)\n", len(skBytes))

	// 6. Key Deserialization
	fmt.Println("\n6. Testing Key Deserialization...")
	pk2, err := m.PublicKeyFromBytes(pkBytes)
	if err != nil {
		t.Fatalf("Failed to deserialize public key: %v", err)
	}
	sk2, err := m.SecretKeyFromBytes(skBytes)
	if err != nil {
		t.Fatalf("Failed to deserialize secret key: %v", err)
	}
	fmt.Println("✓ Key deserialization successful")

	// 7. Re-sign and verify with deserialized keys
	fmt.Println("\n7. Testing with Deserialized Keys...")
	signature2, err := m.Sign(sk2, message)
	if err != nil {
		t.Fatalf("Failed to sign with deserialized key: %v", err)
	}
	err = m.Verify(pk2, message, signature2)
	if err != nil {
		t.Fatalf("Failed to verify with deserialized key: %v", err)
	}
	fmt.Println("✓ Deserialized keys work correctly")

	// 8. Generate commitment (for smart contract)
	fmt.Println("\n8. Generating Commitment...")
	commitment := generateCommitment(message, signature, pkBytes)
	fmt.Printf("✓ Commitment generated: %s\n", hex.EncodeToString(commitment[:]))

	fmt.Println("\n=== Complete ML-DSA Flow Test PASSED ===")
}

// TestCommitmentMechanism tests the commitment mechanism
func TestCommitmentMechanism(t *testing.T) {
	fmt.Println("\n=== Testing Commitment Mechanism ===")

	m := mldsa.New(mldsa.MLDSA65)
	pk, sk, err := m.GenerateKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate key pair: %v", err)
	}

	message := make([]byte, mldsa.MLDSA44HashSize)
	rand.Read(message)
	signature, err := m.Sign(sk, message)
	if err != nil {
		t.Fatalf("Failed to sign message: %v", err)
	}

	pkBytes := m.PublicKeyToBytes(pk)

	// Test 1: Generate valid commitment
	fmt.Println("\n1. Testing Valid Commitment...")
	commitment1 := generateCommitment(message, signature, pkBytes)
	fmt.Printf("✓ Commitment 1: %s\n", hex.EncodeToString(commitment1[:]))

	// Test 2: Same inputs should generate same commitment
	fmt.Println("\n2. Testing Commitment Determinism...")
	commitment2 := generateCommitment(message, signature, pkBytes)
	if commitment1 != commitment2 {
		t.Fatal("Commitments should be identical for same inputs")
	}
	fmt.Println("✓ Commitment is deterministic")

	// Test 3: Different message should generate different commitment
	fmt.Println("\n3. Testing Different Message...")
	message2 := make([]byte, mldsa.MLDSA44HashSize)
	rand.Read(message2)
	commitment3 := generateCommitment(message2, signature, pkBytes)
	if commitment1 == commitment3 {
		t.Fatal("Commitments should differ for different messages")
	}
	fmt.Println("✓ Different message generates different commitment")

	// Test 4: Different signature should generate different commitment
	fmt.Println("\n4. Testing Different Signature...")
	signature2 := make([]byte, len(signature))
	copy(signature2, signature)
	signature2[0] ^= 0xFF
	commitment4 := generateCommitment(message, signature2, pkBytes)
	if commitment1 == commitment4 {
		t.Fatal("Commitments should differ for different signatures")
	}
	fmt.Println("✓ Different signature generates different commitment")

	// Test 5: Different public key should generate different commitment
	fmt.Println("\n5. Testing Different Public Key...")
	pk2, _, err := m.GenerateKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate second key pair: %v", err)
	}
	pkBytes2 := m.PublicKeyToBytes(pk2)
	commitment5 := generateCommitment(message, signature, pkBytes2)
	if commitment1 == commitment5 {
		t.Fatal("Commitments should differ for different public keys")
	}
	fmt.Println("✓ Different public key generates different commitment")

	fmt.Println("\n=== Commitment Mechanism Test PASSED ===")
}

// TestBatchOperations tests batch commitment operations
func TestBatchOperations(t *testing.T) {
	fmt.Println("\n=== Testing Batch Operations ===")

	m := mldsa.New(mldsa.MLDSA65)

	// Generate multiple test cases
	batchSize := 5
	fmt.Printf("\n1. Generating %d test commitments...\n", batchSize)

	type TestCase struct {
		message    []byte
		signature  []byte
		pubkey     []byte
		commitment [32]byte
	}

	testCases := make([]TestCase, batchSize)

	for i := 0; i < batchSize; i++ {
		pk, sk, err := m.GenerateKeyPair()
		if err != nil {
			t.Fatalf("Failed to generate key pair %d: %v", i, err)
		}

		message := make([]byte, mldsa.MLDSA44HashSize)
		rand.Read(message)
		signature, err := m.Sign(sk, message)
		if err != nil {
			t.Fatalf("Failed to sign message %d: %v", i, err)
		}

		pkBytes := m.PublicKeyToBytes(pk)
		commitment := generateCommitment(message, signature, pkBytes)

		testCases[i] = TestCase{
			message:    message,
			signature:  signature,
			pubkey:     pkBytes,
			commitment: commitment,
		}

		fmt.Printf("  ✓ Test case %d generated\n", i+1)
	}

	// Verify all commitments are unique
	fmt.Println("\n2. Verifying all commitments are unique...")
	commitmentSet := make(map[[32]byte]bool)
	for i, tc := range testCases {
		if commitmentSet[tc.commitment] {
			t.Fatalf("Duplicate commitment found for test case %d", i)
		}
		commitmentSet[tc.commitment] = true
	}
	fmt.Println("✓ All commitments are unique")

	// Verify each commitment can be regenerated
	fmt.Println("\n3. Verifying commitment regeneration...")
	for i, tc := range testCases {
		regenerated := generateCommitment(tc.message, tc.signature, tc.pubkey)
		if regenerated != tc.commitment {
			t.Fatalf("Commitment regeneration failed for test case %d", i)
		}
	}
	fmt.Println("✓ All commitments can be regenerated correctly")

	fmt.Println("\n=== Batch Operations Test PASSED ===")
}

// TestSecurityProperties tests various security properties
func TestSecurityProperties(t *testing.T) {
	fmt.Println("\n=== Testing Security Properties ===")

	m := mldsa.New(mldsa.MLDSA65)
	pk, sk, err := m.GenerateKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate key pair: %v", err)
	}

	message := make([]byte, mldsa.MLDSA44HashSize)
	rand.Read(message)
	signature, err := m.Sign(sk, message)
	if err != nil {
		t.Fatalf("Failed to sign message: %v", err)
	}

	pkBytes := m.PublicKeyToBytes(pk)

	// Test 1: Tampered signature should fail verification
	fmt.Println("\n1. Testing Tampered Signature...")
	tamperedSig := make([]byte, len(signature))
	copy(tamperedSig, signature)
	tamperedSig[100] ^= 0xFF
	err = m.Verify(pk, message, tamperedSig)
	if err == nil {
		t.Fatal("Verification should fail with tampered signature")
	}
	fmt.Println("✓ Tampered signature rejected")

	// Test 2: Wrong message should fail verification
	fmt.Println("\n2. Testing Wrong Message...")
	wrongMessage := make([]byte, mldsa.MLDSA44HashSize)
	rand.Read(wrongMessage)
	err = m.Verify(pk, wrongMessage, signature)
	if err == nil {
		t.Fatal("Verification should fail with wrong message")
	}
	fmt.Println("✓ Wrong message rejected")

	// Test 3: Commitment changes with any input change
	fmt.Println("\n3. Testing Commitment Sensitivity...")
	originalCommitment := generateCommitment(message, signature, pkBytes)

	// Change message
	modifiedMessage := make([]byte, len(message))
	copy(modifiedMessage, message)
	modifiedMessage[0] ^= 0xFF
	commitment1 := generateCommitment(modifiedMessage, signature, pkBytes)
	if commitment1 == originalCommitment {
		t.Fatal("Commitment should change with modified message")
	}

	// Change signature
	modifiedSig := make([]byte, len(signature))
	copy(modifiedSig, signature)
	modifiedSig[0] ^= 0xFF
	commitment2 := generateCommitment(message, modifiedSig, pkBytes)
	if commitment2 == originalCommitment {
		t.Fatal("Commitment should change with modified signature")
	}

	// Change pubkey
	pk2, _, err := m.GenerateKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate second key pair: %v", err)
	}
	pkBytes2 := m.PublicKeyToBytes(pk2)
	commitment3 := generateCommitment(message, signature, pkBytes2)
	if commitment3 == originalCommitment {
		t.Fatal("Commitment should change with modified public key")
	}

	fmt.Println("✓ Commitment is sensitive to all input changes")

	fmt.Println("\n=== Security Properties Test PASSED ===")
}

// generateCommitment generates a commitment hash similar to the smart contract
func generateCommitment(message []byte, signature []byte, pubkey []byte) [32]byte {
	// Simple keccak256-like hash (for testing purposes)
	// In real smart contract, this would use solidity's keccak256
	var result [32]byte

	// Simple XOR-based hash for testing
	for i := 0; i < 32; i++ {
		result[i] = 0
	}

	// Mix message
	for i, b := range message {
		result[i%32] ^= b
	}

	// Mix signature
	for i, b := range signature {
		result[(i+7)%32] ^= b
	}

	// Mix pubkey
	for i, b := range pubkey {
		result[(i+13)%32] ^= b
	}

	return result
}

// MainTest runs all integration tests
func TestMLDSAIntegration(t *testing.T) {
	fmt.Println("=======================================")
	fmt.Println("  ML-DSA Integration Test Suite")
	fmt.Println("=======================================")

	t.Run("CompleteFlow", TestCompleteMLDSAFlow)
	t.Run("CommitmentMechanism", TestCommitmentMechanism)
	t.Run("BatchOperations", TestBatchOperations)
	t.Run("SecurityProperties", TestSecurityProperties)

	fmt.Println("\n=======================================")
	fmt.Println("  All Integration Tests PASSED!")
	fmt.Println("=======================================")
}
