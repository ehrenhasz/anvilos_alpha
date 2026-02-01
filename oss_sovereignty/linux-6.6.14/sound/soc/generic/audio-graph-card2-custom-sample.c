






#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <sound/graph_card.h>

 
struct custom_priv {
	struct asoc_simple_priv simple_priv;

	 
	int custom_params;
};

 
#define simple_to_custom(simple) container_of((simple), struct custom_priv, simple_priv)

static int custom_card_probe(struct snd_soc_card *card)
{
	struct asoc_simple_priv *simple_priv = snd_soc_card_get_drvdata(card);
	struct custom_priv *custom_priv = simple_to_custom(simple_priv);
	struct device *dev = simple_priv_to_dev(simple_priv);

	dev_info(dev, "custom probe\n");

	custom_priv->custom_params = 1;

	 
	return asoc_graph_card_probe(card);
}

static int custom_hook_pre(struct asoc_simple_priv *priv)
{
	struct device *dev = simple_priv_to_dev(priv);

	 
	dev_info(dev, "hook : %s\n", __func__);

	return 0;
}

static int custom_hook_post(struct asoc_simple_priv *priv)
{
	struct device *dev = simple_priv_to_dev(priv);
	struct snd_soc_card *card;

	 
	dev_info(dev, "hook : %s\n", __func__);

	 
	card = simple_priv_to_card(priv);
	card->probe = custom_card_probe;

	return 0;
}

static int custom_normal(struct asoc_simple_priv *priv,
			 struct device_node *lnk,
			 struct link_info *li)
{
	struct device *dev = simple_priv_to_dev(priv);

	 
	dev_info(dev, "hook : %s\n", __func__);

	return audio_graph2_link_normal(priv, lnk, li);
}

static int custom_dpcm(struct asoc_simple_priv *priv,
		       struct device_node *lnk,
		       struct link_info *li)
{
	struct device *dev = simple_priv_to_dev(priv);

	 
	dev_info(dev, "hook : %s\n", __func__);

	return audio_graph2_link_dpcm(priv, lnk, li);
}

static int custom_c2c(struct asoc_simple_priv *priv,
		      struct device_node *lnk,
		      struct link_info *li)
{
	struct device *dev = simple_priv_to_dev(priv);

	 
	dev_info(dev, "hook : %s\n", __func__);

	return audio_graph2_link_c2c(priv, lnk, li);
}

 
static struct graph2_custom_hooks custom_hooks = {
	.hook_pre	= custom_hook_pre,
	.hook_post	= custom_hook_post,
	.custom_normal	= custom_normal,
	.custom_dpcm	= custom_dpcm,
	.custom_c2c	= custom_c2c,
};

static int custom_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct asoc_simple_priv *priv = snd_soc_card_get_drvdata(rtd->card);
	struct device *dev = simple_priv_to_dev(priv);

	dev_info(dev, "custom startup\n");

	return asoc_simple_startup(substream);
}

 
static const struct snd_soc_ops custom_ops = {
	.startup	= custom_startup,
	.shutdown	= asoc_simple_shutdown,
	.hw_params	= asoc_simple_hw_params,
};

static int custom_probe(struct platform_device *pdev)
{
	struct custom_priv *custom_priv;
	struct asoc_simple_priv *simple_priv;
	struct device *dev = &pdev->dev;
	int ret;

	custom_priv = devm_kzalloc(dev, sizeof(*custom_priv), GFP_KERNEL);
	if (!custom_priv)
		return -ENOMEM;

	simple_priv		= &custom_priv->simple_priv;
	simple_priv->ops	= &custom_ops;  

	 
	simple_priv->snd_card.name = "card2-custom";

	 
	ret = audio_graph2_parse_of(simple_priv, dev, &custom_hooks);
	if (ret < 0)
		return ret;

	 

	return 0;
}

static const struct of_device_id custom_of_match[] = {
	{ .compatible = "audio-graph-card2-custom-sample", },
	{},
};
MODULE_DEVICE_TABLE(of, custom_of_match);

static struct platform_driver custom_card = {
	.driver = {
		.name = "audio-graph-card2-custom-sample",
		.of_match_table = custom_of_match,
	},
	.probe	= custom_probe,
	.remove	= asoc_simple_remove,
};
module_platform_driver(custom_card);

MODULE_ALIAS("platform:asoc-audio-graph-card2-custom-sample");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ASoC Audio Graph Card2 Custom Sample");
MODULE_AUTHOR("Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>");
