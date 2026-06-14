// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// BHTE Configuration for Layer 2

package bhc

import (
	"time"
)

const (
	defaultMainnetMaxChannelAmount = uint64(1_000_000_000_000_000_000)
	defaultTestnetMaxChannelAmount = uint64(100_000_000_000_000_000)
	defaultDevMaxChannelAmount     = uint64(10_000_000_000_000_000)
)

type BHTEConfig struct {
	NetworkID uint64

	BHCNodeURL string

	AnchorCheckInterval time.Duration
	MinConfirmations    uint64
	MaxAnchorLatency    time.Duration

	ChannelTimeout   time.Duration
	ChallengePeriod  time.Duration
	MaxChannelAmount uint64

	MLDSAEnabled   bool
	MLDSAVerifyGas uint64

	CongestionThreshold float64
	MaxGasMultiplier    uint64

	KernelGasDiscount uint64
	UserGasMultiplier uint64

	BaseGasPrice uint64
}

func DefaultConfig() *BHTEConfig {
	return &BHTEConfig{
		NetworkID: 10000,

		BHCNodeURL: "http://127.0.0.1:18332",

		AnchorCheckInterval: 10 * time.Minute,
		MinConfirmations:    3,
		MaxAnchorLatency:    24 * time.Hour,

		ChannelTimeout:   24 * time.Hour,
		ChallengePeriod:  7 * 24 * time.Hour,
		MaxChannelAmount: defaultMainnetMaxChannelAmount,

		MLDSAEnabled:   true,
		MLDSAVerifyGas: 50000,

		CongestionThreshold: 0.3,
		MaxGasMultiplier:    16,

		KernelGasDiscount: 75,
		UserGasMultiplier: 100,

		BaseGasPrice: 10 * 1e9,
	}
}

func TestnetConfig() *BHTEConfig {
	return &BHTEConfig{
		NetworkID: 10001,

		BHCNodeURL: "http://127.0.0.1:28332",

		AnchorCheckInterval: 5 * time.Minute,
		MinConfirmations:    2,
		MaxAnchorLatency:    12 * time.Hour,

		ChannelTimeout:   12 * time.Hour,
		ChallengePeriod:  3 * 24 * time.Hour,
		MaxChannelAmount: defaultTestnetMaxChannelAmount,

		MLDSAEnabled:   true,
		MLDSAVerifyGas: 30000,

		CongestionThreshold: 0.4,
		MaxGasMultiplier:    8,

		KernelGasDiscount: 50,
		UserGasMultiplier: 100,

		BaseGasPrice: 1 * 1e9,
	}
}

func DevConfig() *BHTEConfig {
	return &BHTEConfig{
		NetworkID: 1337,

		BHCNodeURL: "http://127.0.0.1:38332",

		AnchorCheckInterval: 1 * time.Minute,
		MinConfirmations:    1,
		MaxAnchorLatency:    1 * time.Hour,

		ChannelTimeout:   1 * time.Hour,
		ChallengePeriod:  1 * time.Hour,
		MaxChannelAmount: defaultDevMaxChannelAmount,

		MLDSAEnabled:   false,
		MLDSAVerifyGas: 10000,

		CongestionThreshold: 0.5,
		MaxGasMultiplier:    4,

		KernelGasDiscount: 50,
		UserGasMultiplier: 100,

		BaseGasPrice: 0,
	}
}

func (c *BHTEConfig) IsMLDSAEnabled() bool {
	return c.MLDSAEnabled
}

func (c *BHTEConfig) GetAnchorCheckInterval() time.Duration {
	return c.AnchorCheckInterval
}

func (c *BHTEConfig) GetMinConfirmations() uint64 {
	return c.MinConfirmations
}

func (c *BHTEConfig) GetChallengePeriod() time.Duration {
	return c.ChallengePeriod
}

func (c *BHTEConfig) GetKernelGasPrice(baseGas uint64) uint64 {
	discount := 100 - c.KernelGasDiscount
	return baseGas * discount / 100
}
