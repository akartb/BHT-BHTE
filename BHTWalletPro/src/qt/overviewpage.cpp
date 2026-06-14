// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Overview Page Implementation

#include "overviewpage.h"
#include "walletmodel.h"
#include "transactionview.h"
#include "clientmodel.h"
#include "optionsmodel.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtGui/QPainter>
#include <QtGui/QIcon>
#include <QtCore/QAbstractItemModel>

static constexpr int DECORATION_SIZE = 54;
static constexpr int NUM_ITEMS = 5;
static constexpr QColor COLOR_NEGATIVE(255, 0, 0);
static constexpr QColor COLOR_UNCONFIRMED(128, 128, 128);

OverviewPage::OverviewPage(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

OverviewPage::~OverviewPage()
{
}

void OverviewPage::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    QHBoxLayout* headerLayout = new QHBoxLayout();

    QLabel* titleLabel = new QLabel(tr("Overview"));
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #333;");
    headerLayout->addWidget(titleLabel);

    m_wallet_status_icon = new QLabel(this);
    m_wallet_status_icon->setPixmap(QIcon(":/icons/warning").pixmap(16, 16));
    m_wallet_status_icon->setToolTip(tr("Wallet is out of sync"));
    headerLayout->addWidget(m_wallet_status_icon);

    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    QFrame* balanceFrame = new QFrame(this);
    balanceFrame->setStyleSheet("QFrame { background: #f8f9fa; border-radius: 10px; padding: 15px; }");
    QVBoxLayout* balanceLayout = new QVBoxLayout(balanceFrame);

    QHBoxLayout* balanceRow = new QHBoxLayout();
    
    QVBoxLayout* balanceCol = new QVBoxLayout();
    QLabel* balanceTitle = new QLabel(tr("Available"));
    balanceTitle->setStyleSheet("font-size: 12px; color: #666;");
    balanceCol->addWidget(balanceTitle);

    m_balance_label = new QLabel(tr("0.00000000 BHC"));
    m_balance_label->setStyleSheet("font-size: 28px; font-weight: bold; color: #2196F3;");
    balanceCol->addWidget(m_balance_label);

    balanceRow->addLayout(balanceCol);
    balanceRow->addSpacing(40);

    QVBoxLayout* pendingCol = new QVBoxLayout();
    QLabel* pendingTitle = new QLabel(tr("Pending"));
    pendingTitle->setStyleSheet("font-size: 12px; color: #666;");
    pendingCol->addWidget(pendingTitle);

    m_unconfirmed_label = new QLabel(tr("0.00000000 BHC"));
    m_unconfirmed_label->setStyleSheet("font-size: 18px; font-weight: bold; color: #FF9800;");
    pendingCol->addWidget(m_unconfirmed_label);

    balanceRow->addLayout(pendingCol);
    balanceRow->addSpacing(40);

    QVBoxLayout* totalCol = new QVBoxLayout();
    QLabel* totalTitle = new QLabel(tr("Total"));
    totalTitle->setStyleSheet("font-size: 12px; color: #666;");
    totalCol->addWidget(totalTitle);

    m_total_label = new QLabel(tr("0.00000000 BHC"));
    m_total_label->setStyleSheet("font-size: 18px; font-weight: bold; color: #4CAF50;");
    totalCol->addWidget(m_total_label);

    balanceRow->addLayout(totalCol);
    balanceRow->addStretch();

    balanceLayout->addLayout(balanceRow);

    m_immature_label = new QLabel(tr("Immature: 0.00000000 BHC"));
    m_immature_label->setStyleSheet("font-size: 12px; color: #999;");
    m_immature_label->setVisible(false);
    balanceLayout->addWidget(m_immature_label);

    m_watch_only_label = new QLabel(tr("Watch-only: 0.00000000 BHC"));
    m_watch_only_label->setStyleSheet("font-size: 12px; color: #999;");
    m_watch_only_label->setVisible(false);
    balanceLayout->addWidget(m_watch_only_label);

    mainLayout->addWidget(balanceFrame);

    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_send_button = new QPushButton(tr("Send"));
    m_send_button->setIcon(QIcon(":/icons/send"));
    m_send_button->setStyleSheet(
        "QPushButton { background: #4CAF50; color: white; padding: 12px 30px; "
        "border-radius: 8px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background: #45a049; }");
    buttonLayout->addWidget(m_send_button);

    m_receive_button = new QPushButton(tr("Receive"));
    m_receive_button->setIcon(QIcon(":/icons/receive"));
    m_receive_button->setStyleSheet(
        "QPushButton { background: #2196F3; color: white; padding: 12px 30px; "
        "border-radius: 8px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background: #1976D2; }");
    buttonLayout->addWidget(m_receive_button);

    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    m_alert_label = new QLabel();
    m_alert_label->setStyleSheet("color: red; font-weight: bold;");
    m_alert_label->setVisible(false);
    mainLayout->addWidget(m_alert_label);

    QHBoxLayout* recentHeader = new QHBoxLayout();
    QLabel* recentLabel = new QLabel(tr("Recent Transactions"));
    recentLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #333;");
    recentHeader->addWidget(recentLabel);

    m_tx_status_icon = new QLabel(this);
    m_tx_status_icon->setPixmap(QIcon(":/icons/warning").pixmap(16, 16));
    m_tx_status_icon->setToolTip(tr("Transactions are out of sync"));
    recentHeader->addWidget(m_tx_status_icon);

    recentHeader->addStretch();
    mainLayout->addLayout(recentHeader);

    m_transaction_list = new QListView(this);
    m_transaction_list->setStyleSheet(
        "QListView { background: white; border: 1px solid #ddd; border-radius: 8px; }");
    m_transaction_list->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    m_transaction_list->setAttribute(Qt::WA_MacShowFocusRect, false);
    mainLayout->addWidget(m_transaction_list, 1);

    connect(m_transaction_list, &QListView::clicked, this, &OverviewPage::handleTransactionClicked);
}

void OverviewPage::setWalletModel(WalletModel* model)
{
    m_wallet_model = model;
    if (!model) return;

    m_filter = std::make_unique<TransactionFilterProxy>();
    m_filter->setSourceModel(model->getTransactionTableModel());
    m_filter->setDynamicSortFilter(true);
    m_filter->setSortRole(Qt::EditRole);
    m_filter->setShowInactive(false);
    m_filter->sort(2, Qt::DescendingOrder);

    m_transaction_list->setModel(m_filter.get());
    m_transaction_list->setModelColumn(4);

    connect(m_filter.get(), &TransactionFilterProxy::rowsInserted, this, &OverviewPage::limitTransactionRows);
    connect(m_filter.get(), &TransactionFilterProxy::rowsRemoved, this, &OverviewPage::limitTransactionRows);

    limitTransactionRows();

    if (model->getOptionsModel()) {
        connect(model->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &OverviewPage::updateDisplayUnit);
    }
}

void OverviewPage::setClientModel(ClientModel* model)
{
    m_client_model = model;
    if (model) {
        connect(model, &ClientModel::alertsChanged, this, &OverviewPage::updateAlerts);
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setBalance(qint64 balance, qint64 unconfirmed, qint64 immature,
                              qint64 watchOnly, qint64 watchUnconfirmed, qint64 watchImmature)
{
    Q_UNUSED(watchUnconfirmed);
    Q_UNUSED(watchImmature);

    QString unit = "BHC";

    auto formatBalance = [this, &unit](qint64 amount) -> QString {
        if (m_privacy) {
            return "***** " + unit;
        }
        double btcAmount = amount / 100000000.0;
        return QString("%1 %2").arg(btcAmount, 0, 'f', 8).arg(unit);
    };

    m_balance_label->setText(formatBalance(balance));
    m_unconfirmed_label->setText(formatBalance(unconfirmed));
    m_immature_label->setText(tr("Immature: %1").arg(formatBalance(immature)));
    m_total_label->setText(formatBalance(balance + unconfirmed + immature));

    m_immature_label->setVisible(immature != 0);
    m_watch_only_label->setVisible(watchOnly != 0);
    m_watch_only_label->setText(tr("Watch-only: %1").arg(formatBalance(watchOnly)));
}

void OverviewPage::setPrivacy(bool privacy)
{
    m_privacy = privacy;
    m_transaction_list->setVisible(!privacy);
}

void OverviewPage::updateDisplayUnit()
{
    if (!m_wallet_model) return;
}

void OverviewPage::handleTransactionClicked(const QModelIndex& index)
{
    if (m_filter) {
        Q_EMIT transactionClicked(m_filter->mapToSource(index));
    }
}

void OverviewPage::limitTransactionRows()
{
    if (!m_filter || !m_transaction_list) return;

    for (int i = 0; i < m_filter->rowCount(); ++i) {
        m_transaction_list->setRowHidden(i, i >= NUM_ITEMS);
    }
}

void OverviewPage::updateAlerts(const QString& warnings)
{
    m_alert_label->setVisible(!warnings.isEmpty());
    m_alert_label->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool show)
{
    m_wallet_status_icon->setVisible(show);
    m_tx_status_icon->setVisible(show);
}

TxViewDelegate::TxViewDelegate(QObject* parent)
    : QAbstractItemDelegate(parent)
{
}

void TxViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const
{
    painter->save();

    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    QRect mainRect = option.rect;
    QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));

    if (!icon.isNull()) {
        icon.paint(painter, decorationRect);
    }

    int xspace = DECORATION_SIZE + 8;
    int ypad = 6;
    int halfheight = (mainRect.height() - 2 * ypad) / 2;

    QRect amountRect(mainRect.left() + xspace, mainRect.top() + ypad, 
                     mainRect.width() - xspace, halfheight);
    QRect addressRect(mainRect.left() + xspace, mainRect.top() + ypad + halfheight, 
                      mainRect.width() - xspace, halfheight);

    QString address = index.data(Qt::DisplayRole).toString();
    qint64 amount = index.data(Qt::UserRole + 5).toLongLong();
    bool confirmed = index.data(Qt::UserRole + 6).toBool();

    QDateTime date = QDateTime::fromSecsSinceEpoch(index.data(Qt::UserRole + 8).toLongLong());

    QColor foreground = option.palette.color(QPalette::Text);
    painter->setPen(foreground);
    painter->drawText(addressRect, Qt::AlignLeft | Qt::AlignVCenter, address);

    if (amount < 0) {
        foreground = COLOR_NEGATIVE;
    } else if (!confirmed) {
        foreground = COLOR_UNCONFIRMED;
    }

    painter->setPen(foreground);

    QString amountText = QString::number(amount / 100000000.0, 'f', 8) + " BHC";
    if (!confirmed) {
        amountText = "[" + amountText + "]";
    }

    painter->drawText(amountRect, Qt::AlignRight | Qt::AlignVCenter, amountText);

    painter->setPen(option.palette.color(QPalette::Text));
    painter->drawText(amountRect, Qt::AlignLeft | Qt::AlignVCenter, 
                      date.toString("dd.MM.yyyy hh:mm"));

    painter->restore();
}

QSize TxViewDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(DECORATION_SIZE + 200, DECORATION_SIZE);
}
