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

func TestLogProofRoundTrip(t *testing.T) {
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

	proofRaw, _ := json.Marshal([]interface{}{txHash, "0x0"})
	proofResult, err := node.call("bhte_getLogProof", proofRaw)
	if err != nil {
		t.Fatalf("bhte_getLogProof failed: %v", err)
	}
	proof := proofResult.(map[string]interface{})
	if proof["logIndex"] != "0x0" {
		t.Fatalf("logIndex = %v, want 0x0", proof["logIndex"])
	}

	verifyRaw, _ := json.Marshal([]interface{}{proof})
	verifyResult, err := node.call("bhte_verifyLogProof", verifyRaw)
	if err != nil {
		t.Fatalf("bhte_verifyLogProof failed: %v", err)
	}
	verification := verifyResult.(map[string]interface{})
	if verification["valid"] != true {
		t.Fatalf("log proof did not verify: %#v", verification)
	}

	logObj := proof["log"].(logRecord)
	logObj.Topics[0] = zeroHash()
	proof["log"] = logObj
	verifyRaw, _ = json.Marshal([]interface{}{proof})
	verifyResult, err = node.call("bhte_verifyLogProof", verifyRaw)
	if err != nil {
		t.Fatalf("tampered bhte_verifyLogProof failed: %v", err)
	}
	verification = verifyResult.(map[string]interface{})
	if verification["valid"] != false {
		t.Fatalf("tampered log proof verified: %#v", verification)
	}
}

func TestTrieDatabaseNodeLookupAndReceiptVerification(t *testing.T) {
	node := newTestNode(t)
	from := node.state.Accounts[0]

	raw, _ := json.Marshal([]interface{}{map[string]interface{}{
		"from":  from,
		"to":    node.bridgeAddress,
		"value": "0x3",
		"gas":   "0x5208",
		"data":  "withdraw",
	}})
	txResult, err := node.call("eth_sendTransaction", raw)
	if err != nil {
		t.Fatalf("eth_sendTransaction failed: %v", err)
	}
	txHash := txResult.(string)
	latest := node.state.Blocks[len(node.state.Blocks)-1]

	commitsRaw, _ := json.Marshal([]interface{}{"receipts", "0x2"})
	commitsResult, err := node.call("bhte_getTrieCommits", commitsRaw)
	if err != nil {
		t.Fatalf("bhte_getTrieCommits failed: %v", err)
	}
	commits := commitsResult.([]trieCommitRecord)
	if len(commits) == 0 {
		t.Fatal("receipt trie commit was not returned")
	}
	if commits[0].Root != latest.ReceiptRoot {
		t.Fatalf("receipt commit root = %s, want %s", commits[0].Root, latest.ReceiptRoot)
	}
	if len(commits[0].Nodes) == 0 {
		t.Fatal("receipt trie commit has no nodes")
	}

	nodeRaw, _ := json.Marshal([]interface{}{latest.ReceiptRoot, commits[0].Nodes[0].Hash})
	nodeResult, err := node.call("bhte_getTrieNode", nodeRaw)
	if err != nil {
		t.Fatalf("bhte_getTrieNode failed: %v", err)
	}
	nodeRecord := nodeResult.(map[string]interface{})
	if nodeRecord["blob"] == "" {
		t.Fatalf("trie node blob was empty: %#v", nodeRecord)
	}

	receipt := node.state.Receipts[strings.ToLower(txHash)]
	verifyRaw, _ := json.Marshal([]interface{}{map[string]interface{}{
		"receiptRoot":      latest.ReceiptRoot,
		"transactionIndex": "0x0",
		"receipt":          receipt,
	}})
	verifyResult, err := node.call("bhte_verifyReceiptInTrie", verifyRaw)
	if err != nil {
		t.Fatalf("bhte_verifyReceiptInTrie failed: %v", err)
	}
	verification := verifyResult.(map[string]interface{})
	if verification["valid"] != true {
		t.Fatalf("receipt did not verify from trie database: %#v", verification)
	}

	receipt.Status = "0x0"
	verifyRaw, _ = json.Marshal([]interface{}{map[string]interface{}{
		"receiptRoot":      latest.ReceiptRoot,
		"transactionIndex": "0x0",
		"receipt":          receipt,
	}})
	verifyResult, err = node.call("bhte_verifyReceiptInTrie", verifyRaw)
	if err != nil {
		t.Fatalf("tampered bhte_verifyReceiptInTrie failed: %v", err)
	}
	verification = verifyResult.(map[string]interface{})
	if verification["valid"] != false {
		t.Fatalf("tampered receipt verified from trie database: %#v", verification)
	}
}

func TestHistoricalAccountProofUsesStateSnapshot(t *testing.T) {
	node := newTestNode(t)
	from := node.state.Accounts[0]
	contract := "0x0000000000000000000000000000000000000203"
	slot := zeroHash()

	firstRaw, _ := json.Marshal([]interface{}{map[string]interface{}{
		"from":  from,
		"to":    contract,
		"value": "0x2a",
		"gas":   "0x5208",
		"data":  "0x6057361d",
	}})
	if _, err := node.call("eth_sendTransaction", firstRaw); err != nil {
		t.Fatalf("first eth_sendTransaction failed: %v", err)
	}
	block2 := node.state.Blocks[len(node.state.Blocks)-1]

	secondRaw, _ := json.Marshal([]interface{}{map[string]interface{}{
		"from":  from,
		"to":    contract,
		"value": "0x2b",
		"gas":   "0x5208",
		"data":  "0x6057361d",
	}})
	if _, err := node.call("eth_sendTransaction", secondRaw); err != nil {
		t.Fatalf("second eth_sendTransaction failed: %v", err)
	}

	historicalRaw, _ := json.Marshal([]interface{}{contract, []string{slot}, "0x2"})
	historicalResult, err := node.call("eth_getProof", historicalRaw)
	if err != nil {
		t.Fatalf("historical eth_getProof failed: %v", err)
	}
	historical := historicalResult.(map[string]interface{})
	if historical["blockNumber"] != "0x2" || historical["blockHash"] != block2.Hash {
		t.Fatalf("historical proof block context mismatch: %#v", historical)
	}
	if historical["stateRoot"] != block2.StateRoot {
		t.Fatalf("historical stateRoot = %v, want %s", historical["stateRoot"], block2.StateRoot)
	}
	historicalStorage := historical["storageProof"].([]interface{})[0].(map[string]interface{})
	if historicalStorage["value"] != "0x2a" {
		t.Fatalf("historical storage value = %v, want 0x2a", historicalStorage["value"])
	}

	latestRaw, _ := json.Marshal([]interface{}{contract, []string{slot}, "latest"})
	latestResult, err := node.call("eth_getProof", latestRaw)
	if err != nil {
		t.Fatalf("latest eth_getProof failed: %v", err)
	}
	latest := latestResult.(map[string]interface{})
	latestStorage := latest["storageProof"].([]interface{})[0].(map[string]interface{})
	if latestStorage["value"] != "0x2b" {
		t.Fatalf("latest storage value = %v, want 0x2b", latestStorage["value"])
	}

	node.save()
	reloaded, err := newRPCNode([]string{"--datadir", node.dataDir, "--dev-insecure"})
	if err != nil {
		t.Fatalf("reload node failed: %v", err)
	}
	reloadedResult, err := reloaded.call("eth_getProof", historicalRaw)
	if err != nil {
		t.Fatalf("reloaded historical eth_getProof failed: %v", err)
	}
	reloadedProof := reloadedResult.(map[string]interface{})
	reloadedStorage := reloadedProof["storageProof"].([]interface{})[0].(map[string]interface{})
	if reloadedStorage["value"] != "0x2a" {
		t.Fatalf("reloaded historical storage value = %v, want 0x2a", reloadedStorage["value"])
	}
}
