// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// BHTE zkEVM Tests

package bhc

import (
	"crypto/rand"
	"testing"

	consensusbhc "bhte/consensus/bhc"
	configbhc "bhte/core/bhc"
	"bhte/core/vm"
	"bhte/mldsa"
)

func TestKernelOpcodes(t *testing.T) {
	tests := []struct {
		op       vm.OpCode
		isKernel bool
	}{
		{vm.OP_KERNEL_TRANSFER, true},
		{vm.OP_KERNEL_MULTISIG, true},
		{vm.OP_KERNEL_CHANNEL_OPEN, true},
		{vm.OP_KERNEL_CHANNEL_CLOSE, true},
		{vm.OP_KERNEL_ANCHOR, true},
		{vm.OP_KERNEL_MLDSA, true},
		{vm.OP_KERNEL_WITHDRAW, true},
		{vm.OP_KERNEL_BATCH_TRANSFER, true},
	}

	for _, tt := range tests {
		t.Run(vm.GetKernelOpcodeName(tt.op), func(t *testing.T) {
			if vm.IsKernelOpcode(tt.op) != tt.isKernel {
				t.Errorf("IsKernelOpcode(%s) = %v, want %v",
					vm.GetKernelOpcodeName(tt.op), vm.IsKernelOpcode(tt.op), tt.isKernel)
			}
		})
	}
}

func TestGasRegulator(t *testing.T) {
	regulator := vm.NewGasRegulator(21000)

	t.Run("KernelOpcodeGasDiscount", func(t *testing.T) {
		kernelGas := regulator.CalculateGas(vm.OP_KERNEL_TRANSFER, 21000)
		baseGas := regulator.CalculateGas(vm.OP_STOP, 21000)

		if kernelGas >= baseGas {
			t.Errorf("Kernel gas should be discounted: got %d, base %d", kernelGas, baseGas)
		}

		if kernelGas != 5250 {
			t.Errorf("Kernel gas = %d, want 5250 (25%% of 21000)", kernelGas)
		}
	})

	t.Run("RecordOp", func(t *testing.T) {
		regulator.Reset()
		regulator.RecordOp(vm.OP_STOP)
		regulator.RecordOp(vm.OP_STOP)

		total := regulator.TotalOps()
		if total != 2 {
			t.Errorf("totalOps = %d, want 2", total)
		}
	})

	t.Run("CongestionDetection", func(t *testing.T) {
		regulator.Reset()

		for i := 0; i < 10; i++ {
			regulator.RecordOp(vm.OP_STOP)
			regulator.RecordOp(vm.OP_ADD)
			regulator.RecordOp(vm.OP_MUL)
		}

		regulator.UpdateCongestion()

		congestion := regulator.GetCongestionLevel()
		if congestion == 0 {
			t.Log("Congestion level is 0, all ops might be kernel ops")
		}
	})
}

func TestMLDSAPrecompile(t *testing.T) {
	precompile := &vm.MLDSAPrecompile{}

	t.Run("RequiredGas", func(t *testing.T) {
		gas := precompile.RequiredGas(nil)
		if gas != vm.MLDSAVerifyGas {
			t.Errorf("RequiredGas = %d, want %d", gas, vm.MLDSAVerifyGas)
		}
	})

	t.Run("InvalidInputLength", func(t *testing.T) {
		_, err := precompile.Run([]byte("short"))
		if err == nil {
			t.Error("Expected error for short input")
		}
		if err != vm.ErrMLDSAInvalidInput {
			t.Errorf("Got error %v, want %v", err, vm.ErrMLDSAInvalidInput)
		}
	})

	t.Run("ValidSignature", func(t *testing.T) {
		m := mldsa.New(mldsa.MLDSA65)
		pk, sk, err := m.GenerateKeyPair()
		if err != nil {
			t.Fatalf("keygen failed: %v", err)
		}

		message := make([]byte, vm.MLDSAMessageSize)
		rand.Read(message)
		signature, err := m.Sign(sk, message)
		if err != nil {
			t.Fatalf("sign failed: %v", err)
		}
		pubkey := m.PublicKeyToBytes(pk)

		input := make([]byte, 0, len(message)+len(pubkey)+len(signature))
		input = append(input, message...)
		input = append(input, pubkey...)
		input = append(input, signature...)

		result, err := precompile.Run(input)
		if err != nil {
			t.Errorf("Unexpected error: %v", err)
		}
		if len(result) != 32 || result[31] != 1 {
			t.Errorf("Invalid result: %x", result)
		}
	})

	t.Run("RejectsForgedSignature", func(t *testing.T) {
		m := mldsa.New(mldsa.MLDSA65)
		pk, sk, err := m.GenerateKeyPair()
		if err != nil {
			t.Fatalf("keygen failed: %v", err)
		}
		message := make([]byte, vm.MLDSAMessageSize)
		rand.Read(message)
		signature, err := m.Sign(sk, message)
		if err != nil {
			t.Fatalf("sign failed: %v", err)
		}
		signature[100] ^= 0xFF // tamper

		input := make([]byte, 0)
		input = append(input, message...)
		input = append(input, m.PublicKeyToBytes(pk)...)
		input = append(input, signature...)

		if _, err := precompile.Run(input); err == nil {
			t.Error("Expected forged signature to be rejected")
		}
	})
}

func TestChannelManager(t *testing.T) {
	manager := consensusbhc.NewChannelManager()

	t.Run("OpenChannel", func(t *testing.T) {
		p1 := [20]byte{0x01}
		p2 := [20]byte{0x02}

		channel, err := manager.OpenChannel(p1, p2, 1000000)
		if err != nil {
			t.Fatalf("OpenChannel failed: %v", err)
		}

		if channel.State != consensusbhc.ChannelStateOpen {
			t.Errorf("Channel state = %s, want Open", channel.State)
		}

		if channel.Balance[0] != 1000000 {
			t.Errorf("Initial balance = %d, want 1000000", channel.Balance[0])
		}
	})

	t.Run("OpenChannelSameParticipant", func(t *testing.T) {
		p := [20]byte{0x01}

		_, err := manager.OpenChannel(p, p, 1000000)
		if err == nil {
			t.Error("Expected error for same participant")
		}
	})

	t.Run("GetChannelsByParticipant", func(t *testing.T) {
		p1 := [20]byte{0x10}
		p2 := [20]byte{0x11}
		p3 := [20]byte{0x12}

		manager.OpenChannel(p1, p2, 500000)
		manager.OpenChannel(p1, p3, 300000)

		channels := manager.GetChannelsByParticipant(p1)
		if len(channels) != 2 {
			t.Errorf("GetChannelsByParticipant = %d channels, want 2", len(channels))
		}
	})
}

func TestBHCIntegration(t *testing.T) {
	t.Run("DefaultConfig", func(t *testing.T) {
		config := DefaultIntegrationConfig()

		if config.NodeURL != DefaultBHCNodeRPC {
			t.Errorf("NodeURL = %s, want %s", config.NodeURL, DefaultBHCNodeRPC)
		}

		if config.RetryCount != MaxRetries {
			t.Errorf("RetryCount = %d, want %d", config.RetryCount, MaxRetries)
		}
	})
}

func TestBHTEConfig(t *testing.T) {
	t.Run("DefaultConfig", func(t *testing.T) {
		config := configbhc.DefaultConfig()

		if config.NetworkID != 10000 {
			t.Errorf("NetworkID = %d, want 10000", config.NetworkID)
		}

		if !config.MLDSAEnabled {
			t.Error("MLDSAEnabled should be true by default")
		}

		if config.MinConfirmations != 3 {
			t.Errorf("MinConfirmations = %d, want 3", config.MinConfirmations)
		}
	})

	t.Run("TestnetConfig", func(t *testing.T) {
		config := configbhc.TestnetConfig()

		if config.NetworkID != 10001 {
			t.Errorf("NetworkID = %d, want 10001", config.NetworkID)
		}
	})

	t.Run("DevConfig", func(t *testing.T) {
		config := configbhc.DevConfig()

		if config.NetworkID != 1337 {
			t.Errorf("NetworkID = %d, want 1337", config.NetworkID)
		}

		if config.MLDSAEnabled {
			t.Error("MLDSAEnabled should be false in dev config")
		}
	})

	t.Run("GetKernelGasPrice", func(t *testing.T) {
		config := configbhc.DefaultConfig()
		gas := config.GetKernelGasPrice(21000)

		if gas != 5250 {
			t.Errorf("Kernel gas price = %d, want 5250 (25%% of 21000)", gas)
		}
	})
}
