
 

#include <linux/clk.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "ingenic_ecc.h"

 
int ingenic_ecc_calculate(struct ingenic_ecc *ecc,
			  struct ingenic_ecc_params *params,
			  const u8 *buf, u8 *ecc_code)
{
	return ecc->ops->calculate(ecc, params, buf, ecc_code);
}

 
int ingenic_ecc_correct(struct ingenic_ecc *ecc,
			struct ingenic_ecc_params *params,
			u8 *buf, u8 *ecc_code)
{
	return ecc->ops->correct(ecc, params, buf, ecc_code);
}

 
static struct ingenic_ecc *ingenic_ecc_get(struct device_node *np)
{
	struct platform_device *pdev;
	struct ingenic_ecc *ecc;

	pdev = of_find_device_by_node(np);
	if (!pdev)
		return ERR_PTR(-EPROBE_DEFER);

	if (!platform_get_drvdata(pdev)) {
		put_device(&pdev->dev);
		return ERR_PTR(-EPROBE_DEFER);
	}

	ecc = platform_get_drvdata(pdev);
	clk_prepare_enable(ecc->clk);

	return ecc;
}

 
struct ingenic_ecc *of_ingenic_ecc_get(struct device_node *of_node)
{
	struct ingenic_ecc *ecc = NULL;
	struct device_node *np;

	np = of_parse_phandle(of_node, "ecc-engine", 0);

	 
	if (!np)
		np = of_parse_phandle(of_node, "ingenic,bch-controller", 0);

	if (np) {
		ecc = ingenic_ecc_get(np);
		of_node_put(np);
	}
	return ecc;
}

 
void ingenic_ecc_release(struct ingenic_ecc *ecc)
{
	clk_disable_unprepare(ecc->clk);
	put_device(ecc->dev);
}

int ingenic_ecc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ingenic_ecc *ecc;

	ecc = devm_kzalloc(dev, sizeof(*ecc), GFP_KERNEL);
	if (!ecc)
		return -ENOMEM;

	ecc->ops = device_get_match_data(dev);
	if (!ecc->ops)
		return -EINVAL;

	ecc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ecc->base))
		return PTR_ERR(ecc->base);

	ecc->ops->disable(ecc);

	ecc->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(ecc->clk)) {
		dev_err(dev, "failed to get clock: %ld\n", PTR_ERR(ecc->clk));
		return PTR_ERR(ecc->clk);
	}

	mutex_init(&ecc->lock);

	ecc->dev = dev;
	platform_set_drvdata(pdev, ecc);

	return 0;
}
EXPORT_SYMBOL(ingenic_ecc_probe);
