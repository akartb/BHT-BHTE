#include "walletmodel.h"
#include "transactionrecord.h"
#include "optionsmodel.h"
#include "clientmodel.h"
#include "sendcoinsdialog.h"

#include <QtCore/QDateTime>
#include <QtCore/QSettings>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QThread>
#include <QtCore/QEventLoop>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QProcess>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <algorithm>

static constexpr qint64 SATOSHI = 100000000;
static constexpr int POLL_RPC_TIMEOUT_MS = 1200;

WalletModel::WalletModel(QObject* parent)
    : QObject(parent)
{
    memset(m_balances, 0, sizeof(m_balances));

    QSettings settings;
    m_bht_state.chain = "BHT";
    m_bht_state.endpoint = settings.value("bhtRpcEndpoint", "http://127.0.0.1:18332").toString();
    m_bhte_state.chain = "BHTE";
    m_bhte_state.endpoint = settings.value("bhteRpcEndpoint", "http://127.0.0.1:8545").toString();

    m_poll_timer = new QTimer(this);
    connect(m_poll_timer, &QTimer::timeout, this, &WalletModel::pollTimerFires);
    m_poll_timer->start(5000);
}

WalletModel::~WalletModel()
{
    if (m_poll_timer) {
        m_poll_timer->stop();
    }
    if (m_bht_process) {
        m_bht_process->deleteLater();
    }
    if (m_bhte_process) {
        m_bhte_process->deleteLater();
    }
}

void WalletModel::setClientModel(ClientModel* clientModel)
{
    m_client_model = clientModel;
}

ClientModel* WalletModel::getClientModel()
{
    return m_client_model;
}

void WalletModel::setOptionsModel(OptionsModel* optionsModel)
{
    m_options_model = optionsModel;
}

AddressTableModel* WalletModel::getAddressTableModel()
{
    static AddressTableModel* model = new AddressTableModel(this);
    return model;
}

TransactionTableModel* WalletModel::getTransactionTableModel()
{
    static TransactionTableModel* model = new TransactionTableModel(this);
    return model;
}

CAmount WalletModel::getBalance() const
{
    return m_balances[0];
}

CAmount WalletModel::getWatchOnlyBalance() const
{
    return m_balances[3];
}

CAmount WalletModel::getUnconfirmedBalance() const
{
    return m_balances[1];
}

CAmount WalletModel::getImmatureBalance() const
{
    return m_balances[2];
}

CAmount WalletModel::getWatchUnconfirmedBalance() const
{
    return m_balances[4];
}

CAmount WalletModel::getWatchImmatureBalance() const
{
    return m_balances[5];
}

bool WalletModel::haveWatchOnly() const
{
    return m_watch_only;
}

bool WalletModel::isWalletEnabled() const
{
    return true;
}

QString WalletModel::getWalletName() const
{
    return "Default Wallet";
}

QString WalletModel::getDisplayName() const
{
    return "BHT Wallet";
}

bool WalletModel::isMultiWalletEnabled() const
{
    return false;
}

void WalletModel::setEncryptionStatus(int status)
{
    if (m_encryption_status != status) {
        m_encryption_status = status;
        Q_EMIT encryptionStatusChanged(status);
    }
}

int WalletModel::getEncryptionStatus() const
{
    return m_encryption_status;
}

bool WalletModel::validateAddress(const QString& address)
{
    if (address.startsWith("0x") && address.length() == 42) {
        return true;
    }
    if (address.startsWith("bhte1")) {
        return address.length() >= 42;
    }
    if (address.startsWith("bht1") || address.startsWith("tb1")) {
        return address.length() >= 42;
    }
    if (address.startsWith("1") || address.startsWith("3") || address.startsWith("b")) {
        return address.length() >= 26;
    }
    return false;
}

WalletModel::SendCoinsReturn WalletModel::sendCoins(const QList<SendCoinsRecipient>& recipients, bool use_bhthd)
{
    Q_UNUSED(use_bhthd);

    SendCoinsReturn result;
    result.status = ERROR;

    for (const auto& r : recipients) {
        if (!validateAddress(r.address)) {
            result.status = INVALID_ADDRESS;
            result.msg = "Invalid address: " + r.address;
            return result;
        }
        if (r.asset == "BHTE" && !(r.address.startsWith("0x") || r.address.startsWith("bhte1"))) {
            result.status = INVALID_ADDRESS;
            result.msg = "BHTE transfers require a BHTE account address.";
            return result;
        }
    }

    if (recipients.isEmpty()) {
        result.msg = "No recipients.";
        return result;
    }

    const auto& recipient = recipients.first();
    QString error;
    QJsonValue rpcResult;

    if (recipient.asset == "BHTE") {
        QString from = getDefaultBHTEAccount(&error);
        if (from.isEmpty()) {
            result.msg = error.isEmpty() ? "No BHTE account configured. Set bhteDefaultAccount or unlock an account on the BHTE node." : error;
            return result;
        }

        QJsonObject tx;
        tx["from"] = from;
        tx["to"] = recipient.address;
        tx["value"] = quantityToHex(CAmount(recipient.amount));
        tx["gas"] = "0x5208";
        QJsonArray params;
        params.append(tx);
        if (!rpcCall(m_bhte_state.endpoint, "eth_sendTransaction", params, rpcResult, error)) {
            result.msg = error;
            return result;
        }
        result.txid = rpcResult.toString();
        result.status = OK;
        result.msg = "BHTE transaction broadcast.";
        refreshBHTEState();
        return result;
    }

    QJsonArray params;
    params.append(recipient.address);
    params.append(amountToDecimalString(CAmount(recipient.amount)).toDouble());
    params.append(recipient.label);
    params.append(QString());
    params.append(recipient.subtract_fee);

    QSettings settings;
    if (!rpcCall(m_bht_state.endpoint, "sendtoaddress", params, rpcResult, error,
                 settings.value("bhtRpcUser").toString(), settings.value("bhtRpcPassword").toString())) {
        result.msg = error;
        return result;
    }

    result.txid = rpcResult.toString();
    result.status = OK;
    result.msg = "BHT transaction broadcast.";
    refreshBHTState();
    return result;
}

WalletModel::SendCoinsReturn WalletModel::broadcastRawTransaction(const QString& chain, const QString& rawTransaction)
{
    SendCoinsReturn result;
    result.status = ERROR;

    QString error;
    QJsonValue rpcResult;
    QJsonArray params;
    params.append(rawTransaction);

    QSettings settings;
    if (chain.compare("BHTE", Qt::CaseInsensitive) == 0) {
        if (!rpcCall(m_bhte_state.endpoint, "eth_sendRawTransaction", params, rpcResult, error)) {
            result.msg = error;
            return result;
        }
    } else {
        if (!rpcCall(m_bht_state.endpoint, "sendrawtransaction", params, rpcResult, error,
                     settings.value("bhtRpcUser").toString(), settings.value("bhtRpcPassword").toString())) {
            result.msg = error;
            return result;
        }
    }

    result.status = OK;
    result.txid = rpcResult.toString();
    result.msg = "Raw transaction broadcast.";
    return result;
}

void WalletModel::setRpcEndpoints(const QString& bhtEndpoint, const QString& bhteEndpoint)
{
    QMutexLocker locker(&m_rpc_mutex);
    if (!bhtEndpoint.isEmpty()) {
        m_bht_state.endpoint = bhtEndpoint;
    }
    if (!bhteEndpoint.isEmpty()) {
        m_bhte_state.endpoint = bhteEndpoint;
    }
}

WalletModel::ChainState WalletModel::getBHTState() const
{
    QMutexLocker locker(&m_rpc_mutex);
    return m_bht_state;
}

WalletModel::ChainState WalletModel::getBHTEState() const
{
    QMutexLocker locker(&m_rpc_mutex);
    return m_bhte_state;
}

bool WalletModel::refreshChainStates()
{
    bool bht = refreshBHTState();
    bool bhte = refreshBHTEState();
    QString historyError;
    syncTransactionHistory(&historyError);
    Q_EMIT chainStateChanged(getBHTState(), getBHTEState());
    return bht || bhte;
}

QString WalletModel::createBHTDepositProof(const QString& txid, QString* error)
{
    QJsonValue rpcResult;
    QString rpcError;
    QJsonArray txids;
    txids.append(txid);
    QJsonArray params;
    params.append(txids);

    QSettings settings;
    if (!rpcCall(m_bht_state.endpoint, "gettxoutproof", params, rpcResult, rpcError,
                 settings.value("bhtRpcUser").toString(), settings.value("bhtRpcPassword").toString())) {
        if (error) *error = rpcError;
        return QString();
    }
    return rpcResult.toString();
}

QString WalletModel::createBHTEAccountProof(const QString& address, QString* error)
{
    QJsonValue rpcResult;
    QString rpcError;
    QJsonArray params;
    params.append(address);
    params.append(QJsonArray());
    params.append("latest");

    if (!rpcCall(m_bhte_state.endpoint, "eth_getProof", params, rpcResult, rpcError)) {
        if (error) *error = rpcError;
        return QString();
    }

    return QString::fromUtf8(QJsonDocument(rpcResult.toObject()).toJson(QJsonDocument::Compact));
}

WalletModel::BridgeVerification WalletModel::verifyBHTDepositProof(const QString& txid, const QString& proof, int minConfirmations)
{
    BridgeVerification verification;
    verification.chain = "BHT";
    verification.txid = txid;

    if (txid.trimmed().isEmpty() || proof.trimmed().isEmpty()) {
        verification.message = "Missing BHT txid or merkle proof.";
        return verification;
    }

    QSettings settings;
    const QString user = settings.value("bhtRpcUser").toString();
    const QString password = settings.value("bhtRpcPassword").toString();

    QString error;
    QJsonValue result;
    QJsonArray proofParams;
    proofParams.append(proof.trimmed());
    if (!rpcCall(m_bht_state.endpoint, "verifytxoutproof", proofParams, result, error, user, password)) {
        verification.message = error;
        return verification;
    }

    QJsonArray provedTxids = result.toArray();
    bool containsTx = false;
    for (const QJsonValue& value : provedTxids) {
        if (value.toString().compare(txid, Qt::CaseInsensitive) == 0) {
            containsTx = true;
            break;
        }
    }
    if (!containsTx) {
        verification.message = "Proof does not contain requested transaction.";
        return verification;
    }

    QJsonArray txParams;
    txParams.append(txid);
    txParams.append(true);
    if (!rpcCall(m_bht_state.endpoint, "getrawtransaction", txParams, result, error, user, password)) {
        verification.message = error;
        return verification;
    }

    const QJsonObject tx = result.toObject();
    verification.confirmations = tx.value("confirmations").toVariant().toLongLong();
    verification.blockHash = tx.value("blockhash").toString();
    verification.height = tx.value("blockheight").toVariant().toLongLong();
    verification.proofJson = proof;

    if (verification.confirmations < minConfirmations) {
        verification.message = QString("Proof is valid but has only %1 confirmations; %2 required.")
            .arg(verification.confirmations)
            .arg(minConfirmations);
        return verification;
    }

    verification.valid = true;
    verification.message = "BHT deposit proof verified.";
    return verification;
}

WalletModel::BridgeVerification WalletModel::verifyBHTEAccountProof(const QString& address, const QString& proofJson)
{
    BridgeVerification verification;
    verification.chain = "BHTE";
    verification.txid = address;
    verification.proofJson = proofJson;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(proofJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        verification.message = "Invalid BHTE account proof JSON: " + parseError.errorString();
        return verification;
    }

    const QJsonObject proof = doc.object();
    const QString proofAddress = proof.value("address").toString();
    if (!proofAddress.isEmpty() && proofAddress.compare(address, Qt::CaseInsensitive) != 0) {
        verification.message = "BHTE proof address does not match requested account.";
        return verification;
    }

    const bool hasAccountProof = proof.value("accountProof").isArray() && !proof.value("accountProof").toArray().isEmpty();
    const bool hasBalance = proof.contains("balance");
    const bool hasStateRoot = proof.contains("storageHash") || proof.contains("storageRoot");
    if (!hasAccountProof || !hasBalance || !hasStateRoot) {
        verification.message = "BHTE proof is missing accountProof, balance or storage hash.";
        return verification;
    }

    QString error;
    QJsonValue blockResult;
    if (rpcCall(m_bhte_state.endpoint, "eth_blockNumber", QJsonArray(), blockResult, error, QString(), QString(), POLL_RPC_TIMEOUT_MS)) {
        verification.height = amountFromJson(blockResult);
    }

    verification.stateRoot = proof.value("storageHash").toString(proof.value("storageRoot").toString());
    verification.valid = true;
    verification.message = "BHTE account proof structure verified.";
    return verification;
}

bool WalletModel::syncTransactionHistory(QString* error)
{
    m_indexed_transactions.clear();
    QString bhtError;
    QString bhteError;
    const bool bhtOk = refreshBHTTransactions(&bhtError);
    const bool bhteOk = refreshBHTETransactions(&bhteError);
    std::sort(m_indexed_transactions.begin(), m_indexed_transactions.end(),
              [](const TransactionRecord& a, const TransactionRecord& b) {
                  return a.time > b.time;
              });
    getTransactionTableModel()->setTransactions(m_indexed_transactions);
    if (error) {
        QStringList errors;
        if (!bhtOk && !bhtError.isEmpty()) errors.append("BHT: " + bhtError);
        if (!bhteOk && !bhteError.isEmpty()) errors.append("BHTE: " + bhteError);
        *error = errors.join("; ");
    }
    return bhtOk || bhteOk;
}

WalletModel::NodeProcessState WalletModel::getNodeProcessState(const QString& chain) const
{
    QSettings settings;
    const bool isBHTE = chain.compare("BHTE", Qt::CaseInsensitive) == 0;
    QProcess* process = isBHTE ? m_bhte_process : m_bht_process;

    NodeProcessState state;
    state.chain = isBHTE ? "BHTE" : "BHT";
    state.executable = settings.value(isBHTE ? "bhteNodePath" : "bhtNodePath").toString();
    state.dataDir = settings.value(isBHTE ? "bhteDataDir" : "bhtDataDir", getDataDir()).toString();
    state.arguments = settings.value(isBHTE ? "bhteNodeArgs" : "bhtNodeArgs").toString().split(' ', Qt::SkipEmptyParts);
    state.managed = process != nullptr;
    state.running = process && process->state() != QProcess::NotRunning;
    state.pid = state.running ? process->processId() : 0;
    state.lastError = process ? process->errorString() : QString();
    return state;
}

bool WalletModel::startManagedNode(const QString& chain, QString* error)
{
    QSettings settings;
    const bool isBHTE = chain.compare("BHTE", Qt::CaseInsensitive) == 0;
    QProcess* process = isBHTE ? m_bhte_process : m_bht_process;
    const QString executable = settings.value(isBHTE ? "bhteNodePath" : "bhtNodePath").toString().trimmed();
    const QString dataDir = settings.value(isBHTE ? "bhteDataDir" : "bhtDataDir", getDataDir()).toString();
    QStringList args = settings.value(isBHTE ? "bhteNodeArgs" : "bhtNodeArgs").toString().split(' ', Qt::SkipEmptyParts);

    if (executable.isEmpty()) {
        if (error) *error = QString("No %1 node executable configured.").arg(isBHTE ? "BHTE" : "BHT");
        return false;
    }
    if (process && process->state() != QProcess::NotRunning) {
        return true;
    }

    if (!process) {
        process = new QProcess(this);
        process->setProcessChannelMode(QProcess::MergedChannels);
        if (isBHTE) {
            m_bhte_process = process;
        } else {
            m_bht_process = process;
        }
    }
    process->setWorkingDirectory(dataDir);
    process->start(executable, args);
    if (!process->waitForStarted(5000)) {
        if (error) *error = process->errorString();
        return false;
    }
    return true;
}

bool WalletModel::stopManagedNode(const QString& chain, QString* error)
{
    const bool isBHTE = chain.compare("BHTE", Qt::CaseInsensitive) == 0;
    QProcess* process = isBHTE ? m_bhte_process : m_bht_process;
    if (!process || process->state() == QProcess::NotRunning) {
        return true;
    }

    process->terminate();
    if (!process->waitForFinished(8000)) {
        process->kill();
        if (!process->waitForFinished(3000)) {
            if (error) *error = process->errorString();
            return false;
        }
    }
    return true;
}

WalletModel::TransactionBuildResult WalletModel::buildBHTRawTransaction(const QString& toAddress, CAmount amount,
                                                                        bool subtractFee, bool sign)
{
    TransactionBuildResult build;
    build.chain = "BHT";

    if (!validateAddress(toAddress) || amount <= 0) {
        build.message = "Invalid BHT destination or amount.";
        return build;
    }

    QSettings settings;
    const QString user = settings.value("bhtRpcUser").toString();
    const QString password = settings.value("bhtRpcPassword").toString();
    QString error;
    QJsonValue result;

    QJsonObject output;
    output[toAddress] = amountToDecimalString(amount).toDouble();
    QJsonArray params;
    params.append(QJsonArray());
    params.append(output);
    if (!rpcCall(m_bht_state.endpoint, "createrawtransaction", params, result, error, user, password)) {
        build.message = error;
        return build;
    }
    build.rawHex = result.toString();

    QJsonObject options;
    if (subtractFee) {
        QJsonArray subtractOutputs;
        subtractOutputs.append(0);
        options["subtractFeeFromOutputs"] = subtractOutputs;
    }
    QJsonArray fundParams;
    fundParams.append(build.rawHex);
    fundParams.append(options);
    if (!rpcCall(m_bht_state.endpoint, "fundrawtransaction", fundParams, result, error, user, password)) {
        build.message = error;
        return build;
    }
    const QJsonObject funded = result.toObject();
    build.rawHex = funded.value("hex").toString(build.rawHex);
    build.fee = amountFromJson(funded.value("fee"));

    if (sign) {
        QJsonArray signParams;
        signParams.append(build.rawHex);
        if (!rpcCall(m_bht_state.endpoint, "signrawtransactionwithwallet", signParams, result, error, user, password)) {
            build.message = error;
            return build;
        }
        const QJsonObject signedTx = result.toObject();
        build.signedHex = signedTx.value("hex").toString();
        build.complete = signedTx.value("complete").toBool(false);
        if (!build.complete) {
            build.message = "BHT transaction signing incomplete.";
            return build;
        }
    }

    build.ok = true;
    build.message = sign ? "BHT transaction built and signed." : "BHT transaction built.";
    return build;
}

WalletModel::TransactionBuildResult WalletModel::buildBHTERawTransaction(const QString& toAddress, CAmount amount,
                                                                         bool sign)
{
    TransactionBuildResult build;
    build.chain = "BHTE";

    if (!validateAddress(toAddress) || amount <= 0) {
        build.message = "Invalid BHTE destination or amount.";
        return build;
    }

    QString error;
    const QString from = getDefaultBHTEAccount(&error);
    if (from.isEmpty()) {
        build.message = error.isEmpty() ? "No BHTE account configured." : error;
        return build;
    }

    QJsonObject tx;
    tx["from"] = from;
    tx["to"] = toAddress;
    tx["value"] = quantityToHex(amount);

    QJsonValue result;
    QJsonArray gasParams;
    gasParams.append(tx);
    if (rpcCall(m_bhte_state.endpoint, "eth_estimateGas", gasParams, result, error)) {
        tx["gas"] = result.toString();
    } else {
        tx["gas"] = "0x5208";
    }

    QJsonArray nonceParams;
    nonceParams.append(from);
    nonceParams.append("pending");
    if (rpcCall(m_bhte_state.endpoint, "eth_getTransactionCount", nonceParams, result, error)) {
        tx["nonce"] = result.toString();
    }

    if (rpcCall(m_bhte_state.endpoint, "eth_chainId", QJsonArray(), result, error)) {
        tx["chainId"] = result.toString();
    }

    build.rawHex = QString::fromUtf8(QJsonDocument(tx).toJson(QJsonDocument::Compact));
    if (sign) {
        QJsonArray signParams;
        signParams.append(tx);
        if (!rpcCall(m_bhte_state.endpoint, "eth_signTransaction", signParams, result, error)) {
            build.message = "BHTE node did not sign transaction: " + error;
            return build;
        }
        if (result.isObject()) {
            const QJsonObject signedTx = result.toObject();
            build.signedHex = signedTx.value("raw").toString(signedTx.value("rawTransaction").toString());
            build.txid = signedTx.value("tx").toString(signedTx.value("hash").toString());
        } else {
            build.signedHex = result.toString();
        }
        build.complete = !build.signedHex.isEmpty();
    }

    build.ok = !sign || build.complete;
    build.message = sign ? "BHTE transaction built and signed by node." : "BHTE transaction request built.";
    return build;
}

WalletModel::SendCoinsReturn WalletModel::buildSignAndBroadcast(const QString& chain, const QString& toAddress,
                                                                 CAmount amount, bool subtractFee)
{
    if (chain.compare("BHTE", Qt::CaseInsensitive) == 0) {
        TransactionBuildResult build = buildBHTERawTransaction(toAddress, amount, true);
        if (!build.ok) {
            return {ERROR, build.message, QString(), {}};
        }
        return broadcastRawTransaction("BHTE", build.signedHex);
    }

    TransactionBuildResult build = buildBHTRawTransaction(toAddress, amount, subtractFee, true);
    if (!build.ok) {
        return {ERROR, build.message, QString(), {}};
    }
    return broadcastRawTransaction("BHT", build.signedHex);
}

QString WalletModel::getWalletFile() const
{
    return QDir::homePath() + "/BHT/wallet.dat";
}

QString WalletModel::getDataDir() const
{
    return QDir::homePath() + "/BHT";
}

bool WalletModel::hdEnabled() const
{
    return true;
}

int WalletModel::getDefaultWalletType() const
{
    return 0;
}

bool WalletModel::getDefaultInternal() const
{
    return false;
}

QString WalletModel::getPurpose(const QString& addr) const
{
    Q_UNUSED(addr);
    return "receive";
}

bool WalletModel::getTransactionDetails(const uint256& txid, QString& fee, QString& status) const
{
    Q_UNUSED(txid);
    fee = "0.00000000";
    status = "confirmed";
    return true;
}

std::map<QString, QString> WalletModel::getAddressBookMapping() const
{
    std::map<QString, QString> mapping;
    mapping["Sample Address"] = "bht1qsampleaddress1234567890abcdef";
    return mapping;
}

void WalletModel::updateStatus()
{
    CAmount balance = getBalance();
    CAmount unconfirmed = getUnconfirmedBalance();
    CAmount immature = getImmatureBalance();
    CAmount watchOnly = getWatchOnlyBalance();
    CAmount watchUnconf = getWatchUnconfirmedBalance();
    CAmount watchImmature = getWatchImmatureBalance();

    Q_EMIT balanceChanged(balance, unconfirmed, immature,
                          watchOnly, watchUnconf, watchImmature);
}

void WalletModel::pollBalanceChanged()
{
    updateStatus();
}

void WalletModel::updateTransaction()
{
}

void WalletModel::updateAddressBook()
{
}

void WalletModel::updateWatchOnlyBalance(bool haveWatchOnly)
{
    m_watch_only = haveWatchOnly;
}

RecentRequestsTableModel* WalletModel::getRecentRequestsTableModel()
{
    return nullptr;
}

CoinControlModel* WalletModel::getCoinControlModel()
{
    return nullptr;
}

void WalletModel::pollTimerFires()
{
    if (!m_last_chain_poll.isValid() || m_last_chain_poll.secsTo(QDateTime::currentDateTimeUtc()) >= 15) {
        m_last_chain_poll = QDateTime::currentDateTimeUtc();
        refreshChainStates();
    }
    checkBalanceChanged();
}

void WalletModel::checkBalanceChanged()
{
    static CAmount lastBalance = 0;
    CAmount current = getBalance();
    if (current != lastBalance) {
        lastBalance = current;
        Q_EMIT balanceChanged(getBalance(), getUnconfirmedBalance(),
                             getImmatureBalance(), getWatchOnlyBalance(),
                             getWatchUnconfirmedBalance(), getWatchImmatureBalance());
    }
}

bool WalletModel::hdKeypoolAffected() const
{
    return false;
}

void WalletModel::markAffectedKeypoolDirty(bool affected)
{
    Q_UNUSED(affected);
}

bool WalletModel::rpcCall(const QString& endpoint, const QString& method, const QJsonValue& params,
                          QJsonValue& result, QString& error, const QString& user,
                          const QString& password, int timeoutMs) const
{
    QUrl url(endpoint);
    if (!url.isValid() || url.scheme().isEmpty()) {
        error = "Invalid RPC endpoint: " + endpoint;
        return false;
    }

    QJsonObject payload;
    payload["jsonrpc"] = "2.0";
    payload["id"] = QString("bht-wallet-%1").arg(QDateTime::currentMSecsSinceEpoch());
    payload["method"] = method;
    payload["params"] = params.isUndefined() ? QJsonValue(QJsonArray()) : params;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!user.isEmpty()) {
        const QByteArray token = QString("%1:%2").arg(user, password).toUtf8().toBase64();
        request.setRawHeader("Authorization", "Basic " + token);
    }

    QNetworkAccessManager manager;
    QNetworkReply* reply = manager.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    loop.exec();

    if (timeout.isActive()) {
        timeout.stop();
    } else {
        reply->abort();
        reply->deleteLater();
        error = "RPC timeout: " + endpoint;
        return false;
    }

    if (reply->error() != QNetworkReply::NoError) {
        error = reply->errorString();
        reply->deleteLater();
        return false;
    }

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    reply->deleteLater();

    if (httpStatus < 200 || httpStatus >= 300) {
        error = QString("HTTP %1: %2").arg(httpStatus).arg(QString::fromUtf8(body.left(200)));
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        error = "Invalid JSON-RPC response: " + parseError.errorString();
        return false;
    }

    QJsonObject obj = doc.object();
    if (obj.contains("error") && !obj.value("error").isNull()) {
        QJsonObject errObj = obj.value("error").toObject();
        error = errObj.value("message").toString("RPC error");
        return false;
    }

    result = obj.value("result");
    return true;
}

bool WalletModel::refreshBHTState()
{
    QSettings settings;
    ChainState next;
    {
        QMutexLocker locker(&m_rpc_mutex);
        next = m_bht_state;
    }
    next.endpoint = settings.value("bhtRpcEndpoint", next.endpoint.isEmpty() ? "http://127.0.0.1:18332" : next.endpoint).toString();

    QString error;
    QJsonValue result;
    const QString user = settings.value("bhtRpcUser").toString();
    const QString password = settings.value("bhtRpcPassword").toString();

    if (!rpcCall(next.endpoint, "getblockchaininfo", QJsonArray(), result, error, user, password, POLL_RPC_TIMEOUT_MS)) {
        next.connected = false;
        next.lastError = error;
        QMutexLocker locker(&m_rpc_mutex);
        m_bht_state = next;
        return false;
    }

    QJsonObject info = result.toObject();
    next.connected = true;
    next.height = info.value("blocks").toVariant().toLongLong();
    next.bestBlock = info.value("bestblockhash").toString();
    next.lastError.clear();

    if (rpcCall(next.endpoint, "getnetworkinfo", QJsonArray(), result, error, user, password, POLL_RPC_TIMEOUT_MS)) {
        next.peers = result.toObject().value("connections").toInt();
    }

    if (rpcCall(next.endpoint, "getbalance", QJsonArray(), result, error, user, password, POLL_RPC_TIMEOUT_MS)) {
        next.balance = amountFromJson(result);
        m_balances[0] = next.balance;
    }

    {
        QMutexLocker locker(&m_rpc_mutex);
        m_bht_state = next;
    }
    return true;
}

bool WalletModel::refreshBHTEState()
{
    QSettings settings;
    ChainState next;
    {
        QMutexLocker locker(&m_rpc_mutex);
        next = m_bhte_state;
    }
    next.endpoint = settings.value("bhteRpcEndpoint", next.endpoint.isEmpty() ? "http://127.0.0.1:8545" : next.endpoint).toString();

    QString error;
    QJsonValue result;

    if (!rpcCall(next.endpoint, "eth_blockNumber", QJsonArray(), result, error, QString(), QString(), POLL_RPC_TIMEOUT_MS)) {
        next.connected = false;
        next.lastError = error;
        QMutexLocker locker(&m_rpc_mutex);
        m_bhte_state = next;
        return false;
    }

    next.connected = true;
    next.height = amountFromJson(result);
    next.lastError.clear();

    if (rpcCall(next.endpoint, "net_peerCount", QJsonArray(), result, error, QString(), QString(), POLL_RPC_TIMEOUT_MS)) {
        next.peers = static_cast<int>(amountFromJson(result));
    }

    QString account = getDefaultBHTEAccount(nullptr, POLL_RPC_TIMEOUT_MS);
    if (!account.isEmpty()) {
        QJsonArray params;
        params.append(account);
        params.append("latest");
        if (rpcCall(next.endpoint, "eth_getBalance", params, result, error, QString(), QString(), POLL_RPC_TIMEOUT_MS)) {
            next.balance = amountFromJson(result);
        }
    }

    {
        QMutexLocker locker(&m_rpc_mutex);
        m_bhte_state = next;
    }
    return true;
}

bool WalletModel::refreshBHTTransactions(QString* error)
{
    QSettings settings;
    QString rpcError;
    QJsonValue result;
    const QString user = settings.value("bhtRpcUser").toString();
    const QString password = settings.value("bhtRpcPassword").toString();

    QJsonArray params;
    params.append("*");
    params.append(settings.value("bhtHistoryCount", 100).toInt());
    params.append(0);
    params.append(true);
    if (!rpcCall(m_bht_state.endpoint, "listtransactions", params, result, rpcError,
                 user, password, POLL_RPC_TIMEOUT_MS)) {
        if (error) *error = rpcError;
        return false;
    }

    for (const QJsonValue& value : result.toArray()) {
        const QJsonObject obj = value.toObject();
        TransactionRecord record;
        const QString txid = obj.value("txid").toString();
        record.txid = txid.toStdString();
        record.hash = uint256FromHex(txid);
        record.time = obj.value("time").toVariant().toLongLong();
        record.address = obj.value("address").toString().toStdString();
        record.asset = "BHT";
        record.narration = obj.value("category").toString().toStdString();
        const CAmount amount = amountFromJson(obj.value("amount"));
        if (amount >= 0) {
            record.credit = amount;
            record.type = TransactionRecord::RecvWithAddress;
        } else {
            record.debit = -amount;
            record.type = TransactionRecord::SendToAddress;
        }
        record.fee = amountFromJson(obj.value("fee"));
        record.depth = obj.value("confirmations").toInt();
        record.status = txStatusFromConfirmations(record.depth);
        m_indexed_transactions.push_back(record);
    }
    return true;
}

bool WalletModel::refreshBHTETransactions(QString* error)
{
    QSettings settings;
    QString rpcError;
    QString account = getDefaultBHTEAccount(&rpcError, POLL_RPC_TIMEOUT_MS);
    if (account.isEmpty()) {
        if (error) *error = rpcError;
        return false;
    }

    const QString method = settings.value("bhteHistoryMethod", "bhte_getTransactionsByAddress").toString();
    if (method.trimmed().isEmpty()) {
        return true;
    }

    QJsonArray params;
    params.append(account);
    params.append(settings.value("bhteHistoryCount", 100).toInt());
    QJsonValue result;
    if (!rpcCall(m_bhte_state.endpoint, method, params, result, rpcError, QString(), QString(), POLL_RPC_TIMEOUT_MS)) {
        if (error) *error = rpcError;
        return false;
    }

    for (const QJsonValue& value : result.toArray()) {
        const QJsonObject obj = value.toObject();
        TransactionRecord record;
        const QString txid = obj.value("hash").toString(obj.value("txid").toString());
        const QString from = obj.value("from").toString();
        const QString to = obj.value("to").toString();
        const bool incoming = to.compare(account, Qt::CaseInsensitive) == 0;
        const CAmount amount = amountFromJson(obj.value("value"));
        record.txid = txid.toStdString();
        record.hash = uint256FromHex(txid);
        qint64 timestamp = obj.value("timestamp").toVariant().toLongLong();
        if (timestamp == 0) {
            timestamp = obj.value("time").toVariant().toLongLong();
        }
        record.time = timestamp;
        record.address = incoming ? from.toStdString() : to.toStdString();
        record.asset = "BHTE";
        record.narration = incoming ? "incoming" : "outgoing";
        record.credit = incoming ? amount : 0;
        record.debit = incoming ? 0 : amount;
        record.fee = amountFromJson(obj.value("fee"));
        record.depth = obj.value("confirmations").toInt();
        record.status = txStatusFromConfirmations(record.depth);
        record.type = incoming ? TransactionRecord::RecvWithAddress : TransactionRecord::SendToAddress;
        m_indexed_transactions.push_back(record);
    }
    return true;
}

QString WalletModel::getDefaultBHTEAccount(QString* error, int timeoutMs) const
{
    QSettings settings;
    QString account = settings.value("bhteDefaultAccount").toString();
    if (!account.isEmpty()) {
        return account;
    }

    QString rpcError;
    QJsonValue result;
    ChainState state = getBHTEState();
    if (!rpcCall(state.endpoint, "eth_accounts", QJsonArray(), result, rpcError, QString(), QString(), timeoutMs)) {
        if (error) *error = rpcError;
        return QString();
    }

    QJsonArray accounts = result.toArray();
    if (accounts.isEmpty()) {
        if (error) *error = "No BHTE account returned by eth_accounts.";
        return QString();
    }

    return accounts.first().toString();
}

CAmount WalletModel::amountFromJson(const QJsonValue& value)
{
    if (value.isDouble()) {
        return CAmount(static_cast<qint64>(value.toDouble() * SATOSHI));
    }

    QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        return CAmount(0);
    }

    bool ok = false;
    if (text.startsWith("0x", Qt::CaseInsensitive)) {
        quint64 v = text.mid(2).toULongLong(&ok, 16);
        return CAmount(ok ? static_cast<qint64>(v) : 0);
    }

    double decimal = text.toDouble(&ok);
    if (ok) {
        return CAmount(static_cast<qint64>(decimal * SATOSHI));
    }

    return CAmount(0);
}

QString WalletModel::amountToDecimalString(const CAmount& amount)
{
    return QString::number(amount / static_cast<double>(SATOSHI), 'f', 8);
}

QString WalletModel::quantityToHex(const CAmount& amount)
{
    return "0x" + QString::number(static_cast<qulonglong>(amount), 16);
}

uint256 WalletModel::uint256FromHex(const QString& value)
{
    uint256 out;
    QString hex = value;
    if (hex.startsWith("0x", Qt::CaseInsensitive)) {
        hex = hex.mid(2);
    }
    QByteArray bytes = QByteArray::fromHex(hex.toLatin1());
    const int copyLen = std::min<int>(bytes.size(), 32);
    for (int i = 0; i < copyLen; ++i) {
        out.data[copyLen - i - 1] = static_cast<unsigned char>(bytes.at(bytes.size() - i - 1));
    }
    return out;
}

TransactionRecord::Status WalletModel::txStatusFromConfirmations(int confirmations)
{
    if (confirmations < 0) {
        return TransactionRecord::Conflicted;
    }
    if (confirmations == 0) {
        return TransactionRecord::Unconfirmed;
    }
    if (confirmations < 6) {
        return TransactionRecord::Confirming;
    }
    return TransactionRecord::Confirmed;
}

std::map<QString, std::vector<uint256>> WalletModel::listTxid(uint64_t minOrderPos, int nCreateTime) const
{
    Q_UNUSED(minOrderPos);
    Q_UNUSED(nCreateTime);
    std::map<QString, std::vector<uint256>> result;
    return result;
}

QString WalletModel::getTransactionStatus(const uint256& txid) const
{
    Q_UNUSED(txid);
    return "confirmed";
}

QString WalletModel::getTransactionAmount(const uint256& txid) const
{
    Q_UNUSED(txid);
    return "0.00000000";
}

AddressTableModel::AddressTableModel(WalletModel* parent)
    : QAbstractTableModel(parent)
    , m_wallet_model(parent)
{
    updateEntry("bht1qdemo1234567890abcdefghijklmn", "Main Address", true, false, 0);
    updateEntry("bht1qreceive0987654321zyxwvutsrqpo", "Receive Address", true, false, 1);
}

AddressTableModel::~AddressTableModel()
{
}

int AddressTableModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return m_addresses.size();
}

int AddressTableModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 3;
}

QVariant AddressTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_addresses.size())
        return QVariant();

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case Label:
            return m_labels.at(index.row());
        case Address:
            return m_addresses.at(index.row());
        default:
            return QVariant();
        }
    }

    if (role == Qt::UserRole)
        return QString("send");

    return QVariant();
}

bool AddressTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;

    if (index.column() == Label && index.row() < m_labels.size()) {
        m_labels[index.row()] = value.toString();
        Q_EMIT dataChanged(index, index);
        return true;
    }
    return false;
}

QVariant AddressTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case Label: return QString("Label");
    case Address: return QString("Address");
    default: return QVariant();
    }
}

Qt::ItemFlags AddressTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    if (index.column() == Label) return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex AddressTableModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return createIndex(row, column);
}

bool AddressTableModel::removeRows(int row, int count, const QModelIndex& parent)
{
    Q_UNUSED(parent);
    if (row < 0 || row + count > m_addresses.size()) return false;
    beginRemoveRows(QModelIndex(), row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        m_address_to_row.erase(m_addresses.at(row));
        m_addresses.removeAt(row);
        m_labels.removeAt(row);
    }
    endRemoveRows();
    return true;
}

void AddressTableModel::updateEntry(const QString& address, const QString& label, bool isMine, bool isWatchOnly, int type)
{
    Q_UNUSED(isMine);
    Q_UNUSED(isWatchOnly);
    Q_UNUSED(type);

    auto it = m_address_to_row.find(address);
    if (it != m_address_to_row.end()) {
        int row = it->second;
        m_labels[row] = label;
        Q_EMIT dataChanged(index(row, 0), index(row, 1));
    } else {
        beginInsertRows(QModelIndex(), m_addresses.size(), m_addresses.size());
        m_addresses.append(address);
        m_labels.append(label);
        m_address_to_row[address] = m_addresses.size() - 1;
        endInsertRows();
    }
}

bool AddressTableModel::addAddress(const QString& address, const QString& label)
{
    updateEntry(address, label, true, false, 0);
    return true;
}

bool AddressTableModel::updateAddress(const QString& address, const QString& label, bool isMine, bool isWatchOnly, int type)
{
    updateEntry(address, label, isMine, isWatchOnly, type);
    return true;
}

bool AddressTableModel::removeAddress(const QString& address)
{
    auto it = m_address_to_row.find(address);
    if (it != m_address_to_row.end()) {
        return removeRows(it->second, 1);
    }
    return false;
}

QString AddressTableModel::labelForAddress(const QString& address) const
{
    auto it = m_address_to_row.find(address);
    if (it != m_address_to_row.end()) {
        return m_labels.at(it->second);
    }
    return QString();
}

QString AddressTableModel::addressForLabel(const QString& label) const
{
    for (int i = 0; i < m_labels.size(); ++i) {
        if (m_labels.at(i) == label) {
            return m_addresses.at(i);
        }
    }
    return QString();
}

QList<QString> AddressTableModel::getAddressees() const
{
    return m_addresses;
}

QList<QString> AddressTableModel::getLabels() const
{
    return m_labels;
}

TransactionTableModel::TransactionTableModel(WalletModel* parent)
    : QAbstractTableModel(parent)
    , m_wallet_model(parent)
{
}

TransactionTableModel::~TransactionTableModel()
{
}

int TransactionTableModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return static_cast<int>(m_transactions.size());
}

int TransactionTableModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return 8;
}

QVariant TransactionTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_transactions.size()))
        return QVariant();

    const auto& tx = m_transactions.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case Date: return QDateTime::fromSecsSinceEpoch(tx.time).toString("yyyy-MM-dd hh:mm");
        case Type: return TransactionRecord::toString(tx.type);
        case Description: return QString::fromStdString(tx.address).left(20);
        case Amount: return QString::number((tx.credit > 0 ? tx.credit : -tx.debit) / 100000000.0, 'f', 8);
        case Currency: return QString::fromStdString(tx.asset);
        default: return QVariant();
        }
    }

    switch (role) {
    case TxIDRole: return QString::fromStdString(tx.txid);
    case TxHashRole: return QString::fromStdString(tx.txid);
    case AddressRole: return QString::fromStdString(tx.address);
    case LabelRole: return QString::fromStdString(tx.narration);
    case AmountRole: return QVariant::fromValue(tx.credit > 0 ? tx.credit : -tx.debit);
    case DateRole: return QVariant::fromValue(tx.time);
    case TypeRole: return static_cast<int>(tx.type);
    case StatusRole: return static_cast<int>(tx.status);
    case ConfirmationsRole: return tx.depth;
    default: return QVariant();
    }
}

QVariant TransactionTableModel::data(int row, int role) const
{
    if (row < 0 || row >= static_cast<int>(m_transactions.size()))
        return QVariant();

    const auto& tx = m_transactions.at(row);

    switch (role) {
    case AddressRole: return QString::fromStdString(tx.address);
    case AmountRole: return QVariant::fromValue(tx.credit > 0 ? tx.credit : -tx.debit);
    default: return QVariant();
    }
}

QVariant TransactionTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case Status: return QString("Status");
    case Date: return QString("Date");
    case Type: return QString("Type");
    case Description: return QString("Description");
    case AmountSymbol: return QString("+/-");
    case Amount: return QString("Amount");
    default: return QVariant();
    }
}

Qt::ItemFlags TransactionTableModel::flags(const QModelIndex& index) const
{
    Q_UNUSED(index);
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex TransactionTableModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return createIndex(row, column);
}

void TransactionTableModel::setTransactions(const std::vector<TransactionRecord>& transactions)
{
    beginResetModel();
    m_transactions = transactions;
    endResetModel();
}

QString TransactionTableModel::formatAsset(const QString& asset_symbol, const CAmount& amount) const
{
    return QString::number(amount / 100000000.0, 'f', 8) + " " + asset_symbol;
}

QString TransactionTableModel::formatAssetAmount(const CAmount& amount) const
{
    return QString::number(amount / 100000000.0, 'f', 8) + " BHC";
}
