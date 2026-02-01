
 

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include "ctucanfd.h"

#define DRV_NAME	"ctucanfd"

static void ctucan_platform_set_drvdata(struct device *dev,
					struct net_device *ndev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);

	platform_set_drvdata(pdev, ndev);
}

 
static int ctucan_platform_probe(struct platform_device *pdev)
{
	struct device	*dev = &pdev->dev;
	void __iomem *addr;
	int ret;
	unsigned int ntxbufs;
	int irq;

	 
	addr = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(addr)) {
		ret = PTR_ERR(addr);
		goto err;
	}
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto err;
	}

	 
	ntxbufs = 4;
	ret = ctucan_probe_common(dev, addr, irq, ntxbufs, 0,
				  1, ctucan_platform_set_drvdata);

	if (ret < 0)
		platform_set_drvdata(pdev, NULL);

err:
	return ret;
}

 
static void ctucan_platform_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct ctucan_priv *priv = netdev_priv(ndev);

	netdev_dbg(ndev, "ctucan_remove");

	unregister_candev(ndev);
	pm_runtime_disable(&pdev->dev);
	netif_napi_del(&priv->napi);
	free_candev(ndev);
}

static SIMPLE_DEV_PM_OPS(ctucan_platform_pm_ops, ctucan_suspend, ctucan_resume);

 
static const struct of_device_id ctucan_of_match[] = {
	{ .compatible = "ctu,ctucanfd-2", },
	{ .compatible = "ctu,ctucanfd", },
	{   },
};
MODULE_DEVICE_TABLE(of, ctucan_of_match);

static struct platform_driver ctucanfd_driver = {
	.probe	= ctucan_platform_probe,
	.remove_new = ctucan_platform_remove,
	.driver	= {
		.name = DRV_NAME,
		.pm = &ctucan_platform_pm_ops,
		.of_match_table	= ctucan_of_match,
	},
};

module_platform_driver(ctucanfd_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Martin Jerabek");
MODULE_DESCRIPTION("CTU CAN FD for platform");
