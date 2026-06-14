// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// BHTE Hardware Wallet Support - Cross-Platform HID Interface

package hardware

import (
	"encoding/hex"
	"errors"
	"fmt"
	"sync"
)

type WalletType uint8

const (
	WalletTypeUnknown WalletType = iota
	WalletTypeLedger
	WalletTypeTrezor
	WalletTypeBitBox02
	WalletTypeKeystone
	WalletTypeColdcard
	WalletTypeJade
)

type HardwareError uint8

const (
	HardwareErrorNone HardwareError = iota
	HardwareErrorNotConnected
	HardwareErrorDeviceNotFound
	HardwareErrorTimeout
	HardwareErrorInvalidData
	HardwareErrorDenied
	HardwareErrorCancelled
	HardwareErrorUnknown
)

func (e HardwareError) String() string {
	switch e {
	case HardwareErrorNone:
		return "No error"
	case HardwareErrorNotConnected:
		return "Device not connected"
	case HardwareErrorDeviceNotFound:
		return "Device not found"
	case HardwareErrorTimeout:
		return "Operation timed out"
	case HardwareErrorInvalidData:
		return "Invalid data received"
	case HardwareErrorDenied:
		return "Operation denied by user"
	case HardwareErrorCancelled:
		return "Operation cancelled"
	default:
		return "Unknown error occurred"
	}
}

type WalletInfo struct {
	Type             WalletType
	Manufacturer     string
	Product          string
	SerialNumber     string
	Path             string
	Connected        bool
	HasScreen        bool
	HasKeypad        bool
	FirmwareVersion  string
}

type HWWPath struct {
	Path []uint32
}

func ParseBIP32Path(pathStr string) (*HWWPath, error) {
	path := &HWWPath{Path: make([]uint32, 0)}

	if len(pathStr) == 0 {
		return nil, errors.New("empty path")
	}

	if pathStr[0] == 'm' {
		pathStr = pathStr[2:]
	}

	for _, segment := range splitPath(pathStr) {
		hardened := false
		if len(segment) > 0 && (segment[len(segment)-1] == '\'' || segment[len(segment)-1] == 'h') {
			hardened = true
			segment = segment[:len(segment)-1]
		}

		var value uint32
		if _, err := fmt.Sscanf(segment, "%d", &value); err != nil {
			return nil, fmt.Errorf("invalid path segment: %s", segment)
		}

		if hardened {
			value |= 0x80000000
		}
		path.Path = append(path.Path, value)
	}

	return path, nil
}

func splitPath(path string) []string {
	result := make([]string, 0)
	current := ""

	for _, c := range path {
		if c == '/' {
			if current != "" {
				result = append(result, current)
				current = ""
			}
		} else {
			current += string(c)
		}
	}

	if current != "" {
		result = append(result, current)
	}

	return result
}

func (p *HWWPath) String() string {
	result := "m"
	for _, v := range p.Path {
		result += "/"
		if v&0x80000000 != 0 {
			result += fmt.Sprintf("%d'", v&0x7FFFFFFF)
		} else {
			result += fmt.Sprintf("%d", v)
		}
	}
	return result
}

type HardwareWallet interface {
	Close() error
	GetType() WalletType
	IsConnected() bool

	GetFirmwareVersion() (string, error)
	GetSerialNumber() (string, error)
	GetPublicKey(path *HWWPath) ([]byte, error)
	GetAddress(path *HWWPath) (string, error)

	SignTransaction(path *HWWPath, txData []byte) ([]byte, error)
	SignMessage(path *HWWPath, message string) ([]byte, error)

	DisplayAddress(path *HWWPath, address string) error

	WipeDevice() error
	SetupDevice(label, passphrase string) error
	RestoreDevice(mnemonic, passphrase string) error
}

type HIDDevice interface {
	Close() error
	Write(data []byte) (int, error)
	Read(data []byte, timeoutMs uint32) (int, error)
}

type WalletManager struct {
	mu          sync.RWMutex
	wallets     map[string]HardwareWallet
	deviceInfos map[string]*WalletInfo
}

var (
	defaultManager *WalletManager
	managerOnce    sync.Once
)

func Manager() *WalletManager {
	managerOnce.Do(func() {
		defaultManager = &WalletManager{
			wallets:     make(map[string]HardwareWallet),
			deviceInfos: make(map[string]*WalletInfo),
		}
	})
	return defaultManager
}

func EnumerateDevices() ([]WalletInfo, error) {
	return enumerateHIDDevices()
}

func ConnectDevice(info WalletInfo) (HardwareWallet, error) {
	device, err := openHIDDevice(info.Path)
	if err != nil {
		return nil, fmt.Errorf("failed to open HID device: %w", err)
	}

	wallet := &HardwareWalletImpl{
		info:   info,
		device: device,
		mu:     sync.Mutex{},
	}

	if err := wallet.initialize(); err != nil {
		device.Close()
		return nil, fmt.Errorf("failed to initialize device: %w", err)
	}

	Manager().mu.Lock()
	Manager().wallets[info.Path] = wallet
	Manager().deviceInfos[info.Path] = &info
	Manager().mu.Unlock()

	return wallet, nil
}

func DisconnectDevice(wallet HardwareWallet) error {
	if wallet == nil {
		return errors.New("wallet is nil")
	}

	Manager().mu.Lock()
	defer Manager().mu.Unlock()

	path := ""
	for k, v := range Manager().wallets {
		if v == wallet {
			path = k
			break
		}
	}

	if path != "" {
		delete(Manager().wallets, path)
		delete(Manager().deviceInfos, path)
	}

	return wallet.Close()
}

type HardwareWalletImpl struct {
	info   WalletInfo
	device HIDDevice
	mu     sync.Mutex
}

func (w *HardwareWalletImpl) Close() error {
	if w.device != nil {
		return w.device.Close()
	}
	return nil
}

func (w *HardwareWalletImpl) GetType() WalletType {
	return w.info.Type
}

func (w *HardwareWalletImpl) IsConnected() bool {
	return w.device != nil
}

func (w *HardwareWalletImpl) initialize() error {
	return nil
}

func (w *HardwareWalletImpl) GetFirmwareVersion() (string, error) {
	w.mu.Lock()
	defer w.mu.Unlock()

	if w.device == nil {
		return "", errors.New("device not connected")
	}

	response, err := w.exchangeAPDU(0xE0, 0x01, 0x00, 0x00, nil)
	if err != nil {
		return "", err
	}

	if len(response) < 4 {
		return "", errors.New("invalid firmware response")
	}

	version := fmt.Sprintf("%d.%d.%d", response[1], response[2], response[3])
	return version, nil
}

func (w *HardwareWalletImpl) GetSerialNumber() (string, error) {
	w.mu.Lock()
	defer w.mu.Unlock()

	if w.device == nil {
		return "", errors.New("device not connected")
	}

	response, err := w.exchangeAPDU(0xE0, 0x02, 0x00, 0x00, nil)
	if err != nil {
		return "", err
	}

	if len(response) < 4 {
		return "", errors.New("invalid serial response")
	}

	serial := hex.EncodeToString(response[4:])
	return serial, nil
}

func (w *HardwareWalletImpl) GetPublicKey(path *HWWPath) ([]byte, error) {
	w.mu.Lock()
	defer w.mu.Unlock()

	if w.device == nil {
		return nil, errors.New("device not connected")
	}

	pathBytes := serializePath(path)

	response, err := w.exchangeAPDU(0xE0, 0x40, 0x00, 0x00, pathBytes)
	if err != nil {
		return nil, err
	}

	if len(response) < 65 {
		return nil, errors.New("invalid public key response")
	}

	return response[1:66], nil
}

func (w *HardwareWalletImpl) GetAddress(path *HWWPath) (string, error) {
	pubKey, err := w.GetPublicKey(path)
	if err != nil {
		return "", err
	}

	address := hashPublicKey(pubKey)
	return "0x" + hex.EncodeToString(address[:]), nil
}

func (w *HardwareWalletImpl) SignTransaction(path *HWWPath, txData []byte) ([]byte, error) {
	w.mu.Lock()
	defer w.mu.Unlock()

	if w.device == nil {
		return nil, errors.New("device not connected")
	}

	pathBytes := serializePath(path)

	data := append(pathBytes, txData...)

	chunkSize := 255
	for i := 0; i < len(data); i += chunkSize {
		end := i + chunkSize
		if end > len(data) {
			end = len(data)
		}

		chunk := data[i:end]
		more := uint8(0)
		if end < len(data) {
			more = 1
		}

		_, err := w.exchangeAPDU(0xE0, 0x42, more, 0x00, chunk)
		if err != nil {
			return nil, err
		}
	}

	response, err := w.exchangeAPDU(0xE0, 0x42, 0x00, 0x00, nil)
	if err != nil {
		return nil, err
	}

	if len(response) < 2 {
		return nil, errors.New("invalid signature response")
	}

	return response[1:], nil
}

func (w *HardwareWalletImpl) SignMessage(path *HWWPath, message string) ([]byte, error) {
	w.mu.Lock()
	defer w.mu.Unlock()

	if w.device == nil {
		return nil, errors.New("device not connected")
	}

	pathBytes := serializePath(path)

	messageBytes := []byte(message)
	data := append(pathBytes, messageBytes...)

	chunkSize := 255
	for i := 0; i < len(data); i += chunkSize {
		end := i + chunkSize
		if end > len(data) {
			end = len(data)
		}

		chunk := data[i:end]
		more := uint8(0)
		if end < len(data) {
			more = 1
		}

		_, err := w.exchangeAPDU(0xE0, 0x44, more, 0x00, chunk)
		if err != nil {
			return nil, err
		}
	}

	response, err := w.exchangeAPDU(0xE0, 0x44, 0x00, 0x00, nil)
	if err != nil {
		return nil, err
	}

	if len(response) < 2 {
		return nil, errors.New("invalid signature response")
	}

	return response[1:], nil
}

func (w *HardwareWalletImpl) DisplayAddress(path *HWWPath, address string) error {
	w.mu.Lock()
	defer w.mu.Unlock()

	if w.device == nil {
		return errors.New("device not connected")
	}

	if !w.info.HasScreen {
		return errors.New("device does not have a screen")
	}

	pathBytes := serializePath(path)
	addressBytes := []byte(address)
	data := append(pathBytes, addressBytes...)

	_, err := w.exchangeAPDU(0xE0, 0x50, 0x00, 0x00, data)
	return err
}

func (w *HardwareWalletImpl) WipeDevice() error {
	w.mu.Lock()
	defer w.mu.Unlock()

	if w.device == nil {
		return errors.New("device not connected")
	}

	_, err := w.exchangeAPDU(0xE0, 0xFF, 0x00, 0x00, nil)
	return err
}

func (w *HardwareWalletImpl) SetupDevice(label, passphrase string) error {
	w.mu.Lock()
	defer w.mu.Unlock()

	if w.device == nil {
		return errors.New("device not connected")
	}

	data := []byte(label)
	if passphrase != "" {
		data = append(data, []byte(passphrase)...)
	}

	_, err := w.exchangeAPDU(0xE0, 0x60, 0x00, 0x00, data)
	return err
}

func (w *HardwareWalletImpl) RestoreDevice(mnemonic, passphrase string) error {
	w.mu.Lock()
	defer w.mu.Unlock()

	if w.device == nil {
		return errors.New("device not connected")
	}

	data := []byte(mnemonic)
	if passphrase != "" {
		data = append(data, []byte(passphrase)...)
	}

	_, err := w.exchangeAPDU(0xE0, 0x61, 0x00, 0x00, data)
	return err
}

func (w *HardwareWalletImpl) exchangeAPDU(cla, ins, p1, p2 uint8, data []byte) ([]byte, error) {
	apdu := make([]byte, 5)
	apdu[0] = cla
	apdu[1] = ins
	apdu[2] = p1
	apdu[3] = p2

	if len(data) > 0 {
		apdu = append(apdu, byte(len(data)))
		apdu = append(apdu, data...)
	}

	apdu = append(apdu, 0)

	_, err := w.device.Write(apdu)
	if err != nil {
		return nil, fmt.Errorf("failed to write APDU: %w", err)
	}

	response := make([]byte, 64)
	n, err := w.device.Read(response, 30000)
	if err != nil {
		return nil, fmt.Errorf("failed to read APDU response: %w", err)
	}

	return response[:n], nil
}

func serializePath(path *HWWPath) []byte {
	result := make([]byte, 0)

	for _, v := range path.Path {
		result = append(result,
			byte((v>>24)&0xFF),
			byte((v>>16)&0xFF),
			byte((v>>8)&0xFF),
			byte(v&0xFF),
		)
	}

	return result
}

func hashPublicKey(pubKey []byte) []byte {
	hash := make([]byte, 20)

	for i := 0; i < 20 && i < len(pubKey); i++ {
		hash[i] = pubKey[len(pubKey)-1-i]
	}

	return hash
}
