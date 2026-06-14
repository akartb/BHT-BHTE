// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Transaction View

#ifndef BHT_WALLET_QT_TRANSACTIONVIEW_H
#define BHT_WALLET_QT_TRANSACTIONVIEW_H

#include <QtWidgets/QWidget>
#include <QtWidgets/QTableView>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QDateTimeEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFrame>
#include <QtCore/QDateTime>
#include <QtCore/QSettings>
#include <QtCore/QSortFilterProxyModel>

#include <memory>
#include <vector>

class WalletModel;
class TransactionTableModel;
class TransactionFilterProxy;
class TransactionRecord;

class TransactionView : public QWidget
{
    Q_OBJECT

public:
    explicit TransactionView(QWidget* parent = nullptr);
    ~TransactionView();

    void setModel(WalletModel* model);

    enum DateEnum {
        All,
        Today,
        ThisWeek,
        ThisMonth,
        LastMonth,
        ThisYear,
        Range
    };

    void chooseDate(int idx);
    void chooseType(int idx);
    void changedSearch();
    void changedAmount();

    void exportClicked();

    void focusTransaction(const QString& txid);

Q_SIGNALS:
    void doubleClicked(const QModelIndex& index);
    void message(const QString& title, const QString& body, unsigned int style);

private:
    void setupUI();
    QWidget* createDateRangeWidget();
    void dateRangeChanged();

    void contextualMenu(const QPoint& point);

    void copyAddress();
    void copyLabel();
    void copyAmount();
    void copyTxID();
    void copyTxHex();
    void copyTxPlainText();
    void showDetails();
    void editLabel();
    void abandonTx();
    void bumpFee();

    void openThirdPartyTxUrl(const QString& url);

    WalletModel* m_model{nullptr};
    TransactionTableModel* m_transaction_model{nullptr};
    TransactionFilterProxy* m_proxy_model{nullptr};

    QTableView* m_table_view{nullptr};
    QComboBox* m_date_widget{nullptr};
    QComboBox* m_type_widget{nullptr};
    QLineEdit* m_search_widget{nullptr};
    QLineEdit* m_amount_widget{nullptr};

    QFrame* m_date_range_widget{nullptr};
    QDateTimeEdit* m_date_from{nullptr};
    QDateTimeEdit* m_date_to{nullptr};

    QMenu* m_context_menu{nullptr};
    QAction* m_copy_address_action{nullptr};
    QAction* m_copy_label_action{nullptr};
    QAction* m_bump_fee_action{nullptr};
    QAction* m_abandon_action{nullptr};
};

class TransactionFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit TransactionFilterProxy(QObject* parent = nullptr);

    static const int ALL_TYPES = 0xFFFFFFFF;

    void setDateRange(const QDateTime& from, const QDateTime& to);
    void setTypeFilter(int type);
    void setSearchString(const QString& search);
    void setMinAmount(qint64 amount);
    void setShowInactive(bool show);

    int typeFilter() const { return m_type_filter; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QDateTime m_date_from;
    QDateTime m_date_to;
    int m_type_filter{ALL_TYPES};
    QString m_search_string;
    qint64 m_min_amount{0};
    bool m_show_inactive{true};
};

#endif
