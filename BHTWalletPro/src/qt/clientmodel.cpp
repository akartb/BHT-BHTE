// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Client Model Implementation

#include "clientmodel.h"
#include "optionsmodel.h"

#include <QtCore/QDateTime>
#include <QtCore/QSettings>

ClientModel::ClientModel(OptionsModel* optionsModel, QObject* parent)
    : QObject(parent)
    , m_options_model(optionsModel)
{
    m_poll_timer = new QTimer(this);
    connect(m_poll_timer, &QTimer::timeout, this, &ClientModel::updateTimer);
    m_poll_timer->start(1000);
}

ClientModel::~ClientModel()
{
    if (m_poll_timer) {
        m_poll_timer->stop();
    }
}

OptionsModel* ClientModel::getOptionsModel()
{
    return m_options_model;
}

int ClientModel::getNumConnections() const
{
    return m_num_connections;
}

int ClientModel::getNumBlocks() const
{
    return m_num_blocks;
}

uint256 ClientModel::getBestBlockHash()
{
    return uint256();
}

QDateTime ClientModel::getLastBlockDate() const
{
    return m_last_block_date;
}

double ClientModel::getVerificationProgress() const
{
    return m_verification_progress;
}

bool ClientModel::isNetworkActive() const
{
    return m_network_active;
}

void ClientModel::setNetworkActive(bool active)
{
    if (m_network_active != active) {
        m_network_active = active;
        Q_EMIT networkActiveChanged(active);
    }
}

QString ClientModel::getStatusBarWarnings() const
{
    return m_status_bar_warnings;
}

bool ClientModel::getProxyInfo(QString& ipPort) const
{
    QSettings settings;
    QString proxy = settings.value("proxy", "").toString();
    if (!proxy.isEmpty()) {
        ipPort = proxy;
        return true;
    }
    return false;
}

bool ClientModel::getOnionProxyInfo(QString& ipPort) const
{
    Q_UNUSED(ipPort);
    return false;
}

QString ClientModel::formatFullVersion() const
{
    return "BHT Wallet Pro v1.0.0";
}

QString ClientModel::formatSubVersion() const
{
    return "/BHT:1.0.0/";
}

bool ClientModel::isReleaseVersion() const
{
    return true;
}

QString ClientModel::clientName() const
{
    return "BHT Wallet Pro";
}

QString ClientModel::formatClientStartupTime() const
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
}

bool ClientModel::getHeaderTipHeight(int& height, int64_t& blockTime)
{
    height = m_num_blocks;
    blockTime = m_last_block_date.toSecsSinceEpoch();
    return true;
}

int ClientModel::getHeaderTipTime()
{
    return m_last_block_date.toSecsSinceEpoch();
}

ClientModel::BlockSource ClientModel::getBlockSource() const
{
    return BLOCK_SOURCE_NETWORK;
}

QString ClientModel::getNetworkName() const
{
    return "main";
}

bool ClientModel::isTestNetwork() const
{
    return false;
}

void ClientModel::updateNumConnections(int numConnections)
{
    if (m_num_connections != numConnections) {
        m_num_connections = numConnections;
        Q_EMIT numConnectionsChanged(numConnections);
    }
}

void ClientModel::updateNetworkActive(bool networkActive)
{
    if (m_network_active != networkActive) {
        m_network_active = networkActive;
        Q_EMIT networkActiveChanged(networkActive);
    }
}

void ClientModel::updateBanlist()
{
    Q_EMIT banListChanged();
}

void ClientModel::updateAlert()
{
    Q_EMIT alertsChanged(m_status_bar_warnings);
}

void ClientModel::updateTimer()
{
    m_num_blocks++;
    m_verification_progress = qMin(1.0, m_verification_progress + 0.001);
    m_last_block_date = QDateTime::currentDateTimeUtc();

    Q_EMIT numBlocksChanged(m_num_blocks, m_last_block_date, m_verification_progress, false);
}
