// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Receive Coins Dialog Implementation

#include "receivecoinsdialog.h"
#include "walletmodel.h"
#include "optionsmodel.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>
#include <QtGui/QClipboard>
#include <QtCore/QDateTime>
#include <QtGui/QPainter>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

ReceiveCoinsDialog::ReceiveCoinsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
}

ReceiveCoinsDialog::~ReceiveCoinsDialog()
{
}

void ReceiveCoinsDialog::setupUI()
{
    setWindowTitle(tr("Request payment"));
    setMinimumWidth(600);
    setMinimumHeight(500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QHBoxLayout* contentLayout = new QHBoxLayout();

    QVBoxLayout* leftLayout = new QVBoxLayout();

    QGroupBox* requestGroup = new QGroupBox(tr("Request options"));
    QGridLayout* requestLayout = new QGridLayout(requestGroup);

    requestLayout->addWidget(new QLabel(tr("Asset:")), 0, 0);
    m_asset_combo = new QComboBox();
    m_asset_combo->addItem("BHT", "BHT");
    m_asset_combo->addItem("BHTE", "BHTE");
    requestLayout->addWidget(m_asset_combo, 0, 1);

    requestLayout->addWidget(new QLabel(tr("Label:")), 1, 0);
    m_label_edit = new QLineEdit();
    m_label_edit->setPlaceholderText(tr("Enter a label for this address"));
    requestLayout->addWidget(m_label_edit, 1, 1);

    requestLayout->addWidget(new QLabel(tr("Amount:")), 2, 0);
    m_amount_edit = new QLineEdit();
    m_amount_edit->setPlaceholderText(tr("Amount to request (optional)"));
    requestLayout->addWidget(m_amount_edit, 2, 1);

    requestLayout->addWidget(new QLabel(tr("Message:")), 3, 0);
    m_message_edit = new QLineEdit();
    m_message_edit->setPlaceholderText(tr("Message to include (optional)"));
    requestLayout->addWidget(m_message_edit, 3, 1);

    leftLayout->addWidget(requestGroup);

    QGroupBox* addressGroup = new QGroupBox(tr("Generated address"));
    QVBoxLayout* addressLayout = new QVBoxLayout(addressGroup);

    m_address_edit = new QLineEdit();
    m_address_edit->setReadOnly(true);
    m_address_edit->setStyleSheet("QLineEdit { background: #f5f5f5; padding: 8px; font-family: monospace; }");
    addressLayout->addWidget(m_address_edit);

    leftLayout->addWidget(addressGroup);

    QGroupBox* uriGroup = new QGroupBox(tr("Payment URI"));
    QVBoxLayout* uriLayout = new QVBoxLayout(uriGroup);

    m_uri_edit = new QTextEdit();
    m_uri_edit->setReadOnly(true);
    m_uri_edit->setMaximumHeight(80);
    m_uri_edit->setStyleSheet("QTextEdit { background: #f5f5f5; font-family: monospace; }");
    uriLayout->addWidget(m_uri_edit);

    leftLayout->addWidget(uriGroup);
    leftLayout->addStretch();

    contentLayout->addLayout(leftLayout, 2);

    QVBoxLayout* rightLayout = new QVBoxLayout();

    QGroupBox* qrGroup = new QGroupBox(tr("QR Code"));
    QVBoxLayout* qrLayout = new QVBoxLayout(qrGroup);
    qrLayout->setAlignment(Qt::AlignCenter);

    m_qr_code_label = new QLabel();
    m_qr_code_label->setMinimumSize(200, 200);
    m_qr_code_label->setMaximumSize(250, 250);
    m_qr_code_label->setAlignment(Qt::AlignCenter);
    m_qr_code_label->setStyleSheet("QLabel { background: white; border: 1px solid #ddd; }");
    qrLayout->addWidget(m_qr_code_label);

    m_address_label = new QLabel();
    m_address_label->setAlignment(Qt::AlignCenter);
    m_address_label->setStyleSheet("font-family: monospace; font-size: 10px;");
    m_address_label->setWordWrap(true);
    qrLayout->addWidget(m_address_label);

    rightLayout->addWidget(qrGroup);
    rightLayout->addStretch();

    contentLayout->addLayout(rightLayout, 1);
    mainLayout->addLayout(contentLayout);

    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_generate_button = new QPushButton(tr("&Generate New Address"));
    m_generate_button->setIcon(QIcon(":/icons/add"));
    m_generate_button->setStyleSheet(
        "QPushButton { background: #4CAF50; color: white; padding: 10px 20px; "
        "border-radius: 5px; } QPushButton:hover { background: #45a049; }");
    buttonLayout->addWidget(m_generate_button);

    m_copy_address_button = new QPushButton(tr("Copy &Address"));
    m_copy_address_button->setIcon(QIcon(":/icons/editcopy"));
    buttonLayout->addWidget(m_copy_address_button);

    m_copy_uri_button = new QPushButton(tr("Copy &URI"));
    m_copy_uri_button->setIcon(QIcon(":/icons/editcopy"));
    buttonLayout->addWidget(m_copy_uri_button);

    m_clear_button = new QPushButton(tr("C&lear"));
    buttonLayout->addWidget(m_clear_button);

    buttonLayout->addStretch();

    m_close_button = new QPushButton(tr("&Close"));
    buttonLayout->addWidget(m_close_button);

    mainLayout->addLayout(buttonLayout);

    connect(m_generate_button, &QPushButton::clicked, this, &ReceiveCoinsDialog::generateNewAddress);
    connect(m_copy_address_button, &QPushButton::clicked, this, &ReceiveCoinsDialog::copyAddress);
    connect(m_copy_uri_button, &QPushButton::clicked, this, &ReceiveCoinsDialog::copyURI);
    connect(m_clear_button, &QPushButton::clicked, this, &ReceiveCoinsDialog::clear);
    connect(m_close_button, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_asset_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]() {
        m_current_asset = m_asset_combo->currentData().toString();
        generateNewAddress();
    });

    generateNewAddress();
}

void ReceiveCoinsDialog::setModel(WalletModel* model)
{
    m_model = model;
    if (model && model->getOptionsModel()) {
        connect(model->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &ReceiveCoinsDialog::updateDisplayUnit);
    }
}

void ReceiveCoinsDialog::updateDisplayUnit()
{
}

void ReceiveCoinsDialog::generateQRCode(const QString& address)
{
    QString uri = QString("%1:%2").arg(m_current_asset == "BHTE" ? "bhte" : "bht", address);

    QUrlQuery query;
    if (!m_label_edit->text().isEmpty()) {
        query.addQueryItem("label", m_label_edit->text());
    }
    if (!m_amount_edit->text().isEmpty()) {
        query.addQueryItem("amount", m_amount_edit->text());
    }
    if (!m_message_edit->text().isEmpty()) {
        query.addQueryItem("message", m_message_edit->text());
    }

    QString queryString = query.toString();
    if (!queryString.isEmpty()) {
        uri += "?" + queryString;
    }

    m_uri_edit->setPlainText(uri);

    QPixmap qrPixmap(200, 200);
    qrPixmap.fill(Qt::white);
    QPainter painter(&qrPixmap);
    painter.setPen(Qt::black);
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);
    painter.drawText(QRect(10, 10, 180, 180), Qt::AlignCenter | Qt::TextWordWrap, address);
    painter.end();

    m_qr_code_label->setPixmap(qrPixmap.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_address_label->setText(address);
}

void ReceiveCoinsDialog::generateNewAddress()
{
    QString seed = QString::fromLatin1(QByteArray::number(QDateTime::currentMSecsSinceEpoch(), 16));
    QString dummyAddress;
    if (m_current_asset == "BHTE") {
        dummyAddress = "0x" + seed.leftJustified(40, '0').left(40);
    } else {
        dummyAddress = "bht1q" + seed.leftJustified(38, '0').left(38);
    }

    m_current_address = dummyAddress;
    m_address_edit->setText(m_current_address);
    generateQRCode(m_current_address);
}

void ReceiveCoinsDialog::copyAddress()
{
    if (!m_current_address.isEmpty()) {
        QApplication::clipboard()->setText(m_current_address);
        QMessageBox::information(this, tr("Copy Address"), tr("Address copied to clipboard"));
    }
}

void ReceiveCoinsDialog::copyURI()
{
    QString uri = m_uri_edit->toPlainText();
    if (!uri.isEmpty()) {
        QApplication::clipboard()->setText(uri);
        QMessageBox::information(this, tr("Copy URI"), tr("URI copied to clipboard"));
    }
}

void ReceiveCoinsDialog::copyLabel()
{
    QString label = m_label_edit->text();
    if (!label.isEmpty()) {
        QApplication::clipboard()->setText(label);
    }
}

void ReceiveCoinsDialog::clear()
{
    m_label_edit->clear();
    m_amount_edit->clear();
    m_message_edit->clear();
    generateNewAddress();
}
