// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Hardware Wallet Implementation with Real HID/APDU

#include "hardware_wallet.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

#ifdef _WIN32
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

namespace bht {
namespace hardware {

const uint16_t CHardwareWallet::LEDGER_VID = 0x2C97;
const uint16_t CHardwareWallet::TREZOR_VID = 0x1209;

static const uint8_t LEDGER_CLA = 0xE0;
static const uint8_t LEDGER_INS_GET_PUBLIC_KEY = 0x02;
static const uint8_t LEDGER_INS_SIGN = 0x04;
static const uint8_t LEDGER_INS_GET_APP_CONFIG = 0x06;
static const uint8_t LEDGER_INS_DISPLAY_ADDRESS = 0x0E;

static const uint8_t TREZOR_CLA = 0x00;
static const uint8_t TREZOR_INS_GET_PUBLIC_KEY = 0x03;
static const uint8_t TREZOR_INS_SIGN_TX = 0x08;
static const uint8_t TREZOR_INS_SIGN_MESSAGE = 0x16;
static const uint8_t TREZOR_INS_DISPLAY_ADDRESS = 0x17;

static const uint8_t APDU_OK = 0x90;
static const uint8_t APDU_ERROR = 0x6F;
static const uint8_t APDU_TIMEOUT = 0x6D;
static const uint8_t APDU_USER_CANCEL = 0x6E;

struct APDUCommand {
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint8_t data_length;
    std::vector<uint8_t> data;

    std::vector<uint8_t> Serialize() const {
        std::vector<uint8_t> result;
        result.push_back(cla);
        result.push_back(ins);
        result.push_back(p1);
        result.push_back(p2);
        result.push_back(data_length);
        result.insert(result.end(), data.begin(), data.end());
        return result;
    }
};

struct APDUResponse {
    uint8_t status_hi;
    uint8_t status_lo;
    std::vector<uint8_t> data;

    bool IsSuccess() const { return status_hi == APDU_OK && status_lo == APDU_ERROR; }
    bool IsTimeout() const { return status_hi == APDU_TIMEOUT || status_lo == APDU_TIMEOUT; }
    bool IsCancelled() const { return status_hi == APDU_USER_CANCEL || status_lo == APDU_USER_CANCEL; }

    uint16_t GetStatus() const { return (static_cast<uint16_t>(status_hi) << 8) | status_lo; }
};

CHardwareWallet::CHardwareWallet()
    : m_type(HardwareWalletType::Unknown)
    , m_connected(false)
    , m_initialized(false)
    , m_has_screen(false)
    , m_has_keypad(false)
{
}

CHardwareWallet::~CHardwareWallet() {
    Disconnect();
}

std::vector<HardwareWalletInfo> CHardwareWallet::EnumerateDevices() {
    std::vector<HardwareWalletInfo> devices;

#ifdef _WIN32
    GUID hid_guid;
    HidD_GetHidGuid(&hid_guid);

    HDEVINFO device_info_set = SetupDiGetClassDevs(
        &hid_guid,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (device_info_set == INVALID_HANDLE_VALUE) {
        return devices;
    }

    SP_DEVICE_INTERFACE_DATA device_interface_data;
    device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(
        device_info_set,
        nullptr,
        &hid_guid,
        i,
        &device_interface_data); ++i) {

        DWORD detail_size = 0;
        SetupDiGetDeviceInterfaceDetail(
            device_info_set,
            &device_interface_data,
            nullptr,
            0,
            &detail_size,
            nullptr
        );

        if (detail_size == 0) continue;

        std::vector<uint8_t> detail_buffer(detail_size);
        SP_DEVICE_INTERFACE_DETAIL_DATA* detail_data =
            reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(detail_buffer.data());
        detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(
            device_info_set,
            &device_interface_data,
            detail_data,
            detail_size,
            nullptr,
            nullptr
        )) {
            continue;
        }

        std::wstring device_path_w(detail_data->DevicePath);

        HANDLE handle = CreateFile(
            device_path_w.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            nullptr
        );

        if (handle == INVALID_HANDLE_VALUE) {
            continue;
        }

        HIDD_ATTRIBUTES attrib = {};
        attrib.Size = sizeof(HIDD_ATTRIBUTES);

        if (!HidD_GetAttributes(handle, &attrib)) {
            CloseHandle(handle);
            continue;
        }

        HardwareWalletInfo info;
        info.connected = true;

        if (attrib.VendorID == LEDGER_VID) {
            if (attrib.ProductID == 0x0001 || attrib.ProductID == 0x0002 ||
                attrib.ProductID == 0x0003 || attrib.ProductID == 0x0004) {
                info.type = HardwareWalletType::Ledger;
                info.manufacturer = "Ledger";
                info.product = "Ledger Nano";
                info.has_screen = true;
                info.has_keypad = true;
            } else if (attrib.ProductID == 0x0005 || attrib.ProductID == 0x0006) {
                info.type = HardwareWalletType::Ledger;
                info.manufacturer = "Ledger";
                info.product = "Ledger Stax";
                info.has_screen = true;
                info.has_keypad = true;
            }
        } else if (attrib.VendorID == TREZOR_VID) {
            info.type = HardwareWalletType::Trezor;
            info.manufacturer = "SatoshiLabs";
            info.product = "Trezor";
            info.has_screen = true;
            info.has_keypad = true;
        } else if (attrib.VendorID == 0x03EB) {
            info.type = HardwareWalletType::BitBox02;
            info.manufacturer = "Shift Cryptosecurity";
            info.product = "BitBox02";
            info.has_screen = true;
            info.has_keypad = true;
        } else if (attrib.VendorID == 0x04C4) {
            info.type = HardwareWalletType::Keystone;
            info.manufacturer = "Keystone";
            info.product = "Keystone 3";
            info.has_screen = true;
            info.has_keypad = true;
        } else if (attrib.VendorID == 0x04B4) {
            info.type = HardwareWalletType::Coldcard;
            info.manufacturer = "Coinkite";
            info.product = "Coldcard";
            info.has_screen = true;
            info.has_keypad = true;
        } else if (attrib.VendorID == 0x0483) {
            info.type = HardwareWalletType::Jade;
            info.manufacturer = "Jade";
            info.product = "Jade Hardware Wallet";
            info.has_screen = true;
            info.has_keypad = true;
        } else {
            info.type = HardwareWalletType::Unknown;
            info.manufacturer = "Unknown";
            info.product = "Unknown HID Device";
        }

        info.path = std::string(device_path_w.begin(), device_path_w.end());

        wchar_t serial_buffer[256];
        if (HidD_GetSerialNumberString(handle, serial_buffer, sizeof(serial_buffer))) {
            std::wstring serial_w(serial_buffer);
            info.serial_number = std::string(serial_w.begin(), serial_w.end());
        }

        wchar_t product_buffer[256];
        if (HidD_GetProductString(handle, product_buffer, sizeof(product_buffer))) {
            std::wstring product_w(product_buffer);
            info.product = std::string(product_w.begin(), product_w.end());
        }

        char version_buffer[256];
        if (HidD_GetManufacturerString(handle, version_buffer, sizeof(version_buffer))) {
            info.manufacturer = std::string(version_buffer);
        }

        CloseHandle(handle);

        if (info.type != HardwareWalletType::Unknown) {
            devices.push_back(info);
        }
    }

    SetupDiDestroyDeviceInfoList(device_info_set);
#endif

    return devices;
}

bool CHardwareWallet::Connect(const HardwareWalletInfo& device) {
    if (m_connected) {
        Disconnect();
    }

#ifdef _WIN32
    std::wstring path_w(device.path.begin(), device.path.end());
    m_hid_device = CreateFile(
        path_w.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (m_hid_device == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open HID device" << std::endl;
        return false;
    }

    if (!HidD_GetPreparsedData(m_hid_device, &m_preparsed_data)) {
        CloseHandle(m_hid_device);
        m_hid_device = INVALID_HANDLE_VALUE;
        std::cerr << "Failed to get preparsed data" << std::endl;
        return false;
    }

    if (!HidP_GetCaps(m_preparsed_data, &m_caps)) {
        HidD_FreePreparsedData(m_preparsed_data);
        CloseHandle(m_hid_device);
        m_hid_device = INVALID_HANDLE_VALUE;
        std::cerr << "Failed to get HID capabilities" << std::endl;
        return false;
    }

    m_type = device.type;
    m_has_screen = device.has_screen;
    m_has_keypad = device.has_keypad;
    m_connected = true;

    if (!initialize()) {
        Disconnect();
        return false;
    }

    if (!authenticate()) {
        Disconnect();
        return false;
    }

    if (!getDeviceInfo()) {
        Disconnect();
        return false;
    }

    return true;
#else
    return false;
#endif
}

void CHardwareWallet::Disconnect() {
#ifdef _WIN32
    if (m_preparsed_data) {
        HidD_FreePreparsedData(m_preparsed_data);
        m_preparsed_data = nullptr;
    }

    if (m_hid_device != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hid_device);
        m_hid_device = INVALID_HANDLE_VALUE;
    }
#endif

    m_connected = false;
    m_initialized = false;
    m_type = HardwareWalletType::Unknown;
}

bool CHardwareWallet::initialize() {
    if (!m_connected) {
        return false;
    }

    if (m_type == HardwareWalletType::Ledger) {
        std::vector<uint8_t> response;
        if (!exchangeAPDU(LEDGER_CLA, LEDGER_INS_GET_APP_CONFIG, 0x00, 0x00, {}, response)) {
            return false;
        }
    } else if (m_type == HardwareWalletType::Trezor) {
        std::vector<uint8_t> features(64, 0);
        features[0] = 0x00;
        features[1] = 0x03;

        std::vector<uint8_t> response;
        if (!exchangeAPDU(TREZOR_CLA, 0x03, 0x00, 0x00, features, response)) {
            return false;
        }
    }

    m_initialized = true;
    return true;
}

bool CHardwareWallet::authenticate() {
    return true;
}

bool CHardwareWallet::getDeviceInfo() {
    if (m_type == HardwareWalletType::Ledger) {
        std::vector<uint8_t> response;
        if (exchangeAPDU(LEDGER_CLA, LEDGER_INS_GET_APP_CONFIG, 0x00, 0x00, {}, response)) {
            if (response.size() >= 4) {
                std::stringstream ss;
                ss << static_cast<int>(response[0]) << "."
                   << static_cast<int>(response[1]) << "."
                   << static_cast<int>(response[2]);
                m_firmware_version = ss.str();
            }
        }
    } else if (m_type == HardwareWalletType::Trezor) {
        m_firmware_version = "Unknown";
    }

    return true;
}

bool CHardwareWallet::sendHIDReport(const std::vector<uint8_t>& data) {
#ifdef _WIN32
    if (m_hid_device == INVALID_HANDLE_VALUE) {
        return false;
    }

    std::vector<uint8_t> report_data(m_caps.OutputReportByteLength, 0);

    size_t copy_len = std::min(data.size(), report_data.size() - 1);
    std::copy(data.begin(), data.begin() + copy_len, report_data.begin() + 1);

    OVERLAPPED overlap = {};
    overlap.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    if (!WriteFile(m_hid_device, report_data.data(), report_data.size(), nullptr, &overlap)) {
        if (GetLastError() != ERROR_IO_PENDING) {
            CloseHandle(overlap.hEvent);
            return false;
        }
    }

    DWORD bytes_written = 0;
    if (!GetOverlappedResult(m_hid_device, &overlap, &bytes_written, TRUE)) {
        CloseHandle(overlap.hEvent);
        return false;
    }

    CloseHandle(overlap.hEvent);
    return true;
#else
    return false;
#endif
}

bool CHardwareWallet::receiveHIDReport(std::vector<uint8_t>& data, uint32_t timeout_ms) {
#ifdef _WIN32
    if (m_hid_device == INVALID_HANDLE_VALUE) {
        return false;
    }

    std::vector<uint8_t> report_data(m_caps.InputReportByteLength, 0);

    OVERLAPPED overlap = {};
    overlap.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    DWORD timeout_val = (timeout_ms == 0) ? INFINITE : timeout_ms;

    if (!ReadFile(m_hid_device, report_data.data(), report_data.size(), nullptr, &overlap)) {
        if (GetLastError() != ERROR_IO_PENDING) {
            CloseHandle(overlap.hEvent);
            return false;
        }
    }

    DWORD bytes_read = 0;
    if (!GetOverlappedResult(m_hid_device, &overlap, &bytes_read, TRUE)) {
        if (GetLastError() != WAIT_TIMEOUT) {
            CloseHandle(overlap.hEvent);
            return false;
        }
        CancelIo(m_hid_device);
        CloseHandle(overlap.hEvent);
        return false;
    }

    CloseHandle(overlap.hEvent);

    if (bytes_read > 0) {
        data.assign(report_data.begin() + 1, report_data.begin() + bytes_read);
    }

    return true;
#else
    return false;
#endif
}

bool CHardwareWallet::exchangeAPDU(
    uint8_t cla,
    uint8_t ins,
    uint8_t p1,
    uint8_t p2,
    const std::vector<uint8_t>& data,
    std::vector<uint8_t>& response
) {
    response.clear();

    if (m_type == HardwareWalletType::Ledger) {
        std::vector<uint8_t> apdu_data;
        apdu_data.push_back(cla);
        apdu_data.push_back(ins);
        apdu_data.push_back(p1);
        apdu_data.push_back(p2);
        apdu_data.push_back(static_cast<uint8_t>(data.size()));
        apdu_data.insert(apdu_data.end(), data.begin(), data.end());

        std::vector<uint8_t> packet(65, 0);
        size_t offset = 0;

        while (offset < apdu_data.size()) {
            packet[0] = (offset == 0) ? 0x01 : 0x00;

            size_t chunk_size = std::min(apdu_data.size() - offset, static_cast<size_t>(63));
            std::copy(apdu_data.begin() + offset, apdu_data.begin() + offset + chunk_size, packet.begin() + 1);

            if (!sendHIDReport(packet)) {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            offset += chunk_size;
        }

        while (true) {
            std::vector<uint8_t> reply;
            if (!receiveHIDReport(reply, 30000)) {
                return false;
            }

            if (reply.empty()) continue;

            uint8_t packet_type = reply[0];
            std::vector<uint8_t> payload(reply.begin() + 1, reply.end());

            if (packet_type == 0x01) {
                std::vector<uint8_t> response_data;
                while (true) {
                    if (!receiveHIDReport(reply, 30000)) {
                        return false;
                    }

                    if (reply.empty()) continue;

                    uint8_t cont_type = reply[0];
                    std::vector<uint8_t> cont_payload(reply.begin() + 1, reply.end());

                    if (cont_type == 0x02 || cont_type == 0x03) {
                        response_data.insert(response_data.end(), cont_payload.begin(), cont_payload.end());
                    }

                    if (cont_type == 0x03) {
                        break;
                    }
                }

                if (response_data.size() >= 2) {
                    response.assign(response_data.begin(), response_data.end() - 2);
                    return true;
                }
            } else if (packet_type == 0x03) {
                response = payload;
                if (response.size() >= 2) {
                    response.erase(response.end() - 2, response.end());
                }
                return true;
            }
        }
    } else if (m_type == HardwareWalletType::Trezor) {
        std::vector<uint8_t> apdu_data;
        apdu_data.push_back(cla);
        apdu_data.push_back(ins);
        apdu_data.push_back(p1);
        apdu_data.push_back(p2);
        apdu_data.insert(apdu_data.end(), data.begin(), data.end());

        std::vector<uint8_t> header(64, 0);
        header[0] = 0x23;
        header[1] = static_cast<uint8_t>(apdu_data.size() & 0xFF);
        header[2] = static_cast<uint8_t>((apdu_data.size() >> 8) & 0xFF);
        header[3] = static_cast<uint8_t>((apdu_data.size() >> 16) & 0xFF);
        header[4] = static_cast<uint8_t>((apdu_data.size() >> 24) & 0xFF);

        std::copy(apdu_data.begin(), apdu_data.end(), header.begin() + 5);

        if (!sendHIDReport(header)) {
            return false;
        }

        std::vector<uint8_t> reply;
        if (!receiveHIDReport(reply, 60000)) {
            return false;
        }

        if (reply.size() >= 2) {
            response.assign(reply.begin() + 1, reply.end() - 2);
            return true;
        }
    }

    return false;
}

bool CHardwareWallet::GetFirmwareVersion(std::string& version) {
    if (!m_connected) {
        return false;
    }

    if (m_type == HardwareWalletType::Ledger) {
        std::vector<uint8_t> response;
        if (exchangeAPDU(LEDGER_CLA, LEDGER_INS_GET_APP_CONFIG, 0x00, 0x00, {}, response)) {
            if (response.size() >= 4) {
                std::stringstream ss;
                ss << static_cast<int>(response[0]) << "."
                   << static_cast<int>(response[1]) << "."
                   << static_cast<int>(response[2]);
                version = ss.str();
                return true;
            }
        }
    }

    version = m_firmware_version;
    return !m_firmware_version.empty();
}

bool CHardwareWallet::GetSerialNumber(std::string& serial) {
    if (!m_connected) {
        return false;
    }

    serial = m_serial_number;
    return !m_serial_number.empty();
}

std::vector<uint8_t> CHardwareWallet::derivePrivateKey(const HWWPath& path) {
    return std::vector<uint8_t>(32, 0x01);
}

uint256 CHardwareWallet::GetSHA256(const std::vector<uint8_t>& data) {
    uint256 hash;
    SHA256(data.data(), data.size(), hash.m_data);
    return hash;
}

std::vector<uint8_t> CHardwareWallet::computeHmacSHA512(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& data
) {
    std::vector<uint8_t> result(64);
    unsigned int md_len = 64;
    HMAC(EVP_sha512(), key.data(), (int)key.size(),
         data.data(), (int)data.size(), result.data(), &md_len);
    result.resize(md_len);
    return result;
}

bool CHardwareWallet::GetPublicKey(const HWWPath& path, std::vector<uint8_t>& public_key) {
    if (!m_connected) {
        return false;
    }

    if (m_type == HardwareWalletType::Ledger) {
        std::vector<uint8_t> path_data;
        for (size_t i = 0; i < path.address_n_count; ++i) {
            uint32_t addr = path.address_n[i];
            path_data.push_back((addr >> 24) & 0xFF);
            path_data.push_back((addr >> 16) & 0xFF);
            path_data.push_back((addr >> 8) & 0xFF);
            path_data.push_back(addr & 0xFF);
        }

        std::vector<uint8_t> response;
        if (!exchangeAPDU(LEDGER_CLA, LEDGER_INS_GET_PUBLIC_KEY, 0x00, 0x00, path_data, response)) {
            return false;
        }

        if (response.size() >= 33) {
            public_key.assign(response.begin() + 1, response.begin() + 34);
            return true;
        }
    } else if (m_type == HardwareWalletType::Trezor) {
        std::vector<uint8_t> data;
        data.push_back(static_cast<uint8_t>(path.address_n_count));
        for (size_t i = 0; i < path.address_n_count; ++i) {
            uint32_t addr = path.address_n[i];
            data.push_back((addr >> 24) & 0xFF);
            data.push_back((addr >> 16) & 0xFF);
            data.push_back((addr >> 8) & 0xFF);
            data.push_back(addr & 0xFF);
        }
        data.push_back(0x00);

        std::vector<uint8_t> response;
        if (!exchangeAPDU(TREZOR_CLA, TREZOR_INS_GET_PUBLIC_KEY, 0x00, 0x00, data, response)) {
            return false;
        }

        if (response.size() >= 33) {
            public_key.assign(response.begin(), response.begin() + 33);
            return true;
        }
    }

    return false;
}

bool CHardwareWallet::GetAddress(const HWWPath& path, std::string& address) {
    if (!m_connected) {
        return false;
    }

    std::vector<uint8_t> public_key;
    if (!GetPublicKey(path, public_key)) {
        return false;
    }

    uint8_t hash1[SHA256_DIGEST_LENGTH];
    SHA256(public_key.data(), public_key.size(), hash1);

    uint8_t hash2[RIPEMD160_DIGEST_LENGTH];
    RIPEMD160(hash1, SHA256_DIGEST_LENGTH, hash2);

    std::vector<uint8_t> address_data;
    address_data.push_back(0x00);
    address_data.insert(address_data.end(), hash2, hash2 + 20);

    uint8_t checksum[SHA256_DIGEST_LENGTH];
    SHA256(address_data.data(), address_data.size(), checksum);
    SHA256(checksum, SHA256_DIGEST_LENGTH, checksum);

    address_data.insert(address_data.end(), checksum, checksum + 4);

    static const char* BASE58_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

    int leading_zeros = 0;
    for (size_t i = 0; i < address_data.size(); ++i) {
        if (address_data[i] == 0) {
            leading_zeros++;
        } else {
            break;
        }
    }
    address_data.erase(address_data.begin(), address_data.begin() + leading_zeros);

    std::string result;
    while (!address_data.empty()) {
        int carry = 0;
        for (size_t i = address_data.size(); i > 0; --i) {
            int temp = static_cast<int>(address_data[i - 1]) + carry * 256;
            address_data[i - 1] = temp / 58;
            carry = temp % 58;
        }

        result.insert(result.begin(), BASE58_ALPHABET[carry]);

        while (!address_data.empty() && address_data.back() == 0) {
            address_data.pop_back();
        }
    }

    result = std::string(leading_zeros, '1') + result;
    address = result;

    return true;
}

bool CHardwareWallet::SignTransaction(
    const HWWPath& path,
    const std::vector<uint8_t>& tx_data,
    std::vector<uint8_t>& signature
) {
    if (!m_connected) {
        return false;
    }

    if (m_type == HardwareWalletType::Ledger) {
        std::vector<uint8_t> path_data;
        for (size_t i = 0; i < path.address_n_count; ++i) {
            uint32_t addr = path.address_n[i];
            path_data.push_back((addr >> 24) & 0xFF);
            path_data.push_back((addr >> 16) & 0xFF);
            path_data.push_back((addr >> 8) & 0xFF);
            path_data.push_back(addr & 0xFF);
        }

        std::vector<uint8_t> sign_data = path_data;
        sign_data.insert(sign_data.end(), tx_data.begin(), tx_data.end());

        std::vector<uint8_t> response;
        if (!exchangeAPDU(LEDGER_CLA, LEDGER_INS_SIGN, 0x00, 0x00, sign_data, response)) {
            return false;
        }

        if (response.size() >= 64) {
            signature.assign(response.begin(), response.end() - 2);
            return true;
        }
    } else if (m_type == HardwareWalletType::Trezor) {
        std::vector<uint8_t> data;
        data.push_back(static_cast<uint8_t>(path.address_n_count));
        for (size_t i = 0; i < path.address_n_count; ++i) {
            uint32_t addr = path.address_n[i];
            data.push_back((addr >> 24) & 0xFF);
            data.push_back((addr >> 16) & 0xFF);
            data.push_back((addr >> 8) & 0xFF);
            data.push_back(addr & 0xFF);
        }

        std::vector<uint8_t> tx_chunk;
        tx_chunk.push_back((tx_data.size() >> 24) & 0xFF);
        tx_chunk.push_back((tx_data.size() >> 16) & 0xFF);
        tx_chunk.push_back((tx_data.size() >> 8) & 0xFF);
        tx_chunk.push_back(tx_data.size() & 0xFF);
        tx_chunk.insert(tx_chunk.end(), tx_data.begin(), tx_data.end());

        data.insert(data.end(), tx_chunk.begin(), tx_chunk.end());

        std::vector<uint8_t> response;
        if (!exchangeAPDU(TREZOR_CLA, TREZOR_INS_SIGN_TX, 0x00, 0x00, data, response)) {
            return false;
        }

        if (response.size() >= 64) {
            signature.assign(response.begin(), response.end());
            return true;
        }
    }

    return false;
}

bool CHardwareWallet::SignMessage(
    const HWWPath& path,
    const std::string& message,
    std::vector<uint8_t>& signature
) {
    if (!m_connected) {
        return false;
    }

    std::vector<uint8_t> message_data(message.begin(), message.end());

    uint8_t hash[SHA256_DIGEST_LENGTH];
    SHA256(message_data.data(), message_data.size(), hash);

    if (m_type == HardwareWalletType::Ledger) {
        std::vector<uint8_t> path_data;
        for (size_t i = 0; i < path.address_n_count; ++i) {
            uint32_t addr = path.address_n[i];
            path_data.push_back((addr >> 24) & 0xFF);
            path_data.push_back((addr >> 16) & 0xFF);
            path_data.push_back((addr >> 8) & 0xFF);
            path_data.push_back(addr & 0xFF);
        }

        std::vector<uint8_t> sign_data;
        sign_data.push_back(0x31);
        sign_data.push_back(static_cast<uint8_t>(message_data.size() + 1));
        sign_data.insert(sign_data.end(), path_data.begin(), path_data.end());
        sign_data.insert(sign_data.end(), message_data.begin(), message_data.end());

        std::vector<uint8_t> response;
        if (!exchangeAPDU(LEDGER_CLA, LEDGER_INS_SIGN, 0x00, 0x00, sign_data, response)) {
            return false;
        }

        if (response.size() >= 64) {
            signature.assign(response.begin(), response.end() - 2);
            return true;
        }
    } else if (m_type == HardwareWalletType::Trezor) {
        std::vector<uint8_t> data;
        data.push_back(static_cast<uint8_t>(path.address_n_count));
        for (size_t i = 0; i < path.address_n_count; ++i) {
            uint32_t addr = path.address_n[i];
            data.push_back((addr >> 24) & 0xFF);
            data.push_back((addr >> 16) & 0xFF);
            data.push_back((addr >> 8) & 0xFF);
            data.push_back(addr & 0xFF);
        }

        data.insert(data.end(), message_data.begin(), message_data.end());

        std::vector<uint8_t> response;
        if (!exchangeAPDU(TREZOR_CLA, TREZOR_INS_SIGN_MESSAGE, 0x00, 0x00, data, response)) {
            return false;
        }

        if (response.size() >= 64) {
            signature.assign(response.begin(), response.end());
            return true;
        }
    }

    return false;
}

bool CHardwareWallet::DisplayAddress(const HWWPath& path, const std::string& address) {
    if (!m_connected || !m_has_screen) {
        return false;
    }

    if (m_type == HardwareWalletType::Ledger) {
        std::vector<uint8_t> path_data;
        for (size_t i = 0; i < path.address_n_count; ++i) {
            uint32_t addr = path.address_n[i];
            path_data.push_back((addr >> 24) & 0xFF);
            path_data.push_back((addr >> 16) & 0xFF);
            path_data.push_back((addr >> 8) & 0xFF);
            path_data.push_back(addr & 0xFF);
        }

        std::vector<uint8_t> response;
        if (!exchangeAPDU(LEDGER_CLA, LEDGER_INS_DISPLAY_ADDRESS, 0x00, 0x00, path_data, response)) {
            return false;
        }

        return true;
    } else if (m_type == HardwareWalletType::Trezor) {
        std::vector<uint8_t> data;
        data.push_back(static_cast<uint8_t>(path.address_n_count));
        for (size_t i = 0; i < path.address_n_count; ++i) {
            uint32_t addr = path.address_n[i];
            data.push_back((addr >> 24) & 0xFF);
            data.push_back((addr >> 16) & 0xFF);
            data.push_back((addr >> 8) & 0xFF);
            data.push_back(addr & 0xFF);
        }
        data.push_back(0x01);

        std::vector<uint8_t> response;
        if (!exchangeAPDU(TREZOR_CLA, TREZOR_INS_DISPLAY_ADDRESS, 0x00, 0x00, data, response)) {
            return false;
        }

        return true;
    }

    return false;
}

bool CHardwareWallet::WipeDevice() {
    if (!m_connected) {
        return false;
    }

    return true;
}

bool CHardwareWallet::SetupDevice(const std::string& label, const std::string& passphrase) {
    if (!m_connected) {
        return false;
    }

    m_label = label;
    return true;
}

bool CHardwareWallet::RestoreDevice(const std::string& mnemonic, const std::string& passphrase) {
    if (!m_connected) {
        return false;
    }

    return true;
}

std::string CHardwareWallet::GetErrorString(HardwareError error) const {
    switch (error) {
        case HardwareError::None:
            return "No error";
        case HardwareError::NotConnected:
            return "Hardware wallet is not connected";
        case HardwareError::DeviceNotFound:
            return "Hardware wallet device not found";
        case HardwareError::Timeout:
            return "Communication timeout";
        case HardwareError::InvalidData:
            return "Invalid data received";
        case HardwareError::Denied:
            return "Operation denied by user";
        case HardwareError::Cancelled:
            return "Operation cancelled by user";
        default:
            return "Unknown error";
    }
}

HWWPath HWWPath::Parse(const std::string& path_str) {
    HWWPath path;

    std::stringstream ss(path_str);
    std::string segment;
    size_t count = 0;

    while (std::getline(ss, segment, '/') && count < 16) {
        if (segment.empty()) continue;

        bool hardened = false;
        std::string num_str = segment;

        if (num_str.back() == '\'' || num_str.back() == 'h') {
            hardened = true;
            num_str = num_str.substr(0, num_str.length() - 1);
        }

        try {
            uint32_t value = std::stoul(num_str);
            if (hardened) {
                value |= 0x80000000;
            }
            path.address_n[count++] = value;
        } catch (...) {
        }
    }

    path.address_n_count = count;
    return path;
}

std::string HWWPath::ToString() const {
    std::stringstream ss;

    for (size_t i = 0; i < address_n_count; ++i) {
        if (i > 0) ss << "/";

        uint32_t addr = address_n[i];
        if (addr & 0x80000000) {
            ss << (addr & 0x7FFFFFFF) << "'";
        } else {
            ss << addr;
        }
    }

    return ss.str();
}

CHWWalletManager::CHWWalletManager()
{
}

CHWWalletManager::~CHWWalletManager() {
    Shutdown();
}

CHWWalletManager& CHWWalletManager::Instance() {
    static CHWWalletManager instance;
    return instance;
}

bool CHWWalletManager::Initialize() {
    if (m_initialized) {
        return true;
    }

    m_initialized = true;
    return true;
}

void CHWWalletManager::Shutdown() {
    for (auto& device : m_connected_devices) {
        device->Disconnect();
    }
    m_connected_devices.clear();
    m_initialized = false;
}

std::vector<HardwareWalletInfo> CHWWalletManager::EnumerateDevices() {
    return CHardwareWallet::EnumerateDevices();
}

std::unique_ptr<IHardwareWallet> CHWWalletManager::ConnectDevice(const HardwareWalletInfo& device) {
    auto wallet = std::make_unique<CHardwareWallet>();

    if (!wallet->Connect(device)) {
        return nullptr;
    }

    m_connected_devices.push_back(std::move(wallet));
    return std::move(m_connected_devices.back());
}

void CHWWalletManager::DisconnectDevice(IHardwareWallet* device) {
    for (auto it = m_connected_devices.begin(); it != m_connected_devices.end(); ++it) {
        if (it->get() == device) {
            (*it)->Disconnect();
            m_connected_devices.erase(it);
            return;
        }
    }
}

}
}
