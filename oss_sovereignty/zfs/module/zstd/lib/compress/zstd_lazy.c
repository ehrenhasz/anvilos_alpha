#include "zstd_compress_internal.h"
#include "zstd_lazy.h"
static void
ZSTD_updateDUBT(ZSTD_matchState_t* ms,
                const BYTE* ip, const BYTE* iend,
                U32 mls)
{
    const ZSTD_compressionParameters* const cParams = &ms->cParams;
    U32* const hashTable = ms->hashTable;
    U32  const hashLog = cParams->hashLog;
    U32* const bt = ms->chainTable;
    U32  const btLog  = cParams->chainLog - 1;
    U32  const btMask = (1 << btLog) - 1;
    const BYTE* const base = ms->window.base;
    U32 const target = (U32)(ip - base);
    U32 idx = ms->nextToUpdate;
    if (idx != target)
        DEBUGLOG(7, "ZSTD_updateDUBT, from %u to %u (dictLimit:%u)",
                    idx, target, ms->window.dictLimit);
    assert(ip + 8 <= iend);    
    (void)iend;
    assert(idx >= ms->window.dictLimit);    
    for ( ; idx < target ; idx++) {
        size_t const h  = ZSTD_hashPtr(base + idx, hashLog, mls);    
        U32    const matchIndex = hashTable[h];
        U32*   const nextCandidatePtr = bt + 2*(idx&btMask);
        U32*   const sortMarkPtr  = nextCandidatePtr + 1;
        DEBUGLOG(8, "ZSTD_updateDUBT: insert %u", idx);
        hashTable[h] = idx;    
        *nextCandidatePtr = matchIndex;    
        *sortMarkPtr = ZSTD_DUBT_UNSORTED_MARK;
    }
    ms->nextToUpdate = target;
}
static void
ZSTD_insertDUBT1(ZSTD_matchState_t* ms,
                 U32 current, const BYTE* inputEnd,
                 U32 nbCompares, U32 btLow,
                 const ZSTD_dictMode_e dictMode)
{
    const ZSTD_compressionParameters* const cParams = &ms->cParams;
    U32* const bt = ms->chainTable;
    U32  const btLog  = cParams->chainLog - 1;
    U32  const btMask = (1 << btLog) - 1;
    size_t commonLengthSmaller=0, commonLengthLarger=0;
    const BYTE* const base = ms->window.base;
    const BYTE* const dictBase = ms->window.dictBase;
    const U32 dictLimit = ms->window.dictLimit;
    const BYTE* const ip = (current>=dictLimit) ? base + current : dictBase + current;
    const BYTE* const iend = (current>=dictLimit) ? inputEnd : dictBase + dictLimit;
    const BYTE* const dictEnd = dictBase + dictLimit;
    const BYTE* const prefixStart = base + dictLimit;
    const BYTE* match;
    U32* smallerPtr = bt + 2*(current&btMask);
    U32* largerPtr  = smallerPtr + 1;
    U32 matchIndex = *smallerPtr;    
    U32 dummy32;    
    U32 const windowValid = ms->window.lowLimit;
    U32 const maxDistance = 1U << cParams->windowLog;
    U32 const windowLow = (current - windowValid > maxDistance) ? current - maxDistance : windowValid;
    DEBUGLOG(8, "ZSTD_insertDUBT1(%u) (dictLimit=%u, lowLimit=%u)",
                current, dictLimit, windowLow);
    assert(current >= btLow);
    assert(ip < iend);    
    while (nbCompares-- && (matchIndex > windowLow)) {
        U32* const nextPtr = bt + 2*(matchIndex & btMask);
        size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);    
        assert(matchIndex < current);
        if ( (dictMode != ZSTD_extDict)
          || (matchIndex+matchLength >= dictLimit)   
          || (current < dictLimit)  ) {
            const BYTE* const mBase = ( (dictMode != ZSTD_extDict)
                                     || (matchIndex+matchLength >= dictLimit)) ?
                                        base : dictBase;
            assert( (matchIndex+matchLength >= dictLimit)    
                 || (current < dictLimit) );
            match = mBase + matchIndex;
            matchLength += ZSTD_count(ip+matchLength, match+matchLength, iend);
        } else {
            match = dictBase + matchIndex;
            matchLength += ZSTD_count_2segments(ip+matchLength, match+matchLength, iend, dictEnd, prefixStart);
            if (matchIndex+matchLength >= dictLimit)
                match = base + matchIndex;    
        }
        DEBUGLOG(8, "ZSTD_insertDUBT1: comparing %u with %u : found %u common bytes ",
                    current, matchIndex, (U32)matchLength);
        if (ip+matchLength == iend) {    
            break;    
        }
        if (match[matchLength] < ip[matchLength]) {   
            *smallerPtr = matchIndex;              
            commonLengthSmaller = matchLength;     
            if (matchIndex <= btLow) { smallerPtr=&dummy32; break; }    
            DEBUGLOG(8, "ZSTD_insertDUBT1: %u (>btLow=%u) is smaller : next => %u",
                        matchIndex, btLow, nextPtr[1]);
            smallerPtr = nextPtr+1;                
            matchIndex = nextPtr[1];               
        } else {
            *largerPtr = matchIndex;
            commonLengthLarger = matchLength;
            if (matchIndex <= btLow) { largerPtr=&dummy32; break; }    
            DEBUGLOG(8, "ZSTD_insertDUBT1: %u (>btLow=%u) is larger => %u",
                        matchIndex, btLow, nextPtr[0]);
            largerPtr = nextPtr;
            matchIndex = nextPtr[0];
    }   }
    *smallerPtr = *largerPtr = 0;
}
static size_t
ZSTD_DUBT_findBetterDictMatch (
        ZSTD_matchState_t* ms,
        const BYTE* const ip, const BYTE* const iend,
        size_t* offsetPtr,
        size_t bestLength,
        U32 nbCompares,
        U32 const mls,
        const ZSTD_dictMode_e dictMode)
{
    const ZSTD_matchState_t * const dms = ms->dictMatchState;
    const ZSTD_compressionParameters* const dmsCParams = &dms->cParams;
    const U32 * const dictHashTable = dms->hashTable;
    U32         const hashLog = dmsCParams->hashLog;
    size_t      const h  = ZSTD_hashPtr(ip, hashLog, mls);
    U32               dictMatchIndex = dictHashTable[h];
    const BYTE* const base = ms->window.base;
    const BYTE* const prefixStart = base + ms->window.dictLimit;
    U32         const current = (U32)(ip-base);
    const BYTE* const dictBase = dms->window.base;
    const BYTE* const dictEnd = dms->window.nextSrc;
    U32         const dictHighLimit = (U32)(dms->window.nextSrc - dms->window.base);
    U32         const dictLowLimit = dms->window.lowLimit;
    U32         const dictIndexDelta = ms->window.lowLimit - dictHighLimit;
    U32*        const dictBt = dms->chainTable;
    U32         const btLog  = dmsCParams->chainLog - 1;
    U32         const btMask = (1 << btLog) - 1;
    U32         const btLow = (btMask >= dictHighLimit - dictLowLimit) ? dictLowLimit : dictHighLimit - btMask;
    size_t commonLengthSmaller=0, commonLengthLarger=0;
    (void)dictMode;
    assert(dictMode == ZSTD_dictMatchState);
    while (nbCompares-- && (dictMatchIndex > dictLowLimit)) {
        U32* const nextPtr = dictBt + 2*(dictMatchIndex & btMask);
        size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);    
        const BYTE* match = dictBase + dictMatchIndex;
        matchLength += ZSTD_count_2segments(ip+matchLength, match+matchLength, iend, dictEnd, prefixStart);
        if (dictMatchIndex+matchLength >= dictHighLimit)
            match = base + dictMatchIndex + dictIndexDelta;    
        if (matchLength > bestLength) {
            U32 matchIndex = dictMatchIndex + dictIndexDelta;
            if ( (4*(int)(matchLength-bestLength)) > (int)(ZSTD_highbit32(current-matchIndex+1) - ZSTD_highbit32((U32)offsetPtr[0]+1)) ) {
                DEBUGLOG(9, "ZSTD_DUBT_findBetterDictMatch(%u) : found better match length %u -> %u and offsetCode %u -> %u (dictMatchIndex %u, matchIndex %u)",
                    current, (U32)bestLength, (U32)matchLength, (U32)*offsetPtr, ZSTD_REP_MOVE + current - matchIndex, dictMatchIndex, matchIndex);
                bestLength = matchLength, *offsetPtr = ZSTD_REP_MOVE + current - matchIndex;
            }
            if (ip+matchLength == iend) {    
                break;    
            }
        }
        if (match[matchLength] < ip[matchLength]) {
            if (dictMatchIndex <= btLow) { break; }    
            commonLengthSmaller = matchLength;     
            dictMatchIndex = nextPtr[1];               
        } else {
            if (dictMatchIndex <= btLow) { break; }    
            commonLengthLarger = matchLength;
            dictMatchIndex = nextPtr[0];
        }
    }
    if (bestLength >= MINMATCH) {
        U32 const mIndex = current - ((U32)*offsetPtr - ZSTD_REP_MOVE); (void)mIndex;
        DEBUGLOG(8, "ZSTD_DUBT_findBetterDictMatch(%u) : found match of length %u and offsetCode %u (pos %u)",
                    current, (U32)bestLength, (U32)*offsetPtr, mIndex);
    }
    return bestLength;
}
static size_t
ZSTD_DUBT_findBestMatch(ZSTD_matchState_t* ms,
                        const BYTE* const ip, const BYTE* const iend,
                        size_t* offsetPtr,
                        U32 const mls,
                        const ZSTD_dictMode_e dictMode)
{
    const ZSTD_compressionParameters* const cParams = &ms->cParams;
    U32*   const hashTable = ms->hashTable;
    U32    const hashLog = cParams->hashLog;
    size_t const h  = ZSTD_hashPtr(ip, hashLog, mls);
    U32          matchIndex  = hashTable[h];
    const BYTE* const base = ms->window.base;
    U32    const current = (U32)(ip-base);
    U32    const windowLow = ZSTD_getLowestMatchIndex(ms, current, cParams->windowLog);
    U32*   const bt = ms->chainTable;
    U32    const btLog  = cParams->chainLog - 1;
    U32    const btMask = (1 << btLog) - 1;
    U32    const btLow = (btMask >= current) ? 0 : current - btMask;
    U32    const unsortLimit = MAX(btLow, windowLow);
    U32*         nextCandidate = bt + 2*(matchIndex&btMask);
    U32*         unsortedMark = bt + 2*(matchIndex&btMask) + 1;
    U32          nbCompares = 1U << cParams->searchLog;
    U32          nbCandidates = nbCompares;
    U32          previousCandidate = 0;
    DEBUGLOG(7, "ZSTD_DUBT_findBestMatch (%u) ", current);
    assert(ip <= iend-8);    
    while ( (matchIndex > unsortLimit)
         && (*unsortedMark == ZSTD_DUBT_UNSORTED_MARK)
         && (nbCandidates > 1) ) {
        DEBUGLOG(8, "ZSTD_DUBT_findBestMatch: candidate %u is unsorted",
                    matchIndex);
        *unsortedMark = previousCandidate;   
        previousCandidate = matchIndex;
        matchIndex = *nextCandidate;
        nextCandidate = bt + 2*(matchIndex&btMask);
        unsortedMark = bt + 2*(matchIndex&btMask) + 1;
        nbCandidates --;
    }
    if ( (matchIndex > unsortLimit)
      && (*unsortedMark==ZSTD_DUBT_UNSORTED_MARK) ) {
        DEBUGLOG(7, "ZSTD_DUBT_findBestMatch: nullify last unsorted candidate %u",
                    matchIndex);
        *nextCandidate = *unsortedMark = 0;
    }
    matchIndex = previousCandidate;
    while (matchIndex) {   
        U32* const nextCandidateIdxPtr = bt + 2*(matchIndex&btMask) + 1;
        U32 const nextCandidateIdx = *nextCandidateIdxPtr;
        ZSTD_insertDUBT1(ms, matchIndex, iend,
                         nbCandidates, unsortLimit, dictMode);
        matchIndex = nextCandidateIdx;
        nbCandidates++;
    }
    {   size_t commonLengthSmaller = 0, commonLengthLarger = 0;
        const BYTE* const dictBase = ms->window.dictBase;
        const U32 dictLimit = ms->window.dictLimit;
        const BYTE* const dictEnd = dictBase + dictLimit;
        const BYTE* const prefixStart = base + dictLimit;
        U32* smallerPtr = bt + 2*(current&btMask);
        U32* largerPtr  = bt + 2*(current&btMask) + 1;
        U32 matchEndIdx = current + 8 + 1;
        U32 dummy32;    
        size_t bestLength = 0;
        matchIndex  = hashTable[h];
        hashTable[h] = current;    
        while (nbCompares-- && (matchIndex > windowLow)) {
            U32* const nextPtr = bt + 2*(matchIndex & btMask);
            size_t matchLength = MIN(commonLengthSmaller, commonLengthLarger);    
            const BYTE* match;
            if ((dictMode != ZSTD_extDict) || (matchIndex+matchLength >= dictLimit)) {
                match = base + matchIndex;
                matchLength += ZSTD_count(ip+matchLength, match+matchLength, iend);
            } else {
                match = dictBase + matchIndex;
                matchLength += ZSTD_count_2segments(ip+matchLength, match+matchLength, iend, dictEnd, prefixStart);
                if (matchIndex+matchLength >= dictLimit)
                    match = base + matchIndex;    
            }
            if (matchLength > bestLength) {
                if (matchLength > matchEndIdx - matchIndex)
                    matchEndIdx = matchIndex + (U32)matchLength;
                if ( (4*(int)(matchLength-bestLength)) > (int)(ZSTD_highbit32(current-matchIndex+1) - ZSTD_highbit32((U32)offsetPtr[0]+1)) )
                    bestLength = matchLength, *offsetPtr = ZSTD_REP_MOVE + current - matchIndex;
                if (ip+matchLength == iend) {    
                    if (dictMode == ZSTD_dictMatchState) {
                        nbCompares = 0;  
                    }
                    break;    
                }
            }
            if (match[matchLength] < ip[matchLength]) {
                *smallerPtr = matchIndex;              
                commonLengthSmaller = matchLength;     
                if (matchIndex <= btLow) { smallerPtr=&dummy32; break; }    
                smallerPtr = nextPtr+1;                
                matchIndex = nextPtr[1];               
            } else {
                *largerPtr = matchIndex;
                commonLengthLarger = matchLength;
                if (matchIndex <= btLow) { largerPtr=&dummy32; break; }    
                largerPtr = nextPtr;
                matchIndex = nextPtr[0];
        }   }
        *smallerPtr = *largerPtr = 0;
        if (dictMode == ZSTD_dictMatchState && nbCompares) {
            bestLength = ZSTD_DUBT_findBetterDictMatch(
                    ms, ip, iend,
                    offsetPtr, bestLength, nbCompares,
                    mls, dictMode);
        }
        assert(matchEndIdx > current+8);  
        ms->nextToUpdate = matchEndIdx - 8;    
        if (bestLength >= MINMATCH) {
            U32 const mIndex = current - ((U32)*offsetPtr - ZSTD_REP_MOVE); (void)mIndex;
            DEBUGLOG(8, "ZSTD_DUBT_findBestMatch(%u) : found match of length %u and offsetCode %u (pos %u)",
                        current, (U32)bestLength, (U32)*offsetPtr, mIndex);
        }
        return bestLength;
    }
}
FORCE_INLINE_TEMPLATE size_t
ZSTD_BtFindBestMatch( ZSTD_matchState_t* ms,
                const BYTE* const ip, const BYTE* const iLimit,
                      size_t* offsetPtr,
                const U32 mls  ,
                const ZSTD_dictMode_e dictMode)
{
    DEBUGLOG(7, "ZSTD_BtFindBestMatch");
    if (ip < ms->window.base + ms->nextToUpdate) return 0;    
    ZSTD_updateDUBT(ms, ip, iLimit, mls);
    return ZSTD_DUBT_findBestMatch(ms, ip, iLimit, offsetPtr, mls, dictMode);
}
static size_t
ZSTD_BtFindBestMatch_selectMLS (  ZSTD_matchState_t* ms,
                            const BYTE* ip, const BYTE* const iLimit,
                                  size_t* offsetPtr)
{
    switch(ms->cParams.minMatch)
    {
    default :  
    case 4 : return ZSTD_BtFindBestMatch(ms, ip, iLimit, offsetPtr, 4, ZSTD_noDict);
    case 5 : return ZSTD_BtFindBestMatch(ms, ip, iLimit, offsetPtr, 5, ZSTD_noDict);
    case 7 :
    case 6 : return ZSTD_BtFindBestMatch(ms, ip, iLimit, offsetPtr, 6, ZSTD_noDict);
    }
}
static size_t ZSTD_BtFindBestMatch_dictMatchState_selectMLS (
                        ZSTD_matchState_t* ms,
                        const BYTE* ip, const BYTE* const iLimit,
                        size_t* offsetPtr)
{
    switch(ms->cParams.minMatch)
    {
    default :  
    case 4 : return ZSTD_BtFindBestMatch(ms, ip, iLimit, offsetPtr, 4, ZSTD_dictMatchState);
    case 5 : return ZSTD_BtFindBestMatch(ms, ip, iLimit, offsetPtr, 5, ZSTD_dictMatchState);
    case 7 :
    case 6 : return ZSTD_BtFindBestMatch(ms, ip, iLimit, offsetPtr, 6, ZSTD_dictMatchState);
    }
}
static size_t ZSTD_BtFindBestMatch_extDict_selectMLS (
                        ZSTD_matchState_t* ms,
                        const BYTE* ip, const BYTE* const iLimit,
                        size_t* offsetPtr)
{
    switch(ms->cParams.minMatch)
    {
    default :  
    case 4 : return ZSTD_BtFindBestMatch(ms, ip, iLimit, offsetPtr, 4, ZSTD_extDict);
    case 5 : return ZSTD_BtFindBestMatch(ms, ip, iLimit, offsetPtr, 5, ZSTD_extDict);
    case 7 :
    case 6 : return ZSTD_BtFindBestMatch(ms, ip, iLimit, offsetPtr, 6, ZSTD_extDict);
    }
}
#define NEXT_IN_CHAIN(d, mask)   chainTable[(d) & (mask)]
static U32 ZSTD_insertAndFindFirstIndex_internal(
                        ZSTD_matchState_t* ms,
                        const ZSTD_compressionParameters* const cParams,
                        const BYTE* ip, U32 const mls)
{
    U32* const hashTable  = ms->hashTable;
    const U32 hashLog = cParams->hashLog;
    U32* const chainTable = ms->chainTable;
    const U32 chainMask = (1 << cParams->chainLog) - 1;
    const BYTE* const base = ms->window.base;
    const U32 target = (U32)(ip - base);
    U32 idx = ms->nextToUpdate;
    while(idx < target) {  
        size_t const h = ZSTD_hashPtr(base+idx, hashLog, mls);
        NEXT_IN_CHAIN(idx, chainMask) = hashTable[h];
        hashTable[h] = idx;
        idx++;
    }
    ms->nextToUpdate = target;
    return hashTable[ZSTD_hashPtr(ip, hashLog, mls)];
}
U32 ZSTD_insertAndFindFirstIndex(ZSTD_matchState_t* ms, const BYTE* ip) {
    const ZSTD_compressionParameters* const cParams = &ms->cParams;
    return ZSTD_insertAndFindFirstIndex_internal(ms, cParams, ip, ms->cParams.minMatch);
}
FORCE_INLINE_TEMPLATE
size_t ZSTD_HcFindBestMatch_generic (
                        ZSTD_matchState_t* ms,
                        const BYTE* const ip, const BYTE* const iLimit,
                        size_t* offsetPtr,
                        const U32 mls, const ZSTD_dictMode_e dictMode)
{
    const ZSTD_compressionParameters* const cParams = &ms->cParams;
    U32* const chainTable = ms->chainTable;
    const U32 chainSize = (1 << cParams->chainLog);
    const U32 chainMask = chainSize-1;
    const BYTE* const base = ms->window.base;
    const BYTE* const dictBase = ms->window.dictBase;
    const U32 dictLimit = ms->window.dictLimit;
    const BYTE* const prefixStart = base + dictLimit;
    const BYTE* const dictEnd = dictBase + dictLimit;
    const U32 current = (U32)(ip-base);
    const U32 maxDistance = 1U << cParams->windowLog;
    const U32 lowestValid = ms->window.lowLimit;
    const U32 withinMaxDistance = (current - lowestValid > maxDistance) ? current - maxDistance : lowestValid;
    const U32 isDictionary = (ms->loadedDictEnd != 0);
    const U32 lowLimit = isDictionary ? lowestValid : withinMaxDistance;
    const U32 minChain = current > chainSize ? current - chainSize : 0;
    U32 nbAttempts = 1U << cParams->searchLog;
    size_t ml=4-1;
    U32 matchIndex = ZSTD_insertAndFindFirstIndex_internal(ms, cParams, ip, mls);
    for ( ; (matchIndex>lowLimit) & (nbAttempts>0) ; nbAttempts--) {
        size_t currentMl=0;
        if ((dictMode != ZSTD_extDict) || matchIndex >= dictLimit) {
            const BYTE* const match = base + matchIndex;
            assert(matchIndex >= dictLimit);    
            if (match[ml] == ip[ml])    
                currentMl = ZSTD_count(ip, match, iLimit);
        } else {
            const BYTE* const match = dictBase + matchIndex;
            assert(match+4 <= dictEnd);
            if (MEM_read32(match) == MEM_read32(ip))    
                currentMl = ZSTD_count_2segments(ip+4, match+4, iLimit, dictEnd, prefixStart) + 4;
        }
        if (currentMl > ml) {
            ml = currentMl;
            *offsetPtr = current - matchIndex + ZSTD_REP_MOVE;
            if (ip+currentMl == iLimit) break;  
        }
        if (matchIndex <= minChain) break;
        matchIndex = NEXT_IN_CHAIN(matchIndex, chainMask);
    }
    if (dictMode == ZSTD_dictMatchState) {
        const ZSTD_matchState_t* const dms = ms->dictMatchState;
        const U32* const dmsChainTable = dms->chainTable;
        const U32 dmsChainSize         = (1 << dms->cParams.chainLog);
        const U32 dmsChainMask         = dmsChainSize - 1;
        const U32 dmsLowestIndex       = dms->window.dictLimit;
        const BYTE* const dmsBase      = dms->window.base;
        const BYTE* const dmsEnd       = dms->window.nextSrc;
        const U32 dmsSize              = (U32)(dmsEnd - dmsBase);
        const U32 dmsIndexDelta        = dictLimit - dmsSize;
        const U32 dmsMinChain = dmsSize > dmsChainSize ? dmsSize - dmsChainSize : 0;
        matchIndex = dms->hashTable[ZSTD_hashPtr(ip, dms->cParams.hashLog, mls)];
        for ( ; (matchIndex>dmsLowestIndex) & (nbAttempts>0) ; nbAttempts--) {
            size_t currentMl=0;
            const BYTE* const match = dmsBase + matchIndex;
            assert(match+4 <= dmsEnd);
            if (MEM_read32(match) == MEM_read32(ip))    
                currentMl = ZSTD_count_2segments(ip+4, match+4, iLimit, dmsEnd, prefixStart) + 4;
            if (currentMl > ml) {
                ml = currentMl;
                *offsetPtr = current - (matchIndex + dmsIndexDelta) + ZSTD_REP_MOVE;
                if (ip+currentMl == iLimit) break;  
            }
            if (matchIndex <= dmsMinChain) break;
            matchIndex = dmsChainTable[matchIndex & dmsChainMask];
        }
    }
    return ml;
}
FORCE_INLINE_TEMPLATE size_t ZSTD_HcFindBestMatch_selectMLS (
                        ZSTD_matchState_t* ms,
                        const BYTE* ip, const BYTE* const iLimit,
                        size_t* offsetPtr)
{
    switch(ms->cParams.minMatch)
    {
    default :  
    case 4 : return ZSTD_HcFindBestMatch_generic(ms, ip, iLimit, offsetPtr, 4, ZSTD_noDict);
    case 5 : return ZSTD_HcFindBestMatch_generic(ms, ip, iLimit, offsetPtr, 5, ZSTD_noDict);
    case 7 :
    case 6 : return ZSTD_HcFindBestMatch_generic(ms, ip, iLimit, offsetPtr, 6, ZSTD_noDict);
    }
}
static size_t ZSTD_HcFindBestMatch_dictMatchState_selectMLS (
                        ZSTD_matchState_t* ms,
                        const BYTE* ip, const BYTE* const iLimit,
                        size_t* offsetPtr)
{
    switch(ms->cParams.minMatch)
    {
    default :  
    case 4 : return ZSTD_HcFindBestMatch_generic(ms, ip, iLimit, offsetPtr, 4, ZSTD_dictMatchState);
    case 5 : return ZSTD_HcFindBestMatch_generic(ms, ip, iLimit, offsetPtr, 5, ZSTD_dictMatchState);
    case 7 :
    case 6 : return ZSTD_HcFindBestMatch_generic(ms, ip, iLimit, offsetPtr, 6, ZSTD_dictMatchState);
    }
}
FORCE_INLINE_TEMPLATE size_t ZSTD_HcFindBestMatch_extDict_selectMLS (
                        ZSTD_matchState_t* ms,
                        const BYTE* ip, const BYTE* const iLimit,
                        size_t* offsetPtr)
{
    switch(ms->cParams.minMatch)
    {
    default :  
    case 4 : return ZSTD_HcFindBestMatch_generic(ms, ip, iLimit, offsetPtr, 4, ZSTD_extDict);
    case 5 : return ZSTD_HcFindBestMatch_generic(ms, ip, iLimit, offsetPtr, 5, ZSTD_extDict);
    case 7 :
    case 6 : return ZSTD_HcFindBestMatch_generic(ms, ip, iLimit, offsetPtr, 6, ZSTD_extDict);
    }
}
typedef enum { search_hashChain, search_binaryTree } searchMethod_e;
FORCE_INLINE_TEMPLATE size_t
ZSTD_compressBlock_lazy_generic(
                        ZSTD_matchState_t* ms, seqStore_t* seqStore,
                        U32 rep[ZSTD_REP_NUM],
                        const void* src, size_t srcSize,
                        const searchMethod_e searchMethod, const U32 depth,
                        ZSTD_dictMode_e const dictMode)
{
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;
    const BYTE* const base = ms->window.base;
    const U32 prefixLowestIndex = ms->window.dictLimit;
    const BYTE* const prefixLowest = base + prefixLowestIndex;
    typedef size_t (*searchMax_f)(
                        ZSTD_matchState_t* ms,
                        const BYTE* ip, const BYTE* iLimit, size_t* offsetPtr);
    searchMax_f const searchMax = dictMode == ZSTD_dictMatchState ?
        (searchMethod==search_binaryTree ? ZSTD_BtFindBestMatch_dictMatchState_selectMLS
                                         : ZSTD_HcFindBestMatch_dictMatchState_selectMLS) :
        (searchMethod==search_binaryTree ? ZSTD_BtFindBestMatch_selectMLS
                                         : ZSTD_HcFindBestMatch_selectMLS);
    U32 offset_1 = rep[0], offset_2 = rep[1], savedOffset=0;
    const ZSTD_matchState_t* const dms = ms->dictMatchState;
    const U32 dictLowestIndex      = dictMode == ZSTD_dictMatchState ?
                                     dms->window.dictLimit : 0;
    const BYTE* const dictBase     = dictMode == ZSTD_dictMatchState ?
                                     dms->window.base : NULL;
    const BYTE* const dictLowest   = dictMode == ZSTD_dictMatchState ?
                                     dictBase + dictLowestIndex : NULL;
    const BYTE* const dictEnd      = dictMode == ZSTD_dictMatchState ?
                                     dms->window.nextSrc : NULL;
    const U32 dictIndexDelta       = dictMode == ZSTD_dictMatchState ?
                                     prefixLowestIndex - (U32)(dictEnd - dictBase) :
                                     0;
    const U32 dictAndPrefixLength = (U32)((ip - prefixLowest) + (dictEnd - dictLowest));
    DEBUGLOG(5, "ZSTD_compressBlock_lazy_generic (dictMode=%u)", (U32)dictMode);
    ip += (dictAndPrefixLength == 0);
    if (dictMode == ZSTD_noDict) {
        U32 const current = (U32)(ip - base);
        U32 const windowLow = ZSTD_getLowestPrefixIndex(ms, current, ms->cParams.windowLog);
        U32 const maxRep = current - windowLow;
        if (offset_2 > maxRep) savedOffset = offset_2, offset_2 = 0;
        if (offset_1 > maxRep) savedOffset = offset_1, offset_1 = 0;
    }
    if (dictMode == ZSTD_dictMatchState) {
        assert(offset_1 <= dictAndPrefixLength);
        assert(offset_2 <= dictAndPrefixLength);
    }
#if defined(__GNUC__) && defined(__x86_64__)
    __asm__(".p2align 5");
#endif
    while (ip < ilimit) {
        size_t matchLength=0;
        size_t offset=0;
        const BYTE* start=ip+1;
        if (dictMode == ZSTD_dictMatchState) {
            const U32 repIndex = (U32)(ip - base) + 1 - offset_1;
            const BYTE* repMatch = (dictMode == ZSTD_dictMatchState
                                && repIndex < prefixLowestIndex) ?
                                   dictBase + (repIndex - dictIndexDelta) :
                                   base + repIndex;
            if (((U32)((prefixLowestIndex-1) - repIndex) >= 3  )
                && (MEM_read32(repMatch) == MEM_read32(ip+1)) ) {
                const BYTE* repMatchEnd = repIndex < prefixLowestIndex ? dictEnd : iend;
                matchLength = ZSTD_count_2segments(ip+1+4, repMatch+4, iend, repMatchEnd, prefixLowest) + 4;
                if (depth==0) goto _storeSequence;
            }
        }
        if ( dictMode == ZSTD_noDict
          && ((offset_1 > 0) & (MEM_read32(ip+1-offset_1) == MEM_read32(ip+1)))) {
            matchLength = ZSTD_count(ip+1+4, ip+1+4-offset_1, iend) + 4;
            if (depth==0) goto _storeSequence;
        }
        {   size_t offsetFound = 999999999;
            size_t const ml2 = searchMax(ms, ip, iend, &offsetFound);
            if (ml2 > matchLength)
                matchLength = ml2, start = ip, offset=offsetFound;
        }
        if (matchLength < 4) {
            ip += ((ip-anchor) >> kSearchStrength) + 1;    
            continue;
        }
        if (depth>=1)
        while (ip<ilimit) {
            ip ++;
            if ( (dictMode == ZSTD_noDict)
              && (offset) && ((offset_1>0) & (MEM_read32(ip) == MEM_read32(ip - offset_1)))) {
                size_t const mlRep = ZSTD_count(ip+4, ip+4-offset_1, iend) + 4;
                int const gain2 = (int)(mlRep * 3);
                int const gain1 = (int)(matchLength*3 - ZSTD_highbit32((U32)offset+1) + 1);
                if ((mlRep >= 4) && (gain2 > gain1))
                    matchLength = mlRep, offset = 0, start = ip;
            }
            if (dictMode == ZSTD_dictMatchState) {
                const U32 repIndex = (U32)(ip - base) - offset_1;
                const BYTE* repMatch = repIndex < prefixLowestIndex ?
                               dictBase + (repIndex - dictIndexDelta) :
                               base + repIndex;
                if (((U32)((prefixLowestIndex-1) - repIndex) >= 3  )
                    && (MEM_read32(repMatch) == MEM_read32(ip)) ) {
                    const BYTE* repMatchEnd = repIndex < prefixLowestIndex ? dictEnd : iend;
                    size_t const mlRep = ZSTD_count_2segments(ip+4, repMatch+4, iend, repMatchEnd, prefixLowest) + 4;
                    int const gain2 = (int)(mlRep * 3);
                    int const gain1 = (int)(matchLength*3 - ZSTD_highbit32((U32)offset+1) + 1);
                    if ((mlRep >= 4) && (gain2 > gain1))
                        matchLength = mlRep, offset = 0, start = ip;
                }
            }
            {   size_t offset2=999999999;
                size_t const ml2 = searchMax(ms, ip, iend, &offset2);
                int const gain2 = (int)(ml2*4 - ZSTD_highbit32((U32)offset2+1));    
                int const gain1 = (int)(matchLength*4 - ZSTD_highbit32((U32)offset+1) + 4);
                if ((ml2 >= 4) && (gain2 > gain1)) {
                    matchLength = ml2, offset = offset2, start = ip;
                    continue;    
            }   }
            if ((depth==2) && (ip<ilimit)) {
                ip ++;
                if ( (dictMode == ZSTD_noDict)
                  && (offset) && ((offset_1>0) & (MEM_read32(ip) == MEM_read32(ip - offset_1)))) {
                    size_t const mlRep = ZSTD_count(ip+4, ip+4-offset_1, iend) + 4;
                    int const gain2 = (int)(mlRep * 4);
                    int const gain1 = (int)(matchLength*4 - ZSTD_highbit32((U32)offset+1) + 1);
                    if ((mlRep >= 4) && (gain2 > gain1))
                        matchLength = mlRep, offset = 0, start = ip;
                }
                if (dictMode == ZSTD_dictMatchState) {
                    const U32 repIndex = (U32)(ip - base) - offset_1;
                    const BYTE* repMatch = repIndex < prefixLowestIndex ?
                                   dictBase + (repIndex - dictIndexDelta) :
                                   base + repIndex;
                    if (((U32)((prefixLowestIndex-1) - repIndex) >= 3  )
                        && (MEM_read32(repMatch) == MEM_read32(ip)) ) {
                        const BYTE* repMatchEnd = repIndex < prefixLowestIndex ? dictEnd : iend;
                        size_t const mlRep = ZSTD_count_2segments(ip+4, repMatch+4, iend, repMatchEnd, prefixLowest) + 4;
                        int const gain2 = (int)(mlRep * 4);
                        int const gain1 = (int)(matchLength*4 - ZSTD_highbit32((U32)offset+1) + 1);
                        if ((mlRep >= 4) && (gain2 > gain1))
                            matchLength = mlRep, offset = 0, start = ip;
                    }
                }
                {   size_t offset2=999999999;
                    size_t const ml2 = searchMax(ms, ip, iend, &offset2);
                    int const gain2 = (int)(ml2*4 - ZSTD_highbit32((U32)offset2+1));    
                    int const gain1 = (int)(matchLength*4 - ZSTD_highbit32((U32)offset+1) + 7);
                    if ((ml2 >= 4) && (gain2 > gain1)) {
                        matchLength = ml2, offset = offset2, start = ip;
                        continue;
            }   }   }
            break;   
        }
        if (offset) {
            if (dictMode == ZSTD_noDict) {
                while ( ((start > anchor) & (start - (offset-ZSTD_REP_MOVE) > prefixLowest))
                     && (start[-1] == (start-(offset-ZSTD_REP_MOVE))[-1]) )   
                    { start--; matchLength++; }
            }
            if (dictMode == ZSTD_dictMatchState) {
                U32 const matchIndex = (U32)((start-base) - (offset - ZSTD_REP_MOVE));
                const BYTE* match = (matchIndex < prefixLowestIndex) ? dictBase + matchIndex - dictIndexDelta : base + matchIndex;
                const BYTE* const mStart = (matchIndex < prefixLowestIndex) ? dictLowest : prefixLowest;
                while ((start>anchor) && (match>mStart) && (start[-1] == match[-1])) { start--; match--; matchLength++; }   
            }
            offset_2 = offset_1; offset_1 = (U32)(offset - ZSTD_REP_MOVE);
        }
_storeSequence:
        {   size_t const litLength = start - anchor;
            ZSTD_storeSeq(seqStore, litLength, anchor, iend, (U32)offset, matchLength-MINMATCH);
            anchor = ip = start + matchLength;
        }
        if (dictMode == ZSTD_dictMatchState) {
            while (ip <= ilimit) {
                U32 const current2 = (U32)(ip-base);
                U32 const repIndex = current2 - offset_2;
                const BYTE* repMatch = dictMode == ZSTD_dictMatchState
                    && repIndex < prefixLowestIndex ?
                        dictBase - dictIndexDelta + repIndex :
                        base + repIndex;
                if ( ((U32)((prefixLowestIndex-1) - (U32)repIndex) >= 3  )
                   && (MEM_read32(repMatch) == MEM_read32(ip)) ) {
                    const BYTE* const repEnd2 = repIndex < prefixLowestIndex ? dictEnd : iend;
                    matchLength = ZSTD_count_2segments(ip+4, repMatch+4, iend, repEnd2, prefixLowest) + 4;
                    offset = offset_2; offset_2 = offset_1; offset_1 = (U32)offset;    
                    ZSTD_storeSeq(seqStore, 0, anchor, iend, 0, matchLength-MINMATCH);
                    ip += matchLength;
                    anchor = ip;
                    continue;
                }
                break;
            }
        }
        if (dictMode == ZSTD_noDict) {
            while ( ((ip <= ilimit) & (offset_2>0))
                 && (MEM_read32(ip) == MEM_read32(ip - offset_2)) ) {
                matchLength = ZSTD_count(ip+4, ip+4-offset_2, iend) + 4;
                offset = offset_2; offset_2 = offset_1; offset_1 = (U32)offset;  
                ZSTD_storeSeq(seqStore, 0, anchor, iend, 0, matchLength-MINMATCH);
                ip += matchLength;
                anchor = ip;
                continue;    
    }   }   }
    rep[0] = offset_1 ? offset_1 : savedOffset;
    rep[1] = offset_2 ? offset_2 : savedOffset;
    return (size_t)(iend - anchor);
}
size_t ZSTD_compressBlock_btlazy2(
        ZSTD_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_REP_NUM],
        void const* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_generic(ms, seqStore, rep, src, srcSize, search_binaryTree, 2, ZSTD_noDict);
}
size_t ZSTD_compressBlock_lazy2(
        ZSTD_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_REP_NUM],
        void const* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_generic(ms, seqStore, rep, src, srcSize, search_hashChain, 2, ZSTD_noDict);
}
size_t ZSTD_compressBlock_lazy(
        ZSTD_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_REP_NUM],
        void const* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_generic(ms, seqStore, rep, src, srcSize, search_hashChain, 1, ZSTD_noDict);
}
size_t ZSTD_compressBlock_greedy(
        ZSTD_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_REP_NUM],
        void const* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_generic(ms, seqStore, rep, src, srcSize, search_hashChain, 0, ZSTD_noDict);
}
size_t ZSTD_compressBlock_btlazy2_dictMatchState(
        ZSTD_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_REP_NUM],
        void const* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_generic(ms, seqStore, rep, src, srcSize, search_binaryTree, 2, ZSTD_dictMatchState);
}
size_t ZSTD_compressBlock_lazy2_dictMatchState(
        ZSTD_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_REP_NUM],
        void const* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_generic(ms, seqStore, rep, src, srcSize, search_hashChain, 2, ZSTD_dictMatchState);
}
size_t ZSTD_compressBlock_lazy_dictMatchState(
        ZSTD_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_REP_NUM],
        void const* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_generic(ms, seqStore, rep, src, srcSize, search_hashChain, 1, ZSTD_dictMatchState);
}
size_t ZSTD_compressBlock_greedy_dictMatchState(
        ZSTD_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_REP_NUM],
        void const* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_generic(ms, seqStore, rep, src, srcSize, search_hashChain, 0, ZSTD_dictMatchState);
}
FORCE_INLINE_TEMPLATE
size_t ZSTD_compressBlock_lazy_extDict_generic(
                        ZSTD_matchState_t* ms, seqStore_t* seqStore,
                        U32 rep[ZSTD_REP_NUM],
                        const void* src, size_t srcSize,
                        const searchMethod_e searchMethod, const U32 depth)
{
    const BYTE* const istart = (const BYTE*)src;
    const BYTE* ip = istart;
    const BYTE* anchor = istart;
    const BYTE* const iend = istart + srcSize;
    const BYTE* const ilimit = iend - 8;
    const BYTE* const base = ms->window.base;
    const U32 dictLimit = ms->window.dictLimit;
    const BYTE* const prefixStart = base + dictLimit;
    const BYTE* const dictBase = ms->window.dictBase;
    const BYTE* const dictEnd  = dictBase + dictLimit;
    const BYTE* const dictStart  = dictBase + ms->window.lowLimit;
    const U32 windowLog = ms->cParams.windowLog;
    typedef size_t (*searchMax_f)(
                        ZSTD_matchState_t* ms,
                        const BYTE* ip, const BYTE* iLimit, size_t* offsetPtr);
    searchMax_f searchMax = searchMethod==search_binaryTree ? ZSTD_BtFindBestMatch_extDict_selectMLS : ZSTD_HcFindBestMatch_extDict_selectMLS;
    U32 offset_1 = rep[0], offset_2 = rep[1];
    DEBUGLOG(5, "ZSTD_compressBlock_lazy_extDict_generic");
    ip += (ip == prefixStart);
#if defined(__GNUC__) && defined(__x86_64__)
    __asm__(".p2align 5");
#endif
    while (ip < ilimit) {
        size_t matchLength=0;
        size_t offset=0;
        const BYTE* start=ip+1;
        U32 current = (U32)(ip-base);
        {   const U32 windowLow = ZSTD_getLowestMatchIndex(ms, current+1, windowLog);
            const U32 repIndex = (U32)(current+1 - offset_1);
            const BYTE* const repBase = repIndex < dictLimit ? dictBase : base;
            const BYTE* const repMatch = repBase + repIndex;
            if ( ((U32)((dictLimit-1) - repIndex) >= 3)  
               & (offset_1 < current+1 - windowLow) )  
            if (MEM_read32(ip+1) == MEM_read32(repMatch)) {
                const BYTE* const repEnd = repIndex < dictLimit ? dictEnd : iend;
                matchLength = ZSTD_count_2segments(ip+1+4, repMatch+4, iend, repEnd, prefixStart) + 4;
                if (depth==0) goto _storeSequence;
        }   }
        {   size_t offsetFound = 999999999;
            size_t const ml2 = searchMax(ms, ip, iend, &offsetFound);
            if (ml2 > matchLength)
                matchLength = ml2, start = ip, offset=offsetFound;
        }
         if (matchLength < 4) {
            ip += ((ip-anchor) >> kSearchStrength) + 1;    
            continue;
        }
        if (depth>=1)
        while (ip<ilimit) {
            ip ++;
            current++;
            if (offset) {
                const U32 windowLow = ZSTD_getLowestMatchIndex(ms, current, windowLog);
                const U32 repIndex = (U32)(current - offset_1);
                const BYTE* const repBase = repIndex < dictLimit ? dictBase : base;
                const BYTE* const repMatch = repBase + repIndex;
                    if ( ((U32)((dictLimit-1) - repIndex) >= 3)  
                       & (offset_1 < current - windowLow) )  
                if (MEM_read32(ip) == MEM_read32(repMatch)) {
                    const BYTE* const repEnd = repIndex < dictLimit ? dictEnd : iend;
                    size_t const repLength = ZSTD_count_2segments(ip+4, repMatch+4, iend, repEnd, prefixStart) + 4;
                    int const gain2 = (int)(repLength * 3);
                    int const gain1 = (int)(matchLength*3 - ZSTD_highbit32((U32)offset+1) + 1);
                    if ((repLength >= 4) && (gain2 > gain1))
                        matchLength = repLength, offset = 0, start = ip;
            }   }
            {   size_t offset2=999999999;
                size_t const ml2 = searchMax(ms, ip, iend, &offset2);
                int const gain2 = (int)(ml2*4 - ZSTD_highbit32((U32)offset2+1));    
                int const gain1 = (int)(matchLength*4 - ZSTD_highbit32((U32)offset+1) + 4);
                if ((ml2 >= 4) && (gain2 > gain1)) {
                    matchLength = ml2, offset = offset2, start = ip;
                    continue;    
            }   }
            if ((depth==2) && (ip<ilimit)) {
                ip ++;
                current++;
                if (offset) {
                    const U32 windowLow = ZSTD_getLowestMatchIndex(ms, current, windowLog);
                    const U32 repIndex = (U32)(current - offset_1);
                    const BYTE* const repBase = repIndex < dictLimit ? dictBase : base;
                    const BYTE* const repMatch = repBase + repIndex;
                if ( ((U32)((dictLimit-1) - repIndex) >= 3)  
                   & (offset_1 < current - windowLow) )  
                    if (MEM_read32(ip) == MEM_read32(repMatch)) {
                        const BYTE* const repEnd = repIndex < dictLimit ? dictEnd : iend;
                        size_t const repLength = ZSTD_count_2segments(ip+4, repMatch+4, iend, repEnd, prefixStart) + 4;
                        int const gain2 = (int)(repLength * 4);
                        int const gain1 = (int)(matchLength*4 - ZSTD_highbit32((U32)offset+1) + 1);
                        if ((repLength >= 4) && (gain2 > gain1))
                            matchLength = repLength, offset = 0, start = ip;
                }   }
                {   size_t offset2=999999999;
                    size_t const ml2 = searchMax(ms, ip, iend, &offset2);
                    int const gain2 = (int)(ml2*4 - ZSTD_highbit32((U32)offset2+1));    
                    int const gain1 = (int)(matchLength*4 - ZSTD_highbit32((U32)offset+1) + 7);
                    if ((ml2 >= 4) && (gain2 > gain1)) {
                        matchLength = ml2, offset = offset2, start = ip;
                        continue;
            }   }   }
            break;   
        }
        if (offset) {
            U32 const matchIndex = (U32)((start-base) - (offset - ZSTD_REP_MOVE));
            const BYTE* match = (matchIndex < dictLimit) ? dictBase + matchIndex : base + matchIndex;
            const BYTE* const mStart = (matchIndex < dictLimit) ? dictStart : prefixStart;
            while ((start>anchor) && (match>mStart) && (start[-1] == match[-1])) { start--; match--; matchLength++; }   
            offset_2 = offset_1; offset_1 = (U32)(offset - ZSTD_REP_MOVE);
        }
_storeSequence:
        {   size_t const litLength = start - anchor;
            ZSTD_storeSeq(seqStore, litLength, anchor, iend, (U32)offset, matchLength-MINMATCH);
            anchor = ip = start + matchLength;
        }
        while (ip <= ilimit) {
            const U32 repCurrent = (U32)(ip-base);
            const U32 windowLow = ZSTD_getLowestMatchIndex(ms, repCurrent, windowLog);
            const U32 repIndex = repCurrent - offset_2;
            const BYTE* const repBase = repIndex < dictLimit ? dictBase : base;
            const BYTE* const repMatch = repBase + repIndex;
            if ( ((U32)((dictLimit-1) - repIndex) >= 3)  
               & (offset_2 < repCurrent - windowLow) )  
            if (MEM_read32(ip) == MEM_read32(repMatch)) {
                const BYTE* const repEnd = repIndex < dictLimit ? dictEnd : iend;
                matchLength = ZSTD_count_2segments(ip+4, repMatch+4, iend, repEnd, prefixStart) + 4;
                offset = offset_2; offset_2 = offset_1; offset_1 = (U32)offset;    
                ZSTD_storeSeq(seqStore, 0, anchor, iend, 0, matchLength-MINMATCH);
                ip += matchLength;
                anchor = ip;
                continue;    
            }
            break;
    }   }
    rep[0] = offset_1;
    rep[1] = offset_2;
    return (size_t)(iend - anchor);
}
size_t ZSTD_compressBlock_greedy_extDict(
        ZSTD_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_REP_NUM],
        void const* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_extDict_generic(ms, seqStore, rep, src, srcSize, search_hashChain, 0);
}
size_t ZSTD_compressBlock_lazy_extDict(
        ZSTD_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_REP_NUM],
        void const* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_extDict_generic(ms, seqStore, rep, src, srcSize, search_hashChain, 1);
}
size_t ZSTD_compressBlock_lazy2_extDict(
        ZSTD_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_REP_NUM],
        void const* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_extDict_generic(ms, seqStore, rep, src, srcSize, search_hashChain, 2);
}
size_t ZSTD_compressBlock_btlazy2_extDict(
        ZSTD_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_REP_NUM],
        void const* src, size_t srcSize)
{
    return ZSTD_compressBlock_lazy_extDict_generic(ms, seqStore, rep, src, srcSize, search_binaryTree, 2);
}
