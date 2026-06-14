// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Send Coins Dialog

#ifndef SENDCOINSDIALOG_H
#define SENDCOINSDIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QMenu>
#include <QtGui/QActionGroup>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QButtonGroup>
#include <QtGui/QAction>
#include <QtCore/QSettings>
#include <QtCore/QTimer>

#include <vector>
#include <string>
#include <map>

class WalletModel;
class SendCoinsEntry;
class CoinControlDialog;

struct SendCoinsRecipient {
    QString address;
    QString label;
    QString asset;
    int64_t amount;
    bool subtract_fee;

    SendCoinsRecipient() : amount(0), subtract_fee(false) {}
};

class SendCoinsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendCoinsDialog(QWidget* parent = nullptr);
    ~SendCoinsDialog();

    void setModel(WalletModel* model);

    void setAddress(const QString& address);
    void setAddress(const QString& address, const QString& label, const QString& asset, int64_t amount);

    void pasteEntry(const SendCoinsRecipient& recipient);
    void handlePaste(const SendCoinsRecipient& recipient);

    QString getReturnValue() const { return m_return_value; }

private:
    void setupUi();

    void accept() override;
    void reject() override;

    void setBalanceBalance(int64_t balance, int64_t unconfirmedBalance, int64_t immature,
                         int64_t watchOnlyBalance, int64_t watchUnconfBalance, int64_t watchImmature);

private Q_SLOTS:
    void on_sendButton_clicked();
    void on_clearButton_clicked();
    void on_addEntry_button_clicked();
    void removeEntry(SendCoinsEntry* entry);

    void updateDisplayUnit();
    void handleEntryChanged();
    void handleAddressChanged(const QString& address);
    void handleAmountChanged();
    void handleAssetChanged(int index);

    void updateFeeSection();
    void updateCoinControlVisibility();

private:
    WalletModel* m_model{nullptr};
    QString m_return_value;

    QVBoxLayout* m_main_layout{nullptr};

    QLabel* m_asset_label{nullptr};
    QComboBox* m_asset_combo{nullptr};

    SendCoinsEntry* m_entry{nullptr};

    QLabel* m_balance_label{nullptr};
    int64_t m_balance{0};
    int64_t m_unconfirmed_balance{0};
    int64_t m_immature_balance{0};
    int64_t m_watch_only_balance{0};
    int64_t m_watch_only_unconfirmed_balance{0};
    int64_t m_watch_only_immature_balance{0};

    QLabel* m_fee_limit_label{nullptr};

    int m_current_asset{0};

    QPushButton* m_send_button{nullptr};
    QPushButton* m_clear_button{nullptr};
    QPushButton* m_add_entry_button{nullptr};
};

class SendCoinsEntry : public QFrame
{
    Q_OBJECT

public:
    explicit SendCoinsEntry(QWidget* parent = nullptr);
    ~SendCoinsEntry();

    void setModel(WalletModel* model);

    SendCoinsRecipient getValue();
    void setValue(const SendCoinsRecipient& value);
    void setAddress(const QString& address);

    bool validate();
    int64_t getAmountSatoshis() const;
    QString getAsset() const;

    void setFocus();
    void clear();
    bool isClear() const;
    bool hasAcceptableInput();
    bool isMature() const;

    QWidget* setupTabChain(QWidget* previous);

    void setValid(bool valid);
    QString currentAsset() const;

    enum RemoveAction {
        REMOVE_STATE_DEFAULT,
        REMOVE_STATE_COUNT_DECREASE,
        REMOVE_STATE_COUNT_INCREASE
    };

    void setRemoveAction(RemoveAction action);
    RemoveAction getRemoveAction() const { return m_remove_action; }

Q_SIGNALS:
    void removeEntry(SendCoinsEntry* entry);
    void useAvailableBalance(SendCoinsEntry* entry);
    void displayUnitChanged(int unit);

private:
    void setupUi(QWidget* parent);

private Q_SLOTS:
    void handleAddressChanged(const QString& address);
    void handleAmountChanged();
    void handleAssetChanged(int index);
    void handleSubtractFeeChanged();
    void updateLabel();
    void updateAssetLabel();
    void updateAssetComboVisibility();
    void on_addressBookButton_clicked();
    void on_pasteButton_clicked();
    void on_deleteButton_clicked();
    void on_useAvailableBalance_clicked();
    void handleAddressEdited();
    void handleAmountEdited();
    void updateRemoveEnabled();

private:
    WalletModel* m_model{nullptr};
    RemoveAction m_remove_action{REMOVE_STATE_DEFAULT};
    bool m_matching_address{false};

    QMap<QString, QString> m_address_to_label;
    QMap<QString, QString> m_label_to_address;
};

class CoinControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CoinControlDialog(QWidget* parent = nullptr);
    ~CoinControlDialog();

    void setModel(WalletModel* model);
    void updateView();
    void refreshLabels();
    void setBalanceBalance(int64_t balance, int64_t unconfirmed, int64_t immature,
                         int64_t watchOnlyBalance, int64_t watchUnconf, int64_t watchImmature);

private:
    void setupUi();

    WalletModel* m_model{nullptr};
};

#endif
