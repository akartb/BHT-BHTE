// Copyright (c) 2026 BHC Developers
// Distributed under the MIT software license
// Modified PoW Implementation with ProgPoW and LWMA

#include "pow.h"
#include "crypto/progpow.h"

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include <util/check.h>
#include <logging.h>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast,
                                  const CBlockHeader* pblock,
                                  const Consensus::Params& params) {
    assert(pindexLast != nullptr);
    
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();
    
    if ((pindexLast->nHeight + 1) % params.DifficultyAdjustmentInterval() != 0) {
        if (params.fPowAllowMinDifficultyBlocks) {
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing * 2) {
                return nProofOfWorkLimit;
            } else {
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && 
                       pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && 
                       pindex->nBits == nProofOfWorkLimit) {
                    pindex = pindex->pprev;
                }
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }
    
    return CalculateNextWorkRequiredLWMA(pindexLast, params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast,
                                        int64_t nFirstBlockTime,
                                        const Consensus::Params& params) {
    if (params.fPowNoRetargeting) {
        return pindexLast->nBits;
    }
    
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan / 4) {
        nActualTimespan = params.nPowTargetTimespan / 4;
    }
    if (nActualTimespan > params.nPowTargetTimespan * 4) {
        nActualTimespan = params.nPowTargetTimespan * 4;
    }
    
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;
    
    if (bnNew > bnPowLimit) {
        bnNew = bnPowLimit;
    }
    
    return bnNew.GetCompact();
}

unsigned int CalculateNextWorkRequiredLWMA(const CBlockIndex* pindexLast,
                                           const Consensus::Params& params) {
    if (params.fPowNoRetargeting) {
        return pindexLast->nBits;
    }
    
    return LWMA::Calculate(pindexLast, params);
}

unsigned int LWMA::Calculate(const CBlockIndex* pindexLast,
                             const Consensus::Params& params) {
    const arith_uint256 powLimit = UintToArith256(params.powLimit);
    
    if (pindexLast == nullptr || pindexLast->nHeight < WINDOW_SIZE) {
        return powLimit.GetCompact();
    }
    
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; i < WINDOW_SIZE - 1; ++i) {
        if (pindexFirst->pprev == nullptr) {
            return powLimit.GetCompact();
        }
        pindexFirst = pindexFirst->pprev;
    }
    
    int64_t totalTimespan = 0;
    int64_t totalWeight = 0;
    int64_t weight = WINDOW_SIZE;
    
    const CBlockIndex* pindex = pindexLast;
    const CBlockIndex* pindexPrev = pindex->pprev;
    
    for (int i = 0; i < WINDOW_SIZE - 1 && pindexPrev != nullptr; ++i) {
        int64_t timespan = pindex->GetBlockTime() - pindexPrev->GetBlockTime();
        
        if (timespan < MIN_ACTUAL_TIMESPAN) {
            timespan = MIN_ACTUAL_TIMESPAN;
        }
        if (timespan > MAX_ACTUAL_TIMESPAN) {
            timespan = MAX_ACTUAL_TIMESPAN;
        }
        
        totalTimespan += timespan * weight;
        totalWeight += weight;
        
        weight--;
        pindex = pindexPrev;
        pindexPrev = pindex->pprev;
    }
    
    if (totalWeight == 0) {
        return powLimit.GetCompact();
    }
    
    int64_t avgTimespan = totalTimespan / totalWeight;

    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);

    const int64_t MIN_TIMESPAN = 1;
    const int64_t MAX_TIMESPAN = 3600;

    if (avgTimespan < MIN_TIMESPAN) {
        avgTimespan = MIN_TIMESPAN;
    } else if (avgTimespan > MAX_TIMESPAN) {
        avgTimespan = MAX_TIMESPAN;
    }

    bnNew *= TARGET_SPACING;
    bnNew /= avgTimespan;
    
    if (bnNew > powLimit) {
        bnNew = powLimit;
    }
    
    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits,
                      const Consensus::Params& params) {
    auto bnTarget = DeriveTarget(nBits, params.powLimit);
    if (!bnTarget) {
        return false;
    }
    
    if (UintToArith256(hash) > bnTarget) {
        return false;
    }
    
    return true;
}

bool CheckProofOfWorkProgPoW(const CBlockHeader& block,
                             uint64_t block_number,
                             const Consensus::Params& params) {
    auto bnTarget = DeriveTarget(block.nBits, params.powLimit);
    if (!bnTarget) {
        LogPrintf("ProgPoW: Invalid target bits\n");
        return false;
    }
    
    uint256 headerHash = block.GetHash();
    uint64_t nonce = block.nNonce;
    
    uint256 powHash = bhc::crypto::ProgPoWHash(
        *reinterpret_cast<bhc::crypto::uint256*>(&headerHash),
        nonce,
        block_number
    );
    
    arith_uint256 hashArith = UintToArith256(powHash);
    
    if (hashArith > bnTarget) {
        LogPrintf("ProgPoW: Hash exceeds target\n");
        return false;
    }
    
    return true;
}

std::optional<arith_uint256> DeriveTarget(unsigned int nBits, uint256 pow_limit) {
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;
    
    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);
    
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(pow_limit)) {
        return {};
    }
    
    return bnTarget;
}

bool PermittedDifficultyTransition(const Consensus::Params& params,
                                   int64_t height,
                                   uint32_t old_nbits,
                                   uint32_t new_nbits) {
    if (params.fPowAllowMinDifficultyBlocks) {
        return true;
    }
    
    if (height % params.DifficultyAdjustmentInterval() == 0) {
        int64_t smallest_timespan = params.nPowTargetTimespan / 4;
        int64_t largest_timespan = params.nPowTargetTimespan * 4;
        
        const arith_uint256 pow_limit = UintToArith256(params.powLimit);
        arith_uint256 observed_new_target;
        observed_new_target.SetCompact(new_nbits);
        
        arith_uint256 largest_difficulty_target;
        largest_difficulty_target.SetCompact(old_nbits);
        largest_difficulty_target *= largest_timespan;
        largest_difficulty_target /= params.nPowTargetTimespan;
        
        if (largest_difficulty_target > pow_limit) {
            largest_difficulty_target = pow_limit;
        }
        
        arith_uint256 maximum_new_target;
        maximum_new_target.SetCompact(largest_difficulty_target.GetCompact());
        if (maximum_new_target < observed_new_target) {
            return false;
        }
        
        arith_uint256 smallest_difficulty_target;
        smallest_difficulty_target.SetCompact(old_nbits);
        smallest_difficulty_target *= smallest_timespan;
        smallest_difficulty_target /= params.nPowTargetTimespan;
        
        if (smallest_difficulty_target > pow_limit) {
            smallest_difficulty_target = pow_limit;
        }
        
        arith_uint256 minimum_new_target;
        minimum_new_target.SetCompact(smallest_difficulty_target.GetCompact());
        if (minimum_new_target > observed_new_target) {
            return false;
        }
    } else if (old_nbits != new_nbits) {
        return false;
    }
    
    return true;
}
