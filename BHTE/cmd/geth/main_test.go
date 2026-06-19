// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license

package main

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"
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

func TestReorgRollsBackStateProofIndexesAndPendingTransactions(t *testing.T) {
	node := newTestNode(t)
	from := node.state.Accounts[0]
	contract := "0x0000000000000000000000000000000000000204"
	slot := zeroHash()

	block2Raw, _ := json.Marshal([]interface{}{map[string]interface{}{
		"from":  from,
		"to":    contract,
		"value": "0x2a",
		"gas":   "0x5208",
		"data":  "0x6057361d",
	}})
	if _, err := node.call("eth_sendTransaction", block2Raw); err != nil {
		t.Fatalf("block2 transaction failed: %v", err)
	}
	block2 := node.state.Blocks[len(node.state.Blocks)-1]

	block3Raw, _ := json.Marshal([]interface{}{map[string]interface{}{
		"from":  from,
		"to":    contract,
		"value": "0x2b",
		"gas":   "0x5208",
		"data":  "0x6057361d",
	}})
	txResult, err := node.call("eth_sendTransaction", block3Raw)
	if err != nil {
		t.Fatalf("block3 transaction failed: %v", err)
	}
	block3TxHash := txResult.(string)
	block3 := node.state.Blocks[len(node.state.Blocks)-1]
	if _, ok := node.state.StateSnapshots[3]; !ok {
		t.Fatal("expected block 3 snapshot before reorg")
	}

	reorgRaw, _ := json.Marshal([]interface{}{"0x3", "0xabc"})
	reorgResult, err := node.call("bhte_handleReorg", reorgRaw)
	if err != nil {
		t.Fatalf("bhte_handleReorg failed: %v", err)
	}
	reorg := reorgResult.(map[string]interface{})
	if reorg["reorg"] != true {
		t.Fatalf("expected reorg result, got %#v", reorg)
	}
	if node.state.Height != 2 {
		t.Fatalf("height after reorg = %d, want 2", node.state.Height)
	}
	if len(node.state.Blocks) == 0 || node.state.Blocks[len(node.state.Blocks)-1].Hash != block2.Hash {
		t.Fatalf("latest block after reorg is not block2: %#v", node.state.Blocks)
	}
	if _, ok := node.state.StateSnapshots[3]; ok {
		t.Fatal("block 3 snapshot survived reorg")
	}
	if _, ok := node.state.Receipts[strings.ToLower(block3TxHash)]; ok {
		t.Fatal("block 3 receipt survived reorg")
	}
	if len(node.state.PendingTxs) != 1 || node.state.PendingTxs[0].Hash != block3TxHash {
		t.Fatalf("removed transaction was not requeued: %#v", node.state.PendingTxs)
	}
	for _, commit := range node.trieCommits {
		if commit.Height >= 3 {
			t.Fatalf("trie commit at removed height survived: %#v", commit)
		}
	}

	latestRaw, _ := json.Marshal([]interface{}{contract, []string{slot}, "latest"})
	latestResult, err := node.call("eth_getProof", latestRaw)
	if err != nil {
		t.Fatalf("latest eth_getProof after reorg failed: %v", err)
	}
	latest := latestResult.(map[string]interface{})
	if latest["stateRoot"] != block2.StateRoot {
		t.Fatalf("stateRoot after reorg = %v, want %s", latest["stateRoot"], block2.StateRoot)
	}
	latestStorage := latest["storageProof"].([]interface{})[0].(map[string]interface{})
	if latestStorage["value"] != "0x2a" {
		t.Fatalf("storage value after reorg = %v, want 0x2a", latestStorage["value"])
	}

	block3ProofRaw, _ := json.Marshal([]interface{}{contract, []string{slot}, block3.Number})
	if _, err := node.call("eth_getProof", block3ProofRaw); err == nil {
		t.Fatal("expected block 3 proof to fail after reorg")
	}
}

func TestSyncPeerImportsValidatedBlockRange(t *testing.T) {
	node := newTestNode(t)
	peerNode := newTestNode(t)
	mineTransfer(t, peerNode, "0x0000000000000000000000000000000000000300", "0x1")
	mineTransfer(t, peerNode, "0x0000000000000000000000000000000000000301", "0x2")
	block2 := peerNode.state.Blocks[1]
	block3 := peerNode.state.Blocks[2]
	server := newPeerRPCServer(t, map[string]interface{}{
		"eth_blockNumber":      "0x3",
		"eth_getBlockByNumber": map[string]blockRecord{"0x2": block2, "0x3": block3},
	})
	defer server.Close()

	raw, _ := json.Marshal([]interface{}{server.URL})
	result, err := node.call("bhte_syncPeer", raw)
	if err != nil {
		t.Fatalf("bhte_syncPeer failed: %v", err)
	}
	sync := result.(map[string]interface{})
	if sync["synced"] != true || sync["imported"] != 2 {
		t.Fatalf("unexpected sync result: %#v", sync)
	}
	if node.state.Height != 3 {
		t.Fatalf("height after sync = %d, want 3", node.state.Height)
	}
	if node.state.Canonical[2] != block2.Hash || node.state.Canonical[3] != block3.Hash {
		t.Fatalf("canonical chain not imported: %#v", node.state.Canonical)
	}
	if len(node.state.Txs) != 2 {
		t.Fatalf("imported tx count = %d, want 2", len(node.state.Txs))
	}
	peerID := hashHex([]byte(server.URL))
	peer := node.state.Peers[peerID]
	if peer.Score != peerScoreMax || peer.Failures != 0 || peer.BannedUntil != 0 {
		t.Fatalf("healthy peer score not retained: %#v", peer)
	}
	snapshot, ok := node.state.StateSnapshots[3]
	if !ok {
		t.Fatal("peer-imported block snapshot was not recorded")
	}
	if !snapshot.Complete {
		t.Fatalf("replayed peer snapshot marked incomplete: %#v", snapshot)
	}
	if snapshot.Source != "local" {
		t.Fatalf("replayed peer snapshot source = %q, want local", snapshot.Source)
	}
	proofRaw, _ := json.Marshal([]interface{}{node.state.Accounts[0], []string{}, "latest"})
	if _, err := node.call("eth_getProof", proofRaw); err != nil {
		t.Fatalf("eth_getProof on replayed peer block failed: %v", err)
	}
}

func TestSyncPeerRejectsInvalidStateRoot(t *testing.T) {
	node := newTestNode(t)
	peerNode := newTestNode(t)
	mineTransfer(t, peerNode, "0x0000000000000000000000000000000000000300", "0x1")
	block2 := peerNode.state.Blocks[1]
	block2.StateRoot = hashHex([]byte("wrong-state-root"))
	server := newPeerRPCServer(t, map[string]interface{}{
		"eth_blockNumber":      "0x2",
		"eth_getBlockByNumber": map[string]blockRecord{"0x2": block2},
	})
	defer server.Close()

	raw, _ := json.Marshal([]interface{}{server.URL})
	result, err := node.call("bhte_syncPeer", raw)
	if err != nil {
		t.Fatalf("bhte_syncPeer returned RPC error: %v", err)
	}
	sync := result.(map[string]interface{})
	if sync["synced"] != false {
		t.Fatalf("invalid state root peer sync succeeded: %#v", sync)
	}
	if node.state.Height != 1 {
		t.Fatalf("height after failed sync = %d, want 1", node.state.Height)
	}
	if len(node.state.Txs) != 0 || len(node.state.Logs) != 0 {
		t.Fatalf("failed replay left tx/log state behind: txs=%d logs=%d", len(node.state.Txs), len(node.state.Logs))
	}
}

func TestSyncPeerRejectsInvalidParent(t *testing.T) {
	node := newTestNode(t)
	badBlock := blockRecord{
		Number:       "0x2",
		Hash:         hashHex([]byte("bad-peer-block-2")),
		ParentHash:   hashHex([]byte("wrong-parent")),
		StateRoot:    hashHex([]byte("bad-peer-state-2")),
		TxRoot:       hashJSON([]txRecord{}),
		ReceiptRoot:  hashHex([]byte("bad-peer-receipts-2")),
		LogsBloom:    zeroHash(),
		Timestamp:    "0x2",
		Transactions: []txRecord{},
	}
	server := newPeerRPCServer(t, map[string]interface{}{
		"eth_blockNumber":      "0x2",
		"eth_getBlockByNumber": map[string]blockRecord{"0x2": badBlock},
	})
	defer server.Close()

	raw, _ := json.Marshal([]interface{}{server.URL})
	result, err := node.call("bhte_syncPeer", raw)
	if err != nil {
		t.Fatalf("bhte_syncPeer returned RPC error: %v", err)
	}
	sync := result.(map[string]interface{})
	if sync["synced"] != false {
		t.Fatalf("invalid parent peer sync succeeded: %#v", sync)
	}
	if node.state.Height != 1 {
		t.Fatalf("height after failed sync = %d, want 1", node.state.Height)
	}
}

func TestSyncPeerScoresAndBansInvalidPeer(t *testing.T) {
	node := newTestNode(t)
	badBlock := blockRecord{
		Number:       "0x2",
		Hash:         hashHex([]byte("repeat-bad-peer-block-2")),
		ParentHash:   hashHex([]byte("wrong-parent")),
		StateRoot:    hashHex([]byte("repeat-bad-peer-state-2")),
		TxRoot:       hashJSON([]txRecord{}),
		ReceiptRoot:  hashHex([]byte("repeat-bad-peer-receipts-2")),
		LogsBloom:    zeroHash(),
		Timestamp:    "0x2",
		Transactions: []txRecord{},
	}
	server := newPeerRPCServer(t, map[string]interface{}{
		"eth_blockNumber":      "0x2",
		"eth_getBlockByNumber": map[string]blockRecord{"0x2": badBlock},
	})
	defer server.Close()

	raw, _ := json.Marshal([]interface{}{server.URL})
	for i := 0; i < 4; i++ {
		result, err := node.call("bhte_syncPeer", raw)
		if err != nil {
			t.Fatalf("bhte_syncPeer returned RPC error: %v", err)
		}
		sync := result.(map[string]interface{})
		if sync["synced"] != false {
			t.Fatalf("invalid peer sync %d succeeded: %#v", i, sync)
		}
	}
	peer := node.state.Peers[hashHex([]byte(server.URL))]
	if peer.Score != 0 || peer.Failures != 4 || peer.BannedUntil <= time.Now().Unix() {
		t.Fatalf("invalid peer was not banned after repeated failures: %#v", peer)
	}
	result, err := node.call("bhte_syncPeer", raw)
	if err != nil {
		t.Fatalf("banned bhte_syncPeer returned RPC error: %v", err)
	}
	sync := result.(map[string]interface{})
	if sync["synced"] != false || sync["error"] != "peer is temporarily banned" {
		t.Fatalf("banned peer sync did not short-circuit: %#v", sync)
	}
}

func TestValidateBlockReplaysCandidateWithoutImport(t *testing.T) {
	node := newTestNode(t)
	peerNode := newTestNode(t)
	mineTransfer(t, peerNode, "0x0000000000000000000000000000000000000300", "0x1")
	candidate := peerNode.state.Blocks[1]
	originalHeight := node.state.Height
	originalTxCount := len(node.state.Txs)

	result, err := node.call("bhte_validateBlock", mustBlockParams(t, candidate))
	if err != nil {
		t.Fatalf("bhte_validateBlock failed: %v", err)
	}
	validation := result.(map[string]interface{})
	if validation["valid"] != true {
		t.Fatalf("candidate block validation failed: %#v", validation)
	}
	computed := validation["computed"].(map[string]string)
	if computed["stateRoot"] != candidate.StateRoot {
		t.Fatalf("computed stateRoot = %s, want %s", computed["stateRoot"], candidate.StateRoot)
	}
	if node.state.Height != originalHeight || len(node.state.Txs) != originalTxCount {
		t.Fatalf("validation imported/mutated state: height=%d txs=%d", node.state.Height, len(node.state.Txs))
	}
}

func TestValidateBlockRejectsBadCommitmentWithoutMutation(t *testing.T) {
	node := newTestNode(t)
	peerNode := newTestNode(t)
	mineTransfer(t, peerNode, "0x0000000000000000000000000000000000000300", "0x1")
	candidate := peerNode.state.Blocks[1]
	candidate.StateRoot = hashHex([]byte("bad-candidate-state"))
	originalHeight := node.state.Height
	originalTxCount := len(node.state.Txs)

	result, err := node.call("bhte_validateBlock", mustBlockParams(t, candidate))
	if err != nil {
		t.Fatalf("bhte_validateBlock returned RPC error: %v", err)
	}
	validation := result.(map[string]interface{})
	if validation["valid"] != false {
		t.Fatalf("bad candidate block validation succeeded: %#v", validation)
	}
	if validation["field"] != "stateRoot" {
		t.Fatalf("failure field = %v, want stateRoot", validation["field"])
	}
	if node.state.Height != originalHeight || len(node.state.Txs) != originalTxCount {
		t.Fatalf("failed validation mutated state: height=%d txs=%d", node.state.Height, len(node.state.Txs))
	}
}

func TestReplayChainVerifiesCanonicalBlocks(t *testing.T) {
	node := newTestNode(t)
	mineTransfer(t, node, "0x0000000000000000000000000000000000000300", "0x1")
	mineTransfer(t, node, "0x0000000000000000000000000000000000000301", "0x2")
	originalHeight := node.state.Height
	originalTxCount := len(node.state.Txs)
	originalLogCount := len(node.state.Logs)

	result, err := node.call("bhte_replayChain", nil)
	if err != nil {
		t.Fatalf("bhte_replayChain failed: %v", err)
	}
	replay := result.(map[string]interface{})
	if replay["valid"] != true {
		t.Fatalf("chain replay failed: %#v", replay)
	}
	if replay["checked"] != 2 {
		t.Fatalf("checked blocks = %v, want 2", replay["checked"])
	}
	if node.state.Height != originalHeight || len(node.state.Txs) != originalTxCount || len(node.state.Logs) != originalLogCount {
		t.Fatalf("replay mutated node state: height=%d txs=%d logs=%d", node.state.Height, len(node.state.Txs), len(node.state.Logs))
	}
}

func TestReplayChainRejectsCorruptStateRootWithoutMutation(t *testing.T) {
	node := newTestNode(t)
	mineTransfer(t, node, "0x0000000000000000000000000000000000000300", "0x1")
	originalRoot := node.state.Blocks[1].StateRoot
	originalHeight := node.state.Height
	originalTxCount := len(node.state.Txs)
	node.state.Blocks[1].StateRoot = hashHex([]byte("corrupt-state-root"))
	defer func() {
		node.state.Blocks[1].StateRoot = originalRoot
	}()

	result, err := node.call("bhte_replayChain", nil)
	if err != nil {
		t.Fatalf("bhte_replayChain returned RPC error: %v", err)
	}
	replay := result.(map[string]interface{})
	if replay["valid"] != false {
		t.Fatalf("corrupt chain replay succeeded: %#v", replay)
	}
	if replay["field"] != "stateRoot" {
		t.Fatalf("failure field = %v, want stateRoot", replay["field"])
	}
	if node.state.Height != originalHeight || len(node.state.Txs) != originalTxCount {
		t.Fatalf("failed replay mutated node state: height=%d txs=%d", node.state.Height, len(node.state.Txs))
	}
}

func mineTransfer(t *testing.T, node *rpcNode, to, value string) {
	t.Helper()
	raw, _ := json.Marshal([]interface{}{map[string]interface{}{
		"from":  node.state.Accounts[0],
		"to":    to,
		"value": value,
		"gas":   "0x5208",
	}})
	if _, err := node.call("eth_sendTransaction", raw); err != nil {
		t.Fatalf("eth_sendTransaction failed: %v", err)
	}
}

func mustBlockParams(t *testing.T, block blockRecord) json.RawMessage {
	t.Helper()
	var asMap map[string]interface{}
	data, err := json.Marshal(block)
	if err != nil {
		t.Fatalf("marshal block: %v", err)
	}
	if err := json.Unmarshal(data, &asMap); err != nil {
		t.Fatalf("unmarshal block map: %v", err)
	}
	raw, err := json.Marshal([]interface{}{asMap})
	if err != nil {
		t.Fatalf("marshal block params: %v", err)
	}
	return raw
}

func newPeerRPCServer(t *testing.T, responses map[string]interface{}) *httptest.Server {
	t.Helper()
	return httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		var req struct {
			ID     interface{}       `json:"id"`
			Method string            `json:"method"`
			Params []json.RawMessage `json:"params"`
		}
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			t.Fatalf("decode peer rpc request: %v", err)
		}
		var result interface{}
		switch req.Method {
		case "eth_blockNumber":
			result = responses[req.Method]
		case "eth_getBlockByNumber":
			var number string
			if len(req.Params) == 0 {
				t.Fatal("eth_getBlockByNumber missing number param")
			}
			if err := json.Unmarshal(req.Params[0], &number); err != nil {
				t.Fatalf("decode block number: %v", err)
			}
			blocks := responses[req.Method].(map[string]blockRecord)
			block, ok := blocks[number]
			if !ok {
				t.Fatalf("missing peer block %s", number)
			}
			result = block
		default:
			t.Fatalf("unexpected peer rpc method %s", req.Method)
		}
		w.Header().Set("Content-Type", "application/json")
		_ = json.NewEncoder(w).Encode(map[string]interface{}{
			"jsonrpc": "2.0",
			"id":      req.ID,
			"result":  result,
		})
	}))
}
