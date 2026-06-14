// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Layer 2 BHTE Integration

#ifndef BHT_WALLET_LAYER2_LAYER2_CLIENT_H
#define BHT_WALLET_LAYER2_LAYER2_CLIENT_H

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>
#include <map>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

namespace bht {
namespace layer2 {

struct BHTEAccount {
    std::string address;
    uint64_t balance;
    uint64_t nonce;
    std::vector<std::string> tokens;
    bool is_contract;
    std::string code_hash;
};

struct BHTETransaction {
    std::string txid;
    std::string from;
    std::string to;
    uint64_t amount;
    uint64_t fee;
    uint64_t nonce;
    std::string status;
    uint64_t block_number;
    uint64_t timestamp;
    std::string memo;
    std::vector<uint8_t> input_data;
};

struct BHTEBridgeStatus {
    bool active;
    uint64_t last_synced_block;
    uint64_t current_block;
    uint64_t pending_deposits;
    uint64_t pending_withdrawals;
    std::string layer1_hash;
    std::string layer2_hash;
    uint64_t bridge_fee;
    uint64_t min_deposit;
    uint64_t min_withdrawal;
};

struct BHTEBlock {
    uint64_t number;
    std::string hash;
    uint64_t timestamp;
    std::vector<BHTETransaction> transactions;
};

class Layer2Client {
public:
    Layer2Client();
    ~Layer2Client();

    bool Connect(const std::string& endpoint);
    void Disconnect();
    bool IsConnected() const { return m_connected.load(); }

    void SetEndpoint(const std::string& endpoint) { m_endpoint = endpoint; }
    std::string GetEndpoint() const { return m_endpoint; }

    void SetProxy(const std::string& proxy) { m_proxy = proxy; }
    std::string GetProxy() const { return m_proxy; }

    void SetTimeout(uint32_t timeout_ms) { m_timeout_ms = timeout_ms; }
    uint32_t GetTimeout() const { return m_timeout_ms; }

    bool GetAccount(const std::string& address, BHTEAccount& account);
    bool GetBalance(const std::string& address, uint64_t& balance);
    bool GetNonce(const std::string& address, uint64_t& nonce);

    bool GetTransaction(const std::string& txid, BHTETransaction& tx);
    bool GetTransactionReceipt(const std::string& txid, BHTETransaction& tx);
    bool GetTransactionHistory(
        const std::string& address,
        std::vector<BHTETransaction>& history,
        uint64_t from_block = 0,
        size_t limit = 50
    );

    bool SendTransaction(
        const std::string& from_address,
        const std::string& to_address,
        uint64_t amount,
        uint64_t fee,
        const std::string& memo,
        const std::vector<uint8_t>& signature,
        std::string& txid
    );

    bool CallContract(
        const std::string& from,
        const std::string& to,
        const std::string& data,
        uint64_t& result
    );

    bool GetCode(const std::string& address, std::vector<uint8_t>& code);

    bool DepositToLayer2(
        const std::string& layer1_txid,
        const std::string& layer2_address,
        uint64_t amount,
        const std::vector<uint8_t>& proof,
        std::string& deposit_id
    );

    bool WithdrawFromLayer2(
        const std::string& layer2_address,
        const std::string& layer1_address,
        uint64_t amount,
        const std::vector<uint8_t>& signature,
        std::string& withdrawal_id
    );

    bool GetBridgeStatus(BHTEBridgeStatus& status);
    bool GetLatestBlock(BHTEBlock& block);
    bool GetBlockByNumber(uint64_t number, BHTEBlock& block);
    bool GetBlockByHash(const std::string& hash, BHTEBlock& block);

    bool EstimateGas(
        const std::string& from,
        const std::string& to,
        uint64_t amount,
        std::string& gas_estimate
    );

    bool GetGasPrice(uint64_t& gas_price);
    bool GetChainId(uint64_t& chain_id);

    using ProgressCallback = std::function<void(int progress, const std::string& status)>;
    void SetProgressCallback(ProgressCallback callback) { m_progress_callback = callback; }

    std::string GetLastError() const { return m_last_error; }

private:
    bool sendRPCRequest(
        const std::string& method,
        const std::string& params,
        std::string& response
    );

    bool sendHTTPPOST(
        const std::string& url,
        const std::string& body,
        std::string& response,
        int& status_code
    );

    bool parseJSONResponse(
        const std::string& response,
        std::map<std::string, std::string>& result
    );

    bool parseTransactionFromJSON(
        const std::map<std::string, std::string>& json_fields,
        BHTETransaction& tx
    );

    std::string buildJSONRPCRequest(
        const std::string& method,
        const std::string& params
    );

    std::string escapeJSONString(const std::string& input);
    std::string trimQuotes(const std::string& str);
    uint64_t parseHexToUint64(const std::string& hex);

    std::string m_endpoint;
    std::string m_proxy;
    std::atomic<bool> m_connected{false};
    std::atomic<uint32_t> m_timeout_ms{30000};
    std::string m_last_error;
    static std::string m_rpc_id_prefix;

#ifdef _WIN32
    HINTERNET m_hInternet{nullptr};
    HINTERNET m_hConnection{nullptr};
#else
    int m_socket{-1};
#endif

    ProgressCallback m_progress_callback;
    std::mutex m_request_mutex;

    static std::string generateRPCID();
};

class BHTEBridge {
public:
    BHTEBridge();
    ~BHTEBridge();

    bool Initialize(const std::string& layer1_endpoint, const std::string& layer2_endpoint);

    bool StartBridgeService();
    void StopBridgeService();

    bool ProcessDeposits();
    bool ProcessWithdrawals();

    bool VerifyDepositProof(
        const std::string& layer1_txid,
        std::vector<uint8_t>& proof
    );

    bool SubmitWithdrawalProof(
        const std::string& withdrawal_id,
        const std::vector<uint8_t>& proof
    );

    bool GetPendingDeposits(std::vector<BHTETransaction>& deposits);
    bool GetPendingWithdrawals(std::vector<BHTETransaction>& withdrawals);

    void SetBridgeFee(uint64_t fee) { m_bridge_fee = fee; }
    uint64_t GetBridgeFee() const { return m_bridge_fee; }

    void SetConfirmationBlocks(uint64_t blocks) { m_confirmation_blocks = blocks; }
    uint64_t GetConfirmationBlocks() const { return m_confirmation_blocks; }

private:
    bool syncBlockHeaders();
    bool processPendingDepositsFromL1();
    bool processPendingWithdrawalsToL1();
    bool submitWithdrawalToL1(const BHTETransaction& withdrawal);

    std::string m_layer1_endpoint;
    std::string m_layer2_endpoint;

    std::unique_ptr<Layer2Client> m_layer1_client;
    std::unique_ptr<Layer2Client> m_layer2_client;

    std::atomic<bool> m_running{false};
    uint64_t m_bridge_fee{100};
    uint64_t m_confirmation_blocks{12};

    std::vector<std::string> m_pending_deposits;
    std::vector<std::string> m_pending_withdrawals;
    std::mutex m_pending_mutex;
    std::string m_last_error;
};

}
}

#endif
