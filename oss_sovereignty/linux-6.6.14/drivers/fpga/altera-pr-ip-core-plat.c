
 
#include <linux/fpga/altera-pr-ip-core.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>

static int alt_pr_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	void __iomem *reg_base;

	 
	reg_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(reg_base))
		return PTR_ERR(reg_base);

	return alt_pr_register(dev, reg_base);
}

static const struct of_device_id alt_pr_of_match[] = {
	{ .compatible = "altr,a10-pr-ip", },
	{},
};

MODULE_DEVICE_TABLE(of, alt_pr_of_match);

static struct platform_driver alt_pr_platform_driver = {
	.probe = alt_pr_platform_probe,
	.driver = {
		.name	= "alt_a10_pr_ip",
		.of_match_table = alt_pr_of_match,
	},
};

module_platform_driver(alt_pr_platform_driver);
MODULE_AUTHOR("Matthew Gerlach <matthew.gerlach@linux.intel.com>");
MODULE_DESCRIPTION("Altera Partial Reconfiguration IP Platform Driver");
MODULE_LICENSE("GPL v2");
