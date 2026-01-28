#ifndef __SDMA_PCM_H__
#define __SDMA_PCM_H__
#if IS_ENABLED(CONFIG_SND_SOC_TI_SDMA_PCM)
int sdma_pcm_platform_register(struct device *dev,
			       char *txdmachan, char *rxdmachan);
#else
static inline int sdma_pcm_platform_register(struct device *dev,
					     char *txdmachan, char *rxdmachan)
{
	return -ENODEV;
}
#endif  
#endif  
