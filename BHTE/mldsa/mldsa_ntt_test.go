// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license

package mldsa

import (
	"crypto/rand"
	"testing"
)

// schoolbookMul computes the negacyclic product a*b mod (x^256 + 1, Q).
func schoolbookMul(a, b []int32) []int32 {
	c := make([]int64, 2*N)
	for i := 0; i < N; i++ {
		for j := 0; j < N; j++ {
			c[i+j] += int64(a[i]) * int64(b[j]) % q64
		}
	}
	out := make([]int32, N)
	for i := 0; i < N; i++ {
		v := c[i] - c[i+N] // x^n = -1
		out[i] = modQ(v)
	}
	return out
}

// TestNTTMatchesSchoolbook validates the negacyclic NTT against a direct
// convolution. This is the foundational correctness check for the scheme.
func TestNTTMatchesSchoolbook(t *testing.T) {
	for trial := 0; trial < 20; trial++ {
		a := randPoly(t)
		b := randPoly(t)

		want := schoolbookMul(a, b)

		ah := nttCopy(a)
		bh := nttCopy(b)
		got := pointwise(ah, bh)
		nttInverse(got)
		for i := range got {
			got[i] = modQ(int64(got[i]))
		}

		for i := 0; i < N; i++ {
			if got[i] != want[i] {
				t.Fatalf("trial %d coeff %d: got %d want %d", trial, i, got[i], want[i])
			}
		}
	}
}

// TestNTTRoundTrip checks nttInverse(nttForward(a)) == a.
func TestNTTRoundTrip(t *testing.T) {
	a := randPoly(t)
	orig := make([]int32, N)
	copy(orig, a)
	nttForward(a)
	nttInverse(a)
	for i := 0; i < N; i++ {
		if modQ(int64(a[i])) != orig[i] {
			t.Fatalf("coeff %d: got %d want %d", i, modQ(int64(a[i])), orig[i])
		}
	}
}

func randPoly(t *testing.T) []int32 {
	t.Helper()
	p := make([]int32, N)
	buf := make([]byte, 4*N)
	if _, err := rand.Read(buf); err != nil {
		t.Fatal(err)
	}
	for i := 0; i < N; i++ {
		v := uint32(buf[4*i]) | uint32(buf[4*i+1])<<8 | uint32(buf[4*i+2])<<16 | uint32(buf[4*i+3])<<24
		p[i] = int32(v % Q)
	}
	return p
}
