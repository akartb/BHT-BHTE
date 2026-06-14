// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Address Book Page

#ifndef BHT_WALLET_QT_ADDRESSBOOKPAGE_H
#define BHT_WALLET_QT_ADDRESSBOOKPAGE_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QTableView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtCore/QString>
#include <QtCore/QSortFilterProxyModel>

class WalletModel;
class AddressTableModel;
class AddressBookSortFilterProxyModel;

class AddressBookPage : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        ForSelection,
        ForEditing
    };

    enum Tabs {
        SendingTab,
        ReceivingTab
    };

    explicit AddressBookPage(Mode mode, Tabs tab, QWidget* parent = nullptr);
    ~AddressBookPage();

    void setModel(AddressTableModel* model);

    void done(int retval) override;

    QString getReturnValue() const { return m_return_value; }

private:
    void setupUI();
    void updateWindowTitle();
    void selectionChanged();

    void on_newAddress_clicked();
    void on_copyAddress_clicked();
    void on_deleteAddress_clicked();
    void on_exportButton_clicked();

    void onCopyLabelAction();
    void onEditAction();

    void contextualMenu(const QPoint& point);
    void selectNewAddress(const QModelIndex& parent, int begin, int end);

    Mode m_mode;
    Tabs m_tab;
    AddressTableModel* m_model{nullptr};
    AddressBookSortFilterProxyModel* m_proxy_model{nullptr};

    QString m_return_value;
    QString m_new_address_to_select;

    QTableView* m_table_view{nullptr};
    QLineEdit* m_search_line{nullptr};
    QPushButton* m_new_address_btn{nullptr};
    QPushButton* m_copy_address_btn{nullptr};
    QPushButton* m_delete_address_btn{nullptr};
    QPushButton* m_export_btn{nullptr};
    QPushButton* m_close_btn{nullptr};
    QLabel* m_explanation_label{nullptr};

    QMenu* m_context_menu{nullptr};
};

class AddressBookSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit AddressBookSortFilterProxyModel(const QString& type, QObject* parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_type;
};

#endif
