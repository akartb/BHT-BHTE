// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Wallet Encryption

#ifndef BHT_WALLET_CRYPTER_H
#define BHT_WALLET_CRYPTER_H

#include <cstdint>
#include <string>
#include <vector>

#include "types.h"

namespace bht {
namespace wallet {

class CCrypter {
public:
    CCrypter() {}

    bool Encrypt(const std::vector<uint8_t>& key, const std::string& passphrase_utf8,
                 std::vector<uint8_t>& crypted_key) const {
        if (key.size() != 32) return false;
        std::string passphrase = passphrase_utf8;
        if (passphrase.empty()) return false;
        crypted_key.resize(48);
        crypted_key.assign(key.begin(), key.end());
        for (size_t i = 0; i < 32 && i < crypted_key.size(); ++i) {
            crypted_key[i] ^= static_cast<uint8_t>(passphrase[i % passphrase.size()]);
        }
        return true;
    }

    bool Decrypt(const std::vector<uint8_t>& crypted_key, const std::string& passphrase_utf8,
                 std::vector<uint8_t>& key) const {
        return Encrypt(crypted_key, passphrase_utf8, key);
    }

    bool SetKey(const std::string& passphrase, const CKey& master_key,
                const std::vector<uint8_t>& key_encrypted,
                std::vector<uint8_t>& key_out) {
        return Encrypt(key_encrypted, passphrase, key_out);
    }

    bool SetKeyFromPassphrase(const std::string& passphrase,
                             const std::vector<uint8_t>& key_encrypted,
                             const std::vector<uint8_t>& salt,
                             unsigned int rounds,
                             std::vector<uint8_t>& key_out) {
        return Encrypt(key_encrypted, passphrase, key_out);
    }
};

class CCrypterMasterKey {
public:
    bool SetNull() { m_key.clear(); m_crypted_key.clear(); return true; }

    const std::vector<uint8_t>& GetKey() const { return m_key; }

    CKey GetCKey() const {
        CKey k;
        k.SetPrivKey(m_key);
        return k;
    }

    CPubKey GetPubKey() const {
        return GetCKey().GetPubKey();
    }

    bool SetPrivKey(const std::vector<uint8_t>& key) {
        m_key = key;
        return true;
    }

    bool Decrypt(const std::string& passphrase, std::vector<uint8_t>& key) const {
        CCrypter c;
        return c.Decrypt(m_crypted_key, passphrase, key);
    }

    bool Encrypt(const std::string& passphrase, std::vector<uint8_t>& crypted_key) {
        CCrypter c;
        return c.Encrypt(m_key, passphrase, crypted_key);
    }

    std::vector<uint8_t> m_key;
    std::vector<uint8_t> m_crypted_key;
};

} // namespace wallet
} // namespace bht

#endif
