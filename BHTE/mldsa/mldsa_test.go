// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license

package mldsa

import (
	"crypto/rand"
	"testing"
)

// TestMLDSA65KeyGeneration tests ML-DSA-65 key generation
func TestMLDSA65KeyGeneration(t *testing.T) {
	m := New(MLDSA65)
	
	pk, sk, err := m.GenerateKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate ML-DSA-65 key pair: %v", err)
	}
	
	if pk == nil {
		t.Fatal("Public key is nil")
	}
	
	if sk == nil {
		t.Fatal("Secret key is nil")
	}
	
	t.Log("ML-DSA-65 key generation successful")
}

// TestMLDSA65SignVerify tests ML-DSA-65 signing and verification
func TestMLDSA65SignVerify(t *testing.T) {
	m := New(MLDSA65)
	
	pk, sk, err := m.GenerateKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate key pair: %v", err)
	}
	
	message := make([]byte, MLDSA44HashSize)
	rand.Read(message)
	
	signature, err := m.Sign(sk, message)
	if err != nil {
		t.Fatalf("Failed to sign message: %v", err)
	}
	
	expectedSigSize := MLDSA65SignatureSize
	if len(signature) != expectedSigSize {
		t.Errorf("Signature size mismatch: got %d, want %d", len(signature), expectedSigSize)
	}
	
	err = m.Verify(pk, message, signature)
	if err != nil {
		t.Fatalf("Failed to verify signature: %v", err)
	}
	
	t.Log("ML-DSA-65 sign and verify successful")
}

// TestMLDSA65InvalidSignature tests verification with invalid signature
func TestMLDSA65InvalidSignature(t *testing.T) {
	m := New(MLDSA65)
	
	pk, sk, err := m.GenerateKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate key pair: %v", err)
	}
	
	message := make([]byte, MLDSA44HashSize)
	rand.Read(message)
	
	signature, err := m.Sign(sk, message)
	if err != nil {
		t.Fatalf("Failed to sign message: %v", err)
	}
	
	invalidSignature := make([]byte, len(signature))
	copy(invalidSignature, signature)
	invalidSignature[0] ^= 0xFF
	
	err = m.Verify(pk, message, invalidSignature)
	if err == nil {
		t.Fatal("Verification should fail with invalid signature")
	}
	
	t.Log("ML-DSA-65 invalid signature test passed")
}

// TestMLDSA65WrongMessage tests verification with wrong message
func TestMLDSA65WrongMessage(t *testing.T) {
	m := New(MLDSA65)
	
	pk, sk, err := m.GenerateKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate key pair: %v", err)
	}
	
	message1 := make([]byte, MLDSA44HashSize)
	rand.Read(message1)
	
	message2 := make([]byte, MLDSA44HashSize)
	rand.Read(message2)
	
	signature, err := m.Sign(sk, message1)
	if err != nil {
		t.Fatalf("Failed to sign message: %v", err)
	}
	
	err = m.Verify(pk, message2, signature)
	if err == nil {
		t.Fatal("Verification should fail with wrong message")
	}
	
	t.Log("ML-DSA-65 wrong message test passed")
}

// TestMLDSA65KeySerialization tests key serialization and deserialization
func TestMLDSA65KeySerialization(t *testing.T) {
	m := New(MLDSA65)
	
	pk, sk, err := m.GenerateKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate key pair: %v", err)
	}
	
	pkBytes := m.PublicKeyToBytes(pk)
	expectedPkSize := MLDSA65PublicKeySize
	if len(pkBytes) != expectedPkSize {
		t.Errorf("Public key size mismatch: got %d, want %d", len(pkBytes), expectedPkSize)
	}
	
	skBytes := m.SecretKeyToBytes(sk)
	expectedSkSize := MLDSA65SecretKeySize
	if len(skBytes) != expectedSkSize {
		t.Errorf("Secret key size mismatch: got %d, want %d", len(skBytes), expectedSkSize)
	}
	
	pk2, err := m.PublicKeyFromBytes(pkBytes)
	if err != nil {
		t.Fatalf("Failed to deserialize public key: %v", err)
	}
	
	sk2, err := m.SecretKeyFromBytes(skBytes)
	if err != nil {
		t.Fatalf("Failed to deserialize secret key: %v", err)
	}
	
	message := make([]byte, MLDSA44HashSize)
	rand.Read(message)
	
	signature, err := m.Sign(sk2, message)
	if err != nil {
		t.Fatalf("Failed to sign with deserialized key: %v", err)
	}
	
	err = m.Verify(pk2, message, signature)
	if err != nil {
		t.Fatalf("Failed to verify with deserialized key: %v", err)
	}
	
	t.Log("ML-DSA-65 key serialization test passed")
}

// TestMLDSA87KeyGeneration tests ML-DSA-87 key generation
func TestMLDSA87KeyGeneration(t *testing.T) {
	m := New(MLDSA87)
	
	pk, sk, err := m.GenerateKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate ML-DSA-87 key pair: %v", err)
	}
	
	if pk == nil {
		t.Fatal("Public key is nil")
	}
	
	if sk == nil {
		t.Fatal("Secret key is nil")
	}
	
	t.Log("ML-DSA-87 key generation successful")
}

// TestMLDSA87SignVerify tests ML-DSA-87 signing and verification
func TestMLDSA87SignVerify(t *testing.T) {
	m := New(MLDSA87)
	
	pk, sk, err := m.GenerateKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate key pair: %v", err)
	}
	
	message := make([]byte, MLDSA44HashSize)
	rand.Read(message)
	
	signature, err := m.Sign(sk, message)
	if err != nil {
		t.Fatalf("Failed to sign message: %v", err)
	}
	
	expectedSigSize := MLDSA87SignatureSize
	if len(signature) != expectedSigSize {
		t.Errorf("Signature size mismatch: got %d, want %d", len(signature), expectedSigSize)
	}
	
	err = m.Verify(pk, message, signature)
	if err != nil {
		t.Fatalf("Failed to verify signature: %v", err)
	}
	
	t.Log("ML-DSA-87 sign and verify successful")
}

// TestMLDSA44KeyGeneration tests ML-DSA-44 key generation
func TestMLDSA44KeyGeneration(t *testing.T) {
	m := New(MLDSA44)
	
	pk, sk, err := m.GenerateKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate ML-DSA-44 key pair: %v", err)
	}
	
	if pk == nil {
		t.Fatal("Public key is nil")
	}
	
	if sk == nil {
		t.Fatal("Secret key is nil")
	}
	
	t.Log("ML-DSA-44 key generation successful")
}

// TestMLDSA44SignVerify tests ML-DSA-44 signing and verification
func TestMLDSA44SignVerify(t *testing.T) {
	m := New(MLDSA44)
	
	pk, sk, err := m.GenerateKeyPair()
	if err != nil {
		t.Fatalf("Failed to generate key pair: %v", err)
	}
	
	message := make([]byte, MLDSA44HashSize)
	rand.Read(message)
	
	signature, err := m.Sign(sk, message)
	if err != nil {
		t.Fatalf("Failed to sign message: %v", err)
	}
	
	expectedSigSize := MLDSA44SignatureSize
	if len(signature) != expectedSigSize {
		t.Errorf("Signature size mismatch: got %d, want %d", len(signature), expectedSigSize)
	}
	
	err = m.Verify(pk, message, signature)
	if err != nil {
		t.Fatalf("Failed to verify signature: %v", err)
	}
	
	t.Log("ML-DSA-44 sign and verify successful")
}

// TestDefaultInstance tests the default ML-DSA instance
func TestDefaultInstance(t *testing.T) {
	m := Default()
	if m == nil {
		t.Fatal("Default instance is nil")
	}
	
	if m.param != MLDSA65 {
		t.Errorf("Default parameter set mismatch: got %v, want MLDSA65", m.param)
	}
	
	t.Log("Default instance test passed")
}

// TestParameterSetConstants tests parameter set constants
func TestParameterSetConstants(t *testing.T) {
	testCases := []struct {
		name         string
		param        ParameterSet
		pkSize       int
		skSize       int
		sigSize      int
		expectedName string
	}{
		{"ML-DSA-44", MLDSA44, MLDSA44PublicKeySize, MLDSA44SecretKeySize, MLDSA44SignatureSize, "ML-DSA-44"},
		{"ML-DSA-65", MLDSA65, MLDSA65PublicKeySize, MLDSA65SecretKeySize, MLDSA65SignatureSize, "ML-DSA-65"},
		{"ML-DSA-87", MLDSA87, MLDSA87PublicKeySize, MLDSA87SecretKeySize, MLDSA87SignatureSize, "ML-DSA-87"},
	}
	
	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			if tc.param.PublicKeySize() != tc.pkSize {
				t.Errorf("PublicKeySize mismatch: got %d, want %d", tc.param.PublicKeySize(), tc.pkSize)
			}
			
			if tc.param.SecretKeySize() != tc.skSize {
				t.Errorf("SecretKeySize mismatch: got %d, want %d", tc.param.SecretKeySize(), tc.skSize)
			}
			
			if tc.param.SignatureSize() != tc.sigSize {
				t.Errorf("SignatureSize mismatch: got %d, want %d", tc.param.SignatureSize(), tc.sigSize)
			}
			
			if tc.param.Name() != tc.expectedName {
				t.Errorf("Name mismatch: got %s, want %s", tc.param.Name(), tc.expectedName)
			}
		})
	}
	
	t.Log("Parameter set constants test passed")
}
