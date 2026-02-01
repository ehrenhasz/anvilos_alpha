
 

#include <linux/io.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/omap-mailbox.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>
#include <linux/reset.h>
#include <linux/slab.h>

#include "omap_remoteproc.h"
#include "remoteproc_internal.h"
#include "ti_sci_proc.h"

#define KEYSTONE_RPROC_LOCAL_ADDRESS_MASK	(SZ_16M - 1)

 
struct k3_dsp_mem {
	void __iomem *cpu_addr;
	phys_addr_t bus_addr;
	u32 dev_addr;
	size_t size;
};

 
struct k3_dsp_mem_data {
	const char *name;
	const u32 dev_addr;
};

 
struct k3_dsp_dev_data {
	const struct k3_dsp_mem_data *mems;
	u32 num_mems;
	u32 boot_align_addr;
	bool uses_lreset;
};

 
struct k3_dsp_rproc {
	struct device *dev;
	struct rproc *rproc;
	struct k3_dsp_mem *mem;
	int num_mems;
	struct k3_dsp_mem *rmem;
	int num_rmems;
	struct reset_control *reset;
	const struct k3_dsp_dev_data *data;
	struct ti_sci_proc *tsp;
	const struct ti_sci_handle *ti_sci;
	u32 ti_sci_id;
	struct mbox_chan *mbox;
	struct mbox_client client;
};

 
static void k3_dsp_rproc_mbox_callback(struct mbox_client *client, void *data)
{
	struct k3_dsp_rproc *kproc = container_of(client, struct k3_dsp_rproc,
						  client);
	struct device *dev = kproc->rproc->dev.parent;
	const char *name = kproc->rproc->name;
	u32 msg = omap_mbox_message(data);

	dev_dbg(dev, "mbox msg: 0x%x\n", msg);

	switch (msg) {
	case RP_MBOX_CRASH:
		 
		dev_err(dev, "K3 DSP rproc %s crashed\n", name);
		break;
	case RP_MBOX_ECHO_REPLY:
		dev_info(dev, "received echo reply from %s\n", name);
		break;
	default:
		 
		if (msg >= RP_MBOX_READY && msg < RP_MBOX_END_MSG)
			return;
		if (msg > kproc->rproc->max_notifyid) {
			dev_dbg(dev, "dropping unknown message 0x%x", msg);
			return;
		}
		 
		if (rproc_vq_interrupt(kproc->rproc, msg) == IRQ_NONE)
			dev_dbg(dev, "no message was found in vqid %d\n", msg);
	}
}

 
static void k3_dsp_rproc_kick(struct rproc *rproc, int vqid)
{
	struct k3_dsp_rproc *kproc = rproc->priv;
	struct device *dev = rproc->dev.parent;
	mbox_msg_t msg = (mbox_msg_t)vqid;
	int ret;

	 
	ret = mbox_send_message(kproc->mbox, (void *)msg);
	if (ret < 0)
		dev_err(dev, "failed to send mailbox message, status = %d\n",
			ret);
}

 
static int k3_dsp_rproc_reset(struct k3_dsp_rproc *kproc)
{
	struct device *dev = kproc->dev;
	int ret;

	ret = reset_control_assert(kproc->reset);
	if (ret) {
		dev_err(dev, "local-reset assert failed, ret = %d\n", ret);
		return ret;
	}

	if (kproc->data->uses_lreset)
		return ret;

	ret = kproc->ti_sci->ops.dev_ops.put_device(kproc->ti_sci,
						    kproc->ti_sci_id);
	if (ret) {
		dev_err(dev, "module-reset assert failed, ret = %d\n", ret);
		if (reset_control_deassert(kproc->reset))
			dev_warn(dev, "local-reset deassert back failed\n");
	}

	return ret;
}

 
static int k3_dsp_rproc_release(struct k3_dsp_rproc *kproc)
{
	struct device *dev = kproc->dev;
	int ret;

	if (kproc->data->uses_lreset)
		goto lreset;

	ret = kproc->ti_sci->ops.dev_ops.get_device(kproc->ti_sci,
						    kproc->ti_sci_id);
	if (ret) {
		dev_err(dev, "module-reset deassert failed, ret = %d\n", ret);
		return ret;
	}

lreset:
	ret = reset_control_deassert(kproc->reset);
	if (ret) {
		dev_err(dev, "local-reset deassert failed, ret = %d\n", ret);
		if (kproc->ti_sci->ops.dev_ops.put_device(kproc->ti_sci,
							  kproc->ti_sci_id))
			dev_warn(dev, "module-reset assert back failed\n");
	}

	return ret;
}

static int k3_dsp_rproc_request_mbox(struct rproc *rproc)
{
	struct k3_dsp_rproc *kproc = rproc->priv;
	struct mbox_client *client = &kproc->client;
	struct device *dev = kproc->dev;
	int ret;

	client->dev = dev;
	client->tx_done = NULL;
	client->rx_callback = k3_dsp_rproc_mbox_callback;
	client->tx_block = false;
	client->knows_txdone = false;

	kproc->mbox = mbox_request_channel(client, 0);
	if (IS_ERR(kproc->mbox)) {
		ret = -EBUSY;
		dev_err(dev, "mbox_request_channel failed: %ld\n",
			PTR_ERR(kproc->mbox));
		return ret;
	}

	 
	ret = mbox_send_message(kproc->mbox, (void *)RP_MBOX_ECHO_REQUEST);
	if (ret < 0) {
		dev_err(dev, "mbox_send_message failed: %d\n", ret);
		mbox_free_channel(kproc->mbox);
		return ret;
	}

	return 0;
}
 
static int k3_dsp_rproc_prepare(struct rproc *rproc)
{
	struct k3_dsp_rproc *kproc = rproc->priv;
	struct device *dev = kproc->dev;
	int ret;

	ret = kproc->ti_sci->ops.dev_ops.get_device(kproc->ti_sci,
						    kproc->ti_sci_id);
	if (ret)
		dev_err(dev, "module-reset deassert failed, cannot enable internal RAM loading, ret = %d\n",
			ret);

	return ret;
}

 
static int k3_dsp_rproc_unprepare(struct rproc *rproc)
{
	struct k3_dsp_rproc *kproc = rproc->priv;
	struct device *dev = kproc->dev;
	int ret;

	ret = kproc->ti_sci->ops.dev_ops.put_device(kproc->ti_sci,
						    kproc->ti_sci_id);
	if (ret)
		dev_err(dev, "module-reset assert failed, ret = %d\n", ret);

	return ret;
}

 
static int k3_dsp_rproc_start(struct rproc *rproc)
{
	struct k3_dsp_rproc *kproc = rproc->priv;
	struct device *dev = kproc->dev;
	u32 boot_addr;
	int ret;

	ret = k3_dsp_rproc_request_mbox(rproc);
	if (ret)
		return ret;

	boot_addr = rproc->bootaddr;
	if (boot_addr & (kproc->data->boot_align_addr - 1)) {
		dev_err(dev, "invalid boot address 0x%x, must be aligned on a 0x%x boundary\n",
			boot_addr, kproc->data->boot_align_addr);
		ret = -EINVAL;
		goto put_mbox;
	}

	dev_err(dev, "booting DSP core using boot addr = 0x%x\n", boot_addr);
	ret = ti_sci_proc_set_config(kproc->tsp, boot_addr, 0, 0);
	if (ret)
		goto put_mbox;

	ret = k3_dsp_rproc_release(kproc);
	if (ret)
		goto put_mbox;

	return 0;

put_mbox:
	mbox_free_channel(kproc->mbox);
	return ret;
}

 
static int k3_dsp_rproc_stop(struct rproc *rproc)
{
	struct k3_dsp_rproc *kproc = rproc->priv;

	mbox_free_channel(kproc->mbox);

	k3_dsp_rproc_reset(kproc);

	return 0;
}

 
static int k3_dsp_rproc_attach(struct rproc *rproc)
{
	struct k3_dsp_rproc *kproc = rproc->priv;
	struct device *dev = kproc->dev;
	int ret;

	ret = k3_dsp_rproc_request_mbox(rproc);
	if (ret)
		return ret;

	dev_info(dev, "DSP initialized in IPC-only mode\n");
	return 0;
}

 
static int k3_dsp_rproc_detach(struct rproc *rproc)
{
	struct k3_dsp_rproc *kproc = rproc->priv;
	struct device *dev = kproc->dev;

	mbox_free_channel(kproc->mbox);
	dev_info(dev, "DSP deinitialized in IPC-only mode\n");
	return 0;
}

 
static struct resource_table *k3_dsp_get_loaded_rsc_table(struct rproc *rproc,
							  size_t *rsc_table_sz)
{
	struct k3_dsp_rproc *kproc = rproc->priv;
	struct device *dev = kproc->dev;

	if (!kproc->rmem[0].cpu_addr) {
		dev_err(dev, "memory-region #1 does not exist, loaded rsc table can't be found");
		return ERR_PTR(-ENOMEM);
	}

	 
	*rsc_table_sz = 256;
	return (struct resource_table *)kproc->rmem[0].cpu_addr;
}

 
static void *k3_dsp_rproc_da_to_va(struct rproc *rproc, u64 da, size_t len, bool *is_iomem)
{
	struct k3_dsp_rproc *kproc = rproc->priv;
	void __iomem *va = NULL;
	phys_addr_t bus_addr;
	u32 dev_addr, offset;
	size_t size;
	int i;

	if (len == 0)
		return NULL;

	for (i = 0; i < kproc->num_mems; i++) {
		bus_addr = kproc->mem[i].bus_addr;
		dev_addr = kproc->mem[i].dev_addr;
		size = kproc->mem[i].size;

		if (da < KEYSTONE_RPROC_LOCAL_ADDRESS_MASK) {
			 
			if (da >= dev_addr &&
			    ((da + len) <= (dev_addr + size))) {
				offset = da - dev_addr;
				va = kproc->mem[i].cpu_addr + offset;
				return (__force void *)va;
			}
		} else {
			 
			if (da >= bus_addr &&
			    (da + len) <= (bus_addr + size)) {
				offset = da - bus_addr;
				va = kproc->mem[i].cpu_addr + offset;
				return (__force void *)va;
			}
		}
	}

	 
	for (i = 0; i < kproc->num_rmems; i++) {
		dev_addr = kproc->rmem[i].dev_addr;
		size = kproc->rmem[i].size;

		if (da >= dev_addr && ((da + len) <= (dev_addr + size))) {
			offset = da - dev_addr;
			va = kproc->rmem[i].cpu_addr + offset;
			return (__force void *)va;
		}
	}

	return NULL;
}

static const struct rproc_ops k3_dsp_rproc_ops = {
	.start		= k3_dsp_rproc_start,
	.stop		= k3_dsp_rproc_stop,
	.kick		= k3_dsp_rproc_kick,
	.da_to_va	= k3_dsp_rproc_da_to_va,
};

static int k3_dsp_rproc_of_get_memories(struct platform_device *pdev,
					struct k3_dsp_rproc *kproc)
{
	const struct k3_dsp_dev_data *data = kproc->data;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int num_mems = 0;
	int i;

	num_mems = kproc->data->num_mems;
	kproc->mem = devm_kcalloc(kproc->dev, num_mems,
				  sizeof(*kproc->mem), GFP_KERNEL);
	if (!kproc->mem)
		return -ENOMEM;

	for (i = 0; i < num_mems; i++) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   data->mems[i].name);
		if (!res) {
			dev_err(dev, "found no memory resource for %s\n",
				data->mems[i].name);
			return -EINVAL;
		}
		if (!devm_request_mem_region(dev, res->start,
					     resource_size(res),
					     dev_name(dev))) {
			dev_err(dev, "could not request %s region for resource\n",
				data->mems[i].name);
			return -EBUSY;
		}

		kproc->mem[i].cpu_addr = devm_ioremap_wc(dev, res->start,
							 resource_size(res));
		if (!kproc->mem[i].cpu_addr) {
			dev_err(dev, "failed to map %s memory\n",
				data->mems[i].name);
			return -ENOMEM;
		}
		kproc->mem[i].bus_addr = res->start;
		kproc->mem[i].dev_addr = data->mems[i].dev_addr;
		kproc->mem[i].size = resource_size(res);

		dev_dbg(dev, "memory %8s: bus addr %pa size 0x%zx va %pK da 0x%x\n",
			data->mems[i].name, &kproc->mem[i].bus_addr,
			kproc->mem[i].size, kproc->mem[i].cpu_addr,
			kproc->mem[i].dev_addr);
	}
	kproc->num_mems = num_mems;

	return 0;
}

static int k3_dsp_reserved_mem_init(struct k3_dsp_rproc *kproc)
{
	struct device *dev = kproc->dev;
	struct device_node *np = dev->of_node;
	struct device_node *rmem_np;
	struct reserved_mem *rmem;
	int num_rmems;
	int ret, i;

	num_rmems = of_property_count_elems_of_size(np, "memory-region",
						    sizeof(phandle));
	if (num_rmems <= 0) {
		dev_err(dev, "device does not reserved memory regions, ret = %d\n",
			num_rmems);
		return -EINVAL;
	}
	if (num_rmems < 2) {
		dev_err(dev, "device needs at least two memory regions to be defined, num = %d\n",
			num_rmems);
		return -EINVAL;
	}

	 
	ret = of_reserved_mem_device_init_by_idx(dev, np, 0);
	if (ret) {
		dev_err(dev, "device cannot initialize DMA pool, ret = %d\n",
			ret);
		return ret;
	}

	num_rmems--;
	kproc->rmem = kcalloc(num_rmems, sizeof(*kproc->rmem), GFP_KERNEL);
	if (!kproc->rmem) {
		ret = -ENOMEM;
		goto release_rmem;
	}

	 
	for (i = 0; i < num_rmems; i++) {
		rmem_np = of_parse_phandle(np, "memory-region", i + 1);
		if (!rmem_np) {
			ret = -EINVAL;
			goto unmap_rmem;
		}

		rmem = of_reserved_mem_lookup(rmem_np);
		if (!rmem) {
			of_node_put(rmem_np);
			ret = -EINVAL;
			goto unmap_rmem;
		}
		of_node_put(rmem_np);

		kproc->rmem[i].bus_addr = rmem->base;
		 
		kproc->rmem[i].dev_addr = (u32)rmem->base;
		kproc->rmem[i].size = rmem->size;
		kproc->rmem[i].cpu_addr = ioremap_wc(rmem->base, rmem->size);
		if (!kproc->rmem[i].cpu_addr) {
			dev_err(dev, "failed to map reserved memory#%d at %pa of size %pa\n",
				i + 1, &rmem->base, &rmem->size);
			ret = -ENOMEM;
			goto unmap_rmem;
		}

		dev_dbg(dev, "reserved memory%d: bus addr %pa size 0x%zx va %pK da 0x%x\n",
			i + 1, &kproc->rmem[i].bus_addr,
			kproc->rmem[i].size, kproc->rmem[i].cpu_addr,
			kproc->rmem[i].dev_addr);
	}
	kproc->num_rmems = num_rmems;

	return 0;

unmap_rmem:
	for (i--; i >= 0; i--)
		iounmap(kproc->rmem[i].cpu_addr);
	kfree(kproc->rmem);
release_rmem:
	of_reserved_mem_device_release(kproc->dev);
	return ret;
}

static void k3_dsp_reserved_mem_exit(struct k3_dsp_rproc *kproc)
{
	int i;

	for (i = 0; i < kproc->num_rmems; i++)
		iounmap(kproc->rmem[i].cpu_addr);
	kfree(kproc->rmem);

	of_reserved_mem_device_release(kproc->dev);
}

static
struct ti_sci_proc *k3_dsp_rproc_of_get_tsp(struct device *dev,
					    const struct ti_sci_handle *sci)
{
	struct ti_sci_proc *tsp;
	u32 temp[2];
	int ret;

	ret = of_property_read_u32_array(dev->of_node, "ti,sci-proc-ids",
					 temp, 2);
	if (ret < 0)
		return ERR_PTR(ret);

	tsp = kzalloc(sizeof(*tsp), GFP_KERNEL);
	if (!tsp)
		return ERR_PTR(-ENOMEM);

	tsp->dev = dev;
	tsp->sci = sci;
	tsp->ops = &sci->ops.proc_ops;
	tsp->proc_id = temp[0];
	tsp->host_id = temp[1];

	return tsp;
}

static int k3_dsp_rproc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	const struct k3_dsp_dev_data *data;
	struct k3_dsp_rproc *kproc;
	struct rproc *rproc;
	const char *fw_name;
	bool p_state = false;
	int ret = 0;
	int ret1;

	data = of_device_get_match_data(dev);
	if (!data)
		return -ENODEV;

	ret = rproc_of_parse_firmware(dev, 0, &fw_name);
	if (ret) {
		dev_err(dev, "failed to parse firmware-name property, ret = %d\n",
			ret);
		return ret;
	}

	rproc = rproc_alloc(dev, dev_name(dev), &k3_dsp_rproc_ops, fw_name,
			    sizeof(*kproc));
	if (!rproc)
		return -ENOMEM;

	rproc->has_iommu = false;
	rproc->recovery_disabled = true;
	if (data->uses_lreset) {
		rproc->ops->prepare = k3_dsp_rproc_prepare;
		rproc->ops->unprepare = k3_dsp_rproc_unprepare;
	}
	kproc = rproc->priv;
	kproc->rproc = rproc;
	kproc->dev = dev;
	kproc->data = data;

	kproc->ti_sci = ti_sci_get_by_phandle(np, "ti,sci");
	if (IS_ERR(kproc->ti_sci)) {
		ret = PTR_ERR(kproc->ti_sci);
		if (ret != -EPROBE_DEFER) {
			dev_err(dev, "failed to get ti-sci handle, ret = %d\n",
				ret);
		}
		kproc->ti_sci = NULL;
		goto free_rproc;
	}

	ret = of_property_read_u32(np, "ti,sci-dev-id", &kproc->ti_sci_id);
	if (ret) {
		dev_err(dev, "missing 'ti,sci-dev-id' property\n");
		goto put_sci;
	}

	kproc->reset = devm_reset_control_get_exclusive(dev, NULL);
	if (IS_ERR(kproc->reset)) {
		ret = PTR_ERR(kproc->reset);
		dev_err(dev, "failed to get reset, status = %d\n", ret);
		goto put_sci;
	}

	kproc->tsp = k3_dsp_rproc_of_get_tsp(dev, kproc->ti_sci);
	if (IS_ERR(kproc->tsp)) {
		dev_err(dev, "failed to construct ti-sci proc control, ret = %d\n",
			ret);
		ret = PTR_ERR(kproc->tsp);
		goto put_sci;
	}

	ret = ti_sci_proc_request(kproc->tsp);
	if (ret < 0) {
		dev_err(dev, "ti_sci_proc_request failed, ret = %d\n", ret);
		goto free_tsp;
	}

	ret = k3_dsp_rproc_of_get_memories(pdev, kproc);
	if (ret)
		goto release_tsp;

	ret = k3_dsp_reserved_mem_init(kproc);
	if (ret) {
		dev_err(dev, "reserved memory init failed, ret = %d\n", ret);
		goto release_tsp;
	}

	ret = kproc->ti_sci->ops.dev_ops.is_on(kproc->ti_sci, kproc->ti_sci_id,
					       NULL, &p_state);
	if (ret) {
		dev_err(dev, "failed to get initial state, mode cannot be determined, ret = %d\n",
			ret);
		goto release_mem;
	}

	 
	if (p_state) {
		dev_info(dev, "configured DSP for IPC-only mode\n");
		rproc->state = RPROC_DETACHED;
		 
		rproc->ops->prepare = NULL;
		rproc->ops->unprepare = NULL;
		rproc->ops->start = NULL;
		rproc->ops->stop = NULL;
		rproc->ops->attach = k3_dsp_rproc_attach;
		rproc->ops->detach = k3_dsp_rproc_detach;
		rproc->ops->get_loaded_rsc_table = k3_dsp_get_loaded_rsc_table;
	} else {
		dev_info(dev, "configured DSP for remoteproc mode\n");
		 
		if (data->uses_lreset) {
			ret = reset_control_status(kproc->reset);
			if (ret < 0) {
				dev_err(dev, "failed to get reset status, status = %d\n",
					ret);
				goto release_mem;
			} else if (ret == 0) {
				dev_warn(dev, "local reset is deasserted for device\n");
				k3_dsp_rproc_reset(kproc);
			}
		}
	}

	ret = rproc_add(rproc);
	if (ret) {
		dev_err(dev, "failed to add register device with remoteproc core, status = %d\n",
			ret);
		goto release_mem;
	}

	platform_set_drvdata(pdev, kproc);

	return 0;

release_mem:
	k3_dsp_reserved_mem_exit(kproc);
release_tsp:
	ret1 = ti_sci_proc_release(kproc->tsp);
	if (ret1)
		dev_err(dev, "failed to release proc, ret = %d\n", ret1);
free_tsp:
	kfree(kproc->tsp);
put_sci:
	ret1 = ti_sci_put_handle(kproc->ti_sci);
	if (ret1)
		dev_err(dev, "failed to put ti_sci handle, ret = %d\n", ret1);
free_rproc:
	rproc_free(rproc);
	return ret;
}

static int k3_dsp_rproc_remove(struct platform_device *pdev)
{
	struct k3_dsp_rproc *kproc = platform_get_drvdata(pdev);
	struct rproc *rproc = kproc->rproc;
	struct device *dev = &pdev->dev;
	int ret;

	if (rproc->state == RPROC_ATTACHED) {
		ret = rproc_detach(rproc);
		if (ret) {
			dev_err(dev, "failed to detach proc, ret = %d\n", ret);
			return ret;
		}
	}

	rproc_del(kproc->rproc);

	ret = ti_sci_proc_release(kproc->tsp);
	if (ret)
		dev_err(dev, "failed to release proc, ret = %d\n", ret);

	kfree(kproc->tsp);

	ret = ti_sci_put_handle(kproc->ti_sci);
	if (ret)
		dev_err(dev, "failed to put ti_sci handle, ret = %d\n", ret);

	k3_dsp_reserved_mem_exit(kproc);
	rproc_free(kproc->rproc);

	return 0;
}

static const struct k3_dsp_mem_data c66_mems[] = {
	{ .name = "l2sram", .dev_addr = 0x800000 },
	{ .name = "l1pram", .dev_addr = 0xe00000 },
	{ .name = "l1dram", .dev_addr = 0xf00000 },
};

 
static const struct k3_dsp_mem_data c71_mems[] = {
	{ .name = "l2sram", .dev_addr = 0x800000 },
	{ .name = "l1dram", .dev_addr = 0xe00000 },
};

static const struct k3_dsp_mem_data c7xv_mems[] = {
	{ .name = "l2sram", .dev_addr = 0x800000 },
};

static const struct k3_dsp_dev_data c66_data = {
	.mems = c66_mems,
	.num_mems = ARRAY_SIZE(c66_mems),
	.boot_align_addr = SZ_1K,
	.uses_lreset = true,
};

static const struct k3_dsp_dev_data c71_data = {
	.mems = c71_mems,
	.num_mems = ARRAY_SIZE(c71_mems),
	.boot_align_addr = SZ_2M,
	.uses_lreset = false,
};

static const struct k3_dsp_dev_data c7xv_data = {
	.mems = c7xv_mems,
	.num_mems = ARRAY_SIZE(c7xv_mems),
	.boot_align_addr = SZ_2M,
	.uses_lreset = false,
};

static const struct of_device_id k3_dsp_of_match[] = {
	{ .compatible = "ti,j721e-c66-dsp", .data = &c66_data, },
	{ .compatible = "ti,j721e-c71-dsp", .data = &c71_data, },
	{ .compatible = "ti,j721s2-c71-dsp", .data = &c71_data, },
	{ .compatible = "ti,am62a-c7xv-dsp", .data = &c7xv_data, },
	{   },
};
MODULE_DEVICE_TABLE(of, k3_dsp_of_match);

static struct platform_driver k3_dsp_rproc_driver = {
	.probe	= k3_dsp_rproc_probe,
	.remove	= k3_dsp_rproc_remove,
	.driver	= {
		.name = "k3-dsp-rproc",
		.of_match_table = k3_dsp_of_match,
	},
};

module_platform_driver(k3_dsp_rproc_driver);

MODULE_AUTHOR("Suman Anna <s-anna@ti.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TI K3 DSP Remoteproc driver");
