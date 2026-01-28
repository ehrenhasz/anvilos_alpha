#ifndef _PARISC_DMA_MAPPING_H
#define _PARISC_DMA_MAPPING_H
extern const struct dma_map_ops *hppa_dma_ops;
static inline const struct dma_map_ops *get_arch_dma_ops(void)
{
	return hppa_dma_ops;
}
#endif
