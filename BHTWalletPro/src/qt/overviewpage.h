// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Overview Page

#ifndef BHT_WALLET_QT_OVERVIEWPAGE_H
#define BHT_WALLET_QT_OVERVIEWPAGE_H

#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QListView>
#include <QtCore/QDateTime>

#include <memory>

class WalletModel;
class TransactionFilterProxy;
class ClientModel;

class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(QWidget* parent = nullptr);
    ~OverviewPage();

    void setWalletModel(WalletModel* model);
    void setClientModel(ClientModel* model);

    void setBalance(qint64 balance, qint64 unconfirmed, qint64 immature,
                    qint64 watchOnly, qint64 watchUnconfirmed, qint64 watchImmature);

    void showOutOfSyncWarning(bool show);

Q_SIGNALS:
    void transactionClicked(const QModelIndex& index);
    void outOfSyncWarningClicked();

public Q_SLOTS:
    void setPrivacy(bool privacy);
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex& index);
    void limitTransactionRows();

private:
    void setupUI();
    void updateAlerts(const QString& warnings);

    WalletModel* m_wallet_model{nullptr};
    ClientModel* m_client_model{nullptr};
    std::unique_ptr<TransactionFilterProxy> m_filter;

    bool m_privacy{false};

    QLabel* m_balance_label{nullptr};
    QLabel* m_unconfirmed_label{nullptr};
    QLabel* m_immature_label{nullptr};
    QLabel* m_total_label{nullptr};
    QLabel* m_watch_only_label{nullptr};

    QLabel* m_wallet_status_icon{nullptr};
    QLabel* m_tx_status_icon{nullptr};
    QLabel* m_alert_label{nullptr};

    QListView* m_transaction_list{nullptr};
    QPushButton* m_send_button{nullptr};
    QPushButton* m_receive_button{nullptr};
};

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT

public:
    explicit TxViewDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    int unit{0};

Q_SIGNALS:
    void widthChanged(const QModelIndex& index) const;

private:
    mutable std::map<int, int> m_minimum_width;
};

#endif
