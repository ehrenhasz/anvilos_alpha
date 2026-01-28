#include <sys/zfs_context.h>
int LZ4_uncompress_unknownOutputSize(const char *source, char *dest,
    int isize, int maxOutputSize);
#define	COMPRESSIONLEVEL 12
#define	NOTCOMPRESSIBLE_CONFIRMATION 6
#if defined(_ZFS_BIG_ENDIAN)
#define	LZ4_BIG_ENDIAN 1
#else
#undef LZ4_BIG_ENDIAN
#endif
#ifndef LZ4_FORCE_MEMORY_ACCESS    
#  if defined(__GNUC__) && \
  ( defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) \
  || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__) )
#    define LZ4_FORCE_MEMORY_ACCESS 2
#  elif (defined(__INTEL_COMPILER) && !defined(_WIN32)) || defined(__GNUC__)
#    define LZ4_FORCE_MEMORY_ACCESS 1
#  endif
#endif
#undef	LZ4_FORCE_SW_BITCOUNT
#if defined(__sunos__)
#define	LZ4_FORCE_SW_BITCOUNT
#endif
#define	restrict
#ifdef GCC_VERSION
#undef GCC_VERSION
#endif
#define	GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#ifndef LZ4_FORCE_INLINE
#  ifdef _MSC_VER     
#    define LZ4_FORCE_INLINE static __forceinline
#  else
#    if defined (__cplusplus) || defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L    
#      ifdef __GNUC__
#        define LZ4_FORCE_INLINE static inline __attribute__((always_inline))
#      else
#        define LZ4_FORCE_INLINE static inline
#      endif
#    else
#      define LZ4_FORCE_INLINE static
#    endif  
#  endif   
#endif  
#if defined(__PPC64__) && defined(__LITTLE_ENDIAN__) && defined(__GNUC__) && !defined(__clang__)
#  define LZ4_FORCE_O2  __attribute__((optimize("O2")))
#  undef LZ4_FORCE_INLINE
#  define LZ4_FORCE_INLINE  static __inline __attribute__((optimize("O2"),always_inline))
#else
#  define LZ4_FORCE_O2
#endif
#ifndef expect
#if (defined(__GNUC__) && (__GNUC__ >= 3)) || (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 800)) || defined(__clang__)
#  define expect(expr,value)    (__builtin_expect ((expr),(value)) )
#else
#  define expect(expr,value)    (expr)
#endif
#endif
#ifndef likely
#define	likely(expr)	expect((expr) != 0, 1)
#endif
#ifndef unlikely
#define	unlikely(expr)	expect((expr) != 0, 0)
#endif
#ifndef _KERNEL
#include <stdlib.h>    
#include <string.h>    
#endif
#define ALLOC(s)          malloc(s)
#define ALLOC_AND_ZERO(s) calloc(1,s)
#define FREEMEM(p)        free(p)
#define MEM_INIT(p,v,s)   memset((p),(v),(s))
#define MINMATCH 4
#define WILDCOPYLENGTH 8
#define LASTLITERALS   5    
#define MFLIMIT       12    
#define MATCH_SAFEGUARD_DISTANCE  ((2*WILDCOPYLENGTH) - MINMATCH)    
#define FASTLOOP_SAFE_DISTANCE 64
#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)
#ifndef LZ4_DISTANCE_MAX    
#  define LZ4_DISTANCE_MAX 65535    
#endif
#define LZ4_DISTANCE_ABSOLUTE_MAX 65535
#if (LZ4_DISTANCE_MAX > LZ4_DISTANCE_ABSOLUTE_MAX)    
#  error "LZ4_DISTANCE_MAX is too big : must be <= 65535"
#endif
#define ML_BITS  4
#define ML_MASK  ((1U<<ML_BITS)-1)
#define RUN_BITS (8-ML_BITS)
#define RUN_MASK ((1U<<RUN_BITS)-1)
#define DEBUGLOG(l, ...) {}     
#ifndef assert
#define assert ASSERT
#endif
#ifndef _KERNEL
#include <limits.h>
#endif
#if defined(__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)  )
#ifndef _KERNEL
#include <stdint.h>
#endif
  typedef  uint8_t BYTE;
  typedef uint16_t U16;
  typedef uint32_t U32;
  typedef  int32_t S32;
  typedef uint64_t U64;
  typedef uintptr_t uptrval;
#else
# if UINT_MAX != 4294967295UL
#   error "LZ4 code (when not C++ or C99) assumes that sizeof(int) == 4"
# endif
  typedef unsigned char       BYTE;
  typedef unsigned short      U16;
  typedef unsigned int        U32;
  typedef   signed int        S32;
  typedef unsigned long long  U64;
  typedef size_t              uptrval;    
#endif
#if defined(__x86_64__)
  typedef U64    reg_t;    
#else
  typedef size_t reg_t;    
#endif
typedef enum {
    notLimited = 0,
    limitedOutput = 1,
    fillOutput = 2
} limitedOutput_directive;
#if defined(__GNUC__) && (__GNUC__ >= 4)
#define LZ4_memcpy(dst, src, size) __builtin_memcpy(dst, src, size)
#else
#define LZ4_memcpy(dst, src, size) memcpy(dst, src, size)
#endif
static unsigned LZ4_isLittleEndian(void)
{
    const union { U32 u; BYTE c[4]; } one = { 1 };    
    return one.c[0];
}
#if defined(LZ4_FORCE_MEMORY_ACCESS) && (LZ4_FORCE_MEMORY_ACCESS==2)
static U16 LZ4_read16(const void* memPtr) { return *(const U16*) memPtr; }
static void LZ4_write16(void* memPtr, U16 value) { *(U16*)memPtr = value; }
static void LZ4_write32(void* memPtr, U32 value) { *(U32*)memPtr = value; }
#elif defined(LZ4_FORCE_MEMORY_ACCESS) && (LZ4_FORCE_MEMORY_ACCESS==1)
typedef union { U16 u16; U32 u32; reg_t uArch; } __attribute__((packed)) unalign;
static U16 LZ4_read16(const void* ptr) { return ((const unalign*)ptr)->u16; }
static void LZ4_write32(void* memPtr, U32 value) { ((unalign*)memPtr)->u32 = value; }
#else   
static U16 LZ4_read16(const void* memPtr)
{
    U16 val; LZ4_memcpy(&val, memPtr, sizeof(val)); return val;
}
static void LZ4_write32(void* memPtr, U32 value)
{
    LZ4_memcpy(memPtr, &value, sizeof(value));
}
#endif  
static U16 LZ4_readLE16(const void* memPtr)
{
    if (LZ4_isLittleEndian()) {
        return LZ4_read16(memPtr);
    } else {
        const BYTE* p = (const BYTE*)memPtr;
        return (U16)((U16)p[0] + (p[1]<<8));
    }
}
LZ4_FORCE_INLINE
void LZ4_wildCopy8(void* dstPtr, const void* srcPtr, void* dstEnd)
{
    BYTE* d = (BYTE*)dstPtr;
    const BYTE* s = (const BYTE*)srcPtr;
    BYTE* const e = (BYTE*)dstEnd;
    do { LZ4_memcpy(d,s,8); d+=8; s+=8; } while (d<e);
}
static const unsigned inc32table[8] = {0, 1, 2,  1,  0,  4, 4, 4};
static const int      dec64table[8] = {0, 0, 0, -1, -4,  1, 2, 3};
#ifndef LZ4_FAST_DEC_LOOP
#  if defined __i386__ || defined _M_IX86 || defined __x86_64__ || defined _M_X64
#    define LZ4_FAST_DEC_LOOP 1
#  elif defined(__aarch64__) && !defined(__clang__)
#    define LZ4_FAST_DEC_LOOP 1
#  else
#    define LZ4_FAST_DEC_LOOP 0
#  endif
#endif
#if LZ4_FAST_DEC_LOOP
LZ4_FORCE_INLINE void
LZ4_memcpy_using_offset_base(BYTE* dstPtr, const BYTE* srcPtr, BYTE* dstEnd, const size_t offset)
{
    assert(srcPtr + offset == dstPtr);
    if (offset < 8) {
        LZ4_write32(dstPtr, 0);    
        dstPtr[0] = srcPtr[0];
        dstPtr[1] = srcPtr[1];
        dstPtr[2] = srcPtr[2];
        dstPtr[3] = srcPtr[3];
        srcPtr += inc32table[offset];
        LZ4_memcpy(dstPtr+4, srcPtr, 4);
        srcPtr -= dec64table[offset];
        dstPtr += 8;
    } else {
        LZ4_memcpy(dstPtr, srcPtr, 8);
        dstPtr += 8;
        srcPtr += 8;
    }
    LZ4_wildCopy8(dstPtr, srcPtr, dstEnd);
}
LZ4_FORCE_INLINE void
LZ4_wildCopy32(void* dstPtr, const void* srcPtr, void* dstEnd)
{
    BYTE* d = (BYTE*)dstPtr;
    const BYTE* s = (const BYTE*)srcPtr;
    BYTE* const e = (BYTE*)dstEnd;
    do { LZ4_memcpy(d,s,16); LZ4_memcpy(d+16,s+16,16); d+=32; s+=32; } while (d<e);
}
LZ4_FORCE_INLINE void
LZ4_memcpy_using_offset(BYTE* dstPtr, const BYTE* srcPtr, BYTE* dstEnd, const size_t offset)
{
    BYTE v[8];
    assert(dstEnd >= dstPtr + MINMATCH);
    switch(offset) {
    case 1:
        MEM_INIT(v, *srcPtr, 8);
        break;
    case 2:
        LZ4_memcpy(v, srcPtr, 2);
        LZ4_memcpy(&v[2], srcPtr, 2);
        LZ4_memcpy(&v[4], v, 4);
        break;
    case 4:
        LZ4_memcpy(v, srcPtr, 4);
        LZ4_memcpy(&v[4], srcPtr, 4);
        break;
    default:
        LZ4_memcpy_using_offset_base(dstPtr, srcPtr, dstEnd, offset);
        return;
    }
    LZ4_memcpy(dstPtr, v, 8);
    dstPtr += 8;
    while (dstPtr < dstEnd) {
        LZ4_memcpy(dstPtr, v, 8);
        dstPtr += 8;
    }
}
#endif
typedef enum { clearedTable = 0, byPtr, byU32, byU16 } tableType_t;
typedef enum { noDict = 0, withPrefix64k, usingExtDict, usingDictCtx } dict_directive;
typedef enum { noDictIssue = 0, dictSmall } dictIssue_directive;
typedef enum { endOnOutputSize = 0, endOnInputSize = 1 } endCondition_directive;
typedef enum { decode_full_block = 0, partial_decode = 1 } earlyEnd_directive;
typedef enum { loop_error = -2, initial_error = -1, ok = 0 } variable_length_error;
LZ4_FORCE_INLINE unsigned
read_variable_length(const BYTE**ip, const BYTE* lencheck,
                     int loop_check, int initial_check,
                     variable_length_error* error)
{
    U32 length = 0;
    U32 s;
    if (initial_check && unlikely((*ip) >= lencheck)) {     
        *error = initial_error;
        return length;
    }
    do {
        s = **ip;
        (*ip)++;
        length += s;
        if (loop_check && unlikely((*ip) >= lencheck)) {     
            *error = loop_error;
            return length;
        }
    } while (s==255);
    return length;
}
#define	LZ4_STATIC_ASSERT(c)	ASSERT(c)
LZ4_FORCE_INLINE int
LZ4_decompress_generic(
                 const char* const src,
                 char* const dst,
                 int srcSize,
                 int outputSize,          
                 endCondition_directive endOnInput,    
                 earlyEnd_directive partialDecoding,   
                 dict_directive dict,                  
                 const BYTE* const lowPrefix,   
                 const BYTE* const dictStart,   
                 const size_t dictSize          
                 )
{
    if ((src == NULL) || (outputSize < 0)) { return -1; }
    {   const BYTE* ip = (const BYTE*) src;
        const BYTE* const iend = ip + srcSize;
        BYTE* op = (BYTE*) dst;
        BYTE* const oend = op + outputSize;
        BYTE* cpy;
        const BYTE* const dictEnd = (dictStart == NULL) ? NULL : dictStart + dictSize;
        const int safeDecode = (endOnInput==endOnInputSize);
        const int checkOffset = ((safeDecode) && (dictSize < (int)(64 KB)));
        const BYTE* const shortiend = iend - (endOnInput ? 14 : 8)   - 2  ;
        const BYTE* const shortoend = oend - (endOnInput ? 14 : 8)   - 18  ;
        const BYTE* match;
        size_t offset;
        unsigned token;
        size_t length;
        DEBUGLOG(5, "LZ4_decompress_generic (srcSize:%i, dstSize:%i)", srcSize, outputSize);
        assert(lowPrefix <= op);
        if ((endOnInput) && (unlikely(outputSize==0))) {
            if (partialDecoding) return 0;
            return ((srcSize==1) && (*ip==0)) ? 0 : -1;
        }
        if ((!endOnInput) && (unlikely(outputSize==0))) { return (*ip==0 ? 1 : -1); }
        if ((endOnInput) && unlikely(srcSize==0)) { return -1; }
#if LZ4_FAST_DEC_LOOP
        if ((oend - op) < FASTLOOP_SAFE_DISTANCE) {
            DEBUGLOG(6, "skip fast decode loop");
            goto safe_decode;
        }
        while (1) {
            assert(oend - op >= FASTLOOP_SAFE_DISTANCE);
            if (endOnInput) { assert(ip < iend); }
            token = *ip++;
            length = token >> ML_BITS;   
            assert(!endOnInput || ip <= iend);  
            if (length == RUN_MASK) {
                variable_length_error error = ok;
                length += read_variable_length(&ip, iend-RUN_MASK, (int)endOnInput, (int)endOnInput, &error);
                if (error == initial_error) { goto _output_error; }
                if ((safeDecode) && unlikely((uptrval)(op)+length<(uptrval)(op))) { goto _output_error; }  
                if ((safeDecode) && unlikely((uptrval)(ip)+length<(uptrval)(ip))) { goto _output_error; }  
                cpy = op+length;
                LZ4_STATIC_ASSERT(MFLIMIT >= WILDCOPYLENGTH);
                if (endOnInput) {   
                    if ((cpy>oend-32) || (ip+length>iend-32)) { goto safe_literal_copy; }
                    LZ4_wildCopy32(op, ip, cpy);
                } else {    
                    if (cpy>oend-8) { goto safe_literal_copy; }
                    LZ4_wildCopy8(op, ip, cpy);  
                }
                ip += length; op = cpy;
            } else {
                cpy = op+length;
                if (endOnInput) {   
                    DEBUGLOG(7, "copy %u bytes in a 16-bytes stripe", (unsigned)length);
                    if (ip > iend-(16 + 1 )) { goto safe_literal_copy; }
                    LZ4_memcpy(op, ip, 16);
                } else {   
                    LZ4_memcpy(op, ip, 8);
                    if (length > 8) { LZ4_memcpy(op+8, ip+8, 8); }
                }
                ip += length; op = cpy;
            }
            offset = LZ4_readLE16(ip); ip+=2;
            match = op - offset;
            assert(match <= op);
            length = token & ML_MASK;
            if (length == ML_MASK) {
                variable_length_error error = ok;
                if ((checkOffset) && (unlikely(match + dictSize < lowPrefix))) { goto _output_error; }  
                length += read_variable_length(&ip, iend - LASTLITERALS + 1, (int)endOnInput, 0, &error);
                if (error != ok) { goto _output_error; }
                if ((safeDecode) && unlikely((uptrval)(op)+length<(uptrval)op)) { goto _output_error; }  
                length += MINMATCH;
                if (op + length >= oend - FASTLOOP_SAFE_DISTANCE) {
                    goto safe_match_copy;
                }
            } else {
                length += MINMATCH;
                if (op + length >= oend - FASTLOOP_SAFE_DISTANCE) {
                    goto safe_match_copy;
                }
                if ((dict == withPrefix64k) || (match >= lowPrefix)) {
                    if (offset >= 8) {
                        assert(match >= lowPrefix);
                        assert(match <= op);
                        assert(op + 18 <= oend);
                        LZ4_memcpy(op, match, 8);
                        LZ4_memcpy(op+8, match+8, 8);
                        LZ4_memcpy(op+16, match+16, 2);
                        op += length;
                        continue;
            }   }   }
            if (checkOffset && (unlikely(match + dictSize < lowPrefix))) { goto _output_error; }  
            if ((dict==usingExtDict) && (match < lowPrefix)) {
                if (unlikely(op+length > oend-LASTLITERALS)) {
                    if (partialDecoding) {
                        DEBUGLOG(7, "partialDecoding: dictionary match, close to dstEnd");
                        length = MIN(length, (size_t)(oend-op));
                    } else {
                        goto _output_error;   
                }   }
                if (length <= (size_t)(lowPrefix-match)) {
                    memmove(op, dictEnd - (lowPrefix-match), length);
                    op += length;
                } else {
                    size_t const copySize = (size_t)(lowPrefix - match);
                    size_t const restSize = length - copySize;
                    LZ4_memcpy(op, dictEnd - copySize, copySize);
                    op += copySize;
                    if (restSize > (size_t)(op - lowPrefix)) {   
                        BYTE* const endOfMatch = op + restSize;
                        const BYTE* copyFrom = lowPrefix;
                        while (op < endOfMatch) { *op++ = *copyFrom++; }
                    } else {
                        LZ4_memcpy(op, lowPrefix, restSize);
                        op += restSize;
                }   }
                continue;
            }
            cpy = op + length;
            assert((op <= oend) && (oend-op >= 32));
            if (unlikely(offset<16)) {
                LZ4_memcpy_using_offset(op, match, cpy, offset);
            } else {
                LZ4_wildCopy32(op, match, cpy);
            }
            op = cpy;    
        }
    safe_decode:
#endif
        while (1) {
            token = *ip++;
            length = token >> ML_BITS;   
            assert(!endOnInput || ip <= iend);  
            if ( (endOnInput ? length != RUN_MASK : length <= 8)
              && likely((endOnInput ? ip < shortiend : 1) & (op <= shortoend)) ) {
                LZ4_memcpy(op, ip, endOnInput ? 16 : 8);
                op += length; ip += length;
                length = token & ML_MASK;  
                offset = LZ4_readLE16(ip); ip += 2;
                match = op - offset;
                assert(match <= op);  
                if ( (length != ML_MASK)
                  && (offset >= 8)
                  && (dict==withPrefix64k || match >= lowPrefix) ) {
                    LZ4_memcpy(op + 0, match + 0, 8);
                    LZ4_memcpy(op + 8, match + 8, 8);
                    LZ4_memcpy(op +16, match +16, 2);
                    op += length + MINMATCH;
                    continue;
                }
                goto _copy_match;
            }
            if (length == RUN_MASK) {
                variable_length_error error = ok;
                length += read_variable_length(&ip, iend-RUN_MASK, (int)endOnInput, (int)endOnInput, &error);
                if (error == initial_error) { goto _output_error; }
                if ((safeDecode) && unlikely((uptrval)(op)+length<(uptrval)(op))) { goto _output_error; }  
                if ((safeDecode) && unlikely((uptrval)(ip)+length<(uptrval)(ip))) { goto _output_error; }  
            }
            cpy = op+length;
#if LZ4_FAST_DEC_LOOP
        safe_literal_copy:
#endif
            LZ4_STATIC_ASSERT(MFLIMIT >= WILDCOPYLENGTH);
            if ( ((endOnInput) && ((cpy>oend-MFLIMIT) || (ip+length>iend-(2+1+LASTLITERALS))) )
              || ((!endOnInput) && (cpy>oend-WILDCOPYLENGTH)) )
            {
                if (partialDecoding) {
                    assert(endOnInput);
                    DEBUGLOG(7, "partialDecoding: copying literals, close to input or output end")
                    DEBUGLOG(7, "partialDecoding: literal length = %u", (unsigned)length);
                    DEBUGLOG(7, "partialDecoding: remaining space in dstBuffer : %i", (int)(oend - op));
                    DEBUGLOG(7, "partialDecoding: remaining space in srcBuffer : %i", (int)(iend - ip));
                    if (ip+length > iend) {
                        length = (size_t)(iend-ip);
                        cpy = op + length;
                    }
                    if (cpy > oend) {
                        cpy = oend;
                        assert(op<=oend);
                        length = (size_t)(oend-op);
                    }
                } else {
                    if ((!endOnInput) && (cpy != oend)) { goto _output_error; }
                    if ((endOnInput) && ((ip+length != iend) || (cpy > oend))) {
                        DEBUGLOG(6, "should have been last run of literals")
                        DEBUGLOG(6, "ip(%p) + length(%i) = %p != iend (%p)", ip, (int)length, ip+length, iend);
                        DEBUGLOG(6, "or cpy(%p) > oend(%p)", cpy, oend);
                        goto _output_error;
                    }
                }
                memmove(op, ip, length);   
                ip += length;
                op += length;
                if (!partialDecoding || (cpy == oend) || (ip >= (iend-2))) {
                    break;
                }
            } else {
                LZ4_wildCopy8(op, ip, cpy);    
                ip += length; op = cpy;
            }
            offset = LZ4_readLE16(ip); ip+=2;
            match = op - offset;
            length = token & ML_MASK;
    _copy_match:
            if (length == ML_MASK) {
              variable_length_error error = ok;
              length += read_variable_length(&ip, iend - LASTLITERALS + 1, (int)endOnInput, 0, &error);
              if (error != ok) goto _output_error;
                if ((safeDecode) && unlikely((uptrval)(op)+length<(uptrval)op)) goto _output_error;    
            }
            length += MINMATCH;
#if LZ4_FAST_DEC_LOOP
        safe_match_copy:
#endif
            if ((checkOffset) && (unlikely(match + dictSize < lowPrefix))) goto _output_error;    
            if ((dict==usingExtDict) && (match < lowPrefix)) {
                if (unlikely(op+length > oend-LASTLITERALS)) {
                    if (partialDecoding) length = MIN(length, (size_t)(oend-op));
                    else goto _output_error;    
                }
                if (length <= (size_t)(lowPrefix-match)) {
                    memmove(op, dictEnd - (lowPrefix-match), length);
                    op += length;
                } else {
                    size_t const copySize = (size_t)(lowPrefix - match);
                    size_t const restSize = length - copySize;
                    LZ4_memcpy(op, dictEnd - copySize, copySize);
                    op += copySize;
                    if (restSize > (size_t)(op - lowPrefix)) {   
                        BYTE* const endOfMatch = op + restSize;
                        const BYTE* copyFrom = lowPrefix;
                        while (op < endOfMatch) *op++ = *copyFrom++;
                    } else {
                        LZ4_memcpy(op, lowPrefix, restSize);
                        op += restSize;
                }   }
                continue;
            }
            assert(match >= lowPrefix);
            cpy = op + length;
            assert(op<=oend);
            if (partialDecoding && (cpy > oend-MATCH_SAFEGUARD_DISTANCE)) {
                size_t const mlen = MIN(length, (size_t)(oend-op));
                const BYTE* const matchEnd = match + mlen;
                BYTE* const copyEnd = op + mlen;
                if (matchEnd > op) {    
                    while (op < copyEnd) { *op++ = *match++; }
                } else {
                    LZ4_memcpy(op, match, mlen);
                }
                op = copyEnd;
                if (op == oend) { break; }
                continue;
            }
            if (unlikely(offset<8)) {
                LZ4_write32(op, 0);    
                op[0] = match[0];
                op[1] = match[1];
                op[2] = match[2];
                op[3] = match[3];
                match += inc32table[offset];
                LZ4_memcpy(op+4, match, 4);
                match -= dec64table[offset];
            } else {
                LZ4_memcpy(op, match, 8);
                match += 8;
            }
            op += 8;
            if (unlikely(cpy > oend-MATCH_SAFEGUARD_DISTANCE)) {
                BYTE* const oCopyLimit = oend - (WILDCOPYLENGTH-1);
                if (cpy > oend-LASTLITERALS) { goto _output_error; }  
                if (op < oCopyLimit) {
                    LZ4_wildCopy8(op, match, oCopyLimit);
                    match += oCopyLimit - op;
                    op = oCopyLimit;
                }
                while (op < cpy) { *op++ = *match++; }
            } else {
                LZ4_memcpy(op, match, 8);
                if (length > 16)  { LZ4_wildCopy8(op+8, match+8, cpy); }
            }
            op = cpy;    
        }
        if (endOnInput) {
            DEBUGLOG(5, "decoded %i bytes", (int) (((char*)op)-dst));
           return (int) (((char*)op)-dst);      
       } else {
           return (int) (((const char*)ip)-src);    
       }
    _output_error:
        return (int) (-(((const char*)ip)-src))-1;
    }
}
int LZ4_uncompress_unknownOutputSize(const char* source, char* dest, int compressedSize, int maxDecompressedSize)
{
    return LZ4_decompress_generic(source, dest, compressedSize, maxDecompressedSize,
                                  endOnInputSize, decode_full_block, noDict,
                                  (BYTE*)dest, NULL, 0);
}
