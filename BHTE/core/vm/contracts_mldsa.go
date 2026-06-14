// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// ML-DSA Precompiled Contract for BHTE zkEVM

package vm

import (
	"errors"

	"bhte/mldsa"

	"github.com/ethereum/go-ethereum/common"
)

const (
	MLDSAVerifyGas     uint64 = 50000
	MLDSASignatureSize int    = mldsa.MLDSA65SignatureSize
	MLDSAPublicKeySize int    = mldsa.MLDSA65PublicKeySize
	MLDSAMessageSize   int    = 32

	MLDSAAddress = "0x0000000000000000000000000000000000000100"
)

var (
	ErrMLDSAInvalidSignatureLength = errors.New("invalid ML-DSA signature length")
	ErrMLDSAInvalidPublicKeyLength = errors.New("invalid ML-DSA public key length")
	ErrMLDSAVerificationFailed     = errors.New("ML-DSA verification failed")
	ErrMLDSAInvalidInput           = errors.New("invalid input data length")
)

type MLDSAPrecompile struct{}

func (c *MLDSAPrecompile) RequiredGas(input []byte) uint64 {
	return MLDSAVerifyGas
}

func (c *MLDSAPrecompile) Run(input []byte) ([]byte, error) {
	expectedLen := MLDSAMessageSize + MLDSAPublicKeySize + MLDSASignatureSize
	if len(input) < expectedLen {
		return nil, ErrMLDSAInvalidInput
	}

	message := input[:MLDSAMessageSize]
	pubkey := input[MLDSAMessageSize : MLDSAMessageSize+MLDSAPublicKeySize]
	signature := input[MLDSAMessageSize+MLDSAPublicKeySize : MLDSAMessageSize+MLDSAPublicKeySize+MLDSASignatureSize]

	if len(signature) != MLDSASignatureSize {
		return nil, ErrMLDSAInvalidSignatureLength
	}
	if len(pubkey) != MLDSAPublicKeySize {
		return nil, ErrMLDSAInvalidPublicKeyLength
	}

	valid := verifyMLDSA(message, pubkey, signature)
	if !valid {
		return nil, ErrMLDSAVerificationFailed
	}

	result := make([]byte, 32)
	result[31] = 1
	return result, nil
}

// verifyMLDSA performs genuine FIPS 204 ML-DSA-65 signature verification.
func verifyMLDSA(message, pubkey, signature []byte) bool {
	if len(message) != MLDSAMessageSize {
		return false
	}
	if len(pubkey) != MLDSAPublicKeySize {
		return false
	}
	if len(signature) != MLDSASignatureSize {
		return false
	}

	m := mldsa.New(mldsa.MLDSA65)
	pk, err := m.PublicKeyFromBytes(pubkey)
	if err != nil {
		return false
	}
	return m.Verify(pk, message, signature) == nil
}

func MLDSARun(message []byte, pubkey []byte, signature []byte) (bool, error) {
	if len(message) != MLDSAMessageSize {
		return false, ErrMLDSAInvalidInput
	}
	if len(pubkey) != MLDSAPublicKeySize {
		return false, ErrMLDSAInvalidPublicKeyLength
	}
	if len(signature) != MLDSASignatureSize {
		return false, ErrMLDSAInvalidSignatureLength
	}

	valid := verifyMLDSA(message, pubkey, signature)
	if !valid {
		return false, ErrMLDSAVerificationFailed
	}

	return true, nil
}

func GetMLDSAAddress() common.Address {
	return common.HexToAddress(MLDSAAddress)
}
