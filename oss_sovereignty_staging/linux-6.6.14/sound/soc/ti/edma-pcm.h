 
 

#ifndef __EDMA_PCM_H__
#define __EDMA_PCM_H__

#if IS_ENABLED(CONFIG_SND_SOC_TI_EDMA_PCM)
int edma_pcm_platform_register(struct device *dev);
#else
static inline int edma_pcm_platform_register(struct device *dev)
{
	return 0;
}
#endif  

#endif  
