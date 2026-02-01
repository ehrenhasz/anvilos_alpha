 


 
 
#ifndef ZSTD_HEAPMODE
#  define ZSTD_HEAPMODE 1
#endif

 
#ifndef ZSTD_LEGACY_SUPPORT
#  define ZSTD_LEGACY_SUPPORT 0
#endif

 
#ifndef ZSTD_MAXWINDOWSIZE_DEFAULT
#  define ZSTD_MAXWINDOWSIZE_DEFAULT (((U32)1 << ZSTD_WINDOWLOG_LIMIT_DEFAULT) + 1)
#endif

 
#ifndef ZSTD_NO_FORWARD_PROGRESS_MAX
#  define ZSTD_NO_FORWARD_PROGRESS_MAX 16
#endif


 
#include <string.h>       
#include "../common/cpu.h"          
#include "../common/mem.h"          
#define FSE_STATIC_LINKING_ONLY
#include "../common/fse.h"
#define HUF_STATIC_LINKING_ONLY
#include "../common/huf.h"
#include "../common/zstd_internal.h"   
#include "zstd_decompress_internal.h"    
#include "zstd_ddict.h"   
#include "zstd_decompress_block.h"    

#if defined(ZSTD_LEGACY_SUPPORT) && (ZSTD_LEGACY_SUPPORT>=1)
#  include "../legacy/zstd_legacy.h"
#endif


 
size_t ZSTD_sizeof_DCtx (const ZSTD_DCtx* dctx)
{
    if (dctx==NULL) return 0;    
    return sizeof(*dctx)
           + ZSTD_sizeof_DDict(dctx->ddictLocal)
           + dctx->inBuffSize + dctx->outBuffSize;
}

size_t ZSTD_estimateDCtxSize(void) { return sizeof(ZSTD_DCtx); }


static size_t ZSTD_startingInputLength(ZSTD_format_e format)
{
    size_t const startingInputLength = ZSTD_FRAMEHEADERSIZE_PREFIX(format);
     
    assert( (format == ZSTD_f_zstd1) || (format == ZSTD_f_zstd1_magicless) );
    return startingInputLength;
}

static void ZSTD_initDCtx_internal(ZSTD_DCtx* dctx)
{
    dctx->format = ZSTD_f_zstd1;   
    dctx->staticSize  = 0;
    dctx->maxWindowSize = ZSTD_MAXWINDOWSIZE_DEFAULT;
    dctx->ddict       = NULL;
    dctx->ddictLocal  = NULL;
    dctx->dictEnd     = NULL;
    dctx->ddictIsCold = 0;
    dctx->dictUses = ZSTD_dont_use;
    dctx->inBuff      = NULL;
    dctx->inBuffSize  = 0;
    dctx->outBuffSize = 0;
    dctx->streamStage = zdss_init;
    dctx->legacyContext = NULL;
    dctx->previousLegacyVersion = 0;
    dctx->noForwardProgress = 0;
    dctx->oversizedDuration = 0;
    dctx->bmi2 = ZSTD_cpuid_bmi2(ZSTD_cpuid());
    dctx->outBufferMode = ZSTD_obm_buffered;
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    dctx->dictContentEndForFuzzing = NULL;
#endif
}

ZSTD_DCtx* ZSTD_initStaticDCtx(void *workspace, size_t workspaceSize)
{
    ZSTD_DCtx* const dctx = (ZSTD_DCtx*) workspace;

    if ((size_t)workspace & 7) return NULL;   
    if (workspaceSize < sizeof(ZSTD_DCtx)) return NULL;   

    ZSTD_initDCtx_internal(dctx);
    dctx->staticSize = workspaceSize;
    dctx->inBuff = (char*)(dctx+1);
    return dctx;
}

ZSTD_DCtx* ZSTD_createDCtx_advanced(ZSTD_customMem customMem)
{
    if (!customMem.customAlloc ^ !customMem.customFree) return NULL;

    {   ZSTD_DCtx* const dctx = (ZSTD_DCtx*)ZSTD_malloc(sizeof(*dctx), customMem);
        if (!dctx) return NULL;
        dctx->customMem = customMem;
        ZSTD_initDCtx_internal(dctx);
        return dctx;
    }
}

ZSTD_DCtx* ZSTD_createDCtx(void)
{
    DEBUGLOG(3, "ZSTD_createDCtx");
    return ZSTD_createDCtx_advanced(ZSTD_defaultCMem);
}

static void ZSTD_clearDict(ZSTD_DCtx* dctx)
{
    ZSTD_freeDDict(dctx->ddictLocal);
    dctx->ddictLocal = NULL;
    dctx->ddict = NULL;
    dctx->dictUses = ZSTD_dont_use;
}

size_t ZSTD_freeDCtx(ZSTD_DCtx* dctx)
{
    if (dctx==NULL) return 0;    
    RETURN_ERROR_IF(dctx->staticSize, memory_allocation, "not compatible with static DCtx");
    {   ZSTD_customMem const cMem = dctx->customMem;
        ZSTD_clearDict(dctx);
        ZSTD_free(dctx->inBuff, cMem);
        dctx->inBuff = NULL;
#if defined(ZSTD_LEGACY_SUPPORT) && (ZSTD_LEGACY_SUPPORT >= 1)
        if (dctx->legacyContext)
            ZSTD_freeLegacyStreamContext(dctx->legacyContext, dctx->previousLegacyVersion);
#endif
        ZSTD_free(dctx, cMem);
        return 0;
    }
}

 
void ZSTD_copyDCtx(ZSTD_DCtx* dstDCtx, const ZSTD_DCtx* srcDCtx)
{
    size_t const toCopy = (size_t)((char*)(&dstDCtx->inBuff) - (char*)dstDCtx);
    memcpy(dstDCtx, srcDCtx, toCopy);   
}


 

 
unsigned ZSTD_isFrame(const void* buffer, size_t size)
{
    if (size < ZSTD_FRAMEIDSIZE) return 0;
    {   U32 const magic = MEM_readLE32(buffer);
        if (magic == ZSTD_MAGICNUMBER) return 1;
        if ((magic & ZSTD_MAGIC_SKIPPABLE_MASK) == ZSTD_MAGIC_SKIPPABLE_START) return 1;
    }
#if defined(ZSTD_LEGACY_SUPPORT) && (ZSTD_LEGACY_SUPPORT >= 1)
    if (ZSTD_isLegacy(buffer, size)) return 1;
#endif
    return 0;
}

 
static size_t ZSTD_frameHeaderSize_internal(const void* src, size_t srcSize, ZSTD_format_e format)
{
    size_t const minInputSize = ZSTD_startingInputLength(format);
    RETURN_ERROR_IF(srcSize < minInputSize, srcSize_wrong, "");

    {   BYTE const fhd = ((const BYTE*)src)[minInputSize-1];
        U32 const dictID= fhd & 3;
        U32 const singleSegment = (fhd >> 5) & 1;
        U32 const fcsId = fhd >> 6;
        return minInputSize + !singleSegment
             + ZSTD_did_fieldSize[dictID] + ZSTD_fcs_fieldSize[fcsId]
             + (singleSegment && !fcsId);
    }
}

 
size_t ZSTD_frameHeaderSize(const void* src, size_t srcSize)
{
    return ZSTD_frameHeaderSize_internal(src, srcSize, ZSTD_f_zstd1);
}


 
size_t ZSTD_getFrameHeader_advanced(ZSTD_frameHeader* zfhPtr, const void* src, size_t srcSize, ZSTD_format_e format)
{
    const BYTE* ip = (const BYTE*)src;
    size_t const minInputSize = ZSTD_startingInputLength(format);

    memset(zfhPtr, 0, sizeof(*zfhPtr));    
    if (srcSize < minInputSize) return minInputSize;
    RETURN_ERROR_IF(src==NULL, GENERIC, "invalid parameter");

    if ( (format != ZSTD_f_zstd1_magicless)
      && (MEM_readLE32(src) != ZSTD_MAGICNUMBER) ) {
        if ((MEM_readLE32(src) & ZSTD_MAGIC_SKIPPABLE_MASK) == ZSTD_MAGIC_SKIPPABLE_START) {
             
            if (srcSize < ZSTD_SKIPPABLEHEADERSIZE)
                return ZSTD_SKIPPABLEHEADERSIZE;  
            memset(zfhPtr, 0, sizeof(*zfhPtr));
            zfhPtr->frameContentSize = MEM_readLE32((const char *)src + ZSTD_FRAMEIDSIZE);
            zfhPtr->frameType = ZSTD_skippableFrame;
            return 0;
        }
        RETURN_ERROR(prefix_unknown, "");
    }

     
    {   size_t const fhsize = ZSTD_frameHeaderSize_internal(src, srcSize, format);
        if (srcSize < fhsize) return fhsize;
        zfhPtr->headerSize = (U32)fhsize;
    }

    {   BYTE const fhdByte = ip[minInputSize-1];
        size_t pos = minInputSize;
        U32 const dictIDSizeCode = fhdByte&3;
        U32 const checksumFlag = (fhdByte>>2)&1;
        U32 const singleSegment = (fhdByte>>5)&1;
        U32 const fcsID = fhdByte>>6;
        U64 windowSize = 0;
        U32 dictID = 0;
        U64 frameContentSize = ZSTD_CONTENTSIZE_UNKNOWN;
        RETURN_ERROR_IF((fhdByte & 0x08) != 0, frameParameter_unsupported,
                        "reserved bits, must be zero");

        if (!singleSegment) {
            BYTE const wlByte = ip[pos++];
            U32 const windowLog = (wlByte >> 3) + ZSTD_WINDOWLOG_ABSOLUTEMIN;
            RETURN_ERROR_IF(windowLog > ZSTD_WINDOWLOG_MAX, frameParameter_windowTooLarge, "");
            windowSize = (1ULL << windowLog);
            windowSize += (windowSize >> 3) * (wlByte&7);
        }
        switch(dictIDSizeCode)
        {
            default: assert(0);   
            case 0 : break;
            case 1 : dictID = ip[pos]; pos++; break;
            case 2 : dictID = MEM_readLE16(ip+pos); pos+=2; break;
            case 3 : dictID = MEM_readLE32(ip+pos); pos+=4; break;
        }
        switch(fcsID)
        {
            default: assert(0);   
            case 0 : if (singleSegment) frameContentSize = ip[pos]; break;
            case 1 : frameContentSize = MEM_readLE16(ip+pos)+256; break;
            case 2 : frameContentSize = MEM_readLE32(ip+pos); break;
            case 3 : frameContentSize = MEM_readLE64(ip+pos); break;
        }
        if (singleSegment) windowSize = frameContentSize;

        zfhPtr->frameType = ZSTD_frame;
        zfhPtr->frameContentSize = frameContentSize;
        zfhPtr->windowSize = windowSize;
        zfhPtr->blockSizeMax = (unsigned) MIN(windowSize, ZSTD_BLOCKSIZE_MAX);
        zfhPtr->dictID = dictID;
        zfhPtr->checksumFlag = checksumFlag;
    }
    return 0;
}

 
size_t ZSTD_getFrameHeader(ZSTD_frameHeader* zfhPtr, const void* src, size_t srcSize)
{
    return ZSTD_getFrameHeader_advanced(zfhPtr, src, srcSize, ZSTD_f_zstd1);
}


 
unsigned long long ZSTD_getFrameContentSize(const void *src, size_t srcSize)
{
#if defined(ZSTD_LEGACY_SUPPORT) && (ZSTD_LEGACY_SUPPORT >= 1)
    if (ZSTD_isLegacy(src, srcSize)) {
        unsigned long long const ret = ZSTD_getDecompressedSize_legacy(src, srcSize);
        return ret == 0 ? ZSTD_CONTENTSIZE_UNKNOWN : ret;
    }
#endif
    {   ZSTD_frameHeader zfh;
        if (ZSTD_getFrameHeader(&zfh, src, srcSize) != 0)
            return ZSTD_CONTENTSIZE_ERROR;
        if (zfh.frameType == ZSTD_skippableFrame) {
            return 0;
        } else {
            return zfh.frameContentSize;
    }   }
}

static size_t readSkippableFrameSize(void const* src, size_t srcSize)
{
    size_t const skippableHeaderSize = ZSTD_SKIPPABLEHEADERSIZE;
    U32 sizeU32;

    RETURN_ERROR_IF(srcSize < ZSTD_SKIPPABLEHEADERSIZE, srcSize_wrong, "");

    sizeU32 = MEM_readLE32((BYTE const*)src + ZSTD_FRAMEIDSIZE);
    RETURN_ERROR_IF((U32)(sizeU32 + ZSTD_SKIPPABLEHEADERSIZE) < sizeU32,
                    frameParameter_unsupported, "");
    {
        size_t const skippableSize = skippableHeaderSize + sizeU32;
        RETURN_ERROR_IF(skippableSize > srcSize, srcSize_wrong, "");
        return skippableSize;
    }
}

 
unsigned long long ZSTD_findDecompressedSize(const void* src, size_t srcSize)
{
    unsigned long long totalDstSize = 0;

    while (srcSize >= ZSTD_startingInputLength(ZSTD_f_zstd1)) {
        U32 const magicNumber = MEM_readLE32(src);

        if ((magicNumber & ZSTD_MAGIC_SKIPPABLE_MASK) == ZSTD_MAGIC_SKIPPABLE_START) {
            size_t const skippableSize = readSkippableFrameSize(src, srcSize);
            if (ZSTD_isError(skippableSize)) {
                return ZSTD_CONTENTSIZE_ERROR;
            }
            assert(skippableSize <= srcSize);

            src = (const BYTE *)src + skippableSize;
            srcSize -= skippableSize;
            continue;
        }

        {   unsigned long long const ret = ZSTD_getFrameContentSize(src, srcSize);
            if (ret >= ZSTD_CONTENTSIZE_ERROR) return ret;

             
            if (totalDstSize + ret < totalDstSize) return ZSTD_CONTENTSIZE_ERROR;
            totalDstSize += ret;
        }
        {   size_t const frameSrcSize = ZSTD_findFrameCompressedSize(src, srcSize);
            if (ZSTD_isError(frameSrcSize)) {
                return ZSTD_CONTENTSIZE_ERROR;
            }

            src = (const BYTE *)src + frameSrcSize;
            srcSize -= frameSrcSize;
        }
    }   

    if (srcSize) return ZSTD_CONTENTSIZE_ERROR;

    return totalDstSize;
}

 
unsigned long long ZSTD_getDecompressedSize(const void* src, size_t srcSize)
{
    unsigned long long const ret = ZSTD_getFrameContentSize(src, srcSize);
    ZSTD_STATIC_ASSERT(ZSTD_CONTENTSIZE_ERROR < ZSTD_CONTENTSIZE_UNKNOWN);
    return (ret >= ZSTD_CONTENTSIZE_ERROR) ? 0 : ret;
}


 
static size_t ZSTD_decodeFrameHeader(ZSTD_DCtx* dctx, const void* src, size_t headerSize)
{
    size_t const result = ZSTD_getFrameHeader_advanced(&(dctx->fParams), src, headerSize, dctx->format);
    if (ZSTD_isError(result)) return result;     
    RETURN_ERROR_IF(result>0, srcSize_wrong, "headerSize too small");
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
     
    RETURN_ERROR_IF(dctx->fParams.dictID && (dctx->dictID != dctx->fParams.dictID),
                    dictionary_wrong, "");
#endif
    if (dctx->fParams.checksumFlag) XXH64_reset(&dctx->xxhState, 0);
    return 0;
}

static ZSTD_frameSizeInfo ZSTD_errorFrameSizeInfo(size_t ret)
{
    ZSTD_frameSizeInfo frameSizeInfo;
    frameSizeInfo.compressedSize = ret;
    frameSizeInfo.decompressedBound = ZSTD_CONTENTSIZE_ERROR;
    return frameSizeInfo;
}

static ZSTD_frameSizeInfo ZSTD_findFrameSizeInfo(const void* src, size_t srcSize)
{
    ZSTD_frameSizeInfo frameSizeInfo;
    memset(&frameSizeInfo, 0, sizeof(ZSTD_frameSizeInfo));

#if defined(ZSTD_LEGACY_SUPPORT) && (ZSTD_LEGACY_SUPPORT >= 1)
    if (ZSTD_isLegacy(src, srcSize))
        return ZSTD_findFrameSizeInfoLegacy(src, srcSize);
#endif

    if ((srcSize >= ZSTD_SKIPPABLEHEADERSIZE)
        && (MEM_readLE32(src) & ZSTD_MAGIC_SKIPPABLE_MASK) == ZSTD_MAGIC_SKIPPABLE_START) {
        frameSizeInfo.compressedSize = readSkippableFrameSize(src, srcSize);
        assert(ZSTD_isError(frameSizeInfo.compressedSize) ||
               frameSizeInfo.compressedSize <= srcSize);
        return frameSizeInfo;
    } else {
        const BYTE* ip = (const BYTE*)src;
        const BYTE* const ipstart = ip;
        size_t remainingSize = srcSize;
        size_t nbBlocks = 0;
        ZSTD_frameHeader zfh;

         
        {   size_t const ret = ZSTD_getFrameHeader(&zfh, src, srcSize);
            if (ZSTD_isError(ret))
                return ZSTD_errorFrameSizeInfo(ret);
            if (ret > 0)
                return ZSTD_errorFrameSizeInfo(ERROR(srcSize_wrong));
        }

        ip += zfh.headerSize;
        remainingSize -= zfh.headerSize;

         
        while (1) {
            blockProperties_t blockProperties;
            size_t const cBlockSize = ZSTD_getcBlockSize(ip, remainingSize, &blockProperties);
            if (ZSTD_isError(cBlockSize))
                return ZSTD_errorFrameSizeInfo(cBlockSize);

            if (ZSTD_blockHeaderSize + cBlockSize > remainingSize)
                return ZSTD_errorFrameSizeInfo(ERROR(srcSize_wrong));

            ip += ZSTD_blockHeaderSize + cBlockSize;
            remainingSize -= ZSTD_blockHeaderSize + cBlockSize;
            nbBlocks++;

            if (blockProperties.lastBlock) break;
        }

         
        if (zfh.checksumFlag) {
            if (remainingSize < 4)
                return ZSTD_errorFrameSizeInfo(ERROR(srcSize_wrong));
            ip += 4;
        }

        frameSizeInfo.compressedSize = ip - ipstart;
        frameSizeInfo.decompressedBound = (zfh.frameContentSize != ZSTD_CONTENTSIZE_UNKNOWN)
                                        ? zfh.frameContentSize
                                        : nbBlocks * zfh.blockSizeMax;
        return frameSizeInfo;
    }
}

 
size_t ZSTD_findFrameCompressedSize(const void *src, size_t srcSize)
{
    ZSTD_frameSizeInfo const frameSizeInfo = ZSTD_findFrameSizeInfo(src, srcSize);
    return frameSizeInfo.compressedSize;
}

 
unsigned long long ZSTD_decompressBound(const void* src, size_t srcSize)
{
    unsigned long long bound = 0;
     
    while (srcSize > 0) {
        ZSTD_frameSizeInfo const frameSizeInfo = ZSTD_findFrameSizeInfo(src, srcSize);
        size_t const compressedSize = frameSizeInfo.compressedSize;
        unsigned long long const decompressedBound = frameSizeInfo.decompressedBound;
        if (ZSTD_isError(compressedSize) || decompressedBound == ZSTD_CONTENTSIZE_ERROR)
            return ZSTD_CONTENTSIZE_ERROR;
        assert(srcSize >= compressedSize);
        src = (const BYTE*)src + compressedSize;
        srcSize -= compressedSize;
        bound += decompressedBound;
    }
    return bound;
}


 

 
size_t ZSTD_insertBlock(ZSTD_DCtx* dctx, const void* blockStart, size_t blockSize)
{
    DEBUGLOG(5, "ZSTD_insertBlock: %u bytes", (unsigned)blockSize);
    ZSTD_checkContinuity(dctx, blockStart);
    dctx->previousDstEnd = (const char*)blockStart + blockSize;
    return blockSize;
}


static size_t ZSTD_copyRawBlock(void* dst, size_t dstCapacity,
                          const void* src, size_t srcSize)
{
    DEBUGLOG(5, "ZSTD_copyRawBlock");
    if (dst == NULL) {
        if (srcSize == 0) return 0;
        RETURN_ERROR(dstBuffer_null, "");
    }
    RETURN_ERROR_IF(srcSize > dstCapacity, dstSize_tooSmall, "");
    memcpy(dst, src, srcSize);
    return srcSize;
}

static size_t ZSTD_setRleBlock(void* dst, size_t dstCapacity,
                               BYTE b,
                               size_t regenSize)
{
    if (dst == NULL) {
        if (regenSize == 0) return 0;
        RETURN_ERROR(dstBuffer_null, "");
    }
    RETURN_ERROR_IF(regenSize > dstCapacity, dstSize_tooSmall, "");
    memset(dst, b, regenSize);
    return regenSize;
}


 
static size_t ZSTD_decompressFrame(ZSTD_DCtx* dctx,
                                   void* dst, size_t dstCapacity,
                             const void** srcPtr, size_t *srcSizePtr)
{
    const BYTE* ip = (const BYTE*)(*srcPtr);
    BYTE* const ostart = (BYTE* const)dst;
    BYTE* const oend = dstCapacity != 0 ? ostart + dstCapacity : ostart;
    BYTE* op = ostart;
    size_t remainingSrcSize = *srcSizePtr;

    DEBUGLOG(4, "ZSTD_decompressFrame (srcSize:%i)", (int)*srcSizePtr);

     
    RETURN_ERROR_IF(
        remainingSrcSize < ZSTD_FRAMEHEADERSIZE_MIN(dctx->format)+ZSTD_blockHeaderSize,
        srcSize_wrong, "");

     
    {   size_t const frameHeaderSize = ZSTD_frameHeaderSize_internal(
                ip, ZSTD_FRAMEHEADERSIZE_PREFIX(dctx->format), dctx->format);
        if (ZSTD_isError(frameHeaderSize)) return frameHeaderSize;
        RETURN_ERROR_IF(remainingSrcSize < frameHeaderSize+ZSTD_blockHeaderSize,
                        srcSize_wrong, "");
        FORWARD_IF_ERROR( ZSTD_decodeFrameHeader(dctx, ip, frameHeaderSize) , "");
        ip += frameHeaderSize; remainingSrcSize -= frameHeaderSize;
    }

     
    while (1) {
        size_t decodedSize;
        blockProperties_t blockProperties;
        size_t const cBlockSize = ZSTD_getcBlockSize(ip, remainingSrcSize, &blockProperties);
        if (ZSTD_isError(cBlockSize)) return cBlockSize;

        ip += ZSTD_blockHeaderSize;
        remainingSrcSize -= ZSTD_blockHeaderSize;
        RETURN_ERROR_IF(cBlockSize > remainingSrcSize, srcSize_wrong, "");

        switch(blockProperties.blockType)
        {
        case bt_compressed:
            decodedSize = ZSTD_decompressBlock_internal(dctx, op, oend-op, ip, cBlockSize,   1);
            break;
        case bt_raw :
            decodedSize = ZSTD_copyRawBlock(op, oend-op, ip, cBlockSize);
            break;
        case bt_rle :
            decodedSize = ZSTD_setRleBlock(op, oend-op, *ip, blockProperties.origSize);
            break;
        case bt_reserved :
        default:
            RETURN_ERROR(corruption_detected, "invalid block type");
        }

        if (ZSTD_isError(decodedSize)) return decodedSize;
        if (dctx->fParams.checksumFlag)
            XXH64_update(&dctx->xxhState, op, decodedSize);
        if (decodedSize != 0)
            op += decodedSize;
        assert(ip != NULL);
        ip += cBlockSize;
        remainingSrcSize -= cBlockSize;
        if (blockProperties.lastBlock) break;
    }

    if (dctx->fParams.frameContentSize != ZSTD_CONTENTSIZE_UNKNOWN) {
        RETURN_ERROR_IF((U64)(op-ostart) != dctx->fParams.frameContentSize,
                        corruption_detected, "");
    }
    if (dctx->fParams.checksumFlag) {  
        U32 const checkCalc = (U32)XXH64_digest(&dctx->xxhState);
        U32 checkRead;
        RETURN_ERROR_IF(remainingSrcSize<4, checksum_wrong, "");
        checkRead = MEM_readLE32(ip);
        RETURN_ERROR_IF(checkRead != checkCalc, checksum_wrong, "");
        ip += 4;
        remainingSrcSize -= 4;
    }

     
    *srcPtr = ip;
    *srcSizePtr = remainingSrcSize;
    return op-ostart;
}

static size_t ZSTD_decompressMultiFrame(ZSTD_DCtx* dctx,
                                        void* dst, size_t dstCapacity,
                                  const void* src, size_t srcSize,
                                  const void* dict, size_t dictSize,
                                  const ZSTD_DDict* ddict)
{
    void* const dststart = dst;
    int moreThan1Frame = 0;

    DEBUGLOG(5, "ZSTD_decompressMultiFrame");
    assert(dict==NULL || ddict==NULL);   

    if (ddict) {
        dict = ZSTD_DDict_dictContent(ddict);
        dictSize = ZSTD_DDict_dictSize(ddict);
    }

    while (srcSize >= ZSTD_startingInputLength(dctx->format)) {

#if defined(ZSTD_LEGACY_SUPPORT) && (ZSTD_LEGACY_SUPPORT >= 1)
        if (ZSTD_isLegacy(src, srcSize)) {
            size_t decodedSize;
            size_t const frameSize = ZSTD_findFrameCompressedSizeLegacy(src, srcSize);
            if (ZSTD_isError(frameSize)) return frameSize;
            RETURN_ERROR_IF(dctx->staticSize, memory_allocation,
                "legacy support is not compatible with static dctx");

            decodedSize = ZSTD_decompressLegacy(dst, dstCapacity, src, frameSize, dict, dictSize);
            if (ZSTD_isError(decodedSize)) return decodedSize;

            assert(decodedSize <=- dstCapacity);
            dst = (BYTE*)dst + decodedSize;
            dstCapacity -= decodedSize;

            src = (const BYTE*)src + frameSize;
            srcSize -= frameSize;

            continue;
        }
#endif

        {   U32 const magicNumber = MEM_readLE32(src);
            DEBUGLOG(4, "reading magic number %08X (expecting %08X)",
                        (unsigned)magicNumber, ZSTD_MAGICNUMBER);
            if ((magicNumber & ZSTD_MAGIC_SKIPPABLE_MASK) == ZSTD_MAGIC_SKIPPABLE_START) {
                size_t const skippableSize = readSkippableFrameSize(src, srcSize);
                FORWARD_IF_ERROR(skippableSize, "readSkippableFrameSize failed");
                assert(skippableSize <= srcSize);

                src = (const BYTE *)src + skippableSize;
                srcSize -= skippableSize;
                continue;
        }   }

        if (ddict) {
             
            FORWARD_IF_ERROR(ZSTD_decompressBegin_usingDDict(dctx, ddict), "");
        } else {
             
            FORWARD_IF_ERROR(ZSTD_decompressBegin_usingDict(dctx, dict, dictSize), "");
        }
        ZSTD_checkContinuity(dctx, dst);

        {   const size_t res = ZSTD_decompressFrame(dctx, dst, dstCapacity,
                                                    &src, &srcSize);
            RETURN_ERROR_IF(
                (ZSTD_getErrorCode(res) == ZSTD_error_prefix_unknown)
             && (moreThan1Frame==1),
                srcSize_wrong,
                "at least one frame successfully completed, but following "
                "bytes are garbage: it's more likely to be a srcSize error, "
                "specifying more bytes than compressed size of frame(s). This "
                "error message replaces ERROR(prefix_unknown), which would be "
                "confusing, as the first header is actually correct. Note that "
                "one could be unlucky, it might be a corruption error instead, "
                "happening right at the place where we expect zstd magic "
                "bytes. But this is _much_ less likely than a srcSize field "
                "error.");
            if (ZSTD_isError(res)) return res;
            assert(res <= dstCapacity);
            if (res != 0)
                dst = (BYTE*)dst + res;
            dstCapacity -= res;
        }
        moreThan1Frame = 1;
    }   

    RETURN_ERROR_IF(srcSize, srcSize_wrong, "input not entirely consumed");

    return (BYTE*)dst - (BYTE*)dststart;
}

size_t ZSTD_decompress_usingDict(ZSTD_DCtx* dctx,
                                 void* dst, size_t dstCapacity,
                           const void* src, size_t srcSize,
                           const void* dict, size_t dictSize)
{
    return ZSTD_decompressMultiFrame(dctx, dst, dstCapacity, src, srcSize, dict, dictSize, NULL);
}


static ZSTD_DDict const* ZSTD_getDDict(ZSTD_DCtx* dctx)
{
    switch (dctx->dictUses) {
    default:
        assert(0  );
         
    case ZSTD_dont_use:
        ZSTD_clearDict(dctx);
        return NULL;
    case ZSTD_use_indefinitely:
        return dctx->ddict;
    case ZSTD_use_once:
        dctx->dictUses = ZSTD_dont_use;
        return dctx->ddict;
    }
}

size_t ZSTD_decompressDCtx(ZSTD_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    return ZSTD_decompress_usingDDict(dctx, dst, dstCapacity, src, srcSize, ZSTD_getDDict(dctx));
}


size_t ZSTD_decompress(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
#if defined(ZSTD_HEAPMODE) && (ZSTD_HEAPMODE>=1)
    size_t regenSize;
    ZSTD_DCtx* const dctx = ZSTD_createDCtx();
    RETURN_ERROR_IF(dctx==NULL, memory_allocation, "NULL pointer!");
    regenSize = ZSTD_decompressDCtx(dctx, dst, dstCapacity, src, srcSize);
    ZSTD_freeDCtx(dctx);
    return regenSize;
#else    
    ZSTD_DCtx dctx;
    ZSTD_initDCtx_internal(&dctx);
    return ZSTD_decompressDCtx(&dctx, dst, dstCapacity, src, srcSize);
#endif
}


 
size_t ZSTD_nextSrcSizeToDecompress(ZSTD_DCtx* dctx) { return dctx->expected; }

 
static size_t ZSTD_nextSrcSizeToDecompressWithInputSize(ZSTD_DCtx* dctx, size_t inputSize) {
    if (!(dctx->stage == ZSTDds_decompressBlock || dctx->stage == ZSTDds_decompressLastBlock))
        return dctx->expected;
    if (dctx->bType != bt_raw)
        return dctx->expected;
    return MIN(MAX(inputSize, 1), dctx->expected);
}

ZSTD_nextInputType_e ZSTD_nextInputType(ZSTD_DCtx* dctx) {
    switch(dctx->stage)
    {
    default:    
        assert(0);
    case ZSTDds_getFrameHeaderSize:
    case ZSTDds_decodeFrameHeader:
        return ZSTDnit_frameHeader;
    case ZSTDds_decodeBlockHeader:
        return ZSTDnit_blockHeader;
    case ZSTDds_decompressBlock:
        return ZSTDnit_block;
    case ZSTDds_decompressLastBlock:
        return ZSTDnit_lastBlock;
    case ZSTDds_checkChecksum:
        return ZSTDnit_checksum;
    case ZSTDds_decodeSkippableHeader:
    case ZSTDds_skipFrame:
        return ZSTDnit_skippableFrame;
    }
}

static int ZSTD_isSkipFrame(ZSTD_DCtx* dctx) { return dctx->stage == ZSTDds_skipFrame; }

 
size_t ZSTD_decompressContinue(ZSTD_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
    DEBUGLOG(5, "ZSTD_decompressContinue (srcSize:%u)", (unsigned)srcSize);
     
    RETURN_ERROR_IF(srcSize != ZSTD_nextSrcSizeToDecompressWithInputSize(dctx, srcSize), srcSize_wrong, "not allowed");
    if (dstCapacity) ZSTD_checkContinuity(dctx, dst);

    switch (dctx->stage)
    {
    case ZSTDds_getFrameHeaderSize :
        assert(src != NULL);
        if (dctx->format == ZSTD_f_zstd1) {   
            assert(srcSize >= ZSTD_FRAMEIDSIZE);   
            if ((MEM_readLE32(src) & ZSTD_MAGIC_SKIPPABLE_MASK) == ZSTD_MAGIC_SKIPPABLE_START) {         
                memcpy(dctx->headerBuffer, src, srcSize);
                dctx->expected = ZSTD_SKIPPABLEHEADERSIZE - srcSize;   
                dctx->stage = ZSTDds_decodeSkippableHeader;
                return 0;
        }   }
        dctx->headerSize = ZSTD_frameHeaderSize_internal(src, srcSize, dctx->format);
        if (ZSTD_isError(dctx->headerSize)) return dctx->headerSize;
        memcpy(dctx->headerBuffer, src, srcSize);
        dctx->expected = dctx->headerSize - srcSize;
        dctx->stage = ZSTDds_decodeFrameHeader;
        return 0;

    case ZSTDds_decodeFrameHeader:
        assert(src != NULL);
        memcpy(dctx->headerBuffer + (dctx->headerSize - srcSize), src, srcSize);
        FORWARD_IF_ERROR(ZSTD_decodeFrameHeader(dctx, dctx->headerBuffer, dctx->headerSize), "");
        dctx->expected = ZSTD_blockHeaderSize;
        dctx->stage = ZSTDds_decodeBlockHeader;
        return 0;

    case ZSTDds_decodeBlockHeader:
        {   blockProperties_t bp;
            size_t const cBlockSize = ZSTD_getcBlockSize(src, ZSTD_blockHeaderSize, &bp);
            if (ZSTD_isError(cBlockSize)) return cBlockSize;
            RETURN_ERROR_IF(cBlockSize > dctx->fParams.blockSizeMax, corruption_detected, "Block Size Exceeds Maximum");
            dctx->expected = cBlockSize;
            dctx->bType = bp.blockType;
            dctx->rleSize = bp.origSize;
            if (cBlockSize) {
                dctx->stage = bp.lastBlock ? ZSTDds_decompressLastBlock : ZSTDds_decompressBlock;
                return 0;
            }
             
            if (bp.lastBlock) {
                if (dctx->fParams.checksumFlag) {
                    dctx->expected = 4;
                    dctx->stage = ZSTDds_checkChecksum;
                } else {
                    dctx->expected = 0;  
                    dctx->stage = ZSTDds_getFrameHeaderSize;
                }
            } else {
                dctx->expected = ZSTD_blockHeaderSize;   
                dctx->stage = ZSTDds_decodeBlockHeader;
            }
            return 0;
        }

    case ZSTDds_decompressLastBlock:
    case ZSTDds_decompressBlock:
        DEBUGLOG(5, "ZSTD_decompressContinue: case ZSTDds_decompressBlock");
        {   size_t rSize;
            switch(dctx->bType)
            {
            case bt_compressed:
                DEBUGLOG(5, "ZSTD_decompressContinue: case bt_compressed");
                rSize = ZSTD_decompressBlock_internal(dctx, dst, dstCapacity, src, srcSize,   1);
                dctx->expected = 0;   
                break;
            case bt_raw :
                assert(srcSize <= dctx->expected);
                rSize = ZSTD_copyRawBlock(dst, dstCapacity, src, srcSize);
                FORWARD_IF_ERROR(rSize, "ZSTD_copyRawBlock failed");
                assert(rSize == srcSize);
                dctx->expected -= rSize;
                break;
            case bt_rle :
                rSize = ZSTD_setRleBlock(dst, dstCapacity, *(const BYTE*)src, dctx->rleSize);
                dctx->expected = 0;   
                break;
            case bt_reserved :    
            default:
                RETURN_ERROR(corruption_detected, "invalid block type");
            }
            FORWARD_IF_ERROR(rSize, "");
            RETURN_ERROR_IF(rSize > dctx->fParams.blockSizeMax, corruption_detected, "Decompressed Block Size Exceeds Maximum");
            DEBUGLOG(5, "ZSTD_decompressContinue: decoded size from block : %u", (unsigned)rSize);
            dctx->decodedSize += rSize;
            if (dctx->fParams.checksumFlag) XXH64_update(&dctx->xxhState, dst, rSize);
            dctx->previousDstEnd = (char*)dst + rSize;

             
            if (dctx->expected > 0) {
                return rSize;
            }

            if (dctx->stage == ZSTDds_decompressLastBlock) {    
                DEBUGLOG(4, "ZSTD_decompressContinue: decoded size from frame : %u", (unsigned)dctx->decodedSize);
                RETURN_ERROR_IF(
                    dctx->fParams.frameContentSize != ZSTD_CONTENTSIZE_UNKNOWN
                 && dctx->decodedSize != dctx->fParams.frameContentSize,
                    corruption_detected, "");
                if (dctx->fParams.checksumFlag) {   
                    dctx->expected = 4;
                    dctx->stage = ZSTDds_checkChecksum;
                } else {
                    dctx->expected = 0;    
                    dctx->stage = ZSTDds_getFrameHeaderSize;
                }
            } else {
                dctx->stage = ZSTDds_decodeBlockHeader;
                dctx->expected = ZSTD_blockHeaderSize;
            }
            return rSize;
        }

    case ZSTDds_checkChecksum:
        assert(srcSize == 4);   
        {   U32 const h32 = (U32)XXH64_digest(&dctx->xxhState);
            U32 const check32 = MEM_readLE32(src);
            DEBUGLOG(4, "ZSTD_decompressContinue: checksum : calculated %08X :: %08X read", (unsigned)h32, (unsigned)check32);
            RETURN_ERROR_IF(check32 != h32, checksum_wrong, "");
            dctx->expected = 0;
            dctx->stage = ZSTDds_getFrameHeaderSize;
            return 0;
        }

    case ZSTDds_decodeSkippableHeader:
        assert(src != NULL);
        assert(srcSize <= ZSTD_SKIPPABLEHEADERSIZE);
        memcpy(dctx->headerBuffer + (ZSTD_SKIPPABLEHEADERSIZE - srcSize), src, srcSize);    
        dctx->expected = MEM_readLE32(dctx->headerBuffer + ZSTD_FRAMEIDSIZE);    
        dctx->stage = ZSTDds_skipFrame;
        return 0;

    case ZSTDds_skipFrame:
        dctx->expected = 0;
        dctx->stage = ZSTDds_getFrameHeaderSize;
        return 0;

    default:
        assert(0);    
        RETURN_ERROR(GENERIC, "impossible to reach");    
    }
}


static size_t ZSTD_refDictContent(ZSTD_DCtx* dctx, const void* dict, size_t dictSize)
{
    dctx->dictEnd = dctx->previousDstEnd;
    dctx->virtualStart = (const char*)dict - ((const char*)(dctx->previousDstEnd) - (const char*)(dctx->prefixStart));
    dctx->prefixStart = dict;
    dctx->previousDstEnd = (const char*)dict + dictSize;
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    dctx->dictContentBeginForFuzzing = dctx->prefixStart;
    dctx->dictContentEndForFuzzing = dctx->previousDstEnd;
#endif
    return 0;
}

 
size_t
ZSTD_loadDEntropy(ZSTD_entropyDTables_t* entropy,
                  const void* const dict, size_t const dictSize)
{
    const BYTE* dictPtr = (const BYTE*)dict;
    const BYTE* const dictEnd = dictPtr + dictSize;

    RETURN_ERROR_IF(dictSize <= 8, dictionary_corrupted, "dict is too small");
    assert(MEM_readLE32(dict) == ZSTD_MAGIC_DICTIONARY);    
    dictPtr += 8;    

    ZSTD_STATIC_ASSERT(offsetof(ZSTD_entropyDTables_t, OFTable) == offsetof(ZSTD_entropyDTables_t, LLTable) + sizeof(entropy->LLTable));
    ZSTD_STATIC_ASSERT(offsetof(ZSTD_entropyDTables_t, MLTable) == offsetof(ZSTD_entropyDTables_t, OFTable) + sizeof(entropy->OFTable));
    ZSTD_STATIC_ASSERT(sizeof(entropy->LLTable) + sizeof(entropy->OFTable) + sizeof(entropy->MLTable) >= HUF_DECOMPRESS_WORKSPACE_SIZE);
    {   void* const workspace = &entropy->LLTable;    
        size_t const workspaceSize = sizeof(entropy->LLTable) + sizeof(entropy->OFTable) + sizeof(entropy->MLTable);
#ifdef HUF_FORCE_DECOMPRESS_X1
         
        size_t const hSize = HUF_readDTableX1_wksp(entropy->hufTable,
                                                dictPtr, dictEnd - dictPtr,
                                                workspace, workspaceSize);
#else
        size_t const hSize = HUF_readDTableX2_wksp(entropy->hufTable,
                                                dictPtr, dictEnd - dictPtr,
                                                workspace, workspaceSize);
#endif
        RETURN_ERROR_IF(HUF_isError(hSize), dictionary_corrupted, "");
        dictPtr += hSize;
    }

    {   short offcodeNCount[MaxOff+1];
        unsigned offcodeMaxValue = MaxOff, offcodeLog;
        size_t const offcodeHeaderSize = FSE_readNCount(offcodeNCount, &offcodeMaxValue, &offcodeLog, dictPtr, dictEnd-dictPtr);
        RETURN_ERROR_IF(FSE_isError(offcodeHeaderSize), dictionary_corrupted, "");
        RETURN_ERROR_IF(offcodeMaxValue > MaxOff, dictionary_corrupted, "");
        RETURN_ERROR_IF(offcodeLog > OffFSELog, dictionary_corrupted, "");
        ZSTD_buildFSETable( entropy->OFTable,
                            offcodeNCount, offcodeMaxValue,
                            OF_base, OF_bits,
                            offcodeLog);
        dictPtr += offcodeHeaderSize;
    }

    {   short matchlengthNCount[MaxML+1];
        unsigned matchlengthMaxValue = MaxML, matchlengthLog;
        size_t const matchlengthHeaderSize = FSE_readNCount(matchlengthNCount, &matchlengthMaxValue, &matchlengthLog, dictPtr, dictEnd-dictPtr);
        RETURN_ERROR_IF(FSE_isError(matchlengthHeaderSize), dictionary_corrupted, "");
        RETURN_ERROR_IF(matchlengthMaxValue > MaxML, dictionary_corrupted, "");
        RETURN_ERROR_IF(matchlengthLog > MLFSELog, dictionary_corrupted, "");
        ZSTD_buildFSETable( entropy->MLTable,
                            matchlengthNCount, matchlengthMaxValue,
                            ML_base, ML_bits,
                            matchlengthLog);
        dictPtr += matchlengthHeaderSize;
    }

    {   short litlengthNCount[MaxLL+1];
        unsigned litlengthMaxValue = MaxLL, litlengthLog;
        size_t const litlengthHeaderSize = FSE_readNCount(litlengthNCount, &litlengthMaxValue, &litlengthLog, dictPtr, dictEnd-dictPtr);
        RETURN_ERROR_IF(FSE_isError(litlengthHeaderSize), dictionary_corrupted, "");
        RETURN_ERROR_IF(litlengthMaxValue > MaxLL, dictionary_corrupted, "");
        RETURN_ERROR_IF(litlengthLog > LLFSELog, dictionary_corrupted, "");
        ZSTD_buildFSETable( entropy->LLTable,
                            litlengthNCount, litlengthMaxValue,
                            LL_base, LL_bits,
                            litlengthLog);
        dictPtr += litlengthHeaderSize;
    }

    RETURN_ERROR_IF(dictPtr+12 > dictEnd, dictionary_corrupted, "");
    {   int i;
        size_t const dictContentSize = (size_t)(dictEnd - (dictPtr+12));
        for (i=0; i<3; i++) {
            U32 const rep = MEM_readLE32(dictPtr); dictPtr += 4;
            RETURN_ERROR_IF(rep==0 || rep > dictContentSize,
                            dictionary_corrupted, "");
            entropy->rep[i] = rep;
    }   }

    return dictPtr - (const BYTE*)dict;
}

static size_t ZSTD_decompress_insertDictionary(ZSTD_DCtx* dctx, const void* dict, size_t dictSize)
{
    if (dictSize < 8) return ZSTD_refDictContent(dctx, dict, dictSize);
    {   U32 const magic = MEM_readLE32(dict);
        if (magic != ZSTD_MAGIC_DICTIONARY) {
            return ZSTD_refDictContent(dctx, dict, dictSize);    
    }   }
    dctx->dictID = MEM_readLE32((const char*)dict + ZSTD_FRAMEIDSIZE);

     
    {   size_t const eSize = ZSTD_loadDEntropy(&dctx->entropy, dict, dictSize);
        RETURN_ERROR_IF(ZSTD_isError(eSize), dictionary_corrupted, "");
        dict = (const char*)dict + eSize;
        dictSize -= eSize;
    }
    dctx->litEntropy = dctx->fseEntropy = 1;

     
    return ZSTD_refDictContent(dctx, dict, dictSize);
}

size_t ZSTD_decompressBegin(ZSTD_DCtx* dctx)
{
    assert(dctx != NULL);
    dctx->expected = ZSTD_startingInputLength(dctx->format);   
    dctx->stage = ZSTDds_getFrameHeaderSize;
    dctx->decodedSize = 0;
    dctx->previousDstEnd = NULL;
    dctx->prefixStart = NULL;
    dctx->virtualStart = NULL;
    dctx->dictEnd = NULL;
    dctx->entropy.hufTable[0] = (HUF_DTable)((HufLog)*0x1000001);   
    dctx->litEntropy = dctx->fseEntropy = 0;
    dctx->dictID = 0;
    dctx->bType = bt_reserved;
    ZSTD_STATIC_ASSERT(sizeof(dctx->entropy.rep) == sizeof(repStartValue));
    memcpy(dctx->entropy.rep, repStartValue, sizeof(repStartValue));   
    dctx->LLTptr = dctx->entropy.LLTable;
    dctx->MLTptr = dctx->entropy.MLTable;
    dctx->OFTptr = dctx->entropy.OFTable;
    dctx->HUFptr = dctx->entropy.hufTable;
    return 0;
}

size_t ZSTD_decompressBegin_usingDict(ZSTD_DCtx* dctx, const void* dict, size_t dictSize)
{
    FORWARD_IF_ERROR( ZSTD_decompressBegin(dctx) , "");
    if (dict && dictSize)
        RETURN_ERROR_IF(
            ZSTD_isError(ZSTD_decompress_insertDictionary(dctx, dict, dictSize)),
            dictionary_corrupted, "");
    return 0;
}


 

size_t ZSTD_decompressBegin_usingDDict(ZSTD_DCtx* dctx, const ZSTD_DDict* ddict)
{
    DEBUGLOG(4, "ZSTD_decompressBegin_usingDDict");
    assert(dctx != NULL);
    if (ddict) {
        const char* const dictStart = (const char*)ZSTD_DDict_dictContent(ddict);
        size_t const dictSize = ZSTD_DDict_dictSize(ddict);
        const void* const dictEnd = dictStart + dictSize;
        dctx->ddictIsCold = (dctx->dictEnd != dictEnd);
        DEBUGLOG(4, "DDict is %s",
                    dctx->ddictIsCold ? "~cold~" : "hot!");
    }
    FORWARD_IF_ERROR( ZSTD_decompressBegin(dctx) , "");
    if (ddict) {    
        ZSTD_copyDDictParameters(dctx, ddict);
    }
    return 0;
}

 
unsigned ZSTD_getDictID_fromDict(const void* dict, size_t dictSize)
{
    if (dictSize < 8) return 0;
    if (MEM_readLE32(dict) != ZSTD_MAGIC_DICTIONARY) return 0;
    return MEM_readLE32((const char*)dict + ZSTD_FRAMEIDSIZE);
}

 
unsigned ZSTD_getDictID_fromFrame(const void* src, size_t srcSize)
{
    ZSTD_frameHeader zfp = { 0, 0, 0, ZSTD_frame, 0, 0, 0 };
    size_t const hError = ZSTD_getFrameHeader(&zfp, src, srcSize);
    if (ZSTD_isError(hError)) return 0;
    return zfp.dictID;
}


 
size_t ZSTD_decompress_usingDDict(ZSTD_DCtx* dctx,
                                  void* dst, size_t dstCapacity,
                            const void* src, size_t srcSize,
                            const ZSTD_DDict* ddict)
{
     
    return ZSTD_decompressMultiFrame(dctx, dst, dstCapacity, src, srcSize,
                                     NULL, 0,
                                     ddict);
}


 

ZSTD_DStream* ZSTD_createDStream(void)
{
    DEBUGLOG(3, "ZSTD_createDStream");
    return ZSTD_createDStream_advanced(ZSTD_defaultCMem);
}

ZSTD_DStream* ZSTD_initStaticDStream(void *workspace, size_t workspaceSize)
{
    return ZSTD_initStaticDCtx(workspace, workspaceSize);
}

ZSTD_DStream* ZSTD_createDStream_advanced(ZSTD_customMem customMem)
{
    return ZSTD_createDCtx_advanced(customMem);
}

size_t ZSTD_freeDStream(ZSTD_DStream* zds)
{
    return ZSTD_freeDCtx(zds);
}


 

size_t ZSTD_DStreamInSize(void)  { return ZSTD_BLOCKSIZE_MAX + ZSTD_blockHeaderSize; }
size_t ZSTD_DStreamOutSize(void) { return ZSTD_BLOCKSIZE_MAX; }

size_t ZSTD_DCtx_loadDictionary_advanced(ZSTD_DCtx* dctx,
                                   const void* dict, size_t dictSize,
                                         ZSTD_dictLoadMethod_e dictLoadMethod,
                                         ZSTD_dictContentType_e dictContentType)
{
    RETURN_ERROR_IF(dctx->streamStage != zdss_init, stage_wrong, "");
    ZSTD_clearDict(dctx);
    if (dict && dictSize != 0) {
        dctx->ddictLocal = ZSTD_createDDict_advanced(dict, dictSize, dictLoadMethod, dictContentType, dctx->customMem);
        RETURN_ERROR_IF(dctx->ddictLocal == NULL, memory_allocation, "NULL pointer!");
        dctx->ddict = dctx->ddictLocal;
        dctx->dictUses = ZSTD_use_indefinitely;
    }
    return 0;
}

size_t ZSTD_DCtx_loadDictionary_byReference(ZSTD_DCtx* dctx, const void* dict, size_t dictSize)
{
    return ZSTD_DCtx_loadDictionary_advanced(dctx, dict, dictSize, ZSTD_dlm_byRef, ZSTD_dct_auto);
}

size_t ZSTD_DCtx_loadDictionary(ZSTD_DCtx* dctx, const void* dict, size_t dictSize)
{
    return ZSTD_DCtx_loadDictionary_advanced(dctx, dict, dictSize, ZSTD_dlm_byCopy, ZSTD_dct_auto);
}

size_t ZSTD_DCtx_refPrefix_advanced(ZSTD_DCtx* dctx, const void* prefix, size_t prefixSize, ZSTD_dictContentType_e dictContentType)
{
    FORWARD_IF_ERROR(ZSTD_DCtx_loadDictionary_advanced(dctx, prefix, prefixSize, ZSTD_dlm_byRef, dictContentType), "");
    dctx->dictUses = ZSTD_use_once;
    return 0;
}

size_t ZSTD_DCtx_refPrefix(ZSTD_DCtx* dctx, const void* prefix, size_t prefixSize)
{
    return ZSTD_DCtx_refPrefix_advanced(dctx, prefix, prefixSize, ZSTD_dct_rawContent);
}


 
size_t ZSTD_initDStream_usingDict(ZSTD_DStream* zds, const void* dict, size_t dictSize)
{
    DEBUGLOG(4, "ZSTD_initDStream_usingDict");
    FORWARD_IF_ERROR( ZSTD_DCtx_reset(zds, ZSTD_reset_session_only) , "");
    FORWARD_IF_ERROR( ZSTD_DCtx_loadDictionary(zds, dict, dictSize) , "");
    return ZSTD_startingInputLength(zds->format);
}

 
size_t ZSTD_initDStream(ZSTD_DStream* zds)
{
    DEBUGLOG(4, "ZSTD_initDStream");
    return ZSTD_initDStream_usingDDict(zds, NULL);
}

 
size_t ZSTD_initDStream_usingDDict(ZSTD_DStream* dctx, const ZSTD_DDict* ddict)
{
    FORWARD_IF_ERROR( ZSTD_DCtx_reset(dctx, ZSTD_reset_session_only) , "");
    FORWARD_IF_ERROR( ZSTD_DCtx_refDDict(dctx, ddict) , "");
    return ZSTD_startingInputLength(dctx->format);
}

 
size_t ZSTD_resetDStream(ZSTD_DStream* dctx)
{
    FORWARD_IF_ERROR(ZSTD_DCtx_reset(dctx, ZSTD_reset_session_only), "");
    return ZSTD_startingInputLength(dctx->format);
}


size_t ZSTD_DCtx_refDDict(ZSTD_DCtx* dctx, const ZSTD_DDict* ddict)
{
    RETURN_ERROR_IF(dctx->streamStage != zdss_init, stage_wrong, "");
    ZSTD_clearDict(dctx);
    if (ddict) {
        dctx->ddict = ddict;
        dctx->dictUses = ZSTD_use_indefinitely;
    }
    return 0;
}

 
size_t ZSTD_DCtx_setMaxWindowSize(ZSTD_DCtx* dctx, size_t maxWindowSize)
{
    ZSTD_bounds const bounds = ZSTD_dParam_getBounds(ZSTD_d_windowLogMax);
    size_t const min = (size_t)1 << bounds.lowerBound;
    size_t const max = (size_t)1 << bounds.upperBound;
    RETURN_ERROR_IF(dctx->streamStage != zdss_init, stage_wrong, "");
    RETURN_ERROR_IF(maxWindowSize < min, parameter_outOfBound, "");
    RETURN_ERROR_IF(maxWindowSize > max, parameter_outOfBound, "");
    dctx->maxWindowSize = maxWindowSize;
    return 0;
}

size_t ZSTD_DCtx_setFormat(ZSTD_DCtx* dctx, ZSTD_format_e format)
{
    return ZSTD_DCtx_setParameter(dctx, ZSTD_d_format, format);
}

ZSTD_bounds ZSTD_dParam_getBounds(ZSTD_dParameter dParam)
{
    ZSTD_bounds bounds = { 0, 0, 0 };
    switch(dParam) {
        case ZSTD_d_windowLogMax:
            bounds.lowerBound = ZSTD_WINDOWLOG_ABSOLUTEMIN;
            bounds.upperBound = ZSTD_WINDOWLOG_MAX;
            return bounds;
        case ZSTD_d_format:
            bounds.lowerBound = (int)ZSTD_f_zstd1;
            bounds.upperBound = (int)ZSTD_f_zstd1_magicless;
            ZSTD_STATIC_ASSERT(ZSTD_f_zstd1 < ZSTD_f_zstd1_magicless);
            return bounds;
        case ZSTD_d_stableOutBuffer:
            bounds.lowerBound = (int)ZSTD_obm_buffered;
            bounds.upperBound = (int)ZSTD_obm_stable;
            return bounds;
        default:;
    }
    bounds.error = ERROR(parameter_unsupported);
    return bounds;
}

 
static int ZSTD_dParam_withinBounds(ZSTD_dParameter dParam, int value)
{
    ZSTD_bounds const bounds = ZSTD_dParam_getBounds(dParam);
    if (ZSTD_isError(bounds.error)) return 0;
    if (value < bounds.lowerBound) return 0;
    if (value > bounds.upperBound) return 0;
    return 1;
}

#define CHECK_DBOUNDS(p,v) {                \
    RETURN_ERROR_IF(!ZSTD_dParam_withinBounds(p, v), parameter_outOfBound, ""); \
}

size_t ZSTD_DCtx_setParameter(ZSTD_DCtx* dctx, ZSTD_dParameter dParam, int value)
{
    RETURN_ERROR_IF(dctx->streamStage != zdss_init, stage_wrong, "");
    switch(dParam) {
        case ZSTD_d_windowLogMax:
            if (value == 0) value = ZSTD_WINDOWLOG_LIMIT_DEFAULT;
            CHECK_DBOUNDS(ZSTD_d_windowLogMax, value);
            dctx->maxWindowSize = ((size_t)1) << value;
            return 0;
        case ZSTD_d_format:
            CHECK_DBOUNDS(ZSTD_d_format, value);
            dctx->format = (ZSTD_format_e)value;
            return 0;
        case ZSTD_d_stableOutBuffer:
            CHECK_DBOUNDS(ZSTD_d_stableOutBuffer, value);
            dctx->outBufferMode = (ZSTD_outBufferMode_e)value;
            return 0;
        default:;
    }
    RETURN_ERROR(parameter_unsupported, "");
}

size_t ZSTD_DCtx_reset(ZSTD_DCtx* dctx, ZSTD_ResetDirective reset)
{
    if ( (reset == ZSTD_reset_session_only)
      || (reset == ZSTD_reset_session_and_parameters) ) {
        dctx->streamStage = zdss_init;
        dctx->noForwardProgress = 0;
    }
    if ( (reset == ZSTD_reset_parameters)
      || (reset == ZSTD_reset_session_and_parameters) ) {
        RETURN_ERROR_IF(dctx->streamStage != zdss_init, stage_wrong, "");
        ZSTD_clearDict(dctx);
        dctx->format = ZSTD_f_zstd1;
        dctx->maxWindowSize = ZSTD_MAXWINDOWSIZE_DEFAULT;
    }
    return 0;
}


size_t ZSTD_sizeof_DStream(const ZSTD_DStream* dctx)
{
    return ZSTD_sizeof_DCtx(dctx);
}

size_t ZSTD_decodingBufferSize_min(unsigned long long windowSize, unsigned long long frameContentSize)
{
    size_t const blockSize = (size_t) MIN(windowSize, ZSTD_BLOCKSIZE_MAX);
    unsigned long long const neededRBSize = windowSize + blockSize + (WILDCOPY_OVERLENGTH * 2);
    unsigned long long const neededSize = MIN(frameContentSize, neededRBSize);
    size_t const minRBSize = (size_t) neededSize;
    RETURN_ERROR_IF((unsigned long long)minRBSize != neededSize,
                    frameParameter_windowTooLarge, "");
    return minRBSize;
}

size_t ZSTD_estimateDStreamSize(size_t windowSize)
{
    size_t const blockSize = MIN(windowSize, ZSTD_BLOCKSIZE_MAX);
    size_t const inBuffSize = blockSize;   
    size_t const outBuffSize = ZSTD_decodingBufferSize_min(windowSize, ZSTD_CONTENTSIZE_UNKNOWN);
    return ZSTD_estimateDCtxSize() + inBuffSize + outBuffSize;
}

size_t ZSTD_estimateDStreamSize_fromFrame(const void* src, size_t srcSize)
{
    U32 const windowSizeMax = 1U << ZSTD_WINDOWLOG_MAX;    
    ZSTD_frameHeader zfh;
    size_t const err = ZSTD_getFrameHeader(&zfh, src, srcSize);
    if (ZSTD_isError(err)) return err;
    RETURN_ERROR_IF(err>0, srcSize_wrong, "");
    RETURN_ERROR_IF(zfh.windowSize > windowSizeMax,
                    frameParameter_windowTooLarge, "");
    return ZSTD_estimateDStreamSize((size_t)zfh.windowSize);
}


 

static int ZSTD_DCtx_isOverflow(ZSTD_DStream* zds, size_t const neededInBuffSize, size_t const neededOutBuffSize)
{
    return (zds->inBuffSize + zds->outBuffSize) >= (neededInBuffSize + neededOutBuffSize) * ZSTD_WORKSPACETOOLARGE_FACTOR;
}

static void ZSTD_DCtx_updateOversizedDuration(ZSTD_DStream* zds, size_t const neededInBuffSize, size_t const neededOutBuffSize)
{
    if (ZSTD_DCtx_isOverflow(zds, neededInBuffSize, neededOutBuffSize))
        zds->oversizedDuration++;
    else 
        zds->oversizedDuration = 0;
}

static int ZSTD_DCtx_isOversizedTooLong(ZSTD_DStream* zds)
{
    return zds->oversizedDuration >= ZSTD_WORKSPACETOOLARGE_MAXDURATION;
}

 
static size_t ZSTD_checkOutBuffer(ZSTD_DStream const* zds, ZSTD_outBuffer const* output)
{
    ZSTD_outBuffer const expect = zds->expectedOutBuffer;
     
    if (zds->outBufferMode != ZSTD_obm_stable)
        return 0;
     
    if (zds->streamStage == zdss_init)
        return 0;
     
    if (expect.dst == output->dst && expect.pos == output->pos && expect.size == output->size)
        return 0;
    RETURN_ERROR(dstBuffer_wrong, "ZSTD_obm_stable enabled but output differs!");
}

 
static size_t ZSTD_decompressContinueStream(
            ZSTD_DStream* zds, char** op, char* oend,
            void const* src, size_t srcSize) {
    int const isSkipFrame = ZSTD_isSkipFrame(zds);
    if (zds->outBufferMode == ZSTD_obm_buffered) {
        size_t const dstSize = isSkipFrame ? 0 : zds->outBuffSize - zds->outStart;
        size_t const decodedSize = ZSTD_decompressContinue(zds,
                zds->outBuff + zds->outStart, dstSize, src, srcSize);
        FORWARD_IF_ERROR(decodedSize, "");
        if (!decodedSize && !isSkipFrame) {
            zds->streamStage = zdss_read;
        } else {
            zds->outEnd = zds->outStart + decodedSize;
            zds->streamStage = zdss_flush;
        }
    } else {
         
        size_t const dstSize = isSkipFrame ? 0 : oend - *op;
        size_t const decodedSize = ZSTD_decompressContinue(zds, *op, dstSize, src, srcSize);
        FORWARD_IF_ERROR(decodedSize, "");
        *op += decodedSize;
         
        zds->streamStage = zdss_read;
        assert(*op <= oend);
        assert(zds->outBufferMode == ZSTD_obm_stable);
    }
    return 0;
}

size_t ZSTD_decompressStream(ZSTD_DStream* zds, ZSTD_outBuffer* output, ZSTD_inBuffer* input)
{
    const char* const src = (const char*)input->src;
    const char* const istart = input->pos != 0 ? src + input->pos : src;
    const char* const iend = input->size != 0 ? src + input->size : src;
    const char* ip = istart;
    char* const dst = (char*)output->dst;
    char* const ostart = output->pos != 0 ? dst + output->pos : dst;
    char* const oend = output->size != 0 ? dst + output->size : dst;
    char* op = ostart;
    U32 someMoreWork = 1;

    DEBUGLOG(5, "ZSTD_decompressStream");
    RETURN_ERROR_IF(
        input->pos > input->size,
        srcSize_wrong,
        "forbidden. in: pos: %u   vs size: %u",
        (U32)input->pos, (U32)input->size);
    RETURN_ERROR_IF(
        output->pos > output->size,
        dstSize_tooSmall,
        "forbidden. out: pos: %u   vs size: %u",
        (U32)output->pos, (U32)output->size);
    DEBUGLOG(5, "input size : %u", (U32)(input->size - input->pos));
    FORWARD_IF_ERROR(ZSTD_checkOutBuffer(zds, output), "");

    while (someMoreWork) {
        switch(zds->streamStage)
        {
        case zdss_init :
            DEBUGLOG(5, "stage zdss_init => transparent reset ");
            zds->streamStage = zdss_loadHeader;
            zds->lhSize = zds->inPos = zds->outStart = zds->outEnd = 0;
            zds->legacyVersion = 0;
            zds->hostageByte = 0;
            zds->expectedOutBuffer = *output;
             

        case zdss_loadHeader :
            DEBUGLOG(5, "stage zdss_loadHeader (srcSize : %u)", (U32)(iend - ip));
#if defined(ZSTD_LEGACY_SUPPORT) && (ZSTD_LEGACY_SUPPORT>=1)
            if (zds->legacyVersion) {
                RETURN_ERROR_IF(zds->staticSize, memory_allocation,
                    "legacy support is incompatible with static dctx");
                {   size_t const hint = ZSTD_decompressLegacyStream(zds->legacyContext, zds->legacyVersion, output, input);
                    if (hint==0) zds->streamStage = zdss_init;
                    return hint;
            }   }
#endif
            {   size_t const hSize = ZSTD_getFrameHeader_advanced(&zds->fParams, zds->headerBuffer, zds->lhSize, zds->format);
                DEBUGLOG(5, "header size : %u", (U32)hSize);
                if (ZSTD_isError(hSize)) {
#if defined(ZSTD_LEGACY_SUPPORT) && (ZSTD_LEGACY_SUPPORT>=1)
                    U32 const legacyVersion = ZSTD_isLegacy(istart, iend-istart);
                    if (legacyVersion) {
                        ZSTD_DDict const* const ddict = ZSTD_getDDict(zds);
                        const void* const dict = ddict ? ZSTD_DDict_dictContent(ddict) : NULL;
                        size_t const dictSize = ddict ? ZSTD_DDict_dictSize(ddict) : 0;
                        DEBUGLOG(5, "ZSTD_decompressStream: detected legacy version v0.%u", legacyVersion);
                        RETURN_ERROR_IF(zds->staticSize, memory_allocation,
                            "legacy support is incompatible with static dctx");
                        FORWARD_IF_ERROR(ZSTD_initLegacyStream(&zds->legacyContext,
                                    zds->previousLegacyVersion, legacyVersion,
                                    dict, dictSize), "");
                        zds->legacyVersion = zds->previousLegacyVersion = legacyVersion;
                        {   size_t const hint = ZSTD_decompressLegacyStream(zds->legacyContext, legacyVersion, output, input);
                            if (hint==0) zds->streamStage = zdss_init;    
                            return hint;
                    }   }
#endif
                    return hSize;    
                }
                if (hSize != 0) {    
                    size_t const toLoad = hSize - zds->lhSize;    
                    size_t const remainingInput = (size_t)(iend-ip);
                    assert(iend >= ip);
                    if (toLoad > remainingInput) {    
                        if (remainingInput > 0) {
                            memcpy(zds->headerBuffer + zds->lhSize, ip, remainingInput);
                            zds->lhSize += remainingInput;
                        }
                        input->pos = input->size;
                        return (MAX((size_t)ZSTD_FRAMEHEADERSIZE_MIN(zds->format), hSize) - zds->lhSize) + ZSTD_blockHeaderSize;    
                    }
                    assert(ip != NULL);
                    memcpy(zds->headerBuffer + zds->lhSize, ip, toLoad); zds->lhSize = hSize; ip += toLoad;
                    break;
            }   }

             
            if (zds->fParams.frameContentSize != ZSTD_CONTENTSIZE_UNKNOWN
                && zds->fParams.frameType != ZSTD_skippableFrame
                && (U64)(size_t)(oend-op) >= zds->fParams.frameContentSize) {
                size_t const cSize = ZSTD_findFrameCompressedSize(istart, iend-istart);
                if (cSize <= (size_t)(iend-istart)) {
                     
                    size_t const decompressedSize = ZSTD_decompress_usingDDict(zds, op, oend-op, istart, cSize, ZSTD_getDDict(zds));
                    if (ZSTD_isError(decompressedSize)) return decompressedSize;
                    DEBUGLOG(4, "shortcut to single-pass ZSTD_decompress_usingDDict()")
                    ip = istart + cSize;
                    op += decompressedSize;
                    zds->expected = 0;
                    zds->streamStage = zdss_init;
                    someMoreWork = 0;
                    break;
            }   }

             
            if (zds->outBufferMode == ZSTD_obm_stable
                && zds->fParams.frameType != ZSTD_skippableFrame
                && zds->fParams.frameContentSize != ZSTD_CONTENTSIZE_UNKNOWN
                && (U64)(size_t)(oend-op) < zds->fParams.frameContentSize) {
                RETURN_ERROR(dstSize_tooSmall, "ZSTD_obm_stable passed but ZSTD_outBuffer is too small");
            }

             
            DEBUGLOG(4, "Consume header");
            FORWARD_IF_ERROR(ZSTD_decompressBegin_usingDDict(zds, ZSTD_getDDict(zds)), "");

            if ((MEM_readLE32(zds->headerBuffer) & ZSTD_MAGIC_SKIPPABLE_MASK) == ZSTD_MAGIC_SKIPPABLE_START) {   
                zds->expected = MEM_readLE32(zds->headerBuffer + ZSTD_FRAMEIDSIZE);
                zds->stage = ZSTDds_skipFrame;
            } else {
                FORWARD_IF_ERROR(ZSTD_decodeFrameHeader(zds, zds->headerBuffer, zds->lhSize), "");
                zds->expected = ZSTD_blockHeaderSize;
                zds->stage = ZSTDds_decodeBlockHeader;
            }

             
            DEBUGLOG(4, "Control max memory usage (%u KB <= max %u KB)",
                        (U32)(zds->fParams.windowSize >>10),
                        (U32)(zds->maxWindowSize >> 10) );
            zds->fParams.windowSize = MAX(zds->fParams.windowSize, 1U << ZSTD_WINDOWLOG_ABSOLUTEMIN);
            RETURN_ERROR_IF(zds->fParams.windowSize > zds->maxWindowSize,
                            frameParameter_windowTooLarge, "");

             
            {   size_t const neededInBuffSize = MAX(zds->fParams.blockSizeMax, 4  );
                size_t const neededOutBuffSize = zds->outBufferMode == ZSTD_obm_buffered
                        ? ZSTD_decodingBufferSize_min(zds->fParams.windowSize, zds->fParams.frameContentSize)
                        : 0;

                ZSTD_DCtx_updateOversizedDuration(zds, neededInBuffSize, neededOutBuffSize);

                {   int const tooSmall = (zds->inBuffSize < neededInBuffSize) || (zds->outBuffSize < neededOutBuffSize);
                    int const tooLarge = ZSTD_DCtx_isOversizedTooLong(zds);
                    
                    if (tooSmall || tooLarge) {
                        size_t const bufferSize = neededInBuffSize + neededOutBuffSize;
                        DEBUGLOG(4, "inBuff  : from %u to %u",
                                    (U32)zds->inBuffSize, (U32)neededInBuffSize);
                        DEBUGLOG(4, "outBuff : from %u to %u",
                                    (U32)zds->outBuffSize, (U32)neededOutBuffSize);
                        if (zds->staticSize) {   
                            DEBUGLOG(4, "staticSize : %u", (U32)zds->staticSize);
                            assert(zds->staticSize >= sizeof(ZSTD_DCtx));   
                            RETURN_ERROR_IF(
                                bufferSize > zds->staticSize - sizeof(ZSTD_DCtx),
                                memory_allocation, "");
                        } else {
                            ZSTD_free(zds->inBuff, zds->customMem);
                            zds->inBuffSize = 0;
                            zds->outBuffSize = 0;
                            zds->inBuff = (char*)ZSTD_malloc(bufferSize, zds->customMem);
                            RETURN_ERROR_IF(zds->inBuff == NULL, memory_allocation, "");
                        }
                        zds->inBuffSize = neededInBuffSize;
                        zds->outBuff = zds->inBuff + zds->inBuffSize;
                        zds->outBuffSize = neededOutBuffSize;
            }   }   }
            zds->streamStage = zdss_read;
             

        case zdss_read:
            DEBUGLOG(5, "stage zdss_read");
            {   size_t const neededInSize = ZSTD_nextSrcSizeToDecompressWithInputSize(zds, iend - ip);
                DEBUGLOG(5, "neededInSize = %u", (U32)neededInSize);
                if (neededInSize==0) {   
                    zds->streamStage = zdss_init;
                    someMoreWork = 0;
                    break;
                }
                if ((size_t)(iend-ip) >= neededInSize) {   
                    FORWARD_IF_ERROR(ZSTD_decompressContinueStream(zds, &op, oend, ip, neededInSize), "");
                    ip += neededInSize;
                     
                    break;
            }   }
            if (ip==iend) { someMoreWork = 0; break; }    
            zds->streamStage = zdss_load;
             

        case zdss_load:
            {   size_t const neededInSize = ZSTD_nextSrcSizeToDecompress(zds);
                size_t const toLoad = neededInSize - zds->inPos;
                int const isSkipFrame = ZSTD_isSkipFrame(zds);
                size_t loadedSize;
                 
                assert(neededInSize == ZSTD_nextSrcSizeToDecompressWithInputSize(zds, iend - ip));
                if (isSkipFrame) {
                    loadedSize = MIN(toLoad, (size_t)(iend-ip));
                } else {
                    RETURN_ERROR_IF(toLoad > zds->inBuffSize - zds->inPos,
                                    corruption_detected,
                                    "should never happen");
                    loadedSize = ZSTD_limitCopy(zds->inBuff + zds->inPos, toLoad, ip, iend-ip);
                }
                ip += loadedSize;
                zds->inPos += loadedSize;
                if (loadedSize < toLoad) { someMoreWork = 0; break; }    

                 
                zds->inPos = 0;    
                FORWARD_IF_ERROR(ZSTD_decompressContinueStream(zds, &op, oend, zds->inBuff, neededInSize), "");
                 
                break;
            }
        case zdss_flush:
            {   size_t const toFlushSize = zds->outEnd - zds->outStart;
                size_t const flushedSize = ZSTD_limitCopy(op, oend-op, zds->outBuff + zds->outStart, toFlushSize);
                op += flushedSize;
                zds->outStart += flushedSize;
                if (flushedSize == toFlushSize) {   
                    zds->streamStage = zdss_read;
                    if ( (zds->outBuffSize < zds->fParams.frameContentSize)
                      && (zds->outStart + zds->fParams.blockSizeMax > zds->outBuffSize) ) {
                        DEBUGLOG(5, "restart filling outBuff from beginning (left:%i, needed:%u)",
                                (int)(zds->outBuffSize - zds->outStart),
                                (U32)zds->fParams.blockSizeMax);
                        zds->outStart = zds->outEnd = 0;
                    }
                    break;
            }   }
             
            someMoreWork = 0;
            break;

        default:
            assert(0);     
            RETURN_ERROR(GENERIC, "impossible to reach");    
    }   }

     
    input->pos = (size_t)(ip - (const char*)(input->src));
    output->pos = (size_t)(op - (char*)(output->dst));

     
    zds->expectedOutBuffer = *output;

    if ((ip==istart) && (op==ostart)) {   
        zds->noForwardProgress ++;
        if (zds->noForwardProgress >= ZSTD_NO_FORWARD_PROGRESS_MAX) {
            RETURN_ERROR_IF(op==oend, dstSize_tooSmall, "");
            RETURN_ERROR_IF(ip==iend, srcSize_wrong, "");
            assert(0);
        }
    } else {
        zds->noForwardProgress = 0;
    }
    {   size_t nextSrcSizeHint = ZSTD_nextSrcSizeToDecompress(zds);
        if (!nextSrcSizeHint) {    
            if (zds->outEnd == zds->outStart) {   
                if (zds->hostageByte) {
                    if (input->pos >= input->size) {
                         
                        zds->streamStage = zdss_read;
                        return 1;
                    }
                    input->pos++;   
                }    
                return 0;
            }   
            if (!zds->hostageByte) {  
                input->pos--;    
                zds->hostageByte=1;
            }
            return 1;
        }   
        nextSrcSizeHint += ZSTD_blockHeaderSize * (ZSTD_nextInputType(zds) == ZSTDnit_block);    
        assert(zds->inPos <= nextSrcSizeHint);
        nextSrcSizeHint -= zds->inPos;    
        return nextSrcSizeHint;
    }
}

size_t ZSTD_decompressStream_simpleArgs (
                            ZSTD_DCtx* dctx,
                            void* dst, size_t dstCapacity, size_t* dstPos,
                      const void* src, size_t srcSize, size_t* srcPos)
{
    ZSTD_outBuffer output = { dst, dstCapacity, *dstPos };
    ZSTD_inBuffer  input  = { src, srcSize, *srcPos };
     
    size_t const cErr = ZSTD_decompressStream(dctx, &output, &input);
    *dstPos = output.pos;
    *srcPos = input.pos;
    return cErr;
}
