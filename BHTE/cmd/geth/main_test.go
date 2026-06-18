// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license

package main

import (
	"encoding/json"
	"strings"
	"testing"
)

func newTestNode(t *testing.T) *rpcNode {
	t.Helper()
	node, err := newRPCNode([]string{"--datadir", t.TempDir(), "--dev-insecure"})
	if err != nil {
		t.Fatalf("newRPCNode failed: %v", err)
	}
	return node
}

func TestEthGetProofUsesStateAndStorageTries(t *testing.T) {
	node := newTestNode(t)
	addr := "0x0000000000000000000000000000000000000201"
	slot := zeroHash()
	value := "0x2a"

	node.state.Accounts = append(node.state.Accounts, addr)
	node.state.Balances[addr] = "0x10"
	node.state.Nonces[addr] = 7
	node.state.Code[addr] = "0x6001600055"
	node.state.Storage[addr] = map[string]string{slot: value}

	raw, _ := json.Marshal([]interface{}{addr, []string{slot}, "latest"})
	result, err := node.call("eth_getProof", raw)
	if err != nil {
		t.Fatalf("eth_getProof failed: %v", err)
	}
	proof := result.(map[string]interface{})

	if proof["stateRoot"] == zeroHash() {
		t.Fatal("stateRoot must not be the all-zero placeholder")
	}
	if proof["storageHash"] == zeroHash() {
		t.Fatal("storageHash must not be the all-zero placeholder")
	}
	if proof["codeHash"] == zeroHash() {
		t.Fatal("codeHash must not be the all-zero placeholder")
	}
	accountProof := proof["accountProof"].([]string)
	if len(accountProof) == 0 || !strings.HasPrefix(accountProof[0], "0x") {
		t.Fatalf("accountProof was not populated: %#v", accountProof)
	}
	storageProofs := proof["storageProof"].([]interface{})
	if len(storageProofs) != 1 {
		t.Fatalf("storageProof length = %d, want 1", len(storageProofs))
	}
	storageProof := storageProofs[0].(map[string]interface{})
	if storageProof["value"] != value {
		t.Fatalf("storage proof value = %v, want %s", storageProof["value"], value)
	}
	if len(storageProof["proof"].([]string)) == 0 {
		t.Fatal("storage proof nodes were not populated")
	}
}

func TestTrieCommitsPersistAtMinedBlockHeight(t *testing.T) {
	node := newTestNode(t)
	from := node.state.Accounts[0]
	contract := "0x0000000000000000000000000000000000000202"

	raw, _ := json.Marshal([]interface{}{map[string]interface{}{
		"from":  from,
		"to":    contract,
		"value": "0x2a",
		"gas":   "0x5208",
		"data":  "0x6057361d",
	}})
	if _, err := node.call("eth_sendTransaction", raw); err != nil {
		t.Fatalf("eth_sendTransaction failed: %v", err)
	}
	if node.state.Height != 2 {
		t.Fatalf("height = %d, want 2", node.state.Height)
	}

	latest := node.state.Blocks[len(node.state.Blocks)-1]
	foundState := false
	foundReceipts := false
	foundStorage := false
	for _, commit := range node.trieCommits {
		if strings.EqualFold(commit.Root, latest.StateRoot) && commit.Kind == "state" {
			if commit.Height != 2 {
				t.Fatalf("state trie commit height = %d, want 2", commit.Height)
			}
			foundState = true
		}
		if strings.EqualFold(commit.Root, latest.ReceiptRoot) && commit.Kind == "receipts" {
			if commit.Height != 2 {
				t.Fatalf("receipt trie commit height = %d, want 2", commit.Height)
			}
			foundReceipts = true
		}
		if commit.Height == 2 && strings.HasPrefix(commit.Kind, "storage:") {
			foundStorage = true
		}
	}
	if !foundState || !foundReceipts || !foundStorage {
		t.Fatalf("missing trie commits: state=%v receipts=%v storage=%v commits=%#v", foundState, foundReceipts, foundStorage, node.trieCommits)
	}

	node.save()
	reloaded, err := newRPCNode([]string{"--datadir", node.dataDir, "--dev-insecure"})
	if err != nil {
		t.Fatalf("reload node failed: %v", err)
	}
	if len(reloaded.trieCommits) != len(node.trieCommits) {
		t.Fatalf("reloaded trie commits = %d, want %d", len(reloaded.trieCommits), len(node.trieCommits))
	}
}

func TestReceiptProofRoundTrip(t *testing.T) {
	node := newTestNode(t)
	from := node.state.Accounts[0]

	raw, _ := json.Marshal([]interface{}{map[string]interface{}{
		"from":  from,
		"to":    node.bridgeAddress,
		"value": "0x1",
		"gas":   "0x5208",
		"data":  "withdraw",
	}})
	txResult, err := node.call("eth_sendTransaction", raw)
	if err != nil {
		t.Fatalf("eth_sendTransaction failed: %v", err)
	}
	txHash := txResult.(string)

	receipt := node.state.Receipts[strings.ToLower(txHash)]
	if receipt.LogsBloom == zeroHash() {
		t.Fatal("receipt logsBloom must be populated")
	}
	latest := node.state.Blocks[len(node.state.Blocks)-1]
	if latest.LogsBloom == zeroHash() {
		t.Fatal("block logsBloom must be populated")
	}

	proofRaw, _ := json.Marshal([]interface{}{txHash})
	proofResult, err := node.call("bhte_getReceiptProof", proofRaw)
	if err != nil {
		t.Fatalf("bhte_getReceiptProof failed: %v", err)
	}
	proof := proofResult.(map[string]interface{})
	if proof["receiptRoot"] != latest.ReceiptRoot {
		t.Fatalf("proof receiptRoot = %v, want %s", proof["receiptRoot"], latest.ReceiptRoot)
	}
	if len(proof["proof"].([]string)) == 0 {
		t.Fatal("receipt proof nodes were not populated")
	}

	verifyRaw, _ := json.Marshal([]interface{}{proof})
	verifyResult, err := node.call("bhte_verifyReceiptProof", verifyRaw)
	if err != nil {
		t.Fatalf("bhte_verifyReceiptProof failed: %v", err)
	}
	verification := verifyResult.(map[string]interface{})
	if verification["valid"] != true {
		t.Fatalf("receipt proof did not verify: %#v", verification)
	}

	proof["receiptRoot"] = zeroHash()
	verifyRaw, _ = json.Marshal([]interface{}{proof})
	verifyResult, err = node.call("bhte_verifyReceiptProof", verifyRaw)
	if err != nil {
		t.Fatalf("tampered bhte_verifyReceiptProof failed: %v", err)
	}
	verification = verifyResult.(map[string]interface{})
	if verification["valid"] != false {
		t.Fatalf("tampered receipt proof verified: %#v", verification)
	}
}
