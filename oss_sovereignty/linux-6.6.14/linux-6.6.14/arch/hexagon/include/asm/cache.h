#ifndef __ASM_CACHE_H
#define __ASM_CACHE_H
#define L1_CACHE_SHIFT		(5)
#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)
#define ARCH_DMA_MINALIGN	L1_CACHE_BYTES
#define __cacheline_aligned	__aligned(L1_CACHE_BYTES)
#define ____cacheline_aligned	__aligned(L1_CACHE_BYTES)
#define __read_mostly
#endif
