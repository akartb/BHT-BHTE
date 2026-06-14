// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// BHTE Gas Regulator - Dual-State Execution Environment

package vm

import (
	"sync/atomic"
)

type ExecutionState uint8

const (
	ExecutionStateKernel ExecutionState = iota
	ExecutionStateUser
)

type GasRegulator struct {
	baseGasPrice      uint64
	kernelGasPrice    uint64
	userGasMultiplier atomic.Uint64
	congestionLevel   atomic.Uint64
	maxMultiplier     uint64
	nonKernelOps      atomic.Uint64
	totalOps          atomic.Uint64
}

func NewGasRegulator(baseGas uint64) *GasRegulator {
	return &GasRegulator{
		baseGasPrice:   baseGas,
		kernelGasPrice: baseGas / 4,
		maxMultiplier:  16,
	}
}

func (r *GasRegulator) CalculateGas(op OpCode, baseGas uint64) uint64 {
	if IsKernelOpcode(op) {
		return r.kernelGasPrice * baseGas / r.baseGasPrice
	}

	multiplier := r.userGasMultiplier.Load()
	if multiplier < 100 {
		multiplier = 100
	}

	return baseGas * multiplier / 100
}

func (r *GasRegulator) RecordOp(op OpCode) {
	r.totalOps.Add(1)
	if !IsKernelOpcode(op) {
		r.nonKernelOps.Add(1)
	}
}

func (r *GasRegulator) UpdateCongestion() {
	total := r.totalOps.Load()
	if total == 0 {
		return
	}

	nonKernel := r.nonKernelOps.Load()
	nonKernelRatio := float64(nonKernel) / float64(total)

	if nonKernelRatio > 0.3 {
		newMultiplier := uint64(nonKernelRatio * 100 / 0.3)
		if newMultiplier > r.maxMultiplier*100 {
			newMultiplier = r.maxMultiplier * 100
		}
		r.userGasMultiplier.Store(newMultiplier)
		r.congestionLevel.Store(uint64(nonKernelRatio * 100))
	} else {
		r.userGasMultiplier.Store(100)
		r.congestionLevel.Store(0)
	}

	r.totalOps.Store(0)
	r.nonKernelOps.Store(0)
}

func (r *GasRegulator) GetCongestionLevel() uint64 {
	return r.congestionLevel.Load()
}

func (r *GasRegulator) GetGasMultiplier() uint64 {
	return r.userGasMultiplier.Load()
}

func (r *GasRegulator) TotalOps() uint64 {
	return r.totalOps.Load()
}

func (r *GasRegulator) IsKernelMode(op OpCode) bool {
	return IsKernelOpcode(op)
}

func (r *GasRegulator) Reset() {
	r.userGasMultiplier.Store(100)
	r.congestionLevel.Store(0)
	r.totalOps.Store(0)
	r.nonKernelOps.Store(0)
}
