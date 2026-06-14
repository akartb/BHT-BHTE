// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// BHC L1 Withdrawal Script - BHTE to BHC Asset Withdrawal
// This script handles the verification of withdrawal proofs from BHTE Layer 2

#ifndef BHC_WITHDRAWAL_SCRIPT_H
#define BHC_WITHDRAWAL_SCRIPT_H

#include <cstdint>
#include <vector>
#include <span>
#include <optional>
#include <crypto/mldsa.h>
#include <script/hybrid_sig.h>

namespace bhc {
namespace withdrawal {

constexpr uint64_t CHALLENGE_PERIOD_SECONDS = 7 * 24 * 60 * 60;
constexpr uint64_t MIN_ANCHOR_CONFIRMATIONS = 3;
constexpr uint64_t MAX_WITHDRAWAL_DELAY = 24 * 60 * 60;

enum class WithdrawalStatus : uint8_t {
    PENDING = 0,
    CHALLENGED = 1,
    APPROVED = 2,
    EXECUTED = 3,
    EXPIRED = 4,
    FRAUDULENT = 5
};

struct MerkleProof {
    std::vector<std::vector<uint8_t>> siblings;
    uint32_t path;
    std::vector<uint8_t> leaf_hash;
};

struct L2StateRoot {
    uint64_t block_height;
    std::vector<uint8_t> state_root;
    uint64_t timestamp;
    std::vector<uint8_t> anchor_tx_hash;
    bool confirmed;
};

struct WithdrawalRequest {
    std::vector<uint8_t> recipient_bhc_address;
    uint64_t amount;
    uint64_t l2_block_number;
    std::vector<uint8_t> l2_transaction_hash;
    L2StateRoot state_root;
    MerkleProof merkle_proof;
    std::vector<uint8_t> signature;
    std::vector<uint8_t> mldsa_pubkey;
    uint64_t request_timestamp;
    WithdrawalStatus status;
    uint64_t challenge_end_time;
    std::vector<uint8_t> challenge_reason;
};

struct FraudProof {
    std::vector<uint8_t> l2_block_hash;
    std::vector<uint8_t> correct_state_transition_proof;
    std::vector<uint8_t> fraudster_pubkey;
    std::vector<uint8_t> fraudster_signature;
    uint64_t timestamp;
};

class CWithdrawalValidator {
private:
    crypto::MLDSA_Level m_mldsa_level;
    std::vector<L2StateRoot> m_anchored_roots;
    std::vector<WithdrawalRequest> m_pending_withdrawals;
    std::vector<FraudProof> m_fraud_proofs;

    bool verifyMerkleProof(const MerkleProof& proof,
                          const std::vector<uint8_t>& expected_root);

    bool verifyStateRootAnchor(const L2StateRoot& state_root);

    bool verifyWithdrawalSignature(const WithdrawalRequest& request);

    bool verifyFraudProof(const FraudProof& proof);

public:
    explicit CWithdrawalValidator(crypto::MLDSA_Level level = crypto::MLDSA_Level::Level3);

    bool InitWithdrawalEpoch(const std::vector<uint8_t>& initial_state_root,
                              uint64_t start_block_height);

    std::optional<WithdrawalRequest> ParseWithdrawalRequest(
        const std::vector<uint8_t>& raw_request);

    bool SubmitWithdrawal(const WithdrawalRequest& request);

    bool ChallengeWithdrawal(const std::vector<uint8_t>& withdrawal_hash,
                            const FraudProof& fraud_proof);

    bool ApproveWithdrawal(const std::vector<uint8_t>& withdrawal_hash);

    bool ExecuteWithdrawal(const std::vector<uint8_t>& withdrawal_hash);

    bool ExpireWithdrawal(const std::vector<uint8_t>& withdrawal_hash);

    WithdrawalStatus GetWithdrawalStatus(const std::vector<uint8_t>& withdrawal_hash) const;

    std::vector<WithdrawalRequest> GetPendingWithdrawals() const;

    bool IsWithdrawalReady(const std::vector<uint8_t>& withdrawal_hash) const;

    std::vector<uint8_t> CalculateWithdrawalId(const WithdrawalRequest& request) const;
};

bool VerifyEscapeHatch(const std::vector<uint8_t>& l2_proof,
                        const std::vector<uint8_t>& bhc_recovery_address,
                        uint64_t l2_block_number,
                        const std::vector<L2StateRoot>& historical_roots);

class CEscapeHatchVerifier {
private:
    static constexpr uint64_t EMERGENCY_WINDOW_SECONDS = 24 * 60 * 60;

    std::vector<L2StateRoot> m_historical_roots;
    uint64_t m_last_anchor_time;
    bool m_emergency_mode;

public:
    CEscapeHatchVerifier();

    void RecordAnchor(const L2StateRoot& root);

    bool CheckLiveness(uint64_t current_time) const;

    bool VerifyEscapeProof(const std::vector<uint8_t>& l2_proof,
                          const std::vector<uint8_t>& bhc_address) const;

    void TriggerEmergencyMode();

    bool IsEmergencyMode() const;

    std::optional<std::vector<uint8_t>> ExecuteEmergencyWithdrawal(
        const std::vector<uint8_t>& l2_proof,
        const std::vector<uint8_t>& bhc_address,
        uint64_t amount);
};

class CForcedExitVerifier {
private:
    std::vector<uint8_t> m_escape_hatch_script;
    crypto::MLDSA_Level m_mldsa_level;

public:
    explicit CForcedExitVerifier(crypto::MLDSA_Level level = crypto::MLDSA_Level::Level3);

    bool VerifyForcedExitScript(const std::vector<uint8_t>& exit_script) const;

    std::optional<std::vector<uint8_t>> GenerateForcedExitProof(
        const std::vector<uint8_t>& l2_state_proof,
        const std::vector<uint8_t>& fraud_evidence,
        const std::vector<uint8_t>& exit_recipient);
};

}
}

#endif
