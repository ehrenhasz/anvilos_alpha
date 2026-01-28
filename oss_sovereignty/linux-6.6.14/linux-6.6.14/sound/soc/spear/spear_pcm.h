#ifndef __SPEAR_PCM_H__
#define __SPEAR_PCM_H__
int devm_spear_pcm_platform_register(struct device *dev,
			struct snd_dmaengine_pcm_config *config,
			bool (*filter)(struct dma_chan *chan, void *slave));
#endif
