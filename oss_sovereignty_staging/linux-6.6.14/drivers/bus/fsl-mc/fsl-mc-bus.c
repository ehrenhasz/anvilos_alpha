
 

#define pr_fmt(fmt) "fsl-mc: " fmt

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/limits.h>
#include <linux/bitops.h>
#include <linux/dma-mapping.h>
#include <linux/acpi.h>
#include <linux/iommu.h>
#include <linux/dma-map-ops.h>

#include "fsl-mc-private.h"

 
#define FSL_MC_DEFAULT_DMA_MASK	(~0ULL)

static struct fsl_mc_version mc_version;

 
struct fsl_mc {
	struct fsl_mc_device *root_mc_bus_dev;
	u8 num_translation_ranges;
	struct fsl_mc_addr_translation_range *translation_ranges;
	void __iomem *fsl_mc_regs;
};

 
struct fsl_mc_addr_translation_range {
	enum dprc_region_type mc_region_type;
	u64 start_mc_offset;
	u64 end_mc_offset;
	phys_addr_t start_phys_addr;
};

#define FSL_MC_GCR1	0x0
#define GCR1_P1_STOP	BIT(31)
#define GCR1_P2_STOP	BIT(30)

#define FSL_MC_FAPR	0x28
#define MC_FAPR_PL	BIT(18)
#define MC_FAPR_BMT	BIT(17)

static phys_addr_t mc_portal_base_phys_addr;

 
static int fsl_mc_bus_match(struct device *dev, struct device_driver *drv)
{
	const struct fsl_mc_device_id *id;
	struct fsl_mc_device *mc_dev = to_fsl_mc_device(dev);
	struct fsl_mc_driver *mc_drv = to_fsl_mc_driver(drv);
	bool found = false;

	 
	if (mc_dev->driver_override) {
		found = !strcmp(mc_dev->driver_override, mc_drv->driver.name);
		goto out;
	}

	if (!mc_drv->match_id_table)
		goto out;

	 
	if ((mc_dev->obj_desc.state & FSL_MC_OBJ_STATE_PLUGGED) == 0 &&
	    !fsl_mc_is_root_dprc(&mc_dev->dev))
		goto out;

	 
	for (id = mc_drv->match_id_table; id->vendor != 0x0; id++) {
		if (id->vendor == mc_dev->obj_desc.vendor &&
		    strcmp(id->obj_type, mc_dev->obj_desc.type) == 0) {
			found = true;

			break;
		}
	}

out:
	dev_dbg(dev, "%smatched\n", found ? "" : "not ");
	return found;
}

 
static int fsl_mc_bus_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	const struct fsl_mc_device *mc_dev = to_fsl_mc_device(dev);

	if (add_uevent_var(env, "MODALIAS=fsl-mc:v%08Xd%s",
			   mc_dev->obj_desc.vendor,
			   mc_dev->obj_desc.type))
		return -ENOMEM;

	return 0;
}

static int fsl_mc_dma_configure(struct device *dev)
{
	struct device *dma_dev = dev;
	struct fsl_mc_device *mc_dev = to_fsl_mc_device(dev);
	struct fsl_mc_driver *mc_drv = to_fsl_mc_driver(dev->driver);
	u32 input_id = mc_dev->icid;
	int ret;

	while (dev_is_fsl_mc(dma_dev))
		dma_dev = dma_dev->parent;

	if (dev_of_node(dma_dev))
		ret = of_dma_configure_id(dev, dma_dev->of_node, 0, &input_id);
	else
		ret = acpi_dma_configure_id(dev, DEV_DMA_COHERENT, &input_id);

	if (!ret && !mc_drv->driver_managed_dma) {
		ret = iommu_device_use_default_domain(dev);
		if (ret)
			arch_teardown_dma_ops(dev);
	}

	return ret;
}

static void fsl_mc_dma_cleanup(struct device *dev)
{
	struct fsl_mc_driver *mc_drv = to_fsl_mc_driver(dev->driver);

	if (!mc_drv->driver_managed_dma)
		iommu_device_unuse_default_domain(dev);
}

static ssize_t modalias_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct fsl_mc_device *mc_dev = to_fsl_mc_device(dev);

	return sprintf(buf, "fsl-mc:v%08Xd%s\n", mc_dev->obj_desc.vendor,
		       mc_dev->obj_desc.type);
}
static DEVICE_ATTR_RO(modalias);

static ssize_t driver_override_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct fsl_mc_device *mc_dev = to_fsl_mc_device(dev);
	int ret;

	if (WARN_ON(dev->bus != &fsl_mc_bus_type))
		return -EINVAL;

	ret = driver_set_override(dev, &mc_dev->driver_override, buf, count);
	if (ret)
		return ret;

	return count;
}

static ssize_t driver_override_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct fsl_mc_device *mc_dev = to_fsl_mc_device(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", mc_dev->driver_override);
}
static DEVICE_ATTR_RW(driver_override);

static struct attribute *fsl_mc_dev_attrs[] = {
	&dev_attr_modalias.attr,
	&dev_attr_driver_override.attr,
	NULL,
};

ATTRIBUTE_GROUPS(fsl_mc_dev);

static int scan_fsl_mc_bus(struct device *dev, void *data)
{
	struct fsl_mc_device *root_mc_dev;
	struct fsl_mc_bus *root_mc_bus;

	if (!fsl_mc_is_root_dprc(dev))
		goto exit;

	root_mc_dev = to_fsl_mc_device(dev);
	root_mc_bus = to_fsl_mc_bus(root_mc_dev);
	mutex_lock(&root_mc_bus->scan_mutex);
	dprc_scan_objects(root_mc_dev, false);
	mutex_unlock(&root_mc_bus->scan_mutex);

exit:
	return 0;
}

static ssize_t rescan_store(const struct bus_type *bus,
			    const char *buf, size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val) < 0)
		return -EINVAL;

	if (val)
		bus_for_each_dev(bus, NULL, NULL, scan_fsl_mc_bus);

	return count;
}
static BUS_ATTR_WO(rescan);

static int fsl_mc_bus_set_autorescan(struct device *dev, void *data)
{
	struct fsl_mc_device *root_mc_dev;
	unsigned long val;
	char *buf = data;

	if (!fsl_mc_is_root_dprc(dev))
		goto exit;

	root_mc_dev = to_fsl_mc_device(dev);

	if (kstrtoul(buf, 0, &val) < 0)
		return -EINVAL;

	if (val)
		enable_dprc_irq(root_mc_dev);
	else
		disable_dprc_irq(root_mc_dev);

exit:
	return 0;
}

static int fsl_mc_bus_get_autorescan(struct device *dev, void *data)
{
	struct fsl_mc_device *root_mc_dev;
	char *buf = data;

	if (!fsl_mc_is_root_dprc(dev))
		goto exit;

	root_mc_dev = to_fsl_mc_device(dev);

	sprintf(buf, "%d\n", get_dprc_irq_state(root_mc_dev));
exit:
	return 0;
}

static ssize_t autorescan_store(const struct bus_type *bus,
				const char *buf, size_t count)
{
	bus_for_each_dev(bus, NULL, (void *)buf, fsl_mc_bus_set_autorescan);

	return count;
}

static ssize_t autorescan_show(const struct bus_type *bus, char *buf)
{
	bus_for_each_dev(bus, NULL, (void *)buf, fsl_mc_bus_get_autorescan);
	return strlen(buf);
}

static BUS_ATTR_RW(autorescan);

static struct attribute *fsl_mc_bus_attrs[] = {
	&bus_attr_rescan.attr,
	&bus_attr_autorescan.attr,
	NULL,
};

ATTRIBUTE_GROUPS(fsl_mc_bus);

struct bus_type fsl_mc_bus_type = {
	.name = "fsl-mc",
	.match = fsl_mc_bus_match,
	.uevent = fsl_mc_bus_uevent,
	.dma_configure  = fsl_mc_dma_configure,
	.dma_cleanup = fsl_mc_dma_cleanup,
	.dev_groups = fsl_mc_dev_groups,
	.bus_groups = fsl_mc_bus_groups,
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_type);

struct device_type fsl_mc_bus_dprc_type = {
	.name = "fsl_mc_bus_dprc"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dprc_type);

struct device_type fsl_mc_bus_dpni_type = {
	.name = "fsl_mc_bus_dpni"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dpni_type);

struct device_type fsl_mc_bus_dpio_type = {
	.name = "fsl_mc_bus_dpio"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dpio_type);

struct device_type fsl_mc_bus_dpsw_type = {
	.name = "fsl_mc_bus_dpsw"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dpsw_type);

struct device_type fsl_mc_bus_dpbp_type = {
	.name = "fsl_mc_bus_dpbp"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dpbp_type);

struct device_type fsl_mc_bus_dpcon_type = {
	.name = "fsl_mc_bus_dpcon"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dpcon_type);

struct device_type fsl_mc_bus_dpmcp_type = {
	.name = "fsl_mc_bus_dpmcp"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dpmcp_type);

struct device_type fsl_mc_bus_dpmac_type = {
	.name = "fsl_mc_bus_dpmac"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dpmac_type);

struct device_type fsl_mc_bus_dprtc_type = {
	.name = "fsl_mc_bus_dprtc"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dprtc_type);

struct device_type fsl_mc_bus_dpseci_type = {
	.name = "fsl_mc_bus_dpseci"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dpseci_type);

struct device_type fsl_mc_bus_dpdmux_type = {
	.name = "fsl_mc_bus_dpdmux"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dpdmux_type);

struct device_type fsl_mc_bus_dpdcei_type = {
	.name = "fsl_mc_bus_dpdcei"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dpdcei_type);

struct device_type fsl_mc_bus_dpaiop_type = {
	.name = "fsl_mc_bus_dpaiop"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dpaiop_type);

struct device_type fsl_mc_bus_dpci_type = {
	.name = "fsl_mc_bus_dpci"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dpci_type);

struct device_type fsl_mc_bus_dpdmai_type = {
	.name = "fsl_mc_bus_dpdmai"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dpdmai_type);

struct device_type fsl_mc_bus_dpdbg_type = {
	.name = "fsl_mc_bus_dpdbg"
};
EXPORT_SYMBOL_GPL(fsl_mc_bus_dpdbg_type);

static struct device_type *fsl_mc_get_device_type(const char *type)
{
	static const struct {
		struct device_type *dev_type;
		const char *type;
	} dev_types[] = {
		{ &fsl_mc_bus_dprc_type, "dprc" },
		{ &fsl_mc_bus_dpni_type, "dpni" },
		{ &fsl_mc_bus_dpio_type, "dpio" },
		{ &fsl_mc_bus_dpsw_type, "dpsw" },
		{ &fsl_mc_bus_dpbp_type, "dpbp" },
		{ &fsl_mc_bus_dpcon_type, "dpcon" },
		{ &fsl_mc_bus_dpmcp_type, "dpmcp" },
		{ &fsl_mc_bus_dpmac_type, "dpmac" },
		{ &fsl_mc_bus_dprtc_type, "dprtc" },
		{ &fsl_mc_bus_dpseci_type, "dpseci" },
		{ &fsl_mc_bus_dpdmux_type, "dpdmux" },
		{ &fsl_mc_bus_dpdcei_type, "dpdcei" },
		{ &fsl_mc_bus_dpaiop_type, "dpaiop" },
		{ &fsl_mc_bus_dpci_type, "dpci" },
		{ &fsl_mc_bus_dpdmai_type, "dpdmai" },
		{ &fsl_mc_bus_dpdbg_type, "dpdbg" },
		{ NULL, NULL }
	};
	int i;

	for (i = 0; dev_types[i].dev_type; i++)
		if (!strcmp(dev_types[i].type, type))
			return dev_types[i].dev_type;

	return NULL;
}

static int fsl_mc_driver_probe(struct device *dev)
{
	struct fsl_mc_driver *mc_drv;
	struct fsl_mc_device *mc_dev = to_fsl_mc_device(dev);
	int error;

	mc_drv = to_fsl_mc_driver(dev->driver);

	error = mc_drv->probe(mc_dev);
	if (error < 0) {
		if (error != -EPROBE_DEFER)
			dev_err(dev, "%s failed: %d\n", __func__, error);
		return error;
	}

	return 0;
}

static int fsl_mc_driver_remove(struct device *dev)
{
	struct fsl_mc_driver *mc_drv = to_fsl_mc_driver(dev->driver);
	struct fsl_mc_device *mc_dev = to_fsl_mc_device(dev);

	mc_drv->remove(mc_dev);

	return 0;
}

static void fsl_mc_driver_shutdown(struct device *dev)
{
	struct fsl_mc_driver *mc_drv = to_fsl_mc_driver(dev->driver);
	struct fsl_mc_device *mc_dev = to_fsl_mc_device(dev);

	mc_drv->shutdown(mc_dev);
}

 
int __fsl_mc_driver_register(struct fsl_mc_driver *mc_driver,
			     struct module *owner)
{
	int error;

	mc_driver->driver.owner = owner;
	mc_driver->driver.bus = &fsl_mc_bus_type;

	if (mc_driver->probe)
		mc_driver->driver.probe = fsl_mc_driver_probe;

	if (mc_driver->remove)
		mc_driver->driver.remove = fsl_mc_driver_remove;

	if (mc_driver->shutdown)
		mc_driver->driver.shutdown = fsl_mc_driver_shutdown;

	error = driver_register(&mc_driver->driver);
	if (error < 0) {
		pr_err("driver_register() failed for %s: %d\n",
		       mc_driver->driver.name, error);
		return error;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(__fsl_mc_driver_register);

 
void fsl_mc_driver_unregister(struct fsl_mc_driver *mc_driver)
{
	driver_unregister(&mc_driver->driver);
}
EXPORT_SYMBOL_GPL(fsl_mc_driver_unregister);

 
static int mc_get_version(struct fsl_mc_io *mc_io,
			  u32 cmd_flags,
			  struct fsl_mc_version *mc_ver_info)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpmng_rsp_get_version *rsp_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPMNG_CMDID_GET_VERSION,
					  cmd_flags,
					  0);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dpmng_rsp_get_version *)cmd.params;
	mc_ver_info->revision = le32_to_cpu(rsp_params->revision);
	mc_ver_info->major = le32_to_cpu(rsp_params->version_major);
	mc_ver_info->minor = le32_to_cpu(rsp_params->version_minor);

	return 0;
}

 
struct fsl_mc_version *fsl_mc_get_version(void)
{
	if (mc_version.major)
		return &mc_version;

	return NULL;
}
EXPORT_SYMBOL_GPL(fsl_mc_get_version);

 
void fsl_mc_get_root_dprc(struct device *dev,
			 struct device **root_dprc_dev)
{
	if (!dev) {
		*root_dprc_dev = NULL;
	} else if (!dev_is_fsl_mc(dev)) {
		*root_dprc_dev = NULL;
	} else {
		*root_dprc_dev = dev;
		while (dev_is_fsl_mc((*root_dprc_dev)->parent))
			*root_dprc_dev = (*root_dprc_dev)->parent;
	}
}

static int get_dprc_attr(struct fsl_mc_io *mc_io,
			 int container_id, struct dprc_attributes *attr)
{
	u16 dprc_handle;
	int error;

	error = dprc_open(mc_io, 0, container_id, &dprc_handle);
	if (error < 0) {
		dev_err(mc_io->dev, "dprc_open() failed: %d\n", error);
		return error;
	}

	memset(attr, 0, sizeof(struct dprc_attributes));
	error = dprc_get_attributes(mc_io, 0, dprc_handle, attr);
	if (error < 0) {
		dev_err(mc_io->dev, "dprc_get_attributes() failed: %d\n",
			error);
		goto common_cleanup;
	}

	error = 0;

common_cleanup:
	(void)dprc_close(mc_io, 0, dprc_handle);
	return error;
}

static int get_dprc_icid(struct fsl_mc_io *mc_io,
			 int container_id, u32 *icid)
{
	struct dprc_attributes attr;
	int error;

	error = get_dprc_attr(mc_io, container_id, &attr);
	if (error == 0)
		*icid = attr.icid;

	return error;
}

static int translate_mc_addr(struct fsl_mc_device *mc_dev,
			     enum dprc_region_type mc_region_type,
			     u64 mc_offset, phys_addr_t *phys_addr)
{
	int i;
	struct device *root_dprc_dev;
	struct fsl_mc *mc;

	fsl_mc_get_root_dprc(&mc_dev->dev, &root_dprc_dev);
	mc = dev_get_drvdata(root_dprc_dev->parent);

	if (mc->num_translation_ranges == 0) {
		 
		*phys_addr = mc_offset;
		return 0;
	}

	for (i = 0; i < mc->num_translation_ranges; i++) {
		struct fsl_mc_addr_translation_range *range =
			&mc->translation_ranges[i];

		if (mc_region_type == range->mc_region_type &&
		    mc_offset >= range->start_mc_offset &&
		    mc_offset < range->end_mc_offset) {
			*phys_addr = range->start_phys_addr +
				     (mc_offset - range->start_mc_offset);
			return 0;
		}
	}

	return -EFAULT;
}

static int fsl_mc_device_get_mmio_regions(struct fsl_mc_device *mc_dev,
					  struct fsl_mc_device *mc_bus_dev)
{
	int i;
	int error;
	struct resource *regions;
	struct fsl_mc_obj_desc *obj_desc = &mc_dev->obj_desc;
	struct device *parent_dev = mc_dev->dev.parent;
	enum dprc_region_type mc_region_type;

	if (is_fsl_mc_bus_dprc(mc_dev) ||
	    is_fsl_mc_bus_dpmcp(mc_dev)) {
		mc_region_type = DPRC_REGION_TYPE_MC_PORTAL;
	} else if (is_fsl_mc_bus_dpio(mc_dev)) {
		mc_region_type = DPRC_REGION_TYPE_QBMAN_PORTAL;
	} else {
		 
		return -EINVAL;
	}

	regions = kmalloc_array(obj_desc->region_count,
				sizeof(regions[0]), GFP_KERNEL);
	if (!regions)
		return -ENOMEM;

	for (i = 0; i < obj_desc->region_count; i++) {
		struct dprc_region_desc region_desc;

		error = dprc_get_obj_region(mc_bus_dev->mc_io,
					    0,
					    mc_bus_dev->mc_handle,
					    obj_desc->type,
					    obj_desc->id, i, &region_desc);
		if (error < 0) {
			dev_err(parent_dev,
				"dprc_get_obj_region() failed: %d\n", error);
			goto error_cleanup_regions;
		}
		 
		if (region_desc.base_address) {
			regions[i].start = region_desc.base_address +
						region_desc.base_offset;
		} else {
			error = translate_mc_addr(mc_dev, mc_region_type,
					  region_desc.base_offset,
					  &regions[i].start);

			 
			if (is_fsl_mc_bus_dprc(mc_dev) &&
			    regions[i].start == region_desc.base_offset)
				regions[i].start += mc_portal_base_phys_addr;
		}

		if (error < 0) {
			dev_err(parent_dev,
				"Invalid MC offset: %#x (for %s.%d\'s region %d)\n",
				region_desc.base_offset,
				obj_desc->type, obj_desc->id, i);
			goto error_cleanup_regions;
		}

		regions[i].end = regions[i].start + region_desc.size - 1;
		regions[i].name = "fsl-mc object MMIO region";
		regions[i].flags = region_desc.flags & IORESOURCE_BITS;
		regions[i].flags |= IORESOURCE_MEM;
	}

	mc_dev->regions = regions;
	return 0;

error_cleanup_regions:
	kfree(regions);
	return error;
}

 
bool fsl_mc_is_root_dprc(struct device *dev)
{
	struct device *root_dprc_dev;

	fsl_mc_get_root_dprc(dev, &root_dprc_dev);
	if (!root_dprc_dev)
		return false;
	return dev == root_dprc_dev;
}

static void fsl_mc_device_release(struct device *dev)
{
	struct fsl_mc_device *mc_dev = to_fsl_mc_device(dev);

	kfree(mc_dev->regions);

	if (is_fsl_mc_bus_dprc(mc_dev))
		kfree(to_fsl_mc_bus(mc_dev));
	else
		kfree(mc_dev);
}

 
int fsl_mc_device_add(struct fsl_mc_obj_desc *obj_desc,
		      struct fsl_mc_io *mc_io,
		      struct device *parent_dev,
		      struct fsl_mc_device **new_mc_dev)
{
	int error;
	struct fsl_mc_device *mc_dev = NULL;
	struct fsl_mc_bus *mc_bus = NULL;
	struct fsl_mc_device *parent_mc_dev;

	if (dev_is_fsl_mc(parent_dev))
		parent_mc_dev = to_fsl_mc_device(parent_dev);
	else
		parent_mc_dev = NULL;

	if (strcmp(obj_desc->type, "dprc") == 0) {
		 
		mc_bus = kzalloc(sizeof(*mc_bus), GFP_KERNEL);
		if (!mc_bus)
			return -ENOMEM;

		mutex_init(&mc_bus->scan_mutex);
		mc_dev = &mc_bus->mc_dev;
	} else {
		 
		mc_dev = kzalloc(sizeof(*mc_dev), GFP_KERNEL);
		if (!mc_dev)
			return -ENOMEM;
	}

	mc_dev->obj_desc = *obj_desc;
	mc_dev->mc_io = mc_io;
	device_initialize(&mc_dev->dev);
	mc_dev->dev.parent = parent_dev;
	mc_dev->dev.bus = &fsl_mc_bus_type;
	mc_dev->dev.release = fsl_mc_device_release;
	mc_dev->dev.type = fsl_mc_get_device_type(obj_desc->type);
	if (!mc_dev->dev.type) {
		error = -ENODEV;
		dev_err(parent_dev, "unknown device type %s\n", obj_desc->type);
		goto error_cleanup_dev;
	}
	dev_set_name(&mc_dev->dev, "%s.%d", obj_desc->type, obj_desc->id);

	if (strcmp(obj_desc->type, "dprc") == 0) {
		struct fsl_mc_io *mc_io2;

		mc_dev->flags |= FSL_MC_IS_DPRC;

		 
		if (parent_mc_dev) {
			 
			mc_io2 = parent_mc_dev->mc_io;
		} else {
			 
			if (!mc_io) {
				error = -EINVAL;
				goto error_cleanup_dev;
			}

			mc_io2 = mc_io;
		}

		error = get_dprc_icid(mc_io2, obj_desc->id, &mc_dev->icid);
		if (error < 0)
			goto error_cleanup_dev;
	} else {
		 
		mc_dev->icid = parent_mc_dev->icid;
		mc_dev->dma_mask = FSL_MC_DEFAULT_DMA_MASK;
		mc_dev->dev.dma_mask = &mc_dev->dma_mask;
		mc_dev->dev.coherent_dma_mask = mc_dev->dma_mask;
		dev_set_msi_domain(&mc_dev->dev,
				   dev_get_msi_domain(&parent_mc_dev->dev));
	}

	 
	if (parent_mc_dev && obj_desc->region_count != 0) {
		error = fsl_mc_device_get_mmio_regions(mc_dev,
						       parent_mc_dev);
		if (error < 0)
			goto error_cleanup_dev;
	}

	 
	error = device_add(&mc_dev->dev);
	if (error < 0) {
		dev_err(parent_dev,
			"device_add() failed for device %s: %d\n",
			dev_name(&mc_dev->dev), error);
		goto error_cleanup_dev;
	}

	dev_dbg(parent_dev, "added %s\n", dev_name(&mc_dev->dev));

	*new_mc_dev = mc_dev;
	return 0;

error_cleanup_dev:
	kfree(mc_dev->regions);
	kfree(mc_bus);
	kfree(mc_dev);

	return error;
}
EXPORT_SYMBOL_GPL(fsl_mc_device_add);

static struct notifier_block fsl_mc_nb;

 
void fsl_mc_device_remove(struct fsl_mc_device *mc_dev)
{
	kfree(mc_dev->driver_override);
	mc_dev->driver_override = NULL;

	 
	device_del(&mc_dev->dev);
	put_device(&mc_dev->dev);
}
EXPORT_SYMBOL_GPL(fsl_mc_device_remove);

struct fsl_mc_device *fsl_mc_get_endpoint(struct fsl_mc_device *mc_dev,
					  u16 if_id)
{
	struct fsl_mc_device *mc_bus_dev, *endpoint;
	struct fsl_mc_obj_desc endpoint_desc = {{ 0 }};
	struct dprc_endpoint endpoint1 = {{ 0 }};
	struct dprc_endpoint endpoint2 = {{ 0 }};
	int state, err;

	mc_bus_dev = to_fsl_mc_device(mc_dev->dev.parent);
	strcpy(endpoint1.type, mc_dev->obj_desc.type);
	endpoint1.id = mc_dev->obj_desc.id;
	endpoint1.if_id = if_id;

	err = dprc_get_connection(mc_bus_dev->mc_io, 0,
				  mc_bus_dev->mc_handle,
				  &endpoint1, &endpoint2,
				  &state);

	if (err == -ENOTCONN || state == -1)
		return ERR_PTR(-ENOTCONN);

	if (err < 0) {
		dev_err(&mc_bus_dev->dev, "dprc_get_connection() = %d\n", err);
		return ERR_PTR(err);
	}

	strcpy(endpoint_desc.type, endpoint2.type);
	endpoint_desc.id = endpoint2.id;
	endpoint = fsl_mc_device_lookup(&endpoint_desc, mc_bus_dev);

	 
	if (!endpoint) {
		struct fsl_mc_bus *mc_bus = to_fsl_mc_bus(mc_bus_dev);

		if (mutex_trylock(&mc_bus->scan_mutex)) {
			err = dprc_scan_objects(mc_bus_dev, true);
			mutex_unlock(&mc_bus->scan_mutex);
		}

		if (err < 0)
			return ERR_PTR(err);
	}

	endpoint = fsl_mc_device_lookup(&endpoint_desc, mc_bus_dev);
	 
	if (!endpoint)
		return ERR_PTR(-EPERM);

	return endpoint;
}
EXPORT_SYMBOL_GPL(fsl_mc_get_endpoint);

static int get_mc_addr_translation_ranges(struct device *dev,
					  struct fsl_mc_addr_translation_range
						**ranges,
					  u8 *num_ranges)
{
	struct fsl_mc_addr_translation_range *r;
	struct of_range_parser parser;
	struct of_range range;

	of_range_parser_init(&parser, dev->of_node);
	*num_ranges = of_range_count(&parser);
	if (!*num_ranges) {
		 
		*ranges = NULL;
		return 0;
	}

	*ranges = devm_kcalloc(dev, *num_ranges,
			       sizeof(struct fsl_mc_addr_translation_range),
			       GFP_KERNEL);
	if (!(*ranges))
		return -ENOMEM;

	r = *ranges;
	for_each_of_range(&parser, &range) {
		r->mc_region_type = range.flags;
		r->start_mc_offset = range.bus_addr;
		r->end_mc_offset = range.bus_addr + range.size;
		r->start_phys_addr = range.cpu_addr;
		r++;
	}

	return 0;
}

 
static int fsl_mc_bus_probe(struct platform_device *pdev)
{
	struct fsl_mc_obj_desc obj_desc;
	int error;
	struct fsl_mc *mc;
	struct fsl_mc_device *mc_bus_dev = NULL;
	struct fsl_mc_io *mc_io = NULL;
	int container_id;
	phys_addr_t mc_portal_phys_addr;
	u32 mc_portal_size, mc_stream_id;
	struct resource *plat_res;

	mc = devm_kzalloc(&pdev->dev, sizeof(*mc), GFP_KERNEL);
	if (!mc)
		return -ENOMEM;

	platform_set_drvdata(pdev, mc);

	plat_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (plat_res) {
		mc->fsl_mc_regs = devm_ioremap_resource(&pdev->dev, plat_res);
		if (IS_ERR(mc->fsl_mc_regs))
			return PTR_ERR(mc->fsl_mc_regs);
	}

	if (mc->fsl_mc_regs) {
		if (IS_ENABLED(CONFIG_ACPI) && !dev_of_node(&pdev->dev)) {
			mc_stream_id = readl(mc->fsl_mc_regs + FSL_MC_FAPR);
			 
			mc_stream_id = (mc_stream_id & 0xffff) |
				((mc_stream_id & (MC_FAPR_PL | MC_FAPR_BMT)) ?
					BIT(14) : 0);
			error = acpi_dma_configure_id(&pdev->dev,
						      DEV_DMA_COHERENT,
						      &mc_stream_id);
			if (error == -EPROBE_DEFER)
				return error;
			if (error)
				dev_warn(&pdev->dev,
					 "failed to configure dma: %d.\n",
					 error);
		}

		 
		writel(readl(mc->fsl_mc_regs + FSL_MC_GCR1) &
			     (~(GCR1_P1_STOP | GCR1_P2_STOP)),
		       mc->fsl_mc_regs + FSL_MC_GCR1);
	}

	 
	plat_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mc_portal_phys_addr = plat_res->start;
	mc_portal_size = resource_size(plat_res);
	mc_portal_base_phys_addr = mc_portal_phys_addr & ~0x3ffffff;

	error = fsl_create_mc_io(&pdev->dev, mc_portal_phys_addr,
				 mc_portal_size, NULL,
				 FSL_MC_IO_ATOMIC_CONTEXT_PORTAL, &mc_io);
	if (error < 0)
		return error;

	error = mc_get_version(mc_io, 0, &mc_version);
	if (error != 0) {
		dev_err(&pdev->dev,
			"mc_get_version() failed with error %d\n", error);
		goto error_cleanup_mc_io;
	}

	dev_info(&pdev->dev, "MC firmware version: %u.%u.%u\n",
		 mc_version.major, mc_version.minor, mc_version.revision);

	if (dev_of_node(&pdev->dev)) {
		error = get_mc_addr_translation_ranges(&pdev->dev,
						&mc->translation_ranges,
						&mc->num_translation_ranges);
		if (error < 0)
			goto error_cleanup_mc_io;
	}

	error = dprc_get_container_id(mc_io, 0, &container_id);
	if (error < 0) {
		dev_err(&pdev->dev,
			"dprc_get_container_id() failed: %d\n", error);
		goto error_cleanup_mc_io;
	}

	memset(&obj_desc, 0, sizeof(struct fsl_mc_obj_desc));
	error = dprc_get_api_version(mc_io, 0,
				     &obj_desc.ver_major,
				     &obj_desc.ver_minor);
	if (error < 0)
		goto error_cleanup_mc_io;

	obj_desc.vendor = FSL_MC_VENDOR_FREESCALE;
	strcpy(obj_desc.type, "dprc");
	obj_desc.id = container_id;
	obj_desc.irq_count = 1;
	obj_desc.region_count = 0;

	error = fsl_mc_device_add(&obj_desc, mc_io, &pdev->dev, &mc_bus_dev);
	if (error < 0)
		goto error_cleanup_mc_io;

	mc->root_mc_bus_dev = mc_bus_dev;
	mc_bus_dev->dev.fwnode = pdev->dev.fwnode;
	return 0;

error_cleanup_mc_io:
	fsl_destroy_mc_io(mc_io);
	return error;
}

 
static int fsl_mc_bus_remove(struct platform_device *pdev)
{
	struct fsl_mc *mc = platform_get_drvdata(pdev);
	struct fsl_mc_io *mc_io;

	if (!fsl_mc_is_root_dprc(&mc->root_mc_bus_dev->dev))
		return -EINVAL;

	mc_io = mc->root_mc_bus_dev->mc_io;
	fsl_mc_device_remove(mc->root_mc_bus_dev);
	fsl_destroy_mc_io(mc_io);

	bus_unregister_notifier(&fsl_mc_bus_type, &fsl_mc_nb);

	if (mc->fsl_mc_regs) {
		 
		writel(readl(mc->fsl_mc_regs + FSL_MC_GCR1) |
		       (GCR1_P1_STOP | GCR1_P2_STOP),
		       mc->fsl_mc_regs + FSL_MC_GCR1);
	}

	return 0;
}

static void fsl_mc_bus_shutdown(struct platform_device *pdev)
{
	fsl_mc_bus_remove(pdev);
}

static const struct of_device_id fsl_mc_bus_match_table[] = {
	{.compatible = "fsl,qoriq-mc",},
	{},
};

MODULE_DEVICE_TABLE(of, fsl_mc_bus_match_table);

static const struct acpi_device_id fsl_mc_bus_acpi_match_table[] = {
	{"NXP0008", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, fsl_mc_bus_acpi_match_table);

static struct platform_driver fsl_mc_bus_driver = {
	.driver = {
		   .name = "fsl_mc_bus",
		   .pm = NULL,
		   .of_match_table = fsl_mc_bus_match_table,
		   .acpi_match_table = fsl_mc_bus_acpi_match_table,
		   },
	.probe = fsl_mc_bus_probe,
	.remove = fsl_mc_bus_remove,
	.shutdown = fsl_mc_bus_shutdown,
};

static int fsl_mc_bus_notifier(struct notifier_block *nb,
			       unsigned long action, void *data)
{
	struct device *dev = data;
	struct resource *res;
	void __iomem *fsl_mc_regs;

	if (action != BUS_NOTIFY_ADD_DEVICE)
		return 0;

	if (!of_match_device(fsl_mc_bus_match_table, dev) &&
	    !acpi_match_device(fsl_mc_bus_acpi_match_table, dev))
		return 0;

	res = platform_get_resource(to_platform_device(dev), IORESOURCE_MEM, 1);
	if (!res)
		return 0;

	fsl_mc_regs = ioremap(res->start, resource_size(res));
	if (!fsl_mc_regs)
		return 0;

	 
	writel(readl(fsl_mc_regs + FSL_MC_GCR1) | (GCR1_P1_STOP | GCR1_P2_STOP),
	       fsl_mc_regs + FSL_MC_GCR1);
	iounmap(fsl_mc_regs);

	return 0;
}

static struct notifier_block fsl_mc_nb = {
	.notifier_call = fsl_mc_bus_notifier,
};

static int __init fsl_mc_bus_driver_init(void)
{
	int error;

	error = bus_register(&fsl_mc_bus_type);
	if (error < 0) {
		pr_err("bus type registration failed: %d\n", error);
		goto error_cleanup_cache;
	}

	error = platform_driver_register(&fsl_mc_bus_driver);
	if (error < 0) {
		pr_err("platform_driver_register() failed: %d\n", error);
		goto error_cleanup_bus;
	}

	error = dprc_driver_init();
	if (error < 0)
		goto error_cleanup_driver;

	error = fsl_mc_allocator_driver_init();
	if (error < 0)
		goto error_cleanup_dprc_driver;

	return bus_register_notifier(&platform_bus_type, &fsl_mc_nb);

error_cleanup_dprc_driver:
	dprc_driver_exit();

error_cleanup_driver:
	platform_driver_unregister(&fsl_mc_bus_driver);

error_cleanup_bus:
	bus_unregister(&fsl_mc_bus_type);

error_cleanup_cache:
	return error;
}
postcore_initcall(fsl_mc_bus_driver_init);
