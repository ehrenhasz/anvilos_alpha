
 

#include <dt-bindings/power/xlnx-zynqmp-power.h>
#include <linux/dma-mapping.h>
#include <linux/firmware/xlnx-zynqmp.h>
#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/zynqmp-ipi-message.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>

#include "remoteproc_internal.h"

 
#define IPI_BUF_LEN_MAX	32U

 
#define MBOX_CLIENT_BUF_MAX	(IPI_BUF_LEN_MAX + \
				 sizeof(struct zynqmp_ipi_message))
 
enum zynqmp_r5_cluster_mode {
	SPLIT_MODE = 0,  
	LOCKSTEP_MODE = 1,  
	SINGLE_CPU_MODE = 2,  
};

 
struct mem_bank_data {
	phys_addr_t addr;
	size_t size;
	u32 pm_domain_id;
	char *bank_name;
};

 
struct mbox_info {
	unsigned char rx_mc_buf[MBOX_CLIENT_BUF_MAX];
	unsigned char tx_mc_buf[MBOX_CLIENT_BUF_MAX];
	struct zynqmp_r5_core *r5_core;
	struct work_struct mbox_work;
	struct mbox_client mbox_cl;
	struct mbox_chan *tx_chan;
	struct mbox_chan *rx_chan;
};

 
static const struct mem_bank_data zynqmp_tcm_banks[] = {
	{0xffe00000UL, 0x10000UL, PD_R5_0_ATCM, "atcm0"},  
	{0xffe20000UL, 0x10000UL, PD_R5_0_BTCM, "btcm0"},
	{0xffe90000UL, 0x10000UL, PD_R5_1_ATCM, "atcm1"},
	{0xffeb0000UL, 0x10000UL, PD_R5_1_BTCM, "btcm1"},
};

 
struct zynqmp_r5_core {
	struct device *dev;
	struct device_node *np;
	int tcm_bank_count;
	struct mem_bank_data **tcm_banks;
	struct rproc *rproc;
	u32 pm_domain_id;
	struct mbox_info *ipi;
};

 
struct zynqmp_r5_cluster {
	struct device *dev;
	enum  zynqmp_r5_cluster_mode mode;
	int core_count;
	struct zynqmp_r5_core **r5_cores;
};

 
static int event_notified_idr_cb(int id, void *ptr, void *data)
{
	struct rproc *rproc = data;

	if (rproc_vq_interrupt(rproc, id) == IRQ_NONE)
		dev_dbg(&rproc->dev, "data not found for vqid=%d\n", id);

	return 0;
}

 
static void handle_event_notified(struct work_struct *work)
{
	struct mbox_info *ipi;
	struct rproc *rproc;

	ipi = container_of(work, struct mbox_info, mbox_work);
	rproc = ipi->r5_core->rproc;

	 
	idr_for_each(&rproc->notifyids, event_notified_idr_cb, rproc);
}

 
static void zynqmp_r5_mb_rx_cb(struct mbox_client *cl, void *msg)
{
	struct zynqmp_ipi_message *ipi_msg, *buf_msg;
	struct mbox_info *ipi;
	size_t len;

	ipi = container_of(cl, struct mbox_info, mbox_cl);

	 
	ipi_msg = (struct zynqmp_ipi_message *)msg;
	buf_msg = (struct zynqmp_ipi_message *)ipi->rx_mc_buf;
	len = ipi_msg->len;
	if (len > IPI_BUF_LEN_MAX) {
		dev_warn(cl->dev, "msg size exceeded than %d\n",
			 IPI_BUF_LEN_MAX);
		len = IPI_BUF_LEN_MAX;
	}
	buf_msg->len = len;
	memcpy(buf_msg->data, ipi_msg->data, len);

	 
	if (mbox_send_message(ipi->rx_chan, NULL) < 0)
		dev_err(cl->dev, "ack failed to mbox rx_chan\n");

	schedule_work(&ipi->mbox_work);
}

 
static struct mbox_info *zynqmp_r5_setup_mbox(struct device *cdev)
{
	struct mbox_client *mbox_cl;
	struct mbox_info *ipi;

	ipi = kzalloc(sizeof(*ipi), GFP_KERNEL);
	if (!ipi)
		return NULL;

	mbox_cl = &ipi->mbox_cl;
	mbox_cl->rx_callback = zynqmp_r5_mb_rx_cb;
	mbox_cl->tx_block = false;
	mbox_cl->knows_txdone = false;
	mbox_cl->tx_done = NULL;
	mbox_cl->dev = cdev;

	 
	ipi->tx_chan = mbox_request_channel_byname(mbox_cl, "tx");
	if (IS_ERR(ipi->tx_chan)) {
		ipi->tx_chan = NULL;
		kfree(ipi);
		dev_warn(cdev, "mbox tx channel request failed\n");
		return NULL;
	}

	ipi->rx_chan = mbox_request_channel_byname(mbox_cl, "rx");
	if (IS_ERR(ipi->rx_chan)) {
		mbox_free_channel(ipi->tx_chan);
		ipi->rx_chan = NULL;
		ipi->tx_chan = NULL;
		kfree(ipi);
		dev_warn(cdev, "mbox rx channel request failed\n");
		return NULL;
	}

	INIT_WORK(&ipi->mbox_work, handle_event_notified);

	return ipi;
}

static void zynqmp_r5_free_mbox(struct mbox_info *ipi)
{
	if (!ipi)
		return;

	if (ipi->tx_chan) {
		mbox_free_channel(ipi->tx_chan);
		ipi->tx_chan = NULL;
	}

	if (ipi->rx_chan) {
		mbox_free_channel(ipi->rx_chan);
		ipi->rx_chan = NULL;
	}

	kfree(ipi);
}

 
static void zynqmp_r5_rproc_kick(struct rproc *rproc, int vqid)
{
	struct zynqmp_r5_core *r5_core = rproc->priv;
	struct device *dev = r5_core->dev;
	struct zynqmp_ipi_message *mb_msg;
	struct mbox_info *ipi;
	int ret;

	ipi = r5_core->ipi;
	if (!ipi)
		return;

	mb_msg = (struct zynqmp_ipi_message *)ipi->tx_mc_buf;
	memcpy(mb_msg->data, &vqid, sizeof(vqid));
	mb_msg->len = sizeof(vqid);
	ret = mbox_send_message(ipi->tx_chan, mb_msg);
	if (ret < 0)
		dev_warn(dev, "failed to send message\n");
}

 
static int zynqmp_r5_set_mode(struct zynqmp_r5_core *r5_core,
			      enum rpu_oper_mode fw_reg_val,
			      enum rpu_tcm_comb tcm_mode)
{
	int ret;

	ret = zynqmp_pm_set_rpu_mode(r5_core->pm_domain_id, fw_reg_val);
	if (ret < 0) {
		dev_err(r5_core->dev, "failed to set RPU mode\n");
		return ret;
	}

	ret = zynqmp_pm_set_tcm_config(r5_core->pm_domain_id, tcm_mode);
	if (ret < 0)
		dev_err(r5_core->dev, "failed to configure TCM\n");

	return ret;
}

 
static int zynqmp_r5_rproc_start(struct rproc *rproc)
{
	struct zynqmp_r5_core *r5_core = rproc->priv;
	enum rpu_boot_mem bootmem;
	int ret;

	 
	bootmem = (rproc->bootaddr >= 0xFFFC0000) ?
		   PM_RPU_BOOTMEM_HIVEC : PM_RPU_BOOTMEM_LOVEC;

	dev_dbg(r5_core->dev, "RPU boot addr 0x%llx from %s.", rproc->bootaddr,
		bootmem == PM_RPU_BOOTMEM_HIVEC ? "OCM" : "TCM");

	ret = zynqmp_pm_request_wake(r5_core->pm_domain_id, 1,
				     bootmem, ZYNQMP_PM_REQUEST_ACK_NO);
	if (ret)
		dev_err(r5_core->dev,
			"failed to start RPU = 0x%x\n", r5_core->pm_domain_id);
	return ret;
}

 
static int zynqmp_r5_rproc_stop(struct rproc *rproc)
{
	struct zynqmp_r5_core *r5_core = rproc->priv;
	int ret;

	ret = zynqmp_pm_force_pwrdwn(r5_core->pm_domain_id,
				     ZYNQMP_PM_REQUEST_ACK_BLOCKING);
	if (ret)
		dev_err(r5_core->dev, "failed to stop remoteproc RPU %d\n", ret);

	return ret;
}

 
static int zynqmp_r5_mem_region_map(struct rproc *rproc,
				    struct rproc_mem_entry *mem)
{
	void __iomem *va;

	va = ioremap_wc(mem->dma, mem->len);
	if (IS_ERR_OR_NULL(va))
		return -ENOMEM;

	mem->va = (void *)va;

	return 0;
}

 
static int zynqmp_r5_mem_region_unmap(struct rproc *rproc,
				      struct rproc_mem_entry *mem)
{
	iounmap((void __iomem *)mem->va);
	return 0;
}

 
static int add_mem_regions_carveout(struct rproc *rproc)
{
	struct rproc_mem_entry *rproc_mem;
	struct zynqmp_r5_core *r5_core;
	struct of_phandle_iterator it;
	struct reserved_mem *rmem;
	int i = 0;

	r5_core = rproc->priv;

	 
	of_phandle_iterator_init(&it, r5_core->np, "memory-region", NULL, 0);

	while (of_phandle_iterator_next(&it) == 0) {
		rmem = of_reserved_mem_lookup(it.node);
		if (!rmem) {
			of_node_put(it.node);
			dev_err(&rproc->dev, "unable to acquire memory-region\n");
			return -EINVAL;
		}

		if (!strcmp(it.node->name, "vdev0buffer")) {
			 
			rproc_mem = rproc_of_resm_mem_entry_init(&rproc->dev, i,
								 rmem->size,
								 rmem->base,
								 it.node->name);
		} else {
			 
			rproc_mem = rproc_mem_entry_init(&rproc->dev, NULL,
							 (dma_addr_t)rmem->base,
							 rmem->size, rmem->base,
							 zynqmp_r5_mem_region_map,
							 zynqmp_r5_mem_region_unmap,
							 it.node->name);
		}

		if (!rproc_mem) {
			of_node_put(it.node);
			return -ENOMEM;
		}

		rproc_add_carveout(rproc, rproc_mem);

		dev_dbg(&rproc->dev, "reserved mem carveout %s addr=%llx, size=0x%llx",
			it.node->name, rmem->base, rmem->size);
		i++;
	}

	return 0;
}

 
static int tcm_mem_unmap(struct rproc *rproc, struct rproc_mem_entry *mem)
{
	iounmap((void __iomem *)mem->va);

	return 0;
}

 
static int tcm_mem_map(struct rproc *rproc,
		       struct rproc_mem_entry *mem)
{
	void __iomem *va;

	va = ioremap_wc(mem->dma, mem->len);
	if (IS_ERR_OR_NULL(va))
		return -ENOMEM;

	 
	mem->va = (void *)va;

	 
	memset_io(va, 0, mem->len);

	 
	mem->da &= 0x000fffff;

	 
	if (mem->da == 0x90000 || mem->da == 0xB0000)
		mem->da -= 0x90000;

	 
	if (mem->da != 0x0 && mem->da != 0x20000) {
		dev_err(&rproc->dev, "invalid TCM address: %x\n", mem->da);
		return -EINVAL;
	}
	return 0;
}

 
static int add_tcm_carveout_split_mode(struct rproc *rproc)
{
	struct rproc_mem_entry *rproc_mem;
	struct zynqmp_r5_core *r5_core;
	int i, num_banks, ret;
	phys_addr_t bank_addr;
	struct device *dev;
	u32 pm_domain_id;
	size_t bank_size;
	char *bank_name;

	r5_core = rproc->priv;
	dev = r5_core->dev;
	num_banks = r5_core->tcm_bank_count;

	 
	for (i = 0; i < num_banks; i++) {
		bank_addr = r5_core->tcm_banks[i]->addr;
		bank_name = r5_core->tcm_banks[i]->bank_name;
		bank_size = r5_core->tcm_banks[i]->size;
		pm_domain_id = r5_core->tcm_banks[i]->pm_domain_id;

		ret = zynqmp_pm_request_node(pm_domain_id,
					     ZYNQMP_PM_CAPABILITY_ACCESS, 0,
					     ZYNQMP_PM_REQUEST_ACK_BLOCKING);
		if (ret < 0) {
			dev_err(dev, "failed to turn on TCM 0x%x", pm_domain_id);
			goto release_tcm_split;
		}

		dev_dbg(dev, "TCM carveout split mode %s addr=%llx, size=0x%lx",
			bank_name, bank_addr, bank_size);

		rproc_mem = rproc_mem_entry_init(dev, NULL, bank_addr,
						 bank_size, bank_addr,
						 tcm_mem_map, tcm_mem_unmap,
						 bank_name);
		if (!rproc_mem) {
			ret = -ENOMEM;
			zynqmp_pm_release_node(pm_domain_id);
			goto release_tcm_split;
		}

		rproc_add_carveout(rproc, rproc_mem);
	}

	return 0;

release_tcm_split:
	 
	for (i--; i >= 0; i--) {
		pm_domain_id = r5_core->tcm_banks[i]->pm_domain_id;
		zynqmp_pm_release_node(pm_domain_id);
	}
	return ret;
}

 
static int add_tcm_carveout_lockstep_mode(struct rproc *rproc)
{
	struct rproc_mem_entry *rproc_mem;
	struct zynqmp_r5_core *r5_core;
	int i, num_banks, ret;
	phys_addr_t bank_addr;
	size_t bank_size = 0;
	struct device *dev;
	u32 pm_domain_id;
	char *bank_name;

	r5_core = rproc->priv;
	dev = r5_core->dev;

	 
	num_banks = r5_core->tcm_bank_count;

	 
	bank_addr = r5_core->tcm_banks[0]->addr;
	bank_name = r5_core->tcm_banks[0]->bank_name;

	for (i = 0; i < num_banks; i++) {
		bank_size += r5_core->tcm_banks[i]->size;
		pm_domain_id = r5_core->tcm_banks[i]->pm_domain_id;

		 
		ret = zynqmp_pm_request_node(pm_domain_id,
					     ZYNQMP_PM_CAPABILITY_ACCESS, 0,
					     ZYNQMP_PM_REQUEST_ACK_BLOCKING);
		if (ret < 0) {
			dev_err(dev, "failed to turn on TCM 0x%x", pm_domain_id);
			goto release_tcm_lockstep;
		}
	}

	dev_dbg(dev, "TCM add carveout lockstep mode %s addr=0x%llx, size=0x%lx",
		bank_name, bank_addr, bank_size);

	 
	rproc_mem = rproc_mem_entry_init(dev, NULL, bank_addr,
					 bank_size, bank_addr,
					 tcm_mem_map, tcm_mem_unmap,
					 bank_name);
	if (!rproc_mem) {
		ret = -ENOMEM;
		goto release_tcm_lockstep;
	}

	 
	rproc_add_carveout(rproc, rproc_mem);

	return 0;

release_tcm_lockstep:
	 
	for (i--; i >= 0; i--) {
		pm_domain_id = r5_core->tcm_banks[i]->pm_domain_id;
		zynqmp_pm_release_node(pm_domain_id);
	}
	return ret;
}

 
static int add_tcm_banks(struct rproc *rproc)
{
	struct zynqmp_r5_cluster *cluster;
	struct zynqmp_r5_core *r5_core;
	struct device *dev;

	r5_core = rproc->priv;
	if (!r5_core)
		return -EINVAL;

	dev = r5_core->dev;

	cluster = dev_get_drvdata(dev->parent);
	if (!cluster) {
		dev_err(dev->parent, "Invalid driver data\n");
		return -EINVAL;
	}

	 
	if (cluster->mode == SPLIT_MODE)
		return add_tcm_carveout_split_mode(rproc);
	else if (cluster->mode == LOCKSTEP_MODE)
		return add_tcm_carveout_lockstep_mode(rproc);

	return -EINVAL;
}

 
static int zynqmp_r5_parse_fw(struct rproc *rproc, const struct firmware *fw)
{
	int ret;

	ret = rproc_elf_load_rsc_table(rproc, fw);
	if (ret == -EINVAL) {
		 
		dev_info(&rproc->dev, "no resource table found.\n");
		ret = 0;
	}
	return ret;
}

 
static int zynqmp_r5_rproc_prepare(struct rproc *rproc)
{
	int ret;

	ret = add_tcm_banks(rproc);
	if (ret) {
		dev_err(&rproc->dev, "failed to get TCM banks, err %d\n", ret);
		return ret;
	}

	ret = add_mem_regions_carveout(rproc);
	if (ret) {
		dev_err(&rproc->dev, "failed to get reserve mem regions %d\n", ret);
		return ret;
	}

	return 0;
}

 
static int zynqmp_r5_rproc_unprepare(struct rproc *rproc)
{
	struct zynqmp_r5_core *r5_core;
	u32 pm_domain_id;
	int i;

	r5_core = rproc->priv;

	for (i = 0; i < r5_core->tcm_bank_count; i++) {
		pm_domain_id = r5_core->tcm_banks[i]->pm_domain_id;
		if (zynqmp_pm_release_node(pm_domain_id))
			dev_warn(r5_core->dev,
				 "can't turn off TCM bank 0x%x", pm_domain_id);
	}

	return 0;
}

static const struct rproc_ops zynqmp_r5_rproc_ops = {
	.prepare	= zynqmp_r5_rproc_prepare,
	.unprepare	= zynqmp_r5_rproc_unprepare,
	.start		= zynqmp_r5_rproc_start,
	.stop		= zynqmp_r5_rproc_stop,
	.load		= rproc_elf_load_segments,
	.parse_fw	= zynqmp_r5_parse_fw,
	.find_loaded_rsc_table = rproc_elf_find_loaded_rsc_table,
	.sanity_check	= rproc_elf_sanity_check,
	.get_boot_addr	= rproc_elf_get_boot_addr,
	.kick		= zynqmp_r5_rproc_kick,
};

 
static struct zynqmp_r5_core *zynqmp_r5_add_rproc_core(struct device *cdev)
{
	struct zynqmp_r5_core *r5_core;
	struct rproc *r5_rproc;
	int ret;

	 
	ret = dma_set_coherent_mask(cdev, DMA_BIT_MASK(32));
	if (ret)
		return ERR_PTR(ret);

	 
	r5_rproc = rproc_alloc(cdev, dev_name(cdev),
			       &zynqmp_r5_rproc_ops,
			       NULL, sizeof(struct zynqmp_r5_core));
	if (!r5_rproc) {
		dev_err(cdev, "failed to allocate memory for rproc instance\n");
		return ERR_PTR(-ENOMEM);
	}

	r5_rproc->auto_boot = false;
	r5_core = r5_rproc->priv;
	r5_core->dev = cdev;
	r5_core->np = dev_of_node(cdev);
	if (!r5_core->np) {
		dev_err(cdev, "can't get device node for r5 core\n");
		ret = -EINVAL;
		goto free_rproc;
	}

	 
	ret = rproc_add(r5_rproc);
	if (ret) {
		dev_err(cdev, "failed to add r5 remoteproc\n");
		goto free_rproc;
	}

	r5_core->rproc = r5_rproc;
	return r5_core;

free_rproc:
	rproc_free(r5_rproc);
	return ERR_PTR(ret);
}

 
static int zynqmp_r5_get_tcm_node(struct zynqmp_r5_cluster *cluster)
{
	struct device *dev = cluster->dev;
	struct zynqmp_r5_core *r5_core;
	int tcm_bank_count, tcm_node;
	int i, j;

	tcm_bank_count = ARRAY_SIZE(zynqmp_tcm_banks);

	 
	tcm_bank_count = tcm_bank_count / cluster->core_count;

	 
	tcm_node = 0;
	for (i = 0; i < cluster->core_count; i++) {
		r5_core = cluster->r5_cores[i];
		r5_core->tcm_banks = devm_kcalloc(dev, tcm_bank_count,
						  sizeof(struct mem_bank_data *),
						  GFP_KERNEL);
		if (!r5_core->tcm_banks)
			return -ENOMEM;

		for (j = 0; j < tcm_bank_count; j++) {
			 
			r5_core->tcm_banks[j] =
				(struct mem_bank_data *)&zynqmp_tcm_banks[tcm_node];
			tcm_node++;
		}

		r5_core->tcm_bank_count = tcm_bank_count;
	}

	return 0;
}

 
static int zynqmp_r5_core_init(struct zynqmp_r5_cluster *cluster,
			       enum rpu_oper_mode fw_reg_val,
			       enum rpu_tcm_comb tcm_mode)
{
	struct device *dev = cluster->dev;
	struct zynqmp_r5_core *r5_core;
	int ret, i;

	ret = zynqmp_r5_get_tcm_node(cluster);
	if (ret < 0) {
		dev_err(dev, "can't get tcm node, err %d\n", ret);
		return ret;
	}

	for (i = 0; i < cluster->core_count; i++) {
		r5_core = cluster->r5_cores[i];

		 
		ret = of_property_read_u32_index(r5_core->np, "power-domains",
						 1, &r5_core->pm_domain_id);
		if (ret) {
			dev_err(dev, "failed to get power-domains property\n");
			return ret;
		}

		ret = zynqmp_r5_set_mode(r5_core, fw_reg_val, tcm_mode);
		if (ret) {
			dev_err(dev, "failed to set r5 cluster mode %d, err %d\n",
				cluster->mode, ret);
			return ret;
		}
	}

	return 0;
}

 
static int zynqmp_r5_cluster_init(struct zynqmp_r5_cluster *cluster)
{
	enum zynqmp_r5_cluster_mode cluster_mode = LOCKSTEP_MODE;
	struct device *dev = cluster->dev;
	struct device_node *dev_node = dev_of_node(dev);
	struct platform_device *child_pdev;
	struct zynqmp_r5_core **r5_cores;
	enum rpu_oper_mode fw_reg_val;
	struct device **child_devs;
	struct device_node *child;
	enum rpu_tcm_comb tcm_mode;
	int core_count, ret, i;
	struct mbox_info *ipi;

	ret = of_property_read_u32(dev_node, "xlnx,cluster-mode", &cluster_mode);

	 
	if (ret != -EINVAL && ret != 0) {
		dev_err(dev, "Invalid xlnx,cluster-mode property\n");
		return ret;
	}

	 
	if (cluster_mode == LOCKSTEP_MODE) {
		tcm_mode = PM_RPU_TCM_COMB;
		fw_reg_val = PM_RPU_MODE_LOCKSTEP;
	} else if (cluster_mode == SPLIT_MODE) {
		tcm_mode = PM_RPU_TCM_SPLIT;
		fw_reg_val = PM_RPU_MODE_SPLIT;
	} else {
		dev_err(dev, "driver does not support cluster mode %d\n", cluster_mode);
		return -EINVAL;
	}

	 
	core_count = of_get_available_child_count(dev_node);
	if (core_count == 0) {
		dev_err(dev, "Invalid number of r5 cores %d", core_count);
		return -EINVAL;
	} else if (cluster_mode == SPLIT_MODE && core_count != 2) {
		dev_err(dev, "Invalid number of r5 cores for split mode\n");
		return -EINVAL;
	} else if (cluster_mode == LOCKSTEP_MODE && core_count == 2) {
		dev_warn(dev, "Only r5 core0 will be used\n");
		core_count = 1;
	}

	child_devs = kcalloc(core_count, sizeof(struct device *), GFP_KERNEL);
	if (!child_devs)
		return -ENOMEM;

	r5_cores = kcalloc(core_count,
			   sizeof(struct zynqmp_r5_core *), GFP_KERNEL);
	if (!r5_cores) {
		kfree(child_devs);
		return -ENOMEM;
	}

	i = 0;
	for_each_available_child_of_node(dev_node, child) {
		child_pdev = of_find_device_by_node(child);
		if (!child_pdev) {
			of_node_put(child);
			ret = -ENODEV;
			goto release_r5_cores;
		}

		child_devs[i] = &child_pdev->dev;

		 
		r5_cores[i] = zynqmp_r5_add_rproc_core(&child_pdev->dev);
		if (IS_ERR(r5_cores[i])) {
			of_node_put(child);
			ret = PTR_ERR(r5_cores[i]);
			r5_cores[i] = NULL;
			goto release_r5_cores;
		}

		 
		ipi = zynqmp_r5_setup_mbox(&child_pdev->dev);
		if (ipi) {
			r5_cores[i]->ipi = ipi;
			ipi->r5_core = r5_cores[i];
		}

		 
		if (cluster_mode == LOCKSTEP_MODE) {
			of_node_put(child);
			break;
		}

		i++;
	}

	cluster->mode = cluster_mode;
	cluster->core_count = core_count;
	cluster->r5_cores = r5_cores;

	ret = zynqmp_r5_core_init(cluster, fw_reg_val, tcm_mode);
	if (ret < 0) {
		dev_err(dev, "failed to init r5 core err %d\n", ret);
		cluster->core_count = 0;
		cluster->r5_cores = NULL;

		 
		i = core_count - 1;
		goto release_r5_cores;
	}

	kfree(child_devs);
	return 0;

release_r5_cores:
	while (i >= 0) {
		put_device(child_devs[i]);
		if (r5_cores[i]) {
			zynqmp_r5_free_mbox(r5_cores[i]->ipi);
			of_reserved_mem_device_release(r5_cores[i]->dev);
			rproc_del(r5_cores[i]->rproc);
			rproc_free(r5_cores[i]->rproc);
		}
		i--;
	}
	kfree(r5_cores);
	kfree(child_devs);
	return ret;
}

static void zynqmp_r5_cluster_exit(void *data)
{
	struct platform_device *pdev = data;
	struct zynqmp_r5_cluster *cluster;
	struct zynqmp_r5_core *r5_core;
	int i;

	cluster = platform_get_drvdata(pdev);
	if (!cluster)
		return;

	for (i = 0; i < cluster->core_count; i++) {
		r5_core = cluster->r5_cores[i];
		zynqmp_r5_free_mbox(r5_core->ipi);
		of_reserved_mem_device_release(r5_core->dev);
		put_device(r5_core->dev);
		rproc_del(r5_core->rproc);
		rproc_free(r5_core->rproc);
	}

	kfree(cluster->r5_cores);
	kfree(cluster);
	platform_set_drvdata(pdev, NULL);
}

 
static int zynqmp_r5_remoteproc_probe(struct platform_device *pdev)
{
	struct zynqmp_r5_cluster *cluster;
	struct device *dev = &pdev->dev;
	int ret;

	cluster = kzalloc(sizeof(*cluster), GFP_KERNEL);
	if (!cluster)
		return -ENOMEM;

	cluster->dev = dev;

	ret = devm_of_platform_populate(dev);
	if (ret) {
		dev_err_probe(dev, ret, "failed to populate platform dev\n");
		kfree(cluster);
		return ret;
	}

	 
	platform_set_drvdata(pdev, cluster);

	ret = zynqmp_r5_cluster_init(cluster);
	if (ret) {
		kfree(cluster);
		platform_set_drvdata(pdev, NULL);
		dev_err_probe(dev, ret, "Invalid r5f subsystem device tree\n");
		return ret;
	}

	ret = devm_add_action_or_reset(dev, zynqmp_r5_cluster_exit, pdev);
	if (ret)
		return ret;

	return 0;
}

 
static const struct of_device_id zynqmp_r5_remoteproc_match[] = {
	{ .compatible = "xlnx,zynqmp-r5fss", },
	{   },
};
MODULE_DEVICE_TABLE(of, zynqmp_r5_remoteproc_match);

static struct platform_driver zynqmp_r5_remoteproc_driver = {
	.probe = zynqmp_r5_remoteproc_probe,
	.driver = {
		.name = "zynqmp_r5_remoteproc",
		.of_match_table = zynqmp_r5_remoteproc_match,
	},
};
module_platform_driver(zynqmp_r5_remoteproc_driver);

MODULE_DESCRIPTION("Xilinx R5F remote processor driver");
MODULE_AUTHOR("Xilinx Inc.");
MODULE_LICENSE("GPL");
