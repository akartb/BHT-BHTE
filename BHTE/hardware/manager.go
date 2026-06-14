// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// Hardware Wallet Support Module

package hardware

import (
	"fmt"
)

func init() {
	Manager()
}

type DeviceCallback func(connected bool, message string)
type EnumerateCallback func(devices []WalletInfo)
type EventCallback func(deviceID string, connected bool)

type ManagerConfig struct {
	Enabled     bool
	AutoConnect bool
}

var managerConfig = &ManagerConfig{
	Enabled:     true,
	AutoConnect: true,
}

func SetEnabled(enabled bool) {
	managerConfig.Enabled = enabled
}

func IsEnabled() bool {
	return managerConfig.Enabled
}

func SetAutoConnect(autoConnect bool) {
	managerConfig.AutoConnect = autoConnect
}

func IsAutoConnect() bool {
	return managerConfig.AutoConnect
}

func GetDeviceInfo(path string) (*WalletInfo, bool) {
	Manager().mu.RLock()
	defer Manager().mu.RUnlock()

	info, exists := Manager().deviceInfos[path]
	return info, exists
}

func ListConnectedDevices() []HardwareWallet {
	Manager().mu.RLock()
	defer Manager().mu.RUnlock()

	devices := make([]HardwareWallet, 0, len(Manager().wallets))
	for _, wallet := range Manager().wallets {
		devices = append(devices, wallet)
	}
	return devices
}

func GetDeviceCount() int {
	Manager().mu.RLock()
	defer Manager().mu.RUnlock()
	return len(Manager().wallets)
}

func Shutdown() {
	Manager().mu.Lock()
	defer Manager().mu.Unlock()

	for _, wallet := range Manager().wallets {
		wallet.Close()
	}

	Manager().wallets = make(map[string]HardwareWallet)
	Manager().deviceInfos = make(map[string]*WalletInfo)
}

func GetDeviceTypeString(wt WalletType) string {
	switch wt {
	case WalletTypeLedger:
		return "Ledger"
	case WalletTypeTrezor:
		return "Trezor"
	case WalletTypeBitBox02:
		return "BitBox02"
	case WalletTypeKeystone:
		return "Keystone"
	case WalletTypeColdcard:
		return "Coldcard"
	case WalletTypeJade:
		return "Jade"
	default:
		return "Unknown"
	}
}

func GetBIP44Purpose() uint32 {
	return 44
}

func GetBHTCoinType() uint32 {
	return 0
}

func GetBHTECoinType() uint32 {
	return 1
}

func DeriveBIP44Path(coinType uint32, account uint32, change bool, index uint32) *HWWPath {
	path := &HWWPath{Path: make([]uint32, 0)}

	path.Path = append(path.Path, 0x8000002C)
	path.Path = append(path.Path, 0x80000000+account)

	if change {
		path.Path = append(path.Path, 0x80000001)
	} else {
		path.Path = append(path.Path, 0x80000000)
	}

	path.Path = append(path.Path, 0x80000000+index)

	_ = coinType
	return path
}

func ValidatePath(path *HWWPath) error {
	if path == nil || len(path.Path) == 0 {
		return fmt.Errorf("invalid path: path is nil or empty")
	}

	if len(path.Path) > 16 {
		return fmt.Errorf("invalid path: too many components (max 16)")
	}

	for i, v := range path.Path {
		if v&0x80000000 == 0 && i > 0 {
			return fmt.Errorf("invalid path: only first component can be unhardened")
		}
	}

	return nil
}

type DeviceFilter struct {
	WalletType     WalletType
	Manufacturer   string
	HasScreen      *bool
	HasKeypad      *bool
	FirmwarePrefix string
}

func FilterDevices(devices []WalletInfo, filter DeviceFilter) []WalletInfo {
	result := make([]WalletInfo, 0)

	for _, dev := range devices {
		if filter.WalletType != WalletTypeUnknown && dev.Type != filter.WalletType {
			continue
		}

		if filter.Manufacturer != "" && dev.Manufacturer != filter.Manufacturer {
			continue
		}

		if filter.HasScreen != nil && dev.HasScreen != *filter.HasScreen {
			continue
		}

		if filter.HasKeypad != nil && dev.HasKeypad != *filter.HasKeypad {
			continue
		}

		if filter.FirmwarePrefix != "" {
			found := false
			for i := 0; i < len(dev.FirmwareVersion) && i < len(filter.FirmwarePrefix); i++ {
				if dev.FirmwareVersion[i] == filter.FirmwarePrefix[i] {
					found = true
					break
				}
			}
			if !found {
				continue
			}
		}

		result = append(result, dev)
	}

	return result
}
