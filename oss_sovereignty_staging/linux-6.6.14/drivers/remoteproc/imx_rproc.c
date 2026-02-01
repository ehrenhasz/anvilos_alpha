
 

#include <dt-bindings/firmware/imx/rsrc.h>
#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/firmware/imx/sci.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/regmap.h>
#include <linux/remoteproc.h>
#include <linux/workqueue.h>

#include "imx_rproc.h"
#include "remoteproc_internal.h"

#define IMX7D_SRC_SCR			0x0C
#define IMX7D_ENABLE_M4			BIT(3)
#define IMX7D_SW_M4P_RST		BIT(2)
#define IMX7D_SW_M4C_RST		BIT(1)
#define IMX7D_SW_M4C_NON_SCLR_RST	BIT(0)

#define IMX7D_M4_RST_MASK		(IMX7D_ENABLE_M4 | IMX7D_SW_M4P_RST \
					 | IMX7D_SW_M4C_RST \
					 | IMX7D_SW_M4C_NON_SCLR_RST)

#define IMX7D_M4_START			(IMX7D_ENABLE_M4 | IMX7D_SW_M4P_RST \
					 | IMX7D_SW_M4C_RST)
#define IMX7D_M4_STOP			(IMX7D_ENABLE_M4 | IMX7D_SW_M4C_RST | \
					 IMX7D_SW_M4C_NON_SCLR_RST)

#define IMX8M_M7_STOP			(IMX7D_ENABLE_M4 | IMX7D_SW_M4C_RST)
#define IMX8M_M7_POLL			IMX7D_ENABLE_M4

#define IMX8M_GPR22			0x58
#define IMX8M_GPR22_CM7_CPUWAIT		BIT(0)

 
#define IMX6SX_SRC_SCR			0x00
#define IMX6SX_ENABLE_M4		BIT(22)
#define IMX6SX_SW_M4P_RST		BIT(12)
#define IMX6SX_SW_M4C_NON_SCLR_RST	BIT(4)
#define IMX6SX_SW_M4C_RST		BIT(3)

#define IMX6SX_M4_START			(IMX6SX_ENABLE_M4 | IMX6SX_SW_M4P_RST \
					 | IMX6SX_SW_M4C_RST)
#define IMX6SX_M4_STOP			(IMX6SX_ENABLE_M4 | IMX6SX_SW_M4C_RST | \
					 IMX6SX_SW_M4C_NON_SCLR_RST)
#define IMX6SX_M4_RST_MASK		(IMX6SX_ENABLE_M4 | IMX6SX_SW_M4P_RST \
					 | IMX6SX_SW_M4C_NON_SCLR_RST \
					 | IMX6SX_SW_M4C_RST)

#define IMX_RPROC_MEM_MAX		32

#define IMX_SIP_RPROC			0xC2000005
#define IMX_SIP_RPROC_START		0x00
#define IMX_SIP_RPROC_STARTED		0x01
#define IMX_SIP_RPROC_STOP		0x02

#define IMX_SC_IRQ_GROUP_REBOOTED	5

 
struct imx_rproc_mem {
	void __iomem *cpu_addr;
	phys_addr_t sys_addr;
	size_t size;
};

 
 
#define ATT_OWN         BIT(31)
#define ATT_IOMEM       BIT(30)

#define ATT_CORE_MASK   0xffff
#define ATT_CORE(I)     BIT((I))

static int imx_rproc_xtr_mbox_init(struct rproc *rproc);
static void imx_rproc_free_mbox(struct rproc *rproc);
static int imx_rproc_detach_pd(struct rproc *rproc);

struct imx_rproc {
	struct device			*dev;
	struct regmap			*regmap;
	struct regmap			*gpr;
	struct rproc			*rproc;
	const struct imx_rproc_dcfg	*dcfg;
	struct imx_rproc_mem		mem[IMX_RPROC_MEM_MAX];
	struct clk			*clk;
	struct mbox_client		cl;
	struct mbox_chan		*tx_ch;
	struct mbox_chan		*rx_ch;
	struct work_struct		rproc_work;
	struct workqueue_struct		*workqueue;
	void __iomem			*rsc_table;
	struct imx_sc_ipc		*ipc_handle;
	struct notifier_block		rproc_nb;
	u32				rproc_pt;	 
	u32				rsrc_id;	 
	u32				entry;		 
	int                             num_pd;
	u32				core_index;
	struct device                   **pd_dev;
	struct device_link              **pd_dev_link;
};

static const struct imx_rproc_att imx_rproc_att_imx93[] = {
	 
	 
	{ 0x0FFC0000, 0x201C0000, 0x00020000, ATT_OWN | ATT_IOMEM },
	{ 0x0FFE0000, 0x201E0000, 0x00020000, ATT_OWN | ATT_IOMEM },

	 
	{ 0x1FFC0000, 0x201C0000, 0x00020000, ATT_OWN | ATT_IOMEM },
	{ 0x1FFE0000, 0x201E0000, 0x00020000, ATT_OWN | ATT_IOMEM },

	 
	{ 0x20000000, 0x20200000, 0x00020000, ATT_OWN | ATT_IOMEM },
	{ 0x20020000, 0x20220000, 0x00020000, ATT_OWN | ATT_IOMEM },

	 
	{ 0x30000000, 0x20200000, 0x00020000, ATT_OWN | ATT_IOMEM },
	{ 0x30020000, 0x20220000, 0x00020000, ATT_OWN | ATT_IOMEM },

	 
	{ 0x80000000, 0x80000000, 0x10000000, 0 },
	{ 0x90000000, 0x80000000, 0x10000000, 0 },

	{ 0xC0000000, 0xC0000000, 0x10000000, 0 },
	{ 0xD0000000, 0xC0000000, 0x10000000, 0 },
};

static const struct imx_rproc_att imx_rproc_att_imx8qm[] = {
	 
	{ 0x08000000, 0x08000000, 0x10000000, 0},
	 
	{ 0x1FFE0000, 0x34FE0000, 0x00020000, ATT_OWN | ATT_IOMEM | ATT_CORE(0)},
	{ 0x1FFE0000, 0x38FE0000, 0x00020000, ATT_OWN | ATT_IOMEM | ATT_CORE(1)},
	 
	{ 0x20000000, 0x35000000, 0x00020000, ATT_OWN | ATT_IOMEM | ATT_CORE(0)},
	{ 0x20000000, 0x39000000, 0x00020000, ATT_OWN | ATT_IOMEM | ATT_CORE(1)},
	 
	{ 0x80000000, 0x80000000, 0x60000000, 0 },
};

static const struct imx_rproc_att imx_rproc_att_imx8qxp[] = {
	{ 0x08000000, 0x08000000, 0x10000000, 0 },
	 
	{ 0x1FFE0000, 0x34FE0000, 0x00040000, ATT_OWN | ATT_IOMEM },
	 
	{ 0x21000000, 0x00100000, 0x00018000, 0 },
	 
	{ 0x21100000, 0x00100000, 0x00040000, 0 },
	 
	{ 0x80000000, 0x80000000, 0x60000000, 0 },
};

static const struct imx_rproc_att imx_rproc_att_imx8mn[] = {
	 
	 
	{ 0x00000000, 0x007E0000, 0x00020000, ATT_OWN | ATT_IOMEM },
	 
	{ 0x00180000, 0x00180000, 0x00009000, 0 },
	 
	{ 0x00900000, 0x00900000, 0x00020000, 0 },
	 
	{ 0x00920000, 0x00920000, 0x00020000, 0 },
	 
	{ 0x00940000, 0x00940000, 0x00050000, 0 },
	 
	{ 0x08000000, 0x08000000, 0x08000000, 0 },
	 
	{ 0x10000000, 0x40000000, 0x0FFE0000, 0 },
	 
	{ 0x20000000, 0x00800000, 0x00020000, ATT_OWN | ATT_IOMEM },
	 
	{ 0x20180000, 0x00180000, 0x00008000, ATT_OWN },
	 
	{ 0x20200000, 0x00900000, 0x00020000, ATT_OWN },
	 
	{ 0x20220000, 0x00920000, 0x00020000, ATT_OWN },
	 
	{ 0x20240000, 0x00940000, 0x00040000, ATT_OWN },
	 
	{ 0x40000000, 0x40000000, 0x80000000, 0 },
};

static const struct imx_rproc_att imx_rproc_att_imx8mq[] = {
	 
	 
	{ 0x00000000, 0x007e0000, 0x00020000, ATT_IOMEM},
	 
	{ 0x00180000, 0x00180000, 0x00008000, 0 },
	 
	{ 0x00900000, 0x00900000, 0x00020000, 0 },
	 
	{ 0x00920000, 0x00920000, 0x00020000, 0 },
	 
	{ 0x08000000, 0x08000000, 0x08000000, 0 },
	 
	{ 0x10000000, 0x80000000, 0x0FFE0000, 0 },
	 
	{ 0x1FFE0000, 0x007E0000, 0x00020000, ATT_OWN  | ATT_IOMEM},
	 
	{ 0x20000000, 0x00800000, 0x00020000, ATT_OWN  | ATT_IOMEM},
	 
	{ 0x20180000, 0x00180000, 0x00008000, ATT_OWN },
	 
	{ 0x20200000, 0x00900000, 0x00020000, ATT_OWN },
	 
	{ 0x20220000, 0x00920000, 0x00020000, ATT_OWN },
	 
	{ 0x40000000, 0x40000000, 0x80000000, 0 },
};

static const struct imx_rproc_att imx_rproc_att_imx8ulp[] = {
	{0x1FFC0000, 0x1FFC0000, 0xC0000, ATT_OWN},
	{0x21000000, 0x21000000, 0x10000, ATT_OWN},
	{0x80000000, 0x80000000, 0x60000000, 0}
};

static const struct imx_rproc_att imx_rproc_att_imx7ulp[] = {
	{0x1FFD0000, 0x1FFD0000, 0x30000, ATT_OWN},
	{0x20000000, 0x20000000, 0x10000, ATT_OWN},
	{0x2F000000, 0x2F000000, 0x20000, ATT_OWN},
	{0x2F020000, 0x2F020000, 0x20000, ATT_OWN},
	{0x60000000, 0x60000000, 0x40000000, 0}
};

static const struct imx_rproc_att imx_rproc_att_imx7d[] = {
	 
	 
	{ 0x00000000, 0x00180000, 0x00008000, 0 },
	 
	{ 0x00180000, 0x00180000, 0x00008000, ATT_OWN },
	 
	{ 0x00900000, 0x00900000, 0x00020000, 0 },
	 
	{ 0x00920000, 0x00920000, 0x00020000, 0 },
	 
	{ 0x00940000, 0x00940000, 0x00008000, 0 },
	 
	{ 0x1FFF8000, 0x007F8000, 0x00008000, ATT_OWN | ATT_IOMEM },
	 
	{ 0x10000000, 0x80000000, 0x0FFF0000, 0 },

	 
	{ 0x20000000, 0x00800000, 0x00008000, ATT_OWN | ATT_IOMEM },
	 
	{ 0x20200000, 0x00900000, 0x00020000, 0 },
	 
	{ 0x20220000, 0x00920000, 0x00020000, 0 },
	 
	{ 0x20240000, 0x00940000, 0x00008000, 0 },
	 
	{ 0x80000000, 0x80000000, 0x60000000, 0 },
};

static const struct imx_rproc_att imx_rproc_att_imx6sx[] = {
	 
	 
	{ 0x00000000, 0x007F8000, 0x00008000, ATT_IOMEM },
	 
	{ 0x00180000, 0x008F8000, 0x00004000, 0 },
	 
	{ 0x00180000, 0x008FC000, 0x00004000, 0 },
	 
	{ 0x1FFF8000, 0x007F8000, 0x00008000, ATT_OWN | ATT_IOMEM },
	 
	{ 0x10000000, 0x80000000, 0x0FFF8000, 0 },

	 
	{ 0x20000000, 0x00800000, 0x00008000, ATT_OWN | ATT_IOMEM },
	 
	{ 0x208F8000, 0x008F8000, 0x00004000, 0 },
	 
	{ 0x80000000, 0x80000000, 0x60000000, 0 },
};

static const struct imx_rproc_dcfg imx_rproc_cfg_imx8mn_mmio = {
	.src_reg	= IMX7D_SRC_SCR,
	.src_mask	= IMX7D_M4_RST_MASK,
	.src_start	= IMX7D_M4_START,
	.src_stop	= IMX8M_M7_STOP,
	.gpr_reg	= IMX8M_GPR22,
	.gpr_wait	= IMX8M_GPR22_CM7_CPUWAIT,
	.att		= imx_rproc_att_imx8mn,
	.att_size	= ARRAY_SIZE(imx_rproc_att_imx8mn),
	.method		= IMX_RPROC_MMIO,
};

static const struct imx_rproc_dcfg imx_rproc_cfg_imx8mn = {
	.att		= imx_rproc_att_imx8mn,
	.att_size	= ARRAY_SIZE(imx_rproc_att_imx8mn),
	.method		= IMX_RPROC_SMC,
};

static const struct imx_rproc_dcfg imx_rproc_cfg_imx8mq = {
	.src_reg	= IMX7D_SRC_SCR,
	.src_mask	= IMX7D_M4_RST_MASK,
	.src_start	= IMX7D_M4_START,
	.src_stop	= IMX7D_M4_STOP,
	.att		= imx_rproc_att_imx8mq,
	.att_size	= ARRAY_SIZE(imx_rproc_att_imx8mq),
	.method		= IMX_RPROC_MMIO,
};

static const struct imx_rproc_dcfg imx_rproc_cfg_imx8qm = {
	.att            = imx_rproc_att_imx8qm,
	.att_size       = ARRAY_SIZE(imx_rproc_att_imx8qm),
	.method         = IMX_RPROC_SCU_API,
};

static const struct imx_rproc_dcfg imx_rproc_cfg_imx8qxp = {
	.att		= imx_rproc_att_imx8qxp,
	.att_size	= ARRAY_SIZE(imx_rproc_att_imx8qxp),
	.method		= IMX_RPROC_SCU_API,
};

static const struct imx_rproc_dcfg imx_rproc_cfg_imx8ulp = {
	.att		= imx_rproc_att_imx8ulp,
	.att_size	= ARRAY_SIZE(imx_rproc_att_imx8ulp),
	.method		= IMX_RPROC_NONE,
};

static const struct imx_rproc_dcfg imx_rproc_cfg_imx7ulp = {
	.att		= imx_rproc_att_imx7ulp,
	.att_size	= ARRAY_SIZE(imx_rproc_att_imx7ulp),
	.method		= IMX_RPROC_NONE,
};

static const struct imx_rproc_dcfg imx_rproc_cfg_imx7d = {
	.src_reg	= IMX7D_SRC_SCR,
	.src_mask	= IMX7D_M4_RST_MASK,
	.src_start	= IMX7D_M4_START,
	.src_stop	= IMX7D_M4_STOP,
	.att		= imx_rproc_att_imx7d,
	.att_size	= ARRAY_SIZE(imx_rproc_att_imx7d),
	.method		= IMX_RPROC_MMIO,
};

static const struct imx_rproc_dcfg imx_rproc_cfg_imx6sx = {
	.src_reg	= IMX6SX_SRC_SCR,
	.src_mask	= IMX6SX_M4_RST_MASK,
	.src_start	= IMX6SX_M4_START,
	.src_stop	= IMX6SX_M4_STOP,
	.att		= imx_rproc_att_imx6sx,
	.att_size	= ARRAY_SIZE(imx_rproc_att_imx6sx),
	.method		= IMX_RPROC_MMIO,
};

static const struct imx_rproc_dcfg imx_rproc_cfg_imx93 = {
	.att		= imx_rproc_att_imx93,
	.att_size	= ARRAY_SIZE(imx_rproc_att_imx93),
	.method		= IMX_RPROC_SMC,
};

static int imx_rproc_start(struct rproc *rproc)
{
	struct imx_rproc *priv = rproc->priv;
	const struct imx_rproc_dcfg *dcfg = priv->dcfg;
	struct device *dev = priv->dev;
	struct arm_smccc_res res;
	int ret;

	ret = imx_rproc_xtr_mbox_init(rproc);
	if (ret)
		return ret;

	switch (dcfg->method) {
	case IMX_RPROC_MMIO:
		if (priv->gpr) {
			ret = regmap_clear_bits(priv->gpr, dcfg->gpr_reg,
						dcfg->gpr_wait);
		} else {
			ret = regmap_update_bits(priv->regmap, dcfg->src_reg,
						 dcfg->src_mask,
						 dcfg->src_start);
		}
		break;
	case IMX_RPROC_SMC:
		arm_smccc_smc(IMX_SIP_RPROC, IMX_SIP_RPROC_START, 0, 0, 0, 0, 0, 0, &res);
		ret = res.a0;
		break;
	case IMX_RPROC_SCU_API:
		ret = imx_sc_pm_cpu_start(priv->ipc_handle, priv->rsrc_id, true, priv->entry);
		break;
	default:
		return -EOPNOTSUPP;
	}

	if (ret)
		dev_err(dev, "Failed to enable remote core!\n");

	return ret;
}

static int imx_rproc_stop(struct rproc *rproc)
{
	struct imx_rproc *priv = rproc->priv;
	const struct imx_rproc_dcfg *dcfg = priv->dcfg;
	struct device *dev = priv->dev;
	struct arm_smccc_res res;
	int ret;

	switch (dcfg->method) {
	case IMX_RPROC_MMIO:
		if (priv->gpr) {
			ret = regmap_set_bits(priv->gpr, dcfg->gpr_reg,
					      dcfg->gpr_wait);
			if (ret) {
				dev_err(priv->dev,
					"Failed to quiescence M4 platform!\n");
				return ret;
			}
		}

		ret = regmap_update_bits(priv->regmap, dcfg->src_reg, dcfg->src_mask,
					 dcfg->src_stop);
		break;
	case IMX_RPROC_SMC:
		arm_smccc_smc(IMX_SIP_RPROC, IMX_SIP_RPROC_STOP, 0, 0, 0, 0, 0, 0, &res);
		ret = res.a0;
		if (res.a1)
			dev_info(dev, "Not in wfi, force stopped\n");
		break;
	case IMX_RPROC_SCU_API:
		ret = imx_sc_pm_cpu_start(priv->ipc_handle, priv->rsrc_id, false, priv->entry);
		break;
	default:
		return -EOPNOTSUPP;
	}

	if (ret)
		dev_err(dev, "Failed to stop remote core\n");
	else
		imx_rproc_free_mbox(rproc);

	return ret;
}

static int imx_rproc_da_to_sys(struct imx_rproc *priv, u64 da,
			       size_t len, u64 *sys, bool *is_iomem)
{
	const struct imx_rproc_dcfg *dcfg = priv->dcfg;
	int i;

	 
	for (i = 0; i < dcfg->att_size; i++) {
		const struct imx_rproc_att *att = &dcfg->att[i];

		 
		if (att->flags & ATT_CORE_MASK) {
			if (!((BIT(priv->core_index)) & (att->flags & ATT_CORE_MASK)))
				continue;
		}

		if (da >= att->da && da + len < att->da + att->size) {
			unsigned int offset = da - att->da;

			*sys = att->sa + offset;
			if (is_iomem)
				*is_iomem = att->flags & ATT_IOMEM;
			return 0;
		}
	}

	dev_warn(priv->dev, "Translation failed: da = 0x%llx len = 0x%zx\n",
		 da, len);
	return -ENOENT;
}

static void *imx_rproc_da_to_va(struct rproc *rproc, u64 da, size_t len, bool *is_iomem)
{
	struct imx_rproc *priv = rproc->priv;
	void *va = NULL;
	u64 sys;
	int i;

	if (len == 0)
		return NULL;

	 
	if (imx_rproc_da_to_sys(priv, da, len, &sys, is_iomem))
		return NULL;

	for (i = 0; i < IMX_RPROC_MEM_MAX; i++) {
		if (sys >= priv->mem[i].sys_addr && sys + len <
		    priv->mem[i].sys_addr +  priv->mem[i].size) {
			unsigned int offset = sys - priv->mem[i].sys_addr;
			 
			va = (__force void *)(priv->mem[i].cpu_addr + offset);
			break;
		}
	}

	dev_dbg(&rproc->dev, "da = 0x%llx len = 0x%zx va = 0x%p\n",
		da, len, va);

	return va;
}

static int imx_rproc_mem_alloc(struct rproc *rproc,
			       struct rproc_mem_entry *mem)
{
	struct device *dev = rproc->dev.parent;
	void *va;

	dev_dbg(dev, "map memory: %p+%zx\n", &mem->dma, mem->len);
	va = ioremap_wc(mem->dma, mem->len);
	if (IS_ERR_OR_NULL(va)) {
		dev_err(dev, "Unable to map memory region: %p+%zx\n",
			&mem->dma, mem->len);
		return -ENOMEM;
	}

	 
	mem->va = va;

	return 0;
}

static int imx_rproc_mem_release(struct rproc *rproc,
				 struct rproc_mem_entry *mem)
{
	dev_dbg(rproc->dev.parent, "unmap memory: %pa\n", &mem->dma);
	iounmap(mem->va);

	return 0;
}

static int imx_rproc_prepare(struct rproc *rproc)
{
	struct imx_rproc *priv = rproc->priv;
	struct device_node *np = priv->dev->of_node;
	struct of_phandle_iterator it;
	struct rproc_mem_entry *mem;
	struct reserved_mem *rmem;
	u32 da;

	 
	of_phandle_iterator_init(&it, np, "memory-region", NULL, 0);
	while (of_phandle_iterator_next(&it) == 0) {
		 
		if (!strcmp(it.node->name, "vdev0buffer"))
			continue;

		if (!strcmp(it.node->name, "rsc-table"))
			continue;

		rmem = of_reserved_mem_lookup(it.node);
		if (!rmem) {
			of_node_put(it.node);
			dev_err(priv->dev, "unable to acquire memory-region\n");
			return -EINVAL;
		}

		 
		da = rmem->base;

		 
		mem = rproc_mem_entry_init(priv->dev, NULL, (dma_addr_t)rmem->base, rmem->size, da,
					   imx_rproc_mem_alloc, imx_rproc_mem_release,
					   it.node->name);

		if (mem) {
			rproc_coredump_add_segment(rproc, da, rmem->size);
		} else {
			of_node_put(it.node);
			return -ENOMEM;
		}

		rproc_add_carveout(rproc, mem);
	}

	return  0;
}

static int imx_rproc_parse_fw(struct rproc *rproc, const struct firmware *fw)
{
	int ret;

	ret = rproc_elf_load_rsc_table(rproc, fw);
	if (ret)
		dev_info(&rproc->dev, "No resource table in elf\n");

	return 0;
}

static void imx_rproc_kick(struct rproc *rproc, int vqid)
{
	struct imx_rproc *priv = rproc->priv;
	int err;
	__u32 mmsg;

	if (!priv->tx_ch) {
		dev_err(priv->dev, "No initialized mbox tx channel\n");
		return;
	}

	 
	mmsg = vqid << 16;

	err = mbox_send_message(priv->tx_ch, (void *)&mmsg);
	if (err < 0)
		dev_err(priv->dev, "%s: failed (%d, err:%d)\n",
			__func__, vqid, err);
}

static int imx_rproc_attach(struct rproc *rproc)
{
	return imx_rproc_xtr_mbox_init(rproc);
}

static int imx_rproc_detach(struct rproc *rproc)
{
	struct imx_rproc *priv = rproc->priv;
	const struct imx_rproc_dcfg *dcfg = priv->dcfg;

	if (dcfg->method != IMX_RPROC_SCU_API)
		return -EOPNOTSUPP;

	if (imx_sc_rm_is_resource_owned(priv->ipc_handle, priv->rsrc_id))
		return -EOPNOTSUPP;

	imx_rproc_free_mbox(rproc);

	return 0;
}

static struct resource_table *imx_rproc_get_loaded_rsc_table(struct rproc *rproc, size_t *table_sz)
{
	struct imx_rproc *priv = rproc->priv;

	 
	if (!priv->rsc_table)
		return NULL;

	*table_sz = SZ_1K;
	return (struct resource_table *)priv->rsc_table;
}

static const struct rproc_ops imx_rproc_ops = {
	.prepare	= imx_rproc_prepare,
	.attach		= imx_rproc_attach,
	.detach		= imx_rproc_detach,
	.start		= imx_rproc_start,
	.stop		= imx_rproc_stop,
	.kick		= imx_rproc_kick,
	.da_to_va       = imx_rproc_da_to_va,
	.load		= rproc_elf_load_segments,
	.parse_fw	= imx_rproc_parse_fw,
	.find_loaded_rsc_table = rproc_elf_find_loaded_rsc_table,
	.get_loaded_rsc_table = imx_rproc_get_loaded_rsc_table,
	.sanity_check	= rproc_elf_sanity_check,
	.get_boot_addr	= rproc_elf_get_boot_addr,
};

static int imx_rproc_addr_init(struct imx_rproc *priv,
			       struct platform_device *pdev)
{
	const struct imx_rproc_dcfg *dcfg = priv->dcfg;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int a, b = 0, err, nph;

	 
	for (a = 0; a < dcfg->att_size; a++) {
		const struct imx_rproc_att *att = &dcfg->att[a];

		if (!(att->flags & ATT_OWN))
			continue;

		if (b >= IMX_RPROC_MEM_MAX)
			break;

		if (att->flags & ATT_IOMEM)
			priv->mem[b].cpu_addr = devm_ioremap(&pdev->dev,
							     att->sa, att->size);
		else
			priv->mem[b].cpu_addr = devm_ioremap_wc(&pdev->dev,
								att->sa, att->size);
		if (!priv->mem[b].cpu_addr) {
			dev_err(dev, "failed to remap %#x bytes from %#x\n", att->size, att->sa);
			return -ENOMEM;
		}
		priv->mem[b].sys_addr = att->sa;
		priv->mem[b].size = att->size;
		b++;
	}

	 
	nph = of_count_phandle_with_args(np, "memory-region", NULL);
	if (nph <= 0)
		return 0;

	 
	for (a = 0; a < nph; a++) {
		struct device_node *node;
		struct resource res;

		node = of_parse_phandle(np, "memory-region", a);
		 
		if (!strncmp(node->name, "vdev", strlen("vdev"))) {
			of_node_put(node);
			continue;
		}
		err = of_address_to_resource(node, 0, &res);
		of_node_put(node);
		if (err) {
			dev_err(dev, "unable to resolve memory region\n");
			return err;
		}

		if (b >= IMX_RPROC_MEM_MAX)
			break;

		 
		priv->mem[b].cpu_addr = devm_ioremap_wc(&pdev->dev, res.start, resource_size(&res));
		if (!priv->mem[b].cpu_addr) {
			dev_err(dev, "failed to remap %pr\n", &res);
			return -ENOMEM;
		}
		priv->mem[b].sys_addr = res.start;
		priv->mem[b].size = resource_size(&res);
		if (!strcmp(node->name, "rsc-table"))
			priv->rsc_table = priv->mem[b].cpu_addr;
		b++;
	}

	return 0;
}

static int imx_rproc_notified_idr_cb(int id, void *ptr, void *data)
{
	struct rproc *rproc = data;

	rproc_vq_interrupt(rproc, id);

	return 0;
}

static void imx_rproc_vq_work(struct work_struct *work)
{
	struct imx_rproc *priv = container_of(work, struct imx_rproc,
					      rproc_work);
	struct rproc *rproc = priv->rproc;

	idr_for_each(&rproc->notifyids, imx_rproc_notified_idr_cb, rproc);
}

static void imx_rproc_rx_callback(struct mbox_client *cl, void *msg)
{
	struct rproc *rproc = dev_get_drvdata(cl->dev);
	struct imx_rproc *priv = rproc->priv;

	queue_work(priv->workqueue, &priv->rproc_work);
}

static int imx_rproc_xtr_mbox_init(struct rproc *rproc)
{
	struct imx_rproc *priv = rproc->priv;
	struct device *dev = priv->dev;
	struct mbox_client *cl;

	 
	if (priv->tx_ch && priv->rx_ch)
		return 0;

	if (!of_get_property(dev->of_node, "mbox-names", NULL))
		return 0;

	cl = &priv->cl;
	cl->dev = dev;
	cl->tx_block = true;
	cl->tx_tout = 100;
	cl->knows_txdone = false;
	cl->rx_callback = imx_rproc_rx_callback;

	priv->tx_ch = mbox_request_channel_byname(cl, "tx");
	if (IS_ERR(priv->tx_ch))
		return dev_err_probe(cl->dev, PTR_ERR(priv->tx_ch),
				     "failed to request tx mailbox channel\n");

	priv->rx_ch = mbox_request_channel_byname(cl, "rx");
	if (IS_ERR(priv->rx_ch)) {
		mbox_free_channel(priv->tx_ch);
		return dev_err_probe(cl->dev, PTR_ERR(priv->rx_ch),
				     "failed to request rx mailbox channel\n");
	}

	return 0;
}

static void imx_rproc_free_mbox(struct rproc *rproc)
{
	struct imx_rproc *priv = rproc->priv;

	if (priv->tx_ch) {
		mbox_free_channel(priv->tx_ch);
		priv->tx_ch = NULL;
	}

	if (priv->rx_ch) {
		mbox_free_channel(priv->rx_ch);
		priv->rx_ch = NULL;
	}
}

static void imx_rproc_put_scu(struct rproc *rproc)
{
	struct imx_rproc *priv = rproc->priv;
	const struct imx_rproc_dcfg *dcfg = priv->dcfg;

	if (dcfg->method != IMX_RPROC_SCU_API)
		return;

	if (imx_sc_rm_is_resource_owned(priv->ipc_handle, priv->rsrc_id)) {
		imx_rproc_detach_pd(rproc);
		return;
	}

	imx_scu_irq_group_enable(IMX_SC_IRQ_GROUP_REBOOTED, BIT(priv->rproc_pt), false);
	imx_scu_irq_unregister_notifier(&priv->rproc_nb);
}

static int imx_rproc_partition_notify(struct notifier_block *nb,
				      unsigned long event, void *group)
{
	struct imx_rproc *priv = container_of(nb, struct imx_rproc, rproc_nb);

	 
	if (!((event & BIT(priv->rproc_pt)) && (*(u8 *)group == IMX_SC_IRQ_GROUP_REBOOTED)))
		return 0;

	rproc_report_crash(priv->rproc, RPROC_WATCHDOG);

	pr_info("Partition%d reset!\n", priv->rproc_pt);

	return 0;
}

static int imx_rproc_attach_pd(struct imx_rproc *priv)
{
	struct device *dev = priv->dev;
	int ret, i;

	 
	priv->num_pd = of_count_phandle_with_args(dev->of_node, "power-domains",
						  "#power-domain-cells");
	if (priv->num_pd <= 1)
		return 0;

	priv->pd_dev = devm_kmalloc_array(dev, priv->num_pd, sizeof(*priv->pd_dev), GFP_KERNEL);
	if (!priv->pd_dev)
		return -ENOMEM;

	priv->pd_dev_link = devm_kmalloc_array(dev, priv->num_pd, sizeof(*priv->pd_dev_link),
					       GFP_KERNEL);

	if (!priv->pd_dev_link)
		return -ENOMEM;

	for (i = 0; i < priv->num_pd; i++) {
		priv->pd_dev[i] = dev_pm_domain_attach_by_id(dev, i);
		if (IS_ERR(priv->pd_dev[i])) {
			ret = PTR_ERR(priv->pd_dev[i]);
			goto detach_pd;
		}

		priv->pd_dev_link[i] = device_link_add(dev, priv->pd_dev[i], DL_FLAG_STATELESS |
						       DL_FLAG_PM_RUNTIME | DL_FLAG_RPM_ACTIVE);
		if (!priv->pd_dev_link[i]) {
			dev_pm_domain_detach(priv->pd_dev[i], false);
			ret = -EINVAL;
			goto detach_pd;
		}
	}

	return 0;

detach_pd:
	while (--i >= 0) {
		device_link_del(priv->pd_dev_link[i]);
		dev_pm_domain_detach(priv->pd_dev[i], false);
	}

	return ret;
}

static int imx_rproc_detach_pd(struct rproc *rproc)
{
	struct imx_rproc *priv = rproc->priv;
	int i;

	 
	if (priv->num_pd <= 1)
		return 0;

	for (i = 0; i < priv->num_pd; i++) {
		device_link_del(priv->pd_dev_link[i]);
		dev_pm_domain_detach(priv->pd_dev[i], false);
	}

	return 0;
}

static int imx_rproc_detect_mode(struct imx_rproc *priv)
{
	struct regmap_config config = { .name = "imx-rproc" };
	const struct imx_rproc_dcfg *dcfg = priv->dcfg;
	struct device *dev = priv->dev;
	struct regmap *regmap;
	struct arm_smccc_res res;
	int ret;
	u32 val;
	u8 pt;

	switch (dcfg->method) {
	case IMX_RPROC_NONE:
		priv->rproc->state = RPROC_DETACHED;
		return 0;
	case IMX_RPROC_SMC:
		arm_smccc_smc(IMX_SIP_RPROC, IMX_SIP_RPROC_STARTED, 0, 0, 0, 0, 0, 0, &res);
		if (res.a0)
			priv->rproc->state = RPROC_DETACHED;
		return 0;
	case IMX_RPROC_SCU_API:
		ret = imx_scu_get_handle(&priv->ipc_handle);
		if (ret)
			return ret;
		ret = of_property_read_u32(dev->of_node, "fsl,resource-id", &priv->rsrc_id);
		if (ret) {
			dev_err(dev, "No fsl,resource-id property\n");
			return ret;
		}

		if (priv->rsrc_id == IMX_SC_R_M4_1_PID0)
			priv->core_index = 1;
		else
			priv->core_index = 0;

		 
		if (imx_sc_rm_is_resource_owned(priv->ipc_handle, priv->rsrc_id)) {
			if (of_property_read_u32(dev->of_node, "fsl,entry-address", &priv->entry))
				return -EINVAL;

			return imx_rproc_attach_pd(priv);
		}

		priv->rproc->state = RPROC_DETACHED;
		priv->rproc->recovery_disabled = false;
		rproc_set_feature(priv->rproc, RPROC_FEAT_ATTACH_ON_RECOVERY);

		 
		ret = imx_sc_rm_get_resource_owner(priv->ipc_handle, priv->rsrc_id, &pt);
		if (ret) {
			dev_err(dev, "not able to get resource owner\n");
			return ret;
		}

		priv->rproc_pt = pt;
		priv->rproc_nb.notifier_call = imx_rproc_partition_notify;

		ret = imx_scu_irq_register_notifier(&priv->rproc_nb);
		if (ret) {
			dev_err(dev, "register scu notifier failed, %d\n", ret);
			return ret;
		}

		ret = imx_scu_irq_group_enable(IMX_SC_IRQ_GROUP_REBOOTED, BIT(priv->rproc_pt),
					       true);
		if (ret) {
			imx_scu_irq_unregister_notifier(&priv->rproc_nb);
			dev_err(dev, "Enable irq failed, %d\n", ret);
			return ret;
		}

		return 0;
	default:
		break;
	}

	priv->gpr = syscon_regmap_lookup_by_phandle(dev->of_node, "fsl,iomuxc-gpr");
	if (IS_ERR(priv->gpr))
		priv->gpr = NULL;

	regmap = syscon_regmap_lookup_by_phandle(dev->of_node, "syscon");
	if (IS_ERR(regmap)) {
		dev_err(dev, "failed to find syscon\n");
		return PTR_ERR(regmap);
	}

	priv->regmap = regmap;
	regmap_attach_dev(dev, regmap, &config);

	if (priv->gpr) {
		ret = regmap_read(priv->gpr, dcfg->gpr_reg, &val);
		if (val & dcfg->gpr_wait) {
			 
			imx_rproc_stop(priv->rproc);
			return 0;
		}
	}

	ret = regmap_read(regmap, dcfg->src_reg, &val);
	if (ret) {
		dev_err(dev, "Failed to read src\n");
		return ret;
	}

	if ((val & dcfg->src_mask) != dcfg->src_stop)
		priv->rproc->state = RPROC_DETACHED;

	return 0;
}

static int imx_rproc_clk_enable(struct imx_rproc *priv)
{
	const struct imx_rproc_dcfg *dcfg = priv->dcfg;
	struct device *dev = priv->dev;
	int ret;

	 
	if (dcfg->method == IMX_RPROC_NONE)
		return 0;

	priv->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(priv->clk)) {
		dev_err(dev, "Failed to get clock\n");
		return PTR_ERR(priv->clk);
	}

	 
	ret = clk_prepare_enable(priv->clk);
	if (ret) {
		dev_err(dev, "Failed to enable clock\n");
		return ret;
	}

	return 0;
}

static int imx_rproc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct imx_rproc *priv;
	struct rproc *rproc;
	const struct imx_rproc_dcfg *dcfg;
	int ret;

	 
	rproc = rproc_alloc(dev, "imx-rproc", &imx_rproc_ops,
			    NULL, sizeof(*priv));
	if (!rproc)
		return -ENOMEM;

	dcfg = of_device_get_match_data(dev);
	if (!dcfg) {
		ret = -EINVAL;
		goto err_put_rproc;
	}

	priv = rproc->priv;
	priv->rproc = rproc;
	priv->dcfg = dcfg;
	priv->dev = dev;

	dev_set_drvdata(dev, rproc);
	priv->workqueue = create_workqueue(dev_name(dev));
	if (!priv->workqueue) {
		dev_err(dev, "cannot create workqueue\n");
		ret = -ENOMEM;
		goto err_put_rproc;
	}

	ret = imx_rproc_xtr_mbox_init(rproc);
	if (ret)
		goto err_put_wkq;

	ret = imx_rproc_addr_init(priv, pdev);
	if (ret) {
		dev_err(dev, "failed on imx_rproc_addr_init\n");
		goto err_put_mbox;
	}

	ret = imx_rproc_detect_mode(priv);
	if (ret)
		goto err_put_mbox;

	ret = imx_rproc_clk_enable(priv);
	if (ret)
		goto err_put_scu;

	INIT_WORK(&priv->rproc_work, imx_rproc_vq_work);

	if (rproc->state != RPROC_DETACHED)
		rproc->auto_boot = of_property_read_bool(np, "fsl,auto-boot");

	ret = rproc_add(rproc);
	if (ret) {
		dev_err(dev, "rproc_add failed\n");
		goto err_put_clk;
	}

	return 0;

err_put_clk:
	clk_disable_unprepare(priv->clk);
err_put_scu:
	imx_rproc_put_scu(rproc);
err_put_mbox:
	imx_rproc_free_mbox(rproc);
err_put_wkq:
	destroy_workqueue(priv->workqueue);
err_put_rproc:
	rproc_free(rproc);

	return ret;
}

static void imx_rproc_remove(struct platform_device *pdev)
{
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct imx_rproc *priv = rproc->priv;

	clk_disable_unprepare(priv->clk);
	rproc_del(rproc);
	imx_rproc_put_scu(rproc);
	imx_rproc_free_mbox(rproc);
	destroy_workqueue(priv->workqueue);
	rproc_free(rproc);
}

static const struct of_device_id imx_rproc_of_match[] = {
	{ .compatible = "fsl,imx7ulp-cm4", .data = &imx_rproc_cfg_imx7ulp },
	{ .compatible = "fsl,imx7d-cm4", .data = &imx_rproc_cfg_imx7d },
	{ .compatible = "fsl,imx6sx-cm4", .data = &imx_rproc_cfg_imx6sx },
	{ .compatible = "fsl,imx8mq-cm4", .data = &imx_rproc_cfg_imx8mq },
	{ .compatible = "fsl,imx8mm-cm4", .data = &imx_rproc_cfg_imx8mq },
	{ .compatible = "fsl,imx8mn-cm7", .data = &imx_rproc_cfg_imx8mn },
	{ .compatible = "fsl,imx8mp-cm7", .data = &imx_rproc_cfg_imx8mn },
	{ .compatible = "fsl,imx8mn-cm7-mmio", .data = &imx_rproc_cfg_imx8mn_mmio },
	{ .compatible = "fsl,imx8mp-cm7-mmio", .data = &imx_rproc_cfg_imx8mn_mmio },
	{ .compatible = "fsl,imx8qxp-cm4", .data = &imx_rproc_cfg_imx8qxp },
	{ .compatible = "fsl,imx8qm-cm4", .data = &imx_rproc_cfg_imx8qm },
	{ .compatible = "fsl,imx8ulp-cm33", .data = &imx_rproc_cfg_imx8ulp },
	{ .compatible = "fsl,imx93-cm33", .data = &imx_rproc_cfg_imx93 },
	{},
};
MODULE_DEVICE_TABLE(of, imx_rproc_of_match);

static struct platform_driver imx_rproc_driver = {
	.probe = imx_rproc_probe,
	.remove_new = imx_rproc_remove,
	.driver = {
		.name = "imx-rproc",
		.of_match_table = imx_rproc_of_match,
	},
};

module_platform_driver(imx_rproc_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("i.MX remote processor control driver");
MODULE_AUTHOR("Oleksij Rempel <o.rempel@pengutronix.de>");
