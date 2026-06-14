// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
//
// FIPS 204 ML-DSA core primitives: modular arithmetic, negacyclic NTT,
// sampling (ExpandA / ExpandS / ExpandMask / SampleInBall), rounding/hint
// helpers (Power2Round / Decompose / MakeHint / UseHint) and bit packing.
//
// This is a real, self-consistent implementation of the lattice operations
// described in NIST FIPS 204. It uses crypto/sha3 (stdlib) for the SHAKE128
// and SHAKE256 extendable-output functions. The NTT is the standard Dilithium
// negacyclic transform; correctness is checked against a schoolbook
// negacyclic multiply in the package tests.

package mldsa

import (
	"crypto/sha3"
)

const (
	// Q is the ML-DSA prime modulus 2^23 - 2^13 + 1.
	Q = 8380417
	// N is the polynomial degree (number of coefficients).
	N = 256
	// D is the number of dropped low-order bits of t.
	D = 13
	// rootZeta is a primitive 512-th root of unity modulo Q.
	rootZeta = 1753
)

const q64 = int64(Q)

// zetas[i] = rootZeta^bitrev8(i) mod Q, used by the negacyclic NTT.
var zetas [N]int32

// ninv = 256^{-1} mod Q, applied at the end of the inverse NTT.
var ninv int32

func init() {
	for i := 0; i < N; i++ {
		zetas[i] = int32(modPow(rootZeta, int64(bitrev8(i)), q64))
	}
	ninv = int32(modPow(N, q64-2, q64)) // Fermat inverse of 256
}

// ---- small integer helpers -------------------------------------------------

func bitrev8(x int) int {
	r := 0
	for i := 0; i < 8; i++ {
		r = (r << 1) | ((x >> i) & 1)
	}
	return r
}

func modPow(base, exp, mod int64) int64 {
	base %= mod
	if base < 0 {
		base += mod
	}
	result := int64(1)
	for exp > 0 {
		if exp&1 == 1 {
			result = result * base % mod
		}
		base = base * base % mod
		exp >>= 1
	}
	return result
}

func bitlen(x int) int {
	n := 0
	for x > 0 {
		n++
		x >>= 1
	}
	return n
}

// modQ reduces a into the range [0, Q).
func modQ(a int64) int32 {
	m := a % q64
	if m < 0 {
		m += q64
	}
	return int32(m)
}

// centered maps a coefficient in [0, Q) to the centered range (-Q/2, Q/2].
func centered(a int32) int32 {
	if a > Q/2 {
		return a - Q
	}
	return a
}

// ---- SHAKE streaming -------------------------------------------------------

type xofStream struct {
	s     *sha3.SHAKE
	buf   []byte
	pos   int
	block int
}

func newShake128(parts ...[]byte) *xofStream {
	h := sha3.NewSHAKE128()
	for _, p := range parts {
		h.Write(p)
	}
	return &xofStream{s: h, block: 168}
}

func newShake256(parts ...[]byte) *xofStream {
	h := sha3.NewSHAKE256()
	for _, p := range parts {
		h.Write(p)
	}
	return &xofStream{s: h, block: 136}
}

func (st *xofStream) readByte() byte {
	if st.pos >= len(st.buf) {
		if st.buf == nil {
			st.buf = make([]byte, st.block)
		}
		st.s.Read(st.buf)
		st.pos = 0
	}
	b := st.buf[st.pos]
	st.pos++
	return b
}

func (st *xofStream) readBytes(n int) []byte {
	out := make([]byte, n)
	for i := 0; i < n; i++ {
		out[i] = st.readByte()
	}
	return out
}

// shake256Sum returns the first outLen bytes of SHAKE256(parts...).
func shake256Sum(outLen int, parts ...[]byte) []byte {
	h := sha3.NewSHAKE256()
	for _, p := range parts {
		h.Write(p)
	}
	out := make([]byte, outLen)
	h.Read(out)
	return out
}

// ---- NTT -------------------------------------------------------------------

// nttForward transforms a (standard domain) into the negacyclic evaluation
// domain, in place. Coefficients are kept in [0, Q).
func nttForward(a []int32) {
	k := 0
	for length := 128; length > 0; length >>= 1 {
		for start := 0; start < N; start += 2 * length {
			k++
			zeta := int64(zetas[k])
			for j := start; j < start+length; j++ {
				t := int32(zeta * int64(a[j+length]) % q64)
				a[j+length] = modQ(int64(a[j]) - int64(t))
				a[j] = modQ(int64(a[j]) + int64(t))
			}
		}
	}
}

// nttInverse is the inverse of nttForward, in place.
func nttInverse(a []int32) {
	k := N
	for length := 1; length < N; length <<= 1 {
		for start := 0; start < N; start += 2 * length {
			k--
			zeta := int64(Q - zetas[k]) // -zetas[k] mod Q
			for j := start; j < start+length; j++ {
				t := a[j]
				a[j] = modQ(int64(t) + int64(a[j+length]))
				a[j+length] = modQ(int64(t) - int64(a[j+length]))
				a[j+length] = int32(zeta * int64(a[j+length]) % q64)
			}
		}
	}
	for j := 0; j < N; j++ {
		a[j] = int32(int64(ninv) * int64(a[j]) % q64)
	}
}

// pointwise returns the coefficient-wise product of two evaluation-domain polys.
func pointwise(a, b []int32) []int32 {
	c := make([]int32, N)
	for i := 0; i < N; i++ {
		c[i] = int32(int64(a[i]) * int64(b[i]) % q64)
	}
	return c
}

func polyAddInto(dst, src []int32) {
	for i := 0; i < N; i++ {
		dst[i] = modQ(int64(dst[i]) + int64(src[i]))
	}
}

func nttCopy(a []int32) []int32 {
	c := make([]int32, N)
	copy(c, a)
	nttForward(c)
	return c
}

// ---- rounding & hints ------------------------------------------------------

func power2Round(r int32) (r1, r0 int32) {
	r = modQ(int64(r))
	a := r & ((1 << D) - 1)
	if a > (1 << (D - 1)) {
		a -= 1 << D
	}
	return (r - a) >> D, a
}

func decompose(r, gamma2 int32) (r1, r0 int32) {
	r = modQ(int64(r))
	alpha := 2 * gamma2
	a := r % alpha
	if a > alpha/2 {
		a -= alpha
	}
	if r-a == Q-1 {
		return 0, a - 1
	}
	return (r - a) / alpha, a
}

func highBits(r, gamma2 int32) int32 {
	r1, _ := decompose(r, gamma2)
	return r1
}

func lowBits(r, gamma2 int32) int32 {
	_, r0 := decompose(r, gamma2)
	return r0
}

func useHint(h int, r, gamma2 int32) int32 {
	m := (Q - 1) / (2 * gamma2)
	r1, r0 := decompose(r, gamma2)
	if h == 0 {
		return r1
	}
	if r0 > 0 {
		r1++
	} else {
		r1--
	}
	r1 %= m
	if r1 < 0 {
		r1 += m
	}
	return r1
}

func polyInfNorm(p []int32) int32 {
	max := int32(0)
	for _, c := range p {
		a := centered(modQ(int64(c)))
		if a < 0 {
			a = -a
		}
		if a > max {
			max = a
		}
	}
	return max
}

// ---- sampling --------------------------------------------------------------

// expandA derives the k x l matrix A in the NTT domain from rho.
func expandA(rho []byte, k, l int) [][][]int32 {
	A := make([][][]int32, k)
	for r := 0; r < k; r++ {
		A[r] = make([][]int32, l)
		for s := 0; s < l; s++ {
			A[r][s] = rejNTTPoly(rho, byte(s), byte(r))
		}
	}
	return A
}

func rejNTTPoly(rho []byte, s, r byte) []int32 {
	st := newShake128(rho, []byte{s, r})
	p := make([]int32, N)
	i := 0
	for i < N {
		b := st.readBytes(3)
		d := int(b[0]) | int(b[1])<<8 | int(b[2]&0x7F)<<16
		if d < Q {
			p[i] = int32(d)
			i++
		}
	}
	return p
}

// expandS derives the secret vectors s1 (length l) and s2 (length k) from rhoP.
func expandS(rhoP []byte, k, l, eta int) (s1, s2 [][]int32) {
	s1 = make([][]int32, l)
	for i := 0; i < l; i++ {
		s1[i] = rejBoundedPoly(rhoP, uint16(i), eta)
	}
	s2 = make([][]int32, k)
	for i := 0; i < k; i++ {
		s2[i] = rejBoundedPoly(rhoP, uint16(l+i), eta)
	}
	return
}

func rejBoundedPoly(rhoP []byte, nonce uint16, eta int) []int32 {
	st := newShake256(rhoP, []byte{byte(nonce), byte(nonce >> 8)})
	p := make([]int32, N)
	i := 0
	for i < N {
		b := st.readByte()
		z0 := int(b & 0x0F)
		z1 := int(b >> 4)
		if v, ok := coeffFromHalfByte(z0, eta); ok {
			p[i] = v
			i++
			if i >= N {
				break
			}
		}
		if v, ok := coeffFromHalfByte(z1, eta); ok {
			p[i] = v
			i++
		}
	}
	return p
}

func coeffFromHalfByte(z, eta int) (int32, bool) {
	if eta == 2 {
		if z < 15 {
			return int32(2 - (z % 5)), true
		}
		return 0, false
	}
	// eta == 4
	if z < 9 {
		return int32(4 - z), true
	}
	return 0, false
}

// expandMask derives the masking vector y (length l) from rhoPP and kappa.
func expandMask(rhoPP []byte, kappa, l int, gamma1 int32) [][]int32 {
	width := bitlen(int(2*gamma1 - 1))
	bytesPerPoly := N * width / 8
	y := make([][]int32, l)
	for i := 0; i < l; i++ {
		nonce := uint16(kappa + i)
		st := newShake256(rhoPP, []byte{byte(nonce), byte(nonce >> 8)})
		buf := st.readBytes(bytesPerPoly)
		y[i] = bitUnpack(buf, gamma1-1, gamma1, width)
	}
	return y
}

// sampleInBall derives the challenge polynomial c (tau coefficients of +-1).
func sampleInBall(ctilde []byte, tau int) []int32 {
	st := newShake256(ctilde)
	signBytes := st.readBytes(8)
	var signs uint64
	for i := 0; i < 8; i++ {
		signs |= uint64(signBytes[i]) << (8 * i)
	}
	c := make([]int32, N)
	for i := N - tau; i < N; i++ {
		var j byte
		for {
			j = st.readByte()
			if int(j) <= i {
				break
			}
		}
		c[i] = c[j]
		if signs&1 == 1 {
			c[j] = -1
		} else {
			c[j] = 1
		}
		signs >>= 1
	}
	return c
}

// ---- bit packing -----------------------------------------------------------

type bitWriter struct {
	out   []byte
	cur   uint64
	nbits uint
}

func (w *bitWriter) write(val uint32, bits int) {
	w.cur |= uint64(val) << w.nbits
	w.nbits += uint(bits)
	for w.nbits >= 8 {
		w.out = append(w.out, byte(w.cur))
		w.cur >>= 8
		w.nbits -= 8
	}
}

func (w *bitWriter) bytes() []byte {
	if w.nbits > 0 {
		w.out = append(w.out, byte(w.cur))
	}
	return w.out
}

type bitReader struct {
	in    []byte
	pos   int
	cur   uint64
	nbits uint
}

func (r *bitReader) read(bits int) uint32 {
	for r.nbits < uint(bits) {
		var b byte
		if r.pos < len(r.in) {
			b = r.in[r.pos]
			r.pos++
		}
		r.cur |= uint64(b) << r.nbits
		r.nbits += 8
	}
	v := uint32(r.cur & ((1 << uint(bits)) - 1))
	r.cur >>= uint(bits)
	r.nbits -= uint(bits)
	return v
}

// simpleBitPack packs non-negative coefficients using `bits` bits each.
func simpleBitPack(p []int32, bits int) []byte {
	w := &bitWriter{}
	for _, c := range p {
		w.write(uint32(c), bits)
	}
	return w.bytes()
}

func simpleBitUnpack(buf []byte, bits int) []int32 {
	r := &bitReader{in: buf}
	p := make([]int32, N)
	for i := 0; i < N; i++ {
		p[i] = int32(r.read(bits))
	}
	return p
}

// bitPack packs coefficients in [-a, b] by storing (b - coeff) using `bits` bits.
func bitPack(p []int32, b int32, bits int) []byte {
	w := &bitWriter{}
	for _, c := range p {
		w.write(uint32(b-c), bits)
	}
	return w.bytes()
}

func bitUnpack(buf []byte, a, b int32, bits int) []int32 {
	r := &bitReader{in: buf}
	p := make([]int32, N)
	for i := 0; i < N; i++ {
		p[i] = b - int32(r.read(bits))
	}
	return p
}

// hintPack encodes the hint polynomials into omega+k bytes.
func hintPack(h [][]int32, omega, k int) []byte {
	out := make([]byte, omega+k)
	idx := 0
	for i := 0; i < k; i++ {
		for j := 0; j < N; j++ {
			if h[i][j] != 0 {
				if idx >= omega {
					return nil
				}
				out[idx] = byte(j)
				idx++
			}
		}
		out[omega+i] = byte(idx)
	}
	return out
}

// hintUnpack decodes and validates the hint encoding. ok is false if the
// encoding is malformed (out-of-order indices, too many ones, dirty padding).
func hintUnpack(buf []byte, omega, k int) (h [][]int32, ok bool) {
	if len(buf) != omega+k {
		return nil, false
	}
	h = make([][]int32, k)
	for i := range h {
		h[i] = make([]int32, N)
	}
	idx := 0
	for i := 0; i < k; i++ {
		end := int(buf[omega+i])
		if end < idx || end > omega {
			return nil, false
		}
		first := idx
		for ; idx < end; idx++ {
			if idx > first && buf[idx] <= buf[idx-1] {
				return nil, false
			}
			h[i][buf[idx]] = 1
		}
	}
	for ; idx < omega; idx++ {
		if buf[idx] != 0 {
			return nil, false
		}
	}
	return h, true
}
