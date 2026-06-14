// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// BHTE Wallet Integration Module

package bhc

import (
	"crypto/ecdsa"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"math/big"
	"os"
	"sync"

	"github.com/ethereum/go-ethereum/common"
	"github.com/ethereum/go-ethereum/crypto"
)

type WalletMode uint8

const (
	WalletModeECDSA WalletMode = iota
	WalletModeMLDSA
	WalletModeHybrid
)

type WalletConfig struct {
	Mode WalletMode
	MLDSAEnabled bool
	RequireQuantumSafe bool
}

type Wallet struct {
	config     *WalletConfig
	privateKey *ecdsa.PrivateKey
	address    common.Address
	balance    uint64
	pendingTxs map[common.Hash]*PendingTx
	mu         sync.RWMutex
}

type PendingTx struct {
	Hash      common.Hash
	To        common.Address
	Value     uint64
	GasPrice  uint64
	GasLimit  uint64
	Nonce     uint64
	Timestamp int64
}

func getEnvBool(key string, defaultVal bool) bool {
	if val := os.Getenv(key); val != "" {
		if val == "1" || val == "true" || val == "TRUE" {
			return true
		} else if val == "0" || val == "false" || val == "FALSE" {
			return false
		}
	}
	return defaultVal
}

func getEnvWalletMode() WalletMode {
	if val := os.Getenv("BHC_WALLET_MODE"); val != "" {
		switch val {
		case "ECDSA":
			return WalletModeECDSA
		case "MLDSA":
			return WalletModeMLDSA
		case "Hybrid":
			return WalletModeHybrid
		}
	}
	return WalletModeHybrid
}

func NewWallet(config *WalletConfig) (*Wallet, error) {
	if config == nil {
		config = &WalletConfig{
			Mode:             getEnvWalletMode(),
			MLDSAEnabled:     getEnvBool("BHC_MLDSA_ENABLED", true),
			RequireQuantumSafe: getEnvBool("BHC_REQUIRE_QUANTUM_SAFE", false),
		}
	}

	privateKey, err := crypto.GenerateKey()
	if err != nil {
		return nil, fmt.Errorf("failed to generate key: %w", err)
	}

	publicKey := privateKey.Public()
	publicKeyECDSA, ok := publicKey.(*ecdsa.PublicKey)
	if !ok {
		return nil, fmt.Errorf("failed to cast public key")
	}

	address := crypto.PubkeyToAddress(*publicKeyECDSA)

	return &Wallet{
		config:     config,
		privateKey: privateKey,
		address:    address,
		balance:    0,
		pendingTxs: make(map[common.Hash]*PendingTx),
	}, nil
}

func NewWalletFromKey(privateKeyHex string, config *WalletConfig) (*Wallet, error) {
	if config == nil {
		config = &WalletConfig{
			Mode:             getEnvWalletMode(),
			MLDSAEnabled:     getEnvBool("BHC_MLDSA_ENABLED", true),
			RequireQuantumSafe: getEnvBool("BHC_REQUIRE_QUANTUM_SAFE", false),
		}
	}

	privateKeyBytes, err := hex.DecodeString(privateKeyHex)
	if err != nil {
		return nil, fmt.Errorf("invalid private key hex: %w", err)
	}

	if len(privateKeyBytes) != 32 {
		return nil, fmt.Errorf("invalid private key length: expected 32 bytes, got %d", len(privateKeyBytes))
	}

	privateKey := new(ecdsa.PrivateKey)
	privateKey.D = new(big.Int).SetBytes(privateKeyBytes)
	privateKey.PublicKey.Curve = crypto.S256()

	curveN := crypto.S256().Params().N
	if privateKey.D.Sign() <= 0 {
		return nil, fmt.Errorf("private key must be greater than 0")
	}
	if privateKey.D.Cmp(curveN) >= 0 {
		return nil, fmt.Errorf("private key must be less than curve order")
	}

	if !privateKey.Curve.IsOnCurve(privateKey.PublicKey.X, privateKey.PublicKey.Y) {
		return nil, fmt.Errorf("private key is not valid")
	}

	publicKey := privateKey.Public()
	publicKeyECDSA, ok := publicKey.(*ecdsa.PublicKey)
	if !ok {
		return nil, fmt.Errorf("failed to cast public key")
	}

	address := crypto.PubkeyToAddress(*publicKeyECDSA)

	return &Wallet{
		config:     config,
		privateKey: privateKey,
		address:    address,
		balance:    0,
		pendingTxs: make(map[common.Hash]*PendingTx),
	}, nil
}

func (w *Wallet) Address() common.Address {
	return w.address
}

func (w *Wallet) PublicKey() []byte {
	return crypto.FromECDSAPub(&w.privateKey.PublicKey)
}

func (w *Wallet) Balance() uint64 {
	w.mu.RLock()
	defer w.mu.RUnlock()
	return w.balance
}

func (w *Wallet) SetBalance(balance uint64) {
	w.mu.Lock()
	defer w.mu.Unlock()
	w.balance = balance
}

func (w *Wallet) Mode() WalletMode {
	return w.config.Mode
}

func (w *Wallet) Signtx(to common.Address, value, gasPrice, gasLimit uint64, data []byte) (common.Hash, error) {
	w.mu.Lock()
	defer w.mu.Unlock()

	nonce := uint64(len(w.pendingTxs))

	txHash := w.calculateTxHash(to, value, gasPrice, gasLimit, nonce, data)

	pendingTx := &PendingTx{
		Hash:      txHash,
		To:        to,
		Value:     value,
		GasPrice:  gasPrice,
		GasLimit:  gasLimit,
		Nonce:     nonce,
		Timestamp: 0,
	}

	w.pendingTxs[txHash] = pendingTx

	return txHash, nil
}

func (w *Wallet) Sign(data []byte) ([]byte, error) {
	w.mu.RLock()
	defer w.mu.RUnlock()

	hash := sha256.Sum256(data)

	signature, err := crypto.Sign(hash[:], w.privateKey)
	if err != nil {
		return nil, fmt.Errorf("failed to sign: %w", err)
	}

	return signature, nil
}

func (w *Wallet) SignMessage(message []byte) ([]byte, error) {
	w.mu.RLock()
	defer w.mu.RUnlock()

	signedMessageHash := crypto.Keccak256Hash(
		[]byte("\x19Ethereum Signed Message:\n"),
		[]byte(fmt.Sprintf("%d", len(message))),
		message,
	)

	signature, err := crypto.Sign(signedMessageHash.Bytes(), w.privateKey)
	if err != nil {
		return nil, fmt.Errorf("failed to sign message: %w", err)
	}

	return signature, nil
}

func (w *Wallet) Verify(signature, data []byte) bool {
	w.mu.RLock()
	defer w.mu.RUnlock()

	if len(signature) != 65 {
		return false
	}

	hash := sha256.Sum256(data)

	pubKey, err := crypto.SigToPub(hash[:], signature)
	if err != nil {
		return false
	}

	recoveredAddr := crypto.PubkeyToAddress(*pubKey)

	return recoveredAddr == w.address
}

func (w *Wallet) calculateTxHash(to common.Address, value, gasPrice, gasLimit, nonce uint64, data []byte) common.Hash {
	w.mu.RLock()
	defer w.mu.RUnlock()

	txData := fmt.Sprintf("%s%d%d%d%d%s",
		to.Hex(),
		value,
		gasPrice,
		gasLimit,
		nonce,
		hex.EncodeToString(data),
	)

	hash := crypto.Keccak256Hash([]byte(txData))

	return hash
}

func (w *Wallet) GetPendingTx(hash common.Hash) (*PendingTx, bool) {
	w.mu.RLock()
	defer w.mu.RUnlock()

	tx, exists := w.pendingTxs[hash]
	return tx, exists
}

func (w *Wallet) RemovePendingTx(hash common.Hash) {
	w.mu.Lock()
	defer w.mu.Unlock()

	delete(w.pendingTxs, hash)
}

func (w *Wallet) GetPendingTxCount() int {
	w.mu.RLock()
	defer w.mu.RUnlock()

	return len(w.pendingTxs)
}

func (w *Wallet) String() string {
	return fmt.Sprintf("Wallet{Address: %s, Balance: %d, Mode: %d, PendingTxs: %d}",
		w.address.Hex(),
		w.Balance(),
		w.config.Mode,
		len(w.pendingTxs),
	)
}

func GenerateWalletAddress() (common.Address, error) {
	privateKey, err := crypto.GenerateKey()
	if err != nil {
		return common.Address{}, fmt.Errorf("failed to generate key: %w", err)
	}

	publicKey := privateKey.Public()
	publicKeyECDSA, ok := publicKey.(*ecdsa.PublicKey)
	if !ok {
		return common.Address{}, fmt.Errorf("failed to cast public key")
	}

	return crypto.PubkeyToAddress(*publicKeyECDSA), nil
}

func ValidateAddress(addr common.Address) bool {
	return addr != common.Address{} && addr != common.HexToAddress("0x0000000000000000000000000000000000000000")
}

type WalletManager struct {
	wallets     map[common.Address]*Wallet
	defaultAddr common.Address
	mu          sync.RWMutex
}

func NewWalletManager() *WalletManager {
	return &WalletManager{
		wallets: make(map[common.Address]*Wallet),
	}
}

func (m *WalletManager) AddWallet(wallet *Wallet) {
	m.mu.Lock()
	defer m.mu.Unlock()

	m.wallets[wallet.Address()] = wallet

	if m.defaultAddr == (common.Address{}) {
		m.defaultAddr = wallet.Address()
	}
}

func (m *WalletManager) GetWallet(addr common.Address) (*Wallet, bool) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	wallet, exists := m.wallets[addr]
	return wallet, exists
}

func (m *WalletManager) GetDefaultWallet() (*Wallet, bool) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	wallet, exists := m.wallets[m.defaultAddr]
	return wallet, exists
}

func (m *WalletManager) RemoveWallet(addr common.Address) {
	m.mu.Lock()
	defer m.mu.Unlock()

	delete(m.wallets, addr)

	if m.defaultAddr == addr {
		m.defaultAddr = common.Address{}
		for _, wallet := range m.wallets {
			m.defaultAddr = wallet.Address()
			break
		}
	}
}

func (m *WalletManager) ListWallets() []*Wallet {
	m.mu.RLock()
	defer m.mu.RUnlock()

	wallets := make([]*Wallet, 0, len(m.wallets))
	for _, wallet := range m.wallets {
		wallets = append(wallets, wallet)
	}

	return wallets
}

func (m *WalletManager) SetDefaultWallet(addr common.Address) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	if _, exists := m.wallets[addr]; !exists {
		return fmt.Errorf("wallet not found: %s", addr.Hex())
	}

	m.defaultAddr = addr
	return nil
}

type KeyDerivation interface {
	DerivePrivateKey(path string) (*ecdsa.PrivateKey, error)
}

type HDWallet struct {
	masterKey []byte
	walletMgr *WalletManager
}

func NewHDWallet(mnemonic string) (*HDWallet, error) {
	seed := sha256.Sum256([]byte(mnemonic))

	masterKey := make([]byte, 32)
	rand.Read(masterKey)
	copy(masterKey, seed[:32])

	return &HDWallet{
		masterKey: masterKey,
		walletMgr: NewWalletManager(),
	}, nil
}

func (h *HDWallet) DeriveChildKey(index uint32) ([]byte, error) {
	hash := sha256.Sum256(append(h.masterKey, uint32ToBytes(index)...))
	return hash[:32], nil
}

func uint32ToBytes(n uint32) []byte {
	b := make([]byte, 4)
	b[0] = byte(n >> 24)
	b[1] = byte(n >> 16)
	b[2] = byte(n >> 8)
	b[3] = byte(n)
	return b
}

type Transaction struct {
	From     common.Address
	To       common.Address
	Value    uint64
	GasPrice  uint64
	GasLimit  uint64
	Nonce     uint64
	Data      []byte
	Signature []byte
	Hash      common.Hash
}

func (t *Transaction) Sign(privateKey *ecdsa.PrivateKey) error {
	txHash := t.CalculateHash()

	signature, err := crypto.Sign(txHash.Bytes(), privateKey)
	if err != nil {
		return err
	}

	t.Signature = signature
	t.Hash = t.CalculateHash()

	return nil
}

func (t *Transaction) CalculateHash() common.Hash {
	data := fmt.Sprintf("%s%s%d%d%d%d%s",
		t.From.Hex(),
		t.To.Hex(),
		t.Value,
		t.GasPrice,
		t.GasLimit,
		t.Nonce,
		hex.EncodeToString(t.Data),
	)

	return crypto.Keccak256Hash([]byte(data))
}

func (t *Transaction) VerifySignature() bool {
	if len(t.Signature) != 65 {
		return false
	}

	txHash := t.CalculateHash()

	pubKey, err := crypto.SigToPub(txHash.Bytes(), t.Signature)
	if err != nil {
		return false
	}

	signerAddr := crypto.PubkeyToAddress(*pubKey)

	return signerAddr == t.From
}
