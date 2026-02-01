 
 

#ifndef _I2C_STM32_H
#define _I2C_STM32_H

#include <linux/dma-direction.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>

enum stm32_i2c_speed {
	STM32_I2C_SPEED_STANDARD,  
	STM32_I2C_SPEED_FAST,  
	STM32_I2C_SPEED_FAST_PLUS,  
	STM32_I2C_SPEED_END,
};

 
struct stm32_i2c_dma {
	struct dma_chan *chan_tx;
	struct dma_chan *chan_rx;
	struct dma_chan *chan_using;
	dma_addr_t dma_buf;
	unsigned int dma_len;
	enum dma_transfer_direction dma_transfer_dir;
	enum dma_data_direction dma_data_dir;
	struct completion dma_complete;
};

struct stm32_i2c_dma *stm32_i2c_dma_request(struct device *dev,
					    dma_addr_t phy_addr,
					    u32 txdr_offset, u32 rxdr_offset);

void stm32_i2c_dma_free(struct stm32_i2c_dma *dma);

int stm32_i2c_prep_dma_xfer(struct device *dev, struct stm32_i2c_dma *dma,
			    bool rd_wr, u32 len, u8 *buf,
			    dma_async_tx_callback callback,
			    void *dma_async_param);

#endif  
