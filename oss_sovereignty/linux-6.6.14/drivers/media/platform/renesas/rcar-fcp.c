
 

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>

#include <media/rcar-fcp.h>

struct rcar_fcp_device {
	struct list_head list;
	struct device *dev;
};

static LIST_HEAD(fcp_devices);
static DEFINE_MUTEX(fcp_lock);

 

 
struct rcar_fcp_device *rcar_fcp_get(const struct device_node *np)
{
	struct rcar_fcp_device *fcp;

	mutex_lock(&fcp_lock);

	list_for_each_entry(fcp, &fcp_devices, list) {
		if (fcp->dev->of_node != np)
			continue;

		get_device(fcp->dev);
		goto done;
	}

	fcp = ERR_PTR(-EPROBE_DEFER);

done:
	mutex_unlock(&fcp_lock);
	return fcp;
}
EXPORT_SYMBOL_GPL(rcar_fcp_get);

 
void rcar_fcp_put(struct rcar_fcp_device *fcp)
{
	if (fcp)
		put_device(fcp->dev);
}
EXPORT_SYMBOL_GPL(rcar_fcp_put);

struct device *rcar_fcp_get_device(struct rcar_fcp_device *fcp)
{
	return fcp->dev;
}
EXPORT_SYMBOL_GPL(rcar_fcp_get_device);

 
int rcar_fcp_enable(struct rcar_fcp_device *fcp)
{
	if (!fcp)
		return 0;

	return pm_runtime_resume_and_get(fcp->dev);
}
EXPORT_SYMBOL_GPL(rcar_fcp_enable);

 
void rcar_fcp_disable(struct rcar_fcp_device *fcp)
{
	if (fcp)
		pm_runtime_put(fcp->dev);
}
EXPORT_SYMBOL_GPL(rcar_fcp_disable);

 

static int rcar_fcp_probe(struct platform_device *pdev)
{
	struct rcar_fcp_device *fcp;

	fcp = devm_kzalloc(&pdev->dev, sizeof(*fcp), GFP_KERNEL);
	if (fcp == NULL)
		return -ENOMEM;

	fcp->dev = &pdev->dev;

	dma_set_max_seg_size(fcp->dev, UINT_MAX);

	pm_runtime_enable(&pdev->dev);

	mutex_lock(&fcp_lock);
	list_add_tail(&fcp->list, &fcp_devices);
	mutex_unlock(&fcp_lock);

	platform_set_drvdata(pdev, fcp);

	return 0;
}

static void rcar_fcp_remove(struct platform_device *pdev)
{
	struct rcar_fcp_device *fcp = platform_get_drvdata(pdev);

	mutex_lock(&fcp_lock);
	list_del(&fcp->list);
	mutex_unlock(&fcp_lock);

	pm_runtime_disable(&pdev->dev);
}

static const struct of_device_id rcar_fcp_of_match[] = {
	{ .compatible = "renesas,fcpf" },
	{ .compatible = "renesas,fcpv" },
	{ },
};
MODULE_DEVICE_TABLE(of, rcar_fcp_of_match);

static struct platform_driver rcar_fcp_platform_driver = {
	.probe		= rcar_fcp_probe,
	.remove_new	= rcar_fcp_remove,
	.driver		= {
		.name	= "rcar-fcp",
		.of_match_table = rcar_fcp_of_match,
		.suppress_bind_attrs = true,
	},
};

module_platform_driver(rcar_fcp_platform_driver);

MODULE_ALIAS("rcar-fcp");
MODULE_AUTHOR("Laurent Pinchart <laurent.pinchart@ideasonboard.com>");
MODULE_DESCRIPTION("Renesas FCP Driver");
MODULE_LICENSE("GPL");
