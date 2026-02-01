
 

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#define OCP2SCP_TIMING 0x18
#define SYNC2_MASK 0xf

static int ocp2scp_remove_devices(struct device *dev, void *c)
{
	struct platform_device *pdev = to_platform_device(dev);

	platform_device_unregister(pdev);

	return 0;
}

static int omap_ocp2scp_probe(struct platform_device *pdev)
{
	int ret;
	u32 reg;
	void __iomem *regs;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;

	if (np) {
		ret = of_platform_populate(np, NULL, NULL, &pdev->dev);
		if (ret) {
			dev_err(&pdev->dev,
			    "failed to add resources for ocp2scp child\n");
			goto err0;
		}
	}

	pm_runtime_enable(&pdev->dev);
	 
	if (!of_device_is_compatible(np, "ti,am437x-ocp2scp")) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		regs = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(regs)) {
			ret = PTR_ERR(regs);
			goto err1;
		}

		pm_runtime_get_sync(&pdev->dev);
		reg = readl_relaxed(regs + OCP2SCP_TIMING);
		reg &= ~(SYNC2_MASK);
		reg |= 0x6;
		writel_relaxed(reg, regs + OCP2SCP_TIMING);
		pm_runtime_put_sync(&pdev->dev);
	}

	return 0;

err1:
	pm_runtime_disable(&pdev->dev);

err0:
	device_for_each_child(&pdev->dev, NULL, ocp2scp_remove_devices);

	return ret;
}

static int omap_ocp2scp_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	device_for_each_child(&pdev->dev, NULL, ocp2scp_remove_devices);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id omap_ocp2scp_id_table[] = {
	{ .compatible = "ti,omap-ocp2scp" },
	{ .compatible = "ti,am437x-ocp2scp" },
	{}
};
MODULE_DEVICE_TABLE(of, omap_ocp2scp_id_table);
#endif

static struct platform_driver omap_ocp2scp_driver = {
	.probe		= omap_ocp2scp_probe,
	.remove		= omap_ocp2scp_remove,
	.driver		= {
		.name	= "omap-ocp2scp",
		.of_match_table = of_match_ptr(omap_ocp2scp_id_table),
	},
};

module_platform_driver(omap_ocp2scp_driver);

MODULE_ALIAS("platform:omap-ocp2scp");
MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("OMAP OCP2SCP driver");
MODULE_LICENSE("GPL v2");
