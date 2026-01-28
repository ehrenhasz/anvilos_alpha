#ifndef _SAMSUNG_DMA_H
#define _SAMSUNG_DMA_H
#include <sound/dmaengine_pcm.h>
int samsung_asoc_dma_platform_register(struct device *dev, dma_filter_fn filter,
				       const char *tx, const char *rx,
				       struct device *dma_dev);
#endif  
