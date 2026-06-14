// SPDX-License-Identifier: MIT
// Copyright (c) 2026 BHC Developers
// BHTE L2 Settlement Contract - zkEVM Compatible

pragma solidity ^0.8.24;

import "@openzeppelin/contracts/access/Ownable.sol";
import "@openzeppelin/contracts/utils/ReentrancyGuard.sol";
import "@openzeppelin/contracts/token/ERC20/IERC20.sol";

contract BHTEsettlement is Ownable, ReentrancyGuard {
    
    struct AnchorRecord {
        bytes32 stateRoot;
        uint256 blockHeight;
        uint256 timestamp;
        bytes32 txHash;
        bool verified;
    }
    
    struct WithdrawalRequest {
        address recipient;
        uint256 amount;
        bytes32 l2TxHash;
        uint256 anchorHeight;
        uint256 challengeDeadline;
        bool processed;
        bool challenged;
    }
    
    struct FraudProof {
        bytes32 invalidStateRoot;
        bytes32 validStateRoot;
        uint256 blockHeight;
        address challenger;
        bytes proof;
        bool resolved;
        bool isFraud;
    }
    
    bytes32 public constant DOMAIN_SEPARATOR = keccak256("BHTE_L2_SETTLEMENT_V1");

    uint256 public constant CHALLENGE_PERIOD = 7 days;
    uint256 public constant MIN_CONFIRMATIONS = 3;
    uint256 public constant MAX_PENDING_WITHDRAWALS = 1000;

    // FIPS 204 ML-DSA-65 sizes (must match BHTE/core/vm/contracts_mldsa.go).
    uint256 public constant MLDSA_SIGNATURE_LEN = 3309;
    uint256 public constant MLDSA_PUBKEY_LEN = 1952;

    // Native ML-DSA verification precompile registered by the BHTE zkEVM.
    address public constant MLDSA_PRECOMPILE = address(0x100);

    mapping(uint256 => AnchorRecord) public anchors;
    mapping(bytes32 => WithdrawalRequest) public withdrawals;
    mapping(uint256 => FraudProof) public fraudProofs;
    mapping(bytes32 => bool) public verifiedMLDSACommitments;
    mapping(uint256 => bytes32) public anchorProofHashes;
    mapping(address => bool) public anchorValidators;
    mapping(bytes32 => uint256) public anchorApprovalCount;
    mapping(bytes32 => mapping(address => bool)) public anchorApprovals;
    
    uint256 public latestAnchorHeight;
    uint256 public totalAnchors;
    uint256 public totalWithdrawals;
    uint256 public pendingWithdrawals;
    uint256 public anchorValidatorCount;
    uint256 public anchorThreshold;
    
    address public bhcBridge;
    address public l2Sequencer;
    address public verifier;
    
    bool public paused;
    
    event AnchorSubmitted(
        uint256 indexed blockHeight,
        bytes32 indexed stateRoot,
        bytes32 txHash,
        bytes32 proofHash,
        uint256 timestamp
    );

    event AnchorApproved(
        uint256 indexed blockHeight,
        bytes32 indexed stateRoot,
        bytes32 txHash,
        address indexed validator,
        uint256 approvals
    );
    
    event WithdrawalInitiated(
        bytes32 indexed l2TxHash,
        address indexed recipient,
        uint256 amount,
        uint256 indexed anchorHeight,
        uint256 challengeDeadline
    );
    
    event WithdrawalProcessed(
        bytes32 indexed l2TxHash,
        address indexed recipient,
        uint256 amount
    );
    
    event WithdrawalChallenged(
        bytes32 indexed l2TxHash,
        address indexed challenger,
        string reason
    );
    
    event FraudProofSubmitted(
        uint256 indexed blockHeight,
        address indexed challenger,
        bytes32 invalidStateRoot
    );
    
    event FraudProofResolved(
        uint256 indexed blockHeight,
        bool isFraud,
        address indexed challenger
    );
    
    event BridgeUpdated(address indexed oldBridge, address indexed newBridge);
    event SequencerUpdated(address indexed oldSequencer, address indexed newSequencer);
    event AnchorValidatorUpdated(address indexed validator, bool enabled);
    event AnchorThresholdUpdated(uint256 oldThreshold, uint256 newThreshold);
    event ContractPaused(address indexed by);
    event ContractUnpaused(address indexed by);
    
    modifier onlyBridge() {
        require(msg.sender == bhcBridge, "Only bridge can call");
        _;
    }
    
    modifier onlySequencer() {
        require(msg.sender == l2Sequencer, "Only sequencer can call");
        _;
    }

    modifier onlyAnchorValidator() {
        require(anchorValidators[msg.sender], "Only anchor validator can call");
        _;
    }
    
    modifier whenNotPaused() {
        require(!paused, "Contract is paused");
        _;
    }
    
    constructor(
        address _bhcBridge,
        address _l2Sequencer,
        address _verifier
    ) Ownable(msg.sender) {
        require(_bhcBridge != address(0), "Invalid bridge");
        require(_l2Sequencer != address(0), "Invalid sequencer");
        require(_verifier != address(0), "Invalid verifier");

        bhcBridge = _bhcBridge;
        l2Sequencer = _l2Sequencer;
        verifier = _verifier;
        anchorValidators[_verifier] = true;
        anchorValidatorCount = 1;
        anchorThreshold = 1;
        paused = false;
    }

    function approveAnchor(
        uint256 blockHeight,
        bytes32 stateRoot,
        bytes32 txHash
    ) external onlyAnchorValidator whenNotPaused {
        require(blockHeight > latestAnchorHeight, "Invalid block height");
        require(stateRoot != bytes32(0), "Invalid state root");

        bytes32 anchorId = _anchorId(blockHeight, stateRoot, txHash);
        require(!anchorApprovals[anchorId][msg.sender], "Anchor already approved");

        anchorApprovals[anchorId][msg.sender] = true;
        anchorApprovalCount[anchorId]++;

        emit AnchorApproved(blockHeight, stateRoot, txHash, msg.sender, anchorApprovalCount[anchorId]);
    }
    
    function submitAnchor(
        uint256 blockHeight,
        bytes32 stateRoot,
        bytes32 txHash,
        bytes calldata proof
    ) external onlySequencer whenNotPaused nonReentrant {
        require(blockHeight > latestAnchorHeight, "Invalid block height");
        require(stateRoot != bytes32(0), "Invalid state root");
        
        require(_verifyAnchorProof(blockHeight, stateRoot, txHash, proof), "Invalid proof");

        bytes32 proofHash = keccak256(proof);
        
        anchors[blockHeight] = AnchorRecord({
            stateRoot: stateRoot,
            blockHeight: blockHeight,
            timestamp: block.timestamp,
            txHash: txHash,
            verified: true
        });
        anchorProofHashes[blockHeight] = proofHash;
        
        latestAnchorHeight = blockHeight;
        totalAnchors++;
        
        emit AnchorSubmitted(blockHeight, stateRoot, txHash, proofHash, block.timestamp);
    }
    
    function initiateWithdrawal(
        bytes32 l2TxHash,
        address recipient,
        uint256 amount,
        uint256 anchorHeight,
        bytes32[] calldata merkleProof,
        uint256 leafIndex
    ) external whenNotPaused nonReentrant {
        require(pendingWithdrawals < MAX_PENDING_WITHDRAWALS, "Too many pending withdrawals");
        require(recipient != address(0), "Invalid recipient");
        require(amount > 0, "Invalid amount");
        require(withdrawals[l2TxHash].recipient == address(0), "Withdrawal exists");
        require(anchors[anchorHeight].verified, "Anchor not verified");
        
        require(_verifyMerkleProof(l2TxHash, merkleProof, leafIndex, anchorHeight), "Invalid merkle proof");
        
        uint256 challengeDeadline = block.timestamp + CHALLENGE_PERIOD;
        
        withdrawals[l2TxHash] = WithdrawalRequest({
            recipient: recipient,
            amount: amount,
            l2TxHash: l2TxHash,
            anchorHeight: anchorHeight,
            challengeDeadline: challengeDeadline,
            processed: false,
            challenged: false
        });
        
        pendingWithdrawals++;
        totalWithdrawals++;
        
        emit WithdrawalInitiated(l2TxHash, recipient, amount, anchorHeight, challengeDeadline);
    }
    
    function processWithdrawal(bytes32 l2TxHash) external whenNotPaused nonReentrant {
        WithdrawalRequest storage withdrawal = withdrawals[l2TxHash];
        
        require(withdrawal.recipient != address(0), "Withdrawal not found");
        require(!withdrawal.processed, "Already processed");
        require(!withdrawal.challenged, "Withdrawal challenged");
        require(block.timestamp >= withdrawal.challengeDeadline, "Challenge period not over");
        
        withdrawal.processed = true;
        pendingWithdrawals--;
        
        (bool success, ) = payable(withdrawal.recipient).call{value: withdrawal.amount}("");
        require(success, "Transfer failed");
        
        emit WithdrawalProcessed(l2TxHash, withdrawal.recipient, withdrawal.amount);
    }
    
    function challengeWithdrawal(
        bytes32 l2TxHash,
        string calldata reason,
        bytes calldata evidence
    ) external whenNotPaused {
        WithdrawalRequest storage withdrawal = withdrawals[l2TxHash];
        
        require(withdrawal.recipient != address(0), "Withdrawal not found");
        require(!withdrawal.processed, "Already processed");
        require(!withdrawal.challenged, "Already challenged");
        require(block.timestamp < withdrawal.challengeDeadline, "Challenge period over");
        require(evidence.length > 0, "Evidence required");
        
        withdrawal.challenged = true;
        pendingWithdrawals--;
        
        emit WithdrawalChallenged(l2TxHash, msg.sender, reason);
    }
    
    function submitFraudProof(
        uint256 blockHeight,
        bytes32 invalidStateRoot,
        bytes32 validStateRoot,
        bytes calldata proof
    ) external whenNotPaused {
        require(anchors[blockHeight].verified, "Block not anchored");
        require(anchors[blockHeight].stateRoot == invalidStateRoot, "State root mismatch");
        
        fraudProofs[blockHeight] = FraudProof({
            invalidStateRoot: invalidStateRoot,
            validStateRoot: validStateRoot,
            blockHeight: blockHeight,
            challenger: msg.sender,
            proof: proof,
            resolved: false,
            isFraud: false
        });
        
        emit FraudProofSubmitted(blockHeight, msg.sender, invalidStateRoot);
    }
    
    function resolveFraudProof(uint256 blockHeight, bool isFraud) external onlyOwner {
        FraudProof storage fp = fraudProofs[blockHeight];
        
        require(fp.challenger != address(0), "Fraud proof not found");
        require(!fp.resolved, "Already resolved");
        
        fp.resolved = true;
        fp.isFraud = isFraud;
        
        if (isFraud) {
            anchors[blockHeight].verified = false;
        }
        
        emit FraudProofResolved(blockHeight, isFraud, fp.challenger);
    }
    
    function verifyMLDSASignature(
        bytes32 message,
        bytes calldata signature,
        bytes calldata pubkey
    ) external view returns (bool) {
        require(signature.length == MLDSA_SIGNATURE_LEN, "Invalid ML-DSA signature length");
        require(pubkey.length == MLDSA_PUBKEY_LEN, "Invalid ML-DSA pubkey length");

        return _verifyMLDSA(message, signature, pubkey);
    }
    
    function getAnchor(uint256 blockHeight) external view returns (
        bytes32 stateRoot,
        uint256 timestamp,
        bytes32 txHash,
        bool verified
    ) {
        AnchorRecord storage anchor = anchors[blockHeight];
        return (
            anchor.stateRoot,
            anchor.timestamp,
            anchor.txHash,
            anchor.verified
        );
    }
    
    function getWithdrawal(bytes32 l2TxHash) external view returns (
        address recipient,
        uint256 amount,
        uint256 anchorHeight,
        uint256 challengeDeadline,
        bool processed,
        bool challenged
    ) {
        WithdrawalRequest storage w = withdrawals[l2TxHash];
        return (
            w.recipient,
            w.amount,
            w.anchorHeight,
            w.challengeDeadline,
            w.processed,
            w.challenged
        );
    }
    
    function updateBridge(address newBridge) external onlyOwner {
        address oldBridge = bhcBridge;
        bhcBridge = newBridge;
        emit BridgeUpdated(oldBridge, newBridge);
    }
    
    function updateSequencer(address newSequencer) external onlyOwner {
        address oldSequencer = l2Sequencer;
        l2Sequencer = newSequencer;
        emit SequencerUpdated(oldSequencer, newSequencer);
    }

    function setAnchorValidator(address validator, bool enabled) external onlyOwner {
        require(validator != address(0), "Invalid validator");

        if (enabled && !anchorValidators[validator]) {
            anchorValidators[validator] = true;
            anchorValidatorCount++;
        } else if (!enabled && anchorValidators[validator]) {
            require(anchorValidatorCount > 1, "Cannot remove last validator");
            anchorValidators[validator] = false;
            anchorValidatorCount--;
            require(anchorThreshold <= anchorValidatorCount, "Threshold too high");
        }

        emit AnchorValidatorUpdated(validator, enabled);
    }

    function setAnchorThreshold(uint256 newThreshold) external onlyOwner {
        require(newThreshold > 0, "Invalid threshold");
        require(newThreshold <= anchorValidatorCount, "Threshold too high");

        uint256 oldThreshold = anchorThreshold;
        anchorThreshold = newThreshold;

        emit AnchorThresholdUpdated(oldThreshold, newThreshold);
    }
    
    function pause() external onlyOwner {
        paused = true;
        emit ContractPaused(msg.sender);
    }
    
    function unpause() external onlyOwner {
        paused = false;
        emit ContractUnpaused(msg.sender);
    }
    
    // _verifyAnchorProof accepts either prior on-chain approvals or a packed
    // list of 65-byte ECDSA signatures from configured anchor validators.
    function _verifyAnchorProof(
        uint256 blockHeight,
        bytes32 stateRoot,
        bytes32 txHash,
        bytes calldata proof
    ) internal view returns (bool) {
        if (blockHeight == 0 || stateRoot == bytes32(0) || anchorThreshold == 0) {
            return false;
        }

        bytes32 anchorId = _anchorId(blockHeight, stateRoot, txHash);
        uint256 approvals = anchorApprovalCount[anchorId];
        if (approvals >= anchorThreshold) {
            return true;
        }

        if (proof.length == 0 || proof.length % 65 != 0) {
            return false;
        }

        bytes32 digest = keccak256(abi.encodePacked(DOMAIN_SEPARATOR, blockHeight, stateRoot, txHash));
        bytes32 ethSigned = keccak256(abi.encodePacked("\x19Ethereum Signed Message:\n32", digest));
        uint256 signatureCount = proof.length / 65;
        address[] memory seen = new address[](signatureCount);
        uint256 seenCount = 0;

        for (uint256 i = 0; i < signatureCount; i++) {
            address recovered = _recoverSigner(proof, i * 65, ethSigned);
            if (
                recovered != address(0) &&
                anchorValidators[recovered] &&
                !anchorApprovals[anchorId][recovered] &&
                !_isSeen(seen, seenCount, recovered)
            ) {
                seen[seenCount] = recovered;
                seenCount++;
                approvals++;
                if (approvals >= anchorThreshold) {
                    return true;
                }
            }
        }

        return false;
    }

    function _recoverSigner(
        bytes calldata signatures,
        uint256 offset,
        bytes32 digest
    ) internal pure returns (address) {
        bytes32 r;
        bytes32 s;
        uint8 v;
        assembly {
            let sigOffset := add(signatures.offset, offset)
            r := calldataload(sigOffset)
            s := calldataload(add(sigOffset, 32))
            v := byte(0, calldataload(add(sigOffset, 64)))
        }
        if (v < 27) {
            v += 27;
        }
        if (v != 27 && v != 28) {
            return address(0);
        }
        return ecrecover(digest, v, r, s);
    }
    
    function _verifyMerkleProof(
        bytes32 leaf,
        bytes32[] calldata proof,
        uint256 index,
        uint256 anchorHeight
    ) internal view returns (bool) {
        bytes32 hash = leaf;
        
        for (uint256 i = 0; i < proof.length; i++) {
            if (index % 2 == 0) {
                hash = keccak256(abi.encodePacked(hash, proof[i]));
            } else {
                hash = keccak256(abi.encodePacked(proof[i], hash));
            }
            index /= 2;
        }
        
        AnchorRecord storage anchor = anchors[anchorHeight];
        if (!anchor.verified) {
            return false;
        }
        return hash == anchor.stateRoot;
    }

    function _anchorId(
        uint256 blockHeight,
        bytes32 stateRoot,
        bytes32 txHash
    ) internal pure returns (bytes32) {
        return keccak256(abi.encodePacked(DOMAIN_SEPARATOR, blockHeight, stateRoot, txHash));
    }

    function _isSeen(
        address[] memory seen,
        uint256 seenCount,
        address signer
    ) internal pure returns (bool) {
        for (uint256 i = 0; i < seenCount; i++) {
            if (seen[i] == signer) {
                return true;
            }
        }
        return false;
    }
    
    // addVerifiedMLDSACommitment records an owner-attested ML-DSA commitment.
    // This is a FALLBACK trust path for environments where the native ML-DSA
    // precompile is unavailable; on BHTE itself, _verifyMLDSA verifies natively.
    function addVerifiedMLDSACommitment(
        bytes32 message,
        bytes calldata signature,
        bytes calldata pubkey
    ) external onlyOwner {
        require(signature.length == MLDSA_SIGNATURE_LEN, "Invalid ML-DSA signature length");
        require(pubkey.length == MLDSA_PUBKEY_LEN, "Invalid ML-DSA pubkey length");

        bytes32 commitment = keccak256(abi.encodePacked(message, signature, pubkey));
        verifiedMLDSACommitments[commitment] = true;
    }

    function batchAddVerifiedMLDSACommitments(
        bytes32[] calldata messages,
        bytes[] calldata signatures,
        bytes[] calldata pubkeys
    ) external onlyOwner {
        require(messages.length == signatures.length, "Array length mismatch");
        require(messages.length == pubkeys.length, "Array length mismatch");

        for (uint256 i = 0; i < messages.length; i++) {
            require(signatures[i].length == MLDSA_SIGNATURE_LEN, "Invalid ML-DSA signature length");
            require(pubkeys[i].length == MLDSA_PUBKEY_LEN, "Invalid ML-DSA pubkey length");

            bytes32 commitment = keccak256(abi.encodePacked(messages[i], signatures[i], pubkeys[i]));
            verifiedMLDSACommitments[commitment] = true;
        }
    }

    // _verifyMLDSANative calls the BHTE zkEVM ML-DSA precompile with the layout
    // message(32) || pubkey(1952) || signature(3309) and returns its boolean.
    function _verifyMLDSANative(
        bytes32 message,
        bytes calldata signature,
        bytes calldata pubkey
    ) internal view returns (bool) {
        bytes memory input = abi.encodePacked(message, pubkey, signature);
        (bool success, bytes memory ret) = MLDSA_PRECOMPILE.staticcall(input);
        if (!success || ret.length < 32) {
            return false;
        }
        return abi.decode(ret, (uint256)) == 1;
    }

    // _verifyMLDSA performs native on-chain ML-DSA verification via the
    // precompile, falling back to an owner-attested commitment when the
    // precompile is not present (e.g. on a stock EVM).
    function _verifyMLDSA(
        bytes32 message,
        bytes calldata signature,
        bytes calldata pubkey
    ) internal view returns (bool) {
        require(signature.length == MLDSA_SIGNATURE_LEN, "Invalid ML-DSA signature length");
        require(pubkey.length == MLDSA_PUBKEY_LEN, "Invalid ML-DSA pubkey length");

        if (_verifyMLDSANative(message, signature, pubkey)) {
            return true;
        }

        bytes32 commitment = keccak256(abi.encodePacked(message, signature, pubkey));
        return verifiedMLDSACommitments[commitment];
    }
    
    receive() external payable {}
    
    fallback() external payable {}
}
