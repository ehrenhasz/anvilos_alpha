 
 

#ifndef __LINUX_SPI_INTERNALS_H
#define __LINUX_SPI_INTERNALS_H

#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>
#include <linux/spi/spi.h>

void spi_flush_queue(struct spi_controller *ctrl);

#ifdef CONFIG_HAS_DMA
int spi_map_buf(struct spi_controller *ctlr, struct device *dev,
		struct sg_table *sgt, void *buf, size_t len,
		enum dma_data_direction dir);
void spi_unmap_buf(struct spi_controller *ctlr, struct device *dev,
		   struct sg_table *sgt, enum dma_data_direction dir);
#else  
static inline int spi_map_buf(struct spi_controller *ctlr, struct device *dev,
			      struct sg_table *sgt, void *buf, size_t len,
			      enum dma_data_direction dir)
{
	return -EINVAL;
}

static inline void spi_unmap_buf(struct spi_controller *ctlr,
				 struct device *dev, struct sg_table *sgt,
				 enum dma_data_direction dir)
{
}
#endif  

#endif  
