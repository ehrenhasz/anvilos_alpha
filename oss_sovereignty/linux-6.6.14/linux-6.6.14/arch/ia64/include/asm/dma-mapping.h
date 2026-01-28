#ifndef _ASM_IA64_DMA_MAPPING_H
#define _ASM_IA64_DMA_MAPPING_H
extern const struct dma_map_ops *dma_ops;
static inline const struct dma_map_ops *get_arch_dma_ops(void)
{
	return dma_ops;
}
#endif  
