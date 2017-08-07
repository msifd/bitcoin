#ifndef POS_H
#define POS_H

#include "chain.h"
#include "arith_uint256.h"
#include "primitives/transaction.h"
#include "wallet/wallet.h"

#include <tuple>
#include <boost/optional.hpp>

static const int POS_BLOCKS_HEIGHT = 20;
static const int MIN_STAKE_TX_DEPTH = 5; // 500+
static const int STAKE_TIMESTAMP_MASK = 15; // Supposed to be 2^n-1

//static arith_uint256 bnProofOfStakeLimit(~arith_uint256(0) >> 10);

struct CPosKernel {
    uint256 bnStakeModifier;
    uint256 hashPrevout;
    uint32_t nPrevoutN;
    uint32_t nPrevTime;
    uint32_t nTime;

    uint256 ComputeHash();
};

boost::optional<std::tuple<COutput, CPosKernel>> FindValidKernel(uint32_t nBits);

void SignPosBlock(CBlock* pblock, CTransaction coinsTx, CPosKernel kernel);

bool CheckPosBlock(const CBlock& block);

bool CheckPosBlockHeader(const CBlockIndex& blockIndex);

uint32_t GetNextPosTargetRequired(const CBlockIndex* pindexLast);

#endif // POS_H
