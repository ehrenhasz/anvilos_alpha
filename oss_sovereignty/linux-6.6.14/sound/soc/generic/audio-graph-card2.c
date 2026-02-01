







#include <linux/clk.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <sound/graph_card.h>

 

enum graph_type {
	GRAPH_NORMAL,
	GRAPH_DPCM,
	GRAPH_C2C,

	GRAPH_MULTI,	 
};

#define GRAPH_NODENAME_MULTI	"multi"
#define GRAPH_NODENAME_DPCM	"dpcm"
#define GRAPH_NODENAME_C2C	"codec2codec"

#define port_to_endpoint(port) of_get_child_by_name(port, "endpoint")

static enum graph_type __graph_get_type(struct device_node *lnk)
{
	struct device_node *np, *parent_np;
	enum graph_type ret;

	 
	np = of_get_parent(lnk);
	if (of_node_name_eq(np, "ports")) {
		parent_np = of_get_parent(np);
		of_node_put(np);
		np = parent_np;
	}

	if (of_node_name_eq(np, GRAPH_NODENAME_MULTI)) {
		ret = GRAPH_MULTI;
		goto out_put;
	}

	if (of_node_name_eq(np, GRAPH_NODENAME_DPCM)) {
		ret = GRAPH_DPCM;
		goto out_put;
	}

	if (of_node_name_eq(np, GRAPH_NODENAME_C2C)) {
		ret = GRAPH_C2C;
		goto out_put;
	}

	ret = GRAPH_NORMAL;

out_put:
	of_node_put(np);
	return ret;

}

static enum graph_type graph_get_type(struct asoc_simple_priv *priv,
				      struct device_node *lnk)
{
	enum graph_type type = __graph_get_type(lnk);

	 
	if (type == GRAPH_MULTI)
		type = GRAPH_NORMAL;

#ifdef DEBUG
	{
		struct device *dev = simple_priv_to_dev(priv);
		const char *str = "Normal";

		switch (type) {
		case GRAPH_DPCM:
			if (asoc_graph_is_ports0(lnk))
				str = "DPCM Front-End";
			else
				str = "DPCM Back-End";
			break;
		case GRAPH_C2C:
			str = "Codec2Codec";
			break;
		default:
			break;
		}

		dev_dbg(dev, "%pOF (%s)", lnk, str);
	}
#endif
	return type;
}

static int graph_lnk_is_multi(struct device_node *lnk)
{
	return __graph_get_type(lnk) == GRAPH_MULTI;
}

static struct device_node *graph_get_next_multi_ep(struct device_node **port)
{
	struct device_node *ports = of_get_parent(*port);
	struct device_node *ep = NULL;
	struct device_node *rep = NULL;

	 
	do {
		*port = of_get_next_child(ports, *port);
		if (!*port)
			break;
	} while (!of_node_name_eq(*port, "port"));

	if (*port) {
		ep  = port_to_endpoint(*port);
		rep = of_graph_get_remote_endpoint(ep);
	}

	of_node_put(ep);
	of_node_put(ports);

	return rep;
}

static const struct snd_soc_ops graph_ops = {
	.startup	= asoc_simple_startup,
	.shutdown	= asoc_simple_shutdown,
	.hw_params	= asoc_simple_hw_params,
};

static void graph_parse_convert(struct device_node *ep,
				struct simple_dai_props *props)
{
	struct device_node *port = of_get_parent(ep);
	struct device_node *ports = of_get_parent(port);
	struct asoc_simple_data *adata = &props->adata;

	if (of_node_name_eq(ports, "ports"))
		asoc_simple_parse_convert(ports, NULL, adata);
	asoc_simple_parse_convert(port, NULL, adata);
	asoc_simple_parse_convert(ep,   NULL, adata);

	of_node_put(port);
	of_node_put(ports);
}

static void graph_parse_mclk_fs(struct device_node *ep,
				struct simple_dai_props *props)
{
	struct device_node *port	= of_get_parent(ep);
	struct device_node *ports	= of_get_parent(port);

	if (of_node_name_eq(ports, "ports"))
		of_property_read_u32(ports, "mclk-fs", &props->mclk_fs);
	of_property_read_u32(port,	"mclk-fs", &props->mclk_fs);
	of_property_read_u32(ep,	"mclk-fs", &props->mclk_fs);

	of_node_put(port);
	of_node_put(ports);
}

static int __graph_parse_node(struct asoc_simple_priv *priv,
			      enum graph_type gtype,
			      struct device_node *ep,
			      struct link_info *li,
			      int is_cpu, int idx)
{
	struct device *dev = simple_priv_to_dev(priv);
	struct snd_soc_dai_link *dai_link = simple_priv_to_link(priv, li->link);
	struct simple_dai_props *dai_props = simple_priv_to_props(priv, li->link);
	struct snd_soc_dai_link_component *dlc;
	struct asoc_simple_dai *dai;
	int ret, is_single_links = 0;

	if (is_cpu) {
		dlc = asoc_link_to_cpu(dai_link, idx);
		dai = simple_props_to_dai_cpu(dai_props, idx);
	} else {
		dlc = asoc_link_to_codec(dai_link, idx);
		dai = simple_props_to_dai_codec(dai_props, idx);
	}

	graph_parse_mclk_fs(ep, dai_props);

	ret = asoc_graph_parse_dai(dev, ep, dlc, &is_single_links);
	if (ret < 0)
		return ret;

	ret = asoc_simple_parse_tdm(ep, dai);
	if (ret < 0)
		return ret;

	ret = asoc_simple_parse_tdm_width_map(dev, ep, dai);
	if (ret < 0)
		return ret;

	ret = asoc_simple_parse_clk(dev, ep, dai, dlc);
	if (ret < 0)
		return ret;

	 
	if (!dai_link->name) {
		struct snd_soc_dai_link_component *cpus = dlc;
		struct snd_soc_dai_link_component *codecs = asoc_link_to_codec(dai_link, idx);
		char *cpu_multi   = "";
		char *codec_multi = "";

		if (dai_link->num_cpus > 1)
			cpu_multi = "_multi";
		if (dai_link->num_codecs > 1)
			codec_multi = "_multi";

		switch (gtype) {
		case GRAPH_NORMAL:
			 
			if (is_cpu)
				asoc_simple_set_dailink_name(dev, dai_link, "%s%s-%s%s",
							       cpus->dai_name,   cpu_multi,
							     codecs->dai_name, codec_multi);
			break;
		case GRAPH_DPCM:
			if (is_cpu)
				asoc_simple_set_dailink_name(dev, dai_link, "fe.%pOFP.%s%s",
						cpus->of_node, cpus->dai_name, cpu_multi);
			else
				asoc_simple_set_dailink_name(dev, dai_link, "be.%pOFP.%s%s",
						codecs->of_node, codecs->dai_name, codec_multi);
			break;
		case GRAPH_C2C:
			 
			if (is_cpu)
				asoc_simple_set_dailink_name(dev, dai_link, "c2c.%s%s-%s%s",
							     cpus->dai_name,   cpu_multi,
							     codecs->dai_name, codec_multi);
			break;
		default:
			break;
		}
	}

	 
	if (!is_cpu && gtype == GRAPH_DPCM) {
		struct snd_soc_dai_link_component *codecs = asoc_link_to_codec(dai_link, idx);
		struct snd_soc_codec_conf *cconf = simple_props_to_codec_conf(dai_props, idx);
		struct device_node *rport  = of_get_parent(ep);
		struct device_node *rports = of_get_parent(rport);

		if (of_node_name_eq(rports, "ports"))
			snd_soc_of_parse_node_prefix(rports, cconf, codecs->of_node, "prefix");
		snd_soc_of_parse_node_prefix(rport,  cconf, codecs->of_node, "prefix");

		of_node_put(rport);
		of_node_put(rports);
	}

	if (is_cpu) {
		struct snd_soc_dai_link_component *cpus = dlc;
		struct snd_soc_dai_link_component *platforms = asoc_link_to_platform(dai_link, idx);

		asoc_simple_canonicalize_cpu(cpus, is_single_links);
		asoc_simple_canonicalize_platform(platforms, cpus);
	}

	return 0;
}

static int graph_parse_node(struct asoc_simple_priv *priv,
			    enum graph_type gtype,
			    struct device_node *port,
			    struct link_info *li, int is_cpu)
{
	struct device_node *ep;
	int ret = 0;

	if (graph_lnk_is_multi(port)) {
		int idx;

		of_node_get(port);

		for (idx = 0;; idx++) {
			ep = graph_get_next_multi_ep(&port);
			if (!ep)
				break;

			ret = __graph_parse_node(priv, gtype, ep,
						 li, is_cpu, idx);
			of_node_put(ep);
			if (ret < 0)
				break;
		}
	} else {
		 
		ep = port_to_endpoint(port);
		ret = __graph_parse_node(priv, gtype, ep, li, is_cpu, 0);
		of_node_put(ep);
	}

	return ret;
}

static void graph_parse_daifmt(struct device_node *node,
			       unsigned int *daifmt, unsigned int *bit_frame)
{
	unsigned int fmt;

	 

	 

	 
	*bit_frame |= snd_soc_daifmt_parse_clock_provider_as_bitmap(node, NULL);

#define update_daifmt(name)					\
	if (!(*daifmt & SND_SOC_DAIFMT_##name##_MASK) &&	\
		 (fmt & SND_SOC_DAIFMT_##name##_MASK))		\
		*daifmt |= fmt & SND_SOC_DAIFMT_##name##_MASK

	 
	fmt = snd_soc_daifmt_parse_format(node, NULL);
	update_daifmt(FORMAT);
	update_daifmt(CLOCK);
	update_daifmt(INV);
}

static void graph_link_init(struct asoc_simple_priv *priv,
			    struct device_node *port,
			    struct link_info *li,
			    int is_cpu_node)
{
	struct snd_soc_dai_link *dai_link = simple_priv_to_link(priv, li->link);
	struct device_node *ep;
	struct device_node *ports;
	unsigned int daifmt = 0, daiclk = 0;
	unsigned int bit_frame = 0;

	if (graph_lnk_is_multi(port)) {
		of_node_get(port);
		ep = graph_get_next_multi_ep(&port);
		port = of_get_parent(ep);
	} else {
		ep = port_to_endpoint(port);
	}

	ports = of_get_parent(port);

	 
	graph_parse_daifmt(ep,    &daifmt, &bit_frame);		 
	graph_parse_daifmt(port,  &daifmt, &bit_frame);		 
	if (of_node_name_eq(ports, "ports"))
		graph_parse_daifmt(ports, &daifmt, &bit_frame);	 

	 
	daiclk = snd_soc_daifmt_clock_provider_from_bitmap(bit_frame);
	if (is_cpu_node)
		daiclk = snd_soc_daifmt_clock_provider_flipped(daiclk);

	dai_link->dai_fmt	= daifmt | daiclk;
	dai_link->init		= asoc_simple_dai_init;
	dai_link->ops		= &graph_ops;
	if (priv->ops)
		dai_link->ops	= priv->ops;
}

int audio_graph2_link_normal(struct asoc_simple_priv *priv,
			     struct device_node *lnk,
			     struct link_info *li)
{
	struct device_node *cpu_port = lnk;
	struct device_node *cpu_ep = port_to_endpoint(cpu_port);
	struct device_node *codec_port = of_graph_get_remote_port(cpu_ep);
	int ret;

	 
	ret = graph_parse_node(priv, GRAPH_NORMAL, codec_port, li, 0);
	if (ret < 0)
		goto err;

	 
	ret = graph_parse_node(priv, GRAPH_NORMAL, cpu_port, li, 1);
	if (ret < 0)
		goto err;

	graph_link_init(priv, cpu_port, li, 1);
err:
	of_node_put(codec_port);
	of_node_put(cpu_ep);

	return ret;
}
EXPORT_SYMBOL_GPL(audio_graph2_link_normal);

int audio_graph2_link_dpcm(struct asoc_simple_priv *priv,
			   struct device_node *lnk,
			   struct link_info *li)
{
	struct device_node *ep = port_to_endpoint(lnk);
	struct device_node *rep = of_graph_get_remote_endpoint(ep);
	struct device_node *rport = of_graph_get_remote_port(ep);
	struct snd_soc_dai_link *dai_link = simple_priv_to_link(priv, li->link);
	struct simple_dai_props *dai_props = simple_priv_to_props(priv, li->link);
	int is_cpu = asoc_graph_is_ports0(lnk);
	int ret;

	if (is_cpu) {
		 
		 
		dai_link->dynamic		= 1;
		dai_link->dpcm_merged_format	= 1;

		ret = graph_parse_node(priv, GRAPH_DPCM, rport, li, 1);
		if (ret)
			goto err;
	} else {
		 
		 

		 
		dai_link->no_pcm		= 1;
		dai_link->be_hw_params_fixup	= asoc_simple_be_hw_params_fixup;

		ret = graph_parse_node(priv, GRAPH_DPCM, rport, li, 0);
		if (ret < 0)
			goto err;
	}

	graph_parse_convert(ep,  dai_props);  
	graph_parse_convert(rep, dai_props);  

	snd_soc_dai_link_set_capabilities(dai_link);

	graph_link_init(priv, rport, li, is_cpu);
err:
	of_node_put(ep);
	of_node_put(rep);
	of_node_put(rport);

	return ret;
}
EXPORT_SYMBOL_GPL(audio_graph2_link_dpcm);

int audio_graph2_link_c2c(struct asoc_simple_priv *priv,
			  struct device_node *lnk,
			  struct link_info *li)
{
	struct snd_soc_dai_link *dai_link = simple_priv_to_link(priv, li->link);
	struct device_node *port0, *port1, *ports;
	struct device_node *codec0_port, *codec1_port;
	struct device_node *ep0, *ep1;
	u32 val = 0;
	int ret = -EINVAL;

	 
	of_node_get(lnk);
	port0 = lnk;
	ports = of_get_parent(port0);
	port1 = of_get_next_child(ports, lnk);

	 
	of_property_read_u32(ports, "rate", &val);
	if (val) {
		struct device *dev = simple_priv_to_dev(priv);
		struct snd_soc_pcm_stream *c2c_conf;

		c2c_conf = devm_kzalloc(dev, sizeof(*c2c_conf), GFP_KERNEL);
		if (!c2c_conf)
			goto err1;

		c2c_conf->formats	= SNDRV_PCM_FMTBIT_S32_LE;  
		c2c_conf->rates		= SNDRV_PCM_RATE_8000_384000;
		c2c_conf->rate_min	=
		c2c_conf->rate_max	= val;
		c2c_conf->channels_min	=
		c2c_conf->channels_max	= 2;  

		dai_link->c2c_params		= c2c_conf;
		dai_link->num_c2c_params	= 1;
	}

	ep0 = port_to_endpoint(port0);
	ep1 = port_to_endpoint(port1);

	codec0_port = of_graph_get_remote_port(ep0);
	codec1_port = of_graph_get_remote_port(ep1);

	 
	ret = graph_parse_node(priv, GRAPH_C2C, codec1_port, li, 0);
	if (ret < 0)
		goto err2;

	 
	ret = graph_parse_node(priv, GRAPH_C2C, codec0_port, li, 1);
	if (ret < 0)
		goto err2;

	graph_link_init(priv, codec0_port, li, 1);
err2:
	of_node_put(ep0);
	of_node_put(ep1);
	of_node_put(codec0_port);
	of_node_put(codec1_port);
err1:
	of_node_put(ports);
	of_node_put(port0);
	of_node_put(port1);

	return ret;
}
EXPORT_SYMBOL_GPL(audio_graph2_link_c2c);

static int graph_link(struct asoc_simple_priv *priv,
		      struct graph2_custom_hooks *hooks,
		      enum graph_type gtype,
		      struct device_node *lnk,
		      struct link_info *li)
{
	struct device *dev = simple_priv_to_dev(priv);
	GRAPH2_CUSTOM func = NULL;
	int ret = -EINVAL;

	switch (gtype) {
	case GRAPH_NORMAL:
		if (hooks && hooks->custom_normal)
			func = hooks->custom_normal;
		else
			func = audio_graph2_link_normal;
		break;
	case GRAPH_DPCM:
		if (hooks && hooks->custom_dpcm)
			func = hooks->custom_dpcm;
		else
			func = audio_graph2_link_dpcm;
		break;
	case GRAPH_C2C:
		if (hooks && hooks->custom_c2c)
			func = hooks->custom_c2c;
		else
			func = audio_graph2_link_c2c;
		break;
	default:
		break;
	}

	if (!func) {
		dev_err(dev, "non supported gtype (%d)\n", gtype);
		goto err;
	}

	ret = func(priv, lnk, li);
	if (ret < 0)
		goto err;

	li->link++;
err:
	return ret;
}

static int graph_counter(struct device_node *lnk)
{
	 
	if (graph_lnk_is_multi(lnk))
		return of_graph_get_endpoint_count(of_get_parent(lnk)) - 1;
	 
	else
		return 1;
}

static int graph_count_normal(struct asoc_simple_priv *priv,
			      struct device_node *lnk,
			      struct link_info *li)
{
	struct device_node *cpu_port = lnk;
	struct device_node *cpu_ep = port_to_endpoint(cpu_port);
	struct device_node *codec_port = of_graph_get_remote_port(cpu_ep);

	 
	 
	li->num[li->link].cpus		=
	li->num[li->link].platforms	= graph_counter(cpu_port);

	li->num[li->link].codecs	= graph_counter(codec_port);

	of_node_put(cpu_ep);
	of_node_put(codec_port);

	return 0;
}

static int graph_count_dpcm(struct asoc_simple_priv *priv,
			    struct device_node *lnk,
			    struct link_info *li)
{
	struct device_node *ep = port_to_endpoint(lnk);
	struct device_node *rport = of_graph_get_remote_port(ep);

	 

	if (asoc_graph_is_ports0(lnk)) {
		 
		li->num[li->link].cpus		= graph_counter(rport);  
		li->num[li->link].platforms	= graph_counter(rport);
	} else {
		li->num[li->link].codecs	= graph_counter(rport);  
	}

	of_node_put(ep);
	of_node_put(rport);

	return 0;
}

static int graph_count_c2c(struct asoc_simple_priv *priv,
			   struct device_node *lnk,
			   struct link_info *li)
{
	struct device_node *ports = of_get_parent(lnk);
	struct device_node *port0 = lnk;
	struct device_node *port1 = of_get_next_child(ports, lnk);
	struct device_node *ep0 = port_to_endpoint(port0);
	struct device_node *ep1 = port_to_endpoint(port1);
	struct device_node *codec0 = of_graph_get_remote_port(ep0);
	struct device_node *codec1 = of_graph_get_remote_port(ep1);

	of_node_get(lnk);

	 
	 
	li->num[li->link].cpus		=
	li->num[li->link].platforms	= graph_counter(codec0);

	li->num[li->link].codecs	= graph_counter(codec1);

	of_node_put(ports);
	of_node_put(port1);
	of_node_put(ep0);
	of_node_put(ep1);
	of_node_put(codec0);
	of_node_put(codec1);

	return 0;
}

static int graph_count(struct asoc_simple_priv *priv,
		       struct graph2_custom_hooks *hooks,
		       enum graph_type gtype,
		       struct device_node *lnk,
		       struct link_info *li)
{
	struct device *dev = simple_priv_to_dev(priv);
	GRAPH2_CUSTOM func = NULL;
	int ret = -EINVAL;

	if (li->link >= SNDRV_MAX_LINKS) {
		dev_err(dev, "too many links\n");
		return ret;
	}

	switch (gtype) {
	case GRAPH_NORMAL:
		func = graph_count_normal;
		break;
	case GRAPH_DPCM:
		func = graph_count_dpcm;
		break;
	case GRAPH_C2C:
		func = graph_count_c2c;
		break;
	default:
		break;
	}

	if (!func) {
		dev_err(dev, "non supported gtype (%d)\n", gtype);
		goto err;
	}

	ret = func(priv, lnk, li);
	if (ret < 0)
		goto err;

	li->link++;
err:
	return ret;
}

static int graph_for_each_link(struct asoc_simple_priv *priv,
			       struct graph2_custom_hooks *hooks,
			       struct link_info *li,
			       int (*func)(struct asoc_simple_priv *priv,
					   struct graph2_custom_hooks *hooks,
					   enum graph_type gtype,
					   struct device_node *lnk,
					   struct link_info *li))
{
	struct of_phandle_iterator it;
	struct device *dev = simple_priv_to_dev(priv);
	struct device_node *node = dev->of_node;
	struct device_node *lnk;
	enum graph_type gtype;
	int rc, ret;

	 
	of_for_each_phandle(&it, rc, node, "links", NULL, 0) {
		lnk = it.node;

		gtype = graph_get_type(priv, lnk);

		ret = func(priv, hooks, gtype, lnk, li);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int audio_graph2_parse_of(struct asoc_simple_priv *priv, struct device *dev,
			  struct graph2_custom_hooks *hooks)
{
	struct snd_soc_card *card = simple_priv_to_card(priv);
	struct link_info *li;
	int ret;

	li = devm_kzalloc(dev, sizeof(*li), GFP_KERNEL);
	if (!li)
		return -ENOMEM;

	card->probe	= asoc_graph_card_probe;
	card->owner	= THIS_MODULE;
	card->dev	= dev;

	if ((hooks) && (hooks)->hook_pre) {
		ret = (hooks)->hook_pre(priv);
		if (ret < 0)
			goto err;
	}

	ret = graph_for_each_link(priv, hooks, li, graph_count);
	if (!li->link)
		ret = -EINVAL;
	if (ret < 0)
		goto err;

	ret = asoc_simple_init_priv(priv, li);
	if (ret < 0)
		goto err;

	priv->pa_gpio = devm_gpiod_get_optional(dev, "pa", GPIOD_OUT_LOW);
	if (IS_ERR(priv->pa_gpio)) {
		ret = PTR_ERR(priv->pa_gpio);
		dev_err(dev, "failed to get amplifier gpio: %d\n", ret);
		goto err;
	}

	ret = asoc_simple_parse_widgets(card, NULL);
	if (ret < 0)
		goto err;

	ret = asoc_simple_parse_routing(card, NULL);
	if (ret < 0)
		goto err;

	memset(li, 0, sizeof(*li));
	ret = graph_for_each_link(priv, hooks, li, graph_link);
	if (ret < 0)
		goto err;

	ret = asoc_simple_parse_card_name(card, NULL);
	if (ret < 0)
		goto err;

	snd_soc_card_set_drvdata(card, priv);

	if ((hooks) && (hooks)->hook_post) {
		ret = (hooks)->hook_post(priv);
		if (ret < 0)
			goto err;
	}

	asoc_simple_debug_info(priv);

	ret = devm_snd_soc_register_card(dev, card);
err:
	devm_kfree(dev, li);

	if (ret < 0)
		dev_err_probe(dev, ret, "parse error\n");

	return ret;
}
EXPORT_SYMBOL_GPL(audio_graph2_parse_of);

static int graph_probe(struct platform_device *pdev)
{
	struct asoc_simple_priv *priv;
	struct device *dev = &pdev->dev;

	 
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	return audio_graph2_parse_of(priv, dev, NULL);
}

static const struct of_device_id graph_of_match[] = {
	{ .compatible = "audio-graph-card2", },
	{},
};
MODULE_DEVICE_TABLE(of, graph_of_match);

static struct platform_driver graph_card = {
	.driver = {
		.name = "asoc-audio-graph-card2",
		.pm = &snd_soc_pm_ops,
		.of_match_table = graph_of_match,
	},
	.probe	= graph_probe,
	.remove	= asoc_simple_remove,
};
module_platform_driver(graph_card);

MODULE_ALIAS("platform:asoc-audio-graph-card2");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ASoC Audio Graph Card2");
MODULE_AUTHOR("Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>");
