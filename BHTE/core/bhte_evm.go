// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// BHTE Modified EVM - Dual-State Execution Environment

package bhc

import (
	"fmt"
	"math/big"
	"sync"

	consensusbhc "bhte/consensus/bhc"
	bhtevm "bhte/core/vm"

	gethvm "github.com/ethereum/go-ethereum/core/vm"
)

type BHTEVM struct {
	gethvm.EVM
	gasRegulator   *bhtevm.GasRegulator
	anchorVerifier *consensusbhc.AnchorVerifier
	channelManager *consensusbhc.ChannelManager
	executionState bhtevm.ExecutionState
	mu             sync.RWMutex
}

func NewBHTEVM(vmConfig gethvm.Config, chainConfig interface{}, gasPrice *big.Int, chainContext interface{}) *BHTEVM {
	baseGas := uint64(21000)
	gasRegulator := bhtevm.NewGasRegulator(baseGas)

	return &BHTEVM{
		gasRegulator:   gasRegulator,
		anchorVerifier: consensusbhc.NewAnchorVerifier(""),
		channelManager: consensusbhc.NewChannelManager(),
		executionState: bhtevm.ExecutionStateKernel,
	}
}

func (e *BHTEVM) ExecuteKernelOp(op bhtevm.OpCode) error {
	switch op {
	case bhtevm.OP_KERNEL_TRANSFER:
		return e.executeTransfer()
	case bhtevm.OP_KERNEL_MULTISIG:
		return e.executeMultisig()
	case bhtevm.OP_KERNEL_CHANNEL_OPEN:
		return e.executeChannelOpen()
	case bhtevm.OP_KERNEL_CHANNEL_CLOSE:
		return e.executeChannelClose()
	case bhtevm.OP_KERNEL_ANCHOR:
		return e.executeAnchorVerify()
	case bhtevm.OP_KERNEL_MLDSA:
		return e.executeMLDSAVerify()
	case bhtevm.OP_KERNEL_WITHDRAW:
		return e.executeWithdraw()
	case bhtevm.OP_KERNEL_BATCH_TRANSFER:
		return e.executeBatchTransfer()
	default:
		return fmt.Errorf("unknown kernel opcode: %s", bhtevm.GetKernelOpcodeName(op))
	}
}

func (e *BHTEVM) executeTransfer() error {
	return nil
}

func (e *BHTEVM) executeMultisig() error {
	return nil
}

func (e *BHTEVM) executeChannelOpen() error {
	return nil
}

func (e *BHTEVM) executeChannelClose() error {
	return nil
}

func (e *BHTEVM) executeAnchorVerify() error {
	return nil
}

func (e *BHTEVM) executeMLDSAVerify() error {
	return nil
}

func (e *BHTEVM) executeWithdraw() error {
	return nil
}

func (e *BHTEVM) executeBatchTransfer() error {
	return nil
}

func (e *BHTEVM) SetExecutionState(state bhtevm.ExecutionState) {
	e.mu.Lock()
	defer e.mu.Unlock()
	e.executionState = state
}

func (e *BHTEVM) GetExecutionState() bhtevm.ExecutionState {
	e.mu.RLock()
	defer e.mu.RUnlock()
	return e.executionState
}

func (e *BHTEVM) IsKernelMode() bool {
	return e.executionState == bhtevm.ExecutionStateKernel
}

func (e *BHTEVM) GetGasRegulator() *bhtevm.GasRegulator {
	return e.gasRegulator
}

func (e *BHTEVM) GetAnchorVerifier() *consensusbhc.AnchorVerifier {
	return e.anchorVerifier
}

func (e *BHTEVM) GetChannelManager() *consensusbhc.ChannelManager {
	return e.channelManager
}

func (e *BHTEVM) RecordAndCalculateGas(op bhtevm.OpCode, baseGas uint64) uint64 {
	e.gasRegulator.RecordOp(op)
	return e.gasRegulator.CalculateGas(op, baseGas)
}

func (e *BHTEVM) GetCongestionLevel() uint64 {
	return e.gasRegulator.GetCongestionLevel()
}
