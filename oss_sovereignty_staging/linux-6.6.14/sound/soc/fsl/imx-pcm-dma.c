
 
#include <linux/platform_device.h>
#include <linux/dmaengine.h>
#include <linux/types.h>
#include <linux/module.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>

#include "imx-pcm.h"

static bool filter(struct dma_chan *chan, void *param)
{
	if (!imx_dma_is_general_purpose(chan))
		return false;

	chan->private = param;

	return true;
}

static const struct snd_dmaengine_pcm_config imx_dmaengine_pcm_config = {
	.prepare_slave_config = snd_dmaengine_pcm_prepare_slave_config,
	.compat_filter_fn = filter,
};

int imx_pcm_dma_init(struct platform_device *pdev)
{
	struct snd_dmaengine_pcm_config *config;

	config = devm_kzalloc(&pdev->dev,
			sizeof(struct snd_dmaengine_pcm_config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;
	*config = imx_dmaengine_pcm_config;

	return devm_snd_dmaengine_pcm_register(&pdev->dev,
		config,
		SND_DMAENGINE_PCM_FLAG_COMPAT);
}
EXPORT_SYMBOL_GPL(imx_pcm_dma_init);

MODULE_LICENSE("GPL");
