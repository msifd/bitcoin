#ifndef POS_H
#define POS_H

#include "chain.h"
#include "arith_uint256.h"
#include "primitives/transaction.h"

static const int POS_BLOCKS_HEIGHT = 20;
static const int MIN_STAKE_TX_DEPTH = 5; // 500+
static const int STAKE_TIMESTAMP_MASK = 15; // Supposed to be 2^n-1

static arith_uint256 bnProofOfStakeLimit(~arith_uint256(0) >> 48);

struct CPosKernel {
    uint256 bnStakeModifier;
    uint256 hashPrevout;
    uint32_t nPrevoutN;
    uint64_t nPrevTime;
    uint64_t nTime;

    uint256 ComputeHash();
};

bool FindValidKernel(uint32_t nBits, CTransaction* stakeInputTx, CPosKernel* validKernel);

bool SignPosBlock(CBlock* pblock, CTransaction* coinsTx, CPosKernel* kernel);

#endif // POS_H
