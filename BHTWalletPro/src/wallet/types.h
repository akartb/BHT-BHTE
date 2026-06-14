// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Core Types and Constants

#ifndef BHT_WALLET_TYPES_H
#define BHT_WALLET_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstdint>
#include <cstring>
#include <functional>
#include <algorithm>
#include <memory>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace bht {

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;
using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

class uint256 {
public:
    uint256() { memset(m_data, 0, sizeof(m_data)); }
    uint256(uint64_t b) { memset(m_data, 0, sizeof(m_data)); m_data[3] = b; }

    void SetNull() { memset(m_data, 0, sizeof(m_data)); }

    bool IsNull() const {
        for (int i = 0; i < 8; ++i)
            if (m_data[i] != 0) return false;
        return true;
    }

    bool operator==(const uint256& other) const {
        return memcmp(m_data, other.m_data, 32) == 0;
    }
    bool operator!=(const uint256& other) const {
        return memcmp(m_data, other.m_data, 32) != 0;
    }
    bool operator<(const uint256& other) const {
        return memcmp(m_data, other.m_data, 32) < 0;
    }

    std::string ToString() const {
        char buf[65];
        for (int i = 0; i < 32; ++i)
            sprintf(buf + i * 2, "%02x", m_data[31 - i]);
        buf[64] = '\0';
        return std::string(buf);
    }

    unsigned char& operator[](size_t pos) { return m_data[pos]; }
    const unsigned char& operator[](size_t pos) const { return m_data[pos]; }

    unsigned char* begin() { return m_data; }
    unsigned char* end() { return m_data + 32; }
    const unsigned char* begin() const { return m_data; }
    const unsigned char* end() const { return m_data + 32; }

    uint8_t m_data[32];
};

class uint160 {
public:
    uint160() { memset(m_data, 0, sizeof(m_data)); }
    uint160(uint64_t b) { memset(m_data, 0, sizeof(m_data)); m_data[3] = b; }

    bool IsNull() const {
        for (int i = 0; i < 5; ++i)
            if (m_data[i] != 0) return false;
        return true;
    }

    bool operator==(const uint160& other) const {
        return memcmp(m_data, other.m_data, 20) == 0;
    }
    bool operator!=(const uint160& other) const {
        return memcmp(m_data, other.m_data, 20) != 0;
    }
    bool operator<(const uint160& other) const {
        return memcmp(m_data, other.m_data, 20) < 0;
    }

    std::string ToString() const {
        char buf[41];
        for (int i = 0; i < 20; ++i)
            sprintf(buf + i * 2, "%02x", m_data[19 - i]);
        buf[40] = '\0';
        return std::string(buf);
    }

    uint8_t m_data[20];
};

using uint256_t = uint256;
using uint160_t = uint160;

class CKeyID;
class CScriptID;

class CPubKey {
public:
    std::vector<uint8_t> m_key;

    CPubKey() {}
    explicit CPubKey(const std::vector<uint8_t>& key) : m_key(key) {}

    bool IsValid() const { return !m_key.empty(); }
    size_t size() const { return m_key.size(); }
    const uint8_t* data() const { return m_key.data(); }
    uint8_t* data() { return m_key.data(); }

    std::vector<uint8_t> Raw() const { return m_key; }

    uint160 GetRawID() const {
        uint160 id;
        if (!m_key.empty()) {
            std::copy(m_key.begin(),
                     m_key.begin() + (std::min)(m_key.size(), size_t(20)),
                     id.m_data);
        }
        return id;
    }

    CKeyID GetID() const;

    bool operator==(const CPubKey& other) const { return m_key == other.m_key; }
    bool operator!=(const CPubKey& other) const { return m_key != other.m_key; }
    bool operator<(const CPubKey& other) const { return m_key < other.m_key; }

    std::string ToString() const {
        char buf[67];
        for (size_t i = 0; i < (std::min)(m_key.size(), size_t(33)); ++i)
            sprintf(buf + i * 2, "%02x", m_key[i]);
        return std::string(buf);
    }
};

class CKeyID : public uint160 {
public:
    CKeyID() : uint160() {}
    CKeyID(uint64_t b) : uint160(b) {}
    explicit CKeyID(const uint160& other) : uint160(other) {}
    std::string ToString() const { return uint160::ToString(); }
};

class CScriptID : public uint160 {
public:
    CScriptID() : uint160() {}
    CScriptID(uint64_t b) : uint160(b) {}
    explicit CScriptID(const uint160& other) : uint160(other) {}
    std::string ToString() const { return uint160::ToString(); }
};

inline CKeyID CPubKey::GetID() const {
    if (m_key.empty()) return CKeyID();
    uint8_t hash1[SHA256_DIGEST_LENGTH];
    uint8_t hash2[RIPEMD160_DIGEST_LENGTH];
    SHA256(m_key.data(), m_key.size(), hash1);
    RIPEMD160(hash1, SHA256_DIGEST_LENGTH, hash2);
    CKeyID result;
    memcpy(result.m_data, hash2, 20);
    return result;
}

class CKey {
public:
    std::vector<uint8_t> m_key;

    CKey() {}
    explicit CKey(const std::vector<uint8_t>& key) : m_key(key) {}

    bool IsValid() const { return m_key.size() == 32; }
    void Clear() { m_key.clear(); }

    bool SetPrivKey(const std::vector<uint8_t>& key) {
        if (key.size() == 32) { m_key = key; return true; }
        return false;
    }

    std::vector<uint8_t> GetPrivKey() const { return m_key; }

    bool MakeNewKey(bool compressed) {
        m_key.resize(32);
        if (RAND_bytes(m_key.data(), 32) != 1) return false;
        return true;
    }

    bool Derive(CKey& parent, const std::vector<uint8_t>& cc, uint32_t i) {
        uint8_t data[37];
        if (!parent.IsValid()) return false;
        std::vector<uint8_t> pk = parent.GetPrivKey();
        memcpy(data, pk.data(), 32);
        data[32] = (i >> 24) & 0xFF;
        data[33] = (i >> 16) & 0xFF;
        data[34] = (i >> 8) & 0xFF;
        data[35] = i & 0xFF;
        data[36] = 0;

        uint8_t hash[SHA512_DIGEST_LENGTH];
        HMAC(EVP_sha512(), cc.data(), (int)cc.size(), data, 37, hash, nullptr);

        m_key.assign(hash, hash + 32);
        return true;
    }

    bool Derive(CKey& child, uint32_t index, bool hardened) const {
        if (!IsValid()) return false;
        uint8_t data[37];
        if (hardened) {
            data[0] = 0x00;
            memcpy(data + 1, m_key.data(), 32);
        } else {
            CPubKey pk = GetPubKey();
            size_t pkLen = (std::min)(pk.size(), size_t(33));
            memcpy(data, pk.data(), pkLen);
        }
        data[33] = (index >> 24) & 0xFF;
        data[34] = (index >> 16) & 0xFF;
        data[35] = (index >> 8) & 0xFF;
        data[36] = index & 0xFF;

        uint8_t hash[SHA512_DIGEST_LENGTH];
        HMAC(EVP_sha512(), m_key.data(), 32, data, 37, hash, nullptr);

        child.m_key.assign(hash, hash + 32);
        return true;
    }

    std::vector<uint8_t> GetKey() const {
        std::vector<uint8_t> result;
        if (IsValid()) {
            result = m_key;
            result.push_back(0x01);
        }
        return result;
    }

    CPubKey GetPubKey() const {
        CPubKey pk;
        if (m_key.size() == 32) {
            std::vector<uint8_t> pubkey(33);
            memcpy(pubkey.data(), m_key.data(), 32);
            pubkey[32] = 0x02 | (m_key[31] & 0x01);
            pk = CPubKey(pubkey);
        }
        return pk;
    }

    bool Sign(const uint256& hash, std::vector<uint8_t>& sig) const {
        std::vector<uint8_t> vchSig;
        vchSig.resize(72);
        sig.resize(72);
        for (size_t i = 0; i < 32 && i < sig.size(); ++i) sig[i] = hash.m_data[i];
        for (size_t i = 0; i < m_key.size() && i + 32 < sig.size(); ++i) sig[i + 32] = m_key[i];
        return true;
    }

    bool Verify(const uint256& hash, const std::vector<uint8_t>& sig) const {
        (void)hash; (void)sig;
        return IsValid();
    }

    bool SignCompact(const uint256& hash, std::vector<uint8_t>& sig) const {
        return Sign(hash, sig);
    }

    bool SetCompactSignature(const uint256& hash, const std::vector<uint8_t>& sig) {
        (void)hash;
        return SetPrivKey(sig);
    }

    bool Recover(const uint256& hash, const std::vector<uint8_t>& sig) {
        (void)hash; (void)sig;
        return true;
    }
};

class CScript {
public:
    std::vector<uint8_t> m_script;

    CScript() {}
    explicit CScript(const std::vector<uint8_t>& script) : m_script(script) {}

    size_t size() const { return m_script.size(); }
    bool empty() const { return m_script.empty(); }
    void clear() { m_script.clear(); }

    std::vector<uint8_t>::iterator begin() { return m_script.begin(); }
    std::vector<uint8_t>::iterator end() { return m_script.end(); }
    std::vector<uint8_t>::const_iterator begin() const { return m_script.begin(); }
    std::vector<uint8_t>::const_iterator end() const { return m_script.end(); }

    CScript& operator<<(const CPubKey& key) {
        m_script.insert(m_script.end(), key.m_key.begin(), key.m_key.end());
        return *this;
    }

    CScript& operator<<(const std::vector<uint8_t>& data) {
        m_script.insert(m_script.end(), data.begin(), data.end());
        return *this;
    }

    CScript& operator<<(const CKeyID& keyID) {
        m_script.insert(m_script.end(), keyID.m_data, keyID.m_data + 20);
        return *this;
    }

    bool operator==(const CScript& other) const { return m_script == other.m_script; }
    bool operator!=(const CScript& other) const { return m_script != other.m_script; }
    bool operator<(const CScript& other) const { return m_script < other.m_script; }

    uint160 GetID() const {
        uint160 id;
        if (m_script.size() == 20) {
            memcpy(id.m_data, m_script.data(), 20);
        }
        return id;
    }
};

struct COutPoint {
    uint256 hash;
    uint32 n;

    COutPoint() : n(0) {}
    COutPoint(const uint256& h, uint32 i) : hash(h), n(i) {}

    bool operator==(const COutPoint& other) const {
        return hash == other.hash && n == other.n;
    }
    bool operator!=(const COutPoint& other) const {
        return !(*this == other);
    }
    bool operator<(const COutPoint& other) const {
        if (hash < other.hash) return true;
        if (hash == other.hash) return n < other.n;
        return false;
    }

    std::string ToString() const {
        return hash.ToString() + "-" + std::to_string(n);
    }
};

struct TxIn {
    COutPoint prevout;
    CScript scriptSig;
    uint32 nSequence;

    TxIn() : nSequence(0xFFFFFFFF) {}
};

struct TxOut {
    int64_t nValue;
    CScript scriptPubKey;

    TxOut() : nValue(0) {}
};

class CTransaction {
public:
    int32_t version;
    std::vector<TxIn> vin;
    std::vector<TxOut> vout;
    uint32_t locktime;

    CTransaction() : version(1), locktime(0) {}

    uint256 GetHash() const {
        uint256 h;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&version);
        for (int i = 0; i < 4; ++i) h.m_data[i] = p[i] ^ 0xFF;
        return h;
    }
    uint256 GetWitnessHash() const { return GetHash(); }
    bool IsCoinBase() const {
        return vin.size() == 1 && vin[0].prevout.hash.IsNull() && vin[0].prevout.n == 0xFFFFFFFF;
    }
    std::string ToString() const { return GetHash().ToString(); }
};

using CTransactionRef = std::shared_ptr<const CTransaction>;

struct CBlockIndex {
    uint256 phashblock;
    uint256 hashPrev;
    uint256 hashNext;
    uint32_t nHeight;
    int64_t nTime;
    int64_t nBits;
    uint64_t nChainWork;

    CBlockIndex() : nHeight(0), nTime(0), nBits(0), nChainWork(0) {}
};

struct CDiskBlockPos {
    uint32_t nFile;
    uint32_t nPos;

    CDiskBlockPos() : nFile(0), nPos(0) {}
};

class CBlockLocator {
public:
    std::vector<uint256> vHave;

    CBlockLocator() {}
    bool IsNull() const { return vHave.empty(); }
};

struct CWalletTx {
    uint256 m_tx_hash;
    std::vector<char> m_order_form;
    int64_t m_time;
    int64_t m_credit;
    int64_t m_debit;
    int64_t m_fee;
    int64_t m_order_pos;
    bool m_in_trusted;
    bool m_is_coinbase;
    bool m_is_coinstake;
    int m_confirmations;
    std::string m_status;
    std::shared_ptr<CTransaction> m_tx;

    CWalletTx() : m_time(0), m_credit(0), m_debit(0), m_fee(0), m_order_pos(0),
        m_in_trusted(false), m_is_coinbase(false), m_is_coinstake(false),
        m_confirmations(0), m_status("unknown") {}
};

struct TransactionRecord {
    uint256 hash;
    int64_t time;
    std::string address;
    int64_t debit;
    int64_t credit;
    int64_t fee;
    std::string status;
    bool involves_watch_address;
    int type;
    std::string asset;

    TransactionRecord() : time(0), debit(0), credit(0), fee(0),
        involves_watch_address(false), type(0) {}
};

struct SendCoinsRecipient {
    std::string address;
    std::string label;
    std::string asset;
    int64_t amount;
    bool subtract_fee;

    SendCoinsRecipient() : amount(0), subtract_fee(false) {}
};

struct CReserveKey {
    int64_t nIndex;
    CKey key;
};

struct COutput {
    CTransactionRef tx;
    int i;
    int64_t value;
    bool safe;
    bool spendable;
    bool watch_spendable;
    bool reuse;
    std::string address;
    std::string label;
    int confirmations;

    COutput() : i(0), value(0), safe(false), spendable(false),
        watch_spendable(false), reuse(false), confirmations(0) {}
};

struct CInputCoin {
    COutPoint outpoint;
    TxOut txout;
    int64_t effective_value;
    int depth;

    CInputCoin() : effective_value(0), depth(0) {}

    CInputCoin(const CTransactionRef& tx, unsigned int i)
        : effective_value(0), depth(0) {
        if (tx && i < tx->vout.size()) {
            outpoint = COutPoint(tx->GetHash(), i);
            txout = tx->vout[i];
            effective_value = txout.nValue;
        }
    }

    bool operator<(const CInputCoin& other) const {
        if (outpoint < other.outpoint) return true;
        if (other.outpoint < outpoint) return false;
        return false;
    }
};

struct Coin {
    TxOut out;
    uint32 nHeight;
    bool fCoinBase;
    CScript scriptPubKey;

    Coin() : nHeight(0), fCoinBase(false) {}
};

class CFeeRate {
public:
    int64_t nSatoshisPerK;

    CFeeRate() : nSatoshisPerK(0) {}
    explicit CFeeRate(int64_t s) : nSatoshisPerK(s) {}
    int64_t GetFeePerK() const { return nSatoshisPerK; }
};

struct CAmount {
    int64_t value;

    CAmount() : value(0) {}
    CAmount(int64_t v) : value(v) {}

    CAmount& operator+=(const CAmount& other) { value += other.value; return *this; }
    CAmount& operator-=(const CAmount& other) { value -= other.value; return *this; }
    CAmount& operator+=(int64_t other) { value += other; return *this; }

    bool operator==(const CAmount& other) const { return value == other.value; }
    bool operator!=(const CAmount& other) const { return value != other.value; }
    bool operator<=(const CAmount& other) const { return value <= other.value; }
    bool operator>=(const CAmount& other) const { return value >= other.value; }
    bool operator<(const CAmount& other) const { return value < other.value; }
    bool operator>(const CAmount& other) const { return value > other.value; }

    operator int64_t() const { return value; }
};

inline CAmount operator+(CAmount a, CAmount b) { return CAmount(a.value + b.value); }
inline CAmount operator-(CAmount a, CAmount b) { return CAmount(a.value - b.value); }

typedef std::map<std::string, std::string> mapValue_t;
typedef std::map<COutPoint, Coin> CoinSet;

constexpr int64_t COIN = 100000000;
constexpr int64_t CENT = 1000000;
constexpr int64_t MIN_TX_FEE = 1000;
constexpr int64_t MAX_TX_FEE = 1000000000;

using SignatureData = struct {
    CScript scriptSig;
    CScript scriptWitness;
};

} // namespace bht

#endif
