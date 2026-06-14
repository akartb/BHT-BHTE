// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Core Wallet Header

#ifndef BHT_WALLET_WALLET_H
#define BHT_WALLET_WALLET_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <cstdint>
#include <optional>

#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "types.h"
#include "crypter.h"

namespace bht {
namespace wallet {

constexpr int64_t DEFAULT_TX_FEE = 1000;
constexpr int64_t DUST_THRESHOLD = 546;
constexpr uint32_t BIP32_HARDENED_KEY_LIMIT = 0x80000000;

enum class OutputType {
    Legacy,
    P2SH_SegWit,
    Bech32,
    Bech32m
};

enum class CoinType {
    BHC = 0,
    BHTE = 1
};

enum class SignatureType {
    ECDSA,
    Schnorr,
    ML_DSA
};

struct CMutableTransaction;
using bht::CKey;
using bht::CPubKey;
using bht::CScript;
class CWallet;

struct CRecipient {
    CScript scriptPubKey;
    int64_t nAmount;
    bool fSubtractFeeFromAmount;
};

using bht::CWalletTx;

struct CKeyPool {
    int64_t nCreationTime;
    uint256 m_pre_public_key;
    uint256 m_pre_key_third_party_signature;
    CPubKey vchPubKey;
    OutputType output_type;
};

struct CAccount {
    CPubKey vchPubKey;
    OutputType output_type;
    std::string label;
};

class CWallet {
public:
    static constexpr uint32_t BIP32_HARDENED_KEY_LIMIT = 0x80000000;

    CWallet();
    ~CWallet();

    static std::unique_ptr<CWallet> Create(const std::string& wallet_path);
    static std::unique_ptr<CWallet> Load(const std::string& wallet_path, const std::string& passphrase);

    bool CreateWallet(const std::string& passphrase);
    bool LoadWallet(const std::string& passphrase);
    bool EncryptWallet(const std::string& passphrase);
    bool Unlock(const std::string& passphrase);
    void Lock();

    bool IsLocked() const { return m_locked; }
    bool IsEncrypted() const { return m_encrypted; }

    CPubKey GenerateNewKey(OutputType type, bool internal = false);
    CPubKey GetPubKey(const CKeyID& address) const;
    bool GetKey(const CKeyID& address, CKey& key) const;

    CPubKey GetDefaultPubKey() const { return m_default_pubkey; }
    std::string GetDefaultAddress() const;

    bool AddKey(const CKey& key);
    bool AddKeyPubKey(const CKey& key, const CPubKey& pubkey);

    std::string CreateTransaction(
        const std::vector<CRecipient>& recipients,
        CTransactionRef& tx,
        uint256& wtxid,
        int64_t& fee,
        std::string& fail_reason,
        bool sign = true
    );

    bool SignTransaction(std::shared_ptr<CTransaction> tx, std::map<COutPoint, Coin>& coins);

    bool CommitTransaction(CTransactionRef tx, mapValue_t mapValue);

    CTransactionRef GetTransaction(const uint256& txid) const;
    CAmount GetBalance() const;
    CAmount GetSpendableBalance() const;
    CAmount GetUnconfirmedBalance() const;

    std::vector<COutput> AvailableCoins(bool fOnlySafe = true);

    bool SelectCoins(
        const CAmount& target,
        CAmount& value_ret,
        std::vector<COutput>& setCoinsRet,
        std::set<CInputCoin>& setCoinsSelectedRet
    );

    std::vector<std::string> GetAddressesByAccount(const std::string& account);

    bool CreateBip84Path(std::vector<uint32_t>& path, CoinType coin, bool internal = false);
    bool DeriveKeyPath(CKey& key, const std::vector<uint32_t>& path);

    void SetMinFee(int64_t fee) { m_min_fee = fee; }
    int64_t GetMinFee() const { return m_min_fee; }

    void SetDefaultSignatureType(SignatureType type) { m_default_sigtype = type; }
    SignatureType GetDefaultSignatureType() const { return m_default_sigtype; }

    bool IsMLDSAEnabled() const { return m_use_mldsa; }
    void SetMLDSAEnabled(bool enabled) { m_use_mldsa = enabled; }

    std::string GetMnemonic();
    bool RestoreFromMnemonic(const std::string& mnemonic);

    bool BackupWallet(const std::string& filename);
    bool ExportWallet(const std::string& filename);
    bool ImportWallet(const std::string& filename);

    void MarkDirty() { m_dirty = true; }
    bool IsDirty() const { return m_dirty; }

private:
    bool m_locked{true};
    bool m_encrypted{false};
    bool m_dirty{false};
    bool m_use_mldsa{true};

    int64_t m_min_fee{DEFAULT_TX_FEE};
    SignatureType m_default_sigtype{SignatureType::Schnorr};

    std::string m_wallet_path;
    std::vector<uint8_t> m_master_seed;
    CCrypterMasterKey m_master_key;
    CPubKey m_default_pubkey;

    std::map<CKeyID, CKey> m_keys;
    std::map<CPubKey, CKeyID> m_pubkey_to_keyid;
    std::map<CKeyID, CPubKey> m_pubkeys;
    std::set<CKeyID> m_hd_chains;

    std::map<uint256, CWalletTx> m_wallet_txs;
    std::map<COutPoint, CWalletTx> m_spent_outpoints;

    std::vector<CKeyPool> m_key_pools;

    bool AutoRescan();
    bool UnlockWithMasterKey();

    bool ComputeMnemonic(std::vector<std::string>& words);
    bool ComputeMasterKey(const std::vector<std::string>& words);

    void DeriveChildKey(
        const CKey& parent_key,
        const CPubKey& parent_pubkey,
        uint32_t child_index,
        CKey& child_key,
        CPubKey& child_pubkey
    );

    bool SetupHDChain(bool internal = false);

    static bool ComputeBip32Path(
        const std::string& purpose,
        CoinType coin,
        bool internal,
        std::vector<uint32_t>& path
    );

    friend class CWalletDB;
};

struct CKeyMetadata {
    int nVersion;
    int64_t nCreateTime;
    std::string hdKeypath;
    CKeyID key_id;
    bool has_key_origin;

    CKeyMetadata() : nVersion(1), nCreateTime(0), has_key_origin(false) {}

    static const int VERSION_BASIC = 1;
    static const int VERSION_WITH_HDDATA = 10;
    static const int CURRENT_VERSION = VERSION_WITH_HDDATA;
};

struct CMasterKey {
    std::vector<uint8_t> vchCryptedKey;
    std::vector<uint8_t> vchSalt;
    unsigned int nDerivationMethod;
    unsigned int nDeriveIterations;
    bool fUseChainCode;

    CMasterKey() : nDerivationMethod(0), nDeriveIterations(25000), fUseChainCode(false) {}
};

class CWalletDB {
public:
    CWalletDB(const std::string& db_path);
    ~CWalletDB();

    bool TxnBegin();
    bool TxnCommit();
    bool TxnAbort();

    bool WriteName(const std::string& address, const std::string& label);
    bool EraseName(const std::string& address);

    bool WriteTx(uint256 hash, const CWalletTx& tx);
    bool EraseTx(uint256 hash);

    bool ReadKey(const CKeyID& key_id, CKey& key);
    bool WriteKey(const CKeyID& key_id, const CKey& key);
    bool WriteKey(const CPubKey& pubkey, const CKey& key);
    bool WriteKeyMetadata(const CKeyMetadata& metadata, const CPubKey& pubkey);

    bool ReadCryptedKey(const CKeyID& key_id, std::vector<uint8_t>& key,
                       std::vector<uint8_t>& salt, unsigned int& rounds);
    bool WriteCryptedKey(const CKeyID& key_id,
                        const std::vector<uint8_t>& key,
                        const std::vector<uint8_t>& salt,
                        unsigned int rounds);
    bool WriteCryptedKey(
        const CPubKey& pubkey,
        const std::vector<uint8_t>& crypted_key,
        const CKeyMetadata& metadata
    );

    bool EraseKey(const CKeyID& key_id);

    bool WriteHDChain(const uint256& chain_code);
    bool ReadHDChain(uint256& chain_code);

    bool WriteMasterKey(uint32_t id, const CMasterKey& master_key);
    bool WriteMasterKey(const std::vector<uint8_t>& key,
                       const std::vector<uint8_t>& salt,
                       unsigned int rounds);
    bool ReadMasterKey(std::vector<uint8_t>& key,
                      std::vector<uint8_t>& salt,
                      unsigned int& rounds);

    bool WriteCScript(const uint160& hash, const CScript& redeem_script);

    bool WriteWatchOnly(const CScript& script);
    bool Close();

    std::string m_db_path;
};

}
}

#endif
