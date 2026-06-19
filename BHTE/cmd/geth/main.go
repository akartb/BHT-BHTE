// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// BHTE zkEVM Main Entry

package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"math/big"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"

	"github.com/ethereum/go-ethereum/common"
	"github.com/ethereum/go-ethereum/core/types"
	"github.com/ethereum/go-ethereum/crypto"
	"github.com/ethereum/go-ethereum/log"
	"github.com/ethereum/go-ethereum/rlp"
	"github.com/ethereum/go-ethereum/trie"
	"github.com/ethereum/go-ethereum/trie/trienode"
	"github.com/holiman/uint256"
)

var (
	Version   = "1.0.0"
	GitCommit = ""
	BuildTime = ""
)

const maxPeerSyncBlocks = 128

func main() {
	fmt.Println("BHTE zkEVM - Layer 2 Payment Engine")
	fmt.Printf("Version: %s\n", Version)
	fmt.Printf("Build: %s (%s)\n", GitCommit, BuildTime)
	fmt.Println()

	if len(os.Args) > 1 && os.Args[1] == "--version" {
		return
	}

	if err := run(); err != nil {
		fmt.Fprintf(os.Stderr, "Fatal: %v\n", err)
		os.Exit(1)
	}
}

func run() error {
	log.Info("BHTE starting...")

	node, err := newRPCNode(os.Args[1:])
	if err != nil {
		return err
	}
	if err := node.start(); err != nil {
		return err
	}
	defer node.stop()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)

	<-quit

	log.Info("BHTE shutting down...")
	return nil
}

type rpcRequest struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id"`
	Method  string          `json:"method"`
	Params  json.RawMessage `json:"params"`
}

type rpcResponse struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id"`
	Result  interface{}     `json:"result,omitempty"`
	Error   *rpcError       `json:"error,omitempty"`
}

type rpcError struct {
	Code    int    `json:"code"`
	Message string `json:"message"`
}

type txRecord struct {
	Hash             string `json:"hash"`
	From             string `json:"from"`
	To               string `json:"to"`
	Value            string `json:"value"`
	Nonce            string `json:"nonce,omitempty"`
	Gas              string `json:"gas,omitempty"`
	Input            string `json:"input,omitempty"`
	BlockHash        string `json:"blockHash,omitempty"`
	BlockNumber      string `json:"blockNumber,omitempty"`
	TransactionIndex string `json:"transactionIndex,omitempty"`
	Time             int64  `json:"time"`
	Timestamp        int64  `json:"timestamp"`
	Confirmations    int    `json:"confirmations"`
	Fee              string `json:"fee"`
	Raw              string `json:"raw,omitempty"`
}

type blockRecord struct {
	Number       string     `json:"number"`
	Hash         string     `json:"hash"`
	ParentHash   string     `json:"parentHash"`
	StateRoot    string     `json:"stateRoot"`
	TxRoot       string     `json:"transactionsRoot"`
	ReceiptRoot  string     `json:"receiptsRoot"`
	LogsBloom    string     `json:"logsBloom"`
	Timestamp    string     `json:"timestamp"`
	Transactions []txRecord `json:"transactions"`
}

type logRecord struct {
	Address          string   `json:"address"`
	Topics           []string `json:"topics"`
	Data             string   `json:"data"`
	BlockNumber      string   `json:"blockNumber"`
	BlockHash        string   `json:"blockHash"`
	TransactionHash  string   `json:"transactionHash"`
	TransactionIndex string   `json:"transactionIndex"`
	LogIndex         string   `json:"logIndex"`
	Removed          bool     `json:"removed"`
}

type receiptRecord struct {
	TransactionHash   string      `json:"transactionHash"`
	TransactionIndex  string      `json:"transactionIndex"`
	BlockHash         string      `json:"blockHash"`
	BlockNumber       string      `json:"blockNumber"`
	From              string      `json:"from"`
	To                string      `json:"to"`
	CumulativeGasUsed string      `json:"cumulativeGasUsed"`
	GasUsed           string      `json:"gasUsed"`
	ContractAddress   interface{} `json:"contractAddress"`
	Logs              []logRecord `json:"logs"`
	LogsBloom         string      `json:"logsBloom"`
	Status            string      `json:"status"`
	Type              string      `json:"type"`
}

type withdrawalRecord struct {
	ID                string `json:"id"`
	L2TxHash          string `json:"l2TxHash"`
	Recipient         string `json:"recipient"`
	Amount            string `json:"amount"`
	ChallengeDeadline int64  `json:"challengeDeadline"`
	Processed         bool   `json:"processed"`
	Challenged        bool   `json:"challenged"`
	BHTTxHash         string `json:"bhtTxHash,omitempty"`
	CreatedAt         int64  `json:"createdAt"`
}

type anchorRecord struct {
	Height    uint64 `json:"height"`
	StateRoot string `json:"stateRoot"`
	BlockHash string `json:"blockHash"`
	TxHash    string `json:"txHash"`
	Timestamp int64  `json:"timestamp"`
	Verified  bool   `json:"verified"`
}

type peerRecord struct {
	ID       string `json:"id"`
	URL      string `json:"url"`
	Height   uint64 `json:"height"`
	BestHash string `json:"bestHash"`
	LastSeen int64  `json:"lastSeen"`
	Trusted  bool   `json:"trusted"`
}

type trieNodeRecord struct {
	Hash    string `json:"hash"`
	Path    string `json:"path"`
	Blob    string `json:"blob"`
	Deleted bool   `json:"deleted"`
}

type trieCommitRecord struct {
	Root      string           `json:"root"`
	Kind      string           `json:"kind"`
	Height    uint64           `json:"height"`
	Nodes     []trieNodeRecord `json:"nodes"`
	Timestamp int64            `json:"timestamp"`
}

type stateSnapshotRecord struct {
	Height    uint64                       `json:"height"`
	BlockHash string                       `json:"blockHash"`
	StateRoot string                       `json:"stateRoot"`
	Complete  bool                         `json:"complete"`
	Source    string                       `json:"source,omitempty"`
	Balances  map[string]string            `json:"balances"`
	Nonces    map[string]uint64            `json:"nonces"`
	Code      map[string]string            `json:"code"`
	Storage   map[string]map[string]string `json:"storage"`
	Timestamp int64                        `json:"timestamp"`
}

type nodeState struct {
	Height         uint64                         `json:"height"`
	Accounts       []string                       `json:"accounts"`
	Balances       map[string]string              `json:"balances"`
	Nonces         map[string]uint64              `json:"nonces"`
	PendingTxs     []txRecord                     `json:"pendingTransactions"`
	Txs            []txRecord                     `json:"transactions"`
	Blocks         []blockRecord                  `json:"blocks"`
	Receipts       map[string]receiptRecord       `json:"receipts"`
	Logs           []logRecord                    `json:"logs"`
	Withdrawals    map[string]withdrawalRecord    `json:"withdrawals"`
	Anchors        map[uint64]anchorRecord        `json:"anchors"`
	Canonical      map[uint64]string              `json:"canonical"`
	Code           map[string]string              `json:"code"`
	Storage        map[string]map[string]string   `json:"storage"`
	StateSnapshots map[uint64]stateSnapshotRecord `json:"stateSnapshots"`
	Peers          map[string]peerRecord          `json:"peers"`
}

type rpcNode struct {
	mu               sync.Mutex
	addr             string
	port             string
	dataDir          string
	state            nodeState
	server           *http.Server
	stateFile        string
	trieFile         string
	trieCommits      []trieCommitRecord
	bridgeAddress    string
	bhtRPC           string
	bhtRPCUser       string
	bhtRPCPass       string
	autoMine         bool
	strictMode       bool
	replayMode       bool
	minGasLimit      uint64
	maxGasLimit      uint64
	challengeSeconds int64
}

func newRPCNode(args []string) (*rpcNode, error) {
	node := &rpcNode{
		addr:             "127.0.0.1",
		port:             "8545",
		dataDir:          filepath.Join(os.TempDir(), "bhte"),
		bridgeAddress:    "0x0000000000000000000000000000000000000200",
		bhtRPC:           "http://127.0.0.1:18332",
		autoMine:         true,
		strictMode:       true,
		minGasLimit:      21000,
		maxGasLimit:      30000000,
		challengeSeconds: 7 * 24 * 60 * 60,
		state: nodeState{
			Height:         1,
			Accounts:       []string{"0x0000000000000000000000000000000000000100"},
			Balances:       map[string]string{"0x0000000000000000000000000000000000000100": "0x3635c9adc5dea00000"},
			Nonces:         map[string]uint64{},
			PendingTxs:     []txRecord{},
			Txs:            []txRecord{},
			Blocks:         []blockRecord{},
			Receipts:       map[string]receiptRecord{},
			Logs:           []logRecord{},
			Withdrawals:    map[string]withdrawalRecord{},
			Anchors:        map[uint64]anchorRecord{},
			Canonical:      map[uint64]string{},
			Code:           map[string]string{},
			Storage:        map[string]map[string]string{},
			StateSnapshots: map[uint64]stateSnapshotRecord{},
			Peers:          map[string]peerRecord{},
		},
	}

	for i := 0; i < len(args); i++ {
		switch args[i] {
		case "--http.addr":
			if i+1 < len(args) {
				i++
				node.addr = args[i]
			}
		case "--http.port":
			if i+1 < len(args) {
				i++
				node.port = args[i]
			}
		case "--datadir":
			if i+1 < len(args) {
				i++
				node.dataDir = args[i]
			}
		case "--account":
			if i+1 < len(args) {
				i++
				node.state.Accounts = []string{strings.ToLower(args[i])}
			}
		case "--bridge.address":
			if i+1 < len(args) {
				i++
				node.bridgeAddress = normalizeAddress(args[i])
			}
		case "--bht.rpc":
			if i+1 < len(args) {
				i++
				node.bhtRPC = args[i]
			}
		case "--bht.rpcuser":
			if i+1 < len(args) {
				i++
				node.bhtRPCUser = args[i]
			}
		case "--bht.rpcpass":
			if i+1 < len(args) {
				i++
				node.bhtRPCPass = args[i]
			}
		case "--dev-insecure":
			node.strictMode = false
		case "--no-automine":
			node.autoMine = false
		}
	}

	if err := os.MkdirAll(node.dataDir, 0o755); err != nil {
		return nil, err
	}
	node.stateFile = filepath.Join(node.dataDir, "bhte_state.json")
	node.trieFile = filepath.Join(node.dataDir, "bhte_trie_nodes.json")
	node.load()
	node.loadTrieDB()
	node.ensureState()
	return node, nil
}

func (n *rpcNode) start() error {
	mux := http.NewServeMux()
	mux.HandleFunc("/", n.handleRPC)
	n.server = &http.Server{Addr: n.addr + ":" + n.port, Handler: mux}
	go func() {
		if err := n.server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Error("BHTE RPC server failed", "err", err)
		}
	}()
	log.Info("BHTE RPC listening", "addr", n.server.Addr)
	return nil
}

func (n *rpcNode) stop() {
	if n.server != nil {
		_ = n.server.Close()
	}
	n.save()
}

func (n *rpcNode) load() {
	data, err := os.ReadFile(n.stateFile)
	if err != nil {
		return
	}
	var state nodeState
	if json.Unmarshal(data, &state) == nil && len(state.Accounts) > 0 {
		n.state = state
	}
}

func (n *rpcNode) loadTrieDB() {
	data, err := os.ReadFile(n.trieFile)
	if err != nil {
		return
	}
	_ = json.Unmarshal(data, &n.trieCommits)
}

func (n *rpcNode) ensureState() {
	if n.state.Height == 0 {
		n.state.Height = 1
	}
	if len(n.state.Accounts) == 0 {
		n.state.Accounts = []string{"0x0000000000000000000000000000000000000100"}
	}
	for i, account := range n.state.Accounts {
		n.state.Accounts[i] = normalizeAddress(account)
	}
	if n.state.Balances == nil {
		n.state.Balances = map[string]string{}
	}
	if n.state.Nonces == nil {
		n.state.Nonces = map[string]uint64{}
	}
	if n.state.PendingTxs == nil {
		n.state.PendingTxs = []txRecord{}
	}
	if n.state.Txs == nil {
		n.state.Txs = []txRecord{}
	}
	if n.state.Blocks == nil {
		n.state.Blocks = []blockRecord{}
	}
	if n.state.Receipts == nil {
		n.state.Receipts = map[string]receiptRecord{}
	}
	if n.state.Logs == nil {
		n.state.Logs = []logRecord{}
	}
	if n.state.Withdrawals == nil {
		n.state.Withdrawals = map[string]withdrawalRecord{}
	}
	if n.state.Anchors == nil {
		n.state.Anchors = map[uint64]anchorRecord{}
	}
	if n.state.Canonical == nil {
		n.state.Canonical = map[uint64]string{}
	}
	if n.state.Code == nil {
		n.state.Code = map[string]string{}
	}
	if n.state.Storage == nil {
		n.state.Storage = map[string]map[string]string{}
	}
	if n.state.StateSnapshots == nil {
		n.state.StateSnapshots = map[uint64]stateSnapshotRecord{}
	}
	for height, snapshot := range n.state.StateSnapshots {
		if !snapshot.Complete && snapshot.Source == "" && snapshot.Balances != nil {
			snapshot.Complete = true
			snapshot.Source = "legacy-local"
			n.state.StateSnapshots[height] = snapshot
		}
	}
	if n.state.Peers == nil {
		n.state.Peers = map[string]peerRecord{}
	}
	if len(n.state.Blocks) == 0 {
		n.state.Blocks = append(n.state.Blocks, n.genesisBlock())
	}
	for _, block := range n.state.Blocks {
		n.state.Canonical[parseQuantity(block.Number).Uint64()] = block.Hash
	}
	for _, account := range n.state.Accounts {
		if _, ok := n.state.Balances[account]; !ok {
			n.state.Balances[account] = "0x3635c9adc5dea00000"
		}
	}
	if len(n.state.Blocks) > 0 {
		latest := n.state.Blocks[len(n.state.Blocks)-1]
		height := parseQuantity(latest.Number).Uint64()
		if _, ok := n.state.StateSnapshots[height]; !ok {
			n.storeStateSnapshot(height, latest.Hash, latest.StateRoot)
		}
	}
}

func (n *rpcNode) save() {
	data, err := json.MarshalIndent(n.state, "", "  ")
	if err == nil {
		_ = os.WriteFile(n.stateFile, data, 0o600)
	}
	if n.trieFile != "" {
		trieData, err := json.MarshalIndent(n.trieCommits, "", "  ")
		if err == nil {
			_ = os.WriteFile(n.trieFile, trieData, 0o600)
		}
	}
}

func (n *rpcNode) handleRPC(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
	var req rpcRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeRPC(w, rpcResponse{JSONRPC: "2.0", ID: nil, Error: &rpcError{Code: -32700, Message: err.Error()}})
		return
	}
	result, err := n.call(req.Method, req.Params)
	resp := rpcResponse{JSONRPC: "2.0", ID: req.ID, Result: result}
	if err != nil {
		resp.Result = nil
		resp.Error = &rpcError{Code: -32000, Message: err.Error()}
	}
	writeRPC(w, resp)
}

func writeRPC(w http.ResponseWriter, resp rpcResponse) {
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(resp)
}

func (n *rpcNode) call(method string, raw json.RawMessage) (interface{}, error) {
	n.mu.Lock()
	defer n.mu.Unlock()

	switch method {
	case "web3_clientVersion":
		return "BHTE/" + Version, nil
	case "eth_chainId":
		return "0x27b3", nil
	case "eth_blockNumber":
		return toHex(n.state.Height), nil
	case "eth_getBlockByNumber":
		params := parseParams(raw)
		block, ok := n.blockByNumber(stringParam(params, 0))
		if !ok {
			return nil, nil
		}
		if !boolParam(params, 1, false) {
			hashes := make([]string, 0, len(block.Transactions))
			for _, tx := range block.Transactions {
				hashes = append(hashes, tx.Hash)
			}
			asMap := structToMap(block)
			asMap["transactions"] = hashes
			return asMap, nil
		}
		return block, nil
	case "eth_getBlockByHash":
		params := parseParams(raw)
		block, ok := n.blockByHash(stringParam(params, 0))
		if !ok {
			return nil, nil
		}
		return block, nil
	case "net_peerCount":
		return toHex(uint64(len(n.state.Peers))), nil
	case "admin_addPeer":
		params := parseParams(raw)
		return n.addPeer(stringParam(params, 0)), nil
	case "admin_peers":
		return n.peerList(), nil
	case "bhte_syncPeer":
		params := parseParams(raw)
		return n.syncPeer(stringParam(params, 0)), nil
	case "bhte_consensusStatus":
		return n.consensusStatus(), nil
	case "bhte_validateBlock":
		params := parseParams(raw)
		block, err := objectParam(params, 0)
		if err != nil {
			return nil, err
		}
		return n.validateBlockObject(block), nil
	case "bhte_replayChain":
		return n.replayChain(), nil
	case "eth_accounts":
		return n.state.Accounts, nil
	case "eth_getBalance":
		params := parseParams(raw)
		addr := normalizeAddress(stringParam(params, 0))
		return "0x" + n.balance(addr).Text(16), nil
	case "eth_getTransactionCount":
		params := parseParams(raw)
		addr := normalizeAddress(stringParam(params, 0))
		return toHex(n.state.Nonces[addr]), nil
	case "eth_getTransactionByHash":
		params := parseParams(raw)
		tx, ok := n.transactionByHash(stringParam(params, 0))
		if !ok {
			return nil, nil
		}
		return tx, nil
	case "eth_getTransactionReceipt":
		params := parseParams(raw)
		receipt, ok := n.state.Receipts[strings.ToLower(stringParam(params, 0))]
		if !ok {
			return nil, nil
		}
		return receipt, nil
	case "eth_getCode":
		params := parseParams(raw)
		addr := normalizeAddress(stringParam(params, 0))
		return stringOrDefault(n.state.Code[addr], "0x"), nil
	case "eth_getStorageAt":
		params := parseParams(raw)
		addr := normalizeAddress(stringParam(params, 0))
		key := normalizeStorageKey(stringParam(params, 1))
		if n.state.Storage[addr] == nil {
			return zeroHash(), nil
		}
		return stringOrDefault(n.state.Storage[addr][key], zeroHash()), nil
	case "eth_call":
		params := parseParams(raw)
		tx, err := objectParam(params, 0)
		if err != nil {
			return nil, err
		}
		return n.callContract(tx), nil
	case "eth_getLogs":
		params := parseParams(raw)
		filter, _ := objectParam(params, 0)
		return n.filterLogs(filter), nil
	case "eth_estimateGas":
		return "0x5208", nil
	case "eth_getProof":
		params := parseParams(raw)
		addr := normalizeAddress(stringParam(params, 0))
		keys := stringSliceParam(params, 1)
		return n.accountProof(addr, keys, stringParam(params, 2))
	case "eth_sendTransaction":
		params := parseParams(raw)
		tx, err := objectParam(params, 0)
		if err != nil {
			return nil, err
		}
		if err := n.validateTransaction(tx); err != nil {
			return nil, err
		}
		return n.addPendingTransaction(tx, ""), nil
	case "eth_signTransaction":
		params := parseParams(raw)
		tx, err := objectParam(params, 0)
		if err != nil {
			return nil, err
		}
		encoded, _ := json.Marshal(tx)
		hash := hashHex(encoded)
		return map[string]string{
			"raw":  "0x" + hex.EncodeToString(encoded),
			"hash": hash,
			"tx":   hash,
		}, nil
	case "eth_sendRawTransaction":
		params := parseParams(raw)
		rawTx := stringParam(params, 0)
		return n.addRawTransaction(rawTx), nil
	case "txpool_status":
		return map[string]string{
			"pending": toHex(uint64(len(n.state.PendingTxs))),
			"queued":  "0x0",
		}, nil
	case "txpool_content":
		return n.txpoolContent(), nil
	case "bhte_mineBlock":
		block := n.minePendingBlock()
		return block, nil
	case "bhte_getTransactionsByAddress":
		params := parseParams(raw)
		addr := normalizeAddress(stringParam(params, 0))
		limit := intParam(params, 1, 100)
		return n.transactionsByAddress(addr, limit), nil
	case "bhte_getBridgeEvents":
		params := parseParams(raw)
		limit := intParam(params, 0, 100)
		return n.bridgeEvents(limit), nil
	case "bhte_submitBridgeEvent":
		params := parseParams(raw)
		event, err := objectParam(params, 0)
		if err != nil {
			return nil, err
		}
		return n.submitBridgeEvent(event), nil
	case "bhte_initiateWithdrawal":
		params := parseParams(raw)
		request, err := objectParam(params, 0)
		if err != nil {
			return nil, err
		}
		return n.initiateWithdrawal(request), nil
	case "bhte_processWithdrawal":
		params := parseParams(raw)
		return n.processWithdrawal(stringParam(params, 0))
	case "bhte_getWithdrawal":
		params := parseParams(raw)
		withdrawal, ok := n.state.Withdrawals[strings.ToLower(stringParam(params, 0))]
		if !ok {
			return nil, nil
		}
		return withdrawal, nil
	case "bhte_handleReorg":
		params := parseParams(raw)
		height := quantityParam(params, 0)
		newHash := stringParam(params, 1)
		return n.handleReorg(height, newHash), nil
	case "bhte_securityStatus":
		return n.securityStatus(), nil
	case "bhte_submitAnchor":
		params := parseParams(raw)
		anchor, err := objectParam(params, 0)
		if err != nil {
			return nil, err
		}
		return n.submitAnchor(anchor), nil
	case "bhte_verifyStateRoot":
		params := parseParams(raw)
		height := quantityParam(params, 0)
		stateRoot := stringParam(params, 1)
		proof := stringSliceParam(params, 2)
		return n.verifyStateRoot(height, stateRoot, proof), nil
	case "bhte_getTrieCommit":
		params := parseParams(raw)
		return n.getTrieCommit(stringParam(params, 0)), nil
	case "bhte_getTrieCommits":
		params := parseParams(raw)
		return n.getTrieCommits(stringParam(params, 0), quantityParam(params, 1)), nil
	case "bhte_getTrieNode":
		params := parseParams(raw)
		return n.getTrieNode(stringParam(params, 0), stringParam(params, 1))
	case "bhte_getReceiptProof":
		params := parseParams(raw)
		return n.receiptProof(stringParam(params, 0))
	case "bhte_verifyReceiptProof":
		params := parseParams(raw)
		proof, err := objectParam(params, 0)
		if err != nil {
			return nil, err
		}
		return n.verifyReceiptProof(proof), nil
	case "bhte_verifyReceiptInTrie":
		params := parseParams(raw)
		request, err := objectParam(params, 0)
		if err != nil {
			return nil, err
		}
		return n.verifyReceiptInTrie(request), nil
	case "bhte_getLogProof":
		params := parseParams(raw)
		return n.logProof(stringParam(params, 0), quantityParam(params, 1))
	case "bhte_verifyLogProof":
		params := parseParams(raw)
		proof, err := objectParam(params, 0)
		if err != nil {
			return nil, err
		}
		return n.verifyLogProof(proof), nil
	case "bhte_getProof":
		params := parseParams(raw)
		addr := normalizeAddress(stringParam(params, 0))
		keys := stringSliceParam(params, 1)
		return n.accountProof(addr, keys, stringParam(params, 2))
	default:
		return nil, fmt.Errorf("unsupported method %s", method)
	}
}

func (n *rpcNode) addPendingTransaction(tx map[string]interface{}, raw string) string {
	from := normalizeAddress(fmt.Sprint(tx["from"]))
	to := normalizeAddress(fmt.Sprint(tx["to"]))
	value := parseQuantity(fmt.Sprint(tx["value"]))
	nonce := n.state.Nonces[from]
	hashInput, _ := json.Marshal(map[string]interface{}{"from": from, "to": to, "value": value.String(), "nonce": nonce, "time": time.Now().UnixNano()})
	hash := hashHex(hashInput)
	now := time.Now().Unix()

	record := txRecord{
		Hash:      hash,
		From:      from,
		To:        to,
		Value:     "0x" + value.Text(16),
		Nonce:     toHex(nonce),
		Gas:       stringOrDefault(fmt.Sprint(tx["gas"]), "0x5208"),
		Input:     stringOrDefault(fmt.Sprint(tx["data"]), stringOrDefault(fmt.Sprint(tx["input"]), "0x")),
		Time:      now,
		Timestamp: now,
		Fee:       "0x0",
		Raw:       raw,
	}
	n.state.PendingTxs = append(n.state.PendingTxs, record)
	if n.autoMine {
		n.minePendingBlock()
	} else {
		n.save()
	}
	return hash
}

func (n *rpcNode) validateTransaction(tx map[string]interface{}) error {
	from := normalizeAddress(fmt.Sprint(tx["from"]))
	to := normalizeAddress(fmt.Sprint(tx["to"]))
	value := parseQuantity(fmt.Sprint(tx["value"]))
	gasLimit := quantityFromInterface(tx["gas"])
	if gasLimit == 0 {
		gasLimit = n.minGasLimit
	}
	if !common.IsHexAddress(from) {
		return fmt.Errorf("invalid from address")
	}
	if to != "" && !common.IsHexAddress(to) {
		return fmt.Errorf("invalid to address")
	}
	if gasLimit < n.minGasLimit || gasLimit > n.maxGasLimit {
		return fmt.Errorf("gas limit %d outside allowed range", gasLimit)
	}
	if rawNonce, ok := tx["nonce"]; ok {
		nonce := quantityFromInterface(rawNonce)
		if nonce != n.state.Nonces[from] {
			return fmt.Errorf("invalid nonce: got %d want %d", nonce, n.state.Nonces[from])
		}
	}
	if n.strictMode && n.balance(from).Cmp(value) < 0 {
		return fmt.Errorf("insufficient balance")
	}
	return nil
}

func (n *rpcNode) addRawTransaction(rawTx string) string {
	hash := hashHex([]byte(rawTx))
	now := time.Now().Unix()
	record := txRecord{
		Hash:      hash,
		Value:     "0x0",
		Nonce:     "0x0",
		Gas:       "0x5208",
		Input:     rawTx,
		Time:      now,
		Timestamp: now,
		Fee:       "0x0",
		Raw:       rawTx,
	}
	n.state.PendingTxs = append(n.state.PendingTxs, record)
	if n.autoMine {
		n.minePendingBlock()
	} else {
		n.save()
	}
	return hash
}

func (n *rpcNode) minePendingBlock() blockRecord {
	parent := n.state.Blocks[len(n.state.Blocks)-1]
	pending := append([]txRecord(nil), n.state.PendingTxs...)
	n.state.PendingTxs = []txRecord{}
	blockNumber := n.state.Height + 1
	blockHash := hashJSON(map[string]interface{}{
		"parent": parent.Hash,
		"height": blockNumber,
		"time":   time.Now().UnixNano(),
		"txs":    pending,
	})

	minedTxs := make([]txRecord, 0, len(pending))
	receipts := make([]receiptRecord, 0, len(pending))
	for i, tx := range pending {
		mined, receipt := n.applyMinedTransaction(tx, blockHash, blockNumber, uint64(i))
		minedTxs = append(minedTxs, mined)
		receipts = append(receipts, receipt)
		n.state.Receipts[strings.ToLower(mined.Hash)] = receipt
		n.state.Logs = append(n.state.Logs, receipt.Logs...)
		n.state.Txs = append(n.state.Txs, mined)
	}

	n.state.Height = blockNumber
	block := blockRecord{
		Number:       toHex(blockNumber),
		Hash:         blockHash,
		ParentHash:   parent.Hash,
		StateRoot:    n.computeStateTrieRoot(),
		TxRoot:       hashJSON(minedTxs),
		ReceiptRoot:  n.computeReceiptTrieRoot(receipts),
		LogsBloom:    blockLogsBloomHex(receipts),
		Timestamp:    toHex(uint64(time.Now().Unix())),
		Transactions: minedTxs,
	}
	n.state.Blocks = append(n.state.Blocks, block)
	n.state.Canonical[blockNumber] = block.Hash
	n.state.Anchors[blockNumber] = anchorRecord{
		Height:    blockNumber,
		StateRoot: block.StateRoot,
		BlockHash: block.Hash,
		TxHash:    hashJSON(minedTxs),
		Timestamp: time.Now().Unix(),
		Verified:  true,
	}
	n.storeStateSnapshot(blockNumber, block.Hash, block.StateRoot)
	n.save()
	return block
}

func (n *rpcNode) applyMinedTransaction(tx txRecord, blockHash string, blockNumber uint64, index uint64) (txRecord, receiptRecord) {
	value := parseQuantity(tx.Value)
	if tx.From != "" && tx.To != "" {
		fromBalance := n.balance(tx.From)
		if fromBalance.Cmp(value) >= 0 {
			fromBalance.Sub(fromBalance, value)
			n.state.Balances[tx.From] = "0x" + fromBalance.Text(16)
		}
		toBalance := n.balance(tx.To)
		toBalance.Add(toBalance, value)
		n.state.Balances[tx.To] = "0x" + toBalance.Text(16)
		n.state.Nonces[tx.From]++
	}

	tx.BlockHash = blockHash
	tx.BlockNumber = toHex(blockNumber)
	tx.TransactionIndex = toHex(index)
	tx.Confirmations = 1

	logs, status := n.executeTransaction(tx, blockHash, blockNumber, index)
	receipt := receiptRecord{
		TransactionHash:   tx.Hash,
		TransactionIndex:  toHex(index),
		BlockHash:         blockHash,
		BlockNumber:       toHex(blockNumber),
		From:              tx.From,
		To:                tx.To,
		CumulativeGasUsed: "0x5208",
		GasUsed:           "0x5208",
		ContractAddress:   nil,
		Logs:              logs,
		Status:            status,
		Type:              "0x0",
	}
	receipt.LogsBloom = receiptBloomHex(receipt)
	return tx, receipt
}

func (n *rpcNode) executeTransaction(tx txRecord, blockHash string, blockNumber uint64, txIndex uint64) ([]logRecord, string) {
	if tx.To == "" {
		n.state.Code[contractAddress(tx.From, tx.Nonce)] = tx.Input
		return nil, "0x1"
	}
	if tx.To == n.bridgeAddress {
		logs, ok := n.executeBridgeCall(tx, blockHash, blockNumber, txIndex)
		if !ok {
			return logs, "0x0"
		}
		return logs, "0x1"
	}
	if tx.Input != "" && tx.Input != "0x" {
		n.executeSimpleStorageCall(tx)
	}
	return nil, "0x1"
}

func (n *rpcNode) executeBridgeCall(tx txRecord, blockHash string, blockNumber uint64, txIndex uint64) ([]logRecord, bool) {
	if tx.To != n.bridgeAddress {
		return nil, false
	}
	selector := abiSelector(tx.Input)
	topic := topicHash("BridgeTransfer")
	if selector == "0x7a0f1d9e" || strings.Contains(strings.ToLower(tx.Input), "withdraw") {
		topic = topicHash("WithdrawalInitiated")
		if tx.From != "" {
			n.initiateWithdrawal(map[string]interface{}{
				"recipient": tx.From,
				"amount":    tx.Value,
				"l2TxHash":  tx.Hash,
			})
		}
	}
	if selector == "0x3de90c0b" || strings.Contains(strings.ToLower(tx.Input), "anchor") {
		topic = topicHash("AnchorSubmitted")
		n.submitAnchor(map[string]interface{}{
			"height":    blockNumber,
			"stateRoot": n.computeStateTrieRoot(),
			"blockHash": blockHash,
			"txHash":    tx.Hash,
		})
	}
	return []logRecord{{
		Address:          n.bridgeAddress,
		Topics:           []string{topic, paddedTopic(tx.From), paddedTopic(tx.To)},
		Data:             tx.Value,
		BlockNumber:      toHex(blockNumber),
		BlockHash:        blockHash,
		TransactionHash:  tx.Hash,
		TransactionIndex: toHex(txIndex),
		LogIndex:         toHex(uint64(len(n.state.Logs))),
		Removed:          false,
	}}, true
}

func (n *rpcNode) executeSimpleStorageCall(tx txRecord) {
	selector := abiSelector(tx.Input)
	if selector != "0x6057361d" {
		return
	}
	if n.state.Storage[tx.To] == nil {
		n.state.Storage[tx.To] = map[string]string{}
	}
	n.state.Storage[tx.To][zeroHash()] = tx.Value
}

func (n *rpcNode) callContract(tx map[string]interface{}) string {
	to := normalizeAddress(fmt.Sprint(tx["to"]))
	input := fmt.Sprint(tx["data"])
	if input == "<nil>" {
		input = fmt.Sprint(tx["input"])
	}
	if to == n.bridgeAddress {
		switch abiSelector(input) {
		case "0x3f4ba83a":
			return "0x" + strings.Repeat("0", 63) + "0"
		case "0x5c975abb":
			return "0x" + strings.Repeat("0", 63) + "1"
		default:
			return "0x"
		}
	}
	return "0x"
}

func (n *rpcNode) genesisBlock() blockRecord {
	return blockRecord{
		Number:       "0x1",
		Hash:         hashHex([]byte("bhte-genesis")),
		ParentHash:   zeroHash(),
		StateRoot:    n.computeStateTrieRoot(),
		TxRoot:       zeroHash(),
		ReceiptRoot:  zeroHash(),
		LogsBloom:    zeroHash(),
		Timestamp:    toHex(uint64(time.Now().Unix())),
		Transactions: []txRecord{},
	}
}

func (n *rpcNode) computeStateRoot() string {
	return hashJSON(map[string]interface{}{
		"balances": n.state.Balances,
		"nonces":   n.state.Nonces,
		"height":   n.state.Height,
	})
}

func (n *rpcNode) computeStateTrieRoot() string {
	for _, account := range n.sortedAccounts() {
		if len(n.state.Storage[account]) > 0 {
			n.persistStorageTrie(account)
		}
	}
	tr := n.buildStateTrie()
	root, nodes := tr.Commit(true)
	n.persistTrieNodes(root.Hex(), "state", n.state.Height, nodes)
	return root.Hex()
}

func (n *rpcNode) buildStateTrie() *trie.Trie {
	return buildStateTrieFromSnapshot(n.currentStateSnapshot("", ""))
}

func buildStateTrieFromSnapshot(snapshot stateSnapshotRecord) *trie.Trie {
	tr := trie.NewEmpty(nil)
	for _, account := range sortedSnapshotAccounts(snapshot) {
		addr := common.HexToAddress(account)
		balance, _ := uint256.FromBig(parseQuantity(snapshot.Balances[account]))
		stateAccount := types.StateAccount{
			Nonce:    snapshot.Nonces[account],
			Balance:  balance,
			Root:     common.HexToHash(storageRootFromSnapshot(snapshot, account)),
			CodeHash: codeHashFromSnapshot(snapshot, account).Bytes(),
		}
		encoded, err := rlp.EncodeToBytes(&stateAccount)
		if err != nil {
			continue
		}
		tr.MustUpdate(crypto.Keccak256(addr.Bytes()), encoded)
	}
	return tr
}

func (n *rpcNode) currentStateSnapshot(blockHash, stateRoot string) stateSnapshotRecord {
	return stateSnapshotRecord{
		Height:    n.state.Height,
		BlockHash: blockHash,
		StateRoot: stateRoot,
		Complete:  true,
		Source:    "local",
		Balances:  copyStringMap(n.state.Balances),
		Nonces:    copyNonceMap(n.state.Nonces),
		Code:      copyStringMap(n.state.Code),
		Storage:   copyNestedStringMap(n.state.Storage),
		Timestamp: time.Now().Unix(),
	}
}

func (n *rpcNode) storeStateSnapshot(height uint64, blockHash, stateRoot string) {
	snapshot := n.currentStateSnapshot(blockHash, stateRoot)
	snapshot.Height = height
	n.state.StateSnapshots[height] = snapshot
}

func (n *rpcNode) storePeerStateSnapshot(block blockRecord) {
	height := parseQuantity(block.Number).Uint64()
	n.state.StateSnapshots[height] = stateSnapshotRecord{
		Height:    height,
		BlockHash: block.Hash,
		StateRoot: block.StateRoot,
		Complete:  false,
		Source:    "peer-summary",
		Balances:  map[string]string{},
		Nonces:    map[string]uint64{},
		Code:      map[string]string{},
		Storage:   map[string]map[string]string{},
		Timestamp: time.Now().Unix(),
	}
}

func (n *rpcNode) snapshotForBlock(blockRef string) (stateSnapshotRecord, error) {
	blockRef = strings.TrimSpace(blockRef)
	if blockRef == "" || blockRef == "latest" || blockRef == "pending" {
		latest := n.state.Blocks[len(n.state.Blocks)-1]
		height := parseQuantity(latest.Number).Uint64()
		if snapshot, ok := n.state.StateSnapshots[height]; ok {
			if !snapshot.Complete {
				return stateSnapshotRecord{}, fmt.Errorf("state snapshot for block %s is incomplete; full execution replay required", latest.Number)
			}
		}
		return n.currentStateSnapshot(latest.Hash, ""), nil
	}
	var height uint64
	if strings.HasPrefix(blockRef, "0x") {
		height = parseQuantity(blockRef).Uint64()
	} else {
		for _, block := range n.state.Blocks {
			if strings.EqualFold(block.Hash, blockRef) {
				height = parseQuantity(block.Number).Uint64()
				break
			}
		}
		if height == 0 {
			if parsed, err := strconv.ParseUint(blockRef, 10, 64); err == nil {
				height = parsed
			}
		}
	}
	if height == 0 {
		return stateSnapshotRecord{}, fmt.Errorf("unsupported proof block reference %q", blockRef)
	}
	snapshot, ok := n.state.StateSnapshots[height]
	if !ok {
		return stateSnapshotRecord{}, fmt.Errorf("state snapshot not found for block %s", blockRef)
	}
	if !snapshot.Complete {
		return stateSnapshotRecord{}, fmt.Errorf("state snapshot for block %s is incomplete; full execution replay required", blockRef)
	}
	return snapshot, nil
}

func (n *rpcNode) computeReceiptTrieRoot(receipts []receiptRecord) string {
	tr := n.buildReceiptTrie(receipts)
	root, nodes := tr.Commit(true)
	n.persistTrieNodes(root.Hex(), "receipts", n.state.Height, nodes)
	return root.Hex()
}

func (n *rpcNode) buildReceiptTrie(receipts []receiptRecord) *trie.Trie {
	tr := trie.NewEmpty(nil)
	for i, receipt := range receipts {
		encoded, err := rlp.EncodeToBytes(receipt.toGethReceipt())
		if err != nil {
			continue
		}
		key, _ := rlp.EncodeToBytes(uint(i))
		tr.MustUpdate(key, encoded)
	}
	return tr
}

func receiptTrieRoot(receipts []receiptRecord) string {
	tr := trie.NewEmpty(nil)
	for i, receipt := range receipts {
		encoded, err := rlp.EncodeToBytes(receipt.toGethReceipt())
		if err != nil {
			continue
		}
		key, _ := rlp.EncodeToBytes(uint(i))
		tr.MustUpdate(key, encoded)
	}
	return tr.Hash().Hex()
}

func (n *rpcNode) persistTrieNodes(root, kind string, height uint64, nodes *trienode.NodeSet) {
	if nodes == nil {
		return
	}
	for _, commit := range n.trieCommits {
		if strings.EqualFold(commit.Root, root) && commit.Kind == kind && commit.Height == height {
			return
		}
	}
	record := trieCommitRecord{
		Root:      root,
		Kind:      kind,
		Height:    height,
		Timestamp: time.Now().Unix(),
	}
	nodes.ForEachWithOrder(func(path string, node *trienode.Node) {
		item := trieNodeRecord{
			Hash:    node.Hash.Hex(),
			Path:    hex.EncodeToString([]byte(path)),
			Blob:    "0x" + hex.EncodeToString(node.Blob),
			Deleted: node.IsDeleted(),
		}
		record.Nodes = append(record.Nodes, item)
	})
	n.trieCommits = append(n.trieCommits, record)
}

func (n *rpcNode) sortedAccounts() []string {
	seen := map[string]bool{}
	for account := range n.state.Balances {
		seen[normalizeAddress(account)] = true
	}
	for account := range n.state.Nonces {
		seen[normalizeAddress(account)] = true
	}
	for _, account := range n.state.Accounts {
		seen[normalizeAddress(account)] = true
	}
	for account := range n.state.Code {
		seen[normalizeAddress(account)] = true
	}
	for account := range n.state.Storage {
		seen[normalizeAddress(account)] = true
	}
	accounts := make([]string, 0, len(seen))
	for account := range seen {
		if common.IsHexAddress(account) {
			accounts = append(accounts, account)
		}
	}
	sort.Strings(accounts)
	return accounts
}

func (n *rpcNode) blockByNumber(number string) (blockRecord, bool) {
	if number == "latest" || number == "" {
		return n.state.Blocks[len(n.state.Blocks)-1], true
	}
	if number == "pending" {
		block := n.state.Blocks[len(n.state.Blocks)-1]
		block.Transactions = append([]txRecord(nil), n.state.PendingTxs...)
		return block, true
	}
	height := parseQuantity(number).Uint64()
	for _, block := range n.state.Blocks {
		if parseQuantity(block.Number).Uint64() == height {
			return block, true
		}
	}
	return blockRecord{}, false
}

func (n *rpcNode) blockByHash(hash string) (blockRecord, bool) {
	hash = strings.ToLower(hash)
	for _, block := range n.state.Blocks {
		if strings.ToLower(block.Hash) == hash {
			return block, true
		}
	}
	return blockRecord{}, false
}

func (n *rpcNode) transactionByHash(hash string) (txRecord, bool) {
	hash = strings.ToLower(hash)
	for _, tx := range n.state.PendingTxs {
		if strings.ToLower(tx.Hash) == hash {
			return tx, true
		}
	}
	for _, tx := range n.state.Txs {
		if strings.ToLower(tx.Hash) == hash {
			return tx, true
		}
	}
	return txRecord{}, false
}

func (n *rpcNode) txpoolContent() map[string]interface{} {
	pending := map[string]map[string]txRecord{}
	for _, tx := range n.state.PendingTxs {
		from := tx.From
		if from == "" {
			from = "0x0000000000000000000000000000000000000000"
		}
		if pending[from] == nil {
			pending[from] = map[string]txRecord{}
		}
		pending[from][tx.Nonce] = tx
	}
	return map[string]interface{}{"pending": pending, "queued": map[string]interface{}{}}
}

func (n *rpcNode) filterLogs(filter map[string]interface{}) []logRecord {
	fromBlock := uint64(0)
	toBlock := ^uint64(0)
	if raw, ok := filter["fromBlock"]; ok {
		fromBlock = parseQuantity(fmt.Sprint(raw)).Uint64()
	}
	if raw, ok := filter["toBlock"]; ok && fmt.Sprint(raw) != "latest" {
		toBlock = parseQuantity(fmt.Sprint(raw)).Uint64()
	}
	address := normalizeAddress(fmt.Sprint(filter["address"]))
	result := []logRecord{}
	for _, lg := range n.state.Logs {
		height := parseQuantity(lg.BlockNumber).Uint64()
		if height < fromBlock || height > toBlock {
			continue
		}
		if address != "" && normalizeAddress(lg.Address) != address {
			continue
		}
		result = append(result, lg)
	}
	return result
}

func (n *rpcNode) bridgeEvents(limit int) []logRecord {
	if limit <= 0 {
		limit = 100
	}
	result := []logRecord{}
	for i := len(n.state.Logs) - 1; i >= 0 && len(result) < limit; i-- {
		if normalizeAddress(n.state.Logs[i].Address) == n.bridgeAddress {
			result = append(result, n.state.Logs[i])
		}
	}
	return result
}

func (n *rpcNode) submitBridgeEvent(event map[string]interface{}) map[string]interface{} {
	now := time.Now().Unix()
	txHash := stringOrDefault(fmt.Sprint(event["txHash"]), hashHex([]byte(fmt.Sprint(event)+strconv.FormatInt(now, 10))))
	topic := stringOrDefault(fmt.Sprint(event["topic"]), topicHash("BridgeEvent"))
	log := logRecord{
		Address:          n.bridgeAddress,
		Topics:           []string{topic},
		Data:             stringOrDefault(fmt.Sprint(event["data"]), "0x"),
		BlockNumber:      toHex(n.state.Height),
		BlockHash:        n.state.Blocks[len(n.state.Blocks)-1].Hash,
		TransactionHash:  txHash,
		TransactionIndex: "0x0",
		LogIndex:         toHex(uint64(len(n.state.Logs))),
		Removed:          false,
	}
	n.state.Logs = append(n.state.Logs, log)
	n.save()
	return map[string]interface{}{"accepted": true, "log": log}
}

func (n *rpcNode) initiateWithdrawal(request map[string]interface{}) withdrawalRecord {
	now := time.Now().Unix()
	recipient := normalizeAddress(fmt.Sprint(request["recipient"]))
	amount := stringOrDefault(fmt.Sprint(request["amount"]), "0x0")
	l2TxHash := stringOrDefault(fmt.Sprint(request["l2TxHash"]), hashHex([]byte(recipient+amount+strconv.FormatInt(now, 10))))
	id := strings.ToLower(l2TxHash)
	withdrawal := withdrawalRecord{
		ID:                id,
		L2TxHash:          l2TxHash,
		Recipient:         recipient,
		Amount:            amount,
		ChallengeDeadline: now + n.challengeSeconds,
		Processed:         false,
		Challenged:        false,
		CreatedAt:         now,
	}
	n.state.Withdrawals[id] = withdrawal
	if !n.replayMode {
		n.state.Logs = append(n.state.Logs, logRecord{
			Address:          n.bridgeAddress,
			Topics:           []string{topicHash("WithdrawalInitiated"), paddedTopic(recipient)},
			Data:             amount,
			BlockNumber:      toHex(n.state.Height),
			BlockHash:        n.state.Blocks[len(n.state.Blocks)-1].Hash,
			TransactionHash:  l2TxHash,
			TransactionIndex: "0x0",
			LogIndex:         toHex(uint64(len(n.state.Logs))),
			Removed:          false,
		})
		n.save()
	}
	return withdrawal
}

func (n *rpcNode) processWithdrawal(id string) (withdrawalRecord, error) {
	key := strings.ToLower(id)
	withdrawal := n.state.Withdrawals[key]
	if withdrawal.ID == "" {
		return withdrawalRecord{}, fmt.Errorf("withdrawal not found")
	}
	if withdrawal.Challenged {
		return withdrawal, fmt.Errorf("withdrawal challenged")
	}
	if withdrawal.Processed {
		return withdrawal, nil
	}
	if n.strictMode && time.Now().Unix() < withdrawal.ChallengeDeadline {
		return withdrawal, fmt.Errorf("challenge period not over")
	}
	bhtTxHash, err := n.broadcastBHTWithdrawal(withdrawal)
	if err != nil {
		return withdrawal, err
	}
	withdrawal.Processed = true
	withdrawal.BHTTxHash = bhtTxHash
	n.state.Withdrawals[key] = withdrawal
	n.state.Logs = append(n.state.Logs, logRecord{
		Address:          n.bridgeAddress,
		Topics:           []string{topicHash("WithdrawalProcessed"), paddedTopic(withdrawal.Recipient)},
		Data:             withdrawal.Amount,
		BlockNumber:      toHex(n.state.Height),
		BlockHash:        n.state.Blocks[len(n.state.Blocks)-1].Hash,
		TransactionHash:  withdrawal.L2TxHash,
		TransactionIndex: "0x0",
		LogIndex:         toHex(uint64(len(n.state.Logs))),
		Removed:          false,
	})
	n.save()
	return withdrawal, nil
}

func (n *rpcNode) broadcastBHTWithdrawal(withdrawal withdrawalRecord) (string, error) {
	if !n.strictMode && n.bhtRPC == "" {
		return hashHex([]byte(withdrawal.ID + "bht-withdrawal")), nil
	}
	if n.bhtRPC == "" {
		return "", fmt.Errorf("BHT RPC endpoint is not configured")
	}
	var result string
	params := []interface{}{withdrawal.Recipient, quantityToDecimal(withdrawal.Amount)}
	if err := n.callBHTRPC("sendtoaddress", params, &result); err != nil {
		if n.strictMode {
			return "", err
		}
		return hashHex([]byte(withdrawal.ID + "bht-withdrawal-fallback")), nil
	}
	return result, nil
}

func (n *rpcNode) submitAnchor(anchor map[string]interface{}) anchorRecord {
	height := quantityFromInterface(anchor["height"])
	if height == 0 {
		height = n.state.Height
	}
	stateRoot := stringOrDefault(fmt.Sprint(anchor["stateRoot"]), n.state.Blocks[len(n.state.Blocks)-1].StateRoot)
	record := anchorRecord{
		Height:    height,
		StateRoot: stateRoot,
		BlockHash: stringOrDefault(fmt.Sprint(anchor["blockHash"]), n.state.Blocks[len(n.state.Blocks)-1].Hash),
		TxHash:    stringOrDefault(fmt.Sprint(anchor["txHash"]), hashHex([]byte(stateRoot))),
		Timestamp: time.Now().Unix(),
		Verified:  true,
	}
	n.state.Anchors[height] = record
	if !n.replayMode {
		n.save()
	}
	return record
}

func (n *rpcNode) verifyStateRoot(height uint64, stateRoot string, proof []string) map[string]interface{} {
	anchor, ok := n.state.Anchors[height]
	if !ok {
		return map[string]interface{}{"valid": false, "message": "anchor not found"}
	}
	valid := strings.EqualFold(anchor.StateRoot, stateRoot)
	if len(proof) > 0 {
		valid = valid && strings.EqualFold(proof[0], stateRoot)
	}
	message := "state root mismatch"
	if valid {
		message = "state root verified"
	}
	return map[string]interface{}{
		"valid":     valid,
		"height":    toHex(height),
		"stateRoot": anchor.StateRoot,
		"blockHash": anchor.BlockHash,
		"message":   message,
	}
}

func (n *rpcNode) addPeer(url string) peerRecord {
	url = strings.TrimSpace(url)
	id := hashHex([]byte(url))
	peer := peerRecord{
		ID:       id,
		URL:      url,
		Height:   n.state.Height,
		BestHash: n.state.Blocks[len(n.state.Blocks)-1].Hash,
		LastSeen: time.Now().Unix(),
		Trusted:  strings.HasPrefix(url, "http://127.0.0.1") || strings.HasPrefix(url, "http://localhost"),
	}
	n.state.Peers[id] = peer
	n.save()
	return peer
}

func (n *rpcNode) peerList() []peerRecord {
	peers := make([]peerRecord, 0, len(n.state.Peers))
	for _, peer := range n.state.Peers {
		peers = append(peers, peer)
	}
	sort.Slice(peers, func(i, j int) bool { return peers[i].URL < peers[j].URL })
	return peers
}

func (n *rpcNode) syncPeer(idOrURL string) map[string]interface{} {
	var peer peerRecord
	for _, candidate := range n.state.Peers {
		if candidate.ID == idOrURL || candidate.URL == idOrURL {
			peer = candidate
			break
		}
	}
	if peer.URL == "" {
		peer = n.addPeer(idOrURL)
	}
	var remoteHeight string
	if err := callExternalRPC(peer.URL, "eth_blockNumber", []interface{}{}, &remoteHeight); err != nil {
		return map[string]interface{}{"synced": false, "peer": peer, "error": err.Error()}
	}
	peer.Height = parseQuantity(remoteHeight).Uint64()
	peer.LastSeen = time.Now().Unix()
	imported := 0
	if peer.Height > n.state.Height {
		count, err := n.syncPeerBlocks(&peer, n.state.Height+1, peer.Height)
		if err != nil {
			n.state.Peers[peer.ID] = peer
			n.save()
			return map[string]interface{}{"synced": false, "peer": peer, "error": err.Error(), "imported": imported}
		}
		imported = count
	}
	if n.state.Height > 0 {
		peer.BestHash = n.state.Blocks[len(n.state.Blocks)-1].Hash
	}
	n.state.Peers[peer.ID] = peer
	n.save()
	return map[string]interface{}{
		"synced":       true,
		"peer":         peer,
		"localHeight":  toHex(n.state.Height),
		"remoteHeight": toHex(peer.Height),
		"imported":     imported,
	}
}

func (n *rpcNode) syncPeerBlocks(peer *peerRecord, startHeight, endHeight uint64) (int, error) {
	imported := 0
	for height := startHeight; height <= endHeight && imported < maxPeerSyncBlocks; height++ {
		var block blockRecord
		if err := callExternalRPC(peer.URL, "eth_getBlockByNumber", []interface{}{toHex(height), true}, &block); err != nil {
			return imported, fmt.Errorf("failed to fetch peer block %d: %w", height, err)
		}
		if err := n.validatePeerBlock(block, height); err != nil {
			return imported, err
		}
		importedBlock, err := n.importPeerBlock(block)
		if err != nil {
			return imported, err
		}
		if importedBlock {
			imported++
		}
	}
	return imported, nil
}

func (n *rpcNode) validatePeerBlock(block blockRecord, expectedHeight uint64) error {
	if block.Hash == "" || block.Hash == zeroHash() {
		return fmt.Errorf("peer block %d missing hash", expectedHeight)
	}
	if block.StateRoot == "" || block.StateRoot == zeroHash() {
		return fmt.Errorf("peer block %d missing state root", expectedHeight)
	}
	if block.ReceiptRoot == "" {
		return fmt.Errorf("peer block %d missing receipt root", expectedHeight)
	}
	if parseQuantity(block.Number).Uint64() != expectedHeight {
		return fmt.Errorf("peer block number mismatch: got %s want %s", block.Number, toHex(expectedHeight))
	}
	if existing := n.state.Canonical[expectedHeight]; existing != "" && !strings.EqualFold(existing, block.Hash) {
		return fmt.Errorf("peer block %d conflicts with canonical hash: got %s want %s", expectedHeight, block.Hash, existing)
	}
	expectedParent := n.state.Canonical[expectedHeight-1]
	if expectedParent == "" && len(n.state.Blocks) > 0 {
		expectedParent = n.state.Blocks[len(n.state.Blocks)-1].Hash
	}
	if expectedHeight > 1 && !strings.EqualFold(block.ParentHash, expectedParent) {
		return fmt.Errorf("peer block %d parent mismatch: got %s want %s", expectedHeight, block.ParentHash, expectedParent)
	}
	expectedTxRoot := hashJSON(block.Transactions)
	if block.TxRoot != "" && !strings.EqualFold(block.TxRoot, expectedTxRoot) {
		return fmt.Errorf("peer block %d tx root mismatch: got %s want %s", expectedHeight, block.TxRoot, expectedTxRoot)
	}
	return nil
}

func (n *rpcNode) importPeerBlock(block blockRecord) (bool, error) {
	height := parseQuantity(block.Number).Uint64()
	if existing := n.state.Canonical[height]; existing != "" && strings.EqualFold(existing, block.Hash) {
		return false, nil
	}
	minedTxs, receipts, err := n.replayPeerBlock(block)
	if err != nil {
		return false, err
	}
	block.Transactions = minedTxs
	block.TxRoot = hashJSON(minedTxs)
	block.ReceiptRoot = n.computeReceiptTrieRoot(receipts)
	block.LogsBloom = blockLogsBloomHex(receipts)
	n.state.Height = height
	n.state.Blocks = append(n.state.Blocks, block)
	n.state.Canonical[height] = block.Hash
	n.state.Anchors[height] = anchorRecord{
		Height:    height,
		StateRoot: block.StateRoot,
		BlockHash: block.Hash,
		TxHash:    block.TxRoot,
		Timestamp: time.Now().Unix(),
		Verified:  true,
	}
	n.storeStateSnapshot(height, block.Hash, block.StateRoot)
	for i, tx := range minedTxs {
		receipt := receipts[i]
		n.state.Receipts[strings.ToLower(tx.Hash)] = receipt
		n.state.Logs = append(n.state.Logs, receipt.Logs...)
		if _, exists := n.transactionByHash(tx.Hash); !exists {
			n.state.Txs = append(n.state.Txs, tx)
		}
	}
	return true, nil
}

func (n *rpcNode) replayPeerBlock(block blockRecord) ([]txRecord, []receiptRecord, error) {
	beforeHeight := n.state.Height
	beforeSnapshot := n.currentStateSnapshot("", "")
	beforeReceipts := copyReceiptMap(n.state.Receipts)
	beforeLogs := append([]logRecord(nil), n.state.Logs...)
	beforeTxs := append([]txRecord(nil), n.state.Txs...)
	beforeWithdrawals := copyWithdrawalMap(n.state.Withdrawals)
	beforeAnchors := copyAnchorMap(n.state.Anchors)
	beforeCommits := append([]trieCommitRecord(nil), n.trieCommits...)
	beforeReplayMode := n.replayMode
	restore := func() {
		n.restoreReplayState(beforeHeight, beforeSnapshot, beforeReceipts, beforeLogs, beforeTxs, beforeWithdrawals, beforeAnchors, beforeCommits, beforeReplayMode)
	}

	height := parseQuantity(block.Number).Uint64()
	n.state.Height = height
	n.replayMode = true
	minedTxs := make([]txRecord, 0, len(block.Transactions))
	receipts := make([]receiptRecord, 0, len(block.Transactions))
	for i, tx := range block.Transactions {
		mined, receipt := n.applyMinedTransaction(tx, block.Hash, height, uint64(i))
		minedTxs = append(minedTxs, mined)
		receipts = append(receipts, receipt)
	}
	stateRoot := buildStateTrieFromSnapshot(n.currentStateSnapshot(block.Hash, "")).Hash().Hex()
	txRoot := hashJSON(minedTxs)
	receiptRoot := receiptTrieRoot(receipts)
	logsBloom := blockLogsBloomHex(receipts)

	if !strings.EqualFold(block.StateRoot, stateRoot) {
		restore()
		return nil, nil, fmt.Errorf("peer block %d state root mismatch: got %s want %s", height, stateRoot, block.StateRoot)
	}
	if block.TxRoot != "" && !strings.EqualFold(block.TxRoot, txRoot) {
		restore()
		return nil, nil, fmt.Errorf("peer block %d replay tx root mismatch: got %s want %s", height, txRoot, block.TxRoot)
	}
	if block.ReceiptRoot != "" && !strings.EqualFold(block.ReceiptRoot, receiptRoot) {
		restore()
		return nil, nil, fmt.Errorf("peer block %d receipt root mismatch: got %s want %s", height, receiptRoot, block.ReceiptRoot)
	}
	if block.LogsBloom != "" && block.LogsBloom != zeroHash() && !strings.EqualFold(block.LogsBloom, logsBloom) {
		restore()
		return nil, nil, fmt.Errorf("peer block %d logs bloom mismatch: got %s want %s", height, logsBloom, block.LogsBloom)
	}
	n.replayMode = beforeReplayMode
	return minedTxs, receipts, nil
}

func (n *rpcNode) replayChain() map[string]interface{} {
	if len(n.state.Blocks) == 0 {
		return map[string]interface{}{"valid": false, "checked": 0, "message": "chain has no blocks"}
	}
	beforeHeight := n.state.Height
	beforeSnapshot := n.currentStateSnapshot("", "")
	beforeReceipts := copyReceiptMap(n.state.Receipts)
	beforeLogs := append([]logRecord(nil), n.state.Logs...)
	beforeTxs := append([]txRecord(nil), n.state.Txs...)
	beforeWithdrawals := copyWithdrawalMap(n.state.Withdrawals)
	beforeAnchors := copyAnchorMap(n.state.Anchors)
	beforeCommits := append([]trieCommitRecord(nil), n.trieCommits...)
	beforeReplayMode := n.replayMode
	defer n.restoreReplayState(beforeHeight, beforeSnapshot, beforeReceipts, beforeLogs, beforeTxs, beforeWithdrawals, beforeAnchors, beforeCommits, beforeReplayMode)

	genesis := n.state.Blocks[0]
	genesisHeight := parseQuantity(genesis.Number).Uint64()
	genesisSnapshot, ok := n.state.StateSnapshots[genesisHeight]
	if !ok || !genesisSnapshot.Complete {
		return map[string]interface{}{
			"valid":   false,
			"checked": 0,
			"height":  toHex(genesisHeight),
			"message": "complete genesis snapshot not found",
		}
	}
	n.state.Height = genesisHeight
	n.restoreStateSnapshot(genesisSnapshot)
	n.state.Receipts = map[string]receiptRecord{}
	n.state.Logs = []logRecord{}
	n.state.Txs = []txRecord{}
	n.state.Withdrawals = map[string]withdrawalRecord{}
	n.state.Anchors = map[uint64]anchorRecord{}
	n.replayMode = true

	checked := 0
	parentHash := genesis.Hash
	for i := 1; i < len(n.state.Blocks); i++ {
		block := n.state.Blocks[i]
		height := parseQuantity(block.Number).Uint64()
		if height != genesisHeight+uint64(i) {
			return replayChainFailure(checked, height, "non-contiguous block height")
		}
		if !strings.EqualFold(block.ParentHash, parentHash) {
			return map[string]interface{}{
				"valid":     false,
				"checked":   checked,
				"height":    toHex(height),
				"blockHash": block.Hash,
				"message":   fmt.Sprintf("parent mismatch: got %s want %s", block.ParentHash, parentHash),
			}
		}
		minedTxs := make([]txRecord, 0, len(block.Transactions))
		receipts := make([]receiptRecord, 0, len(block.Transactions))
		n.state.Height = height
		for txIndex, tx := range block.Transactions {
			mined, receipt := n.applyMinedTransaction(tx, block.Hash, height, uint64(txIndex))
			minedTxs = append(minedTxs, mined)
			receipts = append(receipts, receipt)
		}
		stateRoot := buildStateTrieFromSnapshot(n.currentStateSnapshot(block.Hash, "")).Hash().Hex()
		txRoot := hashJSON(minedTxs)
		receiptRoot := receiptTrieRoot(receipts)
		logsBloom := blockLogsBloomHex(receipts)
		if !strings.EqualFold(block.StateRoot, stateRoot) {
			return replayChainRootFailure(checked, height, block.Hash, "stateRoot", stateRoot, block.StateRoot)
		}
		if block.TxRoot != "" && !strings.EqualFold(block.TxRoot, txRoot) {
			return replayChainRootFailure(checked, height, block.Hash, "transactionsRoot", txRoot, block.TxRoot)
		}
		if block.ReceiptRoot != "" && !strings.EqualFold(block.ReceiptRoot, receiptRoot) {
			return replayChainRootFailure(checked, height, block.Hash, "receiptsRoot", receiptRoot, block.ReceiptRoot)
		}
		if block.LogsBloom != "" && block.LogsBloom != zeroHash() && !strings.EqualFold(block.LogsBloom, logsBloom) {
			return replayChainRootFailure(checked, height, block.Hash, "logsBloom", logsBloom, block.LogsBloom)
		}
		for receiptIndex, receipt := range receipts {
			tx := minedTxs[receiptIndex]
			n.state.Receipts[strings.ToLower(tx.Hash)] = receipt
			n.state.Logs = append(n.state.Logs, receipt.Logs...)
			n.state.Txs = append(n.state.Txs, tx)
		}
		checked++
		parentHash = block.Hash
	}
	return map[string]interface{}{
		"valid":    true,
		"checked":  checked,
		"height":   toHex(n.state.Height),
		"bestHash": parentHash,
		"message":  "chain replay verified",
	}
}

func (n *rpcNode) restoreReplayState(height uint64, snapshot stateSnapshotRecord, receipts map[string]receiptRecord, logs []logRecord, txs []txRecord, withdrawals map[string]withdrawalRecord, anchors map[uint64]anchorRecord, commits []trieCommitRecord, replayMode bool) {
	n.state.Height = height
	n.restoreStateSnapshot(snapshot)
	n.state.Receipts = receipts
	n.state.Logs = logs
	n.state.Txs = txs
	n.state.Withdrawals = withdrawals
	n.state.Anchors = anchors
	n.trieCommits = commits
	n.replayMode = replayMode
}

func replayChainFailure(checked int, height uint64, message string) map[string]interface{} {
	return map[string]interface{}{
		"valid":   false,
		"checked": checked,
		"height":  toHex(height),
		"message": message,
	}
}

func replayChainRootFailure(checked int, height uint64, blockHash, field, computed, expected string) map[string]interface{} {
	return map[string]interface{}{
		"valid":     false,
		"checked":   checked,
		"height":    toHex(height),
		"blockHash": blockHash,
		"field":     field,
		"computed":  computed,
		"expected":  expected,
		"message":   field + " mismatch",
	}
}

func (n *rpcNode) consensusStatus() map[string]interface{} {
	best := n.state.Blocks[len(n.state.Blocks)-1]
	return map[string]interface{}{
		"engine":     "bhte-dev-poa",
		"height":     toHex(n.state.Height),
		"bestHash":   best.Hash,
		"stateRoot":  best.StateRoot,
		"peerCount":  toHex(uint64(len(n.state.Peers))),
		"finalized":  toHex(n.finalizedHeight()),
		"strictMode": n.strictMode,
	}
}

func (n *rpcNode) finalizedHeight() uint64 {
	if n.state.Height <= 12 {
		return 1
	}
	return n.state.Height - 12
}

func (n *rpcNode) validateBlockObject(block map[string]interface{}) map[string]interface{} {
	number := quantityFromInterface(block["number"])
	parent := fmt.Sprint(block["parentHash"])
	if number == 0 {
		return map[string]interface{}{"valid": false, "message": "missing block number"}
	}
	if number > 1 {
		expectedParent := n.state.Canonical[number-1]
		if expectedParent != "" && !strings.EqualFold(expectedParent, parent) {
			return map[string]interface{}{"valid": false, "message": "parent hash mismatch"}
		}
	}
	return map[string]interface{}{"valid": true, "message": "block header accepted", "number": toHex(number)}
}

func (n *rpcNode) getTrieCommit(root string) interface{} {
	commit, ok := n.trieCommitByRoot(root)
	if !ok {
		return nil
	}
	return commit
}

func (n *rpcNode) trieCommitByRoot(root string) (trieCommitRecord, bool) {
	root = strings.ToLower(root)
	for i := len(n.trieCommits) - 1; i >= 0; i-- {
		if strings.ToLower(n.trieCommits[i].Root) == root {
			return n.trieCommits[i], true
		}
	}
	return trieCommitRecord{}, false
}

func (n *rpcNode) getTrieCommits(kind string, height uint64) []trieCommitRecord {
	kind = strings.TrimSpace(kind)
	result := make([]trieCommitRecord, 0)
	for i := len(n.trieCommits) - 1; i >= 0; i-- {
		commit := n.trieCommits[i]
		if kind != "" && commit.Kind != kind {
			continue
		}
		if height != 0 && commit.Height != height {
			continue
		}
		result = append(result, commit)
	}
	return result
}

func (n *rpcNode) getTrieNode(root, hash string) (map[string]interface{}, error) {
	commit, ok := n.trieCommitByRoot(root)
	if !ok {
		return nil, fmt.Errorf("trie commit not found")
	}
	hash = strings.ToLower(hash)
	for _, node := range commit.Nodes {
		if strings.ToLower(node.Hash) == hash {
			return map[string]interface{}{
				"root":    commit.Root,
				"kind":    commit.Kind,
				"height":  toHex(commit.Height),
				"hash":    node.Hash,
				"path":    node.Path,
				"blob":    node.Blob,
				"deleted": node.Deleted,
			}, nil
		}
	}
	return nil, fmt.Errorf("trie node not found")
}

func (n *rpcNode) receiptProof(txHash string) (map[string]interface{}, error) {
	receipt, ok := n.state.Receipts[strings.ToLower(txHash)]
	if !ok {
		return nil, fmt.Errorf("receipt not found")
	}
	block, ok := n.blockByHash(receipt.BlockHash)
	if !ok {
		return nil, fmt.Errorf("receipt block not found")
	}
	receipts := n.receiptsForBlock(block)
	txIndex := parseQuantity(receipt.TransactionIndex).Uint64()
	if txIndex >= uint64(len(receipts)) {
		return nil, fmt.Errorf("receipt transaction index out of range")
	}
	tr := n.buildReceiptTrie(receipts)
	root := tr.Hash()
	key, _ := rlp.EncodeToBytes(uint(txIndex))
	proof := trienode.ProofList{}
	if err := tr.Prove(key, &proof); err != nil {
		return nil, err
	}
	return map[string]interface{}{
		"transactionHash":  receipt.TransactionHash,
		"transactionIndex": receipt.TransactionIndex,
		"blockHash":        block.Hash,
		"blockNumber":      block.Number,
		"receiptRoot":      root.Hex(),
		"proof":            proofHexList(proof),
		"receipt":          receipt,
	}, nil
}

func (n *rpcNode) verifyReceiptProof(proof map[string]interface{}) map[string]interface{} {
	root := common.HexToHash(fmt.Sprint(proof["receiptRoot"]))
	index := quantityFromInterface(proof["transactionIndex"])
	nodes := stringSliceFromInterface(proof["proof"])
	proofDB, err := proofSetFromHex(nodes)
	if err != nil {
		return map[string]interface{}{"valid": false, "message": err.Error()}
	}
	key, _ := rlp.EncodeToBytes(uint(index))
	value, err := trie.VerifyProof(root, key, proofDB)
	if err != nil {
		return map[string]interface{}{"valid": false, "message": err.Error()}
	}
	if len(value) == 0 {
		return map[string]interface{}{"valid": false, "message": "receipt proof returned empty value"}
	}
	receiptObj, ok := proof["receipt"].(map[string]interface{})
	if !ok {
		return map[string]interface{}{"valid": false, "message": "missing receipt object"}
	}
	var receipt receiptRecord
	data, _ := json.Marshal(receiptObj)
	if err := json.Unmarshal(data, &receipt); err != nil {
		return map[string]interface{}{"valid": false, "message": err.Error()}
	}
	expected, err := rlp.EncodeToBytes(receipt.toGethReceipt())
	if err != nil {
		return map[string]interface{}{"valid": false, "message": err.Error()}
	}
	valid := bytes.Equal(value, expected)
	message := "receipt proof mismatch"
	if valid {
		message = "receipt proof verified"
	}
	return map[string]interface{}{
		"valid":           valid,
		"message":         message,
		"receiptRoot":     root.Hex(),
		"transactionHash": receipt.TransactionHash,
		"blockHash":       receipt.BlockHash,
	}
}

func (n *rpcNode) verifyReceiptInTrie(request map[string]interface{}) map[string]interface{} {
	root := common.HexToHash(fmt.Sprint(request["receiptRoot"]))
	index := quantityFromInterface(request["transactionIndex"])
	commit, ok := n.trieCommitByRoot(root.Hex())
	if !ok {
		return map[string]interface{}{"valid": false, "message": "trie commit not found"}
	}
	proofDB, err := proofSetFromCommit(commit)
	if err != nil {
		return map[string]interface{}{"valid": false, "message": err.Error()}
	}
	key, _ := rlp.EncodeToBytes(uint(index))
	value, err := trie.VerifyProof(root, key, proofDB)
	if err != nil {
		return map[string]interface{}{"valid": false, "message": err.Error()}
	}
	if len(value) == 0 {
		return map[string]interface{}{"valid": false, "message": "receipt not present in trie"}
	}
	receipt, err := receiptFromInterface(request["receipt"])
	if err != nil {
		return map[string]interface{}{"valid": false, "message": err.Error()}
	}
	expected, err := rlp.EncodeToBytes(receipt.toGethReceipt())
	if err != nil {
		return map[string]interface{}{"valid": false, "message": err.Error()}
	}
	valid := bytes.Equal(value, expected)
	message := "receipt trie value mismatch"
	if valid {
		message = "receipt verified from trie database"
	}
	return map[string]interface{}{
		"valid":           valid,
		"message":         message,
		"receiptRoot":     root.Hex(),
		"transactionHash": receipt.TransactionHash,
		"blockHash":       receipt.BlockHash,
		"commitKind":      commit.Kind,
		"commitHeight":    toHex(commit.Height),
	}
}

func (n *rpcNode) logProof(txHash string, logIndex uint64) (map[string]interface{}, error) {
	receiptProof, err := n.receiptProof(txHash)
	if err != nil {
		return nil, err
	}
	receipt := receiptProof["receipt"].(receiptRecord)
	if logIndex >= uint64(len(receipt.Logs)) {
		return nil, fmt.Errorf("log index out of range")
	}
	log := receipt.Logs[logIndex]
	return map[string]interface{}{
		"transactionHash": txHash,
		"logIndex":        toHex(logIndex),
		"log":             log,
		"receiptProof":    receiptProof,
	}, nil
}

func (n *rpcNode) verifyLogProof(proof map[string]interface{}) map[string]interface{} {
	receiptProof, ok := proof["receiptProof"].(map[string]interface{})
	if !ok {
		return map[string]interface{}{"valid": false, "message": "missing receipt proof"}
	}
	receiptVerification := n.verifyReceiptProof(receiptProof)
	if receiptVerification["valid"] != true {
		return map[string]interface{}{
			"valid":          false,
			"message":        "receipt proof invalid",
			"receiptMessage": receiptVerification["message"],
		}
	}
	receiptObj, ok := receiptProof["receipt"].(map[string]interface{})
	if !ok {
		return map[string]interface{}{"valid": false, "message": "missing receipt object"}
	}
	var receipt receiptRecord
	data, _ := json.Marshal(receiptObj)
	if err := json.Unmarshal(data, &receipt); err != nil {
		return map[string]interface{}{"valid": false, "message": err.Error()}
	}
	logIndex := quantityFromInterface(proof["logIndex"])
	if logIndex >= uint64(len(receipt.Logs)) {
		return map[string]interface{}{"valid": false, "message": "log index out of range"}
	}
	logObj, ok := proof["log"].(map[string]interface{})
	if !ok {
		return map[string]interface{}{"valid": false, "message": "missing log object"}
	}
	var claimed logRecord
	data, _ = json.Marshal(logObj)
	if err := json.Unmarshal(data, &claimed); err != nil {
		return map[string]interface{}{"valid": false, "message": err.Error()}
	}
	actual := receipt.Logs[logIndex]
	valid := logsEqual(actual, claimed)
	message := "log proof mismatch"
	if valid {
		message = "log proof verified"
	}
	return map[string]interface{}{
		"valid":           valid,
		"message":         message,
		"transactionHash": receipt.TransactionHash,
		"blockHash":       receipt.BlockHash,
		"logIndex":        toHex(logIndex),
	}
}

func logsEqual(a, b logRecord) bool {
	if normalizeAddress(a.Address) != normalizeAddress(b.Address) ||
		a.Data != b.Data ||
		a.BlockNumber != b.BlockNumber ||
		a.BlockHash != b.BlockHash ||
		a.TransactionHash != b.TransactionHash ||
		a.TransactionIndex != b.TransactionIndex ||
		a.LogIndex != b.LogIndex ||
		a.Removed != b.Removed ||
		len(a.Topics) != len(b.Topics) {
		return false
	}
	for i := range a.Topics {
		if !strings.EqualFold(a.Topics[i], b.Topics[i]) {
			return false
		}
	}
	return true
}

func (n *rpcNode) receiptsForBlock(block blockRecord) []receiptRecord {
	receipts := make([]receiptRecord, 0, len(block.Transactions))
	for _, tx := range block.Transactions {
		if receipt, ok := n.state.Receipts[strings.ToLower(tx.Hash)]; ok {
			receipts = append(receipts, receipt)
		}
	}
	return receipts
}

func (n *rpcNode) accountProof(addr string, storageKeys []string, blockRef string) (map[string]interface{}, error) {
	addr = normalizeAddress(addr)
	snapshot, err := n.snapshotForBlock(blockRef)
	if err != nil {
		return nil, err
	}
	tr := buildStateTrieFromSnapshot(snapshot)
	stateRoot := tr.Hash()
	if snapshot.StateRoot != "" && snapshot.StateRoot != zeroHash() && !strings.EqualFold(snapshot.StateRoot, stateRoot.Hex()) {
		return nil, fmt.Errorf("state snapshot root mismatch at block %s: snapshot %s computed %s", toHex(snapshot.Height), snapshot.StateRoot, stateRoot.Hex())
	}
	accountProof := trienode.ProofList{}
	_ = tr.Prove(crypto.Keccak256(common.HexToAddress(addr).Bytes()), &accountProof)
	storageRoot := storageRootFromSnapshot(snapshot, addr)
	storageProofs := make([]interface{}, 0, len(storageKeys))
	for _, key := range storageKeys {
		key = normalizeStorageKey(key)
		storageTrie := buildStorageTrieFromSnapshot(snapshot, addr)
		proof := trienode.ProofList{}
		_ = storageTrie.Prove(crypto.Keccak256(common.FromHex(key)), &proof)
		value := zeroHash()
		if snapshot.Storage[addr] != nil {
			value = stringOrDefault(snapshot.Storage[addr][key], zeroHash())
		}
		storageProofs = append(storageProofs, map[string]interface{}{
			"key":   key,
			"value": value,
			"proof": proofHexList(proof),
		})
	}
	return map[string]interface{}{
		"address":      addr,
		"balance":      stringOrDefault(snapshot.Balances[addr], "0x0"),
		"nonce":        toHex(snapshot.Nonces[addr]),
		"codeHash":     codeHashFromSnapshot(snapshot, addr).Hex(),
		"storageHash":  storageRoot,
		"stateRoot":    stateRoot.Hex(),
		"blockNumber":  toHex(snapshot.Height),
		"blockHash":    snapshot.BlockHash,
		"accountProof": proofHexList(accountProof),
		"storageProof": storageProofs,
	}, nil
}

func (n *rpcNode) storageRoot(addr string) string {
	return storageRootFromSnapshot(n.currentStateSnapshot("", ""), addr)
}

func (n *rpcNode) buildStorageTrie(addr string) *trie.Trie {
	return buildStorageTrieFromSnapshot(n.currentStateSnapshot("", ""), addr)
}

func storageRootFromSnapshot(snapshot stateSnapshotRecord, addr string) string {
	return buildStorageTrieFromSnapshot(snapshot, addr).Hash().Hex()
}

func buildStorageTrieFromSnapshot(snapshot stateSnapshotRecord, addr string) *trie.Trie {
	addr = normalizeAddress(addr)
	tr := trie.NewEmpty(nil)
	keys := make([]string, 0, len(snapshot.Storage[addr]))
	for key := range snapshot.Storage[addr] {
		keys = append(keys, normalizeStorageKey(key))
	}
	sort.Strings(keys)
	for _, key := range keys {
		value := snapshot.Storage[addr][key]
		encoded, err := rlp.EncodeToBytes(common.TrimLeftZeroes(common.FromHex(value)))
		if err != nil {
			continue
		}
		tr.MustUpdate(crypto.Keccak256(common.FromHex(key)), encoded)
	}
	return tr
}

func (n *rpcNode) persistStorageTrie(addr string) string {
	tr := n.buildStorageTrie(addr)
	root, nodes := tr.Commit(true)
	n.persistTrieNodes(root.Hex(), "storage:"+addr, n.state.Height, nodes)
	return root.Hex()
}

func (n *rpcNode) codeHash(addr string) common.Hash {
	return codeHashFromSnapshot(n.currentStateSnapshot("", ""), addr)
}

func codeHashFromSnapshot(snapshot stateSnapshotRecord, addr string) common.Hash {
	code := common.FromHex(snapshot.Code[normalizeAddress(addr)])
	if len(code) == 0 {
		return types.EmptyCodeHash
	}
	return crypto.Keccak256Hash(code)
}

func sortedSnapshotAccounts(snapshot stateSnapshotRecord) []string {
	seen := map[string]bool{}
	for account := range snapshot.Balances {
		seen[normalizeAddress(account)] = true
	}
	for account := range snapshot.Nonces {
		seen[normalizeAddress(account)] = true
	}
	for account := range snapshot.Code {
		seen[normalizeAddress(account)] = true
	}
	for account := range snapshot.Storage {
		seen[normalizeAddress(account)] = true
	}
	accounts := make([]string, 0, len(seen))
	for account := range seen {
		if common.IsHexAddress(account) {
			accounts = append(accounts, account)
		}
	}
	sort.Strings(accounts)
	return accounts
}

func proofHexList(proof trienode.ProofList) []string {
	out := make([]string, 0, len(proof))
	for _, node := range proof {
		out = append(out, "0x"+hex.EncodeToString(node))
	}
	return out
}

func proofSetFromHex(nodes []string) (*trienode.ProofSet, error) {
	proof := trienode.ProofList{}
	for _, node := range nodes {
		blob := common.FromHex(node)
		if len(blob) == 0 {
			return nil, fmt.Errorf("empty proof node")
		}
		proof = append(proof, blob)
	}
	return proof.Set(), nil
}

func proofSetFromCommit(commit trieCommitRecord) (*trienode.ProofSet, error) {
	proof := trienode.NewProofSet()
	for _, node := range commit.Nodes {
		if node.Deleted {
			continue
		}
		hash := common.FromHex(node.Hash)
		blob := common.FromHex(node.Blob)
		if len(hash) == 0 || len(blob) == 0 {
			continue
		}
		if err := proof.Put(hash, blob); err != nil {
			return nil, err
		}
	}
	if proof.KeyCount() == 0 {
		return nil, fmt.Errorf("trie commit has no readable nodes")
	}
	return proof, nil
}

func receiptFromInterface(value interface{}) (receiptRecord, error) {
	var receipt receiptRecord
	if value == nil {
		return receipt, fmt.Errorf("missing receipt object")
	}
	data, err := json.Marshal(value)
	if err != nil {
		return receipt, err
	}
	if err := json.Unmarshal(data, &receipt); err != nil {
		return receipt, err
	}
	if receipt.TransactionHash == "" {
		return receipt, fmt.Errorf("missing receipt transaction hash")
	}
	return receipt, nil
}

func copyStringMap(input map[string]string) map[string]string {
	out := make(map[string]string, len(input))
	for key, value := range input {
		out[normalizeAddress(key)] = value
	}
	return out
}

func copyNonceMap(input map[string]uint64) map[string]uint64 {
	out := make(map[string]uint64, len(input))
	for key, value := range input {
		out[normalizeAddress(key)] = value
	}
	return out
}

func copyNestedStringMap(input map[string]map[string]string) map[string]map[string]string {
	out := make(map[string]map[string]string, len(input))
	for account, slots := range input {
		account = normalizeAddress(account)
		out[account] = make(map[string]string, len(slots))
		for key, value := range slots {
			out[account][normalizeStorageKey(key)] = value
		}
	}
	return out
}

func copyReceiptMap(input map[string]receiptRecord) map[string]receiptRecord {
	out := make(map[string]receiptRecord, len(input))
	for key, receipt := range input {
		receipt.Logs = append([]logRecord(nil), receipt.Logs...)
		out[key] = receipt
	}
	return out
}

func copyWithdrawalMap(input map[string]withdrawalRecord) map[string]withdrawalRecord {
	out := make(map[string]withdrawalRecord, len(input))
	for key, withdrawal := range input {
		out[key] = withdrawal
	}
	return out
}

func copyAnchorMap(input map[uint64]anchorRecord) map[uint64]anchorRecord {
	out := make(map[uint64]anchorRecord, len(input))
	for key, anchor := range input {
		out[key] = anchor
	}
	return out
}

func receiptBloomHex(receipt receiptRecord) string {
	bloom := types.CreateBloom(receipt.toGethReceipt())
	return "0x" + hex.EncodeToString(bloom.Bytes())
}

func blockLogsBloomHex(receipts []receiptRecord) string {
	gethReceipts := make(types.Receipts, 0, len(receipts))
	for _, receipt := range receipts {
		r := receipt.toGethReceipt()
		r.Bloom = types.CreateBloom(r)
		gethReceipts = append(gethReceipts, r)
	}
	bloom := types.MergeBloom(gethReceipts)
	return "0x" + hex.EncodeToString(bloom.Bytes())
}

func (n *rpcNode) handleReorg(height uint64, newHash string) map[string]interface{} {
	if height <= 1 {
		return map[string]interface{}{"reorg": false, "height": toHex(height), "message": "cannot reorg genesis"}
	}
	oldHash := n.state.Canonical[height]
	if oldHash == "" || strings.EqualFold(oldHash, newHash) {
		return map[string]interface{}{"reorg": false, "height": toHex(height), "hash": oldHash}
	}
	removed := 0
	rollbackHeight := height - 1
	snapshot, ok := n.state.StateSnapshots[rollbackHeight]
	if !ok {
		return map[string]interface{}{"reorg": false, "height": toHex(height), "message": "rollback snapshot not found"}
	}
	removedTxs := []txRecord{}
	removedTxHashes := map[string]bool{}
	filteredBlocks := n.state.Blocks[:0]
	for _, block := range n.state.Blocks {
		blockHeight := parseQuantity(block.Number).Uint64()
		if blockHeight >= height {
			removed++
			for _, tx := range block.Transactions {
				removedTxHashes[strings.ToLower(tx.Hash)] = true
				tx.BlockHash = ""
				tx.BlockNumber = ""
				tx.TransactionIndex = ""
				tx.Confirmations = 0
				removedTxs = append(removedTxs, tx)
			}
			delete(n.state.Canonical, blockHeight)
			continue
		}
		filteredBlocks = append(filteredBlocks, block)
	}
	n.state.Blocks = filteredBlocks
	n.restoreStateSnapshot(snapshot)

	filteredTxs := n.state.Txs[:0]
	for _, tx := range n.state.Txs {
		if removedTxHashes[strings.ToLower(tx.Hash)] {
			continue
		}
		filteredTxs = append(filteredTxs, tx)
	}
	n.state.Txs = filteredTxs
	for txHash := range removedTxHashes {
		delete(n.state.Receipts, txHash)
	}
	filteredLogs := n.state.Logs[:0]
	removedLogs := 0
	for _, lg := range n.state.Logs {
		if parseQuantity(lg.BlockNumber).Uint64() >= height || removedTxHashes[strings.ToLower(lg.TransactionHash)] {
			removedLogs++
			continue
		}
		filteredLogs = append(filteredLogs, lg)
	}
	n.state.Logs = filteredLogs
	for id, withdrawal := range n.state.Withdrawals {
		if removedTxHashes[strings.ToLower(withdrawal.L2TxHash)] {
			delete(n.state.Withdrawals, id)
		}
	}
	for anchorHeight := range n.state.Anchors {
		if anchorHeight >= height {
			delete(n.state.Anchors, anchorHeight)
		}
	}
	for snapshotHeight := range n.state.StateSnapshots {
		if snapshotHeight >= height {
			delete(n.state.StateSnapshots, snapshotHeight)
		}
	}
	filteredCommits := n.trieCommits[:0]
	for _, commit := range n.trieCommits {
		if commit.Height >= height {
			continue
		}
		filteredCommits = append(filteredCommits, commit)
	}
	n.trieCommits = filteredCommits
	n.state.PendingTxs = append(n.state.PendingTxs, removedTxs...)
	n.save()
	return map[string]interface{}{
		"reorg":         true,
		"height":        toHex(height),
		"rollbackTo":    toHex(rollbackHeight),
		"oldHash":       oldHash,
		"newHash":       newHash,
		"removedBlocks": removed,
		"removedTxs":    len(removedTxs),
		"removedLogs":   removedLogs,
		"requeuedTxs":   len(removedTxs),
	}
}

func (n *rpcNode) restoreStateSnapshot(snapshot stateSnapshotRecord) {
	n.state.Height = snapshot.Height
	n.state.Balances = copyStringMap(snapshot.Balances)
	n.state.Nonces = copyNonceMap(snapshot.Nonces)
	n.state.Code = copyStringMap(snapshot.Code)
	n.state.Storage = copyNestedStringMap(snapshot.Storage)
}

func (n *rpcNode) securityStatus() map[string]interface{} {
	return map[string]interface{}{
		"strictMode":       n.strictMode,
		"autoMine":         n.autoMine,
		"minGasLimit":      toHex(n.minGasLimit),
		"maxGasLimit":      toHex(n.maxGasLimit),
		"challengeSeconds": n.challengeSeconds,
		"bridgeAddress":    n.bridgeAddress,
		"bhtRpcConfigured": n.bhtRPC != "",
	}
}

func (r receiptRecord) toGethReceipt() *types.Receipt {
	logs := make([]*types.Log, 0, len(r.Logs))
	for _, lg := range r.Logs {
		logs = append(logs, lg.toGethLog())
	}
	receipt := &types.Receipt{
		Type:              types.LegacyTxType,
		Status:            parseQuantity(r.Status).Uint64(),
		CumulativeGasUsed: parseQuantity(r.CumulativeGasUsed).Uint64(),
		Logs:              logs,
		TxHash:            common.HexToHash(r.TransactionHash),
		ContractAddress:   common.Address{},
		GasUsed:           parseQuantity(r.GasUsed).Uint64(),
		EffectiveGasPrice: big.NewInt(0),
		BlockHash:         common.HexToHash(r.BlockHash),
		BlockNumber:       new(big.Int).SetUint64(parseQuantity(r.BlockNumber).Uint64()),
		TransactionIndex:  uint(parseQuantity(r.TransactionIndex).Uint64()),
	}
	receipt.Bloom = types.CreateBloom(receipt)
	return receipt
}

func (l logRecord) toGethLog() *types.Log {
	topics := make([]common.Hash, 0, len(l.Topics))
	for _, topic := range l.Topics {
		topics = append(topics, common.HexToHash(topic))
	}
	return &types.Log{
		Address:     common.HexToAddress(l.Address),
		Topics:      topics,
		Data:        common.FromHex(l.Data),
		BlockNumber: parseQuantity(l.BlockNumber).Uint64(),
		TxHash:      common.HexToHash(l.TransactionHash),
		TxIndex:     uint(parseQuantity(l.TransactionIndex).Uint64()),
		BlockHash:   common.HexToHash(l.BlockHash),
		Index:       uint(parseQuantity(l.LogIndex).Uint64()),
		Removed:     l.Removed,
	}
}

func (n *rpcNode) callBHTRPC(method string, params []interface{}, result interface{}) error {
	payload := map[string]interface{}{
		"jsonrpc": "2.0",
		"id":      "bhte-bridge",
		"method":  method,
		"params":  params,
	}
	data, _ := json.Marshal(payload)
	req, err := http.NewRequest(http.MethodPost, n.bhtRPC, bytes.NewReader(data))
	if err != nil {
		return err
	}
	req.Header.Set("Content-Type", "application/json")
	if n.bhtRPCUser != "" {
		req.SetBasicAuth(n.bhtRPCUser, n.bhtRPCPass)
	}
	client := &http.Client{Timeout: 8 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	var out struct {
		Result json.RawMessage `json:"result"`
		Error  *rpcError       `json:"error"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&out); err != nil {
		return err
	}
	if out.Error != nil {
		return fmt.Errorf("%s", out.Error.Message)
	}
	return json.Unmarshal(out.Result, result)
}

func (n *rpcNode) balance(addr string) *big.Int {
	if addr == "" {
		return big.NewInt(0)
	}
	return parseQuantity(n.state.Balances[addr])
}

func (n *rpcNode) transactionsByAddress(addr string, limit int) []txRecord {
	if limit <= 0 {
		limit = 100
	}
	result := make([]txRecord, 0, limit)
	for i := len(n.state.Txs) - 1; i >= 0 && len(result) < limit; i-- {
		tx := n.state.Txs[i]
		if normalizeAddress(tx.From) == addr || normalizeAddress(tx.To) == addr {
			tx.Confirmations = int(n.state.Height) - len(n.state.Txs) + i + 1
			if tx.Confirmations < 0 {
				tx.Confirmations = 0
			}
			result = append(result, tx)
		}
	}
	return result
}

func parseParams(raw json.RawMessage) []interface{} {
	var params []interface{}
	_ = json.Unmarshal(raw, &params)
	return params
}

func stringParam(params []interface{}, index int) string {
	if index < 0 || index >= len(params) || params[index] == nil {
		return ""
	}
	return fmt.Sprint(params[index])
}

func intParam(params []interface{}, index int, fallback int) int {
	if index < 0 || index >= len(params) || params[index] == nil {
		return fallback
	}
	switch v := params[index].(type) {
	case float64:
		return int(v)
	case string:
		if n, err := strconv.Atoi(v); err == nil {
			return n
		}
	}
	return fallback
}

func boolParam(params []interface{}, index int, fallback bool) bool {
	if index < 0 || index >= len(params) || params[index] == nil {
		return fallback
	}
	if v, ok := params[index].(bool); ok {
		return v
	}
	return fallback
}

func quantityParam(params []interface{}, index int) uint64 {
	if index < 0 || index >= len(params) || params[index] == nil {
		return 0
	}
	return quantityFromInterface(params[index])
}

func quantityFromInterface(value interface{}) uint64 {
	switch v := value.(type) {
	case float64:
		return uint64(v)
	case string:
		return parseQuantity(v).Uint64()
	default:
		return parseQuantity(fmt.Sprint(v)).Uint64()
	}
}

func stringSliceParam(params []interface{}, index int) []string {
	if index < 0 || index >= len(params) || params[index] == nil {
		return nil
	}
	return stringSliceFromInterface(params[index])
}

func stringSliceFromInterface(value interface{}) []string {
	if value == nil {
		return nil
	}
	if strings, ok := value.([]string); ok {
		return strings
	}
	items, ok := value.([]interface{})
	if !ok {
		return nil
	}
	out := make([]string, 0, len(items))
	for _, item := range items {
		out = append(out, fmt.Sprint(item))
	}
	return out
}

func objectParam(params []interface{}, index int) (map[string]interface{}, error) {
	if index < 0 || index >= len(params) {
		return nil, fmt.Errorf("missing object parameter")
	}
	obj, ok := params[index].(map[string]interface{})
	if !ok {
		return nil, fmt.Errorf("parameter %d must be object", index)
	}
	return obj, nil
}

func callExternalRPC(endpoint, method string, params []interface{}, result interface{}) error {
	payload := map[string]interface{}{
		"jsonrpc": "2.0",
		"id":      "bhte-peer",
		"method":  method,
		"params":  params,
	}
	data, _ := json.Marshal(payload)
	client := &http.Client{Timeout: 5 * time.Second}
	resp, err := client.Post(endpoint, "application/json", bytes.NewReader(data))
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	var out struct {
		Result json.RawMessage `json:"result"`
		Error  *rpcError       `json:"error"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&out); err != nil {
		return err
	}
	if out.Error != nil {
		return fmt.Errorf("%s", out.Error.Message)
	}
	return json.Unmarshal(out.Result, result)
}

func structToMap(value interface{}) map[string]interface{} {
	data, _ := json.Marshal(value)
	out := map[string]interface{}{}
	_ = json.Unmarshal(data, &out)
	return out
}

func normalizeAddress(addr string) string {
	addr = strings.TrimSpace(strings.ToLower(addr))
	if addr == "" || addr == "<nil>" {
		return ""
	}
	if !strings.HasPrefix(addr, "0x") {
		addr = "0x" + addr
	}
	return addr
}

func normalizeStorageKey(key string) string {
	key = strings.TrimPrefix(strings.ToLower(strings.TrimSpace(key)), "0x")
	if key == "" || key == "<nil>" {
		key = "0"
	}
	if len(key) > 64 {
		key = key[len(key)-64:]
	}
	return "0x" + strings.Repeat("0", 64-len(key)) + key
}

func parseQuantity(value string) *big.Int {
	value = strings.TrimSpace(value)
	if value == "" || value == "<nil>" {
		return big.NewInt(0)
	}
	base := 10
	if strings.HasPrefix(value, "0x") || strings.HasPrefix(value, "0X") {
		value = value[2:]
		base = 16
	}
	n := new(big.Int)
	if _, ok := n.SetString(value, base); ok {
		return n
	}
	return big.NewInt(0)
}

func quantityToDecimal(value string) float64 {
	n := parseQuantity(value)
	rat := new(big.Rat).SetFrac(n, big.NewInt(100000000))
	f, _ := rat.Float64()
	return f
}

func stringOrDefault(value, fallback string) string {
	if value == "" || value == "<nil>" {
		return fallback
	}
	return value
}

func toHex(v uint64) string {
	return "0x" + strconv.FormatUint(v, 16)
}

func zeroHash() string {
	return "0x0000000000000000000000000000000000000000000000000000000000000000"
}

func hashHex(data []byte) string {
	sum := sha256.Sum256(data)
	return "0x" + hex.EncodeToString(sum[:])
}

func hashJSON(value interface{}) string {
	data, _ := json.Marshal(value)
	return hashHex(data)
}

func abiSelector(input string) string {
	input = strings.ToLower(strings.TrimSpace(input))
	if !strings.HasPrefix(input, "0x") || len(input) < 10 {
		return ""
	}
	return input[:10]
}

func contractAddress(from string, nonceHex string) string {
	nonce := parseQuantity(nonceHex).Uint64()
	return crypto.CreateAddress(common.HexToAddress(from), nonce).Hex()
}

func topicHash(name string) string {
	hash := crypto.Keccak256Hash([]byte(name))
	return hash.Hex()
}

func paddedTopic(addr string) string {
	addr = strings.TrimPrefix(normalizeAddress(addr), "0x")
	if len(addr) > 64 {
		addr = addr[len(addr)-64:]
	}
	return "0x" + strings.Repeat("0", 64-len(addr)) + addr
}
