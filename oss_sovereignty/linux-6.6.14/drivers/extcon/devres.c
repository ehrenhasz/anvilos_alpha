
 

#include "extcon.h"

static int devm_extcon_dev_match(struct device *dev, void *res, void *data)
{
	struct extcon_dev **r = res;

	if (WARN_ON(!r || !*r))
		return 0;

	return *r == data;
}

static void devm_extcon_dev_release(struct device *dev, void *res)
{
	extcon_dev_free(*(struct extcon_dev **)res);
}


static void devm_extcon_dev_unreg(struct device *dev, void *res)
{
	extcon_dev_unregister(*(struct extcon_dev **)res);
}

struct extcon_dev_notifier_devres {
	struct extcon_dev *edev;
	unsigned int id;
	struct notifier_block *nb;
};

static void devm_extcon_dev_notifier_unreg(struct device *dev, void *res)
{
	struct extcon_dev_notifier_devres *this = res;

	extcon_unregister_notifier(this->edev, this->id, this->nb);
}

static void devm_extcon_dev_notifier_all_unreg(struct device *dev, void *res)
{
	struct extcon_dev_notifier_devres *this = res;

	extcon_unregister_notifier_all(this->edev, this->nb);
}

 
struct extcon_dev *devm_extcon_dev_allocate(struct device *dev,
					const unsigned int *supported_cable)
{
	struct extcon_dev **ptr, *edev;

	ptr = devres_alloc(devm_extcon_dev_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	edev = extcon_dev_allocate(supported_cable);
	if (IS_ERR(edev)) {
		devres_free(ptr);
		return edev;
	}

	edev->dev.parent = dev;

	*ptr = edev;
	devres_add(dev, ptr);

	return edev;
}
EXPORT_SYMBOL_GPL(devm_extcon_dev_allocate);

 
void devm_extcon_dev_free(struct device *dev, struct extcon_dev *edev)
{
	WARN_ON(devres_release(dev, devm_extcon_dev_release,
			       devm_extcon_dev_match, edev));
}
EXPORT_SYMBOL_GPL(devm_extcon_dev_free);

 
int devm_extcon_dev_register(struct device *dev, struct extcon_dev *edev)
{
	struct extcon_dev **ptr;
	int ret;

	ptr = devres_alloc(devm_extcon_dev_unreg, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	ret = extcon_dev_register(edev);
	if (ret) {
		devres_free(ptr);
		return ret;
	}

	*ptr = edev;
	devres_add(dev, ptr);

	return 0;
}
EXPORT_SYMBOL_GPL(devm_extcon_dev_register);

 
void devm_extcon_dev_unregister(struct device *dev, struct extcon_dev *edev)
{
	WARN_ON(devres_release(dev, devm_extcon_dev_unreg,
			       devm_extcon_dev_match, edev));
}
EXPORT_SYMBOL_GPL(devm_extcon_dev_unregister);

 
int devm_extcon_register_notifier(struct device *dev, struct extcon_dev *edev,
				unsigned int id, struct notifier_block *nb)
{
	struct extcon_dev_notifier_devres *ptr;
	int ret;

	ptr = devres_alloc(devm_extcon_dev_notifier_unreg, sizeof(*ptr),
				GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	ret = extcon_register_notifier(edev, id, nb);
	if (ret) {
		devres_free(ptr);
		return ret;
	}

	ptr->edev = edev;
	ptr->id = id;
	ptr->nb = nb;
	devres_add(dev, ptr);

	return 0;
}
EXPORT_SYMBOL(devm_extcon_register_notifier);

 
void devm_extcon_unregister_notifier(struct device *dev,
				struct extcon_dev *edev, unsigned int id,
				struct notifier_block *nb)
{
	WARN_ON(devres_release(dev, devm_extcon_dev_notifier_unreg,
			       devm_extcon_dev_match, edev));
}
EXPORT_SYMBOL(devm_extcon_unregister_notifier);

 
int devm_extcon_register_notifier_all(struct device *dev, struct extcon_dev *edev,
				struct notifier_block *nb)
{
	struct extcon_dev_notifier_devres *ptr;
	int ret;

	ptr = devres_alloc(devm_extcon_dev_notifier_all_unreg, sizeof(*ptr),
				GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	ret = extcon_register_notifier_all(edev, nb);
	if (ret) {
		devres_free(ptr);
		return ret;
	}

	ptr->edev = edev;
	ptr->nb = nb;
	devres_add(dev, ptr);

	return 0;
}
EXPORT_SYMBOL(devm_extcon_register_notifier_all);

 
void devm_extcon_unregister_notifier_all(struct device *dev,
				struct extcon_dev *edev,
				struct notifier_block *nb)
{
	WARN_ON(devres_release(dev, devm_extcon_dev_notifier_all_unreg,
			       devm_extcon_dev_match, edev));
}
EXPORT_SYMBOL(devm_extcon_unregister_notifier_all);
