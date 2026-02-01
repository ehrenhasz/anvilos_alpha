
 

#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/sys_soc.h>
#include <linux/fsl/mc.h>

#include "compat.h"
#include "debugfs.h"
#include "regs.h"
#include "intern.h"
#include "jr.h"
#include "desc_constr.h"
#include "ctrl.h"

bool caam_dpaa2;
EXPORT_SYMBOL(caam_dpaa2);

#ifdef CONFIG_CAAM_QI
#include "qi.h"
#endif

 
static void build_instantiation_desc(u32 *desc, int handle, int do_sk)
{
	u32 *jump_cmd, op_flags;

	init_job_desc(desc, 0);

	op_flags = OP_TYPE_CLASS1_ALG | OP_ALG_ALGSEL_RNG |
			(handle << OP_ALG_AAI_SHIFT) | OP_ALG_AS_INIT |
			OP_ALG_PR_ON;

	 
	append_operation(desc, op_flags);

	if (!handle && do_sk) {
		 

		 
		jump_cmd = append_jump(desc, JUMP_CLASS_CLASS1);
		set_jump_tgt_here(desc, jump_cmd);

		 
		append_load_imm_u32(desc, 1, LDST_SRCDST_WORD_CLRW);

		 
		append_operation(desc, OP_TYPE_CLASS1_ALG | OP_ALG_ALGSEL_RNG |
				 OP_ALG_AAI_RNG4_SK);
	}

	append_jump(desc, JUMP_CLASS_CLASS1 | JUMP_TYPE_HALT);
}

 
static void build_deinstantiation_desc(u32 *desc, int handle)
{
	init_job_desc(desc, 0);

	 
	append_operation(desc, OP_TYPE_CLASS1_ALG | OP_ALG_ALGSEL_RNG |
			 (handle << OP_ALG_AAI_SHIFT) | OP_ALG_AS_INITFINAL);

	append_jump(desc, JUMP_CLASS_CLASS1 | JUMP_TYPE_HALT);
}

static const struct of_device_id imx8m_machine_match[] = {
	{ .compatible = "fsl,imx8mm", },
	{ .compatible = "fsl,imx8mn", },
	{ .compatible = "fsl,imx8mp", },
	{ .compatible = "fsl,imx8mq", },
	{ .compatible = "fsl,imx8ulp", },
	{ }
};

 
static inline int run_descriptor_deco0(struct device *ctrldev, u32 *desc,
					u32 *status)
{
	struct caam_drv_private *ctrlpriv = dev_get_drvdata(ctrldev);
	struct caam_ctrl __iomem *ctrl = ctrlpriv->ctrl;
	struct caam_deco __iomem *deco = ctrlpriv->deco;
	unsigned int timeout = 100000;
	u32 deco_dbg_reg, deco_state, flags;
	int i;


	if (ctrlpriv->virt_en == 1 ||
	     
	    of_match_node(imx8m_machine_match, of_root)) {
		clrsetbits_32(&ctrl->deco_rsr, 0, DECORSR_JR0);

		while (!(rd_reg32(&ctrl->deco_rsr) & DECORSR_VALID) &&
		       --timeout)
			cpu_relax();

		timeout = 100000;
	}

	clrsetbits_32(&ctrl->deco_rq, 0, DECORR_RQD0ENABLE);

	while (!(rd_reg32(&ctrl->deco_rq) & DECORR_DEN0) &&
								 --timeout)
		cpu_relax();

	if (!timeout) {
		dev_err(ctrldev, "failed to acquire DECO 0\n");
		clrsetbits_32(&ctrl->deco_rq, DECORR_RQD0ENABLE, 0);
		return -ENODEV;
	}

	for (i = 0; i < desc_len(desc); i++)
		wr_reg32(&deco->descbuf[i], caam32_to_cpu(*(desc + i)));

	flags = DECO_JQCR_WHL;
	 
	if (desc_len(desc) >= 4)
		flags |= DECO_JQCR_FOUR;

	 
	clrsetbits_32(&deco->jr_ctl_hi, 0, flags);

	timeout = 10000000;
	do {
		deco_dbg_reg = rd_reg32(&deco->desc_dbg);

		if (ctrlpriv->era < 10)
			deco_state = (deco_dbg_reg & DESC_DBG_DECO_STAT_MASK) >>
				     DESC_DBG_DECO_STAT_SHIFT;
		else
			deco_state = (rd_reg32(&deco->dbg_exec) &
				      DESC_DER_DECO_STAT_MASK) >>
				     DESC_DER_DECO_STAT_SHIFT;

		 
		if (deco_state == DECO_STAT_HOST_ERR)
			break;

		cpu_relax();
	} while ((deco_dbg_reg & DESC_DBG_DECO_STAT_VALID) && --timeout);

	*status = rd_reg32(&deco->op_status_hi) &
		  DECO_OP_STATUS_HI_ERR_MASK;

	if (ctrlpriv->virt_en == 1)
		clrsetbits_32(&ctrl->deco_rsr, DECORSR_JR0, 0);

	 
	clrsetbits_32(&ctrl->deco_rq, DECORR_RQD0ENABLE, 0);

	if (!timeout)
		return -EAGAIN;

	return 0;
}

 
static int deinstantiate_rng(struct device *ctrldev, int state_handle_mask)
{
	u32 *desc, status;
	int sh_idx, ret = 0;

	desc = kmalloc(CAAM_CMD_SZ * 3, GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	for (sh_idx = 0; sh_idx < RNG4_MAX_HANDLES; sh_idx++) {
		 
		if ((1 << sh_idx) & state_handle_mask) {
			 
			build_deinstantiation_desc(desc, sh_idx);

			 
			ret = run_descriptor_deco0(ctrldev, desc, &status);

			if (ret ||
			    (status && status != JRSTA_SSRC_JUMP_HALT_CC)) {
				dev_err(ctrldev,
					"Failed to deinstantiate RNG4 SH%d\n",
					sh_idx);
				break;
			}
			dev_info(ctrldev, "Deinstantiated RNG4 SH%d\n", sh_idx);
		}
	}

	kfree(desc);

	return ret;
}

static void devm_deinstantiate_rng(void *data)
{
	struct device *ctrldev = data;
	struct caam_drv_private *ctrlpriv = dev_get_drvdata(ctrldev);

	 
	if (ctrlpriv->rng4_sh_init)
		deinstantiate_rng(ctrldev, ctrlpriv->rng4_sh_init);
}

 
static int instantiate_rng(struct device *ctrldev, int state_handle_mask,
			   int gen_sk)
{
	struct caam_drv_private *ctrlpriv = dev_get_drvdata(ctrldev);
	struct caam_ctrl __iomem *ctrl;
	u32 *desc, status = 0, rdsta_val;
	int ret = 0, sh_idx;

	ctrl = (struct caam_ctrl __iomem *)ctrlpriv->ctrl;
	desc = kmalloc(CAAM_CMD_SZ * 7, GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	for (sh_idx = 0; sh_idx < RNG4_MAX_HANDLES; sh_idx++) {
		const u32 rdsta_if = RDSTA_IF0 << sh_idx;
		const u32 rdsta_pr = RDSTA_PR0 << sh_idx;
		const u32 rdsta_mask = rdsta_if | rdsta_pr;

		 
		memset(desc, 0x00, CAAM_CMD_SZ * 7);

		 
		if (rdsta_if & state_handle_mask) {
			if (rdsta_pr & state_handle_mask)
				continue;

			dev_info(ctrldev,
				 "RNG4 SH%d was previously instantiated without prediction resistance. Tearing it down\n",
				 sh_idx);

			ret = deinstantiate_rng(ctrldev, rdsta_if);
			if (ret)
				break;
		}

		 
		build_instantiation_desc(desc, sh_idx, gen_sk);

		 
		ret = run_descriptor_deco0(ctrldev, desc, &status);

		 
		if (ret)
			break;

		rdsta_val = rd_reg32(&ctrl->r4tst[0].rdsta) & RDSTA_MASK;
		if ((status && status != JRSTA_SSRC_JUMP_HALT_CC) ||
		    (rdsta_val & rdsta_mask) != rdsta_mask) {
			ret = -EAGAIN;
			break;
		}

		dev_info(ctrldev, "Instantiated RNG4 SH%d\n", sh_idx);
	}

	kfree(desc);

	if (ret)
		return ret;

	return devm_add_action_or_reset(ctrldev, devm_deinstantiate_rng, ctrldev);
}

 
static void kick_trng(struct device *dev, int ent_delay)
{
	struct caam_drv_private *ctrlpriv = dev_get_drvdata(dev);
	struct caam_ctrl __iomem *ctrl;
	struct rng4tst __iomem *r4tst;
	u32 val, rtsdctl;

	ctrl = (struct caam_ctrl __iomem *)ctrlpriv->ctrl;
	r4tst = &ctrl->r4tst[0];

	 
	clrsetbits_32(&r4tst->rtmctl, 0, RTMCTL_PRGM | RTMCTL_ACC);

	 
	rtsdctl = rd_reg32(&r4tst->rtsdctl);
	val = (rtsdctl & RTSDCTL_ENT_DLY_MASK) >> RTSDCTL_ENT_DLY_SHIFT;
	if (ent_delay > val) {
		val = ent_delay;
		 
		wr_reg32(&r4tst->rtfrqmin, val >> 2);
		 
		wr_reg32(&r4tst->rtfrqmax, RTFRQMAX_DISABLE);
	}

	wr_reg32(&r4tst->rtsdctl, (val << RTSDCTL_ENT_DLY_SHIFT) |
		 RTSDCTL_SAMP_SIZE_VAL);

	 
	if ((rtsdctl & RTSDCTL_SAMP_SIZE_MASK) != RTSDCTL_SAMP_SIZE_VAL) {
		wr_reg32(&r4tst->rtscmisc, (2 << 16) | 32);
		wr_reg32(&r4tst->rtpkrrng, 570);
		wr_reg32(&r4tst->rtpkrmax, 1600);
		wr_reg32(&r4tst->rtscml, (122 << 16) | 317);
		wr_reg32(&r4tst->rtscrl[0], (80 << 16) | 107);
		wr_reg32(&r4tst->rtscrl[1], (57 << 16) | 62);
		wr_reg32(&r4tst->rtscrl[2], (39 << 16) | 39);
		wr_reg32(&r4tst->rtscrl[3], (27 << 16) | 26);
		wr_reg32(&r4tst->rtscrl[4], (19 << 16) | 18);
		wr_reg32(&r4tst->rtscrl[5], (18 << 16) | 17);
	}

	 
	clrsetbits_32(&r4tst->rtmctl, RTMCTL_PRGM | RTMCTL_ACC,
		      RTMCTL_SAMP_MODE_RAW_ES_SC);
}

static int caam_get_era_from_hw(struct caam_perfmon __iomem *perfmon)
{
	static const struct {
		u16 ip_id;
		u8 maj_rev;
		u8 era;
	} id[] = {
		{0x0A10, 1, 1},
		{0x0A10, 2, 2},
		{0x0A12, 1, 3},
		{0x0A14, 1, 3},
		{0x0A14, 2, 4},
		{0x0A16, 1, 4},
		{0x0A10, 3, 4},
		{0x0A11, 1, 4},
		{0x0A18, 1, 4},
		{0x0A11, 2, 5},
		{0x0A12, 2, 5},
		{0x0A13, 1, 5},
		{0x0A1C, 1, 5}
	};
	u32 ccbvid, id_ms;
	u8 maj_rev, era;
	u16 ip_id;
	int i;

	ccbvid = rd_reg32(&perfmon->ccb_id);
	era = (ccbvid & CCBVID_ERA_MASK) >> CCBVID_ERA_SHIFT;
	if (era)	 
		return era;

	id_ms = rd_reg32(&perfmon->caam_id_ms);
	ip_id = (id_ms & SECVID_MS_IPID_MASK) >> SECVID_MS_IPID_SHIFT;
	maj_rev = (id_ms & SECVID_MS_MAJ_REV_MASK) >> SECVID_MS_MAJ_REV_SHIFT;

	for (i = 0; i < ARRAY_SIZE(id); i++)
		if (id[i].ip_id == ip_id && id[i].maj_rev == maj_rev)
			return id[i].era;

	return -ENOTSUPP;
}

 
static int caam_get_era(struct caam_perfmon __iomem *perfmon)
{
	struct device_node *caam_node;
	int ret;
	u32 prop;

	caam_node = of_find_compatible_node(NULL, NULL, "fsl,sec-v4.0");
	ret = of_property_read_u32(caam_node, "fsl,sec-era", &prop);
	of_node_put(caam_node);

	if (!ret)
		return prop;
	else
		return caam_get_era_from_hw(perfmon);
}

 
static void handle_imx6_err005766(u32 __iomem *mcr)
{
	if (of_machine_is_compatible("fsl,imx6q") ||
	    of_machine_is_compatible("fsl,imx6dl") ||
	    of_machine_is_compatible("fsl,imx6qp"))
		clrsetbits_32(mcr, MCFGR_AXIPIPE_MASK,
			      1 << MCFGR_AXIPIPE_SHIFT);
}

static const struct of_device_id caam_match[] = {
	{
		.compatible = "fsl,sec-v4.0",
	},
	{
		.compatible = "fsl,sec4.0",
	},
	{},
};
MODULE_DEVICE_TABLE(of, caam_match);

struct caam_imx_data {
	const struct clk_bulk_data *clks;
	int num_clks;
};

static const struct clk_bulk_data caam_imx6_clks[] = {
	{ .id = "ipg" },
	{ .id = "mem" },
	{ .id = "aclk" },
	{ .id = "emi_slow" },
};

static const struct caam_imx_data caam_imx6_data = {
	.clks = caam_imx6_clks,
	.num_clks = ARRAY_SIZE(caam_imx6_clks),
};

static const struct clk_bulk_data caam_imx7_clks[] = {
	{ .id = "ipg" },
	{ .id = "aclk" },
};

static const struct caam_imx_data caam_imx7_data = {
	.clks = caam_imx7_clks,
	.num_clks = ARRAY_SIZE(caam_imx7_clks),
};

static const struct clk_bulk_data caam_imx6ul_clks[] = {
	{ .id = "ipg" },
	{ .id = "mem" },
	{ .id = "aclk" },
};

static const struct caam_imx_data caam_imx6ul_data = {
	.clks = caam_imx6ul_clks,
	.num_clks = ARRAY_SIZE(caam_imx6ul_clks),
};

static const struct clk_bulk_data caam_vf610_clks[] = {
	{ .id = "ipg" },
};

static const struct caam_imx_data caam_vf610_data = {
	.clks = caam_vf610_clks,
	.num_clks = ARRAY_SIZE(caam_vf610_clks),
};

static const struct soc_device_attribute caam_imx_soc_table[] = {
	{ .soc_id = "i.MX6UL", .data = &caam_imx6ul_data },
	{ .soc_id = "i.MX6*",  .data = &caam_imx6_data },
	{ .soc_id = "i.MX7*",  .data = &caam_imx7_data },
	{ .soc_id = "i.MX8M*", .data = &caam_imx7_data },
	{ .soc_id = "VF*",     .data = &caam_vf610_data },
	{ .family = "Freescale i.MX" },
	{   }
};

static void disable_clocks(void *data)
{
	struct caam_drv_private *ctrlpriv = data;

	clk_bulk_disable_unprepare(ctrlpriv->num_clks, ctrlpriv->clks);
}

static int init_clocks(struct device *dev, const struct caam_imx_data *data)
{
	struct caam_drv_private *ctrlpriv = dev_get_drvdata(dev);
	int ret;

	ctrlpriv->num_clks = data->num_clks;
	ctrlpriv->clks = devm_kmemdup(dev, data->clks,
				      data->num_clks * sizeof(data->clks[0]),
				      GFP_KERNEL);
	if (!ctrlpriv->clks)
		return -ENOMEM;

	ret = devm_clk_bulk_get(dev, ctrlpriv->num_clks, ctrlpriv->clks);
	if (ret) {
		dev_err(dev,
			"Failed to request all necessary clocks\n");
		return ret;
	}

	ret = clk_bulk_prepare_enable(ctrlpriv->num_clks, ctrlpriv->clks);
	if (ret) {
		dev_err(dev,
			"Failed to prepare/enable all necessary clocks\n");
		return ret;
	}

	return devm_add_action_or_reset(dev, disable_clocks, ctrlpriv);
}

static void caam_remove_debugfs(void *root)
{
	debugfs_remove_recursive(root);
}

#ifdef CONFIG_FSL_MC_BUS
static bool check_version(struct fsl_mc_version *mc_version, u32 major,
			  u32 minor, u32 revision)
{
	if (mc_version->major > major)
		return true;

	if (mc_version->major == major) {
		if (mc_version->minor > minor)
			return true;

		if (mc_version->minor == minor &&
		    mc_version->revision > revision)
			return true;
	}

	return false;
}
#endif

static bool needs_entropy_delay_adjustment(void)
{
	if (of_machine_is_compatible("fsl,imx6sx"))
		return true;
	return false;
}

static int caam_ctrl_rng_init(struct device *dev)
{
	struct caam_drv_private *ctrlpriv = dev_get_drvdata(dev);
	struct caam_ctrl __iomem *ctrl = ctrlpriv->ctrl;
	int ret, gen_sk, ent_delay = RTSDCTL_ENT_DLY_MIN;
	u8 rng_vid;

	if (ctrlpriv->era < 10) {
		struct caam_perfmon __iomem *perfmon;

		perfmon = ctrlpriv->total_jobrs ?
			  (struct caam_perfmon __iomem *)&ctrlpriv->jr[0]->perfmon :
			  (struct caam_perfmon __iomem *)&ctrl->perfmon;

		rng_vid = (rd_reg32(&perfmon->cha_id_ls) &
			   CHA_ID_LS_RNG_MASK) >> CHA_ID_LS_RNG_SHIFT;
	} else {
		struct version_regs __iomem *vreg;

		vreg = ctrlpriv->total_jobrs ?
			(struct version_regs __iomem *)&ctrlpriv->jr[0]->vreg :
			(struct version_regs __iomem *)&ctrl->vreg;

		rng_vid = (rd_reg32(&vreg->rng) & CHA_VER_VID_MASK) >>
			  CHA_VER_VID_SHIFT;
	}

	 
	if (!(ctrlpriv->mc_en && ctrlpriv->pr_support) && rng_vid >= 4) {
		ctrlpriv->rng4_sh_init =
			rd_reg32(&ctrl->r4tst[0].rdsta);
		 
		gen_sk = ctrlpriv->rng4_sh_init & RDSTA_SKVN ? 0 : 1;
		ctrlpriv->rng4_sh_init &= RDSTA_MASK;
		do {
			int inst_handles =
				rd_reg32(&ctrl->r4tst[0].rdsta) & RDSTA_MASK;
			 
			if (needs_entropy_delay_adjustment())
				ent_delay = 12000;
			if (!(ctrlpriv->rng4_sh_init || inst_handles)) {
				dev_info(dev,
					 "Entropy delay = %u\n",
					 ent_delay);
				kick_trng(dev, ent_delay);
				ent_delay += 400;
			}
			 
			ret = instantiate_rng(dev, inst_handles,
					      gen_sk);
			 
			if (needs_entropy_delay_adjustment())
				break;
			if (ret == -EAGAIN)
				 
				cpu_relax();
		} while ((ret == -EAGAIN) && (ent_delay < RTSDCTL_ENT_DLY_MAX));
		if (ret) {
			dev_err(dev, "failed to instantiate RNG");
			return ret;
		}
		 
		ctrlpriv->rng4_sh_init = ~ctrlpriv->rng4_sh_init & RDSTA_MASK;

		 
		clrsetbits_32(&ctrl->scfgr, 0, SCFGR_RDBENABLE);
	}

	return 0;
}

 
static int caam_off_during_pm(void)
{
	bool not_off_during_pm = of_machine_is_compatible("fsl,imx6q") ||
				 of_machine_is_compatible("fsl,imx6qp") ||
				 of_machine_is_compatible("fsl,imx6dl");

	return not_off_during_pm ? 0 : 1;
}

static void caam_state_save(struct device *dev)
{
	struct caam_drv_private *ctrlpriv = dev_get_drvdata(dev);
	struct caam_ctl_state *state = &ctrlpriv->state;
	struct caam_ctrl __iomem *ctrl = ctrlpriv->ctrl;
	u32 deco_inst, jr_inst;
	int i;

	state->mcr = rd_reg32(&ctrl->mcr);
	state->scfgr = rd_reg32(&ctrl->scfgr);

	deco_inst = (rd_reg32(&ctrl->perfmon.cha_num_ms) &
		     CHA_ID_MS_DECO_MASK) >> CHA_ID_MS_DECO_SHIFT;
	for (i = 0; i < deco_inst; i++) {
		state->deco_mid[i].liodn_ms =
			rd_reg32(&ctrl->deco_mid[i].liodn_ms);
		state->deco_mid[i].liodn_ls =
			rd_reg32(&ctrl->deco_mid[i].liodn_ls);
	}

	jr_inst = (rd_reg32(&ctrl->perfmon.cha_num_ms) &
		   CHA_ID_MS_JR_MASK) >> CHA_ID_MS_JR_SHIFT;
	for (i = 0; i < jr_inst; i++) {
		state->jr_mid[i].liodn_ms =
			rd_reg32(&ctrl->jr_mid[i].liodn_ms);
		state->jr_mid[i].liodn_ls =
			rd_reg32(&ctrl->jr_mid[i].liodn_ls);
	}
}

static void caam_state_restore(const struct device *dev)
{
	const struct caam_drv_private *ctrlpriv = dev_get_drvdata(dev);
	const struct caam_ctl_state *state = &ctrlpriv->state;
	struct caam_ctrl __iomem *ctrl = ctrlpriv->ctrl;
	u32 deco_inst, jr_inst;
	int i;

	wr_reg32(&ctrl->mcr, state->mcr);
	wr_reg32(&ctrl->scfgr, state->scfgr);

	deco_inst = (rd_reg32(&ctrl->perfmon.cha_num_ms) &
		     CHA_ID_MS_DECO_MASK) >> CHA_ID_MS_DECO_SHIFT;
	for (i = 0; i < deco_inst; i++) {
		wr_reg32(&ctrl->deco_mid[i].liodn_ms,
			 state->deco_mid[i].liodn_ms);
		wr_reg32(&ctrl->deco_mid[i].liodn_ls,
			 state->deco_mid[i].liodn_ls);
	}

	jr_inst = (rd_reg32(&ctrl->perfmon.cha_num_ms) &
		   CHA_ID_MS_JR_MASK) >> CHA_ID_MS_JR_SHIFT;
	for (i = 0; i < jr_inst; i++) {
		wr_reg32(&ctrl->jr_mid[i].liodn_ms,
			 state->jr_mid[i].liodn_ms);
		wr_reg32(&ctrl->jr_mid[i].liodn_ls,
			 state->jr_mid[i].liodn_ls);
	}

	if (ctrlpriv->virt_en == 1)
		clrsetbits_32(&ctrl->jrstart, 0, JRSTART_JR0_START |
			      JRSTART_JR1_START | JRSTART_JR2_START |
			      JRSTART_JR3_START);
}

static int caam_ctrl_suspend(struct device *dev)
{
	const struct caam_drv_private *ctrlpriv = dev_get_drvdata(dev);

	if (ctrlpriv->caam_off_during_pm && !ctrlpriv->optee_en)
		caam_state_save(dev);

	return 0;
}

static int caam_ctrl_resume(struct device *dev)
{
	struct caam_drv_private *ctrlpriv = dev_get_drvdata(dev);
	int ret = 0;

	if (ctrlpriv->caam_off_during_pm && !ctrlpriv->optee_en) {
		caam_state_restore(dev);

		 
		devm_remove_action(dev, devm_deinstantiate_rng, dev);
		ret = caam_ctrl_rng_init(dev);
	}

	return ret;
}

static DEFINE_SIMPLE_DEV_PM_OPS(caam_ctrl_pm_ops, caam_ctrl_suspend, caam_ctrl_resume);

 
static int caam_probe(struct platform_device *pdev)
{
	int ret, ring;
	u64 caam_id;
	const struct soc_device_attribute *imx_soc_match;
	struct device *dev;
	struct device_node *nprop, *np;
	struct caam_ctrl __iomem *ctrl;
	struct caam_drv_private *ctrlpriv;
	struct caam_perfmon __iomem *perfmon;
	struct dentry *dfs_root;
	u32 scfgr, comp_params;
	int pg_size;
	int BLOCK_OFFSET = 0;
	bool reg_access = true;

	ctrlpriv = devm_kzalloc(&pdev->dev, sizeof(*ctrlpriv), GFP_KERNEL);
	if (!ctrlpriv)
		return -ENOMEM;

	dev = &pdev->dev;
	dev_set_drvdata(dev, ctrlpriv);
	nprop = pdev->dev.of_node;

	imx_soc_match = soc_device_match(caam_imx_soc_table);
	if (!imx_soc_match && of_match_node(imx8m_machine_match, of_root))
		return -EPROBE_DEFER;

	caam_imx = (bool)imx_soc_match;

	ctrlpriv->caam_off_during_pm = caam_imx && caam_off_during_pm();

	if (imx_soc_match) {
		 
		np = of_find_compatible_node(NULL, NULL, "linaro,optee-tz");
		ctrlpriv->optee_en = !!np;
		of_node_put(np);

		reg_access = !ctrlpriv->optee_en;

		if (!imx_soc_match->data) {
			dev_err(dev, "No clock data provided for i.MX SoC");
			return -EINVAL;
		}

		ret = init_clocks(dev, imx_soc_match->data);
		if (ret)
			return ret;
	}


	 
	 
	ctrl = devm_of_iomap(dev, nprop, 0, NULL);
	ret = PTR_ERR_OR_ZERO(ctrl);
	if (ret) {
		dev_err(dev, "caam: of_iomap() failed\n");
		return ret;
	}

	ring = 0;
	for_each_available_child_of_node(nprop, np)
		if (of_device_is_compatible(np, "fsl,sec-v4.0-job-ring") ||
		    of_device_is_compatible(np, "fsl,sec4.0-job-ring")) {
			u32 reg;

			if (of_property_read_u32_index(np, "reg", 0, &reg)) {
				dev_err(dev, "%s read reg property error\n",
					np->full_name);
				continue;
			}

			ctrlpriv->jr[ring] = (struct caam_job_ring __iomem __force *)
					     ((__force uint8_t *)ctrl + reg);

			ctrlpriv->total_jobrs++;
			ring++;
		}

	 
	perfmon = ring ? (struct caam_perfmon __iomem *)&ctrlpriv->jr[0]->perfmon :
			 (struct caam_perfmon __iomem *)&ctrl->perfmon;

	caam_little_end = !(bool)(rd_reg32(&perfmon->status) &
				  (CSTA_PLEND | CSTA_ALT_PLEND));
	comp_params = rd_reg32(&perfmon->comp_parms_ms);
	if (reg_access && comp_params & CTPR_MS_PS &&
	    rd_reg32(&ctrl->mcr) & MCFGR_LONG_PTR)
		caam_ptr_sz = sizeof(u64);
	else
		caam_ptr_sz = sizeof(u32);
	caam_dpaa2 = !!(comp_params & CTPR_MS_DPAA2);
	ctrlpriv->qi_present = !!(comp_params & CTPR_MS_QI_MASK);

#ifdef CONFIG_CAAM_QI
	 
	if (ctrlpriv->qi_present && !caam_dpaa2) {
		ret = qman_is_probed();
		if (!ret) {
			return -EPROBE_DEFER;
		} else if (ret < 0) {
			dev_err(dev, "failing probe due to qman probe error\n");
			return -ENODEV;
		}

		ret = qman_portals_probed();
		if (!ret) {
			return -EPROBE_DEFER;
		} else if (ret < 0) {
			dev_err(dev, "failing probe due to qman portals probe error\n");
			return -ENODEV;
		}
	}
#endif

	 
	pg_size = (comp_params & CTPR_MS_PG_SZ_MASK) >> CTPR_MS_PG_SZ_SHIFT;
	if (pg_size == 0)
		BLOCK_OFFSET = PG_SIZE_4K;
	else
		BLOCK_OFFSET = PG_SIZE_64K;

	ctrlpriv->ctrl = (struct caam_ctrl __iomem __force *)ctrl;
	ctrlpriv->assure = (struct caam_assurance __iomem __force *)
			   ((__force uint8_t *)ctrl +
			    BLOCK_OFFSET * ASSURE_BLOCK_NUMBER
			   );
	ctrlpriv->deco = (struct caam_deco __iomem __force *)
			 ((__force uint8_t *)ctrl +
			 BLOCK_OFFSET * DECO_BLOCK_NUMBER
			 );

	 
	ctrlpriv->secvio_irq = irq_of_parse_and_map(nprop, 0);
	np = of_find_compatible_node(NULL, NULL, "fsl,qoriq-mc");
	ctrlpriv->mc_en = !!np;
	of_node_put(np);

#ifdef CONFIG_FSL_MC_BUS
	if (ctrlpriv->mc_en) {
		struct fsl_mc_version *mc_version;

		mc_version = fsl_mc_get_version();
		if (mc_version)
			ctrlpriv->pr_support = check_version(mc_version, 10, 20,
							     0);
		else
			return -EPROBE_DEFER;
	}
#endif

	if (!reg_access)
		goto set_dma_mask;

	 
	if (!ctrlpriv->mc_en)
		clrsetbits_32(&ctrl->mcr, MCFGR_AWCACHE_MASK,
			      MCFGR_AWCACHE_CACH | MCFGR_AWCACHE_BUFF |
			      MCFGR_WDENABLE | MCFGR_LARGE_BURST);

	handle_imx6_err005766(&ctrl->mcr);

	 
	scfgr = rd_reg32(&ctrl->scfgr);

	ctrlpriv->virt_en = 0;
	if (comp_params & CTPR_MS_VIRT_EN_INCL) {
		 
		if ((comp_params & CTPR_MS_VIRT_EN_POR) ||
		    (!(comp_params & CTPR_MS_VIRT_EN_POR) &&
		       (scfgr & SCFGR_VIRT_EN)))
				ctrlpriv->virt_en = 1;
	} else {
		 
		if (comp_params & CTPR_MS_VIRT_EN_POR)
				ctrlpriv->virt_en = 1;
	}

	if (ctrlpriv->virt_en == 1)
		clrsetbits_32(&ctrl->jrstart, 0, JRSTART_JR0_START |
			      JRSTART_JR1_START | JRSTART_JR2_START |
			      JRSTART_JR3_START);

set_dma_mask:
	ret = dma_set_mask_and_coherent(dev, caam_get_dma_mask(dev));
	if (ret) {
		dev_err(dev, "dma_set_mask_and_coherent failed (%d)\n", ret);
		return ret;
	}

	ctrlpriv->era = caam_get_era(perfmon);
	ctrlpriv->domain = iommu_get_domain_for_dev(dev);

	dfs_root = debugfs_create_dir(dev_name(dev), NULL);
	if (IS_ENABLED(CONFIG_DEBUG_FS)) {
		ret = devm_add_action_or_reset(dev, caam_remove_debugfs,
					       dfs_root);
		if (ret)
			return ret;
	}

	caam_debugfs_init(ctrlpriv, perfmon, dfs_root);

	 
	if (ctrlpriv->qi_present && !caam_dpaa2) {
		ctrlpriv->qi = (struct caam_queue_if __iomem __force *)
			       ((__force uint8_t *)ctrl +
				 BLOCK_OFFSET * QI_BLOCK_NUMBER
			       );
		 
		wr_reg32(&ctrlpriv->qi->qi_control_lo, QICTL_DQEN);

		 
#ifdef CONFIG_CAAM_QI
		ret = caam_qi_init(pdev);
		if (ret)
			dev_err(dev, "caam qi i/f init failed: %d\n", ret);
#endif
	}

	 
	if ((!ctrlpriv->qi_present) && (!ctrlpriv->total_jobrs)) {
		dev_err(dev, "no queues configured, terminating\n");
		return -ENOMEM;
	}

	comp_params = rd_reg32(&perfmon->comp_parms_ls);
	ctrlpriv->blob_present = !!(comp_params & CTPR_LS_BLOB);

	 
	if (ctrlpriv->era < 10) {
		ctrlpriv->blob_present = ctrlpriv->blob_present &&
			(rd_reg32(&perfmon->cha_num_ls) & CHA_ID_LS_AES_MASK);
	} else {
		struct version_regs __iomem *vreg;

		vreg =  ctrlpriv->total_jobrs ?
			(struct version_regs __iomem *)&ctrlpriv->jr[0]->vreg :
			(struct version_regs __iomem *)&ctrl->vreg;

		ctrlpriv->blob_present = ctrlpriv->blob_present &&
			(rd_reg32(&vreg->aesa) & CHA_VER_MISC_AES_NUM_MASK);
	}

	if (reg_access) {
		ret = caam_ctrl_rng_init(dev);
		if (ret)
			return ret;
	}

	caam_id = (u64)rd_reg32(&perfmon->caam_id_ms) << 32 |
		  (u64)rd_reg32(&perfmon->caam_id_ls);

	 
	dev_info(dev, "device ID = 0x%016llx (Era %d)\n", caam_id,
		 ctrlpriv->era);
	dev_info(dev, "job rings = %d, qi = %d\n",
		 ctrlpriv->total_jobrs, ctrlpriv->qi_present);

	ret = devm_of_platform_populate(dev);
	if (ret)
		dev_err(dev, "JR platform devices creation error\n");

	return ret;
}

static struct platform_driver caam_driver = {
	.driver = {
		.name = "caam",
		.of_match_table = caam_match,
		.pm = pm_ptr(&caam_ctrl_pm_ops),
	},
	.probe       = caam_probe,
};

module_platform_driver(caam_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FSL CAAM request backend");
MODULE_AUTHOR("Freescale Semiconductor - NMG/STC");
