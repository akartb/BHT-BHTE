// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Bitcoin Core Style GUI Implementation

#include "bitcoingui.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "overviewpage.h"
#include "sendcoinsdialog.h"
#include "receivecoinsdialog.h"
#include "transactionview.h"
#include "addressbookpage.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QMenu>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtCore/QSettings>
#include <QtCore/QTimer>
#include <QtGui/QFont>
#include <QtGui/QKeySequence>
#include <QtGui/QPixmap>
#include <QtGui/QCloseEvent>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtCore/QMimeData>
#include <QtCore/QUrl>

BHTWalletGUI::BHTWalletGUI(QWidget* parent)
    : QMainWindow(parent)
{
    QSettings settings;
    m_language = settings.value("language", "en").toString() == "zh_CN" ? 1 : 0;
    m_developerMode = settings.value("developerMode", false).toBool();

    setWindowTitle(tr("BHT Wallet Pro"));
    setWindowIcon(QIcon(":/icons/bht"));

    createActions();
    createMenuBar();
    createToolBars();
    createStatusBar();

    rebuildWorkspace();

    createTrayIcon();

    m_timerProceessingInterval = new QTimer(this);
    connect(m_timerProceessingInterval, &QTimer::timeout, this, &BHTWalletGUI::updateTransactionView);
    m_timerProceessingInterval->start(5000);
}

BHTWalletGUI::~BHTWalletGUI()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
}

void BHTWalletGUI::createActions()
{
    m_createWalletAction = new QAction(QIcon(":/icons/add"), tr("&Create Wallet..."), this);
    m_createWalletAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    m_createWalletAction->setStatusTip(tr("Create a new wallet"));

    m_openWalletAction = new QAction(QIcon(":/icons/add"), tr("&Open Wallet..."), this);
    m_openWalletAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    m_openWalletAction->setStatusTip(tr("Open an existing wallet"));

    m_closeWalletAction = new QAction(QIcon(":/icons/remove"), tr("&Close Wallet..."), this);
    m_closeWalletAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
    m_closeWalletAction->setStatusTip(tr("Close the current wallet"));

    m_backupWalletAction = new QAction(QIcon(":/icons/export"), tr("&Backup Wallet..."), this);
    m_backupWalletAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    m_backupWalletAction->setStatusTip(tr("Backup the current wallet"));

    m_sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("&Send"), this);
    m_sendCoinsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    m_sendCoinsAction->setStatusTip(tr("Send BHC to a BHT address"));

    m_receiveCoinsAction = new QAction(QIcon(":/icons/receive"), tr("&Receive"), this);
    m_receiveCoinsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    m_receiveCoinsAction->setStatusTip(tr("Request payments to a BHT address"));

    m_showOptionsAction = new QAction(QIcon(":/icons/edit"), tr("&Options..."), this);
    m_showOptionsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    m_showOptionsAction->setStatusTip(tr("Modify settings and options"));

    m_showDebugWindowAction = new QAction(QIcon(":/icons/edit"), tr("&Debug Window"), this);
    m_showDebugWindowAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    m_showDebugWindowAction->setStatusTip(tr("Open debugging and diagnostic console"));

    m_showNetworkMonitorAction = new QAction(tr("&Network Monitor"), this);
    m_showNetworkMonitorAction->setStatusTip(tr("Show network monitor"));

    m_showPeerInfoAction = new QAction(tr("&Peer Information"), this);
    m_showPeerInfoAction->setStatusTip(tr("Show peer information"));

    m_showConsoleAction = new QAction(tr("&Console"), this);
    m_showConsoleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    m_showConsoleAction->setStatusTip(tr("Open console"));

    m_signMessageAction = new QAction(QIcon(":/icons/edit"), tr("Sign &Message..."), this);
    m_signMessageAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    m_signMessageAction->setStatusTip(tr("Sign messages to prove you own a BHT address"));

    m_verifyMessageAction = new QAction(QIcon(":/icons/verify"), tr("&Verify Message..."), this);
    m_verifyMessageAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    m_verifyMessageAction->setStatusTip(tr("Verify messages to prove you own a BHT address"));

    m_showAboutAction = new QAction(tr("&About BHT Wallet"), this);
    m_showAboutAction->setStatusTip(tr("Show information about BHT Wallet"));

    m_openBHTAction = new QAction(QIcon(":/icons/bht"), tr("Open BHT &website"), this);
    m_openBHTAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_H));
    m_openBHTAction->setStatusTip(tr("Open the BHT website"));

    m_exportWalletAction = new QAction(tr("&Export Wallet Data..."), this);
    m_exportWalletAction->setStatusTip(tr("Export all wallet data for backup"));

    connect(m_createWalletAction, &QAction::triggered, this, &BHTWalletGUI::createWallet);
    connect(m_openWalletAction, &QAction::triggered, this, &BHTWalletGUI::openWallet);
    connect(m_closeWalletAction, &QAction::triggered, this, &BHTWalletGUI::closeWallet);
    connect(m_backupWalletAction, &QAction::triggered, this, &BHTWalletGUI::backupWallet);
    connect(m_sendCoinsAction, &QAction::triggered, this, &BHTWalletGUI::sendCoins);
    connect(m_receiveCoinsAction, &QAction::triggered, this, &BHTWalletGUI::receiveCoins);
    connect(m_showOptionsAction, &QAction::triggered, this, &BHTWalletGUI::showOptions);
    connect(m_showDebugWindowAction, &QAction::triggered, this, &BHTWalletGUI::showDebugWindow);
    connect(m_showConsoleAction, &QAction::triggered, this, &BHTWalletGUI::showConsole);
    connect(m_signMessageAction, &QAction::triggered, this, &BHTWalletGUI::signMessage);
    connect(m_verifyMessageAction, &QAction::triggered, this, &BHTWalletGUI::verifyMessage);
    connect(m_showAboutAction, &QAction::triggered, this, &BHTWalletGUI::showAbout);
}

void BHTWalletGUI::createMenuBar()
{
    m_appMenuBar = menuBar();

    m_fileMenu = m_appMenuBar->addMenu(tr("&File"));
    m_fileMenu->addAction(m_createWalletAction);
    m_fileMenu->addAction(m_openWalletAction);
    m_fileMenu->addAction(m_closeWalletAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_backupWalletAction);
    m_fileMenu->addAction(m_exportWalletAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_showOptionsAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(tr("&Quit"), qApp, &QApplication::quit, QKeySequence(Qt::CTRL | Qt::Key_Q));

    m_walletMenu = m_appMenuBar->addMenu(tr("&Wallet"));
    m_walletMenu->addAction(m_sendCoinsAction);
    m_walletMenu->addAction(m_receiveCoinsAction);
    m_walletMenu->addSeparator();
    m_walletMenu->addAction(m_signMessageAction);
    m_walletMenu->addAction(m_verifyMessageAction);
    m_walletMenu->addSeparator();

    QMenu* coinJoinMenu = m_walletMenu->addMenu(tr("&CoinJoin"));
    coinJoinMenu->setIcon(QIcon(":/icons/transaction0"));

    m_walletMenu->addSeparator();
    m_walletMenu->addAction(tr("&Lock Wallet"), this, &BHTWalletGUI::lockWallet, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
    m_walletMenu->addAction(tr("&Unlock Wallet..."), this, &BHTWalletGUI::unlockWallet, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_U));

    m_sendMenu = m_walletMenu->addMenu(tr("&Send"));
    m_sendMenu->addAction(m_sendCoinsAction);

    m_receiveMenu = m_walletMenu->addMenu(tr("&Receive"));
    m_receiveMenu->addAction(m_receiveCoinsAction);

    m_optionsMenu = m_appMenuBar->addMenu(tr("&Settings"));
    m_optionsMenu->addAction(m_showOptionsAction);

    m_toolsMenu = m_appMenuBar->addMenu(tr("&Tools"));
    m_toolsMenu->addAction(m_showDebugWindowAction);
    m_toolsMenu->addAction(m_showNetworkMonitorAction);
    m_toolsMenu->addAction(m_showPeerInfoAction);
    m_toolsMenu->addAction(m_showConsoleAction);
    m_toolsMenu->addSeparator();
    m_toolsMenu->addAction(tr("&Wallet Repair"));

    m_helpMenu = m_appMenuBar->addMenu(tr("&Help"));
    m_helpMenu->addAction(m_openBHTAction);
    m_helpMenu->addSeparator();
    m_helpMenu->addAction(m_showAboutAction);
    m_helpMenu->addAction(tr("&Command-line Options"), this, &BHTWalletGUI::showCmdLineHelp);
}

void BHTWalletGUI::createToolBars()
{
    m_toolbar = addToolBar(tr("Main Toolbar"));
    m_toolbar->setMovable(false);
    m_toolbar->setObjectName("mainToolBar");
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    m_toolbar->addAction(m_sendCoinsAction);
    m_toolbar->addAction(m_receiveCoinsAction);
    m_toolbar->addSeparator();
    m_toolbar->addAction(m_showOptionsAction);
    m_toolbar->addAction(m_showDebugWindowAction);
}

void BHTWalletGUI::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));

    m_connectionControl = new QLabel();
    m_connectionControl->setToolTip(tr("Network activity and connection status"));
    statusBar()->addWidget(m_connectionControl);

    m_peerCountLbl = new QLabel();
    m_peerCountLbl->setToolTip(tr("Number of connected peers"));
    statusBar()->addWidget(m_peerCountLbl);

    m_headerLbl = new QLabel();
    statusBar()->addPermanentWidget(m_headerLbl);

    m_progressBar = new QProgressBar();
    m_progressBar->setStyleSheet("QProgressBar { min-width: 200px; }");
    m_progressBar->setMaximumHeight(16);
    m_progressBar->setVisible(false);
    statusBar()->addPermanentWidget(m_progressBar);

    m_progressLabel = new QLabel();
    m_progressLabel->setVisible(false);
    statusBar()->addPermanentWidget(m_progressLabel);

    m_encryptionStatusIcon = new QLabel();
    statusBar()->addPermanentWidget(m_encryptionStatusIcon);

    m_stakingStatusIcon = new QLabel();
    statusBar()->addPermanentWidget(m_stakingStatusIcon);
}

void BHTWalletGUI::setWalletActionsEnabled(bool enabled)
{
    m_sendCoinsAction->setEnabled(enabled);
    m_receiveCoinsAction->setEnabled(enabled);
    m_signMessageAction->setEnabled(enabled);
    m_verifyMessageAction->setEnabled(enabled);
    m_backupWalletAction->setEnabled(enabled);
    m_exportWalletAction->setEnabled(enabled);
    m_closeWalletAction->setEnabled(enabled);
}

void BHTWalletGUI::createWallet()
{
    QString name = QInputDialog::getText(this, tr("Create Wallet"), tr("Wallet Name:"));
    if (name.isEmpty()) return;

    QMessageBox::information(this, tr("Create Wallet"),
        tr("Wallet '%1' will be created.\n\n"
           "This is a demo implementation.").arg(name));
}

void BHTWalletGUI::openWallet()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Open Wallet"), QDir::homePath(), tr("Wallet Files (*.dat)"));
    if (file.isEmpty()) return;

    QMessageBox::information(this, tr("Open Wallet"),
        tr("Opening wallet: %1\n\n"
           "This is a demo implementation.").arg(file));
}

void BHTWalletGUI::closeWallet()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Close Wallet"),
        tr("Are you sure you want to close the current wallet?"),
        QMessageBox::Yes | QMessageBox::Cancel);

    if (reply == QMessageBox::Yes) {
        QMessageBox::information(this, tr("Close Wallet"),
            tr("Wallet closed.\n\n"
               "This is a demo implementation."));
    }
}

void BHTWalletGUI::backupWallet()
{
    QString file = QFileDialog::getSaveFileName(this, tr("Backup Wallet"),
        QDir::homePath() + "/bht-wallet-backup.dat", tr("Wallet Files (*.dat)"));

    if (file.isEmpty()) return;

    QMessageBox::information(this, tr("Backup Wallet"),
        tr("Wallet backed up to: %1\n\n"
           "This is a demo implementation.").arg(file));
}

void BHTWalletGUI::sendCoins()
{
    SendCoinsDialog dlg(this);
    dlg.setModel(m_walletModel);
    dlg.exec();
}

void BHTWalletGUI::receiveCoins()
{
    ReceiveCoinsDialog dlg(this);
    dlg.setModel(m_walletModel);
    dlg.exec();
}

void BHTWalletGUI::showOptions()
{
    QDialog dialog(this);
    dialog.setWindowTitle(text("Options", "选项"));
    dialog.setMinimumWidth(460);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QFormLayout* form = new QFormLayout();
    QComboBox* languageCombo = new QComboBox(&dialog);
    languageCombo->addItem("English", 0);
    languageCombo->addItem(QString::fromUtf8("简体中文"), 1);
    languageCombo->setCurrentIndex(m_language);
    form->addRow(text("Language:", "语言："), languageCombo);

    QComboBox* networkCombo = new QComboBox(&dialog);
    networkCombo->addItems(QStringList() << "Mainnet" << "Testnet" << "Signet" << "Regtest");
    form->addRow(text("BHT network:", "BHT 网络："), networkCombo);

    QSettings currentSettings;
    QLineEdit* bhtRpc = new QLineEdit(currentSettings.value("bhtRpcEndpoint", "http://127.0.0.1:18332").toString(), &dialog);
    form->addRow(text("BHT RPC endpoint:", "BHT RPC 节点："), bhtRpc);

    QLineEdit* bhtUser = new QLineEdit(currentSettings.value("bhtRpcUser").toString(), &dialog);
    form->addRow(text("BHT RPC user:", "BHT RPC 用户："), bhtUser);

    QLineEdit* bhtPassword = new QLineEdit(currentSettings.value("bhtRpcPassword").toString(), &dialog);
    bhtPassword->setEchoMode(QLineEdit::Password);
    form->addRow(text("BHT RPC password:", "BHT RPC 密码："), bhtPassword);

    QLineEdit* bhteRpc = new QLineEdit(currentSettings.value("bhteRpcEndpoint", "http://127.0.0.1:8545").toString(), &dialog);
    form->addRow(text("BHTE RPC endpoint:", "BHTE RPC 节点："), bhteRpc);

    QLineEdit* bhteAccount = new QLineEdit(currentSettings.value("bhteDefaultAccount").toString(), &dialog);
    form->addRow(text("BHTE account:", "BHTE 账户："), bhteAccount);

    QCheckBox* developerCheck = new QCheckBox(text("Enable developer mode", "启用开发者模式"), &dialog);
    developerCheck->setChecked(m_developerMode);
    form->addRow(QString(), developerCheck);

    layout->addLayout(form);
    layout->addStretch();

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_language = languageCombo->currentData().toInt();
    m_developerMode = developerCheck->isChecked();

    QSettings settings;
    settings.setValue("language", m_language == 1 ? "zh_CN" : "en");
    settings.setValue("developerMode", m_developerMode);
    settings.setValue("bhtRpcEndpoint", bhtRpc->text().trimmed());
    settings.setValue("bhtRpcUser", bhtUser->text());
    settings.setValue("bhtRpcPassword", bhtPassword->text());
    settings.setValue("bhteRpcEndpoint", bhteRpc->text().trimmed());
    settings.setValue("bhteDefaultAccount", bhteAccount->text().trimmed());

    if (m_walletModel) {
        m_walletModel->setRpcEndpoints(bhtRpc->text().trimmed(), bhteRpc->text().trimmed());
        m_walletModel->refreshChainStates();
    }

    rebuildWorkspace();
    statusBar()->showMessage(text("Settings updated", "设置已更新"), 3000);
}

void BHTWalletGUI::showDebugWindow()
{
    if (!m_developerMode) {
        m_developerMode = true;
        QSettings().setValue("developerMode", true);
        rebuildWorkspace();
    }
    switchPage(m_pageStack ? m_pageStack->count() - 2 : 0);
    appendConsoleLine(text("Debug window opened.", "调试窗口已打开。"));
}

void BHTWalletGUI::showConsole()
{
    showDebugWindow();
}

void BHTWalletGUI::signMessage()
{
    QString address = QInputDialog::getText(this, tr("Sign Message"), tr("BHT Address:"));
    if (address.isEmpty()) return;

    QString message = QInputDialog::getMultiLineText(this, tr("Sign Message"), tr("Message:"));
    if (message.isEmpty()) return;

    QMessageBox::information(this, tr("Sign Message"),
        tr("Message signed.\n\n"
           "Address: %1\n\n"
           "This is a demo implementation.").arg(address));
}

void BHTWalletGUI::verifyMessage()
{
    QString address = QInputDialog::getText(this, tr("Verify Message"), tr("BHT Address:"));
    if (address.isEmpty()) return;

    QString signature = QInputDialog::getMultiLineText(this, tr("Verify Message"), tr("Signature:"));
    if (signature.isEmpty()) return;

    QString message = QInputDialog::getMultiLineText(this, tr("Verify Message"), tr("Message:"));
    if (message.isEmpty()) return;

    QMessageBox::information(this, tr("Verify Message"),
        tr("Message verified.\n\n"
           "This is a demo implementation."));
}

void BHTWalletGUI::showAbout()
{
    QMessageBox::about(this, tr("About BHT Wallet Pro"),
        tr("<b>BHT Wallet Pro</b><br><br>"
           "Version: 1.0.0<br><br>"
           "A professional wallet for BHT and BHTE.<br><br>"
           "Features:<br>"
           "- ML-DSA Post-Quantum Signatures<br>"
           "- Layer 2 BHTE Integration<br>"
           "- Hardware Wallet Support (HID)<br>"
           "- Bitcoin Core Style Interface<br><br>"
           "Based on Bitcoin Core architecture."));
}

void BHTWalletGUI::showNetworkMonitor()
{
    QMessageBox::information(this, tr("Network Monitor"),
        tr("Network monitor would show here.\n\n"
           "This is a demo implementation."));
}

void BHTWalletGUI::showPeerInfo()
{
    QMessageBox::information(this, tr("Peer Information"),
        tr("Peer information would show here.\n\n"
           "This is a demo implementation."));
}

void BHTWalletGUI::lockWallet()
{
    QMessageBox::information(this, tr("Lock Wallet"),
        tr("Wallet locked.\n\n"
           "This is a demo implementation."));
}

void BHTWalletGUI::unlockWallet()
{
    QString passphrase = QInputDialog::getText(this, tr("Unlock Wallet"),
        tr("Passphrase:"), QLineEdit::Password);

    if (passphrase.isEmpty()) return;

    QMessageBox::information(this, tr("Unlock Wallet"),
        tr("Wallet unlocked.\n\n"
           "This is a demo implementation."));
}

void BHTWalletGUI::showCmdLineHelp()
{
    QMessageBox::information(this, tr("Command-line Options"),
        tr("Command-line options would show here.\n\n"
           "This is a demo implementation."));
}

void BHTWalletGUI::updateTransactionView()
{
}

QWidget* BHTWalletGUI::createNavWidget()
{
    QWidget* nav = new QWidget(this);
    nav->setMinimumWidth(210);
    nav->setMaximumWidth(240);
    nav->setStyleSheet(
        "QWidget { background: #f4f6f8; border-right: 1px solid #d8dde3; }"
        "QPushButton { text-align: left; padding: 9px 10px; border: 0; border-radius: 4px; color: #202428; }"
        "QPushButton:hover { background: #e7ebef; }"
        "QPushButton:checked { background: #d9e8ff; color: #0a4c93; font-weight: 600; }");
    QVBoxLayout* navLayout = new QVBoxLayout(nav);
    navLayout->setContentsMargins(8, 12, 8, 12);
    navLayout->setSpacing(4);

    m_navButtonGroup = new QButtonGroup(nav);
    m_navButtonGroup->setExclusive(true);

    addNavButton(navLayout, text("Overview", "概览"), ":/icons/overview", 0);
    addNavButton(navLayout, text("Transactions", "交易"), ":/icons/history", 1);
    addNavButton(navLayout, text("Addresses", "地址簿"), ":/icons/address-book", 2);
    addNavButton(navLayout, text("BHTE Bridge", "BHTE 跨链桥"), ":/icons/synced", 3);
    addNavButton(navLayout, text("BHT/BHTE Nodes", "BHT/BHTE 节点"), ":/icons/connect4", 4);
    addNavButton(navLayout, text("Security", "安全"), ":/icons/lock_closed", 5);
    addNavButton(navLayout, text("Hardware Wallet", "硬件钱包"), ":/icons/hd_enabled", 6);

    navLayout->addStretch();

    QFrame* separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    navLayout->addWidget(separator);

    if (m_developerMode) {
        addNavButton(navLayout, text("Developer", "开发者"), ":/icons/edit", 7);
        addNavButton(navLayout, text("Settings", "设置"), ":/icons/proxy", 8);
    } else {
        addNavButton(navLayout, text("Settings", "设置"), ":/icons/proxy", 7);
    }

    if (QAbstractButton* first = m_navButtonGroup->button(0)) {
        first->setChecked(true);
    }

    return nav;
}

QWidget* BHTWalletGUI::createMainContentWidget()
{
    m_pageStack = new QStackedWidget(this);
    m_pageStack->addWidget(createOverviewPage());
    m_pageStack->addWidget(createTransactionsPage());
    m_pageStack->addWidget(createAddressesPage());
    m_pageStack->addWidget(createBHTEBridgePage());
    m_pageStack->addWidget(createNodePage());
    m_pageStack->addWidget(createSecurityPage());
    m_pageStack->addWidget(createHardwarePage());
    if (m_developerMode) {
        m_pageStack->addWidget(createDeveloperPage());
    }
    m_pageStack->addWidget(createSettingsPage());
    return m_pageStack;
}

QPushButton* BHTWalletGUI::addNavButton(QVBoxLayout* layout, const QString& text, const QString& icon, int pageIndex)
{
    QPushButton* button = new QPushButton(text, this);
    button->setIcon(QIcon(icon));
    button->setCheckable(true);
    button->setMinimumHeight(38);
    m_navButtonGroup->addButton(button, pageIndex);
    layout->addWidget(button);
    connect(button, &QPushButton::clicked, this, [this, pageIndex]() { switchPage(pageIndex); });
    return button;
}

QLabel* BHTWalletGUI::createSectionTitle(const QString& text)
{
    QLabel* label = new QLabel(text, this);
    label->setStyleSheet("font-size: 18px; font-weight: 700; color: #202428;");
    return label;
}

QFrame* BHTWalletGUI::createMetricCard(const QString& title, const QString& value, const QString& detail, QLabel** valueLabelOut)
{
    QFrame* frame = new QFrame(this);
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setStyleSheet("QFrame { background: #ffffff; border: 1px solid #dfe4ea; border-radius: 6px; }");
    QVBoxLayout* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(12, 10, 12, 10);
    QLabel* titleLabel = new QLabel(title, frame);
    titleLabel->setStyleSheet("font-size: 12px; color: #68717a;");
    QLabel* valueWidget = new QLabel(value, frame);
    valueWidget->setStyleSheet("font-size: 22px; font-weight: 700; color: #1f2933;");
    if (valueLabelOut) {
        *valueLabelOut = valueWidget;
    }
    QLabel* detailLabel = new QLabel(detail, frame);
    detailLabel->setStyleSheet("font-size: 12px; color: #68717a;");
    detailLabel->setWordWrap(true);
    layout->addWidget(titleLabel);
    layout->addWidget(valueWidget);
    layout->addWidget(detailLabel);
    return frame;
}

QString BHTWalletGUI::text(const char* en, const char* zh) const
{
    return m_language == 1 ? QString::fromUtf8(zh) : QString::fromUtf8(en);
}

QWidget* BHTWalletGUI::createOverviewPage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(14);

    QLabel* title = createSectionTitle(text("Portfolio Overview", "资产概览"));
    layout->addWidget(title);

    QGridLayout* cards = new QGridLayout();
    cards->setSpacing(12);
    cards->addWidget(createMetricCard("BHT", "0.00000000", text("Layer 1 settlement balance", "一层结算余额"), &m_bhtBalanceValue), 0, 0);
    cards->addWidget(createMetricCard("BHTE", "0.00000000", text("Layer 2 spendable balance", "二层可用余额"), &m_bhteBalanceValue), 0, 1);
    cards->addWidget(createMetricCard(text("Locked in Bridge", "跨链锁定"), "0.00000000", text("Pending deposit and withdrawal liquidity", "存入与提现排队资金")), 0, 2);
    cards->addWidget(createMetricCard(text("Quantum Mode", "量子安全模式"), "Hybrid", text("ECDSA + ML-DSA signature policy", "ECDSA + ML-DSA 混合签名策略")), 0, 3);
    layout->addLayout(cards);

    QGroupBox* actions = new QGroupBox(text("Wallet Actions", "钱包操作"), page);
    QHBoxLayout* actionLayout = new QHBoxLayout(actions);
    QPushButton* send = new QPushButton(QIcon(":/icons/send"), text("Send", "发送"), actions);
    QPushButton* receive = new QPushButton(QIcon(":/icons/receive"), text("Receive", "接收"), actions);
    QPushButton* bridge = new QPushButton(QIcon(":/icons/synced"), text("Bridge BHT <-> BHTE", "BHT <-> BHTE 跨链"), actions);
    QPushButton* backup = new QPushButton(QIcon(":/icons/export"), text("Backup Wallet", "备份钱包"), actions);
    connect(send, &QPushButton::clicked, this, &BHTWalletGUI::sendCoins);
    connect(receive, &QPushButton::clicked, this, &BHTWalletGUI::receiveCoins);
    connect(bridge, &QPushButton::clicked, this, [this]() { switchPage(3); });
    connect(backup, &QPushButton::clicked, this, &BHTWalletGUI::backupWallet);
    actionLayout->addWidget(send);
    actionLayout->addWidget(receive);
    actionLayout->addWidget(bridge);
    actionLayout->addWidget(backup);
    actionLayout->addStretch();
    layout->addWidget(actions);

    QGroupBox* chain = new QGroupBox(text("Chain Health", "链状态"), page);
    QGridLayout* chainLayout = new QGridLayout(chain);
    m_bhtStatusValue = new QLabel(text("Disconnected", "未连接"), chain);
    m_bhteStatusValue = new QLabel(text("Disconnected", "未连接"), chain);
    chainLayout->addWidget(new QLabel("BHT"), 0, 0);
    chainLayout->addWidget(m_bhtStatusValue, 0, 1);
    chainLayout->addWidget(new QLabel("RPC 127.0.0.1:18332"), 0, 2);
    chainLayout->addWidget(new QLabel("BHTE"), 1, 0);
    chainLayout->addWidget(m_bhteStatusValue, 1, 1);
    chainLayout->addWidget(new QLabel("RPC 127.0.0.1:8545"), 1, 2);
    layout->addWidget(chain);

    QTableWidget* recent = new QTableWidget(4, 5, page);
    recent->setHorizontalHeaderLabels(QStringList()
        << text("Time", "时间")
        << text("Chain", "链")
        << text("Type", "类型")
        << text("Amount", "金额")
        << "Status");
    recent->horizontalHeader()->setStretchLastSection(true);
    recent->verticalHeader()->hide();
    recent->setEditTriggers(QAbstractItemView::NoEditTriggers);
    const QStringList rows = QStringList()
        << text("No confirmed transactions yet", "暂无已确认交易")
        << text("Bridge queue empty", "跨链队列为空")
        << text("No pending withdrawals", "暂无待提现")
        << text("No hardware signing requests", "暂无硬件签名请求");
    for (int i = 0; i < rows.size(); ++i) {
        recent->setItem(i, 0, new QTableWidgetItem("-"));
        recent->setItem(i, 1, new QTableWidgetItem(i == 1 ? "BHTE" : "BHT"));
        recent->setItem(i, 2, new QTableWidgetItem(rows.at(i)));
        recent->setItem(i, 3, new QTableWidgetItem("0.00000000"));
        recent->setItem(i, 4, new QTableWidgetItem(text("Idle", "空闲")));
    }
    layout->addWidget(createSectionTitle(text("Recent Activity", "近期活动")));
    layout->addWidget(recent, 1);

    return page;
}

QWidget* BHTWalletGUI::createTransactionsPage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->addWidget(createSectionTitle(text("Transactions", "交易")));

    QHBoxLayout* filters = new QHBoxLayout();
    filters->addWidget(new QLabel(text("Chain:", "链："), page));
    QComboBox* chainFilter = new QComboBox(page);
    chainFilter->addItems(QStringList() << text("All", "全部") << "BHT" << "BHTE");
    filters->addWidget(chainFilter);
    filters->addWidget(new QLabel(text("Search:", "搜索："), page));
    filters->addWidget(new QLineEdit(page), 1);
    QPushButton* exportButton = new QPushButton(QIcon(":/icons/export"), text("Export CSV", "导出 CSV"), page);
    filters->addWidget(exportButton);
    layout->addLayout(filters);

    QTableWidget* table = new QTableWidget(5, 7, page);
    table->setHorizontalHeaderLabels(QStringList()
        << text("Date", "日期")
        << text("Chain", "链")
        << text("Type", "类型")
        << text("Address", "地址")
        << text("Amount", "金额")
        << text("Fee", "手续费")
        << text("Status", "状态"));
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (int i = 0; i < table->rowCount(); ++i) {
        table->setItem(i, 0, new QTableWidgetItem("-"));
        table->setItem(i, 1, new QTableWidgetItem(i % 2 == 0 ? "BHT" : "BHTE"));
        table->setItem(i, 2, new QTableWidgetItem(text("Waiting for wallet data", "等待钱包数据")));
        table->setItem(i, 3, new QTableWidgetItem("-"));
        table->setItem(i, 4, new QTableWidgetItem("0.00000000"));
        table->setItem(i, 5, new QTableWidgetItem("0.00000000"));
        table->setItem(i, 6, new QTableWidgetItem(text("Local", "本地")));
    }
    layout->addWidget(table, 1);

    return page;
}

QWidget* BHTWalletGUI::createAddressesPage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->addWidget(createSectionTitle(text("Address Book", "地址簿")));

    QHBoxLayout* toolbar = new QHBoxLayout();
    QPushButton* receive = new QPushButton(QIcon(":/icons/receive"), text("New Receiving Address", "新收款地址"), page);
    QPushButton* sending = new QPushButton(QIcon(":/icons/address-book"), text("Manage Sending Addresses", "管理付款地址"), page);
    QPushButton* copy = new QPushButton(QIcon(":/icons/editcopy"), text("Copy", "复制"), page);
    connect(receive, &QPushButton::clicked, this, &BHTWalletGUI::receiveCoins);
    connect(sending, &QPushButton::clicked, this, [this]() {
        AddressBookPage dlg(AddressBookPage::ForEditing, AddressBookPage::SendingTab, this);
        if (m_walletModel) dlg.setModel(m_walletModel->getAddressTableModel());
        dlg.exec();
    });
    toolbar->addWidget(receive);
    toolbar->addWidget(sending);
    toolbar->addWidget(copy);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    QTableWidget* table = new QTableWidget(4, 5, page);
    table->setHorizontalHeaderLabels(QStringList()
        << text("Label", "标签")
        << text("Address", "地址")
        << text("Chain", "链")
        << text("Purpose", "用途")
        << text("Watch-only", "仅观察"));
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
    table->setItem(0, 0, new QTableWidgetItem(text("Main BHT receive", "BHT 主收款")));
    table->setItem(0, 1, new QTableWidgetItem("bht1qdemo1234567890abcdefghijklmn"));
    table->setItem(0, 2, new QTableWidgetItem("BHT"));
    table->setItem(0, 3, new QTableWidgetItem(text("Receive", "收款")));
    table->setItem(0, 4, new QTableWidgetItem(text("No", "否")));
    table->setItem(1, 0, new QTableWidgetItem(text("BHTE account", "BHTE 账户")));
    table->setItem(1, 1, new QTableWidgetItem("0x0000000000000000000000000000000000000100"));
    table->setItem(1, 2, new QTableWidgetItem("BHTE"));
    table->setItem(1, 3, new QTableWidgetItem(text("L2", "二层")));
    table->setItem(1, 4, new QTableWidgetItem(text("No", "否")));
    layout->addWidget(table, 1);

    return page;
}

QWidget* BHTWalletGUI::createBHTEBridgePage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->addWidget(createSectionTitle(text("BHTE Bridge", "BHTE 跨链桥")));

    QTabWidget* tabs = new QTabWidget(page);
    QWidget* depositTab = new QWidget(tabs);
    QFormLayout* depositForm = new QFormLayout(depositTab);
    QLineEdit* depositFrom = new QLineEdit(depositTab);
    QLineEdit* depositTo = new QLineEdit(depositTab);
    QLineEdit* depositAmount = new QLineEdit(depositTab);
    QLineEdit* depositTxid = new QLineEdit(depositTab);
    QLineEdit* depositProof = new QLineEdit(depositTab);
    depositForm->addRow(text("From BHT address:", "从 BHT 地址："), depositFrom);
    depositForm->addRow(text("To BHTE account:", "到 BHTE 账户："), depositTo);
    depositForm->addRow(text("Amount:", "金额："), depositAmount);
    depositForm->addRow(text("BHT deposit txid:", "BHT 存入交易："), depositTxid);
    depositForm->addRow(text("BHT merkle proof:", "BHT 默克尔证明："), depositProof);
    depositForm->addRow(text("Confirmations required:", "需要确认数："), new QLabel("12", depositTab));
    QPushButton* depositButton = new QPushButton(text("Create Deposit Transaction", "创建存入交易"), depositTab);
    QPushButton* verifyDepositButton = new QPushButton(text("Verify Deposit Proof", "验证存入证明"), depositTab);
    depositForm->addRow(QString(), depositButton);
    depositForm->addRow(QString(), verifyDepositButton);

    QWidget* withdrawTab = new QWidget(tabs);
    QFormLayout* withdrawForm = new QFormLayout(withdrawTab);
    QLineEdit* withdrawFrom = new QLineEdit(withdrawTab);
    QLineEdit* withdrawTo = new QLineEdit(withdrawTab);
    QLineEdit* withdrawAmount = new QLineEdit(withdrawTab);
    QLineEdit* accountProof = new QLineEdit(withdrawTab);
    withdrawForm->addRow(text("From BHTE account:", "从 BHTE 账户："), withdrawFrom);
    withdrawForm->addRow(text("To BHT address:", "到 BHT 地址："), withdrawTo);
    withdrawForm->addRow(text("Amount:", "金额："), withdrawAmount);
    withdrawForm->addRow(text("BHTE account proof:", "BHTE 账户证明："), accountProof);
    withdrawForm->addRow(text("Challenge period:", "挑战期："), new QLabel(text("7 days", "7 天"), withdrawTab));
    QPushButton* withdrawButton = new QPushButton(text("Request Withdrawal", "发起提现"), withdrawTab);
    QPushButton* verifyAccountButton = new QPushButton(text("Verify Account Proof", "验证账户证明"), withdrawTab);
    withdrawForm->addRow(QString(), withdrawButton);
    withdrawForm->addRow(QString(), verifyAccountButton);

    QWidget* statusTab = new QWidget(tabs);
    QVBoxLayout* statusLayout = new QVBoxLayout(statusTab);
    statusLayout->addWidget(createMetricCard(text("Bridge State", "桥状态"), text("Offline", "离线"), text("Waiting for BHT and BHTE RPC connections", "等待连接 BHT 和 BHTE RPC")));
    QTableWidget* queue = new QTableWidget(3, 5, statusTab);
    queue->setHorizontalHeaderLabels(QStringList() << "ID" << text("Direction", "方向") << text("Amount", "金额") << text("Confirmations", "确认") << text("Status", "状态"));
    queue->horizontalHeader()->setStretchLastSection(true);
    queue->verticalHeader()->hide();
    statusLayout->addWidget(queue);

    tabs->addTab(depositTab, text("Deposit", "存入"));
    tabs->addTab(withdrawTab, text("Withdraw", "提现"));
    tabs->addTab(statusTab, text("Bridge Queue", "跨链队列"));
    layout->addWidget(tabs, 1);

    connect(depositButton, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, text("BHTE Bridge", "BHTE 跨链桥"), text("Set the bridge lock address in settings, then use the Send page or raw transaction builder to fund it. Proof verification is available below.", "请在设置中配置桥锁定地址，然后通过发送页或原始交易构建器转入；下方已可验证存入证明。"));
    });
    connect(verifyDepositButton, &QPushButton::clicked, this, [this, depositTxid, depositProof]() {
        if (!m_walletModel) return;
        WalletModel::BridgeVerification result = m_walletModel->verifyBHTDepositProof(depositTxid->text(), depositProof->text(), 12);
        QMessageBox::information(this, text("BHT Deposit Proof", "BHT 存入证明"),
            QString("%1\n%2: %3\n%4: %5")
                .arg(result.valid ? text("Valid", "有效") : text("Invalid", "无效"))
                .arg(text("Confirmations", "确认数")).arg(result.confirmations)
                .arg(text("Message", "消息")).arg(result.message));
    });
    connect(withdrawButton, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, text("BHTE Bridge", "BHTE 跨链桥"), text("Withdrawal request building now requires the settlement contract ABI/address and sequencer RPC. Account proof verification is available below.", "提现请求还需要结算合约 ABI/地址和排序器 RPC；下方已可验证账户证明。"));
    });
    connect(verifyAccountButton, &QPushButton::clicked, this, [this, withdrawFrom, accountProof]() {
        if (!m_walletModel) return;
        WalletModel::BridgeVerification result = m_walletModel->verifyBHTEAccountProof(withdrawFrom->text(), accountProof->text());
        QMessageBox::information(this, text("BHTE Account Proof", "BHTE 账户证明"),
            QString("%1\n%2: %3\n%4: %5")
                .arg(result.valid ? text("Valid", "有效") : text("Invalid", "无效"))
                .arg(text("Height", "高度")).arg(result.height)
                .arg(text("Message", "消息")).arg(result.message));
    });

    return page;
}

QWidget* BHTWalletGUI::createNodePage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->addWidget(createSectionTitle(text("BHT / BHTE Nodes", "BHT / BHTE 节点")));

    QTableWidget* table = new QTableWidget(2, 8, page);
    m_nodeStatusTable = table;
    table->setHorizontalHeaderLabels(QStringList()
        << text("Chain", "链")
        << text("Mode", "模式")
        << text("RPC", "RPC")
        << text("P2P", "P2P")
        << text("Height", "高度")
        << text("Peers", "节点")
        << text("Sync", "同步")
        << text("Status", "状态"));
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setItem(0, 0, new QTableWidgetItem("BHT"));
    table->setItem(0, 1, new QTableWidgetItem(text("Full node / SPV", "全节点 / SPV")));
    table->setItem(0, 2, new QTableWidgetItem("18332"));
    table->setItem(0, 3, new QTableWidgetItem("18334"));
    table->setItem(0, 4, new QTableWidgetItem("0"));
    table->setItem(0, 5, new QTableWidgetItem("0"));
    table->setItem(0, 6, new QTableWidgetItem("0%"));
    table->setItem(0, 7, new QTableWidgetItem(text("Disconnected", "未连接")));
    table->setItem(1, 0, new QTableWidgetItem("BHTE"));
    table->setItem(1, 1, new QTableWidgetItem(text("Layer 2 RPC", "二层 RPC")));
    table->setItem(1, 2, new QTableWidgetItem("8545"));
    table->setItem(1, 3, new QTableWidgetItem("8546"));
    table->setItem(1, 4, new QTableWidgetItem("0"));
    table->setItem(1, 5, new QTableWidgetItem("0"));
    table->setItem(1, 6, new QTableWidgetItem("0%"));
    table->setItem(1, 7, new QTableWidgetItem(text("Disconnected", "未连接")));
    layout->addWidget(table);

    QGroupBox* controls = new QGroupBox(text("Node Controls", "节点控制"), page);
    QHBoxLayout* controlLayout = new QHBoxLayout(controls);
    QPushButton* startBht = new QPushButton(text("Start BHT", "启动 BHT"), controls);
    QPushButton* startBhte = new QPushButton(text("Start BHTE", "启动 BHTE"), controls);
    QPushButton* stopBht = new QPushButton(text("Stop BHT", "停止 BHT"), controls);
    QPushButton* stopBhte = new QPushButton(text("Stop BHTE", "停止 BHTE"), controls);
    QPushButton* rescan = new QPushButton(text("Refresh RPC", "刷新 RPC"), controls);
    QPushButton* openData = new QPushButton(text("Open Data Folder", "打开数据目录"), controls);
    connect(startBht, &QPushButton::clicked, this, [this]() {
        QString error;
        if (m_walletModel && m_walletModel->startManagedNode("BHT", &error)) {
            statusBar()->showMessage(text("BHT node started", "BHT 节点已启动"), 4000);
        } else {
            QMessageBox::warning(this, text("BHT Node", "BHT 节点"), error);
        }
    });
    connect(startBhte, &QPushButton::clicked, this, [this]() {
        QString error;
        if (m_walletModel && m_walletModel->startManagedNode("BHTE", &error)) {
            statusBar()->showMessage(text("BHTE node started", "BHTE 节点已启动"), 4000);
        } else {
            QMessageBox::warning(this, text("BHTE Node", "BHTE 节点"), error);
        }
    });
    connect(stopBht, &QPushButton::clicked, this, [this]() {
        QString error;
        if (m_walletModel && m_walletModel->stopManagedNode("BHT", &error)) {
            statusBar()->showMessage(text("BHT node stopped", "BHT 节点已停止"), 4000);
        } else {
            QMessageBox::warning(this, text("BHT Node", "BHT 节点"), error);
        }
    });
    connect(stopBhte, &QPushButton::clicked, this, [this]() {
        QString error;
        if (m_walletModel && m_walletModel->stopManagedNode("BHTE", &error)) {
            statusBar()->showMessage(text("BHTE node stopped", "BHTE 节点已停止"), 4000);
        } else {
            QMessageBox::warning(this, text("BHTE Node", "BHTE 节点"), error);
        }
    });
    connect(rescan, &QPushButton::clicked, this, [this]() {
        if (m_walletModel && m_walletModel->refreshChainStates()) {
            statusBar()->showMessage(text("RPC state refreshed", "RPC 状态已刷新"), 3000);
        } else {
            statusBar()->showMessage(text("RPC refresh finished with disconnected chains", "RPC 刷新完成，但仍有链未连接"), 4000);
        }
    });
    connect(openData, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, text("Data Folder", "数据目录"), m_walletModel ? m_walletModel->getDataDir() : QString());
    });
    controlLayout->addWidget(startBht);
    controlLayout->addWidget(startBhte);
    controlLayout->addWidget(stopBht);
    controlLayout->addWidget(stopBhte);
    controlLayout->addWidget(rescan);
    controlLayout->addWidget(openData);
    controlLayout->addStretch();
    layout->addWidget(controls);

    return page;
}

QWidget* BHTWalletGUI::createSecurityPage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->addWidget(createSectionTitle(text("Security", "安全")));

    QGroupBox* wallet = new QGroupBox(text("Wallet Protection", "钱包保护"), page);
    QFormLayout* form = new QFormLayout(wallet);
    form->addRow(text("Encryption:", "加密："), new QLabel(text("Enabled-ready", "可启用"), wallet));
    form->addRow(text("HD wallet:", "HD 钱包："), new QLabel(text("Enabled", "已启用"), wallet));
    form->addRow(text("Signature policy:", "签名策略："), new QLabel("Hybrid (ECDSA + ML-DSA)", wallet));
    form->addRow(text("Watch-only:", "仅观察："), new QLabel(text("Supported", "支持"), wallet));
    layout->addWidget(wallet);

    QGroupBox* actions = new QGroupBox(text("Security Actions", "安全操作"), page);
    QHBoxLayout* actionLayout = new QHBoxLayout(actions);
    QPushButton* lock = new QPushButton(QIcon(":/icons/lock_closed"), text("Lock", "锁定"), actions);
    QPushButton* unlock = new QPushButton(QIcon(":/icons/lock_open"), text("Unlock", "解锁"), actions);
    QPushButton* sign = new QPushButton(QIcon(":/icons/edit"), text("Sign Message", "签名消息"), actions);
    QPushButton* verify = new QPushButton(QIcon(":/icons/verify"), text("Verify Message", "验证消息"), actions);
    connect(lock, &QPushButton::clicked, this, &BHTWalletGUI::lockWallet);
    connect(unlock, &QPushButton::clicked, this, &BHTWalletGUI::unlockWallet);
    connect(sign, &QPushButton::clicked, this, &BHTWalletGUI::signMessage);
    connect(verify, &QPushButton::clicked, this, &BHTWalletGUI::verifyMessage);
    actionLayout->addWidget(lock);
    actionLayout->addWidget(unlock);
    actionLayout->addWidget(sign);
    actionLayout->addWidget(verify);
    actionLayout->addStretch();
    layout->addWidget(actions);
    layout->addStretch();
    return page;
}

QWidget* BHTWalletGUI::createHardwarePage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->addWidget(createSectionTitle(text("Hardware Wallet", "硬件钱包")));

    QLabel* intro = new QLabel(text("Ledger, Trezor, BitBox02, Keystone, Coldcard and Jade device hooks are available in the wallet backend.", "钱包后端已预留 Ledger、Trezor、BitBox02、Keystone、Coldcard 和 Jade 设备接口。"), page);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    QTableWidget* devices = new QTableWidget(3, 5, page);
    devices->setHorizontalHeaderLabels(QStringList() << text("Device", "设备") << text("Type", "类型") << text("Path", "路径") << text("Firmware", "固件") << text("Status", "状态"));
    devices->horizontalHeader()->setStretchLastSection(true);
    devices->verticalHeader()->hide();
    devices->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(devices, 1);

    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(new QPushButton(text("Scan Devices", "扫描设备"), page));
    buttons->addWidget(new QPushButton(text("Display Address", "在设备上显示地址"), page));
    buttons->addWidget(new QPushButton(text("Sign Transaction", "签名交易"), page));
    buttons->addStretch();
    layout->addLayout(buttons);
    return page;
}

QWidget* BHTWalletGUI::createDeveloperPage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->addWidget(createSectionTitle(text("Developer Console", "开发者控制台")));

    QTabWidget* tabs = new QTabWidget(page);
    QWidget* consoleTab = new QWidget(tabs);
    QVBoxLayout* consoleLayout = new QVBoxLayout(consoleTab);
    m_consoleOutput = new QPlainTextEdit(consoleTab);
    m_consoleOutput->setReadOnly(true);
    m_consoleOutput->setPlainText(text("Developer mode enabled.\nRPC console is ready.\n", "开发者模式已启用。\nRPC 控制台已就绪。\n"));
    QHBoxLayout* inputLayout = new QHBoxLayout();
    QComboBox* chain = new QComboBox(consoleTab);
    chain->addItems(QStringList() << "BHT" << "BHTE");
    QLineEdit* command = new QLineEdit(consoleTab);
    command->setPlaceholderText(text("RPC command, for example getblockcount", "RPC 命令，例如 getblockcount"));
    QPushButton* run = new QPushButton(text("Run", "运行"), consoleTab);
    inputLayout->addWidget(chain);
    inputLayout->addWidget(command, 1);
    inputLayout->addWidget(run);
    consoleLayout->addWidget(m_consoleOutput, 1);
    consoleLayout->addLayout(inputLayout);
    connect(run, &QPushButton::clicked, this, [this, chain, command]() {
        appendConsoleLine(QString("[%1] > %2").arg(chain->currentText(), command->text()));
        appendConsoleLine(text("RPC transport is not connected yet.", "RPC 传输尚未连接。"));
    });

    QWidget* diagnosticsTab = new QWidget(tabs);
    QVBoxLayout* diagLayout = new QVBoxLayout(diagnosticsTab);
    QTableWidget* diag = new QTableWidget(6, 2, diagnosticsTab);
    diag->setHorizontalHeaderLabels(QStringList() << text("Metric", "指标") << text("Value", "值"));
    diag->horizontalHeader()->setStretchLastSection(true);
    diag->verticalHeader()->hide();
    const QStringList metrics = QStringList() << "BHT RPC" << "BHTE RPC" << "ML-DSA" << "Bridge" << "Hardware HID" << "Build";
    for (int i = 0; i < metrics.size(); ++i) {
        diag->setItem(i, 0, new QTableWidgetItem(metrics.at(i)));
        diag->setItem(i, 1, new QTableWidgetItem(i == 2 ? "Hybrid enabled" : text("Not connected", "未连接")));
    }
    diagLayout->addWidget(diag);

    tabs->addTab(consoleTab, text("RPC Console", "RPC 控制台"));
    tabs->addTab(diagnosticsTab, text("Diagnostics", "诊断"));
    layout->addWidget(tabs, 1);
    return page;
}

QWidget* BHTWalletGUI::createSettingsPage()
{
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->addWidget(createSectionTitle(text("Settings", "设置")));

    QGroupBox* general = new QGroupBox(text("General", "通用"), page);
    QFormLayout* form = new QFormLayout(general);
    QComboBox* language = new QComboBox(general);
    language->addItem("English", 0);
    language->addItem(QString::fromUtf8("简体中文"), 1);
    language->setCurrentIndex(m_language);
    QCheckBox* developer = new QCheckBox(text("Enable developer mode", "启用开发者模式"), general);
    developer->setChecked(m_developerMode);
    form->addRow(text("Language:", "语言："), language);
    form->addRow(QString(), developer);
    layout->addWidget(general);

    QGroupBox* endpoints = new QGroupBox(text("Chain Endpoints", "链端点"), page);
    QFormLayout* endpointForm = new QFormLayout(endpoints);
    QSettings currentSettings;
    QLineEdit* bhtRpc = new QLineEdit(currentSettings.value("bhtRpcEndpoint", "http://127.0.0.1:18332").toString(), endpoints);
    QLineEdit* bhtUser = new QLineEdit(currentSettings.value("bhtRpcUser").toString(), endpoints);
    QLineEdit* bhtPassword = new QLineEdit(currentSettings.value("bhtRpcPassword").toString(), endpoints);
    QLineEdit* bhteRpc = new QLineEdit(currentSettings.value("bhteRpcEndpoint", "http://127.0.0.1:8545").toString(), endpoints);
    QLineEdit* bhteAccount = new QLineEdit(currentSettings.value("bhteDefaultAccount").toString(), endpoints);
    QLineEdit* bhteWs = new QLineEdit(currentSettings.value("bhteWsEndpoint", "ws://127.0.0.1:8546").toString(), endpoints);
    QLineEdit* bhteHistoryMethod = new QLineEdit(currentSettings.value("bhteHistoryMethod", "bhte_getTransactionsByAddress").toString(), endpoints);
    bhtPassword->setEchoMode(QLineEdit::Password);
    endpointForm->addRow(text("BHT RPC:", "BHT RPC："), bhtRpc);
    endpointForm->addRow(text("BHT RPC user:", "BHT RPC 用户："), bhtUser);
    endpointForm->addRow(text("BHT RPC password:", "BHT RPC 密码："), bhtPassword);
    endpointForm->addRow(text("BHTE RPC:", "BHTE RPC："), bhteRpc);
    endpointForm->addRow(text("BHTE default account:", "BHTE 默认账户："), bhteAccount);
    endpointForm->addRow(text("BHTE WS:", "BHTE WS："), bhteWs);
    endpointForm->addRow(text("BHTE history method:", "BHTE 历史方法："), bhteHistoryMethod);
    layout->addWidget(endpoints);

    QGroupBox* nodePaths = new QGroupBox(text("Managed Node Processes", "托管节点进程"), page);
    QFormLayout* nodeForm = new QFormLayout(nodePaths);
    QLineEdit* bhtNodePath = new QLineEdit(currentSettings.value("bhtNodePath").toString(), nodePaths);
    QLineEdit* bhtDataDir = new QLineEdit(currentSettings.value("bhtDataDir", m_walletModel ? m_walletModel->getDataDir() : QString()).toString(), nodePaths);
    QLineEdit* bhtNodeArgs = new QLineEdit(currentSettings.value("bhtNodeArgs").toString(), nodePaths);
    QLineEdit* bhteNodePath = new QLineEdit(currentSettings.value("bhteNodePath").toString(), nodePaths);
    QLineEdit* bhteDataDir = new QLineEdit(currentSettings.value("bhteDataDir", m_walletModel ? m_walletModel->getDataDir() : QString()).toString(), nodePaths);
    QLineEdit* bhteNodeArgs = new QLineEdit(currentSettings.value("bhteNodeArgs").toString(), nodePaths);
    nodeForm->addRow(text("BHT executable:", "BHT 可执行文件："), bhtNodePath);
    nodeForm->addRow(text("BHT data dir:", "BHT 数据目录："), bhtDataDir);
    nodeForm->addRow(text("BHT arguments:", "BHT 启动参数："), bhtNodeArgs);
    nodeForm->addRow(text("BHTE executable:", "BHTE 可执行文件："), bhteNodePath);
    nodeForm->addRow(text("BHTE data dir:", "BHTE 数据目录："), bhteDataDir);
    nodeForm->addRow(text("BHTE arguments:", "BHTE 启动参数："), bhteNodeArgs);
    layout->addWidget(nodePaths);

    QPushButton* save = new QPushButton(text("Apply Settings", "应用设置"), page);
    connect(save, &QPushButton::clicked, this, [this, language, developer, bhtRpc, bhtUser, bhtPassword, bhteRpc, bhteAccount, bhteWs, bhteHistoryMethod, bhtNodePath, bhtDataDir, bhtNodeArgs, bhteNodePath, bhteDataDir, bhteNodeArgs]() {
        m_language = language->currentData().toInt();
        m_developerMode = developer->isChecked();
        QSettings settings;
        settings.setValue("language", m_language == 1 ? "zh_CN" : "en");
        settings.setValue("developerMode", m_developerMode);
        settings.setValue("bhtRpcEndpoint", bhtRpc->text().trimmed());
        settings.setValue("bhtRpcUser", bhtUser->text());
        settings.setValue("bhtRpcPassword", bhtPassword->text());
        settings.setValue("bhteRpcEndpoint", bhteRpc->text().trimmed());
        settings.setValue("bhteDefaultAccount", bhteAccount->text().trimmed());
        settings.setValue("bhteWsEndpoint", bhteWs->text().trimmed());
        settings.setValue("bhteHistoryMethod", bhteHistoryMethod->text().trimmed());
        settings.setValue("bhtNodePath", bhtNodePath->text().trimmed());
        settings.setValue("bhtDataDir", bhtDataDir->text().trimmed());
        settings.setValue("bhtNodeArgs", bhtNodeArgs->text().trimmed());
        settings.setValue("bhteNodePath", bhteNodePath->text().trimmed());
        settings.setValue("bhteDataDir", bhteDataDir->text().trimmed());
        settings.setValue("bhteNodeArgs", bhteNodeArgs->text().trimmed());
        if (m_walletModel) {
            m_walletModel->setRpcEndpoints(bhtRpc->text().trimmed(), bhteRpc->text().trimmed());
            m_walletModel->refreshChainStates();
        }
        rebuildWorkspace();
    });
    layout->addWidget(save);
    layout->addStretch();
    return page;
}

void BHTWalletGUI::rebuildWorkspace()
{
    m_mainWindowContent = new QWidget(this);
    m_mainLayout = new QVBoxLayout(m_mainWindowContent);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_headerWidget = new QWidget(m_mainWindowContent);
    m_headerWidget->setStyleSheet("background: #ffffff; border-bottom: 1px solid #d8dde3;");
    QHBoxLayout* headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(16, 10, 16, 10);
    QLabel* titleLabel = new QLabel(text("BHT Wallet Pro", "BHT 钱包 Pro"), m_headerWidget);
    titleLabel->setStyleSheet("font-size: 22px; font-weight: 700; color: #202428;");
    QLabel* subtitle = new QLabel(text("Bitcoin-style wallet for BHT and BHTE", "面向 BHT 与 BHTE 的 Bitcoin 风格钱包"), m_headerWidget);
    subtitle->setStyleSheet("color: #68717a;");
    QVBoxLayout* titleLayout = new QVBoxLayout();
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(subtitle);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    headerLayout->addWidget(new QLabel(m_developerMode ? text("Developer mode", "开发者模式") : text("Standard mode", "标准模式"), m_headerWidget));
    m_mainLayout->addWidget(m_headerWidget);

    QWidget* body = new QWidget(m_mainWindowContent);
    QHBoxLayout* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(createNavWidget());
    bodyLayout->addWidget(createMainContentWidget(), 1);
    m_mainLayout->addWidget(body, 1);

    QWidget* old = centralWidget();
    setCentralWidget(m_mainWindowContent);
    if (old) {
        old->deleteLater();
    }
    applyRuntimeSettings();
    if (m_walletModel) {
        updateChainStateViews(m_walletModel->getBHTState(), m_walletModel->getBHTEState());
    }
}

void BHTWalletGUI::switchPage(int index)
{
    if (!m_pageStack || index < 0 || index >= m_pageStack->count()) {
        return;
    }
    m_pageStack->setCurrentIndex(index);
    if (m_navButtonGroup) {
        if (QAbstractButton* button = m_navButtonGroup->button(index)) {
            button->setChecked(true);
        }
    }
}

void BHTWalletGUI::appendConsoleLine(const QString& line)
{
    if (m_consoleOutput) {
        m_consoleOutput->appendPlainText(line);
    }
}

void BHTWalletGUI::applyRuntimeSettings()
{
    setWindowTitle(text("BHT Wallet Pro", "BHT 钱包 Pro"));
}

void BHTWalletGUI::updateChainStateViews(const WalletModel::ChainState& bht, const WalletModel::ChainState& bhte)
{
    auto formatAmount = [](CAmount amount) {
        return QString::number(amount / 100000000.0, 'f', 8);
    };
    auto statusText = [this](const WalletModel::ChainState& state) {
        if (state.connected) {
            return text("Connected", "已连接");
        }
        return state.lastError.isEmpty() ? text("Disconnected", "未连接") : state.lastError;
    };
    auto setItem = [](QTableWidget* table, int row, int column, const QString& value) {
        if (!table) {
            return;
        }
        QTableWidgetItem* item = table->item(row, column);
        if (!item) {
            item = new QTableWidgetItem();
            table->setItem(row, column, item);
        }
        item->setText(value);
    };
    auto paintStatus = [this, &statusText](QLabel* label, const WalletModel::ChainState& state) {
        if (!label) {
            return;
        }
        label->setText(statusText(state));
        label->setToolTip(state.lastError);
        label->setStyleSheet(state.connected ? "color: #0f7b3f; font-weight: 700;" : "color: #a33a2f; font-weight: 700;");
    };

    if (m_bhtBalanceValue) {
        m_bhtBalanceValue->setText(formatAmount(bht.balance));
    }
    if (m_bhteBalanceValue) {
        m_bhteBalanceValue->setText(formatAmount(bhte.balance));
    }
    paintStatus(m_bhtStatusValue, bht);
    paintStatus(m_bhteStatusValue, bhte);

    if (m_nodeStatusTable) {
        setItem(m_nodeStatusTable, 0, 2, bht.endpoint);
        setItem(m_nodeStatusTable, 0, 4, QString::number(bht.height));
        setItem(m_nodeStatusTable, 0, 5, QString::number(bht.peers));
        setItem(m_nodeStatusTable, 0, 6, bht.connected ? text("RPC ready", "RPC 就绪") : text("Waiting", "等待中"));
        setItem(m_nodeStatusTable, 0, 7, statusText(bht));
        m_nodeStatusTable->item(0, 7)->setToolTip(bht.lastError);

        setItem(m_nodeStatusTable, 1, 2, bhte.endpoint);
        setItem(m_nodeStatusTable, 1, 4, QString::number(bhte.height));
        setItem(m_nodeStatusTable, 1, 5, QString::number(bhte.peers));
        setItem(m_nodeStatusTable, 1, 6, bhte.connected ? text("RPC ready", "RPC 就绪") : text("Waiting", "等待中"));
        setItem(m_nodeStatusTable, 1, 7, statusText(bhte));
        m_nodeStatusTable->item(1, 7)->setToolTip(bhte.lastError);
    }
}

void BHTWalletGUI::createTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/icons/bht"));

    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction(m_sendCoinsAction);
    m_trayMenu->addAction(m_receiveCoinsAction);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_showOptionsAction);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(tr("E&xit"), qApp, &QApplication::quit, QKeySequence(Qt::CTRL | Qt::Key_Q));

    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated,
        [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::DoubleClick) {
                showNormal();
                activateWindow();
            }
        });

    m_trayIcon->show();
}

void BHTWalletGUI::setClientModel(ClientModel* clientModel)
{
    m_clientModel = clientModel;
}

void BHTWalletGUI::setWalletModel(WalletModel* walletModel)
{
    m_walletModel = walletModel;
    setWalletActionsEnabled(walletModel != nullptr);
    if (m_walletModel) {
        connect(m_walletModel, &WalletModel::chainStateChanged,
                this, &BHTWalletGUI::updateChainStateViews, Qt::UniqueConnection);
        updateChainStateViews(m_walletModel->getBHTState(), m_walletModel->getBHTEState());
    }
}

bool BHTWalletGUI::handleURI(const QString& uri)
{
    QMessageBox::information(this, tr("Handle URI"),
        tr("URI: %1\n\n"
           "This is a demo implementation.").arg(uri));
    return true;
}

void BHTWalletGUI::closeEvent(QCloseEvent* event)
{
    event->accept();
}

void BHTWalletGUI::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (windowState() & Qt::WindowMinimized) {
            if (m_trayIcon) {
                hide();
                m_trayIcon->showMessage(tr("BHT Wallet"),
                    tr("BHT Wallet is running in the system tray."),
                    QSystemTrayIcon::Information, 3000);
            }
        }
    }
    QMainWindow::changeEvent(event);
}

void BHTWalletGUI::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void BHTWalletGUI::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            handleURI(url.toString());
        }
    }
}

void BHTWalletGUI::updateNumberOfConnections(int numberOfConnections)
{
    QString icon;
    QString tooltip;

    if (numberOfConnections == 0) {
        icon = ":/icons/connect0";
        tooltip = tr("No active connection to the BHT network");
    } else if (numberOfConnections < 4) {
        icon = ":/icons/connect1";
        tooltip = tr("Catching up... (%1 active connections)").arg(numberOfConnections);
    } else {
        icon = ":/icons/connect4";
        tooltip = tr("Connected to %1 peers").arg(numberOfConnections);
    }

    m_connectionControl->setToolTip(tooltip);
}

void BHTWalletGUI::updateNumberOfBlocks(int numberOfBlocks, const QDateTime& blockDate, double verificationProgress, bool header)
{
    Q_UNUSED(header);

    QString tooltip = tr("Block height: %1\n").arg(numberOfBlocks);

    if (verificationProgress < 0.99) {
        tooltip += tr("Synchronization pending... verification progress: %1%").arg(int(verificationProgress * 100));
    } else {
        tooltip += tr("Up to date. Block height: %1").arg(numberOfBlocks);
    }

    m_headerLbl->setText(tr("%1 behind").arg(1));
    m_headerLbl->setToolTip(tooltip);

    m_progressBar->setVisible(verificationProgress < 0.99);
    m_progressBar->setMaximum(1000000000);
    m_progressBar->setValue(int(verificationProgress * 1000000000));

    if (verificationProgress < 0.99) {
        m_progressLabel->setText(tr("Synchronizing..."));
        m_progressLabel->setVisible(true);
    } else if (verificationProgress < 1.0) {
        m_progressLabel->setText(tr("Verifying blocks..."));
        m_progressLabel->setVisible(true);
    } else {
        m_progressLabel->setVisible(false);
    }
}

void BHTWalletGUI::updateProxyIcon(bool networkEnabled)
{
    Q_UNUSED(networkEnabled);
}

void BHTWalletGUI::updateSslIcon(int state)
{
    Q_UNUSED(state);
}

void BHTWalletGUI::updatePrivacyIndicator()
{
}

QString BHTWalletGUI::getElapsedTimeSince(qint64 nTime)
{
    Q_UNUSED(nTime);
    return QString();
}

void BHTWalletGUI::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
}

void BHTWalletGUI::updateWeight()
{
}

void BHTWalletGUI::modalOverlaySetVisible(bool visible)
{
    Q_UNUSED(visible);
}
