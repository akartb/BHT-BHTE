// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// Cross-Platform HID Implementation for Go

package hardware

import (
	"errors"
	"fmt"
	"os"
	"runtime"
	"sync"
	"unsafe"
)

var (
	ErrDeviceNotFound = errors.New("HID device not found")
	ErrWriteFailed    = errors.New("failed to write to HID device")
	ErrReadFailed     = errors.New("failed to read from HID device")
)

type HIDDeviceImpl struct {
	handle    unsafe.Pointer
	isOpen    bool
	mu        sync.Mutex
	vendorID  uint16
	productID uint16
}

func openHIDDevice(path string) (HIDDevice, error) {
	impl := &HIDDeviceImpl{}

	switch runtime.GOOS {
	case "windows":
		return impl.openWindows(path)
	case "darwin":
		return impl.openMacOS(path)
	case "linux":
		return impl.openLinux(path)
	default:
		return nil, fmt.Errorf("unsupported operating system: %s", runtime.GOOS)
	}
}

func enumerateHIDDevices() ([]WalletInfo, error) {
	switch runtime.GOOS {
	case "windows":
		return enumerateWindows()
	case "darwin":
		return enumerateMacOS()
	case "linux":
		return enumerateLinux()
	default:
		return nil, fmt.Errorf("unsupported operating system: %s", runtime.GOOS)
	}
}

func (d *HIDDeviceImpl) Close() error {
	d.mu.Lock()
	defer d.mu.Unlock()

	if !d.isOpen {
		return nil
	}

	var err error
	switch runtime.GOOS {
	case "windows":
		err = d.closeWindows()
	case "darwin":
		err = d.closeMacOS()
	case "linux":
		err = d.closeLinux()
	}

	d.isOpen = false
	return err
}

func (d *HIDDeviceImpl) Write(data []byte) (int, error) {
	d.mu.Lock()
	defer d.mu.Unlock()

	if !d.isOpen {
		return 0, errors.New("device not open")
	}

	switch runtime.GOOS {
	case "windows":
		return d.writeWindows(data)
	case "darwin":
		return d.writeMacOS(data)
	case "linux":
		return d.writeLinux(data)
	default:
		return 0, errors.New("unsupported OS")
	}
}

func (d *HIDDeviceImpl) Read(data []byte, timeoutMs uint32) (int, error) {
	d.mu.Lock()
	defer d.mu.Unlock()

	if !d.isOpen {
		return 0, errors.New("device not open")
	}

	switch runtime.GOOS {
	case "windows":
		return d.readWindows(data, timeoutMs)
	case "darwin":
		return d.readMacOS(data, timeoutMs)
	case "linux":
		return d.readLinux(data, timeoutMs)
	default:
		return 0, errors.New("unsupported OS")
	}
}

func (d *HIDDeviceImpl) openWindows(path string) (*HIDDeviceImpl, error) {
	return d.openWindowsImpl(path)
}

func (d *HIDDeviceImpl) openMacOS(path string) (*HIDDeviceImpl, error) {
	return d.openMacOSImpl(path)
}

func (d *HIDDeviceImpl) openLinux(path string) (*HIDDeviceImpl, error) {
	return d.openLinuxImpl(path)
}

func (d *HIDDeviceImpl) closeWindows() error {
	return nil
}

func (d *HIDDeviceImpl) closeMacOS() error {
	return nil
}

func (d *HIDDeviceImpl) closeLinux() error {
	return nil
}

func (d *HIDDeviceImpl) writeWindows(data []byte) (int, error) {
	return 0, nil
}

func (d *HIDDeviceImpl) writeMacOS(data []byte) (int, error) {
	return 0, nil
}

func (d *HIDDeviceImpl) writeLinux(data []byte) (int, error) {
	return 0, nil
}

func (d *HIDDeviceImpl) readWindows(data []byte, timeoutMs uint32) (int, error) {
	return 0, nil
}

func (d *HIDDeviceImpl) readMacOS(data []byte, timeoutMs uint32) (int, error) {
	return 0, nil
}

func (d *HIDDeviceImpl) readLinux(data []byte, timeoutMs uint32) (int, error) {
	return 0, nil
}

func enumerateWindows() ([]WalletInfo, error) {
	return nil, nil
}

func enumerateMacOS() ([]WalletInfo, error) {
	return nil, nil
}

func enumerateLinux() ([]WalletInfo, error) {
	return nil, nil
}

func (d *HIDDeviceImpl) openWindowsImpl(path string) (*HIDDeviceImpl, error) {
	if runtime.GOOS != "windows" {
		return nil, errors.New("not windows")
	}

	file, err := os.OpenFile(path, os.O_RDWR, 0)
	if err != nil {
		return nil, fmt.Errorf("failed to open device: %w", err)
	}

	d.handle = unsafe.Pointer(file.Fd())
	d.isOpen = true

	return d, nil
}

func (d *HIDDeviceImpl) openMacOSImpl(path string) (*HIDDeviceImpl, error) {
	if runtime.GOOS != "darwin" {
		return nil, errors.New("not darwin")
	}

	file, err := os.OpenFile(path, os.O_RDWR, 0)
	if err != nil {
		return nil, fmt.Errorf("failed to open device: %w", err)
	}

	d.handle = unsafe.Pointer(file.Fd())
	d.isOpen = true

	return d, nil
}

func (d *HIDDeviceImpl) openLinuxImpl(path string) (*HIDDeviceImpl, error) {
	if runtime.GOOS != "linux" {
		return nil, errors.New("not linux")
	}

	file, err := os.OpenFile(path, os.O_RDWR, 0)
	if err != nil {
		return nil, fmt.Errorf("failed to open device: %w", err)
	}

	d.handle = unsafe.Pointer(file.Fd())
	d.isOpen = true

	return d, nil
}
