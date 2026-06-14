// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Hardware Wallet Support (HID)

#ifndef BHT_WALLET_HARDWARE_HARDWARE_WALLET_H
#define BHT_WALLET_HARDWARE_HARDWARE_WALLET_H

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>
#include <map>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <winusb.h>
#include <initguid.h>
#include <usbiodef.h>
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winusb.lib")
#endif

#include <openssl/ec.h>
#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <openssl/hmac.h>
#include <openssl/ecdsa.h>

#include "wallet/types.h"

namespace bht {
namespace hardware {

enum class HardwareWalletType {
    Unknown,
    Ledger,
    Trezor,
    BitBox02,
    Keystone,
    Coldcard,
    Jade
};

enum class HardwareError {
    None,
    NotConnected,
    DeviceNotFound,
    Timeout,
    InvalidData,
    Denied,
    Cancelled,
    Unknown
};

struct HardwareWalletInfo {
    HardwareWalletType type;
    std::string manufacturer;
    std::string product;
    std::string serial_number;
    std::string path;
    bool connected;
    bool has_screen;
    bool has_keypad;
    std::string firmware_version;
};

struct HWWPath {
    uint32_t address_n[16];
    size_t address_n_count;

    HWWPath() : address_n_count(0) {
        for (size_t i = 0; i < 16; ++i) address_n[i] = 0;
    }

    static HWWPath Parse(const std::string& path);
    std::string ToString() const;
};

class IHardwareWallet {
public:
    virtual ~IHardwareWallet() = default;

    virtual HardwareWalletType GetType() const = 0;
    virtual bool IsConnected() const = 0;
    virtual bool IsInitialized() const = 0;

    virtual bool Connect(const HardwareWalletInfo& device) = 0;
    virtual void Disconnect() = 0;

    virtual bool GetFirmwareVersion(std::string& version) = 0;
    virtual bool GetSerialNumber(std::string& serial) = 0;
    virtual bool GetPublicKey(const HWWPath& path, std::vector<uint8_t>& public_key) = 0;
    virtual bool GetAddress(const HWWPath& path, std::string& address) = 0;

    virtual bool SignTransaction(
        const HWWPath& path,
        const std::vector<uint8_t>& tx_data,
        std::vector<uint8_t>& signature
    ) = 0;

    virtual bool SignMessage(
        const HWWPath& path,
        const std::string& message,
        std::vector<uint8_t>& signature
    ) = 0;

    virtual bool DisplayAddress(const HWWPath& path, const std::string& address) = 0;

    virtual bool WipeDevice() = 0;
    virtual bool SetupDevice(const std::string& label, const std::string& passphrase) = 0;
    virtual bool RestoreDevice(const std::string& mnemonic, const std::string& passphrase) = 0;

    virtual std::string GetErrorString(HardwareError error) const = 0;
};

class CHardwareWallet : public IHardwareWallet {
public:
    CHardwareWallet();
    ~CHardwareWallet() override;

    static std::vector<HardwareWalletInfo> EnumerateDevices();

    HardwareWalletType GetType() const override { return m_type; }
    bool IsConnected() const override { return m_connected; }
    bool IsInitialized() const override { return m_initialized; }

    bool Connect(const HardwareWalletInfo& device) override;
    void Disconnect() override;

    bool GetFirmwareVersion(std::string& version) override;
    bool GetSerialNumber(std::string& serial) override;
    bool GetPublicKey(const HWWPath& path, std::vector<uint8_t>& public_key) override;
    bool GetAddress(const HWWPath& path, std::string& address) override;

    bool SignTransaction(
        const HWWPath& path,
        const std::vector<uint8_t>& tx_data,
        std::vector<uint8_t>& signature
    ) override;

    bool SignMessage(
        const HWWPath& path,
        const std::string& message,
        std::vector<uint8_t>& signature
    ) override;

    bool DisplayAddress(const HWWPath& path, const std::string& address) override;

    bool WipeDevice() override;
    bool SetupDevice(const std::string& label, const std::string& passphrase) override;
    bool RestoreDevice(const std::string& mnemonic, const std::string& passphrase) override;

    std::string GetErrorString(HardwareError error) const override;

    using DeviceCallback = std::function<void(bool connected, const std::string& message)>;
    void SetDeviceCallback(DeviceCallback callback) { m_device_callback = callback; }

private:
    std::vector<uint8_t> derivePrivateKey(const HWWPath& path);
    uint256 GetSHA256(const std::vector<uint8_t>& data);
    std::vector<uint8_t> computeHmacSHA512(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data);

    bool sendHIDReport(const std::vector<uint8_t>& data);
    bool receiveHIDReport(std::vector<uint8_t>& data, uint32_t timeout_ms = 5000);
    bool exchangeAPDU(
        uint8_t cla,
        uint8_t ins,
        uint8_t p1,
        uint8_t p2,
        const std::vector<uint8_t>& data,
        std::vector<uint8_t>& response
    );

    bool initialize();
    bool authenticate();
    bool getDeviceInfo();

    HardwareWalletType m_type{HardwareWalletType::Unknown};
    bool m_connected{false};
    bool m_initialized{false};
    bool m_has_screen{false};
    bool m_has_keypad{false};

    std::string m_firmware_version;
    std::string m_serial_number;
    std::string m_label;

#ifdef _WIN32
    HANDLE m_hid_device{INVALID_HANDLE_VALUE};
    PHIDP_PREPARSED_DATA m_preparsed_data{nullptr};
    HIDP_CAPS m_caps{};
#endif

    DeviceCallback m_device_callback;

    static const uint16_t LEDGER_VID;
    static const uint16_t TREZOR_VID;
};

class CHWWalletManager {
public:
    CHWWalletManager();
    ~CHWWalletManager();

    static CHWWalletManager& Instance();

    bool Initialize();
    void Shutdown();

    std::vector<HardwareWalletInfo> EnumerateDevices();
    std::unique_ptr<IHardwareWallet> ConnectDevice(const HardwareWalletInfo& device);
    void DisconnectDevice(IHardwareWallet* device);

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    void SetAutoConnect(bool auto_connect) { m_auto_connect = auto_connect; }
    bool IsAutoConnect() const { return m_auto_connect; }

    using EnumerateCallback = std::function<void(const std::vector<HardwareWalletInfo>&)>;
    void SetEnumerateCallback(EnumerateCallback callback) { m_enumerate_callback = callback; }

    using EventCallback = std::function<void(const std::string& device_id, bool connected)>;
    void SetEventCallback(EventCallback callback) { m_event_callback = callback; }

private:
    bool m_enabled{true};
    bool m_auto_connect{true};
    bool m_initialized{false};

    std::vector<std::unique_ptr<IHardwareWallet>> m_connected_devices;
    std::map<std::string, HardwareWalletInfo> m_device_registry;

    EnumerateCallback m_enumerate_callback;
    EventCallback m_event_callback;
};

}
}

#endif
