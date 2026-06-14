// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// BHTE Kernel Opcodes - Dual-State Execution Environment

package vm

import "fmt"

type OpCode byte

const (
	OP_STOP OpCode = 0x00
	OP_ADD  OpCode = 0x01
	OP_MUL  OpCode = 0x02

	OP_KERNEL_TRANSFER       OpCode = 0xB0
	OP_KERNEL_MULTISIG       OpCode = 0xB1
	OP_KERNEL_CHANNEL_OPEN   OpCode = 0xB2
	OP_KERNEL_CHANNEL_CLOSE  OpCode = 0xB3
	OP_KERNEL_ANCHOR         OpCode = 0xB4
	OP_KERNEL_MLDSA          OpCode = 0xB5
	OP_KERNEL_WITHDRAW       OpCode = 0xB6
	OP_KERNEL_BATCH_TRANSFER OpCode = 0xB7
)

var KernelOpcodes = map[OpCode]bool{
	OP_KERNEL_TRANSFER:       true,
	OP_KERNEL_MULTISIG:       true,
	OP_KERNEL_CHANNEL_OPEN:   true,
	OP_KERNEL_CHANNEL_CLOSE:  true,
	OP_KERNEL_ANCHOR:         true,
	OP_KERNEL_MLDSA:          true,
	OP_KERNEL_WITHDRAW:       true,
	OP_KERNEL_BATCH_TRANSFER: true,
}

var KernelOpcodeNames = map[OpCode]string{
	OP_KERNEL_TRANSFER:       "KERNEL_TRANSFER",
	OP_KERNEL_MULTISIG:       "KERNEL_MULTISIG",
	OP_KERNEL_CHANNEL_OPEN:   "KERNEL_CHANNEL_OPEN",
	OP_KERNEL_CHANNEL_CLOSE:  "KERNEL_CHANNEL_CLOSE",
	OP_KERNEL_ANCHOR:         "KERNEL_ANCHOR",
	OP_KERNEL_MLDSA:          "KERNEL_MLDSA",
	OP_KERNEL_WITHDRAW:       "KERNEL_WITHDRAW",
	OP_KERNEL_BATCH_TRANSFER: "KERNEL_BATCH_TRANSFER",
}

func IsKernelOpcode(op OpCode) bool {
	return KernelOpcodes[op]
}

func GetKernelOpcodeName(op OpCode) string {
	if name, ok := KernelOpcodeNames[op]; ok {
		return name
	}
	return op.String()
}

func (op OpCode) String() string {
	switch op {
	case OP_STOP:
		return "STOP"
	case OP_ADD:
		return "ADD"
	case OP_MUL:
		return "MUL"
	default:
		return fmt.Sprintf("0x%02x", byte(op))
	}
}
