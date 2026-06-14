// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Address Book Page Implementation

#include "addressbookpage.h"
#include "walletmodel.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtGui/QClipboard>
#include <QtCore/QDebug>

AddressBookPage::AddressBookPage(Mode mode, Tabs tab, QWidget* parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_tab(tab)
{
    setupUI();
}

AddressBookPage::~AddressBookPage()
{
}

void AddressBookPage::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    QHBoxLayout* searchLayout = new QHBoxLayout();
    m_search_line = new QLineEdit(this);
    m_search_line->setPlaceholderText(tr("Search address or label"));
    searchLayout->addWidget(m_search_line);
    mainLayout->addLayout(searchLayout);

    m_explanation_label = new QLabel(this);
    m_explanation_label->setWordWrap(true);
    m_explanation_label->setStyleSheet("color: #666; font-size: 11px;");
    mainLayout->addWidget(m_explanation_label);

    m_table_view = new QTableView(this);
    m_table_view->setAlternatingRowColors(true);
    m_table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table_view->setSortingEnabled(true);
    m_table_view->verticalHeader()->hide();
    m_table_view->setContextMenuPolicy(Qt::CustomContextMenu);

    m_table_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table_view->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    mainLayout->addWidget(m_table_view);

    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_new_address_btn = new QPushButton(tr("&New Address"), this);
    m_new_address_btn->setIcon(QIcon(":/icons/add"));
    buttonLayout->addWidget(m_new_address_btn);

    m_copy_address_btn = new QPushButton(tr("&Copy Address"), this);
    m_copy_address_btn->setIcon(QIcon(":/icons/editcopy"));
    m_copy_address_btn->setEnabled(false);
    buttonLayout->addWidget(m_copy_address_btn);

    m_delete_address_btn = new QPushButton(tr("&Delete"), this);
    m_delete_address_btn->setIcon(QIcon(":/icons/remove"));
    m_delete_address_btn->setEnabled(false);
    buttonLayout->addWidget(m_delete_address_btn);

    buttonLayout->addStretch();

    m_export_btn = new QPushButton(tr("&Export"), this);
    m_export_btn->setIcon(QIcon(":/icons/export"));
    buttonLayout->addWidget(m_export_btn);

    m_close_btn = new QPushButton(tr("&Close"), this);
    buttonLayout->addWidget(m_close_btn);

    mainLayout->addLayout(buttonLayout);

    m_context_menu = new QMenu(this);
    m_context_menu->addAction(tr("&Copy Address"), this, &AddressBookPage::on_copyAddress_clicked);
    m_context_menu->addAction(tr("Copy &Label"), this, &AddressBookPage::onCopyLabelAction);
    m_context_menu->addAction(tr("&Edit"), this, &AddressBookPage::onEditAction);

    if (m_tab == SendingTab) {
        m_context_menu->addAction(tr("&Delete"), this, &AddressBookPage::on_deleteAddress_clicked);
    }

    if (m_mode == ForSelection) {
        switch (m_tab) {
        case SendingTab:
            setWindowTitle(tr("Choose the address to send coins to"));
            break;
        case ReceivingTab:
            setWindowTitle(tr("Choose the address to receive coins with"));
            break;
        }
        connect(m_table_view, &QTableView::doubleClicked, this, &QDialog::accept);
        m_table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table_view->setFocus();
        m_close_btn->setText(tr("C&hoose"));
        m_export_btn->hide();
    }

    switch (m_tab) {
    case SendingTab:
        m_explanation_label->setText(tr("These are your BHT addresses for sending payments. "
            "Always check the amount and the receiving address before sending coins."));
        m_delete_address_btn->setVisible(true);
        m_new_address_btn->setVisible(true);
        break;
    case ReceivingTab:
        m_explanation_label->setText(tr("These are your BHT addresses for receiving payments. "
            "Use the 'Create new receiving address' button in the receive tab to create new addresses."));
        m_delete_address_btn->setVisible(false);
        m_new_address_btn->setVisible(false);
        break;
    }

    updateWindowTitle();

    connect(m_new_address_btn, &QPushButton::clicked, this, &AddressBookPage::on_newAddress_clicked);
    connect(m_copy_address_btn, &QPushButton::clicked, this, &AddressBookPage::on_copyAddress_clicked);
    connect(m_delete_address_btn, &QPushButton::clicked, this, &AddressBookPage::on_deleteAddress_clicked);
    connect(m_export_btn, &QPushButton::clicked, this, &AddressBookPage::on_exportButton_clicked);
    connect(m_close_btn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_table_view, &QTableView::customContextMenuRequested, this, &AddressBookPage::contextualMenu);
}

void AddressBookPage::setModel(AddressTableModel* model)
{
    m_model = model;
    if (!model) return;

    QString type = (m_tab == ReceivingTab) ? "receive" : "send";
    m_proxy_model = new AddressBookSortFilterProxyModel(type, this);
    m_proxy_model->setSourceModel(model);
    m_proxy_model->setDynamicSortFilter(true);
    m_proxy_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy_model->setSortCaseSensitivity(Qt::CaseInsensitive);

    connect(m_search_line, &QLineEdit::textChanged, m_proxy_model, &QSortFilterProxyModel::setFilterWildcard);

    m_table_view->setModel(m_proxy_model);
    m_table_view->sortByColumn(0, Qt::AscendingOrder);

    connect(m_table_view->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, &AddressBookPage::selectionChanged);

    connect(model, &QAbstractItemModel::rowsInserted, this, &AddressBookPage::selectNewAddress);

    selectionChanged();
}

void AddressBookPage::updateWindowTitle()
{
    if (m_mode == ForEditing) {
        switch (m_tab) {
        case SendingTab:
            setWindowTitle(tr("Sending addresses"));
            break;
        case ReceivingTab:
            setWindowTitle(tr("Receiving addresses"));
            break;
        }
    }
}

void AddressBookPage::selectionChanged()
{
    if (!m_table_view->selectionModel()) return;

    bool hasSelection = m_table_view->selectionModel()->hasSelection();

    switch (m_tab) {
    case SendingTab:
        m_delete_address_btn->setEnabled(hasSelection);
        m_delete_address_btn->setVisible(true);
        break;
    case ReceivingTab:
        m_delete_address_btn->setEnabled(false);
        m_delete_address_btn->setVisible(false);
        break;
    }

    m_copy_address_btn->setEnabled(hasSelection);
}

void AddressBookPage::on_newAddress_clicked()
{
    if (!m_model) return;

    if (m_tab == ReceivingTab) return;

    bool ok;
    QString label = QInputDialog::getText(this, tr("New sending address"),
        tr("Label for the new address:"), QLineEdit::Normal, "", &ok);

    if (!ok || label.isEmpty()) return;

    QString address = QInputDialog::getText(this, tr("New sending address"),
        tr("Address:"), QLineEdit::Normal, "", &ok);

    if (!ok || address.isEmpty()) return;

    m_new_address_to_select = address;
    m_model->addAddress(address, label);
}

void AddressBookPage::on_copyAddress_clicked()
{
    if (!m_table_view->selectionModel()) return;

    QModelIndexList indexes = m_table_view->selectionModel()->selectedRows(1);
    if (indexes.isEmpty()) return;

    QString address = indexes.at(0).data().toString();
    QApplication::clipboard()->setText(address);
}

void AddressBookPage::onCopyLabelAction()
{
    if (!m_table_view->selectionModel()) return;

    QModelIndexList indexes = m_table_view->selectionModel()->selectedRows(0);
    if (indexes.isEmpty()) return;

    QString label = indexes.at(0).data().toString();
    QApplication::clipboard()->setText(label);
}

void AddressBookPage::onEditAction()
{
    if (!m_model || !m_table_view->selectionModel()) return;

    QModelIndexList indexes = m_table_view->selectionModel()->selectedRows();
    if (indexes.isEmpty()) return;

    QModelIndex origIndex = m_proxy_model->mapToSource(indexes.at(0));
    QString currentLabel = m_model->data(m_model->index(origIndex.row(), 0)).toString();
    QString currentAddress = m_model->data(m_model->index(origIndex.row(), 1)).toString();

    bool ok;
    QString newLabel = QInputDialog::getText(this, tr("Edit address label"),
        tr("New label:"), QLineEdit::Normal, currentLabel, &ok);

    if (ok && !newLabel.isEmpty()) {
        m_model->updateAddress(currentAddress, newLabel, true, false, 0);
    }
}

void AddressBookPage::on_deleteAddress_clicked()
{
    if (!m_table_view->selectionModel() || !m_table_view->model()) return;

    QModelIndexList indexes = m_table_view->selectionModel()->selectedRows();
    if (indexes.isEmpty()) return;

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        tr("Delete address"),
        tr("Are you sure you want to delete this address?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_table_view->model()->removeRow(indexes.at(0).row());
    }
}

void AddressBookPage::on_exportButton_clicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Export Address List"), QString(),
        tr("Comma separated file (*.csv)"));

    if (filename.isNull()) return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Exporting Failed"),
            tr("There was an error trying to save the address list to %1.").arg(filename));
        return;
    }

    QTextStream out(&file);
    out << "Label,Address\n";

    if (m_proxy_model) {
        for (int i = 0; i < m_proxy_model->rowCount(); ++i) {
            QString label = m_proxy_model->data(m_proxy_model->index(i, 0)).toString();
            QString address = m_proxy_model->data(m_proxy_model->index(i, 1)).toString();
            out << QString("\"%1\",\"%2\"\n").arg(label, address);
        }
    }

    file.close();
    QMessageBox::information(this, tr("Exporting Successful"),
        tr("The address list was successfully saved to %1.").arg(filename));
}

void AddressBookPage::contextualMenu(const QPoint& point)
{
    QModelIndex index = m_table_view->indexAt(point);
    if (index.isValid()) {
        m_context_menu->exec(QCursor::pos());
    }
}

void AddressBookPage::selectNewAddress(const QModelIndex& parent, int begin, int end)
{
    Q_UNUSED(end);

    if (!m_proxy_model || !m_model) return;

    QModelIndex idx = m_proxy_model->mapFromSource(m_model->index(begin, 1, parent));
    if (idx.isValid() && idx.data(Qt::EditRole).toString() == m_new_address_to_select) {
        m_table_view->setFocus();
        m_table_view->selectRow(idx.row());
        m_new_address_to_select.clear();
    }
}

void AddressBookPage::done(int retval)
{
    if (!m_table_view->selectionModel() || !m_table_view->model()) {
        QDialog::done(retval);
        return;
    }

    QModelIndexList indexes = m_table_view->selectionModel()->selectedRows(1);

    for (const QModelIndex& index : indexes) {
        QVariant address = m_table_view->model()->data(index);
        m_return_value = address.toString();
    }

    if (m_return_value.isEmpty() && m_mode == ForSelection) {
        retval = Rejected;
    }

    QDialog::done(retval);
}

AddressBookSortFilterProxyModel::AddressBookSortFilterProxyModel(const QString& type, QObject* parent)
    : QSortFilterProxyModel(parent)
    , m_type(type)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

bool AddressBookSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    auto model = sourceModel();
    auto labelIndex = model->index(sourceRow, 0, sourceParent);

    QString rowType = model->data(labelIndex, Qt::UserRole).toString();
    if (rowType != m_type) {
        return false;
    }

    auto addressIndex = model->index(sourceRow, 1, sourceParent);

    const auto pattern = filterRegularExpression();
    return (model->data(addressIndex).toString().contains(pattern) ||
            model->data(labelIndex).toString().contains(pattern));
}
