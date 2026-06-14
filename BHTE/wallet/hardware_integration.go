// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// Hardware Wallet Integration for BHTE Wallet

package bhc

import (
	"encoding/hex"
	"errors"
	"fmt"

	"github.com/ethereum/go-ethereum/common"
	"github.com/ethereum/go-ethereum/crypto"

	"bhte/hardware"
	"bhte/mldsa"
)

type HardwareWalletBackend struct {
	wallet  *hardware.HardwareWalletImpl
	path    *hardware.HWWPath
	address common.Address
}

type HardwareWalletSigner struct {
	backend *HardwareWalletBackend
}

func NewHardwareWalletSigner(wallet hardware.HardwareWallet, path *hardware.HWWPath) (*HardwareWalletSigner, error) {
	addressStr, err := wallet.GetAddress(path)
	if err != nil {
		return nil, fmt.Errorf("failed to get address from hardware wallet: %w", err)
	}

	address := common.HexToAddress(addressStr)

	return &HardwareWalletSigner{
		backend: &HardwareWalletBackend{
			wallet:  wallet.(*hardware.HardwareWalletImpl),
			path:    path,
			address: address,
		},
	}, nil
}

func (s *HardwareWalletSigner) Address() common.Address {
	return s.backend.address
}

func (s *HardwareWalletSigner) SignMessage(message []byte) ([]byte, error) {
	sig, err := s.backend.wallet.SignMessage(s.backend.path, string(message))
	if err != nil {
		return nil, fmt.Errorf("hardware wallet sign message failed: %w", err)
	}

	sig[64] = 27

	return sig, nil
}

func (s *HardwareWalletSigner) SignTransaction(to common.Address, value, gasPrice, gasLimit uint64, data []byte) ([]byte, error) {
	txData := encodeEIP155Tx(to, value, gasPrice, gasLimit, data)

	sig, err := s.backend.wallet.SignTransaction(s.backend.path, txData)
	if err != nil {
		return nil, fmt.Errorf("hardware wallet sign transaction failed: %w", err)
	}

	sig[64] = 27 + (sig[64] & 1)

	return sig, nil
}

func encodeEIP155Tx(to common.Address, value, gasPrice, gasLimit uint64, data []byte) []byte {
	result := make([]byte, 0)

	nonce := make([]byte, 32)
	nonce[31] = 0x01
	result = append(result, nonce...)

	gasPriceBytes := make([]byte, 32)
	putUint64LE(gasPriceBytes, gasPrice)
	result = append(result, gasPriceBytes...)

	gasLimitBytes := make([]byte, 32)
	putUint64LE(gasLimitBytes, gasLimit)
	result = append(result, gasLimitBytes...)

	toBytes := to.Bytes()
	padding := make([]byte, 32)
	copy(padding[12:], toBytes)
	result = append(result, padding...)

	valueBytes := make([]byte, 32)
	putUint64LE(valueBytes, value)
	result = append(result, valueBytes...)

	dataLenBytes := make([]byte, 32)
	putUint64LE(dataLenBytes, uint64(len(data)))
	result = append(result, dataLenBytes...)
	result = append(result, data...)

	chainIDBytes := make([]byte, 32)
	putUint64LE(chainIDBytes, 1)
	result = append(result, chainIDBytes...)

	result = append(result, []byte{0x80, 0x80}...)

	return result
}

func putUint64LE(b []byte, v uint64) {
	b[0] = byte(v & 0xFF)
	b[1] = byte((v >> 8) & 0xFF)
	b[2] = byte((v >> 16) & 0xFF)
	b[3] = byte((v >> 24) & 0xFF)
	b[4] = byte((v >> 32) & 0xFF)
	b[5] = byte((v >> 40) & 0xFF)
	b[6] = byte((v >> 48) & 0xFF)
	b[7] = byte((v >> 56) & 0xFF)
}

type MultiModeWallet struct {
	softwareWallet *Wallet
	hardwareWallet *HardwareWalletSigner
	mldsaSigner   *mldsa.MLSigner
	mode          WalletMode
}

func NewMultiModeWallet(config *WalletConfig) (*MultiModeWallet, error) {
	softwareWallet, err := NewWallet(config)
	if err != nil {
		return nil, err
	}

	return &MultiModeWallet{
		softwareWallet: softwareWallet,
		mode:          config.Mode,
	}, nil
}

func (w *MultiModeWallet) Address() common.Address {
	if w.hardwareWallet != nil {
		return w.hardwareWallet.Address()
	}
	return w.softwareWallet.address
}

func (w *MultiModeWallet) UseHardwareWallet(hw hardware.HardwareWallet, path *hardware.HWWPath) error {
	signer, err := NewHardwareWalletSigner(hw, path)
	if err != nil {
		return err
	}
	w.hardwareWallet = signer
	return nil
}

func (w *MultiModeWallet) UseMLDSA(seed []byte) error {
	config := &mldsa.MLSignerConfig{
		ParameterSet: mldsa.MLDSA65,
	}
	signer, err := mldsa.NewMLSigner(seed, config)
	if err != nil {
		return err
	}
	w.mldsaSigner = signer
	return nil
}

func (w *MultiModeWallet) SignMessage(message []byte) ([]byte, error) {
	switch w.mode {
	case WalletModeMLDSA:
		if w.mldsaSigner != nil {
			return w.mldsaSigner.Sign(message)
		}
		return w.softwareWallet.SignMessage(message)

	case WalletModeHybrid:
		ecdsaSig, err := w.softwareWallet.SignMessage(message)
		if err != nil {
			return nil, err
		}

		if w.mldsaSigner != nil {
			mldsaSig, err := w.mldsaSigner.Sign(message)
			if err == nil {
				return append(ecdsaSig, mldsaSig...), nil
			}
		}

		return ecdsaSig, nil

	default:
		if w.hardwareWallet != nil {
			return w.hardwareWallet.SignMessage(message)
		}
		return w.softwareWallet.SignMessage(message)
	}
}

func (w *MultiModeWallet) Sign(data []byte) ([]byte, error) {
	if w.hardwareWallet != nil {
		return w.hardwareWallet.backend.wallet.SignMessage(w.hardwareWallet.backend.path, string(data))
	}
	return w.softwareWallet.Sign(data)
}

func (w *MultiModeWallet) SignTransaction(to common.Address, value, gasPrice, gasLimit uint64, data []byte) (common.Hash, error) {
	var sig []byte
	var err error

	switch w.mode {
	case WalletModeMLDSA:
		if w.mldsaSigner != nil {
			txHash := crypto.Keccak256Hash(encodeEIP155TxForHash(to, value, gasPrice, gasLimit, data))
			sig, err = w.mldsaSigner.Sign(txHash.Bytes())
			if err != nil {
				return common.Hash{}, err
			}
		} else {
			txHash, err := w.softwareWallet.Signtx(to, value, gasPrice, gasLimit, data)
			if err != nil {
				return common.Hash{}, err
			}
			sig = txHash[:]
		}

	case WalletModeHybrid:
		txHash, err := w.softwareWallet.Signtx(to, value, gasPrice, gasLimit, data)
		if err != nil {
			return common.Hash{}, err
		}
		sig = txHash[:]

		if w.mldsaSigner != nil {
			txHash := crypto.Keccak256Hash(encodeEIP155TxForHash(to, value, gasPrice, gasLimit, data))
			mldsaSig, err := w.mldsaSigner.Sign(txHash.Bytes())
			if err == nil {
				sig = append(sig, mldsaSig...)
			}
		}

	default:
		if w.hardwareWallet != nil {
			sig, err = w.hardwareWallet.SignTransaction(to, value, gasPrice, gasLimit, data)
			if err != nil {
				return common.Hash{}, err
			}
		} else {
			return w.softwareWallet.Signtx(to, value, gasPrice, gasLimit, data)
		}
	}

	txHash := crypto.Keccak256Hash(sig)
	return txHash, nil
}

func encodeEIP155TxForHash(to common.Address, value, gasPrice, gasLimit uint64, data []byte) []byte {
	rlpData := make([]byte, 0)

	nonce := []byte{0x01}
	rlpData = append(rlpData, nonce...)

	gasPriceBytes := make([]byte, 8)
	putUint64LE(gasPriceBytes, gasPrice)
	rlpData = append(rlpData, gasPriceBytes...)

	gasLimitBytes := make([]byte, 8)
	putUint64LE(gasLimitBytes, gasLimit)
	rlpData = append(rlpData, gasLimitBytes...)

	rlpData = append(rlpData, to.Bytes()...)

	valueBytes := make([]byte, 8)
	putUint64LE(valueBytes, value)
	rlpData = append(rlpData, valueBytes...)

	rlpData = append(rlpData, data...)

	return rlpData
}

func (w *MultiModeWallet) Verify(signature, data []byte) bool {
	if len(signature) == 65 {
		return w.softwareWallet.Verify(signature, data)
	}

	if len(signature) > 65 && w.mldsaSigner != nil {
		ecdsaSig := signature[:65]
		mldsaSig := signature[65:]

		ecdsaValid := w.softwareWallet.Verify(ecdsaSig, data)
		if !ecdsaValid {
			return false
		}

		err := w.mldsaSigner.Verify(data, mldsaSig)
		return err == nil
	}

	return false
}

func ConnectHardwareWallet(path string) (hardware.HardwareWallet, error) {
	devices, err := hardware.EnumerateDevices()
	if err != nil {
		return nil, fmt.Errorf("failed to enumerate devices: %w", err)
	}

	for _, info := range devices {
		if info.Path == path {
			return hardware.ConnectDevice(info)
		}
	}

	return nil, errors.New("hardware wallet not found at specified path")
}

func ConnectLedgerDevice(path string) (hardware.HardwareWallet, error) {
	devices, err := hardware.EnumerateDevices()
	if err != nil {
		return nil, fmt.Errorf("failed to enumerate devices: %w", err)
	}

	for _, info := range devices {
		if info.Path == path && info.Type == hardware.WalletTypeLedger {
			return hardware.ConnectDevice(info)
		}
	}

	return nil, errors.New("Ledger device not found")
}

func ConnectTrezorDevice(path string) (hardware.HardwareWallet, error) {
	devices, err := hardware.EnumerateDevices()
	if err != nil {
		return nil, fmt.Errorf("failed to enumerate devices: %w", err)
	}

	for _, info := range devices {
		if info.Path == path && info.Type == hardware.WalletTypeTrezor {
			return hardware.ConnectDevice(info)
		}
	}

	return nil, errors.New("Trezor device not found")
}

func DeriveBHTETestingPath(account uint32) *hardware.HWWPath {
	return hardware.DeriveBIP44Path(hardware.GetBHTECoinType(), account, false, 0)
}

func DeriveBHTCHDPath(account uint32) *hardware.HWWPath {
	return hardware.DeriveBIP44Path(hardware.GetBHTCoinType(), account, false, 0)
}

func ExportPublicKey(hw hardware.HardwareWallet, path *hardware.HWWPath) (string, error) {
	pubKey, err := hw.GetPublicKey(path)
	if err != nil {
		return "", err
	}
	return "0x" + hex.EncodeToString(pubKey), nil
}
