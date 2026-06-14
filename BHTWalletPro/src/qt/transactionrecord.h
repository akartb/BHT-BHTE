// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Transaction Record

#ifndef BHT_WALLET_TRANSACTION_RECORD_H
#define BHT_WALLET_TRANSACTION_RECORD_H

#include <string>
#include <vector>
#include <map>
#include <QtCore/QtGlobal>
#include <cstring>

struct uint256 {
    unsigned char data[32];
    uint256() { memset(data, 0, sizeof(data)); }
};

typedef long long int64_t;
typedef int64_t CAmount;

struct TransactionRecord {
    enum Type {
        Other,
        Generated,
        SendToSelf,
        SendToAddress,
        SendToOther,
        RecvWithAddress,
        RecvFromOther,
        RecvFromSelf,
        BHTToAsset,
        AssetToBHT,
        Layer2Deposit,
        Layer2Withdrawal,
        HardwareTx
    };

    enum Status {
        Confirmations,
        Confirmed,
        Conflicted,
        Abandoned,
        OpenUntilDate,
        OpenUntilBlock,
        Unconfirmed,
        Confirming,
        Immature,
        MaturesIn,
        DidNotMature,
        OffChain,
        Online,
        HW
    };

    static QString toString(Type type);

    TransactionRecord()
        : idx(0)
        , time(0)
        , type(Other)
        , debit(0)
        , credit(0)
        , fee(0)
        , locktime(0)
        , status(Status::Confirmed)
        , depth(0)
        , cur_format_i(0)
    {
    }

    TransactionRecord(
        uint256 hash,
        int64_t time,
        Type type,
        const std::string& address,
        int64_t debit,
        int64_t credit,
        int64_t fee,
        int64_t locktime,
        Status status,
        int depth,
        int cur_format_i,
        const std::string& asset
    )
        : hash(hash)
        , idx(0)
        , time(time)
        , type(type)
        , address(address)
        , debit(debit)
        , credit(credit)
        , fee(fee)
        , locktime(locktime)
        , status(status)
        , depth(depth)
        , cur_format_i(cur_format_i)
        , asset(asset)
    {
    }

    static TransactionRecord* add(
        const uint256& hash,
        int64_t time,
        Type type,
        const std::string& address,
        int64_t debit,
        int64_t credit,
        int64_t fee,
        int64_t locktime,
        Status status,
        int depth,
        int cur_format_i,
        const std::string& asset
    );

    static QString getTransactionHTML(const QString& address, int cur_format);

    bool involvesWatchAddress() const { return false; }
    bool isAbandoned() const { return status == Status::Abandoned; }
    bool isWareHouseTx() const { return false; }

    void updateStatus(const uint256& newHash);

    static TransactionRecord* create(
        const uint256& hashBlock,
        int64_t time,
        const uint256& hash,
        int position,
        int64_t credit,
        int64_t debit,
        const std::string& address,
        const std::string& asset,
        int64_t fee,
        const std::string& narration
    );

    static QString formatSubtractFeeFromAmount(bool subtract_fee, const CAmount& amount);

    uint256 hash;
    uint256 generated_coinbase_hash;
    size_t idx;
    int64_t time;
    Type type;
    std::string address;
    int64_t debit;
    int64_t credit;
    int64_t fee;
    int64_t locktime;
    Status status;
    int depth;
    int cur_format_i;
    std::string asset;
    std::string txid;
    std::map<std::string, std::string> label;
    std::string narration;

    struct TransactionNotification {
        TransactionNotification()
            : added(0)
            , removed(false)
            , updated(false)
            , statusChanged(false)
            , url("")
        {
        }

        uint256 kd;
        bool added;
        bool removed;
        bool updated;
        bool statusChanged;
        std::string url;
    };

    static std::vector<TransactionNotification> queuedNotifications;
};

#endif
