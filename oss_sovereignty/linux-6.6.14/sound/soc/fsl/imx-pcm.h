 
 

#ifndef _IMX_PCM_H
#define _IMX_PCM_H

#include <linux/dma/imx-dma.h>

 
#define IMX_SSI_DMABUF_SIZE	(64 * 1024)

#define IMX_DEFAULT_DMABUF_SIZE	(64 * 1024)

struct imx_pcm_fiq_params {
	int irq;
	void __iomem *base;

	 
	struct snd_dmaengine_dai_dma_data *dma_params_rx;
	struct snd_dmaengine_dai_dma_data *dma_params_tx;
};

#if IS_ENABLED(CONFIG_SND_SOC_IMX_PCM_DMA)
int imx_pcm_dma_init(struct platform_device *pdev);
#else
static inline int imx_pcm_dma_init(struct platform_device *pdev)
{
	return -ENODEV;
}
#endif

#if IS_ENABLED(CONFIG_SND_SOC_IMX_PCM_FIQ)
int imx_pcm_fiq_init(struct platform_device *pdev,
		struct imx_pcm_fiq_params *params);
void imx_pcm_fiq_exit(struct platform_device *pdev);
#else
static inline int imx_pcm_fiq_init(struct platform_device *pdev,
		struct imx_pcm_fiq_params *params)
{
	return -ENODEV;
}

static inline void imx_pcm_fiq_exit(struct platform_device *pdev)
{
}
#endif

#endif  
