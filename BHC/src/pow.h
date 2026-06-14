// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// Modified PoW Logic with ProgPoW Integration

#ifndef BHC_POW_H
#define BHC_POW_H

#include <cstdint>
#include <optional>
#include <consensus/params.h>

class CBlockHeader;
class CBlockIndex;
class uint256;
class arith_uint256;

std::optional<arith_uint256> DeriveTarget(unsigned int nBits, uint256 pow_limit);

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, 
                                  const CBlockHeader* pblock,
                                  const Consensus::Params& params);

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast,
                                        int64_t nFirstBlockTime,
                                        const Consensus::Params& params);

unsigned int CalculateNextWorkRequiredLWMA(const CBlockIndex* pindexLast,
                                           const Consensus::Params& params);

bool CheckProofOfWork(uint256 hash, unsigned int nBits, 
                      const Consensus::Params& params);

bool CheckProofOfWorkProgPoW(const CBlockHeader& block,
                             uint64_t block_number,
                             const Consensus::Params& params);

bool PermittedDifficultyTransition(const Consensus::Params& params,
                                   int64_t height,
                                   uint32_t old_nbits,
                                   uint32_t new_nbits);

namespace LWMA {
    constexpr int64_t TARGET_SPACING = 600;
    constexpr int WINDOW_SIZE = 45;
    constexpr int64_t MIN_ACTUAL_TIMESPAN = 60;
    constexpr int64_t MAX_ACTUAL_TIMESPAN = 3600;
    
    unsigned int Calculate(const CBlockIndex* pindexLast,
                           const Consensus::Params& params);
}

#endif
