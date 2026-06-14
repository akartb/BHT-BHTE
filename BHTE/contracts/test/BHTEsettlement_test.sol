// SPDX-License-Identifier: MIT
// Copyright (c) 2026 BHC Developers
// BHTE Settlement Contract Tests

pragma solidity ^0.8.24;

import "forge-std/Test.sol";
import "../BHTEsettlement.sol";

contract BHTEsettlementTest is Test {
    BHTEsettlement public settlement;
    
    uint256 internal constant MLDSA_SIGNATURE_LEN = 3309;
    uint256 internal constant MLDSA_PUBKEY_LEN = 1952;
    uint256 internal verifierKey = 0xA11CE;
    uint256 internal validator2Key = 0xB0B;

    address public owner = address(0x1);
    address public bridge = address(0x2);
    address public sequencer = address(0x3);
    address public verifier;
    address public user = address(0x5);
    
    function setUp() public {
        verifier = vm.addr(verifierKey);
        vm.startPrank(owner);
        settlement = new BHTEsettlement(bridge, sequencer, verifier);
        vm.stopPrank();
    }
    
    function test_Owner() public {
        assertEq(settlement.owner(), owner);
    }
    
    function test_InitialState() public {
        assertEq(settlement.latestAnchorHeight(), 0);
        assertEq(settlement.totalAnchors(), 0);
        assertEq(settlement.paused(), false);
        assertEq(settlement.anchorThreshold(), 1);
        assertEq(settlement.anchorValidatorCount(), 1);
        assertTrue(settlement.anchorValidators(verifier));
    }
    
    function test_AddVerifiedMLDSACommitment() public {
        vm.startPrank(owner);
        
        bytes32 message = keccak256("test message");
        bytes memory signature = new bytes(MLDSA_SIGNATURE_LEN);
        bytes memory pubkey = new bytes(MLDSA_PUBKEY_LEN);
        
        for (uint i = 0; i < signature.length; i++) {
            signature[i] = bytes1(uint8(i % 256));
        }
        for (uint i = 0; i < pubkey.length; i++) {
            pubkey[i] = bytes1(uint8((i + 128) % 256));
        }
        
        bytes32 commitment = keccak256(abi.encodePacked(message, signature, pubkey));
        
        assertFalse(settlement.verifiedMLDSACommitments(commitment));
        
        settlement.addVerifiedMLDSACommitment(message, signature, pubkey);
        
        assertTrue(settlement.verifiedMLDSACommitments(commitment));
        
        vm.stopPrank();
    }
    
    function test_AddVerifiedMLDSACommitment_NotOwner() public {
        bytes32 message = keccak256("test message");
        bytes memory signature = new bytes(MLDSA_SIGNATURE_LEN);
        bytes memory pubkey = new bytes(MLDSA_PUBKEY_LEN);
        
        vm.expectRevert(abi.encodeWithSelector(bytes4(keccak256("OwnableUnauthorizedAccount(address)")), user));
        vm.prank(user);
        settlement.addVerifiedMLDSACommitment(message, signature, pubkey);
    }
    
    function test_AddVerifiedMLDSACommitment_InvalidSignatureLength() public {
        vm.startPrank(owner);
        
        bytes32 message = keccak256("test message");
        bytes memory signature = new bytes(100);
        bytes memory pubkey = new bytes(MLDSA_PUBKEY_LEN);
        
        vm.expectRevert("Invalid ML-DSA signature length");
        settlement.addVerifiedMLDSACommitment(message, signature, pubkey);
        
        vm.stopPrank();
    }
    
    function test_AddVerifiedMLDSACommitment_InvalidPubkeyLength() public {
        vm.startPrank(owner);
        
        bytes32 message = keccak256("test message");
        bytes memory signature = new bytes(MLDSA_SIGNATURE_LEN);
        bytes memory pubkey = new bytes(100);
        
        vm.expectRevert("Invalid ML-DSA pubkey length");
        settlement.addVerifiedMLDSACommitment(message, signature, pubkey);
        
        vm.stopPrank();
    }
    
    function test_BatchAddVerifiedMLDSACommitments() public {
        vm.startPrank(owner);
        
        bytes32[] memory messages = new bytes32[](3);
        bytes[] memory signatures = new bytes[](3);
        bytes[] memory pubkeys = new bytes[](3);
        
        for (uint j = 0; j < 3; j++) {
            messages[j] = keccak256(abi.encodePacked("test message ", j));
            signatures[j] = new bytes(MLDSA_SIGNATURE_LEN);
            pubkeys[j] = new bytes(MLDSA_PUBKEY_LEN);
            
            for (uint i = 0; i < signatures[j].length; i++) {
                signatures[j][i] = bytes1(uint8((i + j * 17) % 256));
            }
            for (uint i = 0; i < pubkeys[j].length; i++) {
                pubkeys[j][i] = bytes1(uint8((i + 128 + j * 23) % 256));
            }
            
            bytes32 commitment = keccak256(abi.encodePacked(messages[j], signatures[j], pubkeys[j]));
            assertFalse(settlement.verifiedMLDSACommitments(commitment));
        }
        
        settlement.batchAddVerifiedMLDSACommitments(messages, signatures, pubkeys);
        
        for (uint j = 0; j < 3; j++) {
            bytes32 commitment = keccak256(abi.encodePacked(messages[j], signatures[j], pubkeys[j]));
            assertTrue(settlement.verifiedMLDSACommitments(commitment));
        }
        
        vm.stopPrank();
    }
    
    function test_BatchAddVerifiedMLDSACommitments_ArrayLengthMismatch() public {
        vm.startPrank(owner);
        
        bytes32[] memory messages = new bytes32[](3);
        bytes[] memory signatures = new bytes[](2);
        bytes[] memory pubkeys = new bytes[](3);
        
        vm.expectRevert("Array length mismatch");
        settlement.batchAddVerifiedMLDSACommitments(messages, signatures, pubkeys);
        
        vm.stopPrank();
    }
    
    function test_VerifyMLDSASignature_ValidCommitment() public {
        vm.startPrank(owner);
        
        bytes32 message = keccak256("test message");
        bytes memory signature = new bytes(MLDSA_SIGNATURE_LEN);
        bytes memory pubkey = new bytes(MLDSA_PUBKEY_LEN);
        
        for (uint i = 0; i < signature.length; i++) {
            signature[i] = bytes1(uint8(i % 256));
        }
        for (uint i = 0; i < pubkey.length; i++) {
            pubkey[i] = bytes1(uint8((i + 128) % 256));
        }
        
        settlement.addVerifiedMLDSACommitment(message, signature, pubkey);
        
        vm.stopPrank();
        
        bool result = settlement.verifyMLDSASignature(message, signature, pubkey);
        assertTrue(result);
    }
    
    function test_VerifyMLDSASignature_InvalidCommitment() public {
        bytes32 message = keccak256("test message");
        bytes memory signature = new bytes(MLDSA_SIGNATURE_LEN);
        bytes memory pubkey = new bytes(MLDSA_PUBKEY_LEN);
        
        bool result = settlement.verifyMLDSASignature(message, signature, pubkey);
        assertFalse(result);
    }
    
    function test_VerifyMLDSASignature_InvalidSignatureLength() public {
        bytes32 message = keccak256("test message");
        bytes memory signature = new bytes(100);
        bytes memory pubkey = new bytes(MLDSA_PUBKEY_LEN);
        
        vm.expectRevert("Invalid ML-DSA signature length");
        settlement.verifyMLDSASignature(message, signature, pubkey);
    }
    
    function test_VerifyMLDSASignature_InvalidPubkeyLength() public {
        bytes32 message = keccak256("test message");
        bytes memory signature = new bytes(MLDSA_SIGNATURE_LEN);
        bytes memory pubkey = new bytes(100);
        
        vm.expectRevert("Invalid ML-DSA pubkey length");
        settlement.verifyMLDSASignature(message, signature, pubkey);
    }

    function test_ApproveAndSubmitAnchor() public {
        uint256 blockHeight = 1;
        bytes32 stateRoot = keccak256("state root");
        bytes32 txHash = keccak256("l1 tx");

        vm.prank(verifier);
        settlement.approveAnchor(blockHeight, stateRoot, txHash);

        vm.prank(sequencer);
        settlement.submitAnchor(blockHeight, stateRoot, txHash, "");

        (bytes32 anchoredRoot,, bytes32 anchoredTx, bool verified) = settlement.getAnchor(blockHeight);
        assertEq(anchoredRoot, stateRoot);
        assertEq(anchoredTx, txHash);
        assertTrue(verified);
        assertEq(settlement.anchorProofHashes(blockHeight), keccak256(""));
    }

    function test_SubmitAnchorWithValidatorSignature() public {
        uint256 blockHeight = 1;
        bytes32 stateRoot = keccak256("signed state root");
        bytes32 txHash = keccak256("signed l1 tx");
        bytes memory proof = _anchorSignature(verifierKey, blockHeight, stateRoot, txHash);

        vm.prank(sequencer);
        settlement.submitAnchor(blockHeight, stateRoot, txHash, proof);

        (bytes32 anchoredRoot,, bytes32 anchoredTx, bool verified) = settlement.getAnchor(blockHeight);
        assertEq(anchoredRoot, stateRoot);
        assertEq(anchoredTx, txHash);
        assertTrue(verified);
        assertEq(settlement.anchorProofHashes(blockHeight), keccak256(proof));
    }

    function test_SubmitAnchorRequiresThreshold() public {
        address validator2 = vm.addr(validator2Key);

        vm.startPrank(owner);
        settlement.setAnchorValidator(validator2, true);
        settlement.setAnchorThreshold(2);
        vm.stopPrank();

        uint256 blockHeight = 1;
        bytes32 stateRoot = keccak256("threshold state root");
        bytes32 txHash = keccak256("threshold l1 tx");
        bytes memory proof1 = _anchorSignature(verifierKey, blockHeight, stateRoot, txHash);

        vm.expectRevert("Invalid proof");
        vm.prank(sequencer);
        settlement.submitAnchor(blockHeight, stateRoot, txHash, proof1);

        bytes memory proof2 = bytes.concat(
            proof1,
            _anchorSignature(validator2Key, blockHeight, stateRoot, txHash)
        );

        vm.prank(sequencer);
        settlement.submitAnchor(blockHeight, stateRoot, txHash, proof2);

        assertEq(settlement.latestAnchorHeight(), blockHeight);
    }
    
    function test_Pause() public {
        vm.startPrank(owner);
        settlement.pause();
        assertTrue(settlement.paused());
        vm.stopPrank();
    }
    
    function test_Unpause() public {
        vm.startPrank(owner);
        settlement.pause();
        settlement.unpause();
        assertFalse(settlement.paused());
        vm.stopPrank();
    }
    
    function test_UpdateBridge() public {
        address newBridge = address(0x6);
        
        vm.startPrank(owner);
        settlement.updateBridge(newBridge);
        assertEq(settlement.bhcBridge(), newBridge);
        vm.stopPrank();
    }
    
    function test_UpdateSequencer() public {
        address newSequencer = address(0x7);
        
        vm.startPrank(owner);
        settlement.updateSequencer(newSequencer);
        assertEq(settlement.l2Sequencer(), newSequencer);
        vm.stopPrank();
    }

    function _anchorSignature(
        uint256 key,
        uint256 blockHeight,
        bytes32 stateRoot,
        bytes32 txHash
    ) internal view returns (bytes memory) {
        bytes32 digest = keccak256(
            abi.encodePacked(settlement.DOMAIN_SEPARATOR(), blockHeight, stateRoot, txHash)
        );
        bytes32 ethSigned = keccak256(abi.encodePacked("\x19Ethereum Signed Message:\n32", digest));
        (uint8 v, bytes32 r, bytes32 s) = vm.sign(key, ethSigned);
        return abi.encodePacked(r, s, v);
    }
}
