// Copyright (c) 2026 BHC Developers
// Distributed under the MIT license
// BHTE Configuration for Layer 2

package bhc

import (
	"os"
	"strconv"
	"time"
)

const (
	defaultMainnetMaxChannelAmount = uint64(1_000_000_000_000_000_000)
	defaultTestnetMaxChannelAmount = uint64(100_000_000_000_000_000)
	defaultDevMaxChannelAmount     = uint64(10_000_000_000_000_000)
)

type BHTEConfig struct {
	NetworkID uint64

	BHCNodeURL      string
	BHCNodeUser     string
	BHCNodePassword string

	RPCEnabled    bool
	RPCPort       int
	RPCUser       string
	RPCPassword   string
	RPCTLSEnabled bool

	WSEnabled bool
	WSPort    int

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

	RateLimitEnabled   bool
	RateLimitReqPerSec int
	RateLimitBurst     int
}

func getEnv(key, defaultVal string) string {
	if val := os.Getenv(key); val != "" {
		return val
	}
	return defaultVal
}

func getEnvInt(key string, defaultVal int) int {
	if val := os.Getenv(key); val != "" {
		if intVal, err := strconv.Atoi(val); err == nil {
			return intVal
		}
	}
	return defaultVal
}

func getEnvUint64(key string, defaultVal uint64) uint64 {
	if val := os.Getenv(key); val != "" {
		if uintVal, err := strconv.ParseUint(val, 10, 64); err == nil {
			return uintVal
		}
	}
	return defaultVal
}

func getEnvBool(key string, defaultVal bool) bool {
	if val := os.Getenv(key); val != "" {
		if val == "1" || val == "true" || val == "TRUE" {
			return true
		} else if val == "0" || val == "false" || val == "FALSE" {
			return false
		}
	}
	return defaultVal
}

func getEnvDuration(key string, defaultVal time.Duration) time.Duration {
	if val := os.Getenv(key); val != "" {
		if dur, err := time.ParseDuration(val); err == nil {
			return dur
		}
	}
	return defaultVal
}

func DefaultConfig() *BHTEConfig {
	return &BHTEConfig{
		NetworkID: 10000,

		BHCNodeURL:      getEnv("BHC_NODE_URL", "http://127.0.0.1:18332"),
		BHCNodeUser:     getEnv("BHC_NODE_USER", ""),
		BHCNodePassword: getEnv("BHC_NODE_PASSWORD", ""),

		RPCEnabled:    true,
		RPCPort:       getEnvInt("BHC_RPC_PORT", 8545),
		RPCUser:       getEnv("BHC_RPC_USER", ""),
		RPCPassword:   getEnv("BHC_RPC_PASSWORD", ""),
		RPCTLSEnabled: getEnvBool("BHC_RPC_TLS", false),

		WSEnabled: true,
		WSPort:    getEnvInt("BHC_WS_PORT", 8546),

		AnchorCheckInterval: getEnvDuration("ANCHOR_CHECK_INTERVAL", 10*time.Minute),
		MinConfirmations:    getEnvUint64("MIN_CONFIRMATIONS", 3),
		MaxAnchorLatency:    getEnvDuration("MAX_ANCHOR_LATENCY", 24*time.Hour),

		ChannelTimeout:   getEnvDuration("CHANNEL_TIMEOUT", 24*time.Hour),
		ChallengePeriod:  getEnvDuration("CHALLENGE_PERIOD", 7*24*time.Hour),
		MaxChannelAmount: getEnvUint64("MAX_CHANNEL_AMOUNT", defaultMainnetMaxChannelAmount),

		MLDSAEnabled:   true,
		MLDSAVerifyGas: getEnvUint64("MLDSA_VERIFY_GAS", 50000),

		CongestionThreshold: 0.3,
		MaxGasMultiplier:    getEnvUint64("MAX_GAS_MULTIPLIER", 16),

		KernelGasDiscount: getEnvUint64("KERNEL_GAS_DISCOUNT", 75),
		UserGasMultiplier: getEnvUint64("USER_GAS_MULTIPLIER", 100),

		BaseGasPrice: getEnvUint64("BASE_GAS_PRICE", 10*1e9),

		RateLimitEnabled:   getEnvBool("RATE_LIMIT_ENABLED", true),
		RateLimitReqPerSec: getEnvInt("RATE_LIMIT_RPS", 100),
		RateLimitBurst:     getEnvInt("RATE_LIMIT_BURST", 200),
	}
}

func TestnetConfig() *BHTEConfig {
	return &BHTEConfig{
		NetworkID: 10001,

		BHCNodeURL:      getEnv("BHC_TESTNET_NODE_URL", "http://127.0.0.1:28332"),
		BHCNodeUser:     getEnv("BHC_TESTNET_NODE_USER", ""),
		BHCNodePassword: getEnv("BHC_TESTNET_NODE_PASSWORD", ""),

		RPCEnabled:    true,
		RPCPort:       getEnvInt("BHC_TESTNET_RPC_PORT", 18545),
		RPCUser:       getEnv("BHC_TESTNET_RPC_USER", ""),
		RPCPassword:   getEnv("BHC_TESTNET_RPC_PASSWORD", ""),
		RPCTLSEnabled: getEnvBool("BHC_TESTNET_RPC_TLS", false),

		WSEnabled: true,
		WSPort:    getEnvInt("BHC_TESTNET_WS_PORT", 18546),

		AnchorCheckInterval: getEnvDuration("ANCHOR_CHECK_INTERVAL", 5*time.Minute),
		MinConfirmations:    getEnvUint64("MIN_CONFIRMATIONS", 2),
		MaxAnchorLatency:    getEnvDuration("MAX_ANCHOR_LATENCY", 12*time.Hour),

		ChannelTimeout:   getEnvDuration("CHANNEL_TIMEOUT", 12*time.Hour),
		ChallengePeriod:  getEnvDuration("CHALLENGE_PERIOD", 3*24*time.Hour),
		MaxChannelAmount: getEnvUint64("MAX_CHANNEL_AMOUNT", defaultTestnetMaxChannelAmount),

		MLDSAEnabled:   true,
		MLDSAVerifyGas: getEnvUint64("MLDSA_VERIFY_GAS", 30000),

		CongestionThreshold: 0.4,
		MaxGasMultiplier:    getEnvUint64("MAX_GAS_MULTIPLIER", 8),

		KernelGasDiscount: getEnvUint64("KERNEL_GAS_DISCOUNT", 50),
		UserGasMultiplier: getEnvUint64("USER_GAS_MULTIPLIER", 100),

		BaseGasPrice: getEnvUint64("BASE_GAS_PRICE", 1*1e9),

		RateLimitEnabled:   getEnvBool("RATE_LIMIT_ENABLED", true),
		RateLimitReqPerSec: getEnvInt("RATE_LIMIT_RPS", 200),
		RateLimitBurst:     getEnvInt("RATE_LIMIT_BURST", 400),
	}
}

func DevConfig() *BHTEConfig {
	return &BHTEConfig{
		NetworkID: 1337,

		BHCNodeURL:      getEnv("BHC_DEV_NODE_URL", "http://127.0.0.1:38332"),
		BHCNodeUser:     "",
		BHCNodePassword: "",

		RPCEnabled:    true,
		RPCPort:       getEnvInt("BHC_DEV_RPC_PORT", 38545),
		RPCUser:       "",
		RPCPassword:   "",
		RPCTLSEnabled: false,

		WSEnabled: true,
		WSPort:    getEnvInt("BHC_DEV_WS_PORT", 38546),

		AnchorCheckInterval: getEnvDuration("ANCHOR_CHECK_INTERVAL", 1*time.Minute),
		MinConfirmations:    getEnvUint64("MIN_CONFIRMATIONS", 1),
		MaxAnchorLatency:    getEnvDuration("MAX_ANCHOR_LATENCY", 1*time.Hour),

		ChannelTimeout:   getEnvDuration("CHANNEL_TIMEOUT", 1*time.Hour),
		ChallengePeriod:  getEnvDuration("CHALLENGE_PERIOD", 1*time.Hour),
		MaxChannelAmount: getEnvUint64("MAX_CHANNEL_AMOUNT", defaultDevMaxChannelAmount),

		MLDSAEnabled:   false,
		MLDSAVerifyGas: getEnvUint64("MLDSA_VERIFY_GAS", 10000),

		CongestionThreshold: 0.5,
		MaxGasMultiplier:    getEnvUint64("MAX_GAS_MULTIPLIER", 4),

		KernelGasDiscount: getEnvUint64("KERNEL_GAS_DISCOUNT", 50),
		UserGasMultiplier: getEnvUint64("USER_GAS_MULTIPLIER", 100),

		BaseGasPrice: 0,

		RateLimitEnabled:   false,
		RateLimitReqPerSec: 1000,
		RateLimitBurst:     2000,
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

func (c *BHTEConfig) HasRPCAuth() bool {
	return c.RPCUser != "" && c.RPCPassword != ""
}

func (c *BHTEConfig) HasBHCNodeAuth() bool {
	return c.BHCNodeUser != "" && c.BHCNodePassword != ""
}
