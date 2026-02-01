
 

#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_platform.h>
#include <linux/omap-mailbox.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/reset.h>
#include <linux/slab.h>

#include "omap_remoteproc.h"
#include "remoteproc_internal.h"
#include "ti_sci_proc.h"

 
#define K3_R5_TCM_DEV_ADDR	0x41010000

 
#define PROC_BOOT_CFG_FLAG_R5_DBG_EN			0x00000001
#define PROC_BOOT_CFG_FLAG_R5_DBG_NIDEN			0x00000002
#define PROC_BOOT_CFG_FLAG_R5_LOCKSTEP			0x00000100
#define PROC_BOOT_CFG_FLAG_R5_TEINIT			0x00000200
#define PROC_BOOT_CFG_FLAG_R5_NMFI_EN			0x00000400
#define PROC_BOOT_CFG_FLAG_R5_TCM_RSTBASE		0x00000800
#define PROC_BOOT_CFG_FLAG_R5_BTCM_EN			0x00001000
#define PROC_BOOT_CFG_FLAG_R5_ATCM_EN			0x00002000
 
#define PROC_BOOT_CFG_FLAG_R5_MEM_INIT_DIS		0x00004000
 
#define PROC_BOOT_CFG_FLAG_R5_SINGLE_CORE		0x00008000

 
#define PROC_BOOT_CTRL_FLAG_R5_CORE_HALT		0x00000001

 
#define PROC_BOOT_STATUS_FLAG_R5_WFE			0x00000001
#define PROC_BOOT_STATUS_FLAG_R5_WFI			0x00000002
#define PROC_BOOT_STATUS_FLAG_R5_CLK_GATED		0x00000004
#define PROC_BOOT_STATUS_FLAG_R5_LOCKSTEP_PERMITTED	0x00000100
 
#define PROC_BOOT_STATUS_FLAG_R5_SINGLECORE_ONLY	0x00000200

 
struct k3_r5_mem {
	void __iomem *cpu_addr;
	phys_addr_t bus_addr;
	u32 dev_addr;
	size_t size;
};

 
enum cluster_mode {
	CLUSTER_MODE_SPLIT = 0,
	CLUSTER_MODE_LOCKSTEP,
	CLUSTER_MODE_SINGLECPU,
	CLUSTER_MODE_SINGLECORE
};

 
struct k3_r5_soc_data {
	bool tcm_is_double;
	bool tcm_ecc_autoinit;
	bool single_cpu_mode;
	bool is_single_core;
};

 
struct k3_r5_cluster {
	struct device *dev;
	enum cluster_mode mode;
	struct list_head cores;
	const struct k3_r5_soc_data *soc_data;
};

 
struct k3_r5_core {
	struct list_head elem;
	struct device *dev;
	struct rproc *rproc;
	struct k3_r5_mem *mem;
	struct k3_r5_mem *sram;
	int num_mems;
	int num_sram;
	struct reset_control *reset;
	struct ti_sci_proc *tsp;
	const struct ti_sci_handle *ti_sci;
	u32 ti_sci_id;
	u32 atcm_enable;
	u32 btcm_enable;
	u32 loczrama;
};

 
struct k3_r5_rproc {
	struct device *dev;
	struct k3_r5_cluster *cluster;
	struct mbox_chan *mbox;
	struct mbox_client client;
	struct rproc *rproc;
	struct k3_r5_core *core;
	struct k3_r5_mem *rmem;
	int num_rmems;
};

 
static void k3_r5_rproc_mbox_callback(struct mbox_client *client, void *data)
{
	struct k3_r5_rproc *kproc = container_of(client, struct k3_r5_rproc,
						client);
	struct device *dev = kproc->rproc->dev.parent;
	const char *name = kproc->rproc->name;
	u32 msg = omap_mbox_message(data);

	dev_dbg(dev, "mbox msg: 0x%x\n", msg);

	switch (msg) {
	case RP_MBOX_CRASH:
		 
		dev_err(dev, "K3 R5F rproc %s crashed\n", name);
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

 
static void k3_r5_rproc_kick(struct rproc *rproc, int vqid)
{
	struct k3_r5_rproc *kproc = rproc->priv;
	struct device *dev = rproc->dev.parent;
	mbox_msg_t msg = (mbox_msg_t)vqid;
	int ret;

	 
	ret = mbox_send_message(kproc->mbox, (void *)msg);
	if (ret < 0)
		dev_err(dev, "failed to send mailbox message, status = %d\n",
			ret);
}

static int k3_r5_split_reset(struct k3_r5_core *core)
{
	int ret;

	ret = reset_control_assert(core->reset);
	if (ret) {
		dev_err(core->dev, "local-reset assert failed, ret = %d\n",
			ret);
		return ret;
	}

	ret = core->ti_sci->ops.dev_ops.put_device(core->ti_sci,
						   core->ti_sci_id);
	if (ret) {
		dev_err(core->dev, "module-reset assert failed, ret = %d\n",
			ret);
		if (reset_control_deassert(core->reset))
			dev_warn(core->dev, "local-reset deassert back failed\n");
	}

	return ret;
}

static int k3_r5_split_release(struct k3_r5_core *core)
{
	int ret;

	ret = core->ti_sci->ops.dev_ops.get_device(core->ti_sci,
						   core->ti_sci_id);
	if (ret) {
		dev_err(core->dev, "module-reset deassert failed, ret = %d\n",
			ret);
		return ret;
	}

	ret = reset_control_deassert(core->reset);
	if (ret) {
		dev_err(core->dev, "local-reset deassert failed, ret = %d\n",
			ret);
		if (core->ti_sci->ops.dev_ops.put_device(core->ti_sci,
							 core->ti_sci_id))
			dev_warn(core->dev, "module-reset assert back failed\n");
	}

	return ret;
}

static int k3_r5_lockstep_reset(struct k3_r5_cluster *cluster)
{
	struct k3_r5_core *core;
	int ret;

	 
	list_for_each_entry(core, &cluster->cores, elem) {
		ret = reset_control_assert(core->reset);
		if (ret) {
			dev_err(core->dev, "local-reset assert failed, ret = %d\n",
				ret);
			core = list_prev_entry(core, elem);
			goto unroll_local_reset;
		}
	}

	 
	list_for_each_entry(core, &cluster->cores, elem) {
		ret = core->ti_sci->ops.dev_ops.put_device(core->ti_sci,
							   core->ti_sci_id);
		if (ret) {
			dev_err(core->dev, "module-reset assert failed, ret = %d\n",
				ret);
			goto unroll_module_reset;
		}
	}

	return 0;

unroll_module_reset:
	list_for_each_entry_continue_reverse(core, &cluster->cores, elem) {
		if (core->ti_sci->ops.dev_ops.put_device(core->ti_sci,
							 core->ti_sci_id))
			dev_warn(core->dev, "module-reset assert back failed\n");
	}
	core = list_last_entry(&cluster->cores, struct k3_r5_core, elem);
unroll_local_reset:
	list_for_each_entry_from_reverse(core, &cluster->cores, elem) {
		if (reset_control_deassert(core->reset))
			dev_warn(core->dev, "local-reset deassert back failed\n");
	}

	return ret;
}

static int k3_r5_lockstep_release(struct k3_r5_cluster *cluster)
{
	struct k3_r5_core *core;
	int ret;

	 
	list_for_each_entry_reverse(core, &cluster->cores, elem) {
		ret = core->ti_sci->ops.dev_ops.get_device(core->ti_sci,
							   core->ti_sci_id);
		if (ret) {
			dev_err(core->dev, "module-reset deassert failed, ret = %d\n",
				ret);
			core = list_next_entry(core, elem);
			goto unroll_module_reset;
		}
	}

	 
	list_for_each_entry_reverse(core, &cluster->cores, elem) {
		ret = reset_control_deassert(core->reset);
		if (ret) {
			dev_err(core->dev, "module-reset deassert failed, ret = %d\n",
				ret);
			goto unroll_local_reset;
		}
	}

	return 0;

unroll_local_reset:
	list_for_each_entry_continue(core, &cluster->cores, elem) {
		if (reset_control_assert(core->reset))
			dev_warn(core->dev, "local-reset assert back failed\n");
	}
	core = list_first_entry(&cluster->cores, struct k3_r5_core, elem);
unroll_module_reset:
	list_for_each_entry_from(core, &cluster->cores, elem) {
		if (core->ti_sci->ops.dev_ops.put_device(core->ti_sci,
							 core->ti_sci_id))
			dev_warn(core->dev, "module-reset assert back failed\n");
	}

	return ret;
}

static inline int k3_r5_core_halt(struct k3_r5_core *core)
{
	return ti_sci_proc_set_control(core->tsp,
				       PROC_BOOT_CTRL_FLAG_R5_CORE_HALT, 0);
}

static inline int k3_r5_core_run(struct k3_r5_core *core)
{
	return ti_sci_proc_set_control(core->tsp,
				       0, PROC_BOOT_CTRL_FLAG_R5_CORE_HALT);
}

static int k3_r5_rproc_request_mbox(struct rproc *rproc)
{
	struct k3_r5_rproc *kproc = rproc->priv;
	struct mbox_client *client = &kproc->client;
	struct device *dev = kproc->dev;
	int ret;

	client->dev = dev;
	client->tx_done = NULL;
	client->rx_callback = k3_r5_rproc_mbox_callback;
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

 
static int k3_r5_rproc_prepare(struct rproc *rproc)
{
	struct k3_r5_rproc *kproc = rproc->priv;
	struct k3_r5_cluster *cluster = kproc->cluster;
	struct k3_r5_core *core = kproc->core;
	struct device *dev = kproc->dev;
	u32 ctrl = 0, cfg = 0, stat = 0;
	u64 boot_vec = 0;
	bool mem_init_dis;
	int ret;

	ret = ti_sci_proc_get_status(core->tsp, &boot_vec, &cfg, &ctrl, &stat);
	if (ret < 0)
		return ret;
	mem_init_dis = !!(cfg & PROC_BOOT_CFG_FLAG_R5_MEM_INIT_DIS);

	 
	ret = (cluster->mode == CLUSTER_MODE_LOCKSTEP ||
	       cluster->mode == CLUSTER_MODE_SINGLECPU) ?
		k3_r5_lockstep_release(cluster) : k3_r5_split_release(core);
	if (ret) {
		dev_err(dev, "unable to enable cores for TCM loading, ret = %d\n",
			ret);
		return ret;
	}

	 
	if (cluster->soc_data->tcm_ecc_autoinit && !mem_init_dis) {
		dev_dbg(dev, "leveraging h/w init for TCM memories\n");
		return 0;
	}

	 
	dev_dbg(dev, "zeroing out ATCM memory\n");
	memset(core->mem[0].cpu_addr, 0x00, core->mem[0].size);

	dev_dbg(dev, "zeroing out BTCM memory\n");
	memset(core->mem[1].cpu_addr, 0x00, core->mem[1].size);

	return 0;
}

 
static int k3_r5_rproc_unprepare(struct rproc *rproc)
{
	struct k3_r5_rproc *kproc = rproc->priv;
	struct k3_r5_cluster *cluster = kproc->cluster;
	struct k3_r5_core *core = kproc->core;
	struct device *dev = kproc->dev;
	int ret;

	 
	ret = (cluster->mode == CLUSTER_MODE_LOCKSTEP ||
	       cluster->mode == CLUSTER_MODE_SINGLECPU) ?
		k3_r5_lockstep_reset(cluster) : k3_r5_split_reset(core);
	if (ret)
		dev_err(dev, "unable to disable cores, ret = %d\n", ret);

	return ret;
}

 
static int k3_r5_rproc_start(struct rproc *rproc)
{
	struct k3_r5_rproc *kproc = rproc->priv;
	struct k3_r5_cluster *cluster = kproc->cluster;
	struct device *dev = kproc->dev;
	struct k3_r5_core *core;
	u32 boot_addr;
	int ret;

	ret = k3_r5_rproc_request_mbox(rproc);
	if (ret)
		return ret;

	boot_addr = rproc->bootaddr;
	 
	dev_dbg(dev, "booting R5F core using boot addr = 0x%x\n", boot_addr);

	 
	core = kproc->core;
	ret = ti_sci_proc_set_config(core->tsp, boot_addr, 0, 0);
	if (ret)
		goto put_mbox;

	 
	if (cluster->mode == CLUSTER_MODE_LOCKSTEP) {
		list_for_each_entry_reverse(core, &cluster->cores, elem) {
			ret = k3_r5_core_run(core);
			if (ret)
				goto unroll_core_run;
		}
	} else {
		ret = k3_r5_core_run(core);
		if (ret)
			goto put_mbox;
	}

	return 0;

unroll_core_run:
	list_for_each_entry_continue(core, &cluster->cores, elem) {
		if (k3_r5_core_halt(core))
			dev_warn(core->dev, "core halt back failed\n");
	}
put_mbox:
	mbox_free_channel(kproc->mbox);
	return ret;
}

 
static int k3_r5_rproc_stop(struct rproc *rproc)
{
	struct k3_r5_rproc *kproc = rproc->priv;
	struct k3_r5_cluster *cluster = kproc->cluster;
	struct k3_r5_core *core = kproc->core;
	int ret;

	 
	if (cluster->mode == CLUSTER_MODE_LOCKSTEP) {
		list_for_each_entry(core, &cluster->cores, elem) {
			ret = k3_r5_core_halt(core);
			if (ret) {
				core = list_prev_entry(core, elem);
				goto unroll_core_halt;
			}
		}
	} else {
		ret = k3_r5_core_halt(core);
		if (ret)
			goto out;
	}

	mbox_free_channel(kproc->mbox);

	return 0;

unroll_core_halt:
	list_for_each_entry_from_reverse(core, &cluster->cores, elem) {
		if (k3_r5_core_run(core))
			dev_warn(core->dev, "core run back failed\n");
	}
out:
	return ret;
}

 
static int k3_r5_rproc_attach(struct rproc *rproc)
{
	struct k3_r5_rproc *kproc = rproc->priv;
	struct device *dev = kproc->dev;
	int ret;

	ret = k3_r5_rproc_request_mbox(rproc);
	if (ret)
		return ret;

	dev_info(dev, "R5F core initialized in IPC-only mode\n");
	return 0;
}

 
static int k3_r5_rproc_detach(struct rproc *rproc)
{
	struct k3_r5_rproc *kproc = rproc->priv;
	struct device *dev = kproc->dev;

	mbox_free_channel(kproc->mbox);
	dev_info(dev, "R5F core deinitialized in IPC-only mode\n");
	return 0;
}

 
static struct resource_table *k3_r5_get_loaded_rsc_table(struct rproc *rproc,
							 size_t *rsc_table_sz)
{
	struct k3_r5_rproc *kproc = rproc->priv;
	struct device *dev = kproc->dev;

	if (!kproc->rmem[0].cpu_addr) {
		dev_err(dev, "memory-region #1 does not exist, loaded rsc table can't be found");
		return ERR_PTR(-ENOMEM);
	}

	 
	*rsc_table_sz = 256;
	return (struct resource_table *)kproc->rmem[0].cpu_addr;
}

 
static void *k3_r5_rproc_da_to_va(struct rproc *rproc, u64 da, size_t len, bool *is_iomem)
{
	struct k3_r5_rproc *kproc = rproc->priv;
	struct k3_r5_core *core = kproc->core;
	void __iomem *va = NULL;
	phys_addr_t bus_addr;
	u32 dev_addr, offset;
	size_t size;
	int i;

	if (len == 0)
		return NULL;

	 
	for (i = 0; i < core->num_mems; i++) {
		bus_addr = core->mem[i].bus_addr;
		dev_addr = core->mem[i].dev_addr;
		size = core->mem[i].size;

		 
		if (da >= dev_addr && ((da + len) <= (dev_addr + size))) {
			offset = da - dev_addr;
			va = core->mem[i].cpu_addr + offset;
			return (__force void *)va;
		}

		 
		if (da >= bus_addr && ((da + len) <= (bus_addr + size))) {
			offset = da - bus_addr;
			va = core->mem[i].cpu_addr + offset;
			return (__force void *)va;
		}
	}

	 
	for (i = 0; i < core->num_sram; i++) {
		dev_addr = core->sram[i].dev_addr;
		size = core->sram[i].size;

		if (da >= dev_addr && ((da + len) <= (dev_addr + size))) {
			offset = da - dev_addr;
			va = core->sram[i].cpu_addr + offset;
			return (__force void *)va;
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

static const struct rproc_ops k3_r5_rproc_ops = {
	.prepare	= k3_r5_rproc_prepare,
	.unprepare	= k3_r5_rproc_unprepare,
	.start		= k3_r5_rproc_start,
	.stop		= k3_r5_rproc_stop,
	.kick		= k3_r5_rproc_kick,
	.da_to_va	= k3_r5_rproc_da_to_va,
};

 
static int k3_r5_rproc_configure(struct k3_r5_rproc *kproc)
{
	struct k3_r5_cluster *cluster = kproc->cluster;
	struct device *dev = kproc->dev;
	struct k3_r5_core *core0, *core, *temp;
	u32 ctrl = 0, cfg = 0, stat = 0;
	u32 set_cfg = 0, clr_cfg = 0;
	u64 boot_vec = 0;
	bool lockstep_en;
	bool single_cpu;
	int ret;

	core0 = list_first_entry(&cluster->cores, struct k3_r5_core, elem);
	if (cluster->mode == CLUSTER_MODE_LOCKSTEP ||
	    cluster->mode == CLUSTER_MODE_SINGLECPU ||
	    cluster->mode == CLUSTER_MODE_SINGLECORE) {
		core = core0;
	} else {
		core = kproc->core;
	}

	ret = ti_sci_proc_get_status(core->tsp, &boot_vec, &cfg, &ctrl,
				     &stat);
	if (ret < 0)
		return ret;

	dev_dbg(dev, "boot_vector = 0x%llx, cfg = 0x%x ctrl = 0x%x stat = 0x%x\n",
		boot_vec, cfg, ctrl, stat);

	single_cpu = !!(stat & PROC_BOOT_STATUS_FLAG_R5_SINGLECORE_ONLY);
	lockstep_en = !!(stat & PROC_BOOT_STATUS_FLAG_R5_LOCKSTEP_PERMITTED);

	 
	if (single_cpu && cluster->mode == CLUSTER_MODE_SPLIT) {
		dev_err(cluster->dev, "split-mode not permitted, force configuring for single-cpu mode\n");
		cluster->mode = CLUSTER_MODE_SINGLECPU;
	}

	 
	if (!lockstep_en && cluster->mode == CLUSTER_MODE_LOCKSTEP) {
		dev_err(cluster->dev, "lockstep mode not permitted, force configuring for split-mode\n");
		cluster->mode = CLUSTER_MODE_SPLIT;
	}

	 
	boot_vec = 0x0;
	if (core == core0) {
		clr_cfg = PROC_BOOT_CFG_FLAG_R5_TEINIT;
		 
		if (cluster->mode == CLUSTER_MODE_SINGLECPU ||
		    cluster->mode == CLUSTER_MODE_SINGLECORE) {
			set_cfg = PROC_BOOT_CFG_FLAG_R5_SINGLE_CORE;
		} else {
			 
			if (lockstep_en)
				clr_cfg |= PROC_BOOT_CFG_FLAG_R5_LOCKSTEP;
		}
	}

	if (core->atcm_enable)
		set_cfg |= PROC_BOOT_CFG_FLAG_R5_ATCM_EN;
	else
		clr_cfg |= PROC_BOOT_CFG_FLAG_R5_ATCM_EN;

	if (core->btcm_enable)
		set_cfg |= PROC_BOOT_CFG_FLAG_R5_BTCM_EN;
	else
		clr_cfg |= PROC_BOOT_CFG_FLAG_R5_BTCM_EN;

	if (core->loczrama)
		set_cfg |= PROC_BOOT_CFG_FLAG_R5_TCM_RSTBASE;
	else
		clr_cfg |= PROC_BOOT_CFG_FLAG_R5_TCM_RSTBASE;

	if (cluster->mode == CLUSTER_MODE_LOCKSTEP) {
		 
		list_for_each_entry(temp, &cluster->cores, elem) {
			ret = k3_r5_core_halt(temp);
			if (ret)
				goto out;

			if (temp != core) {
				clr_cfg &= ~PROC_BOOT_CFG_FLAG_R5_LOCKSTEP;
				clr_cfg &= ~PROC_BOOT_CFG_FLAG_R5_TEINIT;
			}
			ret = ti_sci_proc_set_config(temp->tsp, boot_vec,
						     set_cfg, clr_cfg);
			if (ret)
				goto out;
		}

		set_cfg = PROC_BOOT_CFG_FLAG_R5_LOCKSTEP;
		clr_cfg = 0;
		ret = ti_sci_proc_set_config(core->tsp, boot_vec,
					     set_cfg, clr_cfg);
	} else {
		ret = k3_r5_core_halt(core);
		if (ret)
			goto out;

		ret = ti_sci_proc_set_config(core->tsp, boot_vec,
					     set_cfg, clr_cfg);
	}

out:
	return ret;
}

static int k3_r5_reserved_mem_init(struct k3_r5_rproc *kproc)
{
	struct device *dev = kproc->dev;
	struct device_node *np = dev_of_node(dev);
	struct device_node *rmem_np;
	struct reserved_mem *rmem;
	int num_rmems;
	int ret, i;

	num_rmems = of_property_count_elems_of_size(np, "memory-region",
						    sizeof(phandle));
	if (num_rmems <= 0) {
		dev_err(dev, "device does not have reserved memory regions, ret = %d\n",
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
	of_reserved_mem_device_release(dev);
	return ret;
}

static void k3_r5_reserved_mem_exit(struct k3_r5_rproc *kproc)
{
	int i;

	for (i = 0; i < kproc->num_rmems; i++)
		iounmap(kproc->rmem[i].cpu_addr);
	kfree(kproc->rmem);

	of_reserved_mem_device_release(kproc->dev);
}

 
static void k3_r5_adjust_tcm_sizes(struct k3_r5_rproc *kproc)
{
	struct k3_r5_cluster *cluster = kproc->cluster;
	struct k3_r5_core *core = kproc->core;
	struct device *cdev = core->dev;
	struct k3_r5_core *core0;

	if (cluster->mode == CLUSTER_MODE_LOCKSTEP ||
	    cluster->mode == CLUSTER_MODE_SINGLECPU ||
	    cluster->mode == CLUSTER_MODE_SINGLECORE ||
	    !cluster->soc_data->tcm_is_double)
		return;

	core0 = list_first_entry(&cluster->cores, struct k3_r5_core, elem);
	if (core == core0) {
		WARN_ON(core->mem[0].size != SZ_64K);
		WARN_ON(core->mem[1].size != SZ_64K);

		core->mem[0].size /= 2;
		core->mem[1].size /= 2;

		dev_dbg(cdev, "adjusted TCM sizes, ATCM = 0x%zx BTCM = 0x%zx\n",
			core->mem[0].size, core->mem[1].size);
	}
}

 
static int k3_r5_rproc_configure_mode(struct k3_r5_rproc *kproc)
{
	struct k3_r5_cluster *cluster = kproc->cluster;
	struct k3_r5_core *core = kproc->core;
	struct device *cdev = core->dev;
	bool r_state = false, c_state = false, lockstep_en = false, single_cpu = false;
	u32 ctrl = 0, cfg = 0, stat = 0, halted = 0;
	u64 boot_vec = 0;
	u32 atcm_enable, btcm_enable, loczrama;
	struct k3_r5_core *core0;
	enum cluster_mode mode = cluster->mode;
	int ret;

	core0 = list_first_entry(&cluster->cores, struct k3_r5_core, elem);

	ret = core->ti_sci->ops.dev_ops.is_on(core->ti_sci, core->ti_sci_id,
					      &r_state, &c_state);
	if (ret) {
		dev_err(cdev, "failed to get initial state, mode cannot be determined, ret = %d\n",
			ret);
		return ret;
	}
	if (r_state != c_state) {
		dev_warn(cdev, "R5F core may have been powered on by a different host, programmed state (%d) != actual state (%d)\n",
			 r_state, c_state);
	}

	ret = reset_control_status(core->reset);
	if (ret < 0) {
		dev_err(cdev, "failed to get initial local reset status, ret = %d\n",
			ret);
		return ret;
	}

	ret = ti_sci_proc_get_status(core->tsp, &boot_vec, &cfg, &ctrl,
				     &stat);
	if (ret < 0) {
		dev_err(cdev, "failed to get initial processor status, ret = %d\n",
			ret);
		return ret;
	}
	atcm_enable = cfg & PROC_BOOT_CFG_FLAG_R5_ATCM_EN ?  1 : 0;
	btcm_enable = cfg & PROC_BOOT_CFG_FLAG_R5_BTCM_EN ?  1 : 0;
	loczrama = cfg & PROC_BOOT_CFG_FLAG_R5_TCM_RSTBASE ?  1 : 0;
	single_cpu = cfg & PROC_BOOT_CFG_FLAG_R5_SINGLE_CORE ? 1 : 0;
	lockstep_en = cfg & PROC_BOOT_CFG_FLAG_R5_LOCKSTEP ? 1 : 0;

	if (single_cpu && mode != CLUSTER_MODE_SINGLECORE)
		mode = CLUSTER_MODE_SINGLECPU;
	if (lockstep_en)
		mode = CLUSTER_MODE_LOCKSTEP;

	halted = ctrl & PROC_BOOT_CTRL_FLAG_R5_CORE_HALT;

	 
	if (c_state && !ret && !halted) {
		dev_info(cdev, "configured R5F for IPC-only mode\n");
		kproc->rproc->state = RPROC_DETACHED;
		ret = 1;
		 
		kproc->rproc->ops->prepare = NULL;
		kproc->rproc->ops->unprepare = NULL;
		kproc->rproc->ops->start = NULL;
		kproc->rproc->ops->stop = NULL;
		kproc->rproc->ops->attach = k3_r5_rproc_attach;
		kproc->rproc->ops->detach = k3_r5_rproc_detach;
		kproc->rproc->ops->get_loaded_rsc_table =
						k3_r5_get_loaded_rsc_table;
	} else if (!c_state) {
		dev_info(cdev, "configured R5F for remoteproc mode\n");
		ret = 0;
	} else {
		dev_err(cdev, "mismatched mode: local_reset = %s, module_reset = %s, core_state = %s\n",
			!ret ? "deasserted" : "asserted",
			c_state ? "deasserted" : "asserted",
			halted ? "halted" : "unhalted");
		ret = -EINVAL;
	}

	 
	if (ret > 0) {
		if (core == core0)
			cluster->mode = mode;
		core->atcm_enable = atcm_enable;
		core->btcm_enable = btcm_enable;
		core->loczrama = loczrama;
		core->mem[0].dev_addr = loczrama ? 0 : K3_R5_TCM_DEV_ADDR;
		core->mem[1].dev_addr = loczrama ? K3_R5_TCM_DEV_ADDR : 0;
	}

	return ret;
}

static int k3_r5_cluster_rproc_init(struct platform_device *pdev)
{
	struct k3_r5_cluster *cluster = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	struct k3_r5_rproc *kproc;
	struct k3_r5_core *core, *core1;
	struct device *cdev;
	const char *fw_name;
	struct rproc *rproc;
	int ret, ret1;

	core1 = list_last_entry(&cluster->cores, struct k3_r5_core, elem);
	list_for_each_entry(core, &cluster->cores, elem) {
		cdev = core->dev;
		ret = rproc_of_parse_firmware(cdev, 0, &fw_name);
		if (ret) {
			dev_err(dev, "failed to parse firmware-name property, ret = %d\n",
				ret);
			goto out;
		}

		rproc = rproc_alloc(cdev, dev_name(cdev), &k3_r5_rproc_ops,
				    fw_name, sizeof(*kproc));
		if (!rproc) {
			ret = -ENOMEM;
			goto out;
		}

		 
		rproc->has_iommu = false;
		 
		rproc->recovery_disabled = true;

		kproc = rproc->priv;
		kproc->cluster = cluster;
		kproc->core = core;
		kproc->dev = cdev;
		kproc->rproc = rproc;
		core->rproc = rproc;

		ret = k3_r5_rproc_configure_mode(kproc);
		if (ret < 0)
			goto err_config;
		if (ret)
			goto init_rmem;

		ret = k3_r5_rproc_configure(kproc);
		if (ret) {
			dev_err(dev, "initial configure failed, ret = %d\n",
				ret);
			goto err_config;
		}

init_rmem:
		k3_r5_adjust_tcm_sizes(kproc);

		ret = k3_r5_reserved_mem_init(kproc);
		if (ret) {
			dev_err(dev, "reserved memory init failed, ret = %d\n",
				ret);
			goto err_config;
		}

		ret = rproc_add(rproc);
		if (ret) {
			dev_err(dev, "rproc_add failed, ret = %d\n", ret);
			goto err_add;
		}

		 
		if (cluster->mode == CLUSTER_MODE_LOCKSTEP ||
		    cluster->mode == CLUSTER_MODE_SINGLECPU ||
		    cluster->mode == CLUSTER_MODE_SINGLECORE)
			break;
	}

	return 0;

err_split:
	if (rproc->state == RPROC_ATTACHED) {
		ret1 = rproc_detach(rproc);
		if (ret1) {
			dev_err(kproc->dev, "failed to detach rproc, ret = %d\n",
				ret1);
			return ret1;
		}
	}

	rproc_del(rproc);
err_add:
	k3_r5_reserved_mem_exit(kproc);
err_config:
	rproc_free(rproc);
	core->rproc = NULL;
out:
	 
	if (cluster->mode == CLUSTER_MODE_SPLIT && core == core1) {
		core = list_prev_entry(core, elem);
		rproc = core->rproc;
		kproc = rproc->priv;
		goto err_split;
	}
	return ret;
}

static void k3_r5_cluster_rproc_exit(void *data)
{
	struct k3_r5_cluster *cluster = platform_get_drvdata(data);
	struct k3_r5_rproc *kproc;
	struct k3_r5_core *core;
	struct rproc *rproc;
	int ret;

	 
	core = (cluster->mode == CLUSTER_MODE_LOCKSTEP ||
		cluster->mode == CLUSTER_MODE_SINGLECPU) ?
		list_first_entry(&cluster->cores, struct k3_r5_core, elem) :
		list_last_entry(&cluster->cores, struct k3_r5_core, elem);

	list_for_each_entry_from_reverse(core, &cluster->cores, elem) {
		rproc = core->rproc;
		kproc = rproc->priv;

		if (rproc->state == RPROC_ATTACHED) {
			ret = rproc_detach(rproc);
			if (ret) {
				dev_err(kproc->dev, "failed to detach rproc, ret = %d\n", ret);
				return;
			}
		}

		rproc_del(rproc);

		k3_r5_reserved_mem_exit(kproc);

		rproc_free(rproc);
		core->rproc = NULL;
	}
}

static int k3_r5_core_of_get_internal_memories(struct platform_device *pdev,
					       struct k3_r5_core *core)
{
	static const char * const mem_names[] = {"atcm", "btcm"};
	struct device *dev = &pdev->dev;
	struct resource *res;
	int num_mems;
	int i;

	num_mems = ARRAY_SIZE(mem_names);
	core->mem = devm_kcalloc(dev, num_mems, sizeof(*core->mem), GFP_KERNEL);
	if (!core->mem)
		return -ENOMEM;

	for (i = 0; i < num_mems; i++) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   mem_names[i]);
		if (!res) {
			dev_err(dev, "found no memory resource for %s\n",
				mem_names[i]);
			return -EINVAL;
		}
		if (!devm_request_mem_region(dev, res->start,
					     resource_size(res),
					     dev_name(dev))) {
			dev_err(dev, "could not request %s region for resource\n",
				mem_names[i]);
			return -EBUSY;
		}

		 
		core->mem[i].cpu_addr = devm_ioremap_wc(dev, res->start,
							resource_size(res));
		if (!core->mem[i].cpu_addr) {
			dev_err(dev, "failed to map %s memory\n", mem_names[i]);
			return -ENOMEM;
		}
		core->mem[i].bus_addr = res->start;

		 
		if (!strcmp(mem_names[i], "atcm")) {
			core->mem[i].dev_addr = core->loczrama ?
							0 : K3_R5_TCM_DEV_ADDR;
		} else {
			core->mem[i].dev_addr = core->loczrama ?
							K3_R5_TCM_DEV_ADDR : 0;
		}
		core->mem[i].size = resource_size(res);

		dev_dbg(dev, "memory %5s: bus addr %pa size 0x%zx va %pK da 0x%x\n",
			mem_names[i], &core->mem[i].bus_addr,
			core->mem[i].size, core->mem[i].cpu_addr,
			core->mem[i].dev_addr);
	}
	core->num_mems = num_mems;

	return 0;
}

static int k3_r5_core_of_get_sram_memories(struct platform_device *pdev,
					   struct k3_r5_core *core)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct device_node *sram_np;
	struct resource res;
	int num_sram;
	int i, ret;

	num_sram = of_property_count_elems_of_size(np, "sram", sizeof(phandle));
	if (num_sram <= 0) {
		dev_dbg(dev, "device does not use reserved on-chip memories, num_sram = %d\n",
			num_sram);
		return 0;
	}

	core->sram = devm_kcalloc(dev, num_sram, sizeof(*core->sram), GFP_KERNEL);
	if (!core->sram)
		return -ENOMEM;

	for (i = 0; i < num_sram; i++) {
		sram_np = of_parse_phandle(np, "sram", i);
		if (!sram_np)
			return -EINVAL;

		if (!of_device_is_available(sram_np)) {
			of_node_put(sram_np);
			return -EINVAL;
		}

		ret = of_address_to_resource(sram_np, 0, &res);
		of_node_put(sram_np);
		if (ret)
			return -EINVAL;

		core->sram[i].bus_addr = res.start;
		core->sram[i].dev_addr = res.start;
		core->sram[i].size = resource_size(&res);
		core->sram[i].cpu_addr = devm_ioremap_wc(dev, res.start,
							 resource_size(&res));
		if (!core->sram[i].cpu_addr) {
			dev_err(dev, "failed to parse and map sram%d memory at %pad\n",
				i, &res.start);
			return -ENOMEM;
		}

		dev_dbg(dev, "memory sram%d: bus addr %pa size 0x%zx va %pK da 0x%x\n",
			i, &core->sram[i].bus_addr,
			core->sram[i].size, core->sram[i].cpu_addr,
			core->sram[i].dev_addr);
	}
	core->num_sram = num_sram;

	return 0;
}

static
struct ti_sci_proc *k3_r5_core_of_get_tsp(struct device *dev,
					  const struct ti_sci_handle *sci)
{
	struct ti_sci_proc *tsp;
	u32 temp[2];
	int ret;

	ret = of_property_read_u32_array(dev_of_node(dev), "ti,sci-proc-ids",
					 temp, 2);
	if (ret < 0)
		return ERR_PTR(ret);

	tsp = devm_kzalloc(dev, sizeof(*tsp), GFP_KERNEL);
	if (!tsp)
		return ERR_PTR(-ENOMEM);

	tsp->dev = dev;
	tsp->sci = sci;
	tsp->ops = &sci->ops.proc_ops;
	tsp->proc_id = temp[0];
	tsp->host_id = temp[1];

	return tsp;
}

static int k3_r5_core_of_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev_of_node(dev);
	struct k3_r5_core *core;
	int ret;

	if (!devres_open_group(dev, k3_r5_core_of_init, GFP_KERNEL))
		return -ENOMEM;

	core = devm_kzalloc(dev, sizeof(*core), GFP_KERNEL);
	if (!core) {
		ret = -ENOMEM;
		goto err;
	}

	core->dev = dev;
	 
	core->atcm_enable = 0;
	core->btcm_enable = 1;
	core->loczrama = 1;

	ret = of_property_read_u32(np, "ti,atcm-enable", &core->atcm_enable);
	if (ret < 0 && ret != -EINVAL) {
		dev_err(dev, "invalid format for ti,atcm-enable, ret = %d\n",
			ret);
		goto err;
	}

	ret = of_property_read_u32(np, "ti,btcm-enable", &core->btcm_enable);
	if (ret < 0 && ret != -EINVAL) {
		dev_err(dev, "invalid format for ti,btcm-enable, ret = %d\n",
			ret);
		goto err;
	}

	ret = of_property_read_u32(np, "ti,loczrama", &core->loczrama);
	if (ret < 0 && ret != -EINVAL) {
		dev_err(dev, "invalid format for ti,loczrama, ret = %d\n", ret);
		goto err;
	}

	core->ti_sci = devm_ti_sci_get_by_phandle(dev, "ti,sci");
	if (IS_ERR(core->ti_sci)) {
		ret = PTR_ERR(core->ti_sci);
		if (ret != -EPROBE_DEFER) {
			dev_err(dev, "failed to get ti-sci handle, ret = %d\n",
				ret);
		}
		core->ti_sci = NULL;
		goto err;
	}

	ret = of_property_read_u32(np, "ti,sci-dev-id", &core->ti_sci_id);
	if (ret) {
		dev_err(dev, "missing 'ti,sci-dev-id' property\n");
		goto err;
	}

	core->reset = devm_reset_control_get_exclusive(dev, NULL);
	if (IS_ERR_OR_NULL(core->reset)) {
		ret = PTR_ERR_OR_ZERO(core->reset);
		if (!ret)
			ret = -ENODEV;
		if (ret != -EPROBE_DEFER) {
			dev_err(dev, "failed to get reset handle, ret = %d\n",
				ret);
		}
		goto err;
	}

	core->tsp = k3_r5_core_of_get_tsp(dev, core->ti_sci);
	if (IS_ERR(core->tsp)) {
		ret = PTR_ERR(core->tsp);
		dev_err(dev, "failed to construct ti-sci proc control, ret = %d\n",
			ret);
		goto err;
	}

	ret = k3_r5_core_of_get_internal_memories(pdev, core);
	if (ret) {
		dev_err(dev, "failed to get internal memories, ret = %d\n",
			ret);
		goto err;
	}

	ret = k3_r5_core_of_get_sram_memories(pdev, core);
	if (ret) {
		dev_err(dev, "failed to get sram memories, ret = %d\n", ret);
		goto err;
	}

	ret = ti_sci_proc_request(core->tsp);
	if (ret < 0) {
		dev_err(dev, "ti_sci_proc_request failed, ret = %d\n", ret);
		goto err;
	}

	platform_set_drvdata(pdev, core);
	devres_close_group(dev, k3_r5_core_of_init);

	return 0;

err:
	devres_release_group(dev, k3_r5_core_of_init);
	return ret;
}

 
static void k3_r5_core_of_exit(struct platform_device *pdev)
{
	struct k3_r5_core *core = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	int ret;

	ret = ti_sci_proc_release(core->tsp);
	if (ret)
		dev_err(dev, "failed to release proc, ret = %d\n", ret);

	platform_set_drvdata(pdev, NULL);
	devres_release_group(dev, k3_r5_core_of_init);
}

static void k3_r5_cluster_of_exit(void *data)
{
	struct k3_r5_cluster *cluster = platform_get_drvdata(data);
	struct platform_device *cpdev;
	struct k3_r5_core *core, *temp;

	list_for_each_entry_safe_reverse(core, temp, &cluster->cores, elem) {
		list_del(&core->elem);
		cpdev = to_platform_device(core->dev);
		k3_r5_core_of_exit(cpdev);
	}
}

static int k3_r5_cluster_of_init(struct platform_device *pdev)
{
	struct k3_r5_cluster *cluster = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	struct device_node *np = dev_of_node(dev);
	struct platform_device *cpdev;
	struct device_node *child;
	struct k3_r5_core *core;
	int ret;

	for_each_available_child_of_node(np, child) {
		cpdev = of_find_device_by_node(child);
		if (!cpdev) {
			ret = -ENODEV;
			dev_err(dev, "could not get R5 core platform device\n");
			of_node_put(child);
			goto fail;
		}

		ret = k3_r5_core_of_init(cpdev);
		if (ret) {
			dev_err(dev, "k3_r5_core_of_init failed, ret = %d\n",
				ret);
			put_device(&cpdev->dev);
			of_node_put(child);
			goto fail;
		}

		core = platform_get_drvdata(cpdev);
		put_device(&cpdev->dev);
		list_add_tail(&core->elem, &cluster->cores);
	}

	return 0;

fail:
	k3_r5_cluster_of_exit(pdev);
	return ret;
}

static int k3_r5_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev_of_node(dev);
	struct k3_r5_cluster *cluster;
	const struct k3_r5_soc_data *data;
	int ret;
	int num_cores;

	data = of_device_get_match_data(&pdev->dev);
	if (!data) {
		dev_err(dev, "SoC-specific data is not defined\n");
		return -ENODEV;
	}

	cluster = devm_kzalloc(dev, sizeof(*cluster), GFP_KERNEL);
	if (!cluster)
		return -ENOMEM;

	cluster->dev = dev;
	cluster->soc_data = data;
	INIT_LIST_HEAD(&cluster->cores);

	ret = of_property_read_u32(np, "ti,cluster-mode", &cluster->mode);
	if (ret < 0 && ret != -EINVAL) {
		dev_err(dev, "invalid format for ti,cluster-mode, ret = %d\n",
			ret);
		return ret;
	}

	if (ret == -EINVAL) {
		 
		if (!data->is_single_core)
			cluster->mode = data->single_cpu_mode ?
					CLUSTER_MODE_SPLIT : CLUSTER_MODE_LOCKSTEP;
		else
			cluster->mode = CLUSTER_MODE_SINGLECORE;
	}

	if  ((cluster->mode == CLUSTER_MODE_SINGLECPU && !data->single_cpu_mode) ||
	     (cluster->mode == CLUSTER_MODE_SINGLECORE && !data->is_single_core)) {
		dev_err(dev, "Cluster mode = %d is not supported on this SoC\n", cluster->mode);
		return -EINVAL;
	}

	num_cores = of_get_available_child_count(np);
	if (num_cores != 2 && !data->is_single_core) {
		dev_err(dev, "MCU cluster requires both R5F cores to be enabled but num_cores is set to = %d\n",
			num_cores);
		return -ENODEV;
	}

	if (num_cores != 1 && data->is_single_core) {
		dev_err(dev, "SoC supports only single core R5 but num_cores is set to %d\n",
			num_cores);
		return -ENODEV;
	}

	platform_set_drvdata(pdev, cluster);

	ret = devm_of_platform_populate(dev);
	if (ret) {
		dev_err(dev, "devm_of_platform_populate failed, ret = %d\n",
			ret);
		return ret;
	}

	ret = k3_r5_cluster_of_init(pdev);
	if (ret) {
		dev_err(dev, "k3_r5_cluster_of_init failed, ret = %d\n", ret);
		return ret;
	}

	ret = devm_add_action_or_reset(dev, k3_r5_cluster_of_exit, pdev);
	if (ret)
		return ret;

	ret = k3_r5_cluster_rproc_init(pdev);
	if (ret) {
		dev_err(dev, "k3_r5_cluster_rproc_init failed, ret = %d\n",
			ret);
		return ret;
	}

	ret = devm_add_action_or_reset(dev, k3_r5_cluster_rproc_exit, pdev);
	if (ret)
		return ret;

	return 0;
}

static const struct k3_r5_soc_data am65_j721e_soc_data = {
	.tcm_is_double = false,
	.tcm_ecc_autoinit = false,
	.single_cpu_mode = false,
	.is_single_core = false,
};

static const struct k3_r5_soc_data j7200_j721s2_soc_data = {
	.tcm_is_double = true,
	.tcm_ecc_autoinit = true,
	.single_cpu_mode = false,
	.is_single_core = false,
};

static const struct k3_r5_soc_data am64_soc_data = {
	.tcm_is_double = true,
	.tcm_ecc_autoinit = true,
	.single_cpu_mode = true,
	.is_single_core = false,
};

static const struct k3_r5_soc_data am62_soc_data = {
	.tcm_is_double = false,
	.tcm_ecc_autoinit = true,
	.single_cpu_mode = false,
	.is_single_core = true,
};

static const struct of_device_id k3_r5_of_match[] = {
	{ .compatible = "ti,am654-r5fss", .data = &am65_j721e_soc_data, },
	{ .compatible = "ti,j721e-r5fss", .data = &am65_j721e_soc_data, },
	{ .compatible = "ti,j7200-r5fss", .data = &j7200_j721s2_soc_data, },
	{ .compatible = "ti,am64-r5fss",  .data = &am64_soc_data, },
	{ .compatible = "ti,am62-r5fss",  .data = &am62_soc_data, },
	{ .compatible = "ti,j721s2-r5fss",  .data = &j7200_j721s2_soc_data, },
	{   },
};
MODULE_DEVICE_TABLE(of, k3_r5_of_match);

static struct platform_driver k3_r5_rproc_driver = {
	.probe = k3_r5_probe,
	.driver = {
		.name = "k3_r5_rproc",
		.of_match_table = k3_r5_of_match,
	},
};

module_platform_driver(k3_r5_rproc_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TI K3 R5F remote processor driver");
MODULE_AUTHOR("Suman Anna <s-anna@ti.com>");
