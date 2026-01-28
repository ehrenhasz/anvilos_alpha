#ifndef ___ASM_SPARC_DMA_MAPPING_H
#define ___ASM_SPARC_DMA_MAPPING_H
extern const struct dma_map_ops *dma_ops;
static inline const struct dma_map_ops *get_arch_dma_ops(void)
{
	return IS_ENABLED(CONFIG_SPARC64) ? dma_ops : NULL;
}
#endif
