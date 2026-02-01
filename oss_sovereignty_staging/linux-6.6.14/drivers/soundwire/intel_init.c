


 

#include <linux/acpi.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/auxiliary_bus.h>
#include <linux/pm_runtime.h>
#include <linux/soundwire/sdw_intel.h>
#include "cadence_master.h"
#include "intel.h"
#include "intel_auxdevice.h"

static void intel_link_dev_release(struct device *dev)
{
	struct auxiliary_device *auxdev = to_auxiliary_dev(dev);
	struct sdw_intel_link_dev *ldev = auxiliary_dev_to_sdw_intel_link_dev(auxdev);

	kfree(ldev);
}

 
static struct sdw_intel_link_dev *intel_link_dev_register(struct sdw_intel_res *res,
							  struct sdw_intel_ctx *ctx,
							  struct fwnode_handle *fwnode,
							  const char *name,
							  int link_id)
{
	struct sdw_intel_link_dev *ldev;
	struct sdw_intel_link_res *link;
	struct auxiliary_device *auxdev;
	int ret;

	ldev = kzalloc(sizeof(*ldev), GFP_KERNEL);
	if (!ldev)
		return ERR_PTR(-ENOMEM);

	auxdev = &ldev->auxdev;
	auxdev->name = name;
	auxdev->dev.parent = res->parent;
	auxdev->dev.fwnode = fwnode;
	auxdev->dev.release = intel_link_dev_release;

	 
	auxdev->id = link_id;

	 
	ctx->ldev[link_id] = ldev;

	 
	link = &ldev->link_res;
	link->hw_ops = res->hw_ops;
	link->mmio_base = res->mmio_base;
	if (!res->ext) {
		link->registers = res->mmio_base + SDW_LINK_BASE
			+ (SDW_LINK_SIZE * link_id);
		link->ip_offset = 0;
		link->shim = res->mmio_base + res->shim_base;
		link->alh = res->mmio_base + res->alh_base;
		link->shim_lock = &ctx->shim_lock;
	} else {
		link->registers = res->mmio_base + SDW_IP_BASE(link_id);
		link->ip_offset = SDW_CADENCE_MCP_IP_OFFSET;
		link->shim = res->mmio_base +  SDW_SHIM2_GENERIC_BASE(link_id);
		link->shim_vs = res->mmio_base + SDW_SHIM2_VS_BASE(link_id);
		link->shim_lock = res->eml_lock;
	}

	link->ops = res->ops;
	link->dev = res->dev;

	link->clock_stop_quirks = res->clock_stop_quirks;
	link->shim_mask = &ctx->shim_mask;
	link->link_mask = ctx->link_mask;

	link->hbus = res->hbus;

	 
	ret = auxiliary_device_init(auxdev);
	if (ret < 0) {
		dev_err(res->parent, "failed to initialize link dev %s link_id %d\n",
			name, link_id);
		kfree(ldev);
		return ERR_PTR(ret);
	}

	ret = auxiliary_device_add(&ldev->auxdev);
	if (ret < 0) {
		dev_err(res->parent, "failed to add link dev %s link_id %d\n",
			ldev->auxdev.name, link_id);
		 
		auxiliary_device_uninit(&ldev->auxdev);
		return ERR_PTR(ret);
	}

	return ldev;
}

static void intel_link_dev_unregister(struct sdw_intel_link_dev *ldev)
{
	auxiliary_device_delete(&ldev->auxdev);
	auxiliary_device_uninit(&ldev->auxdev);
}

static int sdw_intel_cleanup(struct sdw_intel_ctx *ctx)
{
	struct sdw_intel_link_dev *ldev;
	u32 link_mask;
	int i;

	link_mask = ctx->link_mask;

	for (i = 0; i < ctx->count; i++) {
		if (!(link_mask & BIT(i)))
			continue;

		ldev = ctx->ldev[i];

		pm_runtime_disable(&ldev->auxdev.dev);
		if (!ldev->link_res.clock_stop_quirks)
			pm_runtime_put_noidle(ldev->link_res.dev);

		intel_link_dev_unregister(ldev);
	}

	return 0;
}

irqreturn_t sdw_intel_thread(int irq, void *dev_id)
{
	struct sdw_intel_ctx *ctx = dev_id;
	struct sdw_intel_link_res *link;

	list_for_each_entry(link, &ctx->link_list, list)
		sdw_cdns_irq(irq, link->cdns);

	return IRQ_HANDLED;
}
EXPORT_SYMBOL_NS(sdw_intel_thread, SOUNDWIRE_INTEL_INIT);

static struct sdw_intel_ctx
*sdw_intel_probe_controller(struct sdw_intel_res *res)
{
	struct sdw_intel_link_res *link;
	struct sdw_intel_link_dev *ldev;
	struct sdw_intel_ctx *ctx;
	struct acpi_device *adev;
	struct sdw_slave *slave;
	struct list_head *node;
	struct sdw_bus *bus;
	u32 link_mask;
	int num_slaves = 0;
	int count;
	int i;

	if (!res)
		return NULL;

	adev = acpi_fetch_acpi_dev(res->handle);
	if (!adev)
		return NULL;

	if (!res->count)
		return NULL;

	count = res->count;
	dev_dbg(&adev->dev, "Creating %d SDW Link devices\n", count);

	 
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return NULL;

	ctx->count = count;

	 
	ctx->ldev = kcalloc(ctx->count, sizeof(*ctx->ldev), GFP_KERNEL);
	if (!ctx->ldev) {
		kfree(ctx);
		return NULL;
	}

	ctx->mmio_base = res->mmio_base;
	ctx->shim_base = res->shim_base;
	ctx->alh_base = res->alh_base;
	ctx->link_mask = res->link_mask;
	ctx->handle = res->handle;
	mutex_init(&ctx->shim_lock);

	link_mask = ctx->link_mask;

	INIT_LIST_HEAD(&ctx->link_list);

	for (i = 0; i < count; i++) {
		if (!(link_mask & BIT(i)))
			continue;

		 
		ldev = intel_link_dev_register(res,
					       ctx,
					       acpi_fwnode_handle(adev),
					       "link",
					       i);
		if (IS_ERR(ldev))
			goto err;

		link = &ldev->link_res;
		link->cdns = auxiliary_get_drvdata(&ldev->auxdev);

		if (!link->cdns) {
			dev_err(&adev->dev, "failed to get link->cdns\n");
			 
			i++;
			goto err;
		}
		list_add_tail(&link->list, &ctx->link_list);
		bus = &link->cdns->bus;
		 
		list_for_each(node, &bus->slaves)
			num_slaves++;
	}

	ctx->ids = kcalloc(num_slaves, sizeof(*ctx->ids), GFP_KERNEL);
	if (!ctx->ids)
		goto err;

	ctx->num_slaves = num_slaves;
	i = 0;
	list_for_each_entry(link, &ctx->link_list, list) {
		bus = &link->cdns->bus;
		list_for_each_entry(slave, &bus->slaves, node) {
			ctx->ids[i].id = slave->id;
			ctx->ids[i].link_id = bus->link_id;
			i++;
		}
	}

	return ctx;

err:
	while (i--) {
		if (!(link_mask & BIT(i)))
			continue;
		ldev = ctx->ldev[i];
		intel_link_dev_unregister(ldev);
	}
	kfree(ctx->ldev);
	kfree(ctx);
	return NULL;
}

static int
sdw_intel_startup_controller(struct sdw_intel_ctx *ctx)
{
	struct acpi_device *adev = acpi_fetch_acpi_dev(ctx->handle);
	struct sdw_intel_link_dev *ldev;
	u32 link_mask;
	int i;

	if (!adev)
		return -EINVAL;

	if (!ctx->ldev)
		return -EINVAL;

	link_mask = ctx->link_mask;

	 
	for (i = 0; i < ctx->count; i++) {
		if (!(link_mask & BIT(i)))
			continue;

		ldev = ctx->ldev[i];

		intel_link_startup(&ldev->auxdev);

		if (!ldev->link_res.clock_stop_quirks) {
			 
			pm_runtime_get_noresume(ldev->link_res.dev);
		}
	}

	return 0;
}

 
struct sdw_intel_ctx
*sdw_intel_probe(struct sdw_intel_res *res)
{
	return sdw_intel_probe_controller(res);
}
EXPORT_SYMBOL_NS(sdw_intel_probe, SOUNDWIRE_INTEL_INIT);

 
int sdw_intel_startup(struct sdw_intel_ctx *ctx)
{
	return sdw_intel_startup_controller(ctx);
}
EXPORT_SYMBOL_NS(sdw_intel_startup, SOUNDWIRE_INTEL_INIT);
 
void sdw_intel_exit(struct sdw_intel_ctx *ctx)
{
	sdw_intel_cleanup(ctx);
	kfree(ctx->ids);
	kfree(ctx->ldev);
	kfree(ctx);
}
EXPORT_SYMBOL_NS(sdw_intel_exit, SOUNDWIRE_INTEL_INIT);

void sdw_intel_process_wakeen_event(struct sdw_intel_ctx *ctx)
{
	struct sdw_intel_link_dev *ldev;
	u32 link_mask;
	int i;

	if (!ctx->ldev)
		return;

	link_mask = ctx->link_mask;

	 
	for (i = 0; i < ctx->count; i++) {
		if (!(link_mask & BIT(i)))
			continue;

		ldev = ctx->ldev[i];

		intel_link_process_wakeen_event(&ldev->auxdev);
	}
}
EXPORT_SYMBOL_NS(sdw_intel_process_wakeen_event, SOUNDWIRE_INTEL_INIT);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Intel Soundwire Init Library");
