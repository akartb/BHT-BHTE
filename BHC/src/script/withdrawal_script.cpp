// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// BHC L1 Withdrawal Script Implementation

#include <script/withdrawal_script.h>
#include <util.h>
#include <hash.h>

namespace bhc {
namespace withdrawal {

CWithdrawalValidator::CWithdrawalValidator(crypto::MLDSA_Level level)
    : m_mldsa_level(level), m_emergency_mode(false)
{
}

bool CWithdrawalValidator::verifyMerkleProof(const MerkleProof& proof,
                                             const std::vector<uint8_t>& expected_root)
{
    if (proof.siblings.empty() || proof.leaf_hash.size() != 32) {
        return false;
    }

    std::array<uint8_t, 32> current_hash;
    std::copy(proof.leaf_hash.begin(), proof.leaf_hash.end(), current_hash.begin());

    for (size_t i = 0; i < proof.siblings.size(); ++i) {
        std::array<uint8_t, 32> sibling_hash;
        if (proof.siblings[i].size() != 32) {
            return false;
        }
        std::copy(proof.siblings[i].begin(), proof.siblings[i].end(), sibling_hash.begin());

        std::array<uint8_t, 64> combined;
        uint32_t bit = (proof.path >> i) & 1;

        if (bit == 0) {
            std::copy(current_hash.begin(), current_hash.end(), combined.begin());
            std::copy(sibling_hash.begin(), sibling_hash.end(), combined.begin() + 32);
        } else {
            std::copy(sibling_hash.begin(), sibling_hash.end(), combined.begin());
            std::copy(current_hash.begin(), current_hash.end(), combined.begin() + 32);
        }

        auto new_hash = crypto::CSHA256::Hash(combined.data(), 64);
        current_hash = new_hash;
    }

    return current_hash == expected_root;
}

bool CWithdrawalValidator::verifyStateRootAnchor(const L2StateRoot& state_root)
{
    for (const auto& anchored : m_anchored_roots) {
        if (anchored.block_height == state_root.block_height &&
            anchored.state_root == state_root.state_root &&
            anchored.confirmed) {
            return true;
        }
    }
    return false;
}

bool CWithdrawalValidator::verifyWithdrawalSignature(const WithdrawalRequest& request)
{
    std::vector<uint8_t> message = CalculateWithdrawalId(request);

    crypto::CMLDSA verifier(m_mldsa_level);

    if (!verifier.ImportPublicKey(request.mldsa_pubkey)) {
        return false;
    }

    return verifier.VerifyDetached(message, request.signature, request.mldsa_pubkey);
}

bool CWithdrawalValidator::verifyFraudProof(const FraudProof& proof)
{
    crypto::CMLDSA verifier(m_mldsa_level);

    if (!verifier.ImportPublicKey(proof.fraudster_pubkey)) {
        return false;
    }

    std::vector<uint8_t> message = proof.l2_block_hash;

    return verifier.VerifyDetached(message, proof.fraudster_signature, proof.fraudster_pubkey);
}

bool CWithdrawalValidator::InitWithdrawalEpoch(const std::vector<uint8_t>& initial_state_root,
                                                uint64_t start_block_height)
{
    L2StateRoot epoch_root;
    epoch_root.block_height = start_block_height;
    epoch_root.state_root = initial_state_root;
    epoch_root.timestamp = GetTime();
    epoch_root.anchor_tx_hash = std::vector<uint8_t>(32, 0);
    epoch_root.confirmed = true;

    m_anchored_roots.clear();
    m_anchored_roots.push_back(epoch_root);
    m_pending_withdrawals.clear();
    m_fraud_proofs.clear();

    return true;
}

std::optional<WithdrawalRequest> CWithdrawalValidator::ParseWithdrawalRequest(
    const std::vector<uint8_t>& raw_request)
{
    if (raw_request.size() < 128) {
        return std::nullopt;
    }

    WithdrawalRequest request;
    size_t offset = 0;

    request.recipient_bhc_address = std::vector<uint8_t>(raw_request.begin() + offset,
                                                          raw_request.begin() + offset + 25);
    offset += 25;

    request.amount = 0;
    for (int i = 0; i < 8; ++i) {
        request.amount |= (static_cast<uint64_t>(raw_request[offset + i]) << (8 * i));
    }
    offset += 8;

    request.l2_block_number = 0;
    for (int i = 0; i < 8; ++i) {
        request.l2_block_number |= (static_cast<uint64_t>(raw_request[offset + i]) << (8 * i));
    }
    offset += 8;

    request.l2_transaction_hash = std::vector<uint8_t>(raw_request.begin() + offset,
                                                        raw_request.begin() + offset + 32);
    offset += 32;

    uint32_t merkle_siblings_count = 0;
    for (int i = 0; i < 4; ++i) {
        merkle_siblings_count |= (static_cast<uint32_t>(raw_request[offset + i]) << (8 * i));
    }
    offset += 4;

    for (uint32_t i = 0; i < merkle_siblings_count; ++i) {
        request.merkle_proof.siblings.push_back(
            std::vector<uint8_t>(raw_request.begin() + offset,
                                  raw_request.begin() + offset + 32));
        offset += 32;
    }

    request.signature = std::vector<uint8_t>(raw_request.begin() + offset,
                                              raw_request.begin() + offset + 3293);
    offset += 3293;

    request.mldsa_pubkey = std::vector<uint8_t>(raw_request.begin() + offset,
                                                  raw_request.begin() + offset + 1952);
    offset += 1952;

    request.request_timestamp = 0;
    for (int i = 0; i < 8; ++i) {
        request.request_timestamp |= (static_cast<uint64_t>(raw_request[offset + i]) << (8 * i));
    }

    request.status = WithdrawalStatus::PENDING;
    request.challenge_end_time = request.request_timestamp + CHALLENGE_PERIOD_SECONDS;

    return request;
}

bool CWithdrawalValidator::SubmitWithdrawal(const WithdrawalRequest& request)
{
    if (!verifyWithdrawalSignature(request)) {
        return false;
    }

    if (!verifyStateRootAnchor(request.state_root)) {
        return false;
    }

    std::vector<uint8_t> withdrawal_id = CalculateWithdrawalId(request);
    request.status = WithdrawalStatus::PENDING;
    m_pending_withdrawals.push_back(request);

    return true;
}

bool CWithdrawalValidator::ChallengeWithdrawal(const std::vector<uint8_t>& withdrawal_hash,
                                               const FraudProof& fraud_proof)
{
    for (auto& request : m_pending_withdrawals) {
        std::vector<uint8_t> current_id = CalculateWithdrawalId(request);
        if (current_id == withdrawal_hash) {
            if (!verifyFraudProof(fraud_proof)) {
                return false;
            }

            request.status = WithdrawalStatus::CHALLENGED;
            request.challenge_reason = fraud_proof.correct_state_transition_proof;
            m_fraud_proofs.push_back(fraud_proof);

            return true;
        }
    }

    return false;
}

bool CWithdrawalValidator::ApproveWithdrawal(const std::vector<uint8_t>& withdrawal_hash)
{
    uint64_t current_time = GetTime();

    for (auto& request : m_pending_withdrawals) {
        std::vector<uint8_t> current_id = CalculateWithdrawalId(request);
        if (current_id == withdrawal_hash) {
            if (request.status != WithdrawalStatus::PENDING) {
                return false;
            }

            if (current_time < request.challenge_end_time) {
                return false;
            }

            request.status = WithdrawalStatus::APPROVED;
            return true;
        }
    }

    return false;
}

bool CWithdrawalValidator::ExecuteWithdrawal(const std::vector<uint8_t>& withdrawal_hash)
{
    for (auto& request : m_pending_withdrawals) {
        std::vector<uint8_t> current_id = CalculateWithdrawalId(request);
        if (current_id == withdrawal_hash) {
            if (request.status != WithdrawalStatus::APPROVED) {
                return false;
            }

            if (request.amount > MAX_WITHDRAWAL_DELAY) {
                return false;
            }

            request.status = WithdrawalStatus::EXECUTED;
            return true;
        }
    }

    return false;
}

bool CWithdrawalValidator::ExpireWithdrawal(const std::vector<uint8_t>& withdrawal_hash)
{
    uint64_t current_time = GetTime();

    for (auto& request : m_pending_withdrawals) {
        std::vector<uint8_t> current_id = CalculateWithdrawalId(request);
        if (current_id == withdrawal_hash) {
            if (request.status == WithdrawalStatus::PENDING ||
                request.status == WithdrawalStatus::CHALLENGED) {
                request.status = WithdrawalStatus::EXPIRED;
                return true;
            }
        }
    }

    return false;
}

WithdrawalStatus CWithdrawalValidator::GetWithdrawalStatus(
    const std::vector<uint8_t>& withdrawal_hash) const
{
    for (const auto& request : m_pending_withdrawals) {
        std::vector<uint8_t> current_id = const_cast<CWithdrawalValidator*>(this)->CalculateWithdrawalId(request);
        if (current_id == withdrawal_hash) {
            return request.status;
        }
    }

    return WithdrawalStatus::EXPIRED;
}

std::vector<WithdrawalRequest> CWithdrawalValidator::GetPendingWithdrawals() const
{
    std::vector<WithdrawalRequest> pending;
    for (const auto& request : m_pending_withdrawals) {
        if (request.status == WithdrawalStatus::PENDING) {
            pending.push_back(request);
        }
    }
    return pending;
}

bool CWithdrawalValidator::IsWithdrawalReady(const std::vector<uint8_t>& withdrawal_hash) const
{
    uint64_t current_time = GetTime();

    for (const auto& request : m_pending_withdrawals) {
        std::vector<uint8_t> current_id = const_cast<CWithdrawalValidator*>(this)->CalculateWithdrawalId(request);
        if (current_id == withdrawal_hash) {
            if (request.status == WithdrawalStatus::PENDING &&
                current_time >= request.challenge_end_time) {
                return true;
            }
        }
    }

    return false;
}

std::vector<uint8_t> CWithdrawalValidator::CalculateWithdrawalId(
    const WithdrawalRequest& request) const
{
    std::vector<uint8_t> data;
    data.insert(data.end(), request.l2_transaction_hash.begin(),
                request.l2_transaction_hash.end());
    data.insert(data.end(), request.recipient_bhc_address.begin(),
                request.recipient_bhc_address.end());

    uint8_t amount_bytes[8];
    for (int i = 0; i < 8; ++i) {
        amount_bytes[i] = (request.amount >> (8 * i)) & 0xFF;
    }
    data.insert(data.end(), amount_bytes, amount_bytes + 8);

    return crypto::CSHA256::Hash(data.data(), data.size());
}

CEscapeHatchVerifier::CEscapeHatchVerifier()
    : m_last_anchor_time(0), m_emergency_mode(false)
{
}

void CEscapeHatchVerifier::RecordAnchor(const L2StateRoot& root)
{
    m_historical_roots.push_back(root);
    m_last_anchor_time = root.timestamp;

    if (m_historical_roots.size() > 10000) {
        m_historical_roots.erase(m_historical_roots.begin());
    }
}

bool CEscapeHatchVerifier::CheckLiveness(uint64_t current_time) const
{
    if (current_time - m_last_anchor_time > EMERGENCY_WINDOW_SECONDS) {
        return false;
    }
    return true;
}

bool CEscapeHatchVerifier::VerifyEscapeProof(const std::vector<uint8_t>& l2_proof,
                                              const std::vector<uint8_t>& bhc_address) const
{
    if (l2_proof.size() < 64) {
        return false;
    }

    if (bhc_address.size() != 25) {
        return false;
    }

    uint64_t proof_block_number = 0;
    for (int i = 0; i < 8; ++i) {
        proof_block_number |= (static_cast<uint64_t>(l2_proof[i]) << (8 * i));
    }

    for (const auto& root : m_historical_roots) {
        if (root.block_height == proof_block_number && root.confirmed) {
            return true;
        }
    }

    return false;
}

void CEscapeHatchVerifier::TriggerEmergencyMode()
{
    m_emergency_mode = true;
}

bool CEscapeHatchVerifier::IsEmergencyMode() const
{
    return m_emergency_mode;
}

std::optional<std::vector<uint8_t>> CEscapeHatchVerifier::ExecuteEmergencyWithdrawal(
    const std::vector<uint8_t>& l2_proof,
    const std::vector<uint8_t>& bhc_address,
    uint64_t amount)
{
    if (!m_emergency_mode) {
        return std::nullopt;
    }

    if (!VerifyEscapeProof(l2_proof, bhc_address)) {
        return std::nullopt;
    }

    std::vector<uint8_t> proof_data = l2_proof;
    proof_data.insert(proof_data.end(), bhc_address.begin(), bhc_address.end());

    uint8_t amount_bytes[8];
    for (int i = 0; i < 8; ++i) {
        amount_bytes[i] = (amount >> (8 * i)) & 0xFF;
    }
    proof_data.insert(proof_data.end(), amount_bytes, amount_bytes + 8);

    return crypto::CSHA256::Hash(proof_data.data(), proof_data.size());
}

CForcedExitVerifier::CForcedExitVerifier(crypto::MLDSA_Level level)
    : m_mldsa_level(level)
{
    std::vector<uint8_t> escape_hatch_bytecode = {
        0x6A, 0x4B, 0x45, 0x52, 0x4E, 0x45, 0x4C, 0x5F,
        0x45, 0x53, 0x43, 0x41, 0x50, 0x45
    };
    m_escape_hatch_script = escape_hatch_bytecode;
}

bool CForcedExitVerifier::VerifyForcedExitScript(const std::vector<uint8_t>& exit_script) const
{
    if (exit_script.size() < m_escape_hatch_script.size()) {
        return false;
    }

    for (size_t i = 0; i < m_escape_hatch_script.size(); ++i) {
        if (exit_script[i] != m_escape_hatch_script[i]) {
            return false;
        }
    }

    return true;
}

std::optional<std::vector<uint8_t>> CForcedExitVerifier::GenerateForcedExitProof(
    const std::vector<uint8_t>& l2_state_proof,
    const std::vector<uint8_t>& fraud_evidence,
    const std::vector<uint8_t>& exit_recipient)
{
    if (l2_state_proof.size() < 32 || fraud_evidence.empty() || exit_recipient.size() != 25) {
        return std::nullopt;
    }

    std::vector<uint8_t> proof_data;
    proof_data.insert(proof_data.end(), l2_state_proof.begin(), l2_state_proof.end());
    proof_data.insert(proof_data.end(), fraud_evidence.begin(), fraud_evidence.end());
    proof_data.insert(proof_data.end(), exit_recipient.begin(), exit_recipient.end());

    std::vector<uint8_t> prefix = {0x46, 0x4F, 0x52, 0x43, 0x45, 0x44};
    proof_data.insert(proof_data.begin(), prefix.begin(), prefix.end());

    return crypto::CSHA256::Hash(proof_data.data(), proof_data.size());
}

bool VerifyEscapeHatch(const std::vector<uint8_t>& l2_proof,
                        const std::vector<uint8_t>& bhc_recovery_address,
                        uint64_t l2_block_number,
                        const std::vector<L2StateRoot>& historical_roots)
{
    if (l2_proof.size() < 64) {
        return false;
    }

    if (bhc_recovery_address.size() != 25) {
        return false;
    }

    for (const auto& root : historical_roots) {
        if (root.block_height == l2_block_number && root.confirmed) {
            return true;
        }
    }

    return false;
}

}
}
