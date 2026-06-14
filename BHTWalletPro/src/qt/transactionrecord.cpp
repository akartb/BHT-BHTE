#include "transactionrecord.h"
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <cmath>

QString TransactionRecord::toString(Type type)
{
    switch (type) {
    case Other: return QString("Other");
    case Generated: return QString("Generated");
    case SendToSelf: return QString("SendToSelf");
    case SendToAddress: return QString("SendToAddress");
    case SendToOther: return QString("SendToOther");
    case RecvWithAddress: return QString("RecvWithAddress");
    case RecvFromOther: return QString("RecvFromOther");
    case RecvFromSelf: return QString("RecvFromSelf");
    case BHTToAsset: return QString("BHTToAsset");
    case AssetToBHT: return QString("AssetToBHT");
    case Layer2Deposit: return QString("Layer2Deposit");
    case Layer2Withdrawal: return QString("Layer2Withdrawal");
    case HardwareTx: return QString("HardwareTx");
    default: return QString("Other");
    }
}

TransactionRecord* TransactionRecord::add(
    const uint256& hash,
    int64_t time,
    Type type,
    const std::string& address,
    int64_t debit,
    int64_t credit,
    int64_t fee,
    int64_t locktime,
    Status status,
    int depth,
    int cur_format_i,
    const std::string& asset)
{
    TransactionRecord* record = new TransactionRecord();
    record->hash = hash;
    record->time = time;
    record->type = type;
    record->address = address;
    record->debit = debit;
    record->credit = credit;
    record->fee = fee;
    record->locktime = locktime;
    record->status = status;
    record->depth = depth;
    record->cur_format_i = cur_format_i;
    record->asset = asset;
    return record;
}

QString TransactionRecord::getTransactionHTML(const QString& address, int cur_format)
{
    Q_UNUSED(cur_format);
    QString html;
    html += "<html><body style='font-family: sans-serif;'>";
    html += "<h3>Transaction Details</h3>";
    html += "<p>Address: " + address + "</p>";
    html += "</body></html>";
    return html;
}

void TransactionRecord::updateStatus(const uint256& newHash)
{
    hash = newHash;
    if (depth >= 6) {
        status = Status::Confirmed;
    }
}

TransactionRecord* TransactionRecord::create(
    const uint256& hashBlock,
    int64_t time,
    const uint256& hash,
    int position,
    int64_t credit,
    int64_t debit,
    const std::string& address,
    const std::string& asset,
    int64_t fee,
    const std::string& narration)
{
    TransactionRecord* record = new TransactionRecord();
    record->generated_coinbase_hash = hashBlock;
    record->hash = hash;
    record->idx = position;
    record->time = time;
    record->address = address;
    record->debit = debit;
    record->credit = credit;
    record->fee = fee;
    record->asset = asset;
    record->narration = narration;

    if (credit > 0 && debit == 0) {
        record->type = RecvWithAddress;
    } else if (debit > 0 && credit == 0) {
        record->type = SendToAddress;
    } else {
        record->type = Other;
    }

    record->status = Status::Unconfirmed;
    record->depth = 0;
    record->cur_format_i = 0;

    return record;
}

QString TransactionRecord::formatSubtractFeeFromAmount(bool subtract_fee, const CAmount& amount)
{
    if (subtract_fee) {
        return QString::number(std::abs(amount) / 100000000.0, 'f', 8) + " (fee subtracted)";
    }
    return QString::number(std::abs(amount) / 100000000.0, 'f', 8);
}

std::vector<TransactionRecord::TransactionNotification> TransactionRecord::queuedNotifications;
