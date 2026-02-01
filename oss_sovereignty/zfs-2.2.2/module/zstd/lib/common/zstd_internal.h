 

#ifndef ZSTD_CCOMMON_H_MODULE
#define ZSTD_CCOMMON_H_MODULE

 

 
#if !defined(ZSTD_NO_INTRINSICS) && defined(__ARM_NEON)
#include <arm_neon.h>
#endif
#include "compiler.h"
#include "mem.h"
#include "debug.h"                  
#include "error_private.h"
#define ZSTD_STATIC_LINKING_ONLY
#include "../zstd.h"
#define FSE_STATIC_LINKING_ONLY
#include "fse.h"
#define HUF_STATIC_LINKING_ONLY
#include "huf.h"
#ifndef XXH_STATIC_LINKING_ONLY
#  define XXH_STATIC_LINKING_ONLY   
#endif
#include "xxhash.h"                 

#if defined (__cplusplus)
extern "C" {
#endif

 
#define ZSTD_STATIC_ASSERT(c) DEBUG_STATIC_ASSERT(c)
#define FSE_isError  ERR_isError
#define HUF_isError  ERR_isError


 
#undef MIN
#undef MAX
#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))

 
static INLINE_KEYWORD UNUSED_ATTR
void _force_has_format_string(const char *format, ...) {
  (void)format;
}

 
#define _FORCE_HAS_FORMAT_STRING(...) \
  if (0) { \
    _force_has_format_string(__VA_ARGS__); \
  }

 
#define RETURN_ERROR_IF(cond, err, ...) \
  if (cond) { \
    RAWLOG(3, "%s:%d: ERROR!: check %s failed, returning %s", \
           __FILE__, __LINE__, ZSTD_QUOTE(cond), ZSTD_QUOTE(ERROR(err))); \
    _FORCE_HAS_FORMAT_STRING(__VA_ARGS__); \
    RAWLOG(3, ": " __VA_ARGS__); \
    RAWLOG(3, "\n"); \
    return ERROR(err); \
  }

 
#define RETURN_ERROR(err, ...) \
  do { \
    RAWLOG(3, "%s:%d: ERROR!: unconditional check failed, returning %s", \
           __FILE__, __LINE__, ZSTD_QUOTE(ERROR(err))); \
    _FORCE_HAS_FORMAT_STRING(__VA_ARGS__); \
    RAWLOG(3, ": " __VA_ARGS__); \
    RAWLOG(3, "\n"); \
    return ERROR(err); \
  } while(0);

 
#define FORWARD_IF_ERROR(err, ...) \
  do { \
    size_t const err_code = (err); \
    if (ERR_isError(err_code)) { \
      RAWLOG(3, "%s:%d: ERROR!: forwarding error in %s: %s", \
             __FILE__, __LINE__, ZSTD_QUOTE(err), ERR_getErrorName(err_code)); \
      _FORCE_HAS_FORMAT_STRING(__VA_ARGS__); \
      RAWLOG(3, ": " __VA_ARGS__); \
      RAWLOG(3, "\n"); \
      return err_code; \
    } \
  } while(0);


 
#define ZSTD_OPT_NUM    (1<<12)

#define ZSTD_REP_NUM      3                  
#define ZSTD_REP_MOVE     (ZSTD_REP_NUM-1)
static const U32 repStartValue[ZSTD_REP_NUM] = { 1, 4, 8 };

#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define BIT7 128
#define BIT6  64
#define BIT5  32
#define BIT4  16
#define BIT1   2
#define BIT0   1

#define ZSTD_WINDOWLOG_ABSOLUTEMIN 10
static const size_t ZSTD_fcs_fieldSize[4] = { 0, 2, 4, 8 };
static const size_t ZSTD_did_fieldSize[4] = { 0, 1, 2, 4 };

#define ZSTD_FRAMEIDSIZE 4    

#define ZSTD_BLOCKHEADERSIZE 3    
static const size_t ZSTD_blockHeaderSize = ZSTD_BLOCKHEADERSIZE;
typedef enum { bt_raw, bt_rle, bt_compressed, bt_reserved } blockType_e;

#define ZSTD_FRAMECHECKSUMSIZE 4

#define MIN_SEQUENCES_SIZE 1  
#define MIN_CBLOCK_SIZE (1   + 1   + MIN_SEQUENCES_SIZE  )    

#define HufLog 12
typedef enum { set_basic, set_rle, set_compressed, set_repeat } symbolEncodingType_e;

#define LONGNBSEQ 0x7F00

#define MINMATCH 3

#define Litbits  8
#define MaxLit ((1<<Litbits) - 1)
#define MaxML   52
#define MaxLL   35
#define DefaultMaxOff 28
#define MaxOff  31
#define MaxSeq MAX(MaxLL, MaxML)    
#define MLFSELog    9
#define LLFSELog    9
#define OffFSELog   8
#define MaxFSELog  MAX(MAX(MLFSELog, LLFSELog), OffFSELog)

static const U32 LL_bits[MaxLL+1] = { 0, 0, 0, 0, 0, 0, 0, 0,
                                      0, 0, 0, 0, 0, 0, 0, 0,
                                      1, 1, 1, 1, 2, 2, 3, 3,
                                      4, 6, 7, 8, 9,10,11,12,
                                     13,14,15,16 };
static const S16 LL_defaultNorm[MaxLL+1] = { 4, 3, 2, 2, 2, 2, 2, 2,
                                             2, 2, 2, 2, 2, 1, 1, 1,
                                             2, 2, 2, 2, 2, 2, 2, 2,
                                             2, 3, 2, 1, 1, 1, 1, 1,
                                            -1,-1,-1,-1 };
#define LL_DEFAULTNORMLOG 6   
static const U32 LL_defaultNormLog = LL_DEFAULTNORMLOG;

static const U32 ML_bits[MaxML+1] = { 0, 0, 0, 0, 0, 0, 0, 0,
                                      0, 0, 0, 0, 0, 0, 0, 0,
                                      0, 0, 0, 0, 0, 0, 0, 0,
                                      0, 0, 0, 0, 0, 0, 0, 0,
                                      1, 1, 1, 1, 2, 2, 3, 3,
                                      4, 4, 5, 7, 8, 9,10,11,
                                     12,13,14,15,16 };
static const S16 ML_defaultNorm[MaxML+1] = { 1, 4, 3, 2, 2, 2, 2, 2,
                                             2, 1, 1, 1, 1, 1, 1, 1,
                                             1, 1, 1, 1, 1, 1, 1, 1,
                                             1, 1, 1, 1, 1, 1, 1, 1,
                                             1, 1, 1, 1, 1, 1, 1, 1,
                                             1, 1, 1, 1, 1, 1,-1,-1,
                                            -1,-1,-1,-1,-1 };
#define ML_DEFAULTNORMLOG 6   
static const U32 ML_defaultNormLog = ML_DEFAULTNORMLOG;

static const S16 OF_defaultNorm[DefaultMaxOff+1] = { 1, 1, 1, 1, 1, 1, 2, 2,
                                                     2, 1, 1, 1, 1, 1, 1, 1,
                                                     1, 1, 1, 1, 1, 1, 1, 1,
                                                    -1,-1,-1,-1,-1 };
#define OF_DEFAULTNORMLOG 5   
static const U32 OF_defaultNormLog = OF_DEFAULTNORMLOG;


 
static void ZSTD_copy8(void* dst, const void* src) {
#if !defined(ZSTD_NO_INTRINSICS) && defined(__ARM_NEON)
    vst1_u8((uint8_t*)dst, vld1_u8((const uint8_t*)src));
#else
    memcpy(dst, src, 8);
#endif
}

#define COPY8(d,s) { ZSTD_copy8(d,s); d+=8; s+=8; }
static void ZSTD_copy16(void* dst, const void* src) {
#if !defined(ZSTD_NO_INTRINSICS) && defined(__ARM_NEON)
    vst1q_u8((uint8_t*)dst, vld1q_u8((const uint8_t*)src));
#else
    memcpy(dst, src, 16);
#endif
}
#define COPY16(d,s) { ZSTD_copy16(d,s); d+=16; s+=16; }

#define WILDCOPY_OVERLENGTH 32
#define WILDCOPY_VECLEN 16

typedef enum {
    ZSTD_no_overlap,
    ZSTD_overlap_src_before_dst
     
} ZSTD_overlap_e;

 
MEM_STATIC FORCE_INLINE_ATTR 
void ZSTD_wildcopy(void* dst, const void* src, ptrdiff_t length, ZSTD_overlap_e const ovtype)
{
    ptrdiff_t diff = (BYTE*)dst - (const BYTE*)src;
    const BYTE* ip = (const BYTE*)src;
    BYTE* op = (BYTE*)dst;
    BYTE* const oend = op + length;

    assert(diff >= 8 || (ovtype == ZSTD_no_overlap && diff <= -WILDCOPY_VECLEN));

    if (ovtype == ZSTD_overlap_src_before_dst && diff < WILDCOPY_VECLEN) {
         
        do {
            COPY8(op, ip)
        } while (op < oend);
    } else {
        assert(diff >= WILDCOPY_VECLEN || diff <= -WILDCOPY_VECLEN);
         
#ifndef __aarch64__
        do {
            COPY16(op, ip);
        }
        while (op < oend);
#else
        COPY16(op, ip);
        if (op >= oend) return;
        do {
            COPY16(op, ip);
            COPY16(op, ip);
        }
        while (op < oend);
#endif
    }
}

MEM_STATIC size_t ZSTD_limitCopy(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    size_t const length = MIN(dstCapacity, srcSize);
    if (length > 0) {
        memcpy(dst, src, length);
    }
    return length;
}

 
#define ZSTD_WORKSPACETOOLARGE_FACTOR 3

 
#define ZSTD_WORKSPACETOOLARGE_MAXDURATION 128


 
typedef struct seqDef_s {
    U32 offset;
    U16 litLength;
    U16 matchLength;
} seqDef;

typedef struct {
    seqDef* sequencesStart;
    seqDef* sequences;
    BYTE* litStart;
    BYTE* lit;
    BYTE* llCode;
    BYTE* mlCode;
    BYTE* ofCode;
    size_t maxNbSeq;
    size_t maxNbLit;
    U32   longLengthID;    
    U32   longLengthPos;
} seqStore_t;

typedef struct {
    U32 litLength;
    U32 matchLength;
} ZSTD_sequenceLength;

 
MEM_STATIC ZSTD_sequenceLength ZSTD_getSequenceLength(seqStore_t const* seqStore, seqDef const* seq)
{
    ZSTD_sequenceLength seqLen;
    seqLen.litLength = seq->litLength;
    seqLen.matchLength = seq->matchLength + MINMATCH;
    if (seqStore->longLengthPos == (U32)(seq - seqStore->sequencesStart)) {
        if (seqStore->longLengthID == 1) {
            seqLen.litLength += 0xFFFF;
        }
        if (seqStore->longLengthID == 2) {
            seqLen.matchLength += 0xFFFF;
        }
    }
    return seqLen;
}

 
typedef struct {
    size_t compressedSize;
    unsigned long long decompressedBound;
} ZSTD_frameSizeInfo;    

const seqStore_t* ZSTD_getSeqStore(const ZSTD_CCtx* ctx);    
void ZSTD_seqToCodes(const seqStore_t* seqStorePtr);    

 
void* ZSTD_malloc(size_t size, ZSTD_customMem customMem);
void* ZSTD_calloc(size_t size, ZSTD_customMem customMem);
void ZSTD_free(void* ptr, ZSTD_customMem customMem);


MEM_STATIC U32 ZSTD_highbit32(U32 val)    
{
    assert(val != 0);
    {
#   if defined(_MSC_VER)    
        unsigned long r=0;
        return _BitScanReverse(&r, val) ? (unsigned)r : 0;
#   elif defined(__GNUC__) && (__GNUC__ >= 3)    
        return __builtin_clz (val) ^ 31;
#   elif defined(__ICCARM__)     
        return 31 - __CLZ(val);
#   else    
        static const U32 DeBruijnClz[32] = { 0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30, 8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31 };
        U32 v = val;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        return DeBruijnClz[(v * 0x07C4ACDDU) >> 27];
#   endif
    }
}


 
void ZSTD_invalidateRepCodes(ZSTD_CCtx* cctx);    


typedef struct {
    blockType_e blockType;
    U32 lastBlock;
    U32 origSize;
} blockProperties_t;    

 
 
size_t ZSTD_getcBlockSize(const void* src, size_t srcSize,
                          blockProperties_t* bpPtr);

 
 
size_t ZSTD_decodeSeqHeaders(ZSTD_DCtx* dctx, int* nbSeqPtr,
                       const void* src, size_t srcSize);


#if defined (__cplusplus)
}
#endif

#endif    
