// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Layer 2 BHTE Client Implementation

#include "layer2_client.h"
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstring>

#ifdef _WIN32
#pragma comment(lib, "wininet.lib")
#endif

namespace bht {
namespace layer2 {

std::string Layer2Client::m_rpc_id_prefix = "bht";

Layer2Client::Layer2Client()
    : m_endpoint("http://localhost:8545")
    , m_connected(false)
    , m_timeout_ms(30000)
{
    unsigned char id_bytes[8];
    RAND_bytes(id_bytes, 8);
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 8; ++i) {
        ss << std::setw(2) << (int)id_bytes[i];
    }
    m_rpc_id_prefix = ss.str();
}

Layer2Client::~Layer2Client() {
    Disconnect();
}

bool Layer2Client::Connect(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(m_request_mutex);

    m_endpoint = endpoint;

#ifdef _WIN32
    if (m_hInternet) {
        InternetCloseHandle(m_hInternet);
        m_hInternet = nullptr;
    }

    m_hInternet = InternetOpenA(
        "BHTWalletPro/1.0",
        INTERNET_OPEN_TYPE_DIRECT,
        nullptr,
        nullptr,
        0
    );

    if (!m_hInternet) {
        m_last_error = "Failed to initialize WinINet";
        return false;
    }

    URL_COMPONENTSA urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = (DWORD)-1;
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;

    if (!InternetCrackUrlA(endpoint.c_str(), (DWORD)endpoint.length(), 0, &urlComp)) {
        m_last_error = "Failed to parse URL";
        return false;
    }

    std::string hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
    int port = urlComp.nPort;

    m_hConnection = InternetConnectA(
        m_hInternet,
        hostName.c_str(),
        port,
        nullptr,
        nullptr,
        INTERNET_SERVICE_HTTP,
        0,
        0
    );

    if (!m_hConnection) {
        m_last_error = "Failed to connect to " + hostName + ":" + std::to_string(port);
        return false;
    }

    m_connected = true;
    return true;
#else
    struct addrinfo hints, *result = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    URL_COMPONENTSA urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = (DWORD)-1;
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;

    if (!InternetCrackUrlA(endpoint.c_str(), (DWORD)endpoint.length(), 0, &urlComp)) {
        m_last_error = "Failed to parse URL";
        return false;
    }

    std::string hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
    int port = urlComp.nPort;

    std::string portStr = std::to_string(port);
    if (getaddrinfo(hostName.c_str(), portStr.c_str(), &hints, &result) != 0) {
        m_last_error = "Failed to resolve host: " + hostName;
        return false;
    }

    m_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (m_socket < 0) {
        freeaddrinfo(result);
        m_last_error = "Failed to create socket";
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = m_timeout_ms / 1000;
    timeout.tv_usec = (m_timeout_ms % 1000) * 1000;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    if (connect(m_socket, result->ai_addr, (int)result->ai_addrlen) < 0) {
        freeaddrinfo(result);
        close(m_socket);
        m_socket = -1;
        m_last_error = "Failed to connect to " + hostName + ":" + portStr;
        return false;
    }

    freeaddrinfo(result);
    m_connected = true;
    return true;
#endif
}

void Layer2Client::Disconnect() {
    std::lock_guard<std::mutex> lock(m_request_mutex);

#ifdef _WIN32
    if (m_hConnection) {
        InternetCloseHandle(m_hConnection);
        m_hConnection = nullptr;
    }
    if (m_hInternet) {
        InternetCloseHandle(m_hInternet);
        m_hInternet = nullptr;
    }
#else
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }
#endif

    m_connected = false;
}

bool Layer2Client::sendHTTPPOST(
    const std::string& url,
    const std::string& body,
    std::string& response,
    int& status_code
) {
    response.clear();
    status_code = 0;

#ifdef _WIN32
    if (!m_hConnection) {
        m_last_error = "Not connected";
        return false;
    }

    URL_COMPONENTSA urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = (DWORD)-1;
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;

    if (!InternetCrackUrlA(url.c_str(), (DWORD)url.length(), 0, &urlComp)) {
        m_last_error = "Failed to parse URL";
        return false;
    }

    std::string urlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    if (urlComp.dwUrlPathLength == (DWORD)-1) {
        urlPath = "/";
    }

    const char* acceptTypes[] = {"application/json", nullptr};

    HINTERNET hRequest = HttpOpenRequestA(
        m_hConnection,
        "POST",
        urlPath.empty() ? "/" : urlPath.c_str(),
        nullptr,
        nullptr,
        acceptTypes,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION,
        0
    );

    if (!hRequest) {
        m_last_error = "Failed to open HTTP request";
        return false;
    }

    std::string headers = "Content-Type: application/json\r\n";

    if (!HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(),
                         (LPVOID)body.c_str(), (DWORD)body.length())) {
        m_last_error = "Failed to send HTTP request";
        InternetCloseHandle(hRequest);
        return false;
    }

    DWORD statusCodeSize = sizeof(status_code);
    if (!HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                       &status_code, &statusCodeSize, nullptr)) {
        status_code = 0;
    }

    char buffer[4096];
    DWORD bytesRead;
    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }

    InternetCloseHandle(hRequest);
    return true;
#else
    if (m_socket < 0) {
        m_last_error = "Not connected";
        return false;
    }

    std::string request = "POST / HTTP/1.1\r\n";
    request += "Host: " + m_endpoint + "\r\n";
    request += "Content-Type: application/json\r\n";
    request += "Content-Length: " + std::to_string(body.length()) + "\r\n";
    request += "Connection: close\r\n";
    request += "\r\n";
    request += body;

    if (send(m_socket, request.c_str(), request.length(), 0) < 0) {
        m_last_error = "Failed to send request";
        return false;
    }

    char buffer[4096];
    ssize_t bytesRead;
    while ((bytesRead = recv(m_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }

    size_t headerEnd = response.find("\r\n\r\n");
    if (headerEnd != std::string::npos) {
        std::string headers = response.substr(0, headerEnd);
        std::string body = response.substr(headerEnd + 4);

        size_t statusPos = headers.find("HTTP/");
        if (statusPos != std::string::npos) {
            std::istringstream iss(headers.substr(statusPos));
            std::string httpVersion;
            iss >> httpVersion >> status_code;
        }

        response = body;
    }

    return true;
#endif
}

std::string Layer2Client::buildJSONRPCRequest(
    const std::string& method,
    const std::string& params
) {
    std::string json = "{";
    json += "\"jsonrpc\":\"2.0\",";
    json += "\"method\":\"" + method + "\",";
    json += "\"params\":" + params + ",";
    json += "\"id\":\"" + generateRPCID() + "\"";
    json += "}";
    return json;
}

std::string Layer2Client::generateRPCID() {
    std::stringstream ss;
    ss << m_rpc_id_prefix << "_" << std::hex << std::setfill('0') << std::setw(8)
       << std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::system_clock::now().time_since_epoch()
       ).count();
    return ss.str();
}

bool Layer2Client::sendRPCRequest(
    const std::string& method,
    const std::string& params,
    std::string& response
) {
    if (!m_connected.load()) {
        m_last_error = "Not connected to endpoint";
        return false;
    }

    std::string jsonBody = buildJSONRPCRequest(method, params);

    int statusCode = 0;
    if (!sendHTTPPOST(m_endpoint, jsonBody, response, statusCode)) {
        return false;
    }

    if (statusCode != 200) {
        m_last_error = "HTTP error: " + std::to_string(statusCode);
        return false;
    }

    return true;
}

bool Layer2Client::parseJSONResponse(
    const std::string& response,
    std::map<std::string, std::string>& result
) {
    result.clear();

    size_t resultPos = response.find("\"result\"");
    if (resultPos == std::string::npos) {
        size_t errorPos = response.find("\"error\"");
        if (errorPos != std::string::npos) {
            size_t msgStart = response.find("\"message\"", errorPos);
            if (msgStart != std::string::npos) {
                msgStart = response.find(":", msgStart);
                size_t msgEnd = response.find(",", msgStart);
                if (msgEnd == std::string::npos) msgEnd = response.find("}", msgStart);
                m_last_error = trimQuotes(response.substr(msgStart + 1, msgEnd - msgStart - 1));
            }
            return false;
        }
        return false;
    }

    size_t colonPos = response.find(":", resultPos);
    if (colonPos == std::string::npos) return false;

    size_t startPos = colonPos + 1;
    while (startPos < response.length() && (response[startPos] == ' ' || response[startPos] == '\n')) {
        startPos++;
    }

    if (response[startPos] == '{') {
        size_t braceCount = 1;
        size_t endPos = startPos + 1;
        while (endPos < response.length() && braceCount > 0) {
            if (response[endPos] == '{') braceCount++;
            else if (response[endPos] == '}') braceCount--;
            endPos++;
        }

        std::string jsonObj = response.substr(startPos + 1, endPos - startPos - 2);
        size_t keyPos = 0;
        while (keyPos < jsonObj.length()) {
            size_t keyStart = jsonObj.find("\"", keyPos);
            if (keyStart == std::string::npos) break;
            size_t keyEnd = jsonObj.find("\"", keyStart + 1);
            if (keyEnd == std::string::npos) break;
            std::string key = jsonObj.substr(keyStart + 1, keyEnd - keyStart - 1);

            size_t colonP = jsonObj.find(":", keyEnd);
            if (colonP == std::string::npos) break;
            size_t valueStart = colonP + 1;
            while (valueStart < jsonObj.length() && (jsonObj[valueStart] == ' ' || jsonObj[valueStart] == '\n')) {
                valueStart++;
            }

            size_t valueEnd = valueStart;
            if (jsonObj[valueStart] == '"') {
                valueStart++;
                valueEnd = jsonObj.find("\"", valueStart);
                if (valueEnd == std::string::npos) break;
                std::string value = jsonObj.substr(valueStart, valueEnd - valueStart);
                result[key] = value;
                valueEnd++;
            } else if (jsonObj[valueStart] == '{' || jsonObj[valueStart] == '[') {
                int bracketCount = 1;
                valueEnd = valueStart + 1;
                while (valueEnd < jsonObj.length() && bracketCount > 0) {
                    if (jsonObj[valueEnd] == '{' || jsonObj[valueEnd] == '[') bracketCount++;
                    else if (jsonObj[valueEnd] == '}' || jsonObj[valueEnd] == ']') bracketCount--;
                    valueEnd++;
                }
                result[key] = jsonObj.substr(valueStart, valueEnd - valueStart);
            } else {
                valueEnd = valueStart;
                while (valueEnd < jsonObj.length() && jsonObj[valueEnd] != ',' && jsonObj[valueEnd] != '\n') {
                    valueEnd++;
                }
                result[key] = jsonObj.substr(valueStart, valueEnd - valueStart);
            }

            keyPos = valueEnd;
            while (keyPos < jsonObj.length() && (jsonObj[keyPos] == ',' || jsonObj[keyPos] == ' ' || jsonObj[keyPos] == '\n')) {
                keyPos++;
            }
        }
    } else if (response[startPos] == '[') {
        result["_array"] = response.substr(startPos);
    } else {
        size_t endPos = startPos;
        while (endPos < response.length() && response[endPos] != ',' && response[endPos] != '}') {
            endPos++;
        }
        result["_value"] = response.substr(startPos, endPos - startPos);
    }

    return true;
}

std::string Layer2Client::trimQuotes(const std::string& str) {
    if (str.length() >= 2) {
        if ((str.front() == '"' && str.back() == '"') ||
            (str.front() == '\'' && str.back() == '\'')) {
            return str.substr(1, str.length() - 2);
        }
    }
    return str;
}

uint64_t Layer2Client::parseHexToUint64(const std::string& hex) {
    std::string cleanHex = hex;
    if (cleanHex.substr(0, 2) == "0x") {
        cleanHex = cleanHex.substr(2);
    }
    return std::stoull(cleanHex, nullptr, 16);
}

bool Layer2Client::GetAccount(const std::string& address, BHTEAccount& account) {
    if (!m_connected.load()) return false;

    std::string response;
    std::string params = "[\"" + address + "\"]";
    if (!sendRPCRequest("bhte_getAccount", params, response)) {
        return false;
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    account.address = address;

    auto it = result.find("balance");
    if (it != result.end()) {
        account.balance = parseHexToUint64(it->second);
    }

    it = result.find("nonce");
    if (it != result.end()) {
        account.nonce = parseHexToUint64(it->second);
    }

    it = result.find("isContract");
    if (it != result.end()) {
        account.is_contract = (it->second == "true" || it->second == "1");
    }

    it = result.find("codeHash");
    if (it != result.end()) {
        account.code_hash = trimQuotes(it->second);
    }

    return true;
}

bool Layer2Client::GetBalance(const std::string& address, uint64_t& balance) {
    if (!m_connected.load()) return false;

    std::string response;
    std::string params = "[\"" + address + "\", \"latest\"]";
    if (!sendRPCRequest("eth_getBalance", params, response)) {
        params = "[\"" + address + "\"]";
        if (!sendRPCRequest("bhte_getBalance", params, response)) {
            return false;
        }
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    auto it = result.find("balance");
    if (it != result.end()) {
        balance = parseHexToUint64(it->second);
        return true;
    }

    auto vit = result.find("_value");
    if (vit != result.end()) {
        balance = parseHexToUint64(vit->second);
        return true;
    }

    return false;
}

bool Layer2Client::GetNonce(const std::string& address, uint64_t& nonce) {
    if (!m_connected.load()) return false;

    std::string response;
    std::string params = "[\"" + address + "\", \"latest\"]";
    if (!sendRPCRequest("eth_getTransactionCount", params, response)) {
        params = "[\"" + address + "\"]";
        if (!sendRPCRequest("bhte_getNonce", params, response)) {
            return false;
        }
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    auto it = result.find("nonce");
    if (it != result.end()) {
        nonce = parseHexToUint64(it->second);
        return true;
    }

    auto vit = result.find("_value");
    if (vit != result.end()) {
        nonce = parseHexToUint64(vit->second);
        return true;
    }

    return false;
}

bool Layer2Client::GetTransaction(const std::string& txid, BHTETransaction& tx) {
    if (!m_connected.load()) return false;

    std::string response;
    std::string params = "[\"" + txid + "\"]";

    if (!sendRPCRequest("eth_getTransactionByHash", params, response)) {
        if (!sendRPCRequest("bhte_getTransaction", params, response)) {
            return false;
        }
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        tx.txid = txid;
        tx.status = "pending";
        return true;
    }

    tx.txid = txid;

    auto it = result.find("from");
    if (it != result.end()) tx.from = trimQuotes(it->second);

    it = result.find("to");
    if (it != result.end()) tx.to = trimQuotes(it->second);

    it = result.find("value");
    if (it != result.end()) tx.amount = parseHexToUint64(it->second);

    it = result.find("gasPrice");
    if (it != result.end()) tx.fee = parseHexToUint64(it->second);

    it = result.find("nonce");
    if (it != result.end()) tx.nonce = parseHexToUint64(it->second);

    it = result.find("blockNumber");
    if (it != result.end() && it->second != "null") {
        tx.block_number = parseHexToUint64(it->second);
    }

    it = result.find("status");
    if (it != result.end()) tx.status = trimQuotes(it->second);

    it = result.find("input");
    if (it != result.end()) {
        std::string inputHex = trimQuotes(it->second);
        if (inputHex.length() > 2) {
            std::string inputData = inputHex.substr(2);
            for (size_t i = 0; i < inputData.length(); i += 2) {
                std::string byteStr = inputData.substr(i, 2);
                tx.input_data.push_back(static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16)));
            }
        }
    }

    return true;
}

bool Layer2Client::GetTransactionReceipt(const std::string& txid, BHTETransaction& tx) {
    if (!m_connected.load()) return false;

    std::string response;
    std::string params = "[\"" + txid + "\"]";

    if (!sendRPCRequest("eth_getTransactionReceipt", params, response)) {
        if (!sendRPCRequest("bhte_getTransactionReceipt", params, response)) {
            return false;
        }
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return GetTransaction(txid, tx);
    }

    tx.txid = txid;

    auto it = result.find("status");
    if (it != result.end()) {
        std::string status = trimQuotes(it->second);
        tx.status = (status == "0x1" || status == "1") ? "confirmed" : "failed";
    }

    it = result.find("blockNumber");
    if (it != result.end()) {
        tx.block_number = parseHexToUint64(it->second);
    }

    return true;
}

bool Layer2Client::GetTransactionHistory(
    const std::string& address,
    std::vector<BHTETransaction>& history,
    uint64_t from_block,
    size_t limit
) {
    if (!m_connected.load()) return false;

    history.clear();

    std::string response;
    char blockHex[32];
    snprintf(blockHex, sizeof(blockHex), "0x%llx", (unsigned long long)from_block);

    std::string params = "[{";
    params += "\"from\":\"" + address + "\",";
    params += "\"startBlock\":\"" + std::string(blockHex) + "\",";
    params += "\"endBlock\":\"latest\",";
    params += "\" \"limit\":" + std::to_string(limit);
    params += "}]";

    if (!sendRPCRequest("bhte_getTransactionsByAddress", params, response)) {
        params = "[\"" + address + "\", " + std::to_string(limit) + "]";
        if (!sendRPCRequest("eth_getLogs", params, response)) {
            return false;
        }
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    auto it = result.find("transactions");
    if (it != result.end()) {
        std::string txArray = it->second;
        size_t txStart = 0;
        while ((txStart = txArray.find("{", txStart)) != std::string::npos) {
            size_t txEnd = txArray.find("}", txStart);
            if (txEnd == std::string::npos) break;

            std::string txObj = txArray.substr(txStart, txEnd - txStart + 1);
            BHTETransaction tx;

            size_t fromPos = txObj.find("\"from\"");
            if (fromPos != std::string::npos) {
                size_t colonP = txObj.find(":", fromPos);
                size_t quoteP = txObj.find("\"", colonP + 1);
                size_t endQuoteP = txObj.find("\"", quoteP + 1);
                tx.from = txObj.substr(quoteP + 1, endQuoteP - quoteP - 1);
            }

            size_t toPos = txObj.find("\"to\"");
            if (toPos != std::string::npos) {
                size_t colonP = txObj.find(":", toPos);
                size_t quoteP = txObj.find("\"", colonP + 1);
                size_t endQuoteP = txObj.find("\"", quoteP + 1);
                tx.to = txObj.substr(quoteP + 1, endQuoteP - quoteP - 1);
            }

            size_t valuePos = txObj.find("\"value\"");
            if (valuePos != std::string::npos) {
                size_t colonP = txObj.find(":", valuePos);
                std::string valueStr = txObj.substr(colonP + 1, 20);
                tx.amount = parseHexToUint64(valueStr);
            }

            history.push_back(tx);
            txStart = txEnd + 1;
        }
    }

    return true;
}

bool Layer2Client::SendTransaction(
    const std::string& from_address,
    const std::string& to_address,
    uint64_t amount,
    uint64_t fee,
    const std::string& memo,
    const std::vector<uint8_t>& signature,
    std::string& txid
) {
    if (!m_connected.load()) return false;

    std::string response;
    std::string params = "[{";
    params += "\"from\":\"" + from_address + "\",";
    params += "\"to\":\"" + to_address + "\",";
    params += "\"value\":\"0x" + std::to_string(amount) + "\",";
    params += "\"gasPrice\":\"0x" + std::to_string(fee) + "\",";
    params += "\"data\":\"";

    for (size_t i = 0; i < signature.size(); ++i) {
        char hexBuf[4];
        snprintf(hexBuf, sizeof(hexBuf), "%02x", signature[i]);
        params += hexBuf;
    }
    params += "\"";

    if (!memo.empty()) {
        params += ",\"memo\":\"" + escapeJSONString(memo) + "\"";
    }

    params += "}]";

    if (!sendRPCRequest("eth_sendTransaction", params, response)) {
        if (!sendRPCRequest("bhte_sendTransaction", params, response)) {
            return false;
        }
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    auto it = result.find("transactionHash");
    if (it != result.end()) {
        txid = trimQuotes(it->second);
        return true;
    }

    auto vit = result.find("_value");
    if (vit != result.end()) {
        txid = trimQuotes(vit->second);
        return true;
    }

    return false;
}

bool Layer2Client::CallContract(
    const std::string& from,
    const std::string& to,
    const std::string& data,
    uint64_t& result
) {
    if (!m_connected.load()) return false;

    std::string response;
    std::string params = "[{";
    params += "\"from\":\"" + from + "\",";
    params += "\"to\":\"" + to + "\",";
    params += "\"data\":\"" + data + "\"";
    params += "}, \"latest\"]";

    if (!sendRPCRequest("eth_call", params, response)) {
        return false;
    }

    std::map<std::string, std::string> jsonResult;
    if (!parseJSONResponse(response, jsonResult)) {
        return false;
    }

    auto it = jsonResult.find("_value");
    if (it != jsonResult.end()) {
        result = parseHexToUint64(it->second);
        return true;
    }

    return false;
}

bool Layer2Client::GetCode(const std::string& address, std::vector<uint8_t>& code) {
    if (!m_connected.load()) return false;

    std::string response;
    std::string params = "[\"" + address + "\", \"latest\"]";
    if (!sendRPCRequest("eth_getCode", params, response)) {
        return false;
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    auto it = result.find("_value");
    if (it != result.end()) {
        std::string codeHex = trimQuotes(it->second);
        if (codeHex.length() > 2) {
            codeHex = codeHex.substr(2);
            for (size_t i = 0; i < codeHex.length(); i += 2) {
                std::string byteStr = codeHex.substr(i, 2);
                code.push_back(static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16)));
            }
            return true;
        }
    }

    return false;
}

bool Layer2Client::DepositToLayer2(
    const std::string& layer1_txid,
    const std::string& layer2_address,
    uint64_t amount,
    const std::vector<uint8_t>& proof,
    std::string& deposit_id
) {
    if (!m_connected.load()) return false;

    std::string response;
    std::string params = "[{";
    params += "\"layer1Txid\":\"" + layer1_txid + "\",";
    params += "\"layer2Address\":\"" + layer2_address + "\",";
    params += "\"amount\":\"0x" + std::to_string(amount) + "\",";
    params += "\"proof\":\"";

    for (size_t i = 0; i < proof.size(); ++i) {
        char hexBuf[4];
        snprintf(hexBuf, sizeof(hexBuf), "%02x", proof[i]);
        params += hexBuf;
    }
    params += "\"}]";

    if (!sendRPCRequest("bhte_deposit", params, response)) {
        return false;
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    auto it = result.find("depositId");
    if (it != result.end()) {
        deposit_id = trimQuotes(it->second);
        return true;
    }

    return false;
}

bool Layer2Client::WithdrawFromLayer2(
    const std::string& layer2_address,
    const std::string& layer1_address,
    uint64_t amount,
    const std::vector<uint8_t>& signature,
    std::string& withdrawal_id
) {
    if (!m_connected.load()) return false;

    std::string response;
    std::string params = "[{";
    params += "\"layer2Address\":\"" + layer2_address + "\",";
    params += "\"layer1Address\":\"" + layer1_address + "\",";
    params += "\"amount\":\"0x" + std::to_string(amount) + "\",";
    params += "\"signature\":\"";

    for (size_t i = 0; i < signature.size(); ++i) {
        char hexBuf[4];
        snprintf(hexBuf, sizeof(hexBuf), "%02x", signature[i]);
        params += hexBuf;
    }
    params += "\"}]";

    if (!sendRPCRequest("bhte_withdraw", params, response)) {
        return false;
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    auto it = result.find("withdrawalId");
    if (it != result.end()) {
        withdrawal_id = trimQuotes(it->second);
        return true;
    }

    return false;
}

bool Layer2Client::GetBridgeStatus(BHTEBridgeStatus& status) {
    if (!m_connected.load()) return false;

    std::string response;
    if (!sendRPCRequest("bhte_getBridgeStatus", "[]", response)) {
        return false;
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    auto it = result.find("active");
    if (it != result.end()) {
        status.active = (it->second == "true" || it->second == "1");
    }

    it = result.find("lastSyncedBlock");
    if (it != result.end()) {
        status.last_synced_block = parseHexToUint64(it->second);
    }

    it = result.find("currentBlock");
    if (it != result.end()) {
        status.current_block = parseHexToUint64(it->second);
    }

    it = result.find("pendingDeposits");
    if (it != result.end()) {
        status.pending_deposits = parseHexToUint64(it->second);
    }

    it = result.find("pendingWithdrawals");
    if (it != result.end()) {
        status.pending_withdrawals = parseHexToUint64(it->second);
    }

    it = result.find("bridgeFee");
    if (it != result.end()) {
        status.bridge_fee = parseHexToUint64(it->second);
    }

    it = result.find("minDeposit");
    if (it != result.end()) {
        status.min_deposit = parseHexToUint64(it->second);
    }

    it = result.find("minWithdrawal");
    if (it != result.end()) {
        status.min_withdrawal = parseHexToUint64(it->second);
    }

    return true;
}

bool Layer2Client::GetLatestBlock(BHTEBlock& block) {
    if (!m_connected.load()) return false;

    std::string response;
    if (!sendRPCRequest("eth_blockNumber", "[]", response)) {
        if (!sendRPCRequest("bhte_blockNumber", "[]", response)) {
            return false;
        }
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    auto it = result.find("_value");
    if (it != result.end()) {
        block.number = parseHexToUint64(it->second);
        block.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        return true;
    }

    return false;
}

bool Layer2Client::GetBlockByNumber(uint64_t number, BHTEBlock& block) {
    if (!m_connected.load()) return false;

    std::string response;
    char blockHex[32];
    snprintf(blockHex, sizeof(blockHex), "0x%llx", (unsigned long long)number);
    std::string params = "[\"" + std::string(blockHex) + "\", false]";

    if (!sendRPCRequest("eth_getBlockByNumber", params, response)) {
        return false;
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    block.number = number;

    auto it = result.find("hash");
    if (it != result.end()) {
        block.hash = trimQuotes(it->second);
    }

    it = result.find("timestamp");
    if (it != result.end()) {
        block.timestamp = parseHexToUint64(it->second);
    }

    return true;
}

bool Layer2Client::GetBlockByHash(const std::string& hash, BHTEBlock& block) {
    if (!m_connected.load()) return false;

    std::string response;
    std::string params = "[\"" + hash + "\", false]";

    if (!sendRPCRequest("eth_getBlockByHash", params, response)) {
        return false;
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    block.hash = hash;

    auto it = result.find("number");
    if (it != result.end()) {
        block.number = parseHexToUint64(it->second);
    }

    it = result.find("timestamp");
    if (it != result.end()) {
        block.timestamp = parseHexToUint64(it->second);
    }

    return true;
}

bool Layer2Client::EstimateGas(
    const std::string& from,
    const std::string& to,
    uint64_t amount,
    std::string& gas_estimate
) {
    if (!m_connected.load()) return false;

    std::string response;
    std::string params = "[{";
    params += "\"from\":\"" + from + "\",";
    params += "\"to\":\"" + to + "\",";
    params += "\"value\":\"0x" + std::to_string(amount) + "\"";
    params += "}]";

    if (!sendRPCRequest("eth_estimateGas", params, response)) {
        return false;
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    auto it = result.find("_value");
    if (it != result.end()) {
        gas_estimate = trimQuotes(it->second);
        return true;
    }

    return false;
}

bool Layer2Client::GetGasPrice(uint64_t& gas_price) {
    if (!m_connected.load()) return false;

    std::string response;
    if (!sendRPCRequest("eth_gasPrice", "[]", response)) {
        if (!sendRPCRequest("bhte_gasPrice", "[]", response)) {
            return false;
        }
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    auto it = result.find("_value");
    if (it != result.end()) {
        gas_price = parseHexToUint64(it->second);
        return true;
    }

    return false;
}

bool Layer2Client::GetChainId(uint64_t& chain_id) {
    if (!m_connected.load()) return false;

    std::string response;
    if (!sendRPCRequest("eth_chainId", "[]", response)) {
        if (!sendRPCRequest("bhte_chainId", "[]", response)) {
            return false;
        }
    }

    std::map<std::string, std::string> result;
    if (!parseJSONResponse(response, result)) {
        return false;
    }

    auto it = result.find("_value");
    if (it != result.end()) {
        chain_id = parseHexToUint64(it->second);
        return true;
    }

    return false;
}

std::string Layer2Client::escapeJSONString(const std::string& input) {
    std::string result;
    for (char c : input) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

bool Layer2Client::parseTransactionFromJSON(
    const std::map<std::string, std::string>& json_fields,
    BHTETransaction& tx
) {
    (void)json_fields; (void)tx;
    return false;
}

BHTEBridge::BHTEBridge()
    : m_running(false)
    , m_bridge_fee(100)
    , m_confirmation_blocks(12)
{
}

BHTEBridge::~BHTEBridge() {
    StopBridgeService();
}

bool BHTEBridge::Initialize(
    const std::string& layer1_endpoint,
    const std::string& layer2_endpoint
) {
    m_layer1_endpoint = layer1_endpoint;
    m_layer2_endpoint = layer2_endpoint;

    m_layer1_client = std::make_unique<Layer2Client>();
    m_layer2_client = std::make_unique<Layer2Client>();

    if (!m_layer1_client->Connect(layer1_endpoint)) {
        m_last_error = "Failed to connect to L1: " + m_layer1_client->GetLastError();
        return false;
    }

    if (!m_layer2_client->Connect(layer2_endpoint)) {
        m_last_error = "Failed to connect to L2: " + m_layer2_client->GetLastError();
        return false;
    }

    return true;
}

bool BHTEBridge::StartBridgeService() {
    if (m_running) return true;
    m_running = true;
    return true;
}

void BHTEBridge::StopBridgeService() {
    m_running = false;
}

bool BHTEBridge::ProcessDeposits() {
    if (!m_running) return false;
    return processPendingDepositsFromL1();
}

bool BHTEBridge::ProcessWithdrawals() {
    if (!m_running) return false;
    return processPendingWithdrawalsToL1();
}

bool BHTEBridge::syncBlockHeaders() {
    if (!m_layer1_client || !m_layer2_client) return false;

    BHTEBlock l1Block, l2Block;

    if (!m_layer1_client->GetLatestBlock(l1Block)) {
        return false;
    }

    if (!m_layer2_client->GetLatestBlock(l2Block)) {
        return false;
    }

    return true;
}

bool BHTEBridge::processPendingDepositsFromL1() {
    if (!m_layer1_client) return false;

    std::vector<BHTETransaction> deposits;
    if (!m_layer1_client->GetTransactionHistory("bridge_address", deposits, 0, 50)) {
        return false;
    }

    for (const auto& deposit : deposits) {
        if (deposit.status != "confirmed") continue;

        std::string deposit_id;
        std::vector<uint8_t> proof;

        if (m_layer2_client->DepositToLayer2(
            deposit.txid,
            deposit.to,
            deposit.amount,
            proof,
            deposit_id
        )) {
            std::lock_guard<std::mutex> lock(m_pending_mutex);
            m_pending_deposits.push_back(deposit_id);
        }
    }

    return true;
}

bool BHTEBridge::processPendingWithdrawalsToL1() {
    if (!m_layer2_client) return false;

    std::vector<BHTETransaction> withdrawals;
    if (!m_layer2_client->GetTransactionHistory("bridge_address", withdrawals, 0, 50)) {
        return false;
    }

    for (const auto& withdrawal : withdrawals) {
        if (withdrawal.status != "confirmed") continue;

        if (submitWithdrawalToL1(withdrawal)) {
            std::lock_guard<std::mutex> lock(m_pending_mutex);
            m_pending_withdrawals.push_back(withdrawal.txid);
        }
    }

    return true;
}

bool BHTEBridge::submitWithdrawalToL1(const BHTETransaction& withdrawal) {
    return true;
}

bool BHTEBridge::VerifyDepositProof(
    const std::string& layer1_txid,
    std::vector<uint8_t>& proof
) {
    proof.clear();
    return true;
}

bool BHTEBridge::SubmitWithdrawalProof(
    const std::string& withdrawal_id,
    const std::vector<uint8_t>& proof
) {
    return true;
}

bool BHTEBridge::GetPendingDeposits(std::vector<BHTETransaction>& deposits) {
    deposits.clear();
    std::lock_guard<std::mutex> lock(m_pending_mutex);

    for (const auto& id : m_pending_deposits) {
        BHTETransaction tx;
        tx.txid = id;
        tx.status = "pending";
        deposits.push_back(tx);
    }

    return true;
}

bool BHTEBridge::GetPendingWithdrawals(std::vector<BHTETransaction>& withdrawals) {
    withdrawals.clear();
    std::lock_guard<std::mutex> lock(m_pending_mutex);

    for (const auto& id : m_pending_withdrawals) {
        BHTETransaction tx;
        tx.txid = id;
        tx.status = "pending";
        withdrawals.push_back(tx);
    }

    return true;
}

}
}
