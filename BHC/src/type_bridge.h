// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// Type System Bridge - Bitcoin Core uint256 <-> BHC Crypto uint256
// This module provides converters between Bitcoin Core's uint256 and bhc::crypto::uint256

#ifndef BHC_TYPE_BRIDGE_H
#define BHC_TYPE_BRIDGE_H

#include <cstdint>
#include <array>
#include <vector>
#include <cstring>

#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace bhc {
namespace type_bridge {

using crypto_uint256 = std::array<uint8_t, 32>;

constexpr size_t UINT256_SIZE = 32;

inline crypto_uint256 ToBhcCrypto(const std::vector<uint8_t>& hash) {
    crypto_uint256 ret{};
    if (hash.size() >= UINT256_SIZE) {
        std::memcpy(ret.data(), hash.data(), UINT256_SIZE);
    }
    return ret;
}

inline std::vector<uint8_t> FromBhcCrypto(const crypto_uint256& hash) {
    return std::vector<uint8_t>(hash.begin(), hash.end());
}

inline void ToBhcCryptoInPlace(const uint8_t* src, uint8_t* dst) {
    std::memcpy(dst, src, UINT256_SIZE);
}

inline crypto_uint256 Uint256LEToBE(const crypto_uint256& le_hash) {
    crypto_uint256 be_hash;
    for (size_t i = 0; i < UINT256_SIZE; ++i) {
        be_hash[i] = le_hash[UINT256_SIZE - 1 - i];
    }
    return be_hash;
}

inline crypto_uint256 ReverseUint256(const crypto_uint256& hash) {
    return Uint256LEToBE(hash);
}

inline bool IsZero(const crypto_uint256& hash) {
    for (size_t i = 0; i < UINT256_SIZE; ++i) {
        if (hash[i] != 0) {
            return false;
        }
    }
    return true;
}

inline bool IsEqual(const crypto_uint256& a, const crypto_uint256& b) {
    for (size_t i = 0; i < UINT256_SIZE; ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

inline void SetZero(crypto_uint256& hash) {
    hash.fill(0);
}

inline void SetOne(crypto_uint256& hash) {
    hash.fill(0);
    hash[UINT256_SIZE - 1] = 1;
}

inline void Xor(crypto_uint256& result, const crypto_uint256& a, const crypto_uint256& b) {
    for (size_t i = 0; i < UINT256_SIZE; ++i) {
        result[i] = a[i] ^ b[i];
    }
}

inline void And(crypto_uint256& result, const crypto_uint256& a, const crypto_uint256& b) {
    for (size_t i = 0; i < UINT256_SIZE; ++i) {
        result[i] = a[i] & b[i];
    }
}

inline void Or(crypto_uint256& result, const crypto_uint256& a, const crypto_uint256& b) {
    for (size_t i = 0; i < UINT256_SIZE; ++i) {
        result[i] = a[i] | b[i];
    }
}

inline std::string ToHex(const crypto_uint256& hash) {
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(UINT256_SIZE * 2);
    for (size_t i = 0; i < UINT256_SIZE; ++i) {
        result += hex_chars[hash[i] >> 4];
        result += hex_chars[hash[i] & 0x0F];
    }
    return result;
}

inline std::string ToHexReversed(const crypto_uint256& hash) {
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(UINT256_SIZE * 2);
    for (size_t i = UINT256_SIZE; i-- > 0; ) {
        result += hex_chars[hash[i] >> 4];
        result += hex_chars[hash[i] & 0x0F];
    }
    return result;
}

inline bool FromHex(const std::string& hex, crypto_uint256& hash) {
    if (hex.size() != UINT256_SIZE * 2) {
        return false;
    }

    for (size_t i = 0; i < UINT256_SIZE; ++i) {
        char high = hex[i * 2];
        char low = hex[i * 2 + 1];

        uint8_t h = 0, l = 0;

        if (high >= '0' && high <= '9') h = high - '0';
        else if (high >= 'a' && high <= 'f') h = high - 'a' + 10;
        else if (high >= 'A' && high <= 'F') h = high - 'A' + 10;
        else return false;

        if (low >= '0' && low <= '9') l = low - '0';
        else if (low >= 'a' && low <= 'f') l = low - 'a' + 10;
        else if (low >= 'A' && low <= 'F') l = low - 'A' + 10;
        else return false;

        hash[i] = (h << 4) | l;
    }

    return true;
}

inline crypto_uint256 FromHexOrPanic(const std::string& hex) {
    crypto_uint256 result;
    if (!FromHex(hex, result)) {
        SetZero(result);
    }
    return result;
}

class CBTCAmount {
private:
    uint64_t m_satoshis;

public:
    explicit CBTCAmount(uint64_t satoshis = 0) : m_satoshis(satoshis) {}

    uint64_t GetSatoshis() const { return m_satoshis; }
    void SetSatoshis(uint64_t s) { m_satoshis = s; }

    double GetBTC() const { return static_cast<double>(m_satoshis) / 100000000.0; }

    static CBTCAmount FromBTC(double btc) {
        return CBTCAmount(static_cast<uint64_t>(btc * 100000000.0));
    }

    CBTCAmount& operator+=(const CBTCAmount& other) {
        m_satoshis += other.m_satoshis;
        return *this;
    }

    CBTCAmount& operator-=(const CBTCAmount& other) {
        if (m_satoshis < other.m_satoshis) {
            m_satoshis = 0;
        } else {
            m_satoshis -= other.m_satoshis;
        }
        return *this;
    }

    bool operator==(const CBTCAmount& other) const {
        return m_satoshis == other.m_satoshis;
    }

    bool operator<(const CBTCAmount& other) const {
        return m_satoshis < other.m_satoshis;
    }

    bool operator>(const CBTCAmount& other) const {
        return m_satoshis > other.m_satoshis;
    }
};

inline CBTCAmount operator+(const CBTCAmount& a, const CBTCAmount& b) {
    CBTCAmount result(a);
    result += b;
    return result;
}

inline CBTCAmount operator-(const CBTCAmount& a, const CBTCAmount& b) {
    CBTCAmount result(a);
    result -= b;
    return result;
}

}
}

#endif
