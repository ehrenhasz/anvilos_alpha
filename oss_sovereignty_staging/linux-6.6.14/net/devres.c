
 

#include <linux/device.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>

struct net_device_devres {
	struct net_device *ndev;
};

static void devm_free_netdev(struct device *dev, void *this)
{
	struct net_device_devres *res = this;

	free_netdev(res->ndev);
}

struct net_device *devm_alloc_etherdev_mqs(struct device *dev, int sizeof_priv,
					   unsigned int txqs, unsigned int rxqs)
{
	struct net_device_devres *dr;

	dr = devres_alloc(devm_free_netdev, sizeof(*dr), GFP_KERNEL);
	if (!dr)
		return NULL;

	dr->ndev = alloc_etherdev_mqs(sizeof_priv, txqs, rxqs);
	if (!dr->ndev) {
		devres_free(dr);
		return NULL;
	}

	devres_add(dev, dr);

	return dr->ndev;
}
EXPORT_SYMBOL(devm_alloc_etherdev_mqs);

static void devm_unregister_netdev(struct device *dev, void *this)
{
	struct net_device_devres *res = this;

	unregister_netdev(res->ndev);
}

static int netdev_devres_match(struct device *dev, void *this, void *match_data)
{
	struct net_device_devres *res = this;
	struct net_device *ndev = match_data;

	return ndev == res->ndev;
}

 
int devm_register_netdev(struct device *dev, struct net_device *ndev)
{
	struct net_device_devres *dr;
	int ret;

	 
	if (WARN_ON(!devres_find(dev, devm_free_netdev,
				 netdev_devres_match, ndev)))
		return -EINVAL;

	dr = devres_alloc(devm_unregister_netdev, sizeof(*dr), GFP_KERNEL);
	if (!dr)
		return -ENOMEM;

	ret = register_netdev(ndev);
	if (ret) {
		devres_free(dr);
		return ret;
	}

	dr->ndev = ndev;
	devres_add(ndev->dev.parent, dr);

	return 0;
}
EXPORT_SYMBOL(devm_register_netdev);
