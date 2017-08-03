#include "pos.h"

// #include "primitives/block.h"
// #include "streams.h"

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
bool FindValidKernel(uint32_t nBits, CTransaction* stakeCoinsTx, CPosKernel* validKernel) {
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

        const CTransaction& utxo = *out.tx->tx;
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
//        auto target = ArithToUint256(bnTarget);

        if (kernelHash < bnTarget) {
            stakeCoinsTx = const_cast<CTransaction*>(&utxo);
            validKernel = const_cast<CPosKernel*>(&kernel);
            return true;
        }
        printf("FindValidKernel: Invalid hash: %s\n", kernelHash.ToString().c_str());
    }

    return false;
}

/**
 * Add Stake transaction to the block
 */
bool SignPosBlock(CBlock* pblock, CTransaction* coinsTx, CPosKernel* kernel) {
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);

    // Create stake transaction
    CMutableTransaction stakeTx;
    stakeTx.vin.resize(1);
    stakeTx.vout.resize(2);
    stakeTx.vin[0] = CTxIn(coinsTx->GetHash(), 0); // FIXME: real nOut
    stakeTx.vout[1].scriptPubKey = CScript() << OP_RETURN;

    pblock->nTime = kernel->nTime;
    pblock->vtx.insert(pblock->vtx.begin() + 1, MakeTransactionRef(std::move(stakeTx)));
}