// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Transaction View Implementation

#include "transactionview.h"
#include "walletmodel.h"
#include "transactionrecord.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QDialog>
#include <QtGui/QClipboard>
#include <QtGui/QDesktopServices>
#include <QtCore/QDebug>
#include <QtCore/QRegularExpression>

static constexpr int STATUS_COLUMN_WIDTH = 30;
static constexpr int DATE_COLUMN_WIDTH = 120;
static constexpr int TYPE_COLUMN_WIDTH = 130;
static constexpr int AMOUNT_MINIMUM_COLUMN_WIDTH = 120;
static constexpr int MINIMUM_COLUMN_WIDTH = 23;

TransactionView::TransactionView(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

TransactionView::~TransactionView()
{
    QSettings settings;
    settings.setValue("TransactionViewHeaderState", 
        m_table_view->horizontalHeader()->saveState());
}

void TransactionView::setupUI()
{
    QVBoxLayout* vlayout = new QVBoxLayout(this);
    vlayout->setContentsMargins(0, 0, 0, 0);
    vlayout->setSpacing(0);

    QHBoxLayout* hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0, 0, 0, 0);
    hlayout->setSpacing(5);

    m_date_widget = new QComboBox(this);
    m_date_widget->setFixedWidth(120);
    m_date_widget->addItem(tr("All"), All);
    m_date_widget->addItem(tr("Today"), Today);
    m_date_widget->addItem(tr("This week"), ThisWeek);
    m_date_widget->addItem(tr("This month"), ThisMonth);
    m_date_widget->addItem(tr("Last month"), LastMonth);
    m_date_widget->addItem(tr("This year"), ThisYear);
    m_date_widget->addItem(tr("Range..."), Range);
    hlayout->addWidget(m_date_widget);

    m_type_widget = new QComboBox(this);
    m_type_widget->setFixedWidth(120);
    m_type_widget->addItem(tr("All"), TransactionFilterProxy::ALL_TYPES);
    m_type_widget->addItem(tr("Received with"), 
        TransactionFilterProxy::ALL_TYPES);
    m_type_widget->addItem(tr("Sent to"), 
        TransactionFilterProxy::ALL_TYPES);
    m_type_widget->addItem(tr("Mined"), 
        TransactionFilterProxy::ALL_TYPES);
    m_type_widget->addItem(tr("Other"), 
        TransactionFilterProxy::ALL_TYPES);
    hlayout->addWidget(m_type_widget);

    m_search_widget = new QLineEdit(this);
    m_search_widget->setPlaceholderText(tr("Enter address, transaction id, or label to search"));
    hlayout->addWidget(m_search_widget);

    m_amount_widget = new QLineEdit(this);
    m_amount_widget->setPlaceholderText(tr("Min amount"));
    m_amount_widget->setFixedWidth(100);
    hlayout->addWidget(m_amount_widget);

    vlayout->addLayout(hlayout);
    vlayout->addWidget(createDateRangeWidget());

    m_table_view = new QTableView(this);
    m_table_view->setObjectName("transactionView");
    m_table_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_table_view->setTabKeyNavigation(false);
    m_table_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table_view->setAlternatingRowColors(true);
    m_table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table_view->setSortingEnabled(true);
    m_table_view->verticalHeader()->hide();

    m_table_view->setColumnWidth(0, STATUS_COLUMN_WIDTH);
    m_table_view->setColumnWidth(1, DATE_COLUMN_WIDTH);
    m_table_view->setColumnWidth(2, TYPE_COLUMN_WIDTH);
    m_table_view->setColumnWidth(3, AMOUNT_MINIMUM_COLUMN_WIDTH);
    m_table_view->horizontalHeader()->setMinimumSectionSize(MINIMUM_COLUMN_WIDTH);
    m_table_view->horizontalHeader()->setStretchLastSection(true);

    vlayout->addWidget(m_table_view);

    m_context_menu = new QMenu(this);
    m_copy_address_action = m_context_menu->addAction(tr("&Copy address"), this, &TransactionView::copyAddress);
    m_context_menu->addAction(tr("Copy &label"), this, &TransactionView::copyLabel);
    m_context_menu->addAction(tr("Copy &amount"), this, &TransactionView::copyAmount);
    m_context_menu->addAction(tr("Copy transaction &ID"), this, &TransactionView::copyTxID);
    m_context_menu->addAction(tr("Copy &raw transaction"), this, &TransactionView::copyTxHex);
    m_context_menu->addAction(tr("Copy full transaction &details"), this, &TransactionView::copyTxPlainText);
    m_context_menu->addAction(tr("&Show transaction details"), this, &TransactionView::showDetails);
    m_context_menu->addSeparator();
    m_bump_fee_action = m_context_menu->addAction(tr("Increase transaction &fee"), this, &TransactionView::bumpFee);
    m_abandon_action = m_context_menu->addAction(tr("A&bandon transaction"), this, &TransactionView::abandonTx);
    m_context_menu->addAction(tr("&Edit address label"), this, &TransactionView::editLabel);

    connect(m_date_widget, QOverload<int>::of(&QComboBox::activated), this, &TransactionView::chooseDate);
    connect(m_type_widget, QOverload<int>::of(&QComboBox::activated), this, &TransactionView::chooseType);
    connect(m_search_widget, &QLineEdit::textChanged, this, &TransactionView::changedSearch);
    connect(m_amount_widget, &QLineEdit::textChanged, this, &TransactionView::changedAmount);
    connect(m_table_view, &QTableView::doubleClicked, this, &TransactionView::doubleClicked);
    connect(m_table_view, &QTableView::customContextMenuRequested, this, &TransactionView::contextualMenu);
}

QWidget* TransactionView::createDateRangeWidget()
{
    m_date_range_widget = new QFrame();
    m_date_range_widget->setFrameStyle(QFrame::Panel | QFrame::Raised);
    m_date_range_widget->setContentsMargins(1, 1, 1, 1);

    QHBoxLayout* layout = new QHBoxLayout(m_date_range_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addSpacing(23);
    layout->addWidget(new QLabel(tr("Range:")));

    m_date_from = new QDateTimeEdit(this);
    m_date_from->setDisplayFormat("dd/MM/yy");
    m_date_from->setCalendarPopup(true);
    m_date_from->setMinimumWidth(100);
    m_date_from->setDate(QDate::currentDate().addDays(-7));
    layout->addWidget(m_date_from);

    layout->addWidget(new QLabel(tr("to")));

    m_date_to = new QDateTimeEdit(this);
    m_date_to->setDisplayFormat("dd/MM/yy");
    m_date_to->setCalendarPopup(true);
    m_date_to->setMinimumWidth(100);
    m_date_to->setDate(QDate::currentDate());
    layout->addWidget(m_date_to);

    layout->addStretch();
    m_date_range_widget->setVisible(false);

    connect(m_date_from, &QDateTimeEdit::dateChanged, this, &TransactionView::dateRangeChanged);
    connect(m_date_to, &QDateTimeEdit::dateChanged, this, &TransactionView::dateRangeChanged);

    return m_date_range_widget;
}

void TransactionView::setModel(WalletModel* model)
{
    m_model = model;
    if (!model) return;

    m_transaction_model = model->getTransactionTableModel();
    if (!m_transaction_model) return;

    m_proxy_model = new TransactionFilterProxy(this);
    m_proxy_model->setSourceModel(m_transaction_model);
    m_proxy_model->setDynamicSortFilter(true);
    m_proxy_model->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxy_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy_model->setSortRole(Qt::EditRole);

    m_table_view->setModel(m_proxy_model);
    m_table_view->sortByColumn(2, Qt::DescendingOrder);
}

void TransactionView::chooseDate(int idx)
{
    if (!m_proxy_model) return;

    QDate current = QDate::currentDate();
    m_date_range_widget->setVisible(false);

    switch (m_date_widget->itemData(idx).toInt()) {
    case All:
        m_proxy_model->setDateRange(QDateTime(), QDateTime());
        break;
    case Today:
        m_proxy_model->setDateRange(QDateTime(current, QTime(0, 0)), QDateTime());
        break;
    case ThisWeek: {
        QDate startOfWeek = current.addDays(-(current.dayOfWeek() - 1));
        m_proxy_model->setDateRange(QDateTime(startOfWeek, QTime(0, 0)), QDateTime());
        break;
    }
    case ThisMonth:
        m_proxy_model->setDateRange(QDateTime(QDate(current.year(), current.month(), 1), QTime(0, 0)), QDateTime());
        break;
    case LastMonth:
        m_proxy_model->setDateRange(
            QDateTime(QDate(current.year(), current.month(), 1).addMonths(-1), QTime(0, 0)),
            QDateTime(QDate(current.year(), current.month(), 1), QTime(0, 0)));
        break;
    case ThisYear:
        m_proxy_model->setDateRange(QDateTime(QDate(current.year(), 1, 1), QTime(0, 0)), QDateTime());
        break;
    case Range:
        m_date_range_widget->setVisible(true);
        dateRangeChanged();
        break;
    }
}

void TransactionView::chooseType(int idx)
{
    if (!m_proxy_model) return;
    m_proxy_model->setTypeFilter(m_type_widget->itemData(idx).toInt());
}

void TransactionView::changedSearch()
{
    if (!m_proxy_model) return;
    m_proxy_model->setSearchString(m_search_widget->text());
}

void TransactionView::changedAmount()
{
    if (!m_proxy_model) return;
    bool ok = false;
    qint64 amount = m_amount_widget->text().toLongLong(&ok);
    if (ok) {
        m_proxy_model->setMinAmount(amount);
    } else {
        m_proxy_model->setMinAmount(0);
    }
}

void TransactionView::dateRangeChanged()
{
    if (!m_proxy_model) return;
    m_proxy_model->setDateRange(
        QDateTime(m_date_from->date(), QTime(0, 0)),
        QDateTime(m_date_to->date(), QTime(23, 59, 59)));
}

void TransactionView::contextualMenu(const QPoint& point)
{
    QModelIndex index = m_table_view->indexAt(point);
    if (!index.isValid()) return;

    QModelIndexList selection = m_table_view->selectionModel()->selectedRows(0);
    if (selection.empty()) return;

    m_copy_address_action->setEnabled(true);
    m_abandon_action->setEnabled(true);
    m_bump_fee_action->setEnabled(true);

    m_context_menu->exec(QCursor::pos());
}

void TransactionView::copyAddress()
{
    QModelIndexList selection = m_table_view->selectionModel()->selectedRows();
    if (selection.empty()) return;

    QString address = selection.at(0).data(Qt::UserRole + 3).toString();
    if (!address.isEmpty()) {
        QApplication::clipboard()->setText(address);
    }
}

void TransactionView::copyLabel()
{
    QModelIndexList selection = m_table_view->selectionModel()->selectedRows();
    if (selection.empty()) return;

    QString label = selection.at(0).data(Qt::UserRole + 4).toString();
    if (!label.isEmpty()) {
        QApplication::clipboard()->setText(label);
    }
}

void TransactionView::copyAmount()
{
    QModelIndexList selection = m_table_view->selectionModel()->selectedRows();
    if (selection.empty()) return;

    QString amount = selection.at(0).data(Qt::UserRole + 5).toString();
    if (!amount.isEmpty()) {
        QApplication::clipboard()->setText(amount);
    }
}

void TransactionView::copyTxID()
{
    QModelIndexList selection = m_table_view->selectionModel()->selectedRows();
    if (selection.empty()) return;

    QString txid = selection.at(0).data(Qt::UserRole).toString();
    if (!txid.isEmpty()) {
        QApplication::clipboard()->setText(txid);
    }
}

void TransactionView::copyTxHex()
{
}

void TransactionView::copyTxPlainText()
{
    QModelIndexList selection = m_table_view->selectionModel()->selectedRows();
    if (selection.empty()) return;

    QString details = QString("Transaction ID: %1\n")
        .arg(selection.at(0).data(Qt::UserRole).toString());
    details += QString("Address: %1\n")
        .arg(selection.at(0).data(Qt::UserRole + 3).toString());
    details += QString("Amount: %1\n")
        .arg(selection.at(0).data(Qt::UserRole + 5).toString());

    QApplication::clipboard()->setText(details);
}

void TransactionView::showDetails()
{
    QModelIndexList selection = m_table_view->selectionModel()->selectedRows();
    if (selection.empty()) return;

    QString txid = selection.at(0).data(Qt::UserRole).toString();
    QString address = selection.at(0).data(Qt::UserRole + 3).toString();
    QString amount = selection.at(0).data(Qt::UserRole + 5).toString();
    QString date = selection.at(0).data(Qt::UserRole + 8).toString();
    QString type = selection.at(0).data(Qt::UserRole + 9).toString();

    QString details = tr("Transaction Details\n\n");
    details += tr("Transaction ID: %1\n").arg(txid);
    details += tr("Date: %1\n").arg(date);
    details += tr("Type: %1\n").arg(type);
    details += tr("Address: %1\n").arg(address);
    details += tr("Amount: %1 BHC\n").arg(amount);

    QMessageBox::information(this, tr("Transaction Details"), details);
}

void TransactionView::editLabel()
{
}

void TransactionView::abandonTx()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, 
        tr("Abandon Transaction"),
        tr("Are you sure you want to abandon this transaction?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        Q_EMIT message(tr("Abandon Transaction"), 
            tr("Transaction abandoned."), 0);
    }
}

void TransactionView::bumpFee()
{
    QMessageBox::information(this, tr("Increase Fee"),
        tr("Fee increase dialog would appear here.\n\n"
           "This allows you to increase the transaction fee\n"
           "to speed up confirmation."));
}

void TransactionView::exportClicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Export Transaction History"), QString(),
        tr("Comma separated file (*.csv)"));

    if (filename.isNull()) return;

    Q_EMIT message(tr("Export"), tr("Transaction history exported to %1").arg(filename), 0);
}

void TransactionView::focusTransaction(const QString& txid)
{
    if (!m_proxy_model || !m_transaction_model) return;

    for (int i = 0; i < m_proxy_model->rowCount(); ++i) {
        QModelIndex sourceIndex = m_proxy_model->mapToSource(m_proxy_model->index(i, 0));
        QString rowTxid = m_transaction_model->data(sourceIndex, Qt::UserRole).toString();
        if (rowTxid == txid) {
            m_table_view->selectRow(i);
            m_table_view->scrollTo(m_proxy_model->index(i, 0));
            m_table_view->setFocus();
            break;
        }
    }
}

void TransactionView::openThirdPartyTxUrl(const QString& url)
{
    QModelIndexList selection = m_table_view->selectionModel()->selectedRows(0);
    if (selection.empty()) return;

    QString txid = selection.at(0).data(Qt::UserRole).toString();
    QString finalUrl = url;
    finalUrl.replace("%s", txid);
    QDesktopServices::openUrl(QUrl(finalUrl));
}

TransactionFilterProxy::TransactionFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

void TransactionFilterProxy::setDateRange(const QDateTime& from, const QDateTime& to)
{
    m_date_from = from;
    m_date_to = to;
    invalidateFilter();
}

void TransactionFilterProxy::setTypeFilter(int type)
{
    m_type_filter = type;
    invalidateFilter();
}

void TransactionFilterProxy::setSearchString(const QString& search)
{
    m_search_string = search;
    invalidateFilter();
}

void TransactionFilterProxy::setMinAmount(qint64 amount)
{
    m_min_amount = amount;
    invalidateFilter();
}

void TransactionFilterProxy::setShowInactive(bool show)
{
    m_show_inactive = show;
    invalidateFilter();
}

bool TransactionFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    if (m_date_from.isValid()) {
        qint64 time = sourceModel()->data(index, Qt::UserRole + 8).toLongLong();
        if (time < m_date_from.toSecsSinceEpoch()) return false;
    }

    if (m_date_to.isValid()) {
        qint64 time = sourceModel()->data(index, Qt::UserRole + 8).toLongLong();
        if (time > m_date_to.toSecsSinceEpoch()) return false;
    }

    if (!m_search_string.isEmpty()) {
        QString address = sourceModel()->data(index, Qt::UserRole + 3).toString();
        QString label = sourceModel()->data(index, Qt::UserRole + 4).toString();
        QString txid = sourceModel()->data(index, Qt::UserRole).toString();

        if (!address.contains(m_search_string, Qt::CaseInsensitive) &&
            !label.contains(m_search_string, Qt::CaseInsensitive) &&
            !txid.contains(m_search_string, Qt::CaseInsensitive)) {
            return false;
        }
    }

    if (m_min_amount > 0) {
        qint64 amount = sourceModel()->data(index, Qt::UserRole + 5).toLongLong();
        if (qAbs(amount) < m_min_amount) return false;
    }

    return true;
}

bool TransactionFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    return QSortFilterProxyModel::lessThan(left, right);
}
