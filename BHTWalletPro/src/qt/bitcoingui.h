// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Bitcoin Core Style GUI Header

#ifndef BHT_WALLET_QT_BITCOINGUI_H
#define BHT_WALLET_QT_BITCOINGUI_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtCore/QTimer>
#include <QtCore/QSettings>

#include "walletmodel.h"

#include <memory>
#include <vector>
#include <map>

class ClientModel;

class BHTWalletGUI : public QMainWindow
{
    Q_OBJECT

public:
    explicit BHTWalletGUI(QWidget* parent = nullptr);
    ~BHTWalletGUI();

    void setClientModel(ClientModel* clientModel);
    void setWalletModel(WalletModel* walletModel);
    bool handleURI(const QString& uri);

public Q_SLOTS:
    void createWallet();
    void openWallet();
    void closeWallet();
    void backupWallet();
    void sendCoins();
    void receiveCoins();
    void showOptions();
    void showAbout();
    void showDebugWindow();
    void showNetworkMonitor();
    void showPeerInfo();
    void showConsole();
    void signMessage();
    void verifyMessage();
    void lockWallet();
    void unlockWallet();
    void showCmdLineHelp();
    void updateWeight();
    void modalOverlaySetVisible(bool visible);
    void updateTransactionView();

private:
    void createActions();
    void createMenuBar();
    void createToolBars();
    void createStatusBar();
    void setWalletActionsEnabled(bool enabled);

    QWidget* createNavWidget();
    QWidget* createMainContentWidget();
    QWidget* createOverviewPage();
    QWidget* createTransactionsPage();
    QWidget* createAddressesPage();
    QWidget* createBHTEBridgePage();
    QWidget* createNodePage();
    QWidget* createSecurityPage();
    QWidget* createHardwarePage();
    QWidget* createDeveloperPage();
    QWidget* createSettingsPage();
    void createTrayIcon();

    QPushButton* addNavButton(QVBoxLayout* layout, const QString& text, const QString& icon, int pageIndex);
    QLabel* createSectionTitle(const QString& text);
    QFrame* createMetricCard(const QString& title, const QString& value, const QString& detail, QLabel** valueLabel = nullptr);
    QString text(const char* en, const char* zh) const;
    void rebuildWorkspace();
    void switchPage(int index);
    void appendConsoleLine(const QString& line);
    void applyRuntimeSettings();
    void updateChainStateViews(const WalletModel::ChainState& bht, const WalletModel::ChainState& bhte);

    void updateNumberOfConnections(int numberOfConnections);
    void updateNumberOfBlocks(int numberOfBlocks, const QDateTime& blockDate, double verificationProgress, bool header);
    void updateProxyIcon(bool networkEnabled);
    void updateSslIcon(int state);
    void updatePrivacyIndicator();

    QString getElapsedTimeSince(qint64 nTime);

    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    QAction* m_openWalletAction{nullptr};
    QAction* m_createWalletAction{nullptr};
    QAction* m_closeWalletAction{nullptr};
    QAction* m_backupWalletAction{nullptr};
    QAction* m_sendCoinsAction{nullptr};
    QAction* m_receiveCoinsAction{nullptr};
    QAction* m_showOptionsAction{nullptr};
    QAction* m_showDebugWindowAction{nullptr};
    QAction* m_showNetworkMonitorAction{nullptr};
    QAction* m_showPeerInfoAction{nullptr};
    QAction* m_showConsoleAction{nullptr};
    QAction* m_signMessageAction{nullptr};
    QAction* m_verifyMessageAction{nullptr};
    QAction* m_openBHTAction{nullptr};
    QAction* m_exportWalletAction{nullptr};
    QAction* m_showAboutAction{nullptr};

    QMenuBar* m_appMenuBar{nullptr};
    QMenu* m_fileMenu{nullptr};
    QMenu* m_walletMenu{nullptr};
    QMenu* m_sendMenu{nullptr};
    QMenu* m_receiveMenu{nullptr};
    QMenu* m_optionsMenu{nullptr};
    QMenu* m_toolsMenu{nullptr};
    QMenu* m_helpMenu{nullptr};

    QToolBar* m_toolbar{nullptr};

    QTimer* m_timerProceessingInterval{nullptr};

    QWidget* m_mainWindowContent{nullptr};
    QVBoxLayout* m_mainLayout{nullptr};
    QWidget* m_headerWidget{nullptr};
    QWidget* m_footerWidget{nullptr};

    QLabel* m_totalBalance{nullptr};
    QLabel* m_watchOnlyBalance{nullptr};
    QStackedWidget* m_pageStack{nullptr};
    QWidget* m_contentWidget{nullptr};
    QButtonGroup* m_navButtonGroup{nullptr};
    QPlainTextEdit* m_consoleOutput{nullptr};
    QTableWidget* m_nodeStatusTable{nullptr};
    QLabel* m_bhtBalanceValue{nullptr};
    QLabel* m_bhteBalanceValue{nullptr};
    QLabel* m_bhtStatusValue{nullptr};
    QLabel* m_bhteStatusValue{nullptr};

    QLabel* m_connectionControl{nullptr};
    QLabel* m_peerCountLbl{nullptr};
    QLabel* m_headerLbl{nullptr};
    QProgressBar* m_progressBar{nullptr};
    QLabel* m_progressLabel{nullptr};

    QLabel* m_encryptionStatusIcon{nullptr};
    QLabel* m_stakingStatusIcon{nullptr};

    QSystemTrayIcon* m_trayIcon{nullptr};
    QMenu* m_trayMenu{nullptr};

    ClientModel* m_clientModel{nullptr};
    WalletModel* m_walletModel{nullptr};

    int m_language{0};
    bool m_developerMode{false};
};

#endif
