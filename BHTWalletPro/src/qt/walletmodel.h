// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Wallet Model

#ifndef WALLETMODEL_H
#define WALLETMODEL_H

#include <QtCore/QAbstractTableModel>
#include <QtCore/QTimer>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QDateTime>
#include <QtCore/QJsonValue>
#include <QtCore/QStringList>

#include <memory>
#include <vector>
#include <string>
#include <map>

#include "transactionrecord.h"

namespace bht {
namespace wallet {
class CWallet;
}
}

class OptionsModel;
class AddressTableModel;
class TransactionTableModel;
class CoinControlModel;
class RecentRequestsTableModel;
class WalletModel;
class ClientModel;
class QProcess;
struct SendCoinsRecipient;

class WalletModel : public QObject
{
    Q_OBJECT

public:
    explicit WalletModel(QObject* parent = nullptr);
    ~WalletModel();

    enum StatusCode {
        OK,
        INVALID_ADDRESS,
        DUPLICATE_ADDRESS,
        WALLET_UNLOCK_NEEDED,
        WALLET_KEYPOOL_NEEDED,
        ERROR
    };

    struct SendCoinsReturn {
        StatusCode status;
        QString msg;
        QString txid;
        std::map<std::string, std::string> detailed_status;
    };

    struct ChainState {
        QString chain;
        QString endpoint;
        bool connected{false};
        qint64 height{0};
        int peers{0};
        CAmount balance{0};
        QString bestBlock;
        QString lastError;
    };

    struct BridgeVerification {
        bool valid{false};
        QString chain;
        QString txid;
        qint64 confirmations{0};
        qint64 height{0};
        QString blockHash;
        QString stateRoot;
        QString message;
        QString proofJson;
    };

    struct NodeProcessState {
        QString chain;
        QString executable;
        QString dataDir;
        QStringList arguments;
        bool managed{false};
        bool running{false};
        qint64 pid{0};
        QString lastError;
    };

    struct TransactionBuildResult {
        bool ok{false};
        QString chain;
        QString rawHex;
        QString signedHex;
        QString txid;
        QString message;
        CAmount fee{0};
        bool complete{false};
    };

    void setClientModel(ClientModel* clientModel);
    ClientModel* getClientModel();

    void setOptionsModel(OptionsModel* optionsModel);
    OptionsModel* getOptionsModel() const { return m_options_model; }

    AddressTableModel* getAddressTableModel();
    TransactionTableModel* getTransactionTableModel();
    RecentRequestsTableModel* getRecentRequestsTableModel();
    CoinControlModel* getCoinControlModel();

    CAmount getBalance() const;
    CAmount getUnconfirmedBalance() const;
    CAmount getImmatureBalance() const;
    CAmount getWatchOnlyBalance() const;
    CAmount getWatchUnconfirmedBalance() const;
    CAmount getWatchImmatureBalance() const;

    bool haveWatchOnly() const;
    bool isWalletEnabled() const;

    QString getWalletName() const;
    QString getDisplayName() const;

    bool isMultiWalletEnabled() const;

    void setEncryptionStatus(int status);
    int getEncryptionStatus() const;

    bool validateAddress(const QString& address);

    SendCoinsReturn sendCoins(const QList<SendCoinsRecipient>& recipients, bool use_bhthd = false);
    SendCoinsReturn broadcastRawTransaction(const QString& chain, const QString& rawTransaction);

    void setRpcEndpoints(const QString& bhtEndpoint, const QString& bhteEndpoint);
    ChainState getBHTState() const;
    ChainState getBHTEState() const;
    QString createBHTDepositProof(const QString& txid, QString* error = nullptr);
    QString createBHTEAccountProof(const QString& address, QString* error = nullptr);
    BridgeVerification verifyBHTDepositProof(const QString& txid, const QString& proof, int minConfirmations = 12);
    BridgeVerification verifyBHTEAccountProof(const QString& address, const QString& proofJson);
    bool refreshChainStates();
    bool syncTransactionHistory(QString* error = nullptr);

    NodeProcessState getNodeProcessState(const QString& chain) const;
    bool startManagedNode(const QString& chain, QString* error = nullptr);
    bool stopManagedNode(const QString& chain, QString* error = nullptr);

    TransactionBuildResult buildBHTRawTransaction(const QString& toAddress, CAmount amount,
                                                 bool subtractFee = false, bool sign = true);
    TransactionBuildResult buildBHTERawTransaction(const QString& toAddress, CAmount amount,
                                                  bool sign = true);
    SendCoinsReturn buildSignAndBroadcast(const QString& chain, const QString& toAddress,
                                          CAmount amount, bool subtractFee = false);

    QString getWalletFile() const;
    QString getDataDir() const;

    bool hdEnabled() const;
    int getDefaultWalletType() const;
    bool getDefaultInternal() const;
    QString getPurpose(const QString& addr) const;

    bool getTransactionDetails(const uint256& txid, QString& fee, QString& status) const;

    std::map<QString, QString> getAddressBookMapping() const;

    std::map<QString, std::vector<uint256>> listTxid(uint64_t minOrderPos, int nCreateTime) const;

    QString getTransactionStatus(const uint256& txid) const;
    QString getTransactionAmount(const uint256& txid) const;

    bool hdKeypoolAffected() const;
    void markAffectedKeypoolDirty(bool affected);

    void updateStatus();
    void pollBalanceChanged();

Q_SIGNALS:
    void balanceChanged(const CAmount& balance, const CAmount& unconfirmedBalance,
                       const CAmount& immatureBalance, const CAmount& watchOnlyBalance,
                       const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance);

    void encryptionStatusChanged(int status);

    void requiredWalletUnlockFlag(bool explicitUnlocking = false);

    void message(const QString& title, const QString& message, unsigned int style);

    void coinsSent(WalletModel* wallet, SendCoinsRecipient recipient);

    void hwTransactionSigned(WalletModel* wallet);

    void transactionStatusChanged(const uint256& txid, const QString& status);

    void showProgress(const QString& title, int nProgress);
    void chainStateChanged(const WalletModel::ChainState& bht, const WalletModel::ChainState& bhte);

public Q_SLOTS:
    void updateTransaction();
    void updateAddressBook();
    void updateWatchOnlyBalance(bool haveWatchOnly);
    void pollTimerFires();

private:
    void checkBalanceChanged();
    bool rpcCall(const QString& endpoint, const QString& method, const QJsonValue& params,
                 QJsonValue& result, QString& error, const QString& user = QString(),
                 const QString& password = QString(), int timeoutMs = 8000) const;
    bool refreshBHTState();
    bool refreshBHTEState();
    bool refreshBHTTransactions(QString* error = nullptr);
    bool refreshBHTETransactions(QString* error = nullptr);
    QString getDefaultBHTEAccount(QString* error = nullptr, int timeoutMs = 8000) const;
    static CAmount amountFromJson(const QJsonValue& value);
    static QString amountToDecimalString(const CAmount& amount);
    static QString quantityToHex(const CAmount& amount);
    static uint256 uint256FromHex(const QString& value);
    static TransactionRecord::Status txStatusFromConfirmations(int confirmations);

    ClientModel* m_client_model{nullptr};
    OptionsModel* m_options_model{nullptr};

    WalletModel* m_next_model{nullptr};

    CAmount m_balances[6];
    bool m_watch_only{false};

    int m_encryption_status{0};

    QTimer* m_poll_timer{nullptr};
    QMutex m_wallet_mutex;
    mutable QMutex m_rpc_mutex;
    ChainState m_bht_state;
    ChainState m_bhte_state;
    QDateTime m_last_chain_poll;
    QProcess* m_bht_process{nullptr};
    QProcess* m_bhte_process{nullptr};
    std::vector<TransactionRecord> m_indexed_transactions;

};

class AddressTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit AddressTableModel(WalletModel* parent = nullptr);
    ~AddressTableModel();

    enum ColumnIndex {
        Label = 0,
        Address = 1,
        Type = 2,
        CreationDate = 3
    };

    enum RoleIndex {
        TypeRole = Qt::UserRole,
        AddressRole = Qt::UserRole + 1,
        LabelRole = Qt::UserRole + 2,
        IsMineRole = Qt::UserRole + 3,
        IsWatchOnlyRole = Qt::UserRole + 4,
        HasBalanceRole = Qt::UserRole + 5
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;

    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

    void updateEntry(const QString& address, const QString& label, bool isMine, bool isWatchOnly, int type);
    bool addAddress(const QString& address, const QString& label);
    bool updateAddress(const QString& address, const QString& label, bool isMine, bool isWatchOnly, int type);
    bool removeAddress(const QString& address);

    QString labelForAddress(const QString& address) const;
    QString addressForLabel(const QString& label) const;

    QList<QString> getAddressees() const;
    QList<QString> getLabels() const;

private:
    WalletModel* m_wallet_model{nullptr};
    QStringList m_addresses;
    QStringList m_labels;
    std::map<QString, int> m_address_to_row;
};

class TransactionTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit TransactionTableModel(WalletModel* parent = nullptr);
    ~TransactionTableModel();

    enum ColumnIndex {
        Status = 0,
        Watchonly = 1,
        Date = 2,
        Type = 3,
        Description = 4,
        AmountSymbol = 5,
        Amount = 6,
        Currency = 7
    };

    enum RoleIndex {
        TxIDRole = Qt::UserRole,
        TxHashRole = Qt::UserRole + 1,
        OutputIndexRole = Qt::UserRole + 2,
        AddressRole = Qt::UserRole + 3,
        LabelRole = Qt::UserRole + 4,
        AmountRole = Qt::UserRole + 5,
        StatusRole = Qt::UserRole + 6,
        WatchOnlyRole = Qt::UserRole + 7,
        DateRole = Qt::UserRole + 8,
        TypeRole = Qt::UserRole + 9,
        ConfirmationsRole = Qt::UserRole + 10,
        BlockRole = Qt::UserRole + 11,
        TxRole = Qt::UserRole + 12
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant data(int row, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;

    QString formatAsset(const QString& asset_symbol, const CAmount& amount) const;
    QString formatAssetAmount(const CAmount& amount) const;
    void setTransactions(const std::vector<TransactionRecord>& transactions);

private:
    WalletModel* m_wallet_model{nullptr};
    std::vector<TransactionRecord> m_transactions;
};

#endif
