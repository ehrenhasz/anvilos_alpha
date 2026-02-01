 
 

#ifndef _ATMEL_PCM_H
#define _ATMEL_PCM_H

#include <linux/atmel-ssc.h>

#define ATMEL_SSC_DMABUF_SIZE	(64 * 1024)

 
struct atmel_pdc_regs {
	unsigned int	xpr;		 
	unsigned int	xcr;		 
	unsigned int	xnpr;		 
	unsigned int	xncr;		 
	unsigned int	ptcr;		 
};

struct atmel_ssc_mask {
	u32	ssc_enable;		 
	u32	ssc_disable;		 
	u32	ssc_error;		 
	u32	ssc_endx;		 
	u32	ssc_endbuf;		 
	u32	pdc_enable;		 
	u32	pdc_disable;		 
};

 
struct atmel_pcm_dma_params {
	char *name;			 
	int pdc_xfer_size;		 
	struct ssc_device *ssc;		 
	struct atmel_pdc_regs *pdc;	 
	struct atmel_ssc_mask *mask;	 
	struct snd_pcm_substream *substream;
	void (*dma_intr_handler)(u32, struct snd_pcm_substream *);
};

 
#define ssc_readx(base, reg)            (__raw_readl((base) + (reg)))
#define ssc_writex(base, reg, value)    __raw_writel((value), (base) + (reg))

#if IS_ENABLED(CONFIG_SND_ATMEL_SOC_PDC)
int atmel_pcm_pdc_platform_register(struct device *dev);
#else
static inline int atmel_pcm_pdc_platform_register(struct device *dev)
{
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_SND_ATMEL_SOC_DMA)
int atmel_pcm_dma_platform_register(struct device *dev);
#else
static inline int atmel_pcm_dma_platform_register(struct device *dev)
{
	return 0;
}
#endif

#endif  
