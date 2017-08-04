#include "pos.h"

// #include "primitives/block.h"
// #include "streams.h"

#include "consensus/merkle.h"
#include "wallet/wallet.h"
#include "arith_uint256.h"
#include "chainparams.h"
#include "validation.h"
#include "coins.h"
#include "timedata.h"
#include "util.h"

#include <vector>

/* References:
 * - http://earlz.net/view/2017/07/27/1904/the-missing-explanation-of-proof-of-stake-version
 * - https://blackcoin.co/blackcoin-pos-protocol-v2-whitepaper.pdf
 * - http://ppcsuite.github.io/images/peercoin-pos-diagram.html
 */

uint256 CPosKernel::ComputeHash() {
    CDataStream ss(SER_GETHASH, 0);
    ss << bnStakeModifier << nPrevTime << hashPrevout << nPrevoutN << nTime;
    return Hash(ss.begin(), ss.end());
}

uint256 ComputeStakeModifier(const CBlockIndex* pindexPrev, const uint256& kernel) {
    if (!pindexPrev)
        return uint256(); // genesis block's modifier is 0

    CDataStream ss(SER_GETHASH, 0);
    ss << kernel << pindexPrev->bnStakeModifier;
    return Hash(ss.begin(), ss.end());
}

// TODO: add "return error"s
boost::optional<std::tuple<CTransaction, CPosKernel>> FindValidKernel(uint32_t nBits) {
    assert(pwalletMain != NULL);

    // Get wallet's UTXOs
    std::vector<COutput> vecOutputs;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    pwalletMain->AvailableCoins(vecOutputs, true, NULL, false);

    CPosKernel kernel;
    for (const COutput& out : vecOutputs) {
        if (out.nDepth <= MIN_STAKE_TX_DEPTH
                || out.tx->nIndex < 0)
            continue;

        CTransactionRef utxoRef = out.tx->tx;
        CTransaction utxo = *utxoRef;
        const COutPoint& prevout = utxo.vin[0].prevout;

        CBlockIndex* blockIndexPrev = mapBlockIndex[out.tx->hashBlock];
        int64_t nValueIn = utxo.vout[out.tx->nIndex].nValue;

        auto current_time = GetAdjustedTime();
        kernel.nTime = current_time;
        kernel.nPrevTime = blockIndexPrev->nTime;
        kernel.nPrevoutN = prevout.n;
        kernel.hashPrevout = prevout.hash;
        kernel.bnStakeModifier = ComputeStakeModifier(blockIndexPrev, prevout.hash);

        arith_uint256 kernelHash = UintToArith256(kernel.ComputeHash());

        arith_uint256 bnTarget;
        bnTarget.SetCompact(nBits);
        bnTarget *= nValueIn;

        if (kernelHash < bnTarget) {
            return std::make_tuple(utxo, kernel);
        }
    }

    return boost::none;
}

/**
 * Add Stake transaction to the block
 */
void SignPosBlock(CBlock* pblock, CTransaction coinsTx, CPosKernel kernel) {
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);

    // Create stake transaction
    CMutableTransaction stakeTx;
    stakeTx.vin.resize(1);
    stakeTx.vout.resize(2);
    stakeTx.vin[0] = CTxIn(coinsTx.GetHash(), 0); // FIXME: real nOut
    stakeTx.vout[0].nValue = 0;
    stakeTx.vout[1].nValue = GetBlockSubsidy(chainActive.Height() + 1, Params().GetConsensus());
    stakeTx.vout[1].scriptPubKey = CScript() << OP_RETURN;

    pblock->nTime = kernel.nTime;
    pblock->vtx.insert(pblock->vtx.begin() + 1, MakeTransactionRef(std::move(stakeTx)));
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}

bool CheckPosBlock(const CBlock& block) {
    return true;
}

bool CheckPosBlockHeader(const CBlockIndex& blockIndex) {
    return true;
}

// ppcoin: find last block index up to pindex
const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex) {
    while (pindex && pindex->pprev)
        pindex = pindex->pprev;
    return pindex;
}

static const int64_t nTargetTimespan = 16 * 60;  // 16 mins

uint32_t GetNextPosTargetRequired(const CBlockIndex* pindexLast) {
//    arith_uint256 bnTargetLimit = bnProofOfStakeLimit;
    arith_uint256 bnTargetLimit = UintToArith256(Params().GetConsensus().powLimit);
    bnTargetLimit.SetCompact(pindexLast->nBits);

    if (pindexLast == NULL)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast);
    if (pindexPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* pindexPrevPrev = GetLastBlockIndex(pindexPrev->pprev);
    if (pindexPrevPrev->pprev == NULL)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nTargetSpacing = 64;
    int64_t nActualSpacing = pindexPrev->GetBlockTime() - pindexPrevPrev->GetBlockTime();
    if (nActualSpacing > nTargetSpacing * 10)
        nActualSpacing = nTargetSpacing * 10;

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexPrev->nBits);
    int64_t nInterval = nTargetTimespan / nTargetSpacing;
    bnNew *= ((nInterval - 1) * nTargetSpacing + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * nTargetSpacing);

    if (bnNew <= 0 || bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}
