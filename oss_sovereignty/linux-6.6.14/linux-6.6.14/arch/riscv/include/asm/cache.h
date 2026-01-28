#ifndef _ASM_RISCV_CACHE_H
#define _ASM_RISCV_CACHE_H
#define L1_CACHE_SHIFT		6
#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)
#ifdef CONFIG_RISCV_DMA_NONCOHERENT
#define ARCH_DMA_MINALIGN L1_CACHE_BYTES
#define ARCH_KMALLOC_MINALIGN	(8)
#endif
#ifndef CONFIG_MMU
#define ARCH_SLAB_MINALIGN	16
#endif
#ifndef __ASSEMBLY__
#ifdef CONFIG_RISCV_DMA_NONCOHERENT
extern int dma_cache_alignment;
#define dma_get_cache_alignment dma_get_cache_alignment
static inline int dma_get_cache_alignment(void)
{
	return dma_cache_alignment;
}
#endif
#endif	 
#endif  
