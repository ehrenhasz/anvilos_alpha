
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_iommu.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-direct.h>  
#include <linux/dma-map-ops.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <asm/errno.h>
#include "of_private.h"

 
const struct of_device_id *of_match_device(const struct of_device_id *matches,
					   const struct device *dev)
{
	if (!matches || !dev->of_node || dev->of_node_reused)
		return NULL;
	return of_match_node(matches, dev->of_node);
}
EXPORT_SYMBOL(of_match_device);

static void
of_dma_set_restricted_buffer(struct device *dev, struct device_node *np)
{
	struct device_node *node, *of_node = dev->of_node;
	int count, i;

	if (!IS_ENABLED(CONFIG_DMA_RESTRICTED_POOL))
		return;

	count = of_property_count_elems_of_size(of_node, "memory-region",
						sizeof(u32));
	 
	if (count <= 0) {
		of_node = np;
		count = of_property_count_elems_of_size(
			of_node, "memory-region", sizeof(u32));
	}

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(of_node, "memory-region", i);
		 
		if (of_device_is_compatible(node, "restricted-dma-pool") &&
		    of_device_is_available(node)) {
			of_node_put(node);
			break;
		}
		of_node_put(node);
	}

	 
	if (i < count && of_reserved_mem_device_init_by_idx(dev, of_node, i))
		dev_warn(dev, "failed to initialise \"restricted-dma-pool\" memory node\n");
}

 
int of_dma_configure_id(struct device *dev, struct device_node *np,
			bool force_dma, const u32 *id)
{
	const struct iommu_ops *iommu;
	const struct bus_dma_region *map = NULL;
	struct device_node *bus_np;
	u64 dma_start = 0;
	u64 mask, end, size = 0;
	bool coherent;
	int ret;

	if (np == dev->of_node)
		bus_np = __of_get_dma_parent(np);
	else
		bus_np = of_node_get(np);

	ret = of_dma_get_range(bus_np, &map);
	of_node_put(bus_np);
	if (ret < 0) {
		 
		if (!force_dma)
			return ret == -ENODEV ? 0 : ret;
	} else {
		const struct bus_dma_region *r = map;
		u64 dma_end = 0;

		 
		for (dma_start = ~0; r->size; r++) {
			 
			if (r->dma_start < dma_start)
				dma_start = r->dma_start;
			if (r->dma_start + r->size > dma_end)
				dma_end = r->dma_start + r->size;
		}
		size = dma_end - dma_start;

		 
		if (size & 1) {
			dev_warn(dev, "Invalid size 0x%llx for dma-range(s)\n",
				 size);
			size = size + 1;
		}

		if (!size) {
			dev_err(dev, "Adjusted size 0x%llx invalid\n", size);
			kfree(map);
			return -EINVAL;
		}
	}

	 
	if (!dev->dma_mask) {
		dev_warn(dev, "DMA mask not set\n");
		dev->dma_mask = &dev->coherent_dma_mask;
	}

	if (!size && dev->coherent_dma_mask)
		size = max(dev->coherent_dma_mask, dev->coherent_dma_mask + 1);
	else if (!size)
		size = 1ULL << 32;

	 
	end = dma_start + size - 1;
	mask = DMA_BIT_MASK(ilog2(end) + 1);
	dev->coherent_dma_mask &= mask;
	*dev->dma_mask &= mask;
	 
	if (!ret) {
		dev->bus_dma_limit = end;
		dev->dma_range_map = map;
	}

	coherent = of_dma_is_coherent(np);
	dev_dbg(dev, "device is%sdma coherent\n",
		coherent ? " " : " not ");

	iommu = of_iommu_configure(dev, np, id);
	if (PTR_ERR(iommu) == -EPROBE_DEFER) {
		 
		if (!ret)
			dev->dma_range_map = NULL;
		kfree(map);
		return -EPROBE_DEFER;
	}

	dev_dbg(dev, "device is%sbehind an iommu\n",
		iommu ? " " : " not ");

	arch_setup_dma_ops(dev, dma_start, size, iommu, coherent);

	if (!iommu)
		of_dma_set_restricted_buffer(dev, np);

	return 0;
}
EXPORT_SYMBOL_GPL(of_dma_configure_id);

const void *of_device_get_match_data(const struct device *dev)
{
	const struct of_device_id *match;

	match = of_match_device(dev->driver->of_match_table, dev);
	if (!match)
		return NULL;

	return match->data;
}
EXPORT_SYMBOL(of_device_get_match_data);

 
ssize_t of_device_modalias(struct device *dev, char *str, ssize_t len)
{
	ssize_t sl;

	if (!dev || !dev->of_node || dev->of_node_reused)
		return -ENODEV;

	sl = of_modalias(dev->of_node, str, len - 2);
	if (sl < 0)
		return sl;
	if (sl > len - 2)
		return -ENOMEM;

	str[sl++] = '\n';
	str[sl] = 0;
	return sl;
}
EXPORT_SYMBOL_GPL(of_device_modalias);

 
void of_device_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	const char *compat, *type;
	struct alias_prop *app;
	struct property *p;
	int seen = 0;

	if ((!dev) || (!dev->of_node))
		return;

	add_uevent_var(env, "OF_NAME=%pOFn", dev->of_node);
	add_uevent_var(env, "OF_FULLNAME=%pOF", dev->of_node);
	type = of_node_get_device_type(dev->of_node);
	if (type)
		add_uevent_var(env, "OF_TYPE=%s", type);

	 
	of_property_for_each_string(dev->of_node, "compatible", p, compat) {
		add_uevent_var(env, "OF_COMPATIBLE_%d=%s", seen, compat);
		seen++;
	}
	add_uevent_var(env, "OF_COMPATIBLE_N=%d", seen);

	seen = 0;
	mutex_lock(&of_mutex);
	list_for_each_entry(app, &aliases_lookup, link) {
		if (dev->of_node == app->np) {
			add_uevent_var(env, "OF_ALIAS_%d=%s", seen,
				       app->alias);
			seen++;
		}
	}
	mutex_unlock(&of_mutex);
}
EXPORT_SYMBOL_GPL(of_device_uevent);

int of_device_uevent_modalias(const struct device *dev, struct kobj_uevent_env *env)
{
	int sl;

	if ((!dev) || (!dev->of_node) || dev->of_node_reused)
		return -ENODEV;

	 
	if (add_uevent_var(env, "MODALIAS="))
		return -ENOMEM;

	sl = of_modalias(dev->of_node, &env->buf[env->buflen-1],
			 sizeof(env->buf) - env->buflen);
	if (sl < 0)
		return sl;
	if (sl >= (sizeof(env->buf) - env->buflen))
		return -ENOMEM;
	env->buflen += sl;

	return 0;
}
EXPORT_SYMBOL_GPL(of_device_uevent_modalias);
