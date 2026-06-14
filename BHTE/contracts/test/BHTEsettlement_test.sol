// SPDX-License-Identifier: MIT
// Copyright (c) 2026 BHC Developers
// BHTE Settlement Contract Tests

pragma solidity ^0.8.24;

import "forge-std/Test.sol";
import "../BHTEsettlement.sol";

contract BHTEsettlementTest is Test {
    BHTEsettlement public settlement;
    
    address public owner = address(0x1);
    address public bridge = address(0x2);
    address public sequencer = address(0x3);
    address public verifier = address(0x4);
    address public user = address(0x5);
    
    function setUp() public {
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
    }
    
    function test_AddVerifiedMLDSACommitment() public {
        vm.startPrank(owner);
        
        bytes32 message = keccak256("test message");
        bytes memory signature = new bytes(3293);
        bytes memory pubkey = new bytes(1952);
        
        for (uint i = 0; i < 3293; i++) {
            signature[i] = bytes1(uint8(i % 256));
        }
        for (uint i = 0; i < 1952; i++) {
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
        bytes memory signature = new bytes(3293);
        bytes memory pubkey = new bytes(1952);
        
        vm.expectRevert("Ownable: caller is not the owner");
        vm.prank(user);
        settlement.addVerifiedMLDSACommitment(message, signature, pubkey);
    }
    
    function test_AddVerifiedMLDSACommitment_InvalidSignatureLength() public {
        vm.startPrank(owner);
        
        bytes32 message = keccak256("test message");
        bytes memory signature = new bytes(100);
        bytes memory pubkey = new bytes(1952);
        
        vm.expectRevert("Invalid ML-DSA signature length");
        settlement.addVerifiedMLDSACommitment(message, signature, pubkey);
        
        vm.stopPrank();
    }
    
    function test_AddVerifiedMLDSACommitment_InvalidPubkeyLength() public {
        vm.startPrank(owner);
        
        bytes32 message = keccak256("test message");
        bytes memory signature = new bytes(3293);
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
            signatures[j] = new bytes(3293);
            pubkeys[j] = new bytes(1952);
            
            for (uint i = 0; i < 3293; i++) {
                signatures[j][i] = bytes1(uint8((i + j * 17) % 256));
            }
            for (uint i = 0; i < 1952; i++) {
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
        bytes memory signature = new bytes(3293);
        bytes memory pubkey = new bytes(1952);
        
        for (uint i = 0; i < 3293; i++) {
            signature[i] = bytes1(uint8(i % 256));
        }
        for (uint i = 0; i < 1952; i++) {
            pubkey[i] = bytes1(uint8((i + 128) % 256));
        }
        
        settlement.addVerifiedMLDSACommitment(message, signature, pubkey);
        
        vm.stopPrank();
        
        bool result = settlement.verifyMLDSASignature(message, signature, pubkey);
        assertTrue(result);
    }
    
    function test_VerifyMLDSASignature_InvalidCommitment() public {
        bytes32 message = keccak256("test message");
        bytes memory signature = new bytes(3293);
        bytes memory pubkey = new bytes(1952);
        
        bool result = settlement.verifyMLDSASignature(message, signature, pubkey);
        assertFalse(result);
    }
    
    function test_VerifyMLDSASignature_InvalidSignatureLength() public {
        bytes32 message = keccak256("test message");
        bytes memory signature = new bytes(100);
        bytes memory pubkey = new bytes(1952);
        
        vm.expectRevert("Invalid ML-DSA signature length");
        settlement.verifyMLDSASignature(message, signature, pubkey);
    }
    
    function test_VerifyMLDSASignature_InvalidPubkeyLength() public {
        bytes32 message = keccak256("test message");
        bytes memory signature = new bytes(3293);
        bytes memory pubkey = new bytes(100);
        
        vm.expectRevert("Invalid ML-DSA pubkey length");
        settlement.verifyMLDSASignature(message, signature, pubkey);
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
}
