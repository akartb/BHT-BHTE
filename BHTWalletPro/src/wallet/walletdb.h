// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Wallet Database

#ifndef BHT_WALLET_DB_H
#define BHT_WALLET_DB_H

#include "wallet.h"

namespace bht {
namespace wallet {

inline CWalletDB::CWalletDB(const std::string& db_path) : m_db_path(db_path) {}

inline CWalletDB::~CWalletDB() {}

inline bool CWalletDB::TxnBegin() { return true; }
inline bool CWalletDB::TxnCommit() { return true; }
inline bool CWalletDB::TxnAbort() { return true; }

inline bool CWalletDB::WriteName(const std::string& address, const std::string& label) {
    (void)address; (void)label; return false;
}
inline bool CWalletDB::EraseName(const std::string& address) { (void)address; return false; }

inline bool CWalletDB::WriteTx(uint256 hash, const CWalletTx& tx) {
    (void)hash; (void)tx; return false;
}
inline bool CWalletDB::EraseTx(uint256 hash) { (void)hash; return false; }

inline bool CWalletDB::ReadKey(const CKeyID& key_id, CKey& key) {
    (void)key_id; (void)key; return false;
}
inline bool CWalletDB::WriteKey(const CKeyID& key_id, const CKey& key) {
    (void)key_id; (void)key; return false;
}
inline bool CWalletDB::WriteKey(const CPubKey& pubkey, const CKey& key) {
    (void)pubkey; (void)key; return false;
}
inline bool CWalletDB::WriteKeyMetadata(const CKeyMetadata& metadata, const CPubKey& pubkey) {
    (void)metadata; (void)pubkey; return false;
}

inline bool CWalletDB::ReadCryptedKey(const CKeyID& key_id, std::vector<uint8_t>& key,
                                       std::vector<uint8_t>& salt, unsigned int& rounds) {
    (void)key_id; (void)key; (void)salt; (void)rounds; return false;
}
inline bool CWalletDB::WriteCryptedKey(const CKeyID& key_id,
                                        const std::vector<uint8_t>& key,
                                        const std::vector<uint8_t>& salt,
                                        unsigned int rounds) {
    (void)key_id; (void)key; (void)salt; (void)rounds; return false;
}
inline bool CWalletDB::WriteCryptedKey(const CPubKey& pubkey,
                                        const std::vector<uint8_t>& crypted_key,
                                        const CKeyMetadata& metadata) {
    (void)pubkey; (void)crypted_key; (void)metadata; return false;
}

inline bool CWalletDB::EraseKey(const CKeyID& key_id) { (void)key_id; return false; }

inline bool CWalletDB::WriteHDChain(const uint256& chain_code) { (void)chain_code; return false; }
inline bool CWalletDB::ReadHDChain(uint256& chain_code) { (void)chain_code; return false; }

inline bool CWalletDB::WriteMasterKey(uint32_t id, const CMasterKey& master_key) {
    (void)id; (void)master_key; return false;
}
inline bool CWalletDB::WriteMasterKey(const std::vector<uint8_t>& key,
                                       const std::vector<uint8_t>& salt,
                                       unsigned int rounds) {
    (void)key; (void)salt; (void)rounds; return false;
}
inline bool CWalletDB::ReadMasterKey(std::vector<uint8_t>& key,
                                      std::vector<uint8_t>& salt,
                                      unsigned int& rounds) {
    (void)key; (void)salt; (void)rounds; return false;
}

inline bool CWalletDB::WriteCScript(const uint160& hash, const CScript& redeem_script) {
    (void)hash; (void)redeem_script; return false;
}
inline bool CWalletDB::WriteWatchOnly(const CScript& script) { (void)script; return false; }
inline bool CWalletDB::Close() { return true; }

} // namespace wallet
} // namespace bht

#endif
