#ifndef DMA_H
#define DMA_H
#include <asm/glue-cache.h>
#ifndef MULTI_CACHE
#define dmac_map_area			__glue(_CACHE,_dma_map_area)
#define dmac_unmap_area 		__glue(_CACHE,_dma_unmap_area)
extern void dmac_map_area(const void *, size_t, int);
extern void dmac_unmap_area(const void *, size_t, int);
#else
#define dmac_map_area			cpu_cache.dma_map_area
#define dmac_unmap_area 		cpu_cache.dma_unmap_area
#endif
#endif
