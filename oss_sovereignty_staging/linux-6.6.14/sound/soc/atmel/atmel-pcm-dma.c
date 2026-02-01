
 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/atmel-ssc.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>

#include "atmel-pcm.h"

 
static const struct snd_pcm_hardware atmel_pcm_dma_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_RESUME |
				  SNDRV_PCM_INFO_PAUSE,
	.period_bytes_min	= 256,		 
	.period_bytes_max	= 2 * 0xffff,	 
	.periods_min		= 8,
	.periods_max		= 1024,		 
	.buffer_bytes_max	= 512 * 1024,
};

 
static void atmel_pcm_dma_irq(u32 ssc_sr,
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct atmel_pcm_dma_params *prtd;

	prtd = snd_soc_dai_get_dma_data(asoc_rtd_to_cpu(rtd, 0), substream);

	if (ssc_sr & prtd->mask->ssc_error) {
		if (snd_pcm_running(substream))
			pr_warn("atmel-pcm: buffer %s on %s (SSC_SR=%#x)\n",
				substream->stream == SNDRV_PCM_STREAM_PLAYBACK
				? "underrun" : "overrun", prtd->name,
				ssc_sr);

		 
		ssc_writex(prtd->ssc->regs, SSC_CR, prtd->mask->ssc_disable);
		snd_pcm_stop_xrun(substream);

		 
		ssc_readx(prtd->ssc->regs, SSC_RHR);
		ssc_readx(prtd->ssc->regs, SSC_SR);
	}
}

static int atmel_pcm_configure_dma(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct dma_slave_config *slave_config)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct atmel_pcm_dma_params *prtd;
	struct ssc_device *ssc;
	int ret;

	prtd = snd_soc_dai_get_dma_data(asoc_rtd_to_cpu(rtd, 0), substream);
	ssc = prtd->ssc;

	ret = snd_hwparams_to_dma_slave_config(substream, params, slave_config);
	if (ret) {
		pr_err("atmel-pcm: hwparams to dma slave configure failed\n");
		return ret;
	}

	slave_config->dst_addr = ssc->phybase + SSC_THR;
	slave_config->dst_maxburst = 1;

	slave_config->src_addr = ssc->phybase + SSC_RHR;
	slave_config->src_maxburst = 1;

	prtd->dma_intr_handler = atmel_pcm_dma_irq;

	return 0;
}

static const struct snd_dmaengine_pcm_config atmel_dmaengine_pcm_config = {
	.prepare_slave_config = atmel_pcm_configure_dma,
	.pcm_hardware = &atmel_pcm_dma_hardware,
	.prealloc_buffer_size = 64 * 1024,
};

int atmel_pcm_dma_platform_register(struct device *dev)
{
	return devm_snd_dmaengine_pcm_register(dev,
					&atmel_dmaengine_pcm_config, 0);
}
EXPORT_SYMBOL(atmel_pcm_dma_platform_register);

MODULE_AUTHOR("Bo Shen <voice.shen@atmel.com>");
MODULE_DESCRIPTION("Atmel DMA based PCM module");
MODULE_LICENSE("GPL");
