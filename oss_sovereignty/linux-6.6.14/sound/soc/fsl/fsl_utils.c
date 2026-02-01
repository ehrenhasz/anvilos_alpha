







#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <sound/soc.h>

#include "fsl_utils.h"

 
int fsl_asoc_get_dma_channel(struct device_node *ssi_np,
			     const char *name,
			     struct snd_soc_dai_link *dai,
			     unsigned int *dma_channel_id,
			     unsigned int *dma_id)
{
	struct resource res;
	struct device_node *dma_channel_np, *dma_np;
	const __be32 *iprop;
	int ret;

	dma_channel_np = of_parse_phandle(ssi_np, name, 0);
	if (!dma_channel_np)
		return -EINVAL;

	if (!of_device_is_compatible(dma_channel_np, "fsl,ssi-dma-channel")) {
		of_node_put(dma_channel_np);
		return -EINVAL;
	}

	 
	ret = of_address_to_resource(dma_channel_np, 0, &res);
	if (ret) {
		of_node_put(dma_channel_np);
		return ret;
	}
	snprintf((char *)dai->platforms->name, DAI_NAME_SIZE, "%llx.%pOFn",
		 (unsigned long long) res.start, dma_channel_np);

	iprop = of_get_property(dma_channel_np, "cell-index", NULL);
	if (!iprop) {
		of_node_put(dma_channel_np);
		return -EINVAL;
	}
	*dma_channel_id = be32_to_cpup(iprop);

	dma_np = of_get_parent(dma_channel_np);
	iprop = of_get_property(dma_np, "cell-index", NULL);
	if (!iprop) {
		of_node_put(dma_np);
		of_node_put(dma_channel_np);
		return -EINVAL;
	}
	*dma_id = be32_to_cpup(iprop);

	of_node_put(dma_np);
	of_node_put(dma_channel_np);

	return 0;
}
EXPORT_SYMBOL(fsl_asoc_get_dma_channel);

 
void fsl_asoc_get_pll_clocks(struct device *dev, struct clk **pll8k_clk,
			     struct clk **pll11k_clk)
{
	*pll8k_clk = devm_clk_get(dev, "pll8k");
	if (IS_ERR(*pll8k_clk))
		*pll8k_clk = NULL;

	*pll11k_clk = devm_clk_get(dev, "pll11k");
	if (IS_ERR(*pll11k_clk))
		*pll11k_clk = NULL;
}
EXPORT_SYMBOL(fsl_asoc_get_pll_clocks);

 
void fsl_asoc_reparent_pll_clocks(struct device *dev, struct clk *clk,
				  struct clk *pll8k_clk,
				  struct clk *pll11k_clk, u64 ratio)
{
	struct clk *p, *pll = NULL, *npll = NULL;
	bool reparent = false;
	int ret;

	if (!clk || !pll8k_clk || !pll11k_clk)
		return;

	p = clk;
	while (p && pll8k_clk && pll11k_clk) {
		struct clk *pp = clk_get_parent(p);

		if (clk_is_match(pp, pll8k_clk) ||
		    clk_is_match(pp, pll11k_clk)) {
			pll = pp;
			break;
		}
		p = pp;
	}

	npll = (do_div(ratio, 8000) ? pll11k_clk : pll8k_clk);
	reparent = (pll && !clk_is_match(pll, npll));

	if (reparent) {
		ret = clk_set_parent(p, npll);
		if (ret < 0)
			dev_warn(dev, "failed to set parent:%d\n", ret);
	}
}
EXPORT_SYMBOL(fsl_asoc_reparent_pll_clocks);

MODULE_AUTHOR("Timur Tabi <timur@freescale.com>");
MODULE_DESCRIPTION("Freescale ASoC utility code");
MODULE_LICENSE("GPL v2");
