// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Receive Coins Dialog

#ifndef BHT_WALLET_QT_RECEIVECOINSDIALOG_H
#define BHT_WALLET_QT_RECEIVECOINSDIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtGui/QPixmap>
#include <QtCore/QString>

class WalletModel;
class OptionsModel;

class ReceiveCoinsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReceiveCoinsDialog(QWidget* parent = nullptr);
    ~ReceiveCoinsDialog();

    void setModel(WalletModel* model);

private:
    void setupUI();
    void updateDisplayUnit();
    void generateQRCode(const QString& address);
    void generateNewAddress();
    void copyAddress();
    void copyURI();
    void copyLabel();
    void clear();

    WalletModel* m_model{nullptr};

    QLabel* m_qr_code_label{nullptr};
    QLabel* m_address_label{nullptr};
    QLineEdit* m_address_edit{nullptr};
    QLineEdit* m_label_edit{nullptr};
    QLineEdit* m_amount_edit{nullptr};
    QLineEdit* m_message_edit{nullptr};
    QTextEdit* m_uri_edit{nullptr};
    QComboBox* m_asset_combo{nullptr};

    QPushButton* m_generate_button{nullptr};
    QPushButton* m_copy_address_button{nullptr};
    QPushButton* m_copy_uri_button{nullptr};
    QPushButton* m_clear_button{nullptr};
    QPushButton* m_close_button{nullptr};

    QString m_current_address;
    QString m_current_asset{"BHC"};
};

#endif
