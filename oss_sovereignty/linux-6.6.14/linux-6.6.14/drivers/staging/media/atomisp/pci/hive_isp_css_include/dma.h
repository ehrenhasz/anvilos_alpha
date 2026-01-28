#ifndef __DMA_H_INCLUDED__
#define __DMA_H_INCLUDED__
#include "system_local.h"
#include "dma_local.h"
#ifndef __INLINE_DMA__
#define STORAGE_CLASS_DMA_H extern
#define STORAGE_CLASS_DMA_C
#include "dma_public.h"
#else   
#define STORAGE_CLASS_DMA_H static inline
#define STORAGE_CLASS_DMA_C static inline
#include "dma_private.h"
#endif  
#endif  
