 


#ifndef ZSTD_DEC_BLOCK_H
#define ZSTD_DEC_BLOCK_H

 
#include <stddef.h>    
#include "../zstd.h"     
#include "../common/zstd_internal.h"   
#include "zstd_decompress_internal.h"   


 

 

 


 
size_t ZSTD_decompressBlock_internal(ZSTD_DCtx* dctx,
                               void* dst, size_t dstCapacity,
                         const void* src, size_t srcSize, const int frame);

 
void ZSTD_buildFSETable(ZSTD_seqSymbol* dt,
             const short* normalizedCounter, unsigned maxSymbolValue,
             const U32* baseValue, const U32* nbAdditionalBits,
                   unsigned tableLog);


#endif  
