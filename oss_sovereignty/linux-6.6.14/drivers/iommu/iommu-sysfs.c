
 

#include <linux/device.h>
#include <linux/iommu.h>
#include <linux/init.h>
#include <linux/slab.h>

 
static struct attribute *devices_attr[] = {
	NULL,
};

static const struct attribute_group devices_attr_group = {
	.name = "devices",
	.attrs = devices_attr,
};

static const struct attribute_group *dev_groups[] = {
	&devices_attr_group,
	NULL,
};

static void release_device(struct device *dev)
{
	kfree(dev);
}

static struct class iommu_class = {
	.name = "iommu",
	.dev_release = release_device,
	.dev_groups = dev_groups,
};

static int __init iommu_dev_init(void)
{
	return class_register(&iommu_class);
}
postcore_initcall(iommu_dev_init);

 
int iommu_device_sysfs_add(struct iommu_device *iommu,
			   struct device *parent,
			   const struct attribute_group **groups,
			   const char *fmt, ...)
{
	va_list vargs;
	int ret;

	iommu->dev = kzalloc(sizeof(*iommu->dev), GFP_KERNEL);
	if (!iommu->dev)
		return -ENOMEM;

	device_initialize(iommu->dev);

	iommu->dev->class = &iommu_class;
	iommu->dev->parent = parent;
	iommu->dev->groups = groups;

	va_start(vargs, fmt);
	ret = kobject_set_name_vargs(&iommu->dev->kobj, fmt, vargs);
	va_end(vargs);
	if (ret)
		goto error;

	ret = device_add(iommu->dev);
	if (ret)
		goto error;

	dev_set_drvdata(iommu->dev, iommu);

	return 0;

error:
	put_device(iommu->dev);
	return ret;
}
EXPORT_SYMBOL_GPL(iommu_device_sysfs_add);

void iommu_device_sysfs_remove(struct iommu_device *iommu)
{
	dev_set_drvdata(iommu->dev, NULL);
	device_unregister(iommu->dev);
	iommu->dev = NULL;
}
EXPORT_SYMBOL_GPL(iommu_device_sysfs_remove);

 
int iommu_device_link(struct iommu_device *iommu, struct device *link)
{
	int ret;

	ret = sysfs_add_link_to_group(&iommu->dev->kobj, "devices",
				      &link->kobj, dev_name(link));
	if (ret)
		return ret;

	ret = sysfs_create_link_nowarn(&link->kobj, &iommu->dev->kobj, "iommu");
	if (ret)
		sysfs_remove_link_from_group(&iommu->dev->kobj, "devices",
					     dev_name(link));

	return ret;
}

void iommu_device_unlink(struct iommu_device *iommu, struct device *link)
{
	sysfs_remove_link(&link->kobj, "iommu");
	sysfs_remove_link_from_group(&iommu->dev->kobj, "devices", dev_name(link));
}
