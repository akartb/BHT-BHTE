// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license

package spv

import (
	"encoding/hex"
	"math/big"
	"testing"
)

// TestDecodeBits checks the compact nBits → target conversion against the
// canonical Bitcoin difficulty-1 value and a couple of edge cases.
func TestDecodeBits(t *testing.T) {
	c := &BHCSPVClient{}

	// Difficulty-1 target: 0x1d00ffff → 0x00000000FFFF0000...0000 (256-bit).
	got := c.decodeBits(0x1d00ffff)
	want, _ := new(big.Int).SetString("00000000ffff0000000000000000000000000000000000000000000000000000", 16)
	if got.Cmp(want) != 0 {
		t.Fatalf("decodeBits(0x1d00ffff) = %x, want %x", got, want)
	}

	// Small exponent (<=3): mantissa right-shifted.
	// 0x03123456 → exponent 3 → target = 0x123456.
	if g := c.decodeBits(0x03123456); g.Cmp(big.NewInt(0x123456)) != 0 {
		t.Fatalf("decodeBits(0x03123456) = %x, want 123456", g)
	}
	// 0x02123456 → exponent 2 → target = 0x1234.
	if g := c.decodeBits(0x02123456); g.Cmp(big.NewInt(0x1234)) != 0 {
		t.Fatalf("decodeBits(0x02123456) = %x, want 1234", g)
	}
}

// buildLeaf produces a deterministic 32-byte internal-order leaf hash.
func buildLeaf(b byte) []byte {
	out := make([]byte, 32)
	for i := range out {
		out[i] = b
	}
	return out
}

func internalToDisplay(b []byte) string {
	return encodeReversed(b)
}

// TestMerkleProofTwoLeaves verifies the fold direction for a depth-1 tree.
func TestMerkleProofTwoLeaves(t *testing.T) {
	c := &BHCSPVClient{}

	l0 := buildLeaf(0x11)
	l1 := buildLeaf(0x22)
	root := sha256d(append(append([]byte{}, l0...), l1...)) // H(l0 || l1)

	// Proof for l0: sibling l1 is on the right (flag true).
	got, err := c.computeMerkleRoot(internalToDisplay(l0), []string{internalToDisplay(l1)}, []bool{true})
	if err != nil {
		t.Fatal(err)
	}
	if got != internalToDisplay(root) {
		t.Fatalf("l0 proof: got %s want %s", got, internalToDisplay(root))
	}

	// Proof for l1: sibling l0 is on the left (flag false).
	got, err = c.computeMerkleRoot(internalToDisplay(l1), []string{internalToDisplay(l0)}, []bool{false})
	if err != nil {
		t.Fatal(err)
	}
	if got != internalToDisplay(root) {
		t.Fatalf("l1 proof: got %s want %s", got, internalToDisplay(root))
	}
}

// TestMerkleProofFourLeaves verifies a depth-2 proof and that tampering fails.
func TestMerkleProofFourLeaves(t *testing.T) {
	c := &BHCSPVClient{}

	l := [][]byte{buildLeaf(0x01), buildLeaf(0x02), buildLeaf(0x03), buildLeaf(0x04)}
	p01 := sha256d(append(append([]byte{}, l[0]...), l[1]...))
	p23 := sha256d(append(append([]byte{}, l[2]...), l[3]...))
	root := sha256d(append(append([]byte{}, p01...), p23...))

	// Proof for leaf 0: level0 sibling = l1 (right), level1 sibling = p23 (right).
	got, err := c.computeMerkleRoot(
		internalToDisplay(l[0]),
		[]string{internalToDisplay(l[1]), internalToDisplay(p23)},
		[]bool{true, true},
	)
	if err != nil {
		t.Fatal(err)
	}
	if got != internalToDisplay(root) {
		t.Fatalf("leaf0 proof: got %s want %s", got, internalToDisplay(root))
	}

	// Proof for leaf 2: level0 sibling = l3 (right), level1 sibling = p01 (left).
	got, err = c.computeMerkleRoot(
		internalToDisplay(l[2]),
		[]string{internalToDisplay(l[3]), internalToDisplay(p01)},
		[]bool{true, false},
	)
	if err != nil {
		t.Fatal(err)
	}
	if got != internalToDisplay(root) {
		t.Fatalf("leaf2 proof: got %s want %s", got, internalToDisplay(root))
	}

	// Tampered sibling must not reproduce the root.
	bad := internalToDisplay(buildLeaf(0xFF))
	got, _ = c.computeMerkleRoot(internalToDisplay(l[0]), []string{bad, internalToDisplay(p23)}, []bool{true, true})
	if got == internalToDisplay(root) {
		t.Fatal("tampered proof unexpectedly matched root")
	}
}

// TestSha256dKnownAnswer checks double-SHA256 of empty input.
func TestSha256dKnownAnswer(t *testing.T) {
	got := hex.EncodeToString(sha256d([]byte{}))
	// SHA256d("") = sha256(sha256("")).
	want := "5df6e0e2761359d30a8275058e299fcc0381534545f55cf43e41983f5d4c9456"
	if got != want {
		t.Fatalf("sha256d(\"\") = %s want %s", got, want)
	}
}
