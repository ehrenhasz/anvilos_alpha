
 

#define pr_fmt(fmt)    "iommu: " fmt

#include <linux/amba/bus.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/bits.h>
#include <linux/bug.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/host1x_context_bus.h>
#include <linux/iommu.h>
#include <linux/idr.h>
#include <linux/err.h>
#include <linux/pci.h>
#include <linux/pci-ats.h>
#include <linux/bitops.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/fsl/mc.h>
#include <linux/module.h>
#include <linux/cc_platform.h>
#include <linux/cdx/cdx_bus.h>
#include <trace/events/iommu.h>
#include <linux/sched/mm.h>
#include <linux/msi.h>

#include "dma-iommu.h"
#include "iommu-priv.h"

#include "iommu-sva.h"
#include "iommu-priv.h"

static struct kset *iommu_group_kset;
static DEFINE_IDA(iommu_group_ida);
static DEFINE_IDA(iommu_global_pasid_ida);

static unsigned int iommu_def_domain_type __read_mostly;
static bool iommu_dma_strict __read_mostly = IS_ENABLED(CONFIG_IOMMU_DEFAULT_DMA_STRICT);
static u32 iommu_cmd_line __read_mostly;

struct iommu_group {
	struct kobject kobj;
	struct kobject *devices_kobj;
	struct list_head devices;
	struct xarray pasid_array;
	struct mutex mutex;
	void *iommu_data;
	void (*iommu_data_release)(void *iommu_data);
	char *name;
	int id;
	struct iommu_domain *default_domain;
	struct iommu_domain *blocking_domain;
	struct iommu_domain *domain;
	struct list_head entry;
	unsigned int owner_cnt;
	void *owner;
};

struct group_device {
	struct list_head list;
	struct device *dev;
	char *name;
};

 
#define for_each_group_device(group, pos) \
	list_for_each_entry(pos, &(group)->devices, list)

struct iommu_group_attribute {
	struct attribute attr;
	ssize_t (*show)(struct iommu_group *group, char *buf);
	ssize_t (*store)(struct iommu_group *group,
			 const char *buf, size_t count);
};

static const char * const iommu_group_resv_type_string[] = {
	[IOMMU_RESV_DIRECT]			= "direct",
	[IOMMU_RESV_DIRECT_RELAXABLE]		= "direct-relaxable",
	[IOMMU_RESV_RESERVED]			= "reserved",
	[IOMMU_RESV_MSI]			= "msi",
	[IOMMU_RESV_SW_MSI]			= "msi",
};

#define IOMMU_CMD_LINE_DMA_API		BIT(0)
#define IOMMU_CMD_LINE_STRICT		BIT(1)

static int iommu_bus_notifier(struct notifier_block *nb,
			      unsigned long action, void *data);
static void iommu_release_device(struct device *dev);
static struct iommu_domain *__iommu_domain_alloc(const struct bus_type *bus,
						 unsigned type);
static int __iommu_attach_device(struct iommu_domain *domain,
				 struct device *dev);
static int __iommu_attach_group(struct iommu_domain *domain,
				struct iommu_group *group);

enum {
	IOMMU_SET_DOMAIN_MUST_SUCCEED = 1 << 0,
};

static int __iommu_device_set_domain(struct iommu_group *group,
				     struct device *dev,
				     struct iommu_domain *new_domain,
				     unsigned int flags);
static int __iommu_group_set_domain_internal(struct iommu_group *group,
					     struct iommu_domain *new_domain,
					     unsigned int flags);
static int __iommu_group_set_domain(struct iommu_group *group,
				    struct iommu_domain *new_domain)
{
	return __iommu_group_set_domain_internal(group, new_domain, 0);
}
static void __iommu_group_set_domain_nofail(struct iommu_group *group,
					    struct iommu_domain *new_domain)
{
	WARN_ON(__iommu_group_set_domain_internal(
		group, new_domain, IOMMU_SET_DOMAIN_MUST_SUCCEED));
}

static int iommu_setup_default_domain(struct iommu_group *group,
				      int target_type);
static int iommu_create_device_direct_mappings(struct iommu_domain *domain,
					       struct device *dev);
static ssize_t iommu_group_store_type(struct iommu_group *group,
				      const char *buf, size_t count);
static struct group_device *iommu_group_alloc_device(struct iommu_group *group,
						     struct device *dev);
static void __iommu_group_free_device(struct iommu_group *group,
				      struct group_device *grp_dev);

#define IOMMU_GROUP_ATTR(_name, _mode, _show, _store)		\
struct iommu_group_attribute iommu_group_attr_##_name =		\
	__ATTR(_name, _mode, _show, _store)

#define to_iommu_group_attr(_attr)	\
	container_of(_attr, struct iommu_group_attribute, attr)
#define to_iommu_group(_kobj)		\
	container_of(_kobj, struct iommu_group, kobj)

static LIST_HEAD(iommu_device_list);
static DEFINE_SPINLOCK(iommu_device_lock);

static struct bus_type * const iommu_buses[] = {
	&platform_bus_type,
#ifdef CONFIG_PCI
	&pci_bus_type,
#endif
#ifdef CONFIG_ARM_AMBA
	&amba_bustype,
#endif
#ifdef CONFIG_FSL_MC_BUS
	&fsl_mc_bus_type,
#endif
#ifdef CONFIG_TEGRA_HOST1X_CONTEXT_BUS
	&host1x_context_device_bus_type,
#endif
#ifdef CONFIG_CDX_BUS
	&cdx_bus_type,
#endif
};

 
static const char *iommu_domain_type_str(unsigned int t)
{
	switch (t) {
	case IOMMU_DOMAIN_BLOCKED:
		return "Blocked";
	case IOMMU_DOMAIN_IDENTITY:
		return "Passthrough";
	case IOMMU_DOMAIN_UNMANAGED:
		return "Unmanaged";
	case IOMMU_DOMAIN_DMA:
	case IOMMU_DOMAIN_DMA_FQ:
		return "Translated";
	default:
		return "Unknown";
	}
}

static int __init iommu_subsys_init(void)
{
	struct notifier_block *nb;

	if (!(iommu_cmd_line & IOMMU_CMD_LINE_DMA_API)) {
		if (IS_ENABLED(CONFIG_IOMMU_DEFAULT_PASSTHROUGH))
			iommu_set_default_passthrough(false);
		else
			iommu_set_default_translated(false);

		if (iommu_default_passthrough() && cc_platform_has(CC_ATTR_MEM_ENCRYPT)) {
			pr_info("Memory encryption detected - Disabling default IOMMU Passthrough\n");
			iommu_set_default_translated(false);
		}
	}

	if (!iommu_default_passthrough() && !iommu_dma_strict)
		iommu_def_domain_type = IOMMU_DOMAIN_DMA_FQ;

	pr_info("Default domain type: %s%s\n",
		iommu_domain_type_str(iommu_def_domain_type),
		(iommu_cmd_line & IOMMU_CMD_LINE_DMA_API) ?
			" (set via kernel command line)" : "");

	if (!iommu_default_passthrough())
		pr_info("DMA domain TLB invalidation policy: %s mode%s\n",
			iommu_dma_strict ? "strict" : "lazy",
			(iommu_cmd_line & IOMMU_CMD_LINE_STRICT) ?
				" (set via kernel command line)" : "");

	nb = kcalloc(ARRAY_SIZE(iommu_buses), sizeof(*nb), GFP_KERNEL);
	if (!nb)
		return -ENOMEM;

	for (int i = 0; i < ARRAY_SIZE(iommu_buses); i++) {
		nb[i].notifier_call = iommu_bus_notifier;
		bus_register_notifier(iommu_buses[i], &nb[i]);
	}

	return 0;
}
subsys_initcall(iommu_subsys_init);

static int remove_iommu_group(struct device *dev, void *data)
{
	if (dev->iommu && dev->iommu->iommu_dev == data)
		iommu_release_device(dev);

	return 0;
}

 
int iommu_device_register(struct iommu_device *iommu,
			  const struct iommu_ops *ops, struct device *hwdev)
{
	int err = 0;

	 
	if (WARN_ON(is_module_address((unsigned long)ops) && !ops->owner))
		return -EINVAL;
	 
	if (iommu_buses[0]->iommu_ops && iommu_buses[0]->iommu_ops != ops)
		return -EBUSY;

	iommu->ops = ops;
	if (hwdev)
		iommu->fwnode = dev_fwnode(hwdev);

	spin_lock(&iommu_device_lock);
	list_add_tail(&iommu->list, &iommu_device_list);
	spin_unlock(&iommu_device_lock);

	for (int i = 0; i < ARRAY_SIZE(iommu_buses) && !err; i++) {
		iommu_buses[i]->iommu_ops = ops;
		err = bus_iommu_probe(iommu_buses[i]);
	}
	if (err)
		iommu_device_unregister(iommu);
	return err;
}
EXPORT_SYMBOL_GPL(iommu_device_register);

void iommu_device_unregister(struct iommu_device *iommu)
{
	for (int i = 0; i < ARRAY_SIZE(iommu_buses); i++)
		bus_for_each_dev(iommu_buses[i], NULL, iommu, remove_iommu_group);

	spin_lock(&iommu_device_lock);
	list_del(&iommu->list);
	spin_unlock(&iommu_device_lock);
}
EXPORT_SYMBOL_GPL(iommu_device_unregister);

#if IS_ENABLED(CONFIG_IOMMUFD_TEST)
void iommu_device_unregister_bus(struct iommu_device *iommu,
				 struct bus_type *bus,
				 struct notifier_block *nb)
{
	bus_unregister_notifier(bus, nb);
	iommu_device_unregister(iommu);
}
EXPORT_SYMBOL_GPL(iommu_device_unregister_bus);

 
int iommu_device_register_bus(struct iommu_device *iommu,
			      const struct iommu_ops *ops, struct bus_type *bus,
			      struct notifier_block *nb)
{
	int err;

	iommu->ops = ops;
	nb->notifier_call = iommu_bus_notifier;
	err = bus_register_notifier(bus, nb);
	if (err)
		return err;

	spin_lock(&iommu_device_lock);
	list_add_tail(&iommu->list, &iommu_device_list);
	spin_unlock(&iommu_device_lock);

	bus->iommu_ops = ops;
	err = bus_iommu_probe(bus);
	if (err) {
		iommu_device_unregister_bus(iommu, bus, nb);
		return err;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(iommu_device_register_bus);
#endif

static struct dev_iommu *dev_iommu_get(struct device *dev)
{
	struct dev_iommu *param = dev->iommu;

	if (param)
		return param;

	param = kzalloc(sizeof(*param), GFP_KERNEL);
	if (!param)
		return NULL;

	mutex_init(&param->lock);
	dev->iommu = param;
	return param;
}

static void dev_iommu_free(struct device *dev)
{
	struct dev_iommu *param = dev->iommu;

	dev->iommu = NULL;
	if (param->fwspec) {
		fwnode_handle_put(param->fwspec->iommu_fwnode);
		kfree(param->fwspec);
	}
	kfree(param);
}

static u32 dev_iommu_get_max_pasids(struct device *dev)
{
	u32 max_pasids = 0, bits = 0;
	int ret;

	if (dev_is_pci(dev)) {
		ret = pci_max_pasids(to_pci_dev(dev));
		if (ret > 0)
			max_pasids = ret;
	} else {
		ret = device_property_read_u32(dev, "pasid-num-bits", &bits);
		if (!ret)
			max_pasids = 1UL << bits;
	}

	return min_t(u32, max_pasids, dev->iommu->iommu_dev->max_pasids);
}

 
static int iommu_init_device(struct device *dev, const struct iommu_ops *ops)
{
	struct iommu_device *iommu_dev;
	struct iommu_group *group;
	int ret;

	if (!dev_iommu_get(dev))
		return -ENOMEM;

	if (!try_module_get(ops->owner)) {
		ret = -EINVAL;
		goto err_free;
	}

	iommu_dev = ops->probe_device(dev);
	if (IS_ERR(iommu_dev)) {
		ret = PTR_ERR(iommu_dev);
		goto err_module_put;
	}

	ret = iommu_device_link(iommu_dev, dev);
	if (ret)
		goto err_release;

	group = ops->device_group(dev);
	if (WARN_ON_ONCE(group == NULL))
		group = ERR_PTR(-EINVAL);
	if (IS_ERR(group)) {
		ret = PTR_ERR(group);
		goto err_unlink;
	}
	dev->iommu_group = group;

	dev->iommu->iommu_dev = iommu_dev;
	dev->iommu->max_pasids = dev_iommu_get_max_pasids(dev);
	if (ops->is_attach_deferred)
		dev->iommu->attach_deferred = ops->is_attach_deferred(dev);
	return 0;

err_unlink:
	iommu_device_unlink(iommu_dev, dev);
err_release:
	if (ops->release_device)
		ops->release_device(dev);
err_module_put:
	module_put(ops->owner);
err_free:
	dev_iommu_free(dev);
	return ret;
}

static void iommu_deinit_device(struct device *dev)
{
	struct iommu_group *group = dev->iommu_group;
	const struct iommu_ops *ops = dev_iommu_ops(dev);

	lockdep_assert_held(&group->mutex);

	iommu_device_unlink(dev->iommu->iommu_dev, dev);

	 
	if (ops->release_device)
		ops->release_device(dev);

	 
	if (list_empty(&group->devices)) {
		if (group->default_domain) {
			iommu_domain_free(group->default_domain);
			group->default_domain = NULL;
		}
		if (group->blocking_domain) {
			iommu_domain_free(group->blocking_domain);
			group->blocking_domain = NULL;
		}
		group->domain = NULL;
	}

	 
	dev->iommu_group = NULL;
	module_put(ops->owner);
	dev_iommu_free(dev);
}

DEFINE_MUTEX(iommu_probe_device_lock);

static int __iommu_probe_device(struct device *dev, struct list_head *group_list)
{
	const struct iommu_ops *ops = dev->bus->iommu_ops;
	struct iommu_group *group;
	struct group_device *gdev;
	int ret;

	if (!ops)
		return -ENODEV;
	 
	lockdep_assert_held(&iommu_probe_device_lock);

	 
	if (dev->iommu_group)
		return 0;

	ret = iommu_init_device(dev, ops);
	if (ret)
		return ret;

	group = dev->iommu_group;
	gdev = iommu_group_alloc_device(group, dev);
	mutex_lock(&group->mutex);
	if (IS_ERR(gdev)) {
		ret = PTR_ERR(gdev);
		goto err_put_group;
	}

	 
	list_add_tail(&gdev->list, &group->devices);
	WARN_ON(group->default_domain && !group->domain);
	if (group->default_domain)
		iommu_create_device_direct_mappings(group->default_domain, dev);
	if (group->domain) {
		ret = __iommu_device_set_domain(group, dev, group->domain, 0);
		if (ret)
			goto err_remove_gdev;
	} else if (!group->default_domain && !group_list) {
		ret = iommu_setup_default_domain(group, 0);
		if (ret)
			goto err_remove_gdev;
	} else if (!group->default_domain) {
		 
		if (list_empty(&group->entry))
			list_add_tail(&group->entry, group_list);
	}
	mutex_unlock(&group->mutex);

	if (dev_is_pci(dev))
		iommu_dma_set_pci_32bit_workaround(dev);

	return 0;

err_remove_gdev:
	list_del(&gdev->list);
	__iommu_group_free_device(group, gdev);
err_put_group:
	iommu_deinit_device(dev);
	mutex_unlock(&group->mutex);
	iommu_group_put(group);

	return ret;
}

int iommu_probe_device(struct device *dev)
{
	const struct iommu_ops *ops;
	int ret;

	mutex_lock(&iommu_probe_device_lock);
	ret = __iommu_probe_device(dev, NULL);
	mutex_unlock(&iommu_probe_device_lock);
	if (ret)
		return ret;

	ops = dev_iommu_ops(dev);
	if (ops->probe_finalize)
		ops->probe_finalize(dev);

	return 0;
}

static void __iommu_group_free_device(struct iommu_group *group,
				      struct group_device *grp_dev)
{
	struct device *dev = grp_dev->dev;

	sysfs_remove_link(group->devices_kobj, grp_dev->name);
	sysfs_remove_link(&dev->kobj, "iommu_group");

	trace_remove_device_from_group(group->id, dev);

	 
	if (list_empty(&group->devices))
		WARN_ON(group->owner_cnt ||
			group->domain != group->default_domain);

	kfree(grp_dev->name);
	kfree(grp_dev);
}

 
static void __iommu_group_remove_device(struct device *dev)
{
	struct iommu_group *group = dev->iommu_group;
	struct group_device *device;

	mutex_lock(&group->mutex);
	for_each_group_device(group, device) {
		if (device->dev != dev)
			continue;

		list_del(&device->list);
		__iommu_group_free_device(group, device);
		if (dev->iommu && dev->iommu->iommu_dev)
			iommu_deinit_device(dev);
		else
			dev->iommu_group = NULL;
		break;
	}
	mutex_unlock(&group->mutex);

	 
	iommu_group_put(group);
}

static void iommu_release_device(struct device *dev)
{
	struct iommu_group *group = dev->iommu_group;

	if (group)
		__iommu_group_remove_device(dev);

	 
	if (dev->iommu)
		dev_iommu_free(dev);
}

static int __init iommu_set_def_domain_type(char *str)
{
	bool pt;
	int ret;

	ret = kstrtobool(str, &pt);
	if (ret)
		return ret;

	if (pt)
		iommu_set_default_passthrough(true);
	else
		iommu_set_default_translated(true);

	return 0;
}
early_param("iommu.passthrough", iommu_set_def_domain_type);

static int __init iommu_dma_setup(char *str)
{
	int ret = kstrtobool(str, &iommu_dma_strict);

	if (!ret)
		iommu_cmd_line |= IOMMU_CMD_LINE_STRICT;
	return ret;
}
early_param("iommu.strict", iommu_dma_setup);

void iommu_set_dma_strict(void)
{
	iommu_dma_strict = true;
	if (iommu_def_domain_type == IOMMU_DOMAIN_DMA_FQ)
		iommu_def_domain_type = IOMMU_DOMAIN_DMA;
}

static ssize_t iommu_group_attr_show(struct kobject *kobj,
				     struct attribute *__attr, char *buf)
{
	struct iommu_group_attribute *attr = to_iommu_group_attr(__attr);
	struct iommu_group *group = to_iommu_group(kobj);
	ssize_t ret = -EIO;

	if (attr->show)
		ret = attr->show(group, buf);
	return ret;
}

static ssize_t iommu_group_attr_store(struct kobject *kobj,
				      struct attribute *__attr,
				      const char *buf, size_t count)
{
	struct iommu_group_attribute *attr = to_iommu_group_attr(__attr);
	struct iommu_group *group = to_iommu_group(kobj);
	ssize_t ret = -EIO;

	if (attr->store)
		ret = attr->store(group, buf, count);
	return ret;
}

static const struct sysfs_ops iommu_group_sysfs_ops = {
	.show = iommu_group_attr_show,
	.store = iommu_group_attr_store,
};

static int iommu_group_create_file(struct iommu_group *group,
				   struct iommu_group_attribute *attr)
{
	return sysfs_create_file(&group->kobj, &attr->attr);
}

static void iommu_group_remove_file(struct iommu_group *group,
				    struct iommu_group_attribute *attr)
{
	sysfs_remove_file(&group->kobj, &attr->attr);
}

static ssize_t iommu_group_show_name(struct iommu_group *group, char *buf)
{
	return sysfs_emit(buf, "%s\n", group->name);
}

 
static int iommu_insert_resv_region(struct iommu_resv_region *new,
				    struct list_head *regions)
{
	struct iommu_resv_region *iter, *tmp, *nr, *top;
	LIST_HEAD(stack);

	nr = iommu_alloc_resv_region(new->start, new->length,
				     new->prot, new->type, GFP_KERNEL);
	if (!nr)
		return -ENOMEM;

	 
	list_for_each_entry(iter, regions, list) {
		if (nr->start < iter->start ||
		    (nr->start == iter->start && nr->type <= iter->type))
			break;
	}
	list_add_tail(&nr->list, &iter->list);

	 
	list_for_each_entry_safe(iter, tmp, regions, list) {
		phys_addr_t top_end, iter_end = iter->start + iter->length - 1;

		 
		if (iter->type != new->type) {
			list_move_tail(&iter->list, &stack);
			continue;
		}

		 
		list_for_each_entry_reverse(top, &stack, list)
			if (top->type == iter->type)
				goto check_overlap;

		list_move_tail(&iter->list, &stack);
		continue;

check_overlap:
		top_end = top->start + top->length - 1;

		if (iter->start > top_end + 1) {
			list_move_tail(&iter->list, &stack);
		} else {
			top->length = max(top_end, iter_end) - top->start + 1;
			list_del(&iter->list);
			kfree(iter);
		}
	}
	list_splice(&stack, regions);
	return 0;
}

static int
iommu_insert_device_resv_regions(struct list_head *dev_resv_regions,
				 struct list_head *group_resv_regions)
{
	struct iommu_resv_region *entry;
	int ret = 0;

	list_for_each_entry(entry, dev_resv_regions, list) {
		ret = iommu_insert_resv_region(entry, group_resv_regions);
		if (ret)
			break;
	}
	return ret;
}

int iommu_get_group_resv_regions(struct iommu_group *group,
				 struct list_head *head)
{
	struct group_device *device;
	int ret = 0;

	mutex_lock(&group->mutex);
	for_each_group_device(group, device) {
		struct list_head dev_resv_regions;

		 
		if (!device->dev->iommu)
			break;

		INIT_LIST_HEAD(&dev_resv_regions);
		iommu_get_resv_regions(device->dev, &dev_resv_regions);
		ret = iommu_insert_device_resv_regions(&dev_resv_regions, head);
		iommu_put_resv_regions(device->dev, &dev_resv_regions);
		if (ret)
			break;
	}
	mutex_unlock(&group->mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(iommu_get_group_resv_regions);

static ssize_t iommu_group_show_resv_regions(struct iommu_group *group,
					     char *buf)
{
	struct iommu_resv_region *region, *next;
	struct list_head group_resv_regions;
	int offset = 0;

	INIT_LIST_HEAD(&group_resv_regions);
	iommu_get_group_resv_regions(group, &group_resv_regions);

	list_for_each_entry_safe(region, next, &group_resv_regions, list) {
		offset += sysfs_emit_at(buf, offset, "0x%016llx 0x%016llx %s\n",
					(long long)region->start,
					(long long)(region->start +
						    region->length - 1),
					iommu_group_resv_type_string[region->type]);
		kfree(region);
	}

	return offset;
}

static ssize_t iommu_group_show_type(struct iommu_group *group,
				     char *buf)
{
	char *type = "unknown";

	mutex_lock(&group->mutex);
	if (group->default_domain) {
		switch (group->default_domain->type) {
		case IOMMU_DOMAIN_BLOCKED:
			type = "blocked";
			break;
		case IOMMU_DOMAIN_IDENTITY:
			type = "identity";
			break;
		case IOMMU_DOMAIN_UNMANAGED:
			type = "unmanaged";
			break;
		case IOMMU_DOMAIN_DMA:
			type = "DMA";
			break;
		case IOMMU_DOMAIN_DMA_FQ:
			type = "DMA-FQ";
			break;
		}
	}
	mutex_unlock(&group->mutex);

	return sysfs_emit(buf, "%s\n", type);
}

static IOMMU_GROUP_ATTR(name, S_IRUGO, iommu_group_show_name, NULL);

static IOMMU_GROUP_ATTR(reserved_regions, 0444,
			iommu_group_show_resv_regions, NULL);

static IOMMU_GROUP_ATTR(type, 0644, iommu_group_show_type,
			iommu_group_store_type);

static void iommu_group_release(struct kobject *kobj)
{
	struct iommu_group *group = to_iommu_group(kobj);

	pr_debug("Releasing group %d\n", group->id);

	if (group->iommu_data_release)
		group->iommu_data_release(group->iommu_data);

	ida_free(&iommu_group_ida, group->id);

	 
	WARN_ON(group->default_domain);
	WARN_ON(group->blocking_domain);

	kfree(group->name);
	kfree(group);
}

static const struct kobj_type iommu_group_ktype = {
	.sysfs_ops = &iommu_group_sysfs_ops,
	.release = iommu_group_release,
};

 
struct iommu_group *iommu_group_alloc(void)
{
	struct iommu_group *group;
	int ret;

	group = kzalloc(sizeof(*group), GFP_KERNEL);
	if (!group)
		return ERR_PTR(-ENOMEM);

	group->kobj.kset = iommu_group_kset;
	mutex_init(&group->mutex);
	INIT_LIST_HEAD(&group->devices);
	INIT_LIST_HEAD(&group->entry);
	xa_init(&group->pasid_array);

	ret = ida_alloc(&iommu_group_ida, GFP_KERNEL);
	if (ret < 0) {
		kfree(group);
		return ERR_PTR(ret);
	}
	group->id = ret;

	ret = kobject_init_and_add(&group->kobj, &iommu_group_ktype,
				   NULL, "%d", group->id);
	if (ret) {
		kobject_put(&group->kobj);
		return ERR_PTR(ret);
	}

	group->devices_kobj = kobject_create_and_add("devices", &group->kobj);
	if (!group->devices_kobj) {
		kobject_put(&group->kobj);  
		return ERR_PTR(-ENOMEM);
	}

	 
	kobject_put(&group->kobj);

	ret = iommu_group_create_file(group,
				      &iommu_group_attr_reserved_regions);
	if (ret) {
		kobject_put(group->devices_kobj);
		return ERR_PTR(ret);
	}

	ret = iommu_group_create_file(group, &iommu_group_attr_type);
	if (ret) {
		kobject_put(group->devices_kobj);
		return ERR_PTR(ret);
	}

	pr_debug("Allocated group %d\n", group->id);

	return group;
}
EXPORT_SYMBOL_GPL(iommu_group_alloc);

 
void *iommu_group_get_iommudata(struct iommu_group *group)
{
	return group->iommu_data;
}
EXPORT_SYMBOL_GPL(iommu_group_get_iommudata);

 
void iommu_group_set_iommudata(struct iommu_group *group, void *iommu_data,
			       void (*release)(void *iommu_data))
{
	group->iommu_data = iommu_data;
	group->iommu_data_release = release;
}
EXPORT_SYMBOL_GPL(iommu_group_set_iommudata);

 
int iommu_group_set_name(struct iommu_group *group, const char *name)
{
	int ret;

	if (group->name) {
		iommu_group_remove_file(group, &iommu_group_attr_name);
		kfree(group->name);
		group->name = NULL;
		if (!name)
			return 0;
	}

	group->name = kstrdup(name, GFP_KERNEL);
	if (!group->name)
		return -ENOMEM;

	ret = iommu_group_create_file(group, &iommu_group_attr_name);
	if (ret) {
		kfree(group->name);
		group->name = NULL;
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(iommu_group_set_name);

static int iommu_create_device_direct_mappings(struct iommu_domain *domain,
					       struct device *dev)
{
	struct iommu_resv_region *entry;
	struct list_head mappings;
	unsigned long pg_size;
	int ret = 0;

	pg_size = domain->pgsize_bitmap ? 1UL << __ffs(domain->pgsize_bitmap) : 0;
	INIT_LIST_HEAD(&mappings);

	if (WARN_ON_ONCE(iommu_is_dma_domain(domain) && !pg_size))
		return -EINVAL;

	iommu_get_resv_regions(dev, &mappings);

	 
	list_for_each_entry(entry, &mappings, list) {
		dma_addr_t start, end, addr;
		size_t map_size = 0;

		if (entry->type == IOMMU_RESV_DIRECT)
			dev->iommu->require_direct = 1;

		if ((entry->type != IOMMU_RESV_DIRECT &&
		     entry->type != IOMMU_RESV_DIRECT_RELAXABLE) ||
		    !iommu_is_dma_domain(domain))
			continue;

		start = ALIGN(entry->start, pg_size);
		end   = ALIGN(entry->start + entry->length, pg_size);

		for (addr = start; addr <= end; addr += pg_size) {
			phys_addr_t phys_addr;

			if (addr == end)
				goto map_end;

			phys_addr = iommu_iova_to_phys(domain, addr);
			if (!phys_addr) {
				map_size += pg_size;
				continue;
			}

map_end:
			if (map_size) {
				ret = iommu_map(domain, addr - map_size,
						addr - map_size, map_size,
						entry->prot, GFP_KERNEL);
				if (ret)
					goto out;
				map_size = 0;
			}
		}

	}

	if (!list_empty(&mappings) && iommu_is_dma_domain(domain))
		iommu_flush_iotlb_all(domain);

out:
	iommu_put_resv_regions(dev, &mappings);

	return ret;
}

 
static struct group_device *iommu_group_alloc_device(struct iommu_group *group,
						     struct device *dev)
{
	int ret, i = 0;
	struct group_device *device;

	device = kzalloc(sizeof(*device), GFP_KERNEL);
	if (!device)
		return ERR_PTR(-ENOMEM);

	device->dev = dev;

	ret = sysfs_create_link(&dev->kobj, &group->kobj, "iommu_group");
	if (ret)
		goto err_free_device;

	device->name = kasprintf(GFP_KERNEL, "%s", kobject_name(&dev->kobj));
rename:
	if (!device->name) {
		ret = -ENOMEM;
		goto err_remove_link;
	}

	ret = sysfs_create_link_nowarn(group->devices_kobj,
				       &dev->kobj, device->name);
	if (ret) {
		if (ret == -EEXIST && i >= 0) {
			 
			kfree(device->name);
			device->name = kasprintf(GFP_KERNEL, "%s.%d",
						 kobject_name(&dev->kobj), i++);
			goto rename;
		}
		goto err_free_name;
	}

	trace_add_device_to_group(group->id, dev);

	dev_info(dev, "Adding to iommu group %d\n", group->id);

	return device;

err_free_name:
	kfree(device->name);
err_remove_link:
	sysfs_remove_link(&dev->kobj, "iommu_group");
err_free_device:
	kfree(device);
	dev_err(dev, "Failed to add to iommu group %d: %d\n", group->id, ret);
	return ERR_PTR(ret);
}

 
int iommu_group_add_device(struct iommu_group *group, struct device *dev)
{
	struct group_device *gdev;

	gdev = iommu_group_alloc_device(group, dev);
	if (IS_ERR(gdev))
		return PTR_ERR(gdev);

	iommu_group_ref_get(group);
	dev->iommu_group = group;

	mutex_lock(&group->mutex);
	list_add_tail(&gdev->list, &group->devices);
	mutex_unlock(&group->mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(iommu_group_add_device);

 
void iommu_group_remove_device(struct device *dev)
{
	struct iommu_group *group = dev->iommu_group;

	if (!group)
		return;

	dev_info(dev, "Removing from iommu group %d\n", group->id);

	__iommu_group_remove_device(dev);
}
EXPORT_SYMBOL_GPL(iommu_group_remove_device);

 
int iommu_group_for_each_dev(struct iommu_group *group, void *data,
			     int (*fn)(struct device *, void *))
{
	struct group_device *device;
	int ret = 0;

	mutex_lock(&group->mutex);
	for_each_group_device(group, device) {
		ret = fn(device->dev, data);
		if (ret)
			break;
	}
	mutex_unlock(&group->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(iommu_group_for_each_dev);

 
struct iommu_group *iommu_group_get(struct device *dev)
{
	struct iommu_group *group = dev->iommu_group;

	if (group)
		kobject_get(group->devices_kobj);

	return group;
}
EXPORT_SYMBOL_GPL(iommu_group_get);

 
struct iommu_group *iommu_group_ref_get(struct iommu_group *group)
{
	kobject_get(group->devices_kobj);
	return group;
}
EXPORT_SYMBOL_GPL(iommu_group_ref_get);

 
void iommu_group_put(struct iommu_group *group)
{
	if (group)
		kobject_put(group->devices_kobj);
}
EXPORT_SYMBOL_GPL(iommu_group_put);

 
int iommu_register_device_fault_handler(struct device *dev,
					iommu_dev_fault_handler_t handler,
					void *data)
{
	struct dev_iommu *param = dev->iommu;
	int ret = 0;

	if (!param)
		return -EINVAL;

	mutex_lock(&param->lock);
	 
	if (param->fault_param) {
		ret = -EBUSY;
		goto done_unlock;
	}

	get_device(dev);
	param->fault_param = kzalloc(sizeof(*param->fault_param), GFP_KERNEL);
	if (!param->fault_param) {
		put_device(dev);
		ret = -ENOMEM;
		goto done_unlock;
	}
	param->fault_param->handler = handler;
	param->fault_param->data = data;
	mutex_init(&param->fault_param->lock);
	INIT_LIST_HEAD(&param->fault_param->faults);

done_unlock:
	mutex_unlock(&param->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(iommu_register_device_fault_handler);

 
int iommu_unregister_device_fault_handler(struct device *dev)
{
	struct dev_iommu *param = dev->iommu;
	int ret = 0;

	if (!param)
		return -EINVAL;

	mutex_lock(&param->lock);

	if (!param->fault_param)
		goto unlock;

	 
	if (!list_empty(&param->fault_param->faults)) {
		ret = -EBUSY;
		goto unlock;
	}

	kfree(param->fault_param);
	param->fault_param = NULL;
	put_device(dev);
unlock:
	mutex_unlock(&param->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(iommu_unregister_device_fault_handler);

 
int iommu_report_device_fault(struct device *dev, struct iommu_fault_event *evt)
{
	struct dev_iommu *param = dev->iommu;
	struct iommu_fault_event *evt_pending = NULL;
	struct iommu_fault_param *fparam;
	int ret = 0;

	if (!param || !evt)
		return -EINVAL;

	 
	mutex_lock(&param->lock);
	fparam = param->fault_param;
	if (!fparam || !fparam->handler) {
		ret = -EINVAL;
		goto done_unlock;
	}

	if (evt->fault.type == IOMMU_FAULT_PAGE_REQ &&
	    (evt->fault.prm.flags & IOMMU_FAULT_PAGE_REQUEST_LAST_PAGE)) {
		evt_pending = kmemdup(evt, sizeof(struct iommu_fault_event),
				      GFP_KERNEL);
		if (!evt_pending) {
			ret = -ENOMEM;
			goto done_unlock;
		}
		mutex_lock(&fparam->lock);
		list_add_tail(&evt_pending->list, &fparam->faults);
		mutex_unlock(&fparam->lock);
	}

	ret = fparam->handler(&evt->fault, fparam->data);
	if (ret && evt_pending) {
		mutex_lock(&fparam->lock);
		list_del(&evt_pending->list);
		mutex_unlock(&fparam->lock);
		kfree(evt_pending);
	}
done_unlock:
	mutex_unlock(&param->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(iommu_report_device_fault);

int iommu_page_response(struct device *dev,
			struct iommu_page_response *msg)
{
	bool needs_pasid;
	int ret = -EINVAL;
	struct iommu_fault_event *evt;
	struct iommu_fault_page_request *prm;
	struct dev_iommu *param = dev->iommu;
	const struct iommu_ops *ops = dev_iommu_ops(dev);
	bool has_pasid = msg->flags & IOMMU_PAGE_RESP_PASID_VALID;

	if (!ops->page_response)
		return -ENODEV;

	if (!param || !param->fault_param)
		return -EINVAL;

	if (msg->version != IOMMU_PAGE_RESP_VERSION_1 ||
	    msg->flags & ~IOMMU_PAGE_RESP_PASID_VALID)
		return -EINVAL;

	 
	mutex_lock(&param->fault_param->lock);
	if (list_empty(&param->fault_param->faults)) {
		dev_warn_ratelimited(dev, "no pending PRQ, drop response\n");
		goto done_unlock;
	}
	 
	list_for_each_entry(evt, &param->fault_param->faults, list) {
		prm = &evt->fault.prm;
		if (prm->grpid != msg->grpid)
			continue;

		 
		needs_pasid = prm->flags & IOMMU_FAULT_PAGE_RESPONSE_NEEDS_PASID;
		if (needs_pasid && (!has_pasid || msg->pasid != prm->pasid))
			continue;

		if (!needs_pasid && has_pasid) {
			 
			msg->flags &= ~IOMMU_PAGE_RESP_PASID_VALID;
			msg->pasid = 0;
		}

		ret = ops->page_response(dev, evt, msg);
		list_del(&evt->list);
		kfree(evt);
		break;
	}

done_unlock:
	mutex_unlock(&param->fault_param->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(iommu_page_response);

 
int iommu_group_id(struct iommu_group *group)
{
	return group->id;
}
EXPORT_SYMBOL_GPL(iommu_group_id);

static struct iommu_group *get_pci_alias_group(struct pci_dev *pdev,
					       unsigned long *devfns);

 
#define REQ_ACS_FLAGS   (PCI_ACS_SV | PCI_ACS_RR | PCI_ACS_CR | PCI_ACS_UF)

 
static struct iommu_group *get_pci_function_alias_group(struct pci_dev *pdev,
							unsigned long *devfns)
{
	struct pci_dev *tmp = NULL;
	struct iommu_group *group;

	if (!pdev->multifunction || pci_acs_enabled(pdev, REQ_ACS_FLAGS))
		return NULL;

	for_each_pci_dev(tmp) {
		if (tmp == pdev || tmp->bus != pdev->bus ||
		    PCI_SLOT(tmp->devfn) != PCI_SLOT(pdev->devfn) ||
		    pci_acs_enabled(tmp, REQ_ACS_FLAGS))
			continue;

		group = get_pci_alias_group(tmp, devfns);
		if (group) {
			pci_dev_put(tmp);
			return group;
		}
	}

	return NULL;
}

 
static struct iommu_group *get_pci_alias_group(struct pci_dev *pdev,
					       unsigned long *devfns)
{
	struct pci_dev *tmp = NULL;
	struct iommu_group *group;

	if (test_and_set_bit(pdev->devfn & 0xff, devfns))
		return NULL;

	group = iommu_group_get(&pdev->dev);
	if (group)
		return group;

	for_each_pci_dev(tmp) {
		if (tmp == pdev || tmp->bus != pdev->bus)
			continue;

		 
		if (pci_devs_are_dma_aliases(pdev, tmp)) {
			group = get_pci_alias_group(tmp, devfns);
			if (group) {
				pci_dev_put(tmp);
				return group;
			}

			group = get_pci_function_alias_group(tmp, devfns);
			if (group) {
				pci_dev_put(tmp);
				return group;
			}
		}
	}

	return NULL;
}

struct group_for_pci_data {
	struct pci_dev *pdev;
	struct iommu_group *group;
};

 
static int get_pci_alias_or_group(struct pci_dev *pdev, u16 alias, void *opaque)
{
	struct group_for_pci_data *data = opaque;

	data->pdev = pdev;
	data->group = iommu_group_get(&pdev->dev);

	return data->group != NULL;
}

 
struct iommu_group *generic_device_group(struct device *dev)
{
	return iommu_group_alloc();
}
EXPORT_SYMBOL_GPL(generic_device_group);

 
struct iommu_group *pci_device_group(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct group_for_pci_data data;
	struct pci_bus *bus;
	struct iommu_group *group = NULL;
	u64 devfns[4] = { 0 };

	if (WARN_ON(!dev_is_pci(dev)))
		return ERR_PTR(-EINVAL);

	 
	if (pci_for_each_dma_alias(pdev, get_pci_alias_or_group, &data))
		return data.group;

	pdev = data.pdev;

	 
	for (bus = pdev->bus; !pci_is_root_bus(bus); bus = bus->parent) {
		if (!bus->self)
			continue;

		if (pci_acs_path_enabled(bus->self, NULL, REQ_ACS_FLAGS))
			break;

		pdev = bus->self;

		group = iommu_group_get(&pdev->dev);
		if (group)
			return group;
	}

	 
	group = get_pci_alias_group(pdev, (unsigned long *)devfns);
	if (group)
		return group;

	 
	group = get_pci_function_alias_group(pdev, (unsigned long *)devfns);
	if (group)
		return group;

	 
	return iommu_group_alloc();
}
EXPORT_SYMBOL_GPL(pci_device_group);

 
struct iommu_group *fsl_mc_device_group(struct device *dev)
{
	struct device *cont_dev = fsl_mc_cont_dev(dev);
	struct iommu_group *group;

	group = iommu_group_get(cont_dev);
	if (!group)
		group = iommu_group_alloc();
	return group;
}
EXPORT_SYMBOL_GPL(fsl_mc_device_group);

static int iommu_get_def_domain_type(struct device *dev)
{
	const struct iommu_ops *ops = dev_iommu_ops(dev);

	if (dev_is_pci(dev) && to_pci_dev(dev)->untrusted)
		return IOMMU_DOMAIN_DMA;

	if (ops->def_domain_type)
		return ops->def_domain_type(dev);

	return 0;
}

static struct iommu_domain *
__iommu_group_alloc_default_domain(const struct bus_type *bus,
				   struct iommu_group *group, int req_type)
{
	if (group->default_domain && group->default_domain->type == req_type)
		return group->default_domain;
	return __iommu_domain_alloc(bus, req_type);
}

 
static struct iommu_domain *
iommu_group_alloc_default_domain(struct iommu_group *group, int req_type)
{
	const struct bus_type *bus =
		list_first_entry(&group->devices, struct group_device, list)
			->dev->bus;
	struct iommu_domain *dom;

	lockdep_assert_held(&group->mutex);

	if (req_type)
		return __iommu_group_alloc_default_domain(bus, group, req_type);

	 
	dom = __iommu_group_alloc_default_domain(bus, group, iommu_def_domain_type);
	if (dom)
		return dom;

	 
	if (iommu_def_domain_type == IOMMU_DOMAIN_DMA)
		return NULL;
	dom = __iommu_group_alloc_default_domain(bus, group, IOMMU_DOMAIN_DMA);
	if (!dom)
		return NULL;

	pr_warn("Failed to allocate default IOMMU domain of type %u for group %s - Falling back to IOMMU_DOMAIN_DMA",
		iommu_def_domain_type, group->name);
	return dom;
}

struct iommu_domain *iommu_group_default_domain(struct iommu_group *group)
{
	return group->default_domain;
}

static int probe_iommu_group(struct device *dev, void *data)
{
	struct list_head *group_list = data;
	int ret;

	mutex_lock(&iommu_probe_device_lock);
	ret = __iommu_probe_device(dev, group_list);
	mutex_unlock(&iommu_probe_device_lock);
	if (ret == -ENODEV)
		ret = 0;

	return ret;
}

static int iommu_bus_notifier(struct notifier_block *nb,
			      unsigned long action, void *data)
{
	struct device *dev = data;

	if (action == BUS_NOTIFY_ADD_DEVICE) {
		int ret;

		ret = iommu_probe_device(dev);
		return (ret) ? NOTIFY_DONE : NOTIFY_OK;
	} else if (action == BUS_NOTIFY_REMOVED_DEVICE) {
		iommu_release_device(dev);
		return NOTIFY_OK;
	}

	return 0;
}

 
static int iommu_get_default_domain_type(struct iommu_group *group,
					 int target_type)
{
	int best_type = target_type;
	struct group_device *gdev;
	struct device *last_dev;

	lockdep_assert_held(&group->mutex);

	for_each_group_device(group, gdev) {
		unsigned int type = iommu_get_def_domain_type(gdev->dev);

		if (best_type && type && best_type != type) {
			if (target_type) {
				dev_err_ratelimited(
					gdev->dev,
					"Device cannot be in %s domain\n",
					iommu_domain_type_str(target_type));
				return -1;
			}

			dev_warn(
				gdev->dev,
				"Device needs domain type %s, but device %s in the same iommu group requires type %s - using default\n",
				iommu_domain_type_str(type), dev_name(last_dev),
				iommu_domain_type_str(best_type));
			return 0;
		}
		if (!best_type)
			best_type = type;
		last_dev = gdev->dev;
	}
	return best_type;
}

static void iommu_group_do_probe_finalize(struct device *dev)
{
	const struct iommu_ops *ops = dev_iommu_ops(dev);

	if (ops->probe_finalize)
		ops->probe_finalize(dev);
}

int bus_iommu_probe(const struct bus_type *bus)
{
	struct iommu_group *group, *next;
	LIST_HEAD(group_list);
	int ret;

	ret = bus_for_each_dev(bus, NULL, &group_list, probe_iommu_group);
	if (ret)
		return ret;

	list_for_each_entry_safe(group, next, &group_list, entry) {
		struct group_device *gdev;

		mutex_lock(&group->mutex);

		 
		list_del_init(&group->entry);

		 
		ret = iommu_setup_default_domain(group, 0);
		if (ret) {
			mutex_unlock(&group->mutex);
			return ret;
		}
		mutex_unlock(&group->mutex);

		 
		for_each_group_device(group, gdev)
			iommu_group_do_probe_finalize(gdev->dev);
	}

	return 0;
}

bool iommu_present(const struct bus_type *bus)
{
	return bus->iommu_ops != NULL;
}
EXPORT_SYMBOL_GPL(iommu_present);

 
bool device_iommu_capable(struct device *dev, enum iommu_cap cap)
{
	const struct iommu_ops *ops;

	if (!dev->iommu || !dev->iommu->iommu_dev)
		return false;

	ops = dev_iommu_ops(dev);
	if (!ops->capable)
		return false;

	return ops->capable(dev, cap);
}
EXPORT_SYMBOL_GPL(device_iommu_capable);

 
bool iommu_group_has_isolated_msi(struct iommu_group *group)
{
	struct group_device *group_dev;
	bool ret = true;

	mutex_lock(&group->mutex);
	for_each_group_device(group, group_dev)
		ret &= msi_device_has_isolated_msi(group_dev->dev);
	mutex_unlock(&group->mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(iommu_group_has_isolated_msi);

 
void iommu_set_fault_handler(struct iommu_domain *domain,
					iommu_fault_handler_t handler,
					void *token)
{
	BUG_ON(!domain);

	domain->handler = handler;
	domain->handler_token = token;
}
EXPORT_SYMBOL_GPL(iommu_set_fault_handler);

static struct iommu_domain *__iommu_domain_alloc(const struct bus_type *bus,
						 unsigned type)
{
	struct iommu_domain *domain;
	unsigned int alloc_type = type & IOMMU_DOMAIN_ALLOC_FLAGS;

	if (bus == NULL || bus->iommu_ops == NULL)
		return NULL;

	domain = bus->iommu_ops->domain_alloc(alloc_type);
	if (!domain)
		return NULL;

	domain->type = type;
	 
	if (!domain->pgsize_bitmap)
		domain->pgsize_bitmap = bus->iommu_ops->pgsize_bitmap;

	if (!domain->ops)
		domain->ops = bus->iommu_ops->default_domain_ops;

	if (iommu_is_dma_domain(domain) && iommu_get_dma_cookie(domain)) {
		iommu_domain_free(domain);
		domain = NULL;
	}
	return domain;
}

struct iommu_domain *iommu_domain_alloc(const struct bus_type *bus)
{
	return __iommu_domain_alloc(bus, IOMMU_DOMAIN_UNMANAGED);
}
EXPORT_SYMBOL_GPL(iommu_domain_alloc);

void iommu_domain_free(struct iommu_domain *domain)
{
	if (domain->type == IOMMU_DOMAIN_SVA)
		mmdrop(domain->mm);
	iommu_put_dma_cookie(domain);
	domain->ops->free(domain);
}
EXPORT_SYMBOL_GPL(iommu_domain_free);

 
static void __iommu_group_set_core_domain(struct iommu_group *group)
{
	struct iommu_domain *new_domain;

	if (group->owner)
		new_domain = group->blocking_domain;
	else
		new_domain = group->default_domain;

	__iommu_group_set_domain_nofail(group, new_domain);
}

static int __iommu_attach_device(struct iommu_domain *domain,
				 struct device *dev)
{
	int ret;

	if (unlikely(domain->ops->attach_dev == NULL))
		return -ENODEV;

	ret = domain->ops->attach_dev(domain, dev);
	if (ret)
		return ret;
	dev->iommu->attach_deferred = 0;
	trace_attach_device_to_domain(dev);
	return 0;
}

 
int iommu_attach_device(struct iommu_domain *domain, struct device *dev)
{
	struct iommu_group *group;
	int ret;

	group = iommu_group_get(dev);
	if (!group)
		return -ENODEV;

	 
	mutex_lock(&group->mutex);
	ret = -EINVAL;
	if (list_count_nodes(&group->devices) != 1)
		goto out_unlock;

	ret = __iommu_attach_group(domain, group);

out_unlock:
	mutex_unlock(&group->mutex);
	iommu_group_put(group);

	return ret;
}
EXPORT_SYMBOL_GPL(iommu_attach_device);

int iommu_deferred_attach(struct device *dev, struct iommu_domain *domain)
{
	if (dev->iommu && dev->iommu->attach_deferred)
		return __iommu_attach_device(domain, dev);

	return 0;
}

void iommu_detach_device(struct iommu_domain *domain, struct device *dev)
{
	struct iommu_group *group;

	group = iommu_group_get(dev);
	if (!group)
		return;

	mutex_lock(&group->mutex);
	if (WARN_ON(domain != group->domain) ||
	    WARN_ON(list_count_nodes(&group->devices) != 1))
		goto out_unlock;
	__iommu_group_set_core_domain(group);

out_unlock:
	mutex_unlock(&group->mutex);
	iommu_group_put(group);
}
EXPORT_SYMBOL_GPL(iommu_detach_device);

struct iommu_domain *iommu_get_domain_for_dev(struct device *dev)
{
	struct iommu_domain *domain;
	struct iommu_group *group;

	group = iommu_group_get(dev);
	if (!group)
		return NULL;

	domain = group->domain;

	iommu_group_put(group);

	return domain;
}
EXPORT_SYMBOL_GPL(iommu_get_domain_for_dev);

 
struct iommu_domain *iommu_get_dma_domain(struct device *dev)
{
	return dev->iommu_group->default_domain;
}

static int __iommu_attach_group(struct iommu_domain *domain,
				struct iommu_group *group)
{
	if (group->domain && group->domain != group->default_domain &&
	    group->domain != group->blocking_domain)
		return -EBUSY;

	return __iommu_group_set_domain(group, domain);
}

 
int iommu_attach_group(struct iommu_domain *domain, struct iommu_group *group)
{
	int ret;

	mutex_lock(&group->mutex);
	ret = __iommu_attach_group(domain, group);
	mutex_unlock(&group->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(iommu_attach_group);

 
int iommu_group_replace_domain(struct iommu_group *group,
			       struct iommu_domain *new_domain)
{
	int ret;

	if (!new_domain)
		return -EINVAL;

	mutex_lock(&group->mutex);
	ret = __iommu_group_set_domain(group, new_domain);
	mutex_unlock(&group->mutex);
	return ret;
}
EXPORT_SYMBOL_NS_GPL(iommu_group_replace_domain, IOMMUFD_INTERNAL);

static int __iommu_device_set_domain(struct iommu_group *group,
				     struct device *dev,
				     struct iommu_domain *new_domain,
				     unsigned int flags)
{
	int ret;

	 
	if (dev->iommu->require_direct &&
	    (new_domain->type == IOMMU_DOMAIN_BLOCKED ||
	     new_domain == group->blocking_domain)) {
		dev_warn(dev,
			 "Firmware has requested this device have a 1:1 IOMMU mapping, rejecting configuring the device without a 1:1 mapping. Contact your platform vendor.\n");
		return -EINVAL;
	}

	if (dev->iommu->attach_deferred) {
		if (new_domain == group->default_domain)
			return 0;
		dev->iommu->attach_deferred = 0;
	}

	ret = __iommu_attach_device(new_domain, dev);
	if (ret) {
		 
		if ((flags & IOMMU_SET_DOMAIN_MUST_SUCCEED) &&
		    group->blocking_domain &&
		    group->blocking_domain != new_domain)
			__iommu_attach_device(group->blocking_domain, dev);
		return ret;
	}
	return 0;
}

 
static int __iommu_group_set_domain_internal(struct iommu_group *group,
					     struct iommu_domain *new_domain,
					     unsigned int flags)
{
	struct group_device *last_gdev;
	struct group_device *gdev;
	int result;
	int ret;

	lockdep_assert_held(&group->mutex);

	if (group->domain == new_domain)
		return 0;

	 
	if (!new_domain) {
		for_each_group_device(group, gdev) {
			const struct iommu_ops *ops = dev_iommu_ops(gdev->dev);

			if (!WARN_ON(!ops->set_platform_dma_ops))
				ops->set_platform_dma_ops(gdev->dev);
		}
		group->domain = NULL;
		return 0;
	}

	 
	result = 0;
	for_each_group_device(group, gdev) {
		ret = __iommu_device_set_domain(group, gdev->dev, new_domain,
						flags);
		if (ret) {
			result = ret;
			 
			if (flags & IOMMU_SET_DOMAIN_MUST_SUCCEED)
				continue;
			goto err_revert;
		}
	}
	group->domain = new_domain;
	return result;

err_revert:
	 
	last_gdev = gdev;
	for_each_group_device(group, gdev) {
		const struct iommu_ops *ops = dev_iommu_ops(gdev->dev);

		 
		if (group->domain)
			WARN_ON(__iommu_device_set_domain(
				group, gdev->dev, group->domain,
				IOMMU_SET_DOMAIN_MUST_SUCCEED));
		else if (ops->set_platform_dma_ops)
			ops->set_platform_dma_ops(gdev->dev);
		if (gdev == last_gdev)
			break;
	}
	return ret;
}

void iommu_detach_group(struct iommu_domain *domain, struct iommu_group *group)
{
	mutex_lock(&group->mutex);
	__iommu_group_set_core_domain(group);
	mutex_unlock(&group->mutex);
}
EXPORT_SYMBOL_GPL(iommu_detach_group);

phys_addr_t iommu_iova_to_phys(struct iommu_domain *domain, dma_addr_t iova)
{
	if (domain->type == IOMMU_DOMAIN_IDENTITY)
		return iova;

	if (domain->type == IOMMU_DOMAIN_BLOCKED)
		return 0;

	return domain->ops->iova_to_phys(domain, iova);
}
EXPORT_SYMBOL_GPL(iommu_iova_to_phys);

static size_t iommu_pgsize(struct iommu_domain *domain, unsigned long iova,
			   phys_addr_t paddr, size_t size, size_t *count)
{
	unsigned int pgsize_idx, pgsize_idx_next;
	unsigned long pgsizes;
	size_t offset, pgsize, pgsize_next;
	unsigned long addr_merge = paddr | iova;

	 
	pgsizes = domain->pgsize_bitmap & GENMASK(__fls(size), 0);

	 
	if (likely(addr_merge))
		pgsizes &= GENMASK(__ffs(addr_merge), 0);

	 
	BUG_ON(!pgsizes);

	 
	pgsize_idx = __fls(pgsizes);
	pgsize = BIT(pgsize_idx);
	if (!count)
		return pgsize;

	 
	pgsizes = domain->pgsize_bitmap & ~GENMASK(pgsize_idx, 0);
	if (!pgsizes)
		goto out_set_count;

	pgsize_idx_next = __ffs(pgsizes);
	pgsize_next = BIT(pgsize_idx_next);

	 
	if ((iova ^ paddr) & (pgsize_next - 1))
		goto out_set_count;

	 
	offset = pgsize_next - (addr_merge & (pgsize_next - 1));

	 
	if (offset + pgsize_next <= size)
		size = offset;

out_set_count:
	*count = size >> pgsize_idx;
	return pgsize;
}

static int __iommu_map_pages(struct iommu_domain *domain, unsigned long iova,
			     phys_addr_t paddr, size_t size, int prot,
			     gfp_t gfp, size_t *mapped)
{
	const struct iommu_domain_ops *ops = domain->ops;
	size_t pgsize, count;
	int ret;

	pgsize = iommu_pgsize(domain, iova, paddr, size, &count);

	pr_debug("mapping: iova 0x%lx pa %pa pgsize 0x%zx count %zu\n",
		 iova, &paddr, pgsize, count);

	if (ops->map_pages) {
		ret = ops->map_pages(domain, iova, paddr, pgsize, count, prot,
				     gfp, mapped);
	} else {
		ret = ops->map(domain, iova, paddr, pgsize, prot, gfp);
		*mapped = ret ? 0 : pgsize;
	}

	return ret;
}

static int __iommu_map(struct iommu_domain *domain, unsigned long iova,
		       phys_addr_t paddr, size_t size, int prot, gfp_t gfp)
{
	const struct iommu_domain_ops *ops = domain->ops;
	unsigned long orig_iova = iova;
	unsigned int min_pagesz;
	size_t orig_size = size;
	phys_addr_t orig_paddr = paddr;
	int ret = 0;

	if (unlikely(!(ops->map || ops->map_pages) ||
		     domain->pgsize_bitmap == 0UL))
		return -ENODEV;

	if (unlikely(!(domain->type & __IOMMU_DOMAIN_PAGING)))
		return -EINVAL;

	 
	min_pagesz = 1 << __ffs(domain->pgsize_bitmap);

	 
	if (!IS_ALIGNED(iova | paddr | size, min_pagesz)) {
		pr_err("unaligned: iova 0x%lx pa %pa size 0x%zx min_pagesz 0x%x\n",
		       iova, &paddr, size, min_pagesz);
		return -EINVAL;
	}

	pr_debug("map: iova 0x%lx pa %pa size 0x%zx\n", iova, &paddr, size);

	while (size) {
		size_t mapped = 0;

		ret = __iommu_map_pages(domain, iova, paddr, size, prot, gfp,
					&mapped);
		 
		size -= mapped;

		if (ret)
			break;

		iova += mapped;
		paddr += mapped;
	}

	 
	if (ret)
		iommu_unmap(domain, orig_iova, orig_size - size);
	else
		trace_map(orig_iova, orig_paddr, orig_size);

	return ret;
}

int iommu_map(struct iommu_domain *domain, unsigned long iova,
	      phys_addr_t paddr, size_t size, int prot, gfp_t gfp)
{
	const struct iommu_domain_ops *ops = domain->ops;
	int ret;

	might_sleep_if(gfpflags_allow_blocking(gfp));

	 
	if (WARN_ON_ONCE(gfp & (__GFP_COMP | __GFP_DMA | __GFP_DMA32 |
				__GFP_HIGHMEM)))
		return -EINVAL;

	ret = __iommu_map(domain, iova, paddr, size, prot, gfp);
	if (ret == 0 && ops->iotlb_sync_map)
		ops->iotlb_sync_map(domain, iova, size);

	return ret;
}
EXPORT_SYMBOL_GPL(iommu_map);

static size_t __iommu_unmap_pages(struct iommu_domain *domain,
				  unsigned long iova, size_t size,
				  struct iommu_iotlb_gather *iotlb_gather)
{
	const struct iommu_domain_ops *ops = domain->ops;
	size_t pgsize, count;

	pgsize = iommu_pgsize(domain, iova, iova, size, &count);
	return ops->unmap_pages ?
	       ops->unmap_pages(domain, iova, pgsize, count, iotlb_gather) :
	       ops->unmap(domain, iova, pgsize, iotlb_gather);
}

static size_t __iommu_unmap(struct iommu_domain *domain,
			    unsigned long iova, size_t size,
			    struct iommu_iotlb_gather *iotlb_gather)
{
	const struct iommu_domain_ops *ops = domain->ops;
	size_t unmapped_page, unmapped = 0;
	unsigned long orig_iova = iova;
	unsigned int min_pagesz;

	if (unlikely(!(ops->unmap || ops->unmap_pages) ||
		     domain->pgsize_bitmap == 0UL))
		return 0;

	if (unlikely(!(domain->type & __IOMMU_DOMAIN_PAGING)))
		return 0;

	 
	min_pagesz = 1 << __ffs(domain->pgsize_bitmap);

	 
	if (!IS_ALIGNED(iova | size, min_pagesz)) {
		pr_err("unaligned: iova 0x%lx size 0x%zx min_pagesz 0x%x\n",
		       iova, size, min_pagesz);
		return 0;
	}

	pr_debug("unmap this: iova 0x%lx size 0x%zx\n", iova, size);

	 
	while (unmapped < size) {
		unmapped_page = __iommu_unmap_pages(domain, iova,
						    size - unmapped,
						    iotlb_gather);
		if (!unmapped_page)
			break;

		pr_debug("unmapped: iova 0x%lx size 0x%zx\n",
			 iova, unmapped_page);

		iova += unmapped_page;
		unmapped += unmapped_page;
	}

	trace_unmap(orig_iova, size, unmapped);
	return unmapped;
}

size_t iommu_unmap(struct iommu_domain *domain,
		   unsigned long iova, size_t size)
{
	struct iommu_iotlb_gather iotlb_gather;
	size_t ret;

	iommu_iotlb_gather_init(&iotlb_gather);
	ret = __iommu_unmap(domain, iova, size, &iotlb_gather);
	iommu_iotlb_sync(domain, &iotlb_gather);

	return ret;
}
EXPORT_SYMBOL_GPL(iommu_unmap);

size_t iommu_unmap_fast(struct iommu_domain *domain,
			unsigned long iova, size_t size,
			struct iommu_iotlb_gather *iotlb_gather)
{
	return __iommu_unmap(domain, iova, size, iotlb_gather);
}
EXPORT_SYMBOL_GPL(iommu_unmap_fast);

ssize_t iommu_map_sg(struct iommu_domain *domain, unsigned long iova,
		     struct scatterlist *sg, unsigned int nents, int prot,
		     gfp_t gfp)
{
	const struct iommu_domain_ops *ops = domain->ops;
	size_t len = 0, mapped = 0;
	phys_addr_t start;
	unsigned int i = 0;
	int ret;

	might_sleep_if(gfpflags_allow_blocking(gfp));

	 
	if (WARN_ON_ONCE(gfp & (__GFP_COMP | __GFP_DMA | __GFP_DMA32 |
				__GFP_HIGHMEM)))
		return -EINVAL;

	while (i <= nents) {
		phys_addr_t s_phys = sg_phys(sg);

		if (len && s_phys != start + len) {
			ret = __iommu_map(domain, iova + mapped, start,
					len, prot, gfp);

			if (ret)
				goto out_err;

			mapped += len;
			len = 0;
		}

		if (sg_dma_is_bus_address(sg))
			goto next;

		if (len) {
			len += sg->length;
		} else {
			len = sg->length;
			start = s_phys;
		}

next:
		if (++i < nents)
			sg = sg_next(sg);
	}

	if (ops->iotlb_sync_map)
		ops->iotlb_sync_map(domain, iova, mapped);
	return mapped;

out_err:
	 
	iommu_unmap(domain, iova, mapped);

	return ret;
}
EXPORT_SYMBOL_GPL(iommu_map_sg);

 
int report_iommu_fault(struct iommu_domain *domain, struct device *dev,
		       unsigned long iova, int flags)
{
	int ret = -ENOSYS;

	 
	if (domain->handler)
		ret = domain->handler(domain, dev, iova, flags,
						domain->handler_token);

	trace_io_page_fault(dev, iova, flags);
	return ret;
}
EXPORT_SYMBOL_GPL(report_iommu_fault);

static int __init iommu_init(void)
{
	iommu_group_kset = kset_create_and_add("iommu_groups",
					       NULL, kernel_kobj);
	BUG_ON(!iommu_group_kset);

	iommu_debugfs_setup();

	return 0;
}
core_initcall(iommu_init);

int iommu_enable_nesting(struct iommu_domain *domain)
{
	if (domain->type != IOMMU_DOMAIN_UNMANAGED)
		return -EINVAL;
	if (!domain->ops->enable_nesting)
		return -EINVAL;
	return domain->ops->enable_nesting(domain);
}
EXPORT_SYMBOL_GPL(iommu_enable_nesting);

int iommu_set_pgtable_quirks(struct iommu_domain *domain,
		unsigned long quirk)
{
	if (domain->type != IOMMU_DOMAIN_UNMANAGED)
		return -EINVAL;
	if (!domain->ops->set_pgtable_quirks)
		return -EINVAL;
	return domain->ops->set_pgtable_quirks(domain, quirk);
}
EXPORT_SYMBOL_GPL(iommu_set_pgtable_quirks);

 
void iommu_get_resv_regions(struct device *dev, struct list_head *list)
{
	const struct iommu_ops *ops = dev_iommu_ops(dev);

	if (ops->get_resv_regions)
		ops->get_resv_regions(dev, list);
}
EXPORT_SYMBOL_GPL(iommu_get_resv_regions);

 
void iommu_put_resv_regions(struct device *dev, struct list_head *list)
{
	struct iommu_resv_region *entry, *next;

	list_for_each_entry_safe(entry, next, list, list) {
		if (entry->free)
			entry->free(dev, entry);
		else
			kfree(entry);
	}
}
EXPORT_SYMBOL(iommu_put_resv_regions);

struct iommu_resv_region *iommu_alloc_resv_region(phys_addr_t start,
						  size_t length, int prot,
						  enum iommu_resv_type type,
						  gfp_t gfp)
{
	struct iommu_resv_region *region;

	region = kzalloc(sizeof(*region), gfp);
	if (!region)
		return NULL;

	INIT_LIST_HEAD(&region->list);
	region->start = start;
	region->length = length;
	region->prot = prot;
	region->type = type;
	return region;
}
EXPORT_SYMBOL_GPL(iommu_alloc_resv_region);

void iommu_set_default_passthrough(bool cmd_line)
{
	if (cmd_line)
		iommu_cmd_line |= IOMMU_CMD_LINE_DMA_API;
	iommu_def_domain_type = IOMMU_DOMAIN_IDENTITY;
}

void iommu_set_default_translated(bool cmd_line)
{
	if (cmd_line)
		iommu_cmd_line |= IOMMU_CMD_LINE_DMA_API;
	iommu_def_domain_type = IOMMU_DOMAIN_DMA;
}

bool iommu_default_passthrough(void)
{
	return iommu_def_domain_type == IOMMU_DOMAIN_IDENTITY;
}
EXPORT_SYMBOL_GPL(iommu_default_passthrough);

const struct iommu_ops *iommu_ops_from_fwnode(struct fwnode_handle *fwnode)
{
	const struct iommu_ops *ops = NULL;
	struct iommu_device *iommu;

	spin_lock(&iommu_device_lock);
	list_for_each_entry(iommu, &iommu_device_list, list)
		if (iommu->fwnode == fwnode) {
			ops = iommu->ops;
			break;
		}
	spin_unlock(&iommu_device_lock);
	return ops;
}

int iommu_fwspec_init(struct device *dev, struct fwnode_handle *iommu_fwnode,
		      const struct iommu_ops *ops)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);

	if (fwspec)
		return ops == fwspec->ops ? 0 : -EINVAL;

	if (!dev_iommu_get(dev))
		return -ENOMEM;

	 
	fwspec = kzalloc(struct_size(fwspec, ids, 1), GFP_KERNEL);
	if (!fwspec)
		return -ENOMEM;

	of_node_get(to_of_node(iommu_fwnode));
	fwspec->iommu_fwnode = iommu_fwnode;
	fwspec->ops = ops;
	dev_iommu_fwspec_set(dev, fwspec);
	return 0;
}
EXPORT_SYMBOL_GPL(iommu_fwspec_init);

void iommu_fwspec_free(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);

	if (fwspec) {
		fwnode_handle_put(fwspec->iommu_fwnode);
		kfree(fwspec);
		dev_iommu_fwspec_set(dev, NULL);
	}
}
EXPORT_SYMBOL_GPL(iommu_fwspec_free);

int iommu_fwspec_add_ids(struct device *dev, u32 *ids, int num_ids)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	int i, new_num;

	if (!fwspec)
		return -EINVAL;

	new_num = fwspec->num_ids + num_ids;
	if (new_num > 1) {
		fwspec = krealloc(fwspec, struct_size(fwspec, ids, new_num),
				  GFP_KERNEL);
		if (!fwspec)
			return -ENOMEM;

		dev_iommu_fwspec_set(dev, fwspec);
	}

	for (i = 0; i < num_ids; i++)
		fwspec->ids[fwspec->num_ids + i] = ids[i];

	fwspec->num_ids = new_num;
	return 0;
}
EXPORT_SYMBOL_GPL(iommu_fwspec_add_ids);

 
int iommu_dev_enable_feature(struct device *dev, enum iommu_dev_features feat)
{
	if (dev->iommu && dev->iommu->iommu_dev) {
		const struct iommu_ops *ops = dev->iommu->iommu_dev->ops;

		if (ops->dev_enable_feat)
			return ops->dev_enable_feat(dev, feat);
	}

	return -ENODEV;
}
EXPORT_SYMBOL_GPL(iommu_dev_enable_feature);

 
int iommu_dev_disable_feature(struct device *dev, enum iommu_dev_features feat)
{
	if (dev->iommu && dev->iommu->iommu_dev) {
		const struct iommu_ops *ops = dev->iommu->iommu_dev->ops;

		if (ops->dev_disable_feat)
			return ops->dev_disable_feat(dev, feat);
	}

	return -EBUSY;
}
EXPORT_SYMBOL_GPL(iommu_dev_disable_feature);

 
static int iommu_setup_default_domain(struct iommu_group *group,
				      int target_type)
{
	struct iommu_domain *old_dom = group->default_domain;
	struct group_device *gdev;
	struct iommu_domain *dom;
	bool direct_failed;
	int req_type;
	int ret;

	lockdep_assert_held(&group->mutex);

	req_type = iommu_get_default_domain_type(group, target_type);
	if (req_type < 0)
		return -EINVAL;

	 
	dom = iommu_group_alloc_default_domain(group, req_type);
	if (!dom) {
		 
		if (group->default_domain)
			return -ENODEV;
		group->default_domain = NULL;
		return 0;
	}

	if (group->default_domain == dom)
		return 0;

	 
	direct_failed = false;
	for_each_group_device(group, gdev) {
		if (iommu_create_device_direct_mappings(dom, gdev->dev)) {
			direct_failed = true;
			dev_warn_once(
				gdev->dev->iommu->iommu_dev->dev,
				"IOMMU driver was not able to establish FW requested direct mapping.");
		}
	}

	 
	group->default_domain = dom;
	if (!group->domain) {
		 
		ret = __iommu_group_set_domain_internal(
			group, dom, IOMMU_SET_DOMAIN_MUST_SUCCEED);
		if (WARN_ON(ret))
			goto out_free_old;
	} else {
		ret = __iommu_group_set_domain(group, dom);
		if (ret)
			goto err_restore_def_domain;
	}

	 
	if (direct_failed) {
		for_each_group_device(group, gdev) {
			ret = iommu_create_device_direct_mappings(dom, gdev->dev);
			if (ret)
				goto err_restore_domain;
		}
	}

out_free_old:
	if (old_dom)
		iommu_domain_free(old_dom);
	return ret;

err_restore_domain:
	if (old_dom)
		__iommu_group_set_domain_internal(
			group, old_dom, IOMMU_SET_DOMAIN_MUST_SUCCEED);
err_restore_def_domain:
	if (old_dom) {
		iommu_domain_free(dom);
		group->default_domain = old_dom;
	}
	return ret;
}

 
static ssize_t iommu_group_store_type(struct iommu_group *group,
				      const char *buf, size_t count)
{
	struct group_device *gdev;
	int ret, req_type;

	if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
		return -EACCES;

	if (WARN_ON(!group) || !group->default_domain)
		return -EINVAL;

	if (sysfs_streq(buf, "identity"))
		req_type = IOMMU_DOMAIN_IDENTITY;
	else if (sysfs_streq(buf, "DMA"))
		req_type = IOMMU_DOMAIN_DMA;
	else if (sysfs_streq(buf, "DMA-FQ"))
		req_type = IOMMU_DOMAIN_DMA_FQ;
	else if (sysfs_streq(buf, "auto"))
		req_type = 0;
	else
		return -EINVAL;

	mutex_lock(&group->mutex);
	 
	if (req_type == IOMMU_DOMAIN_DMA_FQ &&
	    group->default_domain->type == IOMMU_DOMAIN_DMA) {
		ret = iommu_dma_init_fq(group->default_domain);
		if (ret)
			goto out_unlock;

		group->default_domain->type = IOMMU_DOMAIN_DMA_FQ;
		ret = count;
		goto out_unlock;
	}

	 
	if (list_empty(&group->devices) || group->owner_cnt) {
		ret = -EPERM;
		goto out_unlock;
	}

	ret = iommu_setup_default_domain(group, req_type);
	if (ret)
		goto out_unlock;

	 
	mutex_unlock(&group->mutex);

	 
	for_each_group_device(group, gdev)
		iommu_group_do_probe_finalize(gdev->dev);
	return count;

out_unlock:
	mutex_unlock(&group->mutex);
	return ret ?: count;
}

static bool iommu_is_default_domain(struct iommu_group *group)
{
	if (group->domain == group->default_domain)
		return true;

	 
	if (group->default_domain &&
	    group->default_domain->type == IOMMU_DOMAIN_IDENTITY &&
	    group->domain && group->domain->type == IOMMU_DOMAIN_IDENTITY)
		return true;
	return false;
}

 
int iommu_device_use_default_domain(struct device *dev)
{
	struct iommu_group *group = iommu_group_get(dev);
	int ret = 0;

	if (!group)
		return 0;

	mutex_lock(&group->mutex);
	if (group->owner_cnt) {
		if (group->owner || !iommu_is_default_domain(group) ||
		    !xa_empty(&group->pasid_array)) {
			ret = -EBUSY;
			goto unlock_out;
		}
	}

	group->owner_cnt++;

unlock_out:
	mutex_unlock(&group->mutex);
	iommu_group_put(group);

	return ret;
}

 
void iommu_device_unuse_default_domain(struct device *dev)
{
	struct iommu_group *group = iommu_group_get(dev);

	if (!group)
		return;

	mutex_lock(&group->mutex);
	if (!WARN_ON(!group->owner_cnt || !xa_empty(&group->pasid_array)))
		group->owner_cnt--;

	mutex_unlock(&group->mutex);
	iommu_group_put(group);
}

static int __iommu_group_alloc_blocking_domain(struct iommu_group *group)
{
	struct group_device *dev =
		list_first_entry(&group->devices, struct group_device, list);

	if (group->blocking_domain)
		return 0;

	group->blocking_domain =
		__iommu_domain_alloc(dev->dev->bus, IOMMU_DOMAIN_BLOCKED);
	if (!group->blocking_domain) {
		 
		group->blocking_domain = __iommu_domain_alloc(
			dev->dev->bus, IOMMU_DOMAIN_UNMANAGED);
		if (!group->blocking_domain)
			return -EINVAL;
	}
	return 0;
}

static int __iommu_take_dma_ownership(struct iommu_group *group, void *owner)
{
	int ret;

	if ((group->domain && group->domain != group->default_domain) ||
	    !xa_empty(&group->pasid_array))
		return -EBUSY;

	ret = __iommu_group_alloc_blocking_domain(group);
	if (ret)
		return ret;
	ret = __iommu_group_set_domain(group, group->blocking_domain);
	if (ret)
		return ret;

	group->owner = owner;
	group->owner_cnt++;
	return 0;
}

 
int iommu_group_claim_dma_owner(struct iommu_group *group, void *owner)
{
	int ret = 0;

	if (WARN_ON(!owner))
		return -EINVAL;

	mutex_lock(&group->mutex);
	if (group->owner_cnt) {
		ret = -EPERM;
		goto unlock_out;
	}

	ret = __iommu_take_dma_ownership(group, owner);
unlock_out:
	mutex_unlock(&group->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(iommu_group_claim_dma_owner);

 
int iommu_device_claim_dma_owner(struct device *dev, void *owner)
{
	struct iommu_group *group;
	int ret = 0;

	if (WARN_ON(!owner))
		return -EINVAL;

	group = iommu_group_get(dev);
	if (!group)
		return -ENODEV;

	mutex_lock(&group->mutex);
	if (group->owner_cnt) {
		if (group->owner != owner) {
			ret = -EPERM;
			goto unlock_out;
		}
		group->owner_cnt++;
		goto unlock_out;
	}

	ret = __iommu_take_dma_ownership(group, owner);
unlock_out:
	mutex_unlock(&group->mutex);
	iommu_group_put(group);

	return ret;
}
EXPORT_SYMBOL_GPL(iommu_device_claim_dma_owner);

static void __iommu_release_dma_ownership(struct iommu_group *group)
{
	if (WARN_ON(!group->owner_cnt || !group->owner ||
		    !xa_empty(&group->pasid_array)))
		return;

	group->owner_cnt = 0;
	group->owner = NULL;
	__iommu_group_set_domain_nofail(group, group->default_domain);
}

 
void iommu_group_release_dma_owner(struct iommu_group *group)
{
	mutex_lock(&group->mutex);
	__iommu_release_dma_ownership(group);
	mutex_unlock(&group->mutex);
}
EXPORT_SYMBOL_GPL(iommu_group_release_dma_owner);

 
void iommu_device_release_dma_owner(struct device *dev)
{
	struct iommu_group *group = iommu_group_get(dev);

	mutex_lock(&group->mutex);
	if (group->owner_cnt > 1)
		group->owner_cnt--;
	else
		__iommu_release_dma_ownership(group);
	mutex_unlock(&group->mutex);
	iommu_group_put(group);
}
EXPORT_SYMBOL_GPL(iommu_device_release_dma_owner);

 
bool iommu_group_dma_owner_claimed(struct iommu_group *group)
{
	unsigned int user;

	mutex_lock(&group->mutex);
	user = group->owner_cnt;
	mutex_unlock(&group->mutex);

	return user;
}
EXPORT_SYMBOL_GPL(iommu_group_dma_owner_claimed);

static int __iommu_set_group_pasid(struct iommu_domain *domain,
				   struct iommu_group *group, ioasid_t pasid)
{
	struct group_device *device;
	int ret = 0;

	for_each_group_device(group, device) {
		ret = domain->ops->set_dev_pasid(domain, device->dev, pasid);
		if (ret)
			break;
	}

	return ret;
}

static void __iommu_remove_group_pasid(struct iommu_group *group,
				       ioasid_t pasid)
{
	struct group_device *device;
	const struct iommu_ops *ops;

	for_each_group_device(group, device) {
		ops = dev_iommu_ops(device->dev);
		ops->remove_dev_pasid(device->dev, pasid);
	}
}

 
int iommu_attach_device_pasid(struct iommu_domain *domain,
			      struct device *dev, ioasid_t pasid)
{
	struct iommu_group *group;
	void *curr;
	int ret;

	if (!domain->ops->set_dev_pasid)
		return -EOPNOTSUPP;

	group = iommu_group_get(dev);
	if (!group)
		return -ENODEV;

	mutex_lock(&group->mutex);
	curr = xa_cmpxchg(&group->pasid_array, pasid, NULL, domain, GFP_KERNEL);
	if (curr) {
		ret = xa_err(curr) ? : -EBUSY;
		goto out_unlock;
	}

	ret = __iommu_set_group_pasid(domain, group, pasid);
	if (ret) {
		__iommu_remove_group_pasid(group, pasid);
		xa_erase(&group->pasid_array, pasid);
	}
out_unlock:
	mutex_unlock(&group->mutex);
	iommu_group_put(group);

	return ret;
}
EXPORT_SYMBOL_GPL(iommu_attach_device_pasid);

 
void iommu_detach_device_pasid(struct iommu_domain *domain, struct device *dev,
			       ioasid_t pasid)
{
	struct iommu_group *group = iommu_group_get(dev);

	mutex_lock(&group->mutex);
	__iommu_remove_group_pasid(group, pasid);
	WARN_ON(xa_erase(&group->pasid_array, pasid) != domain);
	mutex_unlock(&group->mutex);

	iommu_group_put(group);
}
EXPORT_SYMBOL_GPL(iommu_detach_device_pasid);

 
struct iommu_domain *iommu_get_domain_for_dev_pasid(struct device *dev,
						    ioasid_t pasid,
						    unsigned int type)
{
	struct iommu_domain *domain;
	struct iommu_group *group;

	group = iommu_group_get(dev);
	if (!group)
		return NULL;

	xa_lock(&group->pasid_array);
	domain = xa_load(&group->pasid_array, pasid);
	if (type && domain && domain->type != type)
		domain = ERR_PTR(-EBUSY);
	xa_unlock(&group->pasid_array);
	iommu_group_put(group);

	return domain;
}
EXPORT_SYMBOL_GPL(iommu_get_domain_for_dev_pasid);

struct iommu_domain *iommu_sva_domain_alloc(struct device *dev,
					    struct mm_struct *mm)
{
	const struct iommu_ops *ops = dev_iommu_ops(dev);
	struct iommu_domain *domain;

	domain = ops->domain_alloc(IOMMU_DOMAIN_SVA);
	if (!domain)
		return NULL;

	domain->type = IOMMU_DOMAIN_SVA;
	mmgrab(mm);
	domain->mm = mm;
	domain->iopf_handler = iommu_sva_handle_iopf;
	domain->fault_data = mm;

	return domain;
}

ioasid_t iommu_alloc_global_pasid(struct device *dev)
{
	int ret;

	 
	if (!dev->iommu->max_pasids)
		return IOMMU_PASID_INVALID;

	 
	ret = ida_alloc_range(&iommu_global_pasid_ida, IOMMU_FIRST_GLOBAL_PASID,
			      dev->iommu->max_pasids - 1, GFP_KERNEL);
	return ret < 0 ? IOMMU_PASID_INVALID : ret;
}
EXPORT_SYMBOL_GPL(iommu_alloc_global_pasid);

void iommu_free_global_pasid(ioasid_t pasid)
{
	if (WARN_ON(pasid == IOMMU_PASID_INVALID))
		return;

	ida_free(&iommu_global_pasid_ida, pasid);
}
EXPORT_SYMBOL_GPL(iommu_free_global_pasid);
