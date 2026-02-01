 
 

#ifndef _LOONGSON_I2S_H
#define _LOONGSON_I2S_H

#include <linux/regmap.h>
#include <sound/dmaengine_pcm.h>

 
#define LS_I2S_VER	0x00  
#define LS_I2S_CFG	0x04  
#define LS_I2S_CTRL	0x08  
#define LS_I2S_RX_DATA	0x0C  
#define LS_I2S_TX_DATA	0x10  

 
#define LS_I2S_CFG1	0x14  

 
#define LS_I2S_TX_ORDER	0x100  
#define LS_I2S_RX_ORDER 0x110  

 
#define I2S_CTRL_MCLK_READY	(1 << 16)  
#define I2S_CTRL_MASTER		(1 << 15)  
#define I2S_CTRL_MSB		(1 << 14)  
#define I2S_CTRL_RX_EN		(1 << 13)  
#define I2S_CTRL_TX_EN		(1 << 12)  
#define I2S_CTRL_RX_DMA_EN	(1 << 11)  
#define I2S_CTRL_CLK_READY	(1 << 8)   
#define I2S_CTRL_TX_DMA_EN	(1 << 7)   
#define I2S_CTRL_RESET		(1 << 4)   
#define I2S_CTRL_MCLK_EN	(1 << 3)   
#define I2S_CTRL_RX_INT_EN	(1 << 1)   
#define I2S_CTRL_TX_INT_EN	(1 << 0)   

#define LS_I2S_DRVNAME		"loongson-i2s"

struct loongson_dma_data {
	dma_addr_t dev_addr;		 
	void __iomem *order_addr;	 
	int irq;			 
};

struct loongson_i2s {
	struct device *dev;
	union {
		struct snd_dmaengine_dai_dma_data playback_dma_data;
		struct loongson_dma_data tx_dma_data;
	};
	union {
		struct snd_dmaengine_dai_dma_data capture_dma_data;
		struct loongson_dma_data rx_dma_data;
	};
	struct regmap *regmap;
	void __iomem *reg_base;
	u32 rev_id;
	u32 clk_rate;
	u32 sysclk;
};

extern const struct dev_pm_ops loongson_i2s_pm;
extern struct snd_soc_dai_driver loongson_i2s_dai;

#endif
