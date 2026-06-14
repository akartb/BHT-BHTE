// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Client Model

#ifndef BHT_WALLET_QT_CLIENTMODEL_H
#define BHT_WALLET_QT_CLIENTMODEL_H

#include <QtCore/QObject>
#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

#include "transactionrecord.h"

class OptionsModel;

class ClientModel : public QObject
{
    Q_OBJECT

public:
    explicit ClientModel(OptionsModel* optionsModel, QObject* parent = nullptr);
    ~ClientModel();

    OptionsModel* getOptionsModel();

    int getNumConnections() const;
    int getNumBlocks() const;
    uint256 getBestBlockHash();
    QDateTime getLastBlockDate() const;
    double getVerificationProgress() const;

    bool isNetworkActive() const;
    void setNetworkActive(bool active);

    QString getStatusBarWarnings() const;

    bool getProxyInfo(QString& ipPort) const;
    bool getOnionProxyInfo(QString& ipPort) const;

    QString formatFullVersion() const;
    QString formatSubVersion() const;
    bool isReleaseVersion() const;
    QString clientName() const;
    QString formatClientStartupTime() const;

    bool getHeaderTipHeight(int& height, int64_t& blockTime);
    int getHeaderTipTime();

    enum BlockSource {
        BLOCK_SOURCE_NONE,
        BLOCK_SOURCE_REINDEX,
        BLOCK_SOURCE_DISK,
        BLOCK_SOURCE_NETWORK
    };

    BlockSource getBlockSource() const;

    QString getNetworkName() const;
    bool isTestNetwork() const;

    void updateNumConnections(int numConnections);
    void updateNetworkActive(bool networkActive);

Q_SIGNALS:
    void numConnectionsChanged(int count);
    void numBlocksChanged(int count, const QDateTime& blockDate, double nVerificationProgress, bool header);
    void mempoolSizeChanged(long count, size_t mempoolSizeInBytes);
    void networkActiveChanged(bool networkActive);
    void alertsChanged(const QString& warnings);
    void bytesChanged(quint64 totalBytesIn, quint64 totalBytesOut);
    void banListChanged();

public Q_SLOTS:
    void updateBanlist();
    void updateAlert();
    void updateTimer();

private:
    OptionsModel* m_options_model{nullptr};

    int m_num_connections{0};
    int m_num_blocks{0};
    QDateTime m_last_block_date;
    double m_verification_progress{0.0};
    bool m_network_active{true};

    QString m_status_bar_warnings;

    QTimer* m_poll_timer{nullptr};
};

#endif
