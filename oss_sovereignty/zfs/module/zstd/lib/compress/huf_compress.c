#ifdef _MSC_VER     
#  pragma warning(disable : 4127)         
#endif
#include <string.h>      
#include <stdio.h>       
#include "../common/compiler.h"
#include "../common/bitstream.h"
#include "hist.h"
#define FSE_STATIC_LINKING_ONLY    
#include "../common/fse.h"         
#define HUF_STATIC_LINKING_ONLY
#include "../common/huf.h"
#include "../common/error_private.h"
#define HUF_isError ERR_isError
#define HUF_STATIC_ASSERT(c) DEBUG_STATIC_ASSERT(c)    
unsigned HUF_optimalTableLog(unsigned maxTableLog, size_t srcSize, unsigned maxSymbolValue)
{
    return FSE_optimalTableLog_internal(maxTableLog, srcSize, maxSymbolValue, 1);
}
#define MAX_FSE_TABLELOG_FOR_HUFF_HEADER 6
static size_t HUF_compressWeights (void* dst, size_t dstSize, const void* weightTable, size_t wtSize)
{
    BYTE* const ostart = (BYTE*) dst;
    BYTE* op = ostart;
    BYTE* const oend = ostart + dstSize;
    unsigned maxSymbolValue = HUF_TABLELOG_MAX;
    U32 tableLog = MAX_FSE_TABLELOG_FOR_HUFF_HEADER;
    FSE_CTable CTable[FSE_CTABLE_SIZE_U32(MAX_FSE_TABLELOG_FOR_HUFF_HEADER, HUF_TABLELOG_MAX)];
    BYTE scratchBuffer[1<<MAX_FSE_TABLELOG_FOR_HUFF_HEADER];
    unsigned count[HUF_TABLELOG_MAX+1];
    S16 norm[HUF_TABLELOG_MAX+1];
    if (wtSize <= 1) return 0;   
    {   unsigned const maxCount = HIST_count_simple(count, &maxSymbolValue, weightTable, wtSize);    
        if (maxCount == wtSize) return 1;    
        if (maxCount == 1) return 0;         
    }
    tableLog = FSE_optimalTableLog(tableLog, wtSize, maxSymbolValue);
    CHECK_F( FSE_normalizeCount(norm, tableLog, count, wtSize, maxSymbolValue) );
    {   CHECK_V_F(hSize, FSE_writeNCount(op, (size_t)(oend-op), norm, maxSymbolValue, tableLog) );
        op += hSize;
    }
    CHECK_F( FSE_buildCTable_wksp(CTable, norm, maxSymbolValue, tableLog, scratchBuffer, sizeof(scratchBuffer)) );
    {   CHECK_V_F(cSize, FSE_compress_usingCTable(op, (size_t)(oend - op), weightTable, wtSize, CTable) );
        if (cSize == 0) return 0;    
        op += cSize;
    }
    return (size_t)(op-ostart);
}
struct HUF_CElt_s {
  U16  val;
  BYTE nbBits;
};    
size_t HUF_writeCTable (void* dst, size_t maxDstSize,
                        const HUF_CElt* CTable, unsigned maxSymbolValue, unsigned huffLog)
{
    BYTE bitsToWeight[HUF_TABLELOG_MAX + 1];    
    BYTE huffWeight[HUF_SYMBOLVALUE_MAX];
    BYTE* op = (BYTE*)dst;
    U32 n;
    if (maxSymbolValue > HUF_SYMBOLVALUE_MAX) return ERROR(maxSymbolValue_tooLarge);
    bitsToWeight[0] = 0;
    for (n=1; n<huffLog+1; n++)
        bitsToWeight[n] = (BYTE)(huffLog + 1 - n);
    for (n=0; n<maxSymbolValue; n++)
        huffWeight[n] = bitsToWeight[CTable[n].nbBits];
    {   CHECK_V_F(hSize, HUF_compressWeights(op+1, maxDstSize-1, huffWeight, maxSymbolValue) );
        if ((hSize>1) & (hSize < maxSymbolValue/2)) {    
            op[0] = (BYTE)hSize;
            return hSize+1;
    }   }
    if (maxSymbolValue > (256-128)) return ERROR(GENERIC);    
    if (((maxSymbolValue+1)/2) + 1 > maxDstSize) return ERROR(dstSize_tooSmall);    
    op[0] = (BYTE)(128   + (maxSymbolValue-1));
    huffWeight[maxSymbolValue] = 0;    
    for (n=0; n<maxSymbolValue; n+=2)
        op[(n/2)+1] = (BYTE)((huffWeight[n] << 4) + huffWeight[n+1]);
    return ((maxSymbolValue+1)/2) + 1;
}
size_t HUF_readCTable (HUF_CElt* CTable, unsigned* maxSymbolValuePtr, const void* src, size_t srcSize, unsigned* hasZeroWeights)
{
    BYTE huffWeight[HUF_SYMBOLVALUE_MAX + 1];    
    U32 rankVal[HUF_TABLELOG_ABSOLUTEMAX + 1];    
    U32 tableLog = 0;
    U32 nbSymbols = 0;
    CHECK_V_F(readSize, HUF_readStats(huffWeight, HUF_SYMBOLVALUE_MAX+1, rankVal, &nbSymbols, &tableLog, src, srcSize));
    if (tableLog > HUF_TABLELOG_MAX) return ERROR(tableLog_tooLarge);
    if (nbSymbols > *maxSymbolValuePtr+1) return ERROR(maxSymbolValue_tooSmall);
    {   U32 n, nextRankStart = 0;
        for (n=1; n<=tableLog; n++) {
            U32 current = nextRankStart;
            nextRankStart += (rankVal[n] << (n-1));
            rankVal[n] = current;
    }   }
    *hasZeroWeights = 0;
    {   U32 n; for (n=0; n<nbSymbols; n++) {
            const U32 w = huffWeight[n];
            *hasZeroWeights |= (w == 0);
            CTable[n].nbBits = (BYTE)(tableLog + 1 - w) & -(w != 0);
    }   }
    {   U16 nbPerRank[HUF_TABLELOG_MAX+2]  = {0};   
        U16 valPerRank[HUF_TABLELOG_MAX+2] = {0};
        { U32 n; for (n=0; n<nbSymbols; n++) nbPerRank[CTable[n].nbBits]++; }
        valPerRank[tableLog+1] = 0;    
        {   U16 min = 0;
            U32 n; for (n=tableLog; n>0; n--) {   
                valPerRank[n] = min;      
                min += nbPerRank[n];
                min >>= 1;
        }   }
        { U32 n; for (n=0; n<nbSymbols; n++) CTable[n].val = valPerRank[CTable[n].nbBits]++; }
    }
    *maxSymbolValuePtr = nbSymbols - 1;
    return readSize;
}
U32 HUF_getNbBits(const void* symbolTable, U32 symbolValue)
{
    const HUF_CElt* table = (const HUF_CElt*)symbolTable;
    assert(symbolValue <= HUF_SYMBOLVALUE_MAX);
    return table[symbolValue].nbBits;
}
typedef struct nodeElt_s {
    U32 count;
    U16 parent;
    BYTE byte;
    BYTE nbBits;
} nodeElt;
static U32 HUF_setMaxHeight(nodeElt* huffNode, U32 lastNonNull, U32 maxNbBits)
{
    const U32 largestBits = huffNode[lastNonNull].nbBits;
    if (largestBits <= maxNbBits) return largestBits;    
    {   int totalCost = 0;
        const U32 baseCost = 1 << (largestBits - maxNbBits);
        int n = (int)lastNonNull;
        while (huffNode[n].nbBits > maxNbBits) {
            totalCost += baseCost - (1 << (largestBits - huffNode[n].nbBits));
            huffNode[n].nbBits = (BYTE)maxNbBits;
            n --;
        }   
        while (huffNode[n].nbBits == maxNbBits) n--;    
        totalCost >>= (largestBits - maxNbBits);   
        {   U32 const noSymbol = 0xF0F0F0F0;
            U32 rankLast[HUF_TABLELOG_MAX+2];
            memset(rankLast, 0xF0, sizeof(rankLast));
            {   U32 currentNbBits = maxNbBits;
                int pos;
                for (pos=n ; pos >= 0; pos--) {
                    if (huffNode[pos].nbBits >= currentNbBits) continue;
                    currentNbBits = huffNode[pos].nbBits;    
                    rankLast[maxNbBits-currentNbBits] = (U32)pos;
            }   }
            while (totalCost > 0) {
                U32 nBitsToDecrease = BIT_highbit32((U32)totalCost) + 1;
                for ( ; nBitsToDecrease > 1; nBitsToDecrease--) {
                    U32 const highPos = rankLast[nBitsToDecrease];
                    U32 const lowPos = rankLast[nBitsToDecrease-1];
                    if (highPos == noSymbol) continue;
                    if (lowPos == noSymbol) break;
                    {   U32 const highTotal = huffNode[highPos].count;
                        U32 const lowTotal = 2 * huffNode[lowPos].count;
                        if (highTotal <= lowTotal) break;
                }   }
                while ((nBitsToDecrease<=HUF_TABLELOG_MAX) && (rankLast[nBitsToDecrease] == noSymbol))
                    nBitsToDecrease ++;
                totalCost -= 1 << (nBitsToDecrease-1);
                if (rankLast[nBitsToDecrease-1] == noSymbol)
                    rankLast[nBitsToDecrease-1] = rankLast[nBitsToDecrease];    
                huffNode[rankLast[nBitsToDecrease]].nbBits ++;
                if (rankLast[nBitsToDecrease] == 0)     
                    rankLast[nBitsToDecrease] = noSymbol;
                else {
                    rankLast[nBitsToDecrease]--;
                    if (huffNode[rankLast[nBitsToDecrease]].nbBits != maxNbBits-nBitsToDecrease)
                        rankLast[nBitsToDecrease] = noSymbol;    
            }   }    
            while (totalCost < 0) {   
                if (rankLast[1] == noSymbol) {   
                    while (huffNode[n].nbBits == maxNbBits) n--;
                    huffNode[n+1].nbBits--;
                    assert(n >= 0);
                    rankLast[1] = (U32)(n+1);
                    totalCost++;
                    continue;
                }
                huffNode[ rankLast[1] + 1 ].nbBits--;
                rankLast[1]++;
                totalCost ++;
    }   }   }    
    return maxNbBits;
}
typedef struct {
    U32 base;
    U32 current;
} rankPos;
typedef nodeElt huffNodeTable[HUF_CTABLE_WORKSPACE_SIZE_U32];
#define RANK_POSITION_TABLE_SIZE 32
typedef struct {
  huffNodeTable huffNodeTbl;
  rankPos rankPosition[RANK_POSITION_TABLE_SIZE];
} HUF_buildCTable_wksp_tables;
static void HUF_sort(nodeElt* huffNode, const unsigned* count, U32 maxSymbolValue, rankPos* rankPosition)
{
    U32 n;
    memset(rankPosition, 0, sizeof(*rankPosition) * RANK_POSITION_TABLE_SIZE);
    for (n=0; n<=maxSymbolValue; n++) {
        U32 r = BIT_highbit32(count[n] + 1);
        rankPosition[r].base ++;
    }
    for (n=30; n>0; n--) rankPosition[n-1].base += rankPosition[n].base;
    for (n=0; n<32; n++) rankPosition[n].current = rankPosition[n].base;
    for (n=0; n<=maxSymbolValue; n++) {
        U32 const c = count[n];
        U32 const r = BIT_highbit32(c+1) + 1;
        U32 pos = rankPosition[r].current++;
        while ((pos > rankPosition[r].base) && (c > huffNode[pos-1].count)) {
            huffNode[pos] = huffNode[pos-1];
            pos--;
        }
        huffNode[pos].count = c;
        huffNode[pos].byte  = (BYTE)n;
    }
}
#define STARTNODE (HUF_SYMBOLVALUE_MAX+1)
size_t HUF_buildCTable_wksp (HUF_CElt* tree, const unsigned* count, U32 maxSymbolValue, U32 maxNbBits, void* workSpace, size_t wkspSize)
{
    HUF_buildCTable_wksp_tables* const wksp_tables = (HUF_buildCTable_wksp_tables*)workSpace;
    nodeElt* const huffNode0 = wksp_tables->huffNodeTbl;
    nodeElt* const huffNode = huffNode0+1;
    int nonNullRank;
    int lowS, lowN;
    int nodeNb = STARTNODE;
    int n, nodeRoot;
    if (((size_t)workSpace & 3) != 0) return ERROR(GENERIC);   
    if (wkspSize < sizeof(HUF_buildCTable_wksp_tables))
      return ERROR(workSpace_tooSmall);
    if (maxNbBits == 0) maxNbBits = HUF_TABLELOG_DEFAULT;
    if (maxSymbolValue > HUF_SYMBOLVALUE_MAX)
      return ERROR(maxSymbolValue_tooLarge);
    memset(huffNode0, 0, sizeof(huffNodeTable));
    HUF_sort(huffNode, count, maxSymbolValue, wksp_tables->rankPosition);
    nonNullRank = (int)maxSymbolValue;
    while(huffNode[nonNullRank].count == 0) nonNullRank--;
    lowS = nonNullRank; nodeRoot = nodeNb + lowS - 1; lowN = nodeNb;
    huffNode[nodeNb].count = huffNode[lowS].count + huffNode[lowS-1].count;
    huffNode[lowS].parent = huffNode[lowS-1].parent = (U16)nodeNb;
    nodeNb++; lowS-=2;
    for (n=nodeNb; n<=nodeRoot; n++) huffNode[n].count = (U32)(1U<<30);
    huffNode0[0].count = (U32)(1U<<31);   
    while (nodeNb <= nodeRoot) {
        int const n1 = (huffNode[lowS].count < huffNode[lowN].count) ? lowS-- : lowN++;
        int const n2 = (huffNode[lowS].count < huffNode[lowN].count) ? lowS-- : lowN++;
        huffNode[nodeNb].count = huffNode[n1].count + huffNode[n2].count;
        huffNode[n1].parent = huffNode[n2].parent = (U16)nodeNb;
        nodeNb++;
    }
    huffNode[nodeRoot].nbBits = 0;
    for (n=nodeRoot-1; n>=STARTNODE; n--)
        huffNode[n].nbBits = huffNode[ huffNode[n].parent ].nbBits + 1;
    for (n=0; n<=nonNullRank; n++)
        huffNode[n].nbBits = huffNode[ huffNode[n].parent ].nbBits + 1;
    maxNbBits = HUF_setMaxHeight(huffNode, (U32)nonNullRank, maxNbBits);
    {   U16 nbPerRank[HUF_TABLELOG_MAX+1] = {0};
        U16 valPerRank[HUF_TABLELOG_MAX+1] = {0};
        int const alphabetSize = (int)(maxSymbolValue + 1);
        if (maxNbBits > HUF_TABLELOG_MAX) return ERROR(GENERIC);    
        for (n=0; n<=nonNullRank; n++)
            nbPerRank[huffNode[n].nbBits]++;
        {   U16 min = 0;
            for (n=(int)maxNbBits; n>0; n--) {
                valPerRank[n] = min;       
                min += nbPerRank[n];
                min >>= 1;
        }   }
        for (n=0; n<alphabetSize; n++)
            tree[huffNode[n].byte].nbBits = huffNode[n].nbBits;    
        for (n=0; n<alphabetSize; n++)
            tree[n].val = valPerRank[tree[n].nbBits]++;    
    }
    return maxNbBits;
}
size_t HUF_buildCTable (HUF_CElt* tree, const unsigned* count, unsigned maxSymbolValue, unsigned maxNbBits)
{
    HUF_buildCTable_wksp_tables workspace;
    return HUF_buildCTable_wksp(tree, count, maxSymbolValue, maxNbBits, &workspace, sizeof(workspace));
}
size_t HUF_estimateCompressedSize(const HUF_CElt* CTable, const unsigned* count, unsigned maxSymbolValue)
{
    size_t nbBits = 0;
    int s;
    for (s = 0; s <= (int)maxSymbolValue; ++s) {
        nbBits += CTable[s].nbBits * count[s];
    }
    return nbBits >> 3;
}
int HUF_validateCTable(const HUF_CElt* CTable, const unsigned* count, unsigned maxSymbolValue) {
  int bad = 0;
  int s;
  for (s = 0; s <= (int)maxSymbolValue; ++s) {
    bad |= (count[s] != 0) & (CTable[s].nbBits == 0);
  }
  return !bad;
}
size_t HUF_compressBound(size_t size) { return HUF_COMPRESSBOUND(size); }
FORCE_INLINE_TEMPLATE void
HUF_encodeSymbol(BIT_CStream_t* bitCPtr, U32 symbol, const HUF_CElt* CTable)
{
    BIT_addBitsFast(bitCPtr, CTable[symbol].val, CTable[symbol].nbBits);
}
#define HUF_FLUSHBITS(s)  BIT_flushBits(s)
#define HUF_FLUSHBITS_1(stream) \
    if (sizeof((stream)->bitContainer)*8 < HUF_TABLELOG_MAX*2+7) HUF_FLUSHBITS(stream)
#define HUF_FLUSHBITS_2(stream) \
    if (sizeof((stream)->bitContainer)*8 < HUF_TABLELOG_MAX*4+7) HUF_FLUSHBITS(stream)
FORCE_INLINE_TEMPLATE size_t
HUF_compress1X_usingCTable_internal_body(void* dst, size_t dstSize,
                                   const void* src, size_t srcSize,
                                   const HUF_CElt* CTable)
{
    const BYTE* ip = (const BYTE*) src;
    BYTE* const ostart = (BYTE*)dst;
    BYTE* const oend = ostart + dstSize;
    BYTE* op = ostart;
    size_t n;
    BIT_CStream_t bitC;
    if (dstSize < 8) return 0;    
    { size_t const initErr = BIT_initCStream(&bitC, op, (size_t)(oend-op));
      if (HUF_isError(initErr)) return 0; }
    n = srcSize & ~3;   
    switch (srcSize & 3)
    {
        case 3 : HUF_encodeSymbol(&bitC, ip[n+ 2], CTable);
                 HUF_FLUSHBITS_2(&bitC);
        case 2 : HUF_encodeSymbol(&bitC, ip[n+ 1], CTable);
                 HUF_FLUSHBITS_1(&bitC);
        case 1 : HUF_encodeSymbol(&bitC, ip[n+ 0], CTable);
                 HUF_FLUSHBITS(&bitC);
        case 0 :  
        default: break;
    }
    for (; n>0; n-=4) {   
        HUF_encodeSymbol(&bitC, ip[n- 1], CTable);
        HUF_FLUSHBITS_1(&bitC);
        HUF_encodeSymbol(&bitC, ip[n- 2], CTable);
        HUF_FLUSHBITS_2(&bitC);
        HUF_encodeSymbol(&bitC, ip[n- 3], CTable);
        HUF_FLUSHBITS_1(&bitC);
        HUF_encodeSymbol(&bitC, ip[n- 4], CTable);
        HUF_FLUSHBITS(&bitC);
    }
    return BIT_closeCStream(&bitC);
}
#if DYNAMIC_BMI2
static TARGET_ATTRIBUTE("bmi2") size_t
HUF_compress1X_usingCTable_internal_bmi2(void* dst, size_t dstSize,
                                   const void* src, size_t srcSize,
                                   const HUF_CElt* CTable)
{
    return HUF_compress1X_usingCTable_internal_body(dst, dstSize, src, srcSize, CTable);
}
static size_t
HUF_compress1X_usingCTable_internal_default(void* dst, size_t dstSize,
                                      const void* src, size_t srcSize,
                                      const HUF_CElt* CTable)
{
    return HUF_compress1X_usingCTable_internal_body(dst, dstSize, src, srcSize, CTable);
}
static size_t
HUF_compress1X_usingCTable_internal(void* dst, size_t dstSize,
                              const void* src, size_t srcSize,
                              const HUF_CElt* CTable, const int bmi2)
{
    if (bmi2) {
        return HUF_compress1X_usingCTable_internal_bmi2(dst, dstSize, src, srcSize, CTable);
    }
    return HUF_compress1X_usingCTable_internal_default(dst, dstSize, src, srcSize, CTable);
}
#else
static size_t
HUF_compress1X_usingCTable_internal(void* dst, size_t dstSize,
                              const void* src, size_t srcSize,
                              const HUF_CElt* CTable, const int bmi2)
{
    (void)bmi2;
    return HUF_compress1X_usingCTable_internal_body(dst, dstSize, src, srcSize, CTable);
}
#endif
size_t HUF_compress1X_usingCTable(void* dst, size_t dstSize, const void* src, size_t srcSize, const HUF_CElt* CTable)
{
    return HUF_compress1X_usingCTable_internal(dst, dstSize, src, srcSize, CTable,   0);
}
static size_t
HUF_compress4X_usingCTable_internal(void* dst, size_t dstSize,
                              const void* src, size_t srcSize,
                              const HUF_CElt* CTable, int bmi2)
{
    size_t const segmentSize = (srcSize+3)/4;    
    const BYTE* ip = (const BYTE*) src;
    const BYTE* const iend = ip + srcSize;
    BYTE* const ostart = (BYTE*) dst;
    BYTE* const oend = ostart + dstSize;
    BYTE* op = ostart;
    if (dstSize < 6 + 1 + 1 + 1 + 8) return 0;    
    if (srcSize < 12) return 0;    
    op += 6;    
    assert(op <= oend);
    {   CHECK_V_F(cSize, HUF_compress1X_usingCTable_internal(op, (size_t)(oend-op), ip, segmentSize, CTable, bmi2) );
        if (cSize==0) return 0;
        assert(cSize <= 65535);
        MEM_writeLE16(ostart, (U16)cSize);
        op += cSize;
    }
    ip += segmentSize;
    assert(op <= oend);
    {   CHECK_V_F(cSize, HUF_compress1X_usingCTable_internal(op, (size_t)(oend-op), ip, segmentSize, CTable, bmi2) );
        if (cSize==0) return 0;
        assert(cSize <= 65535);
        MEM_writeLE16(ostart+2, (U16)cSize);
        op += cSize;
    }
    ip += segmentSize;
    assert(op <= oend);
    {   CHECK_V_F(cSize, HUF_compress1X_usingCTable_internal(op, (size_t)(oend-op), ip, segmentSize, CTable, bmi2) );
        if (cSize==0) return 0;
        assert(cSize <= 65535);
        MEM_writeLE16(ostart+4, (U16)cSize);
        op += cSize;
    }
    ip += segmentSize;
    assert(op <= oend);
    assert(ip <= iend);
    {   CHECK_V_F(cSize, HUF_compress1X_usingCTable_internal(op, (size_t)(oend-op), ip, (size_t)(iend-ip), CTable, bmi2) );
        if (cSize==0) return 0;
        op += cSize;
    }
    return (size_t)(op-ostart);
}
size_t HUF_compress4X_usingCTable(void* dst, size_t dstSize, const void* src, size_t srcSize, const HUF_CElt* CTable)
{
    return HUF_compress4X_usingCTable_internal(dst, dstSize, src, srcSize, CTable,   0);
}
typedef enum { HUF_singleStream, HUF_fourStreams } HUF_nbStreams_e;
static size_t HUF_compressCTable_internal(
                BYTE* const ostart, BYTE* op, BYTE* const oend,
                const void* src, size_t srcSize,
                HUF_nbStreams_e nbStreams, const HUF_CElt* CTable, const int bmi2)
{
    size_t const cSize = (nbStreams==HUF_singleStream) ?
                         HUF_compress1X_usingCTable_internal(op, (size_t)(oend - op), src, srcSize, CTable, bmi2) :
                         HUF_compress4X_usingCTable_internal(op, (size_t)(oend - op), src, srcSize, CTable, bmi2);
    if (HUF_isError(cSize)) { return cSize; }
    if (cSize==0) { return 0; }    
    op += cSize;
    assert(op >= ostart);
    if ((size_t)(op-ostart) >= srcSize-1) { return 0; }
    return (size_t)(op-ostart);
}
typedef struct {
    unsigned count[HUF_SYMBOLVALUE_MAX + 1];
    HUF_CElt CTable[HUF_SYMBOLVALUE_MAX + 1];
    HUF_buildCTable_wksp_tables buildCTable_wksp;
} HUF_compress_tables_t;
static size_t
HUF_compress_internal (void* dst, size_t dstSize,
                 const void* src, size_t srcSize,
                       unsigned maxSymbolValue, unsigned huffLog,
                       HUF_nbStreams_e nbStreams,
                       void* workSpace, size_t wkspSize,
                       HUF_CElt* oldHufTable, HUF_repeat* repeat, int preferRepeat,
                 const int bmi2)
{
    HUF_compress_tables_t* const table = (HUF_compress_tables_t*)workSpace;
    BYTE* const ostart = (BYTE*)dst;
    BYTE* const oend = ostart + dstSize;
    BYTE* op = ostart;
    HUF_STATIC_ASSERT(sizeof(*table) <= HUF_WORKSPACE_SIZE);
    if (((size_t)workSpace & 3) != 0) return ERROR(GENERIC);   
    if (wkspSize < HUF_WORKSPACE_SIZE) return ERROR(workSpace_tooSmall);
    if (!srcSize) return 0;   
    if (!dstSize) return 0;   
    if (srcSize > HUF_BLOCKSIZE_MAX) return ERROR(srcSize_wrong);    
    if (huffLog > HUF_TABLELOG_MAX) return ERROR(tableLog_tooLarge);
    if (maxSymbolValue > HUF_SYMBOLVALUE_MAX) return ERROR(maxSymbolValue_tooLarge);
    if (!maxSymbolValue) maxSymbolValue = HUF_SYMBOLVALUE_MAX;
    if (!huffLog) huffLog = HUF_TABLELOG_DEFAULT;
    if (preferRepeat && repeat && *repeat == HUF_repeat_valid) {
        return HUF_compressCTable_internal(ostart, op, oend,
                                           src, srcSize,
                                           nbStreams, oldHufTable, bmi2);
    }
    {   CHECK_V_F(largest, HIST_count_wksp (table->count, &maxSymbolValue, (const BYTE*)src, srcSize, workSpace, wkspSize) );
        if (largest == srcSize) { *ostart = ((const BYTE*)src)[0]; return 1; }    
        if (largest <= (srcSize >> 7)+4) return 0;    
    }
    if ( repeat
      && *repeat == HUF_repeat_check
      && !HUF_validateCTable(oldHufTable, table->count, maxSymbolValue)) {
        *repeat = HUF_repeat_none;
    }
    if (preferRepeat && repeat && *repeat != HUF_repeat_none) {
        return HUF_compressCTable_internal(ostart, op, oend,
                                           src, srcSize,
                                           nbStreams, oldHufTable, bmi2);
    }
    huffLog = HUF_optimalTableLog(huffLog, srcSize, maxSymbolValue);
    {   size_t const maxBits = HUF_buildCTable_wksp(table->CTable, table->count,
                                            maxSymbolValue, huffLog,
                                            &table->buildCTable_wksp, sizeof(table->buildCTable_wksp));
        CHECK_F(maxBits);
        huffLog = (U32)maxBits;
        memset(table->CTable + (maxSymbolValue + 1), 0,
               sizeof(table->CTable) - ((maxSymbolValue + 1) * sizeof(HUF_CElt)));
    }
    {   CHECK_V_F(hSize, HUF_writeCTable (op, dstSize, table->CTable, maxSymbolValue, huffLog) );
        if (repeat && *repeat != HUF_repeat_none) {
            size_t const oldSize = HUF_estimateCompressedSize(oldHufTable, table->count, maxSymbolValue);
            size_t const newSize = HUF_estimateCompressedSize(table->CTable, table->count, maxSymbolValue);
            if (oldSize <= hSize + newSize || hSize + 12 >= srcSize) {
                return HUF_compressCTable_internal(ostart, op, oend,
                                                   src, srcSize,
                                                   nbStreams, oldHufTable, bmi2);
        }   }
        if (hSize + 12ul >= srcSize) { return 0; }
        op += hSize;
        if (repeat) { *repeat = HUF_repeat_none; }
        if (oldHufTable)
            memcpy(oldHufTable, table->CTable, sizeof(table->CTable));   
    }
    return HUF_compressCTable_internal(ostart, op, oend,
                                       src, srcSize,
                                       nbStreams, table->CTable, bmi2);
}
size_t HUF_compress1X_wksp (void* dst, size_t dstSize,
                      const void* src, size_t srcSize,
                      unsigned maxSymbolValue, unsigned huffLog,
                      void* workSpace, size_t wkspSize)
{
    return HUF_compress_internal(dst, dstSize, src, srcSize,
                                 maxSymbolValue, huffLog, HUF_singleStream,
                                 workSpace, wkspSize,
                                 NULL, NULL, 0, 0  );
}
size_t HUF_compress1X_repeat (void* dst, size_t dstSize,
                      const void* src, size_t srcSize,
                      unsigned maxSymbolValue, unsigned huffLog,
                      void* workSpace, size_t wkspSize,
                      HUF_CElt* hufTable, HUF_repeat* repeat, int preferRepeat, int bmi2)
{
    return HUF_compress_internal(dst, dstSize, src, srcSize,
                                 maxSymbolValue, huffLog, HUF_singleStream,
                                 workSpace, wkspSize, hufTable,
                                 repeat, preferRepeat, bmi2);
}
size_t HUF_compress1X (void* dst, size_t dstSize,
                 const void* src, size_t srcSize,
                 unsigned maxSymbolValue, unsigned huffLog)
{
    unsigned workSpace[HUF_WORKSPACE_SIZE_U32];
    return HUF_compress1X_wksp(dst, dstSize, src, srcSize, maxSymbolValue, huffLog, workSpace, sizeof(workSpace));
}
size_t HUF_compress4X_wksp (void* dst, size_t dstSize,
                      const void* src, size_t srcSize,
                      unsigned maxSymbolValue, unsigned huffLog,
                      void* workSpace, size_t wkspSize)
{
    return HUF_compress_internal(dst, dstSize, src, srcSize,
                                 maxSymbolValue, huffLog, HUF_fourStreams,
                                 workSpace, wkspSize,
                                 NULL, NULL, 0, 0  );
}
size_t HUF_compress4X_repeat (void* dst, size_t dstSize,
                      const void* src, size_t srcSize,
                      unsigned maxSymbolValue, unsigned huffLog,
                      void* workSpace, size_t wkspSize,
                      HUF_CElt* hufTable, HUF_repeat* repeat, int preferRepeat, int bmi2)
{
    return HUF_compress_internal(dst, dstSize, src, srcSize,
                                 maxSymbolValue, huffLog, HUF_fourStreams,
                                 workSpace, wkspSize,
                                 hufTable, repeat, preferRepeat, bmi2);
}
size_t HUF_compress2 (void* dst, size_t dstSize,
                const void* src, size_t srcSize,
                unsigned maxSymbolValue, unsigned huffLog)
{
    unsigned workSpace[HUF_WORKSPACE_SIZE_U32];
    return HUF_compress4X_wksp(dst, dstSize, src, srcSize, maxSymbolValue, huffLog, workSpace, sizeof(workSpace));
}
size_t HUF_compress (void* dst, size_t maxDstSize, const void* src, size_t srcSize)
{
    return HUF_compress2(dst, maxDstSize, src, srcSize, 255, HUF_TABLELOG_DEFAULT);
}
