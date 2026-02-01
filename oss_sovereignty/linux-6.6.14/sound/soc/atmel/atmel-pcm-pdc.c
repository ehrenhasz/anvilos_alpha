
 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/atmel_pdc.h>
#include <linux/atmel-ssc.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "atmel-pcm.h"


static int atmel_pcm_new(struct snd_soc_component *component,
			 struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	int ret;

	ret = dma_coerce_mask_and_coherent(card->dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	snd_pcm_set_managed_buffer_all(rtd->pcm, SNDRV_DMA_TYPE_DEV,
				       card->dev, ATMEL_SSC_DMABUF_SIZE,
				       ATMEL_SSC_DMABUF_SIZE);

	return 0;
}

 
 
static const struct snd_pcm_hardware atmel_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_PAUSE,
	.period_bytes_min	= 32,
	.period_bytes_max	= 8192,
	.periods_min		= 2,
	.periods_max		= 1024,
	.buffer_bytes_max	= ATMEL_SSC_DMABUF_SIZE,
};


 
struct atmel_runtime_data {
	struct atmel_pcm_dma_params *params;
	dma_addr_t dma_buffer;		 
	dma_addr_t dma_buffer_end;	 
	size_t period_size;

	dma_addr_t period_ptr;		 
};

 
static void atmel_pcm_dma_irq(u32 ssc_sr,
	struct snd_pcm_substream *substream)
{
	struct atmel_runtime_data *prtd = substream->runtime->private_data;
	struct atmel_pcm_dma_params *params = prtd->params;
	static int count;

	count++;

	if (ssc_sr & params->mask->ssc_endbuf) {
		pr_warn("atmel-pcm: buffer %s on %s (SSC_SR=%#x, count=%d)\n",
				substream->stream == SNDRV_PCM_STREAM_PLAYBACK
				? "underrun" : "overrun",
				params->name, ssc_sr, count);

		 
		ssc_writex(params->ssc->regs, ATMEL_PDC_PTCR,
			   params->mask->pdc_disable);
		prtd->period_ptr += prtd->period_size;
		if (prtd->period_ptr >= prtd->dma_buffer_end)
			prtd->period_ptr = prtd->dma_buffer;

		ssc_writex(params->ssc->regs, params->pdc->xpr,
			   prtd->period_ptr);
		ssc_writex(params->ssc->regs, params->pdc->xcr,
			   prtd->period_size / params->pdc_xfer_size);
		ssc_writex(params->ssc->regs, ATMEL_PDC_PTCR,
			   params->mask->pdc_enable);
	}

	if (ssc_sr & params->mask->ssc_endx) {
		 
		prtd->period_ptr += prtd->period_size;
		if (prtd->period_ptr >= prtd->dma_buffer_end)
			prtd->period_ptr = prtd->dma_buffer;

		ssc_writex(params->ssc->regs, params->pdc->xnpr,
			   prtd->period_ptr);
		ssc_writex(params->ssc->regs, params->pdc->xncr,
			   prtd->period_size / params->pdc_xfer_size);
	}

	snd_pcm_period_elapsed(substream);
}


 
static int atmel_pcm_hw_params(struct snd_soc_component *component,
			       struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct atmel_runtime_data *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);

	 

	prtd->params = snd_soc_dai_get_dma_data(asoc_rtd_to_cpu(rtd, 0), substream);
	prtd->params->dma_intr_handler = atmel_pcm_dma_irq;

	prtd->dma_buffer = runtime->dma_addr;
	prtd->dma_buffer_end = runtime->dma_addr + runtime->dma_bytes;
	prtd->period_size = params_period_bytes(params);

	pr_debug("atmel-pcm: "
		"hw_params: DMA for %s initialized "
		"(dma_bytes=%zu, period_size=%zu)\n",
		prtd->params->name,
		runtime->dma_bytes,
		prtd->period_size);
	return 0;
}

static int atmel_pcm_hw_free(struct snd_soc_component *component,
			     struct snd_pcm_substream *substream)
{
	struct atmel_runtime_data *prtd = substream->runtime->private_data;
	struct atmel_pcm_dma_params *params = prtd->params;

	if (params != NULL) {
		ssc_writex(params->ssc->regs, SSC_PDC_PTCR,
			   params->mask->pdc_disable);
		prtd->params->dma_intr_handler = NULL;
	}

	return 0;
}

static int atmel_pcm_prepare(struct snd_soc_component *component,
			     struct snd_pcm_substream *substream)
{
	struct atmel_runtime_data *prtd = substream->runtime->private_data;
	struct atmel_pcm_dma_params *params = prtd->params;

	ssc_writex(params->ssc->regs, SSC_IDR,
		   params->mask->ssc_endx | params->mask->ssc_endbuf);
	ssc_writex(params->ssc->regs, ATMEL_PDC_PTCR,
		   params->mask->pdc_disable);
	return 0;
}

static int atmel_pcm_trigger(struct snd_soc_component *component,
			     struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *rtd = substream->runtime;
	struct atmel_runtime_data *prtd = rtd->private_data;
	struct atmel_pcm_dma_params *params = prtd->params;
	int ret = 0;

	pr_debug("atmel-pcm:buffer_size = %ld,"
		"dma_area = %p, dma_bytes = %zu\n",
		rtd->buffer_size, rtd->dma_area, rtd->dma_bytes);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		prtd->period_ptr = prtd->dma_buffer;

		ssc_writex(params->ssc->regs, params->pdc->xpr,
			   prtd->period_ptr);
		ssc_writex(params->ssc->regs, params->pdc->xcr,
			   prtd->period_size / params->pdc_xfer_size);

		prtd->period_ptr += prtd->period_size;
		ssc_writex(params->ssc->regs, params->pdc->xnpr,
			   prtd->period_ptr);
		ssc_writex(params->ssc->regs, params->pdc->xncr,
			   prtd->period_size / params->pdc_xfer_size);

		pr_debug("atmel-pcm: trigger: "
			"period_ptr=%lx, xpr=%u, "
			"xcr=%u, xnpr=%u, xncr=%u\n",
			(unsigned long)prtd->period_ptr,
			ssc_readx(params->ssc->regs, params->pdc->xpr),
			ssc_readx(params->ssc->regs, params->pdc->xcr),
			ssc_readx(params->ssc->regs, params->pdc->xnpr),
			ssc_readx(params->ssc->regs, params->pdc->xncr));

		ssc_writex(params->ssc->regs, SSC_IER,
			   params->mask->ssc_endx | params->mask->ssc_endbuf);
		ssc_writex(params->ssc->regs, SSC_PDC_PTCR,
			   params->mask->pdc_enable);

		pr_debug("sr=%u imr=%u\n",
			ssc_readx(params->ssc->regs, SSC_SR),
			ssc_readx(params->ssc->regs, SSC_IER));
		break;		 

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		ssc_writex(params->ssc->regs, ATMEL_PDC_PTCR,
			   params->mask->pdc_disable);
		break;

	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		ssc_writex(params->ssc->regs, ATMEL_PDC_PTCR,
			   params->mask->pdc_enable);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static snd_pcm_uframes_t atmel_pcm_pointer(struct snd_soc_component *component,
					   struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct atmel_runtime_data *prtd = runtime->private_data;
	struct atmel_pcm_dma_params *params = prtd->params;
	dma_addr_t ptr;
	snd_pcm_uframes_t x;

	ptr = (dma_addr_t) ssc_readx(params->ssc->regs, params->pdc->xpr);
	x = bytes_to_frames(runtime, ptr - prtd->dma_buffer);

	if (x == runtime->buffer_size)
		x = 0;

	return x;
}

static int atmel_pcm_open(struct snd_soc_component *component,
			  struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct atmel_runtime_data *prtd;
	int ret = 0;

	snd_soc_set_runtime_hwparams(substream, &atmel_pcm_hardware);

	 
	ret = snd_pcm_hw_constraint_integer(runtime,
						SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto out;

	prtd = kzalloc(sizeof(struct atmel_runtime_data), GFP_KERNEL);
	if (prtd == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	runtime->private_data = prtd;

 out:
	return ret;
}

static int atmel_pcm_close(struct snd_soc_component *component,
			   struct snd_pcm_substream *substream)
{
	struct atmel_runtime_data *prtd = substream->runtime->private_data;

	kfree(prtd);
	return 0;
}

static const struct snd_soc_component_driver atmel_soc_platform = {
	.open		= atmel_pcm_open,
	.close		= atmel_pcm_close,
	.hw_params	= atmel_pcm_hw_params,
	.hw_free	= atmel_pcm_hw_free,
	.prepare	= atmel_pcm_prepare,
	.trigger	= atmel_pcm_trigger,
	.pointer	= atmel_pcm_pointer,
	.pcm_construct	= atmel_pcm_new,
};

int atmel_pcm_pdc_platform_register(struct device *dev)
{
	return devm_snd_soc_register_component(dev, &atmel_soc_platform,
					       NULL, 0);
}
EXPORT_SYMBOL(atmel_pcm_pdc_platform_register);

MODULE_AUTHOR("Sedji Gaouaou <sedji.gaouaou@atmel.com>");
MODULE_DESCRIPTION("Atmel PCM module");
MODULE_LICENSE("GPL");
