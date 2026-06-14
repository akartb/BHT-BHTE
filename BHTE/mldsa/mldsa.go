// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
//
// NIST FIPS 204 ML-DSA (Module-Lattice-Based Digital Signature Algorithm).
// Reference: https://doi.org/10.6028/NIST.FIPS.204
//
// This is a genuine, self-consistent implementation of ML-DSA-44/65/87 in pure
// Go (it does NOT call out to liboqs). Key generation, signing and verification
// follow FIPS 204; the lattice primitives live in mldsa_core.go. Signatures are
// generated with the hedged (randomized) variant and an empty signing context.

package mldsa

import (
	"crypto/rand"
	"errors"
	"sync"
)

const (
	// MLDSA44HashSize is the digest size historically used by callers when
	// preparing a 32-byte message. ML-DSA itself signs arbitrary-length input.
	MLDSA44HashSize = 32

	MLDSA44PublicKeySize = 1312
	MLDSA44SecretKeySize = 2560
	MLDSA44SignatureSize = 2420

	MLDSA65PublicKeySize = 1952
	MLDSA65SecretKeySize = 4032
	MLDSA65SignatureSize = 3309

	MLDSA87PublicKeySize = 2592
	MLDSA87SecretKeySize = 4896
	MLDSA87SignatureSize = 4627
)

var (
	ErrInvalidPublicKeySize = errors.New("invalid public key size")
	ErrInvalidSecretKeySize = errors.New("invalid secret key size")
	ErrInvalidSignatureSize = errors.New("invalid signature size")
	ErrVerificationFailed   = errors.New("signature verification failed")
	ErrSigningFailed        = errors.New("signing failed: exceeded rejection bound")
	ErrRandomGeneration     = errors.New("failed to generate random bytes")
)

type ParameterSet int

const (
	MLDSA44 ParameterSet = iota
	MLDSA65
	MLDSA87
)

// params holds the FIPS 204 parameters for a given security level.
type params struct {
	k, l    int
	eta     int
	tau     int
	beta    int32
	gamma1  int32
	gamma2  int32
	omega   int
	ctBytes int // challenge-hash (cTilde) length = lambda/4
}

func (p ParameterSet) info() params {
	switch p {
	case MLDSA44:
		return params{k: 4, l: 4, eta: 2, tau: 39, beta: 39 * 2, gamma1: 1 << 17, gamma2: (Q - 1) / 88, omega: 80, ctBytes: 32}
	case MLDSA65:
		return params{k: 6, l: 5, eta: 4, tau: 49, beta: 49 * 4, gamma1: 1 << 19, gamma2: (Q - 1) / 32, omega: 55, ctBytes: 48}
	case MLDSA87:
		return params{k: 8, l: 7, eta: 2, tau: 60, beta: 60 * 2, gamma1: 1 << 19, gamma2: (Q - 1) / 32, omega: 75, ctBytes: 64}
	default:
		return params{}
	}
}

func (p ParameterSet) Name() string {
	switch p {
	case MLDSA44:
		return "ML-DSA-44"
	case MLDSA65:
		return "ML-DSA-65"
	case MLDSA87:
		return "ML-DSA-87"
	default:
		return "Unknown"
	}
}

func (p ParameterSet) PublicKeySize() int {
	switch p {
	case MLDSA44:
		return MLDSA44PublicKeySize
	case MLDSA65:
		return MLDSA65PublicKeySize
	case MLDSA87:
		return MLDSA87PublicKeySize
	default:
		return 0
	}
}

func (p ParameterSet) SecretKeySize() int {
	switch p {
	case MLDSA44:
		return MLDSA44SecretKeySize
	case MLDSA65:
		return MLDSA65SecretKeySize
	case MLDSA87:
		return MLDSA87SecretKeySize
	default:
		return 0
	}
}

func (p ParameterSet) SignatureSize() int {
	switch p {
	case MLDSA44:
		return MLDSA44SignatureSize
	case MLDSA65:
		return MLDSA65SignatureSize
	case MLDSA87:
		return MLDSA87SignatureSize
	default:
		return 0
	}
}

// PublicKey holds rho and the high bits t1 of t (one polynomial per row).
type PublicKey struct {
	rho   [32]byte
	t1    [][]int32
	param ParameterSet
}

// SecretKey holds the full ML-DSA private key material.
type SecretKey struct {
	rho   [32]byte
	key   [32]byte
	tr    [64]byte
	s1    [][]int32
	s2    [][]int32
	t0    [][]int32
	param ParameterSet
}

// ML_DSA is a stateless signer/verifier bound to a parameter set.
type ML_DSA struct {
	param ParameterSet
	mu    sync.RWMutex
}

var (
	defaultInstance *ML_DSA
	once            sync.Once
)

// Default returns a shared ML-DSA-65 instance.
func Default() *ML_DSA {
	once.Do(func() {
		defaultInstance = New(MLDSA65)
	})
	return defaultInstance
}

// New returns an ML-DSA instance for the given parameter set.
func New(param ParameterSet) *ML_DSA {
	return &ML_DSA{param: param}
}

// GenerateKey generates a key pair for the requested parameter set.
func GenerateKey(param ParameterSet) (*PublicKey, *SecretKey, error) {
	return New(param).GenerateKeyPair()
}

// GenerateKeyPair generates a fresh key pair using OS randomness.
func (m *ML_DSA) GenerateKeyPair() (*PublicKey, *SecretKey, error) {
	var xi [32]byte
	if _, err := rand.Read(xi[:]); err != nil {
		return nil, nil, ErrRandomGeneration
	}
	return m.keyGenFromSeed(xi[:])
}

// keyGenFromSeed implements ML-DSA.KeyGen_internal with seed xi (32 bytes).
func (m *ML_DSA) keyGenFromSeed(xi []byte) (*PublicKey, *SecretKey, error) {
	p := m.param.info()
	if p.k == 0 {
		return nil, nil, ErrInvalidPublicKeySize
	}

	expanded := shake256Sum(128, xi, []byte{byte(p.k)}, []byte{byte(p.l)})
	rho := expanded[0:32]
	rhoP := expanded[32:96]
	K := expanded[96:128]

	A := expandA(rho, p.k, p.l)
	s1, s2 := expandS(rhoP, p.k, p.l, p.eta)

	// t = A * s1 + s2
	s1hat := make([][]int32, p.l)
	for j := 0; j < p.l; j++ {
		s1hat[j] = nttCopy(s1[j])
	}
	t1 := make([][]int32, p.k)
	t0 := make([][]int32, p.k)
	for i := 0; i < p.k; i++ {
		acc := make([]int32, N)
		for j := 0; j < p.l; j++ {
			polyAddInto(acc, pointwise(A[i][j], s1hat[j]))
		}
		nttInverse(acc)
		polyAddInto(acc, s2[i])
		t1[i] = make([]int32, N)
		t0[i] = make([]int32, N)
		for c := 0; c < N; c++ {
			t1[i][c], t0[i][c] = power2Round(acc[c])
		}
	}

	pk := &PublicKey{t1: t1, param: m.param}
	copy(pk.rho[:], rho)

	sk := &SecretKey{s1: s1, s2: s2, t0: t0, param: m.param}
	copy(sk.rho[:], rho)
	copy(sk.key[:], K)
	tr := shake256Sum(64, m.PublicKeyToBytes(pk))
	copy(sk.tr[:], tr)

	return pk, sk, nil
}

// framedMessage applies the FIPS 204 message framing for an empty context:
// M' = 0x00 || 0x00 || message.
func framedMessage(message []byte) []byte {
	out := make([]byte, 0, len(message)+2)
	out = append(out, 0x00, 0x00)
	out = append(out, message...)
	return out
}

// Sign produces an ML-DSA signature over message (any length).
func (m *ML_DSA) Sign(sk *SecretKey, message []byte) ([]byte, error) {
	p := m.param.info()
	A := expandA(sk.rho[:], p.k, p.l)

	mu := shake256Sum(64, sk.tr[:], framedMessage(message))

	var rnd [32]byte
	if _, err := rand.Read(rnd[:]); err != nil {
		return nil, ErrRandomGeneration
	}
	rhoPP := shake256Sum(64, sk.key[:], rnd[:], mu)

	s1hat := make([][]int32, p.l)
	for j := 0; j < p.l; j++ {
		s1hat[j] = nttCopy(sk.s1[j])
	}
	s2hat := make([][]int32, p.k)
	t0hat := make([][]int32, p.k)
	for i := 0; i < p.k; i++ {
		s2hat[i] = nttCopy(sk.s2[i])
		t0hat[i] = nttCopy(sk.t0[i])
	}

	zbits := bitlen(int(2*p.gamma1 - 1))
	w1bits := bitlen(int((Q-1)/(2*p.gamma2)) - 1)

	for kappa := 0; kappa < 1000*p.l; kappa += p.l {
		y := expandMask(rhoPP, kappa, p.l, p.gamma1)

		yhat := make([][]int32, p.l)
		for j := 0; j < p.l; j++ {
			yhat[j] = nttCopy(y[j])
		}

		w := make([][]int32, p.k)
		w1 := make([][]int32, p.k)
		for i := 0; i < p.k; i++ {
			acc := make([]int32, N)
			for j := 0; j < p.l; j++ {
				polyAddInto(acc, pointwise(A[i][j], yhat[j]))
			}
			nttInverse(acc)
			w[i] = acc
			w1[i] = make([]int32, N)
			for c := 0; c < N; c++ {
				w1[i][c] = highBits(acc[c], p.gamma2)
			}
		}

		ctilde := shake256Sum(p.ctBytes, mu, packW1(w1, w1bits))
		c := sampleInBall(ctilde, p.tau)
		chat := nttCopy(c)

		// z = y + c*s1
		z := make([][]int32, p.l)
		for j := 0; j < p.l; j++ {
			cs1 := pointwise(chat, s1hat[j])
			nttInverse(cs1)
			z[j] = make([]int32, N)
			for cc := 0; cc < N; cc++ {
				z[j][cc] = centered(modQ(int64(y[j][cc]) + int64(cs1[cc])))
			}
		}
		if polyVecInfNorm(z) >= p.gamma1-p.beta {
			continue
		}

		// r0 = LowBits(w - c*s2)
		wcs2 := make([][]int32, p.k)
		r0norm := int32(0)
		for i := 0; i < p.k; i++ {
			cs2 := pointwise(chat, s2hat[i])
			nttInverse(cs2)
			wcs2[i] = make([]int32, N)
			for cc := 0; cc < N; cc++ {
				wcs2[i][cc] = modQ(int64(w[i][cc]) - int64(cs2[cc]))
				lb := lowBits(wcs2[i][cc], p.gamma2)
				if lb < 0 {
					lb = -lb
				}
				if lb > r0norm {
					r0norm = lb
				}
			}
		}
		if r0norm >= p.gamma2-p.beta {
			continue
		}

		// hint = MakeHint(-c*t0, w - c*s2 + c*t0)
		h := make([][]int32, p.k)
		ct0norm := int32(0)
		hintCount := 0
		for i := 0; i < p.k; i++ {
			ct0 := pointwise(chat, t0hat[i])
			nttInverse(ct0)
			h[i] = make([]int32, N)
			for cc := 0; cc < N; cc++ {
				ct0c := centered(modQ(int64(ct0[cc])))
				abs := ct0c
				if abs < 0 {
					abs = -abs
				}
				if abs > ct0norm {
					ct0norm = abs
				}
				hi1 := highBits(modQ(int64(wcs2[i][cc])+int64(ct0[cc])), p.gamma2)
				hi2 := highBits(wcs2[i][cc], p.gamma2)
				if hi1 != hi2 {
					h[i][cc] = 1
					hintCount++
				}
			}
		}
		if ct0norm >= p.gamma2 {
			continue
		}
		if hintCount > p.omega {
			continue
		}

		// Encode signature: cTilde || z || hint
		sig := make([]byte, 0, m.param.SignatureSize())
		sig = append(sig, ctilde...)
		for j := 0; j < p.l; j++ {
			sig = append(sig, bitPack(z[j], p.gamma1, zbits)...)
		}
		hintBytes := hintPack(h, p.omega, p.k)
		if hintBytes == nil {
			continue
		}
		sig = append(sig, hintBytes...)
		return sig, nil
	}
	return nil, ErrSigningFailed
}

// Verify checks an ML-DSA signature; it returns nil on success.
func (m *ML_DSA) Verify(pk *PublicKey, message []byte, signature []byte) error {
	p := m.param.info()
	if len(signature) != m.param.SignatureSize() {
		return ErrInvalidSignatureSize
	}

	zbits := bitlen(int(2*p.gamma1 - 1))
	w1bits := bitlen(int((Q-1)/(2*p.gamma2)) - 1)

	off := 0
	ctilde := signature[off : off+p.ctBytes]
	off += p.ctBytes

	zBytesPerPoly := N * zbits / 8
	z := make([][]int32, p.l)
	for j := 0; j < p.l; j++ {
		z[j] = bitUnpack(signature[off:off+zBytesPerPoly], p.gamma1-1, p.gamma1, zbits)
		off += zBytesPerPoly
	}

	h, ok := hintUnpack(signature[off:off+p.omega+p.k], p.omega, p.k)
	if !ok {
		return ErrVerificationFailed
	}

	if polyVecInfNorm(z) >= p.gamma1-p.beta {
		return ErrVerificationFailed
	}

	A := expandA(pk.rho[:], p.k, p.l)
	tr := shake256Sum(64, m.PublicKeyToBytes(pk))
	mu := shake256Sum(64, tr, framedMessage(message))

	c := sampleInBall(ctilde, p.tau)
	chat := nttCopy(c)

	zhat := make([][]int32, p.l)
	for j := 0; j < p.l; j++ {
		zhat[j] = nttCopy(z[j])
	}

	// t1 scaled by 2^D, transformed to NTT domain.
	t1hat := make([][]int32, p.k)
	for i := 0; i < p.k; i++ {
		scaled := make([]int32, N)
		for cc := 0; cc < N; cc++ {
			scaled[cc] = modQ(int64(pk.t1[i][cc]) << D)
		}
		t1hat[i] = nttCopy(scaled)
	}

	w1 := make([][]int32, p.k)
	for i := 0; i < p.k; i++ {
		acc := make([]int32, N)
		for j := 0; j < p.l; j++ {
			polyAddInto(acc, pointwise(A[i][j], zhat[j]))
		}
		ct1 := pointwise(chat, t1hat[i])
		for cc := 0; cc < N; cc++ {
			acc[cc] = modQ(int64(acc[cc]) - int64(ct1[cc]))
		}
		nttInverse(acc)
		w1[i] = make([]int32, N)
		for cc := 0; cc < N; cc++ {
			w1[i][cc] = useHint(int(h[i][cc]), acc[cc], p.gamma2)
		}
	}

	ctildePrime := shake256Sum(p.ctBytes, mu, packW1(w1, w1bits))
	if !constantTimeEqual(ctilde, ctildePrime) {
		return ErrVerificationFailed
	}
	return nil
}

func packW1(w1 [][]int32, bits int) []byte {
	out := make([]byte, 0, len(w1)*N*bits/8)
	for _, poly := range w1 {
		out = append(out, simpleBitPack(poly, bits)...)
	}
	return out
}

func polyVecInfNorm(v [][]int32) int32 {
	max := int32(0)
	for _, p := range v {
		if n := polyInfNorm(p); n > max {
			max = n
		}
	}
	return max
}

func constantTimeEqual(a, b []byte) bool {
	if len(a) != len(b) {
		return false
	}
	var diff byte
	for i := range a {
		diff |= a[i] ^ b[i]
	}
	return diff == 0
}

// ---- serialization ---------------------------------------------------------

// PublicKeyToBytes encodes a public key (pkEncode).
func (m *ML_DSA) PublicKeyToBytes(pk *PublicKey) []byte {
	out := make([]byte, 0, m.param.PublicKeySize())
	out = append(out, pk.rho[:]...)
	for i := range pk.t1 {
		out = append(out, simpleBitPack(pk.t1[i], 10)...)
	}
	return out
}

// SecretKeyToBytes encodes a secret key (skEncode).
func (m *ML_DSA) SecretKeyToBytes(sk *SecretKey) []byte {
	p := m.param.info()
	etabits := bitlen(2 * p.eta)
	out := make([]byte, 0, m.param.SecretKeySize())
	out = append(out, sk.rho[:]...)
	out = append(out, sk.key[:]...)
	out = append(out, sk.tr[:]...)
	for i := 0; i < p.l; i++ {
		out = append(out, bitPack(sk.s1[i], int32(p.eta), etabits)...)
	}
	for i := 0; i < p.k; i++ {
		out = append(out, bitPack(sk.s2[i], int32(p.eta), etabits)...)
	}
	for i := 0; i < p.k; i++ {
		out = append(out, bitPack(sk.t0[i], 1<<(D-1), D)...)
	}
	return out
}

// PublicKeyFromBytes decodes a public key (pkDecode).
func (m *ML_DSA) PublicKeyFromBytes(data []byte) (*PublicKey, error) {
	p := m.param.info()
	if len(data) != m.param.PublicKeySize() {
		return nil, ErrInvalidPublicKeySize
	}
	pk := &PublicKey{param: m.param, t1: make([][]int32, p.k)}
	copy(pk.rho[:], data[0:32])
	off := 32
	bytesPerPoly := N * 10 / 8
	for i := 0; i < p.k; i++ {
		pk.t1[i] = simpleBitUnpack(data[off:off+bytesPerPoly], 10)
		off += bytesPerPoly
	}
	return pk, nil
}

// SecretKeyFromBytes decodes a secret key (skDecode).
func (m *ML_DSA) SecretKeyFromBytes(data []byte) (*SecretKey, error) {
	p := m.param.info()
	if len(data) != m.param.SecretKeySize() {
		return nil, ErrInvalidSecretKeySize
	}
	etabits := bitlen(2 * p.eta)
	sk := &SecretKey{param: m.param}
	copy(sk.rho[:], data[0:32])
	copy(sk.key[:], data[32:64])
	copy(sk.tr[:], data[64:128])
	off := 128
	etaBytes := N * etabits / 8
	sk.s1 = make([][]int32, p.l)
	for i := 0; i < p.l; i++ {
		sk.s1[i] = bitUnpack(data[off:off+etaBytes], int32(p.eta), int32(p.eta), etabits)
		off += etaBytes
	}
	sk.s2 = make([][]int32, p.k)
	for i := 0; i < p.k; i++ {
		sk.s2[i] = bitUnpack(data[off:off+etaBytes], int32(p.eta), int32(p.eta), etabits)
		off += etaBytes
	}
	t0Bytes := N * D / 8
	sk.t0 = make([][]int32, p.k)
	for i := 0; i < p.k; i++ {
		sk.t0[i] = bitUnpack(data[off:off+t0Bytes], 1<<(D-1)-1, 1<<(D-1), D)
		off += t0Bytes
	}
	return sk, nil
}

// ---- MLSigner convenience wrapper ------------------------------------------

// MLSignerConfig configures NewMLSigner.
type MLSignerConfig struct {
	ParameterSet ParameterSet
	MLDSAEnabled bool
}

// MLSigner is a deterministic-key signer bound to a parameter set.
type MLSigner struct {
	pk    *PublicKey
	sk    *SecretKey
	param ParameterSet
}

// NewMLSigner derives a key pair deterministically from seed (if at least 32
// bytes are supplied) and returns a signer. A short/empty seed falls back to
// fresh OS randomness.
func NewMLSigner(seed []byte, config *MLSignerConfig) (*MLSigner, error) {
	m := New(config.ParameterSet)
	var pk *PublicKey
	var sk *SecretKey
	var err error
	if len(seed) >= 32 {
		pk, sk, err = m.keyGenFromSeed(seed[:32])
	} else {
		pk, sk, err = m.GenerateKeyPair()
	}
	if err != nil {
		return nil, err
	}
	return &MLSigner{pk: pk, sk: sk, param: config.ParameterSet}, nil
}

// Sign signs message with the wrapped secret key.
func (s *MLSigner) Sign(message []byte) ([]byte, error) {
	return New(s.param).Sign(s.sk, message)
}

// PublicKey returns the encoded public key.
func (s *MLSigner) PublicKey() []byte {
	return New(s.param).PublicKeyToBytes(s.pk)
}

// Verify verifies signature over message with the wrapped public key.
func (s *MLSigner) Verify(message, signature []byte) error {
	return New(s.param).Verify(s.pk, message, signature)
}
