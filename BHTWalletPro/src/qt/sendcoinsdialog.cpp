#include "sendcoinsdialog.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "addressbookpage.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QButtonGroup>
#include <QtGui/QClipboard>

static constexpr int64_t SATOSHI = 100000000;

SendCoinsDialog::SendCoinsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

SendCoinsDialog::~SendCoinsDialog()
{
}

void SendCoinsDialog::setupUi()
{
    setWindowTitle("Send BHT");
    setMinimumWidth(650);
    setMinimumHeight(500);

    m_main_layout = new QVBoxLayout(this);
    m_main_layout->setSpacing(10);
    m_main_layout->setContentsMargins(15, 15, 15, 15);

    QHBoxLayout* assetLayout = new QHBoxLayout();
    m_asset_label = new QLabel("Asset:");
    assetLayout->addWidget(m_asset_label);
    m_asset_combo = new QComboBox();
    m_asset_combo->addItem("BHT", 0);
    m_asset_combo->addItem("BHTE", 1);
    connect(m_asset_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SendCoinsDialog::handleAssetChanged);
    assetLayout->addWidget(m_asset_combo);
    assetLayout->addStretch();
    m_main_layout->addLayout(assetLayout);

    m_entry = new SendCoinsEntry(this);
    m_main_layout->addWidget(m_entry);

    QGroupBox* feeGroup = new QGroupBox("Transaction Fee");
    QVBoxLayout* feeLayout = new QVBoxLayout(feeGroup);
    m_fee_limit_label = new QLabel("Estimated fee: 0.00000000 BHT");
    m_fee_limit_label->setStyleSheet("color: #666;");
    feeLayout->addWidget(m_fee_limit_label);
    m_main_layout->addWidget(feeGroup);

    m_balance_label = new QLabel("Balance: 0.00000000 BHT");
    m_balance_label->setStyleSheet("font-size: 16px; color: #333;");
    m_main_layout->addWidget(m_balance_label);

    m_main_layout->addStretch();

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_send_button = new QPushButton("&Send");
    m_send_button->setStyleSheet(
        "QPushButton { background: #4CAF50; color: white; padding: 10px 25px; "
        "border-radius: 5px; font-size: 14px; } QPushButton:hover { background: #45a049; }");
    connect(m_send_button, &QPushButton::clicked, this, &SendCoinsDialog::on_sendButton_clicked);
    buttonLayout->addWidget(m_send_button);

    m_clear_button = new QPushButton("C&lear");
    connect(m_clear_button, &QPushButton::clicked, this, &SendCoinsDialog::on_clearButton_clicked);
    buttonLayout->addWidget(m_clear_button);

    m_add_entry_button = new QPushButton("Add &Recipient");
    connect(m_add_entry_button, &QPushButton::clicked, this, &SendCoinsDialog::on_addEntry_button_clicked);
    buttonLayout->addWidget(m_add_entry_button);
    buttonLayout->addStretch();

    QPushButton* closeBtn = new QPushButton("&Cancel");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(closeBtn);

    m_main_layout->addLayout(buttonLayout);
    connect(m_entry, &SendCoinsEntry::removeEntry, this, &SendCoinsDialog::removeEntry);
}

void SendCoinsDialog::setModel(WalletModel* model)
{
    m_model = model;
    if (m_entry) m_entry->setModel(model);
    if (model && model->getOptionsModel()) {
        connect(model->getOptionsModel(), &OptionsModel::displayUnitChanged,
                this, &SendCoinsDialog::updateDisplayUnit);
    }
    if (model) {
        setBalanceBalance(model->getBalance(), model->getUnconfirmedBalance(),
                         model->getImmatureBalance(), model->getWatchOnlyBalance(), 0, 0);
    }
}

void SendCoinsDialog::setBalanceBalance(int64_t balance, int64_t unconfirmed, int64_t immature,
                                       int64_t watchOnly, int64_t watchUnconf, int64_t watchImmature)
{
    m_balance = balance;
    m_unconfirmed_balance = unconfirmed;
    m_immature_balance = immature;
    m_watch_only_balance = watchOnly;
    m_watch_only_unconfirmed_balance = watchUnconf;
    m_watch_only_immature_balance = watchImmature;
    QString unit = m_current_asset == 1 ? "BHTE" : "BHT";
    QString text = QString::number(balance / (double)SATOSHI, 'f', 8) + " " + unit;
    if (m_balance_label) m_balance_label->setText("Balance: " + text);
}

void SendCoinsDialog::on_sendButton_clicked()
{
    if (!m_entry || !m_model) return;
    SendCoinsRecipient recipient = m_entry->getValue();
    if (recipient.address.isEmpty()) {
        QMessageBox::warning(this, "Send Coins", "Please enter a valid address.");
        return;
    }
    if (recipient.amount <= 0) {
        QMessageBox::warning(this, "Send Coins", "Please enter a valid amount.");
        return;
    }
    QList<SendCoinsRecipient> recipients;
    recipients.append(recipient);
    auto result = m_model->sendCoins(recipients);
    if (result.status == WalletModel::OK) {
        QMessageBox::information(this, "Send Coins",
            "Transaction created!\n\nTXID: " + result.txid);
        m_return_value = result.txid;
        accept();
    } else {
        QMessageBox::critical(this, "Error", result.msg);
    }
}

void SendCoinsDialog::on_clearButton_clicked()
{
    if (m_entry) m_entry->clear();
}

void SendCoinsDialog::on_addEntry_button_clicked()
{
}

void SendCoinsDialog::removeEntry(SendCoinsEntry* entry)
{
    Q_UNUSED(entry);
}

void SendCoinsDialog::updateCoinControlVisibility() {}
void SendCoinsDialog::updateDisplayUnit() {}
void SendCoinsDialog::updateFeeSection()
{
    QString unit = m_current_asset == 1 ? "BHTE gas" : "BHT";
    if (m_fee_limit_label) m_fee_limit_label->setText("Estimated fee: 0.00000100 " + unit);
}
void SendCoinsDialog::handleEntryChanged() {}
void SendCoinsDialog::handleAddressChanged(const QString& a) { Q_UNUSED(a); }
void SendCoinsDialog::handleAmountChanged() {}
void SendCoinsDialog::handleAssetChanged(int idx)
{
    m_current_asset = idx;
    const QString unit = idx == 1 ? "BHTE" : "BHT";
    setWindowTitle("Send " + unit);
    if (m_entry) {
        QComboBox* assetCombo = m_entry->findChild<QComboBox*>("assetCombo");
        if (assetCombo && assetCombo->currentIndex() != idx) {
            assetCombo->setCurrentIndex(idx);
        }
    }
    updateFeeSection();
    setBalanceBalance(m_balance, m_unconfirmed_balance, m_immature_balance,
                      m_watch_only_balance, m_watch_only_unconfirmed_balance,
                      m_watch_only_immature_balance);
}
void SendCoinsDialog::setAddress(const QString& addr)
{
    if (m_entry) m_entry->setAddress(addr);
}
void SendCoinsDialog::setAddress(const QString& addr, const QString& label,
                                 const QString& asset, int64_t amount)
{
    Q_UNUSED(label); Q_UNUSED(asset); Q_UNUSED(amount);
    if (m_entry) m_entry->setAddress(addr);
}

void SendCoinsDialog::accept() { QDialog::accept(); }
void SendCoinsDialog::reject() { QDialog::reject(); }

void SendCoinsDialog::pasteEntry(const SendCoinsRecipient& recipient)
{
    if (m_entry) m_entry->setValue(recipient);
}

void SendCoinsDialog::handlePaste(const SendCoinsRecipient& recipient)
{
    pasteEntry(recipient);
}

SendCoinsEntry::SendCoinsEntry(QWidget* parent)
    : QFrame(parent)
{
    setupUi(parent);
}

SendCoinsEntry::~SendCoinsEntry() {}

void SendCoinsEntry::setupUi(QWidget* p)
{
    Q_UNUSED(p);
    setFrameStyle(QFrame::Panel | QFrame::Raised);
    setStyleSheet("SendCoinsEntry { border: 1px solid #ddd; border-radius: 5px; padding: 10px; }");

    QGridLayout* layout = new QGridLayout(this);
    layout->setSpacing(8);

    layout->addWidget(new QLabel("Pay To:"), 0, 0);
    QLineEdit* addrEdit = new QLineEdit();
    addrEdit->setPlaceholderText("Enter a BHT address");
    addrEdit->setObjectName("address");
    layout->addWidget(addrEdit, 0, 1);
    QPushButton* addrBookBtn = new QPushButton("Addresses");
    connect(addrBookBtn, &QPushButton::clicked, this, &SendCoinsEntry::on_addressBookButton_clicked);
    layout->addWidget(addrBookBtn, 0, 2);

    layout->addWidget(new QLabel("Label:"), 1, 0);
    QLineEdit* labelEdit = new QLineEdit();
    labelEdit->setPlaceholderText("Enter a label");
    labelEdit->setObjectName("label");
    layout->addWidget(labelEdit, 1, 1);

    layout->addWidget(new QLabel("Amount:"), 2, 0);
    QLineEdit* amountEdit = new QLineEdit();
    amountEdit->setPlaceholderText("Amount in BHT");
    amountEdit->setObjectName("amount");
    layout->addWidget(amountEdit, 2, 1);
    QPushButton* maxBtn = new QPushButton("Max");
    connect(maxBtn, &QPushButton::clicked, this, &SendCoinsEntry::on_useAvailableBalance_clicked);
    layout->addWidget(maxBtn, 2, 2);

    QComboBox* assetCombo = new QComboBox();
    assetCombo->setObjectName("assetCombo");
    assetCombo->addItem("BHT");
    assetCombo->addItem("BHTE");
    connect(assetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SendCoinsEntry::handleAssetChanged);
    layout->addWidget(new QLabel("Asset:"), 3, 0);
    layout->addWidget(assetCombo, 3, 1);

    QPushButton* pasteBtn = new QPushButton("Paste");
    connect(pasteBtn, &QPushButton::clicked, this, &SendCoinsEntry::on_pasteButton_clicked);
    layout->addWidget(pasteBtn, 3, 2);

    QCheckBox* subFeeCheck = new QCheckBox("Subtract fee from amount");
    subFeeCheck->setObjectName("subFeeCheck");
    connect(subFeeCheck, &QCheckBox::stateChanged, this, &SendCoinsEntry::handleSubtractFeeChanged);
    layout->addWidget(subFeeCheck, 4, 1);

    QPushButton* deleteBtn = new QPushButton("Remove");
    deleteBtn->setStyleSheet("QPushButton { color: red; }");
    connect(deleteBtn, &QPushButton::clicked, this, &SendCoinsEntry::on_deleteButton_clicked);
    QHBoxLayout* delLayout = new QHBoxLayout();
    delLayout->addWidget(deleteBtn);
    delLayout->addStretch();
    layout->addLayout(delLayout, 5, 1, 1, 2);

    connect(addrEdit, &QLineEdit::textChanged, this, &SendCoinsEntry::handleAddressChanged);
    connect(amountEdit, &QLineEdit::textChanged, this, &SendCoinsEntry::handleAmountChanged);
    connect(labelEdit, &QLineEdit::textChanged, this, &SendCoinsEntry::updateLabel);
}

void SendCoinsEntry::setModel(WalletModel* model) { m_model = model; }

SendCoinsRecipient SendCoinsEntry::getValue()
{
    SendCoinsRecipient r;
    QLineEdit* a = findChild<QLineEdit*>("address");
    QLineEdit* m = findChild<QLineEdit*>("amount");
    QLineEdit* l = findChild<QLineEdit*>("label");
    QCheckBox* s = findChild<QCheckBox*>("subFeeCheck");
    if (a) r.address = a->text();
    if (l) r.label = l->text();
    if (m) { double v = m->text().toDouble(); r.amount = (int64_t)(v * SATOSHI); }
    if (s) r.subtract_fee = s->isChecked();
    if (QComboBox* c = findChild<QComboBox*>("assetCombo")) r.asset = c->currentText();
    return r;
}

void SendCoinsEntry::setValue(const SendCoinsRecipient& v)
{
    QLineEdit* a = findChild<QLineEdit*>("address");
    QLineEdit* m = findChild<QLineEdit*>("amount");
    QLineEdit* l = findChild<QLineEdit*>("label");
    if (a) a->setText(v.address);
    if (l) l->setText(v.label);
    if (m) m->setText(QString::number(v.amount / (double)SATOSHI, 'f', 8));
}

void SendCoinsEntry::setAddress(const QString& addr)
{
    QLineEdit* e = findChild<QLineEdit*>("address");
    if (e) e->setText(addr);
}

void SendCoinsEntry::handleAddressChanged(const QString& a)
{
    m_matching_address = false;
    if (!a.trimmed().isEmpty()) setValid(true);
}
void SendCoinsEntry::updateLabel()
{
    QLineEdit* a = findChild<QLineEdit*>("address");
    QLineEdit* l = findChild<QLineEdit*>("label");
    if (!a || !l) return;
    auto it = m_address_to_label.find(a->text());
    if (it != m_address_to_label.end()) l->setText(it.value());
}

void SendCoinsEntry::on_addressBookButton_clicked()
{
    if (!m_model) return;
    AddressBookPage dlg(AddressBookPage::ForSelection, AddressBookPage::SendingTab, this);
    dlg.setModel(m_model->getAddressTableModel());
    if (dlg.exec() == QDialog::Accepted) {
        setAddress(dlg.getReturnValue());
    }
}
void SendCoinsEntry::on_pasteButton_clicked()
{
    QString t = QApplication::clipboard()->text();
    if (t.startsWith("bht:")) { t = t.mid(4); int p = t.indexOf('?'); if (p > 0) t = t.left(p); }
    QLineEdit* e = findChild<QLineEdit*>("address");
    if (e && !t.isEmpty()) e->setText(t);
}
void SendCoinsEntry::on_deleteButton_clicked() { Q_EMIT removeEntry(this); }
void SendCoinsEntry::on_useAvailableBalance_clicked()
{
    QLineEdit* e = findChild<QLineEdit*>("amount");
    if (e && m_model) e->setText(QString::number(m_model->getBalance() / (double)SATOSHI, 'f', 8));
}

void SendCoinsEntry::setFocus()
{
    QLineEdit* e = findChild<QLineEdit*>("address");
    if (e) e->setFocus();
}
void SendCoinsEntry::clear()
{
    QLineEdit* a = findChild<QLineEdit*>("address");
    QLineEdit* m = findChild<QLineEdit*>("amount");
    QLineEdit* l = findChild<QLineEdit*>("label");
    if (a) a->clear();
    if (m) m->clear();
    if (l) l->clear();
}
void SendCoinsEntry::setValid(bool v)
{
    QLineEdit* e = findChild<QLineEdit*>("address");
    if (e) e->setStyleSheet(v ? "" : "border: 1px solid red;");
}
void SendCoinsEntry::setRemoveAction(RemoveAction a) { m_remove_action = a; }
bool SendCoinsEntry::validate()
{
    QLineEdit* e = findChild<QLineEdit*>("address");
    return e && !e->text().trimmed().isEmpty();
}
int64_t SendCoinsEntry::getAmountSatoshis() const
{
    const QLineEdit* e = findChild<QLineEdit*>("amount");
    if (!e) return 0;
    return (int64_t)(e->text().toDouble() * SATOSHI);
}
bool SendCoinsEntry::isClear() const
{
    const QLineEdit* a = findChild<QLineEdit*>("address");
    const QLineEdit* m = findChild<QLineEdit*>("amount");
    return (!a || a->text().isEmpty()) && (!m || m->text().isEmpty());
}
QWidget* SendCoinsEntry::setupTabChain(QWidget* prev) { Q_UNUSED(prev); return this; }
void SendCoinsEntry::handleAmountChanged() {}
void SendCoinsEntry::handleSubtractFeeChanged() {}
void SendCoinsEntry::handleAssetChanged(int index)
{
    QLineEdit* address = findChild<QLineEdit*>("address");
    QLineEdit* amount = findChild<QLineEdit*>("amount");
    if (index == 1) {
        if (address) address->setPlaceholderText("Enter a BHTE 0x account");
        if (amount) amount->setPlaceholderText("Amount in BHTE");
    } else {
        if (address) address->setPlaceholderText("Enter a BHT address");
        if (amount) amount->setPlaceholderText("Amount in BHT");
    }
}
void SendCoinsEntry::handleAddressEdited() {}
void SendCoinsEntry::handleAmountEdited() {}
void SendCoinsEntry::updateAssetLabel() {}
void SendCoinsEntry::updateAssetComboVisibility() {}
void SendCoinsEntry::updateRemoveEnabled() {}
QString SendCoinsEntry::currentAsset() const
{
    const QComboBox* c = findChild<QComboBox*>("assetCombo");
    return c ? c->currentText() : "BHT";
}
bool SendCoinsEntry::hasAcceptableInput() { return true; }
QString SendCoinsEntry::getAsset() const { return currentAsset(); }
bool SendCoinsEntry::isMature() const { return true; }

CoinControlDialog::CoinControlDialog(QWidget* parent) : QDialog(parent) { setupUi(); }
CoinControlDialog::~CoinControlDialog() {}

void CoinControlDialog::setupUi()
{
    setWindowTitle("Coin Control");
    setMinimumWidth(550);
    QVBoxLayout* l = new QVBoxLayout(this);
    QLabel* t = new QLabel("Coin Control");
    t->setStyleSheet("font-size: 18px; font-weight: bold;");
    l->addWidget(t);
    QLabel* info = new QLabel(
        "Coin Control allows you to select specific inputs\n"
        "when creating transactions. This gives you precise\n"
        "control over privacy and fees.\n\n"
        "Feature coming in next release.");
    info->setWordWrap(true); info->setStyleSheet("color: #666; padding: 20px;");
    l->addWidget(info);
    l->addStretch();
    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    l->addWidget(bb);
}

void CoinControlDialog::setModel(WalletModel* m) { m_model = m; }
void CoinControlDialog::updateView() {}
void CoinControlDialog::refreshLabels() {}
void CoinControlDialog::setBalanceBalance(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t) {}
