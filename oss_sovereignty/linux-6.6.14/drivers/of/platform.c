
 

#define pr_fmt(fmt)	"OF: " fmt

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/amba/bus.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "of_private.h"

const struct of_device_id of_default_bus_match_table[] = {
	{ .compatible = "simple-bus", },
	{ .compatible = "simple-mfd", },
	{ .compatible = "isa", },
#ifdef CONFIG_ARM_AMBA
	{ .compatible = "arm,amba-bus", },
#endif  
	{}  
};

 
struct platform_device *of_find_device_by_node(struct device_node *np)
{
	struct device *dev;

	dev = bus_find_device_by_of_node(&platform_bus_type, np);
	return dev ? to_platform_device(dev) : NULL;
}
EXPORT_SYMBOL(of_find_device_by_node);

int of_device_add(struct platform_device *ofdev)
{
	BUG_ON(ofdev->dev.of_node == NULL);

	 
	ofdev->name = dev_name(&ofdev->dev);
	ofdev->id = PLATFORM_DEVID_NONE;

	 
	set_dev_node(&ofdev->dev, of_node_to_nid(ofdev->dev.of_node));

	return device_add(&ofdev->dev);
}

int of_device_register(struct platform_device *pdev)
{
	device_initialize(&pdev->dev);
	return of_device_add(pdev);
}
EXPORT_SYMBOL(of_device_register);

void of_device_unregister(struct platform_device *ofdev)
{
	device_unregister(&ofdev->dev);
}
EXPORT_SYMBOL(of_device_unregister);

#ifdef CONFIG_OF_ADDRESS
static const struct of_device_id of_skipped_node_table[] = {
	{ .compatible = "operating-points-v2", },
	{}  
};

 

 
static void of_device_make_bus_id(struct device *dev)
{
	struct device_node *node = dev->of_node;
	const __be32 *reg;
	u64 addr;
	u32 mask;

	 
	while (node->parent) {
		 
		reg = of_get_property(node, "reg", NULL);
		if (reg && (addr = of_translate_address(node, reg)) != OF_BAD_ADDR) {
			if (!of_property_read_u32(node, "mask", &mask))
				dev_set_name(dev, dev_name(dev) ? "%llx.%x.%pOFn:%s" : "%llx.%x.%pOFn",
					     addr, ffs(mask) - 1, node, dev_name(dev));

			else
				dev_set_name(dev, dev_name(dev) ? "%llx.%pOFn:%s" : "%llx.%pOFn",
					     addr, node, dev_name(dev));
			return;
		}

		 
		dev_set_name(dev, dev_name(dev) ? "%s:%s" : "%s",
			     kbasename(node->full_name), dev_name(dev));
		node = node->parent;
	}
}

 
struct platform_device *of_device_alloc(struct device_node *np,
				  const char *bus_id,
				  struct device *parent)
{
	struct platform_device *dev;
	int rc, i, num_reg = 0;
	struct resource *res;

	dev = platform_device_alloc("", PLATFORM_DEVID_NONE);
	if (!dev)
		return NULL;

	 
	num_reg = of_address_count(np);

	 
	if (num_reg) {
		res = kcalloc(num_reg, sizeof(*res), GFP_KERNEL);
		if (!res) {
			platform_device_put(dev);
			return NULL;
		}

		dev->num_resources = num_reg;
		dev->resource = res;
		for (i = 0; i < num_reg; i++, res++) {
			rc = of_address_to_resource(np, i, res);
			WARN_ON(rc);
		}
	}

	 
	device_set_node(&dev->dev, of_fwnode_handle(of_node_get(np)));
	dev->dev.parent = parent ? : &platform_bus;

	if (bus_id)
		dev_set_name(&dev->dev, "%s", bus_id);
	else
		of_device_make_bus_id(&dev->dev);

	return dev;
}
EXPORT_SYMBOL(of_device_alloc);

 
static struct platform_device *of_platform_device_create_pdata(
					struct device_node *np,
					const char *bus_id,
					void *platform_data,
					struct device *parent)
{
	struct platform_device *dev;

	if (!of_device_is_available(np) ||
	    of_node_test_and_set_flag(np, OF_POPULATED))
		return NULL;

	dev = of_device_alloc(np, bus_id, parent);
	if (!dev)
		goto err_clear_flag;

	dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	if (!dev->dev.dma_mask)
		dev->dev.dma_mask = &dev->dev.coherent_dma_mask;
	dev->dev.bus = &platform_bus_type;
	dev->dev.platform_data = platform_data;
	of_msi_configure(&dev->dev, dev->dev.of_node);

	if (of_device_add(dev) != 0) {
		platform_device_put(dev);
		goto err_clear_flag;
	}

	return dev;

err_clear_flag:
	of_node_clear_flag(np, OF_POPULATED);
	return NULL;
}

 
struct platform_device *of_platform_device_create(struct device_node *np,
					    const char *bus_id,
					    struct device *parent)
{
	return of_platform_device_create_pdata(np, bus_id, NULL, parent);
}
EXPORT_SYMBOL(of_platform_device_create);

#ifdef CONFIG_ARM_AMBA
static struct amba_device *of_amba_device_create(struct device_node *node,
						 const char *bus_id,
						 void *platform_data,
						 struct device *parent)
{
	struct amba_device *dev;
	int ret;

	pr_debug("Creating amba device %pOF\n", node);

	if (!of_device_is_available(node) ||
	    of_node_test_and_set_flag(node, OF_POPULATED))
		return NULL;

	dev = amba_device_alloc(NULL, 0, 0);
	if (!dev)
		goto err_clear_flag;

	 
	dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	dev->dev.dma_mask = &dev->dev.coherent_dma_mask;

	 
	device_set_node(&dev->dev, of_fwnode_handle(of_node_get(node)));
	dev->dev.parent = parent ? : &platform_bus;
	dev->dev.platform_data = platform_data;
	if (bus_id)
		dev_set_name(&dev->dev, "%s", bus_id);
	else
		of_device_make_bus_id(&dev->dev);

	 
	of_property_read_u32(node, "arm,primecell-periphid", &dev->periphid);

	ret = of_address_to_resource(node, 0, &dev->res);
	if (ret) {
		pr_err("amba: of_address_to_resource() failed (%d) for %pOF\n",
		       ret, node);
		goto err_free;
	}

	ret = amba_device_add(dev, &iomem_resource);
	if (ret) {
		pr_err("amba_device_add() failed (%d) for %pOF\n",
		       ret, node);
		goto err_free;
	}

	return dev;

err_free:
	amba_device_put(dev);
err_clear_flag:
	of_node_clear_flag(node, OF_POPULATED);
	return NULL;
}
#else  
static struct amba_device *of_amba_device_create(struct device_node *node,
						 const char *bus_id,
						 void *platform_data,
						 struct device *parent)
{
	return NULL;
}
#endif  

 
static const struct of_dev_auxdata *of_dev_lookup(const struct of_dev_auxdata *lookup,
				 struct device_node *np)
{
	const struct of_dev_auxdata *auxdata;
	struct resource res;
	int compatible = 0;

	if (!lookup)
		return NULL;

	auxdata = lookup;
	for (; auxdata->compatible; auxdata++) {
		if (!of_device_is_compatible(np, auxdata->compatible))
			continue;
		compatible++;
		if (!of_address_to_resource(np, 0, &res))
			if (res.start != auxdata->phys_addr)
				continue;
		pr_debug("%pOF: devname=%s\n", np, auxdata->name);
		return auxdata;
	}

	if (!compatible)
		return NULL;

	 
	auxdata = lookup;
	for (; auxdata->compatible; auxdata++) {
		if (!of_device_is_compatible(np, auxdata->compatible))
			continue;
		if (!auxdata->phys_addr && !auxdata->name) {
			pr_debug("%pOF: compatible match\n", np);
			return auxdata;
		}
	}

	return NULL;
}

 
static int of_platform_bus_create(struct device_node *bus,
				  const struct of_device_id *matches,
				  const struct of_dev_auxdata *lookup,
				  struct device *parent, bool strict)
{
	const struct of_dev_auxdata *auxdata;
	struct device_node *child;
	struct platform_device *dev;
	const char *bus_id = NULL;
	void *platform_data = NULL;
	int rc = 0;

	 
	if (strict && (!of_get_property(bus, "compatible", NULL))) {
		pr_debug("%s() - skipping %pOF, no compatible prop\n",
			 __func__, bus);
		return 0;
	}

	 
	if (unlikely(of_match_node(of_skipped_node_table, bus))) {
		pr_debug("%s() - skipping %pOF node\n", __func__, bus);
		return 0;
	}

	if (of_node_check_flag(bus, OF_POPULATED_BUS)) {
		pr_debug("%s() - skipping %pOF, already populated\n",
			__func__, bus);
		return 0;
	}

	auxdata = of_dev_lookup(lookup, bus);
	if (auxdata) {
		bus_id = auxdata->name;
		platform_data = auxdata->platform_data;
	}

	if (of_device_is_compatible(bus, "arm,primecell")) {
		 
		of_amba_device_create(bus, bus_id, platform_data, parent);
		return 0;
	}

	dev = of_platform_device_create_pdata(bus, bus_id, platform_data, parent);
	if (!dev || !of_match_node(matches, bus))
		return 0;

	for_each_child_of_node(bus, child) {
		pr_debug("   create child: %pOF\n", child);
		rc = of_platform_bus_create(child, matches, lookup, &dev->dev, strict);
		if (rc) {
			of_node_put(child);
			break;
		}
	}
	of_node_set_flag(bus, OF_POPULATED_BUS);
	return rc;
}

 
int of_platform_bus_probe(struct device_node *root,
			  const struct of_device_id *matches,
			  struct device *parent)
{
	struct device_node *child;
	int rc = 0;

	root = root ? of_node_get(root) : of_find_node_by_path("/");
	if (!root)
		return -EINVAL;

	pr_debug("%s()\n", __func__);
	pr_debug(" starting at: %pOF\n", root);

	 
	if (of_match_node(matches, root)) {
		rc = of_platform_bus_create(root, matches, NULL, parent, false);
	} else for_each_child_of_node(root, child) {
		if (!of_match_node(matches, child))
			continue;
		rc = of_platform_bus_create(child, matches, NULL, parent, false);
		if (rc) {
			of_node_put(child);
			break;
		}
	}

	of_node_put(root);
	return rc;
}
EXPORT_SYMBOL(of_platform_bus_probe);

 
int of_platform_populate(struct device_node *root,
			const struct of_device_id *matches,
			const struct of_dev_auxdata *lookup,
			struct device *parent)
{
	struct device_node *child;
	int rc = 0;

	root = root ? of_node_get(root) : of_find_node_by_path("/");
	if (!root)
		return -EINVAL;

	pr_debug("%s()\n", __func__);
	pr_debug(" starting at: %pOF\n", root);

	device_links_supplier_sync_state_pause();
	for_each_child_of_node(root, child) {
		rc = of_platform_bus_create(child, matches, lookup, parent, true);
		if (rc) {
			of_node_put(child);
			break;
		}
	}
	device_links_supplier_sync_state_resume();

	of_node_set_flag(root, OF_POPULATED_BUS);

	of_node_put(root);
	return rc;
}
EXPORT_SYMBOL_GPL(of_platform_populate);

int of_platform_default_populate(struct device_node *root,
				 const struct of_dev_auxdata *lookup,
				 struct device *parent)
{
	return of_platform_populate(root, of_default_bus_match_table, lookup,
				    parent);
}
EXPORT_SYMBOL_GPL(of_platform_default_populate);

static const struct of_device_id reserved_mem_matches[] = {
	{ .compatible = "phram" },
	{ .compatible = "qcom,rmtfs-mem" },
	{ .compatible = "qcom,cmd-db" },
	{ .compatible = "qcom,smem" },
	{ .compatible = "ramoops" },
	{ .compatible = "nvmem-rmem" },
	{ .compatible = "google,open-dice" },
	{}
};

static int __init of_platform_default_populate_init(void)
{
	struct device_node *node;

	device_links_supplier_sync_state_pause();

	if (!of_have_populated_dt())
		return -ENODEV;

	if (IS_ENABLED(CONFIG_PPC)) {
		struct device_node *boot_display = NULL;
		struct platform_device *dev;
		int display_number = 0;
		int ret;

		 
		if (of_property_present(of_chosen, "linux,bootx-noscreen")) {
			 
			dev = platform_device_alloc("bootx-noscreen", 0);
			if (WARN_ON(!dev))
				return -ENOMEM;
			ret = platform_device_add(dev);
			if (WARN_ON(ret)) {
				platform_device_put(dev);
				return ret;
			}
		}

		 
		for_each_node_by_type(node, "display") {
			if (!of_get_property(node, "linux,opened", NULL) ||
			    !of_get_property(node, "linux,boot-display", NULL))
				continue;
			dev = of_platform_device_create(node, "of-display", NULL);
			of_node_put(node);
			if (WARN_ON(!dev))
				return -ENOMEM;
			boot_display = node;
			display_number++;
			break;
		}
		for_each_node_by_type(node, "display") {
			char buf[14];
			const char *of_display_format = "of-display.%d";

			if (!of_get_property(node, "linux,opened", NULL) || node == boot_display)
				continue;
			ret = snprintf(buf, sizeof(buf), of_display_format, display_number++);
			if (ret < sizeof(buf))
				of_platform_device_create(node, buf, NULL);
		}

	} else {
		 
		for_each_matching_node(node, reserved_mem_matches)
			of_platform_device_create(node, NULL, NULL);

		node = of_find_node_by_path("/firmware");
		if (node) {
			of_platform_populate(node, NULL, NULL, NULL);
			of_node_put(node);
		}

		node = of_get_compatible_child(of_chosen, "simple-framebuffer");
		of_platform_device_create(node, NULL, NULL);
		of_node_put(node);

		 
		of_platform_default_populate(NULL, NULL, NULL);
	}

	return 0;
}
arch_initcall_sync(of_platform_default_populate_init);

static int __init of_platform_sync_state_init(void)
{
	device_links_supplier_sync_state_resume();
	return 0;
}
late_initcall_sync(of_platform_sync_state_init);

int of_platform_device_destroy(struct device *dev, void *data)
{
	 
	if (!dev->of_node || !of_node_check_flag(dev->of_node, OF_POPULATED))
		return 0;

	 
	if (of_node_check_flag(dev->of_node, OF_POPULATED_BUS))
		device_for_each_child(dev, NULL, of_platform_device_destroy);

	of_node_clear_flag(dev->of_node, OF_POPULATED);
	of_node_clear_flag(dev->of_node, OF_POPULATED_BUS);

	if (dev->bus == &platform_bus_type)
		platform_device_unregister(to_platform_device(dev));
#ifdef CONFIG_ARM_AMBA
	else if (dev->bus == &amba_bustype)
		amba_device_unregister(to_amba_device(dev));
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(of_platform_device_destroy);

 
void of_platform_depopulate(struct device *parent)
{
	if (parent->of_node && of_node_check_flag(parent->of_node, OF_POPULATED_BUS)) {
		device_for_each_child_reverse(parent, NULL, of_platform_device_destroy);
		of_node_clear_flag(parent->of_node, OF_POPULATED_BUS);
	}
}
EXPORT_SYMBOL_GPL(of_platform_depopulate);

static void devm_of_platform_populate_release(struct device *dev, void *res)
{
	of_platform_depopulate(*(struct device **)res);
}

 
int devm_of_platform_populate(struct device *dev)
{
	struct device **ptr;
	int ret;

	if (!dev)
		return -EINVAL;

	ptr = devres_alloc(devm_of_platform_populate_release,
			   sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (ret) {
		devres_free(ptr);
	} else {
		*ptr = dev;
		devres_add(dev, ptr);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(devm_of_platform_populate);

static int devm_of_platform_match(struct device *dev, void *res, void *data)
{
	struct device **ptr = res;

	if (!ptr) {
		WARN_ON(!ptr);
		return 0;
	}

	return *ptr == data;
}

 
void devm_of_platform_depopulate(struct device *dev)
{
	int ret;

	ret = devres_release(dev, devm_of_platform_populate_release,
			     devm_of_platform_match, dev);

	WARN_ON(ret);
}
EXPORT_SYMBOL_GPL(devm_of_platform_depopulate);

#ifdef CONFIG_OF_DYNAMIC
static int of_platform_notify(struct notifier_block *nb,
				unsigned long action, void *arg)
{
	struct of_reconfig_data *rd = arg;
	struct platform_device *pdev_parent, *pdev;
	bool children_left;

	switch (of_reconfig_get_state_change(action, rd)) {
	case OF_RECONFIG_CHANGE_ADD:
		 
		if (!of_node_check_flag(rd->dn->parent, OF_POPULATED_BUS))
			return NOTIFY_OK;	 

		 
		if (of_node_check_flag(rd->dn, OF_POPULATED))
			return NOTIFY_OK;

		 
		rd->dn->fwnode.flags &= ~FWNODE_FLAG_NOT_DEVICE;
		 
		pdev_parent = of_find_device_by_node(rd->dn->parent);
		pdev = of_platform_device_create(rd->dn, NULL,
				pdev_parent ? &pdev_parent->dev : NULL);
		platform_device_put(pdev_parent);

		if (pdev == NULL) {
			pr_err("%s: failed to create for '%pOF'\n",
					__func__, rd->dn);
			 
			return notifier_from_errno(-EINVAL);
		}
		break;

	case OF_RECONFIG_CHANGE_REMOVE:

		 
		if (!of_node_check_flag(rd->dn, OF_POPULATED))
			return NOTIFY_OK;

		 
		pdev = of_find_device_by_node(rd->dn);
		if (pdev == NULL)
			return NOTIFY_OK;	 

		 
		of_platform_device_destroy(&pdev->dev, &children_left);

		 
		platform_device_put(pdev);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block platform_of_notifier = {
	.notifier_call = of_platform_notify,
};

void of_platform_register_reconfig_notifier(void)
{
	WARN_ON(of_reconfig_notifier_register(&platform_of_notifier));
}
#endif  

#endif  
