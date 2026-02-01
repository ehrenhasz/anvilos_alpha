
 

#include <linux/bitfield.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/types.h>

#define RC_UPDATE_EN		BIT(0)
#define RC_ROOT_EN		BIT(1)

#define RC_REG_CFG_UPDATE	0x60
#define RC_CFG_UPDATE_EN	BIT(8)
#define RC_CFG_ACK		GENMASK(31, 16)

#define RC_DCVS_CFG_SID		2
#define RC_LINK_SID		3
#define RC_LMH_SID		6
#define RC_DFS_SID		14

#define RC_UPDATE_TIMEOUT_US	500

 
struct qcom_ramp_controller_desc {
	const struct reg_sequence *cfg_dfs_sid;
	const struct reg_sequence *cfg_link_sid;
	const struct reg_sequence *cfg_lmh_sid;
	const struct reg_sequence *cfg_ramp_en;
	const struct reg_sequence *cfg_ramp_dis;
	u8 cmd_reg;
	u8 num_dfs_sids;
	u8 num_link_sids;
	u8 num_lmh_sids;
	u8 num_ramp_en;
	u8 num_ramp_dis;
};

 
struct qcom_ramp_controller {
	struct regmap *regmap;
	const struct qcom_ramp_controller_desc *desc;
};

 
static int rc_wait_for_update(struct qcom_ramp_controller *qrc)
{
	const struct qcom_ramp_controller_desc *d = qrc->desc;
	struct regmap *r = qrc->regmap;
	u32 val;
	int ret;

	ret = regmap_set_bits(r, d->cmd_reg, RC_ROOT_EN);
	if (ret)
		return ret;

	return regmap_read_poll_timeout(r, d->cmd_reg, val, !(val & RC_UPDATE_EN),
					1, RC_UPDATE_TIMEOUT_US);
}

 
static int rc_set_cfg_update(struct qcom_ramp_controller *qrc, u8 ce)
{
	const struct qcom_ramp_controller_desc *d = qrc->desc;
	struct regmap *r = qrc->regmap;
	u32 ack, val;
	int ret;

	 
	ack = FIELD_PREP(RC_CFG_ACK, BIT(ce));

	 
	ret = regmap_set_bits(r, d->cmd_reg + RC_REG_CFG_UPDATE, ce);
	if (ret)
		return ret;

	 
	ret = regmap_set_bits(r, d->cmd_reg + RC_REG_CFG_UPDATE, RC_CFG_UPDATE_EN);
	if (ret)
		return ret;

	 
	ret = regmap_read_poll_timeout(r, d->cmd_reg + RC_REG_CFG_UPDATE, val,
				       val & ack, 1, RC_UPDATE_TIMEOUT_US);
	if (ret)
		return ret;

	 
	ret = regmap_write(r, d->cmd_reg + RC_REG_CFG_UPDATE, 0);
	if (ret)
		return ret;

	 
	return regmap_read_poll_timeout(r, d->cmd_reg + RC_REG_CFG_UPDATE,
					val, !(val & RC_CFG_ACK), 1,
					RC_UPDATE_TIMEOUT_US);
}

 
static int rc_write_cfg(struct qcom_ramp_controller *qrc,
			const struct reg_sequence *seq,
			u16 ce, u8 nsids)
{
	int ret;
	u8 i;

	 
	ret = rc_wait_for_update(qrc);
	if (ret)
		return ret;

	 
	ret = regmap_multi_reg_write(qrc->regmap, seq, nsids);
	if (ret)
		return ret;

	 
	for (i = 0; i < nsids; i++) {
		ret = rc_set_cfg_update(qrc, (u8)ce - i);
		if (ret)
			return ret;
	}

	return 0;
}

 
static int rc_ramp_ctrl_enable(struct qcom_ramp_controller *qrc)
{
	const struct qcom_ramp_controller_desc *d = qrc->desc;
	int i, ret;

	for (i = 0; i < d->num_ramp_en; i++) {
		ret = rc_write_cfg(qrc, &d->cfg_ramp_en[i], RC_DCVS_CFG_SID, 1);
		if (ret)
			return ret;
	}

	return 0;
}

 
static int qcom_ramp_controller_start(struct qcom_ramp_controller *qrc)
{
	const struct qcom_ramp_controller_desc *d = qrc->desc;
	int ret;

	 
	ret = rc_write_cfg(qrc, d->cfg_lmh_sid, RC_LMH_SID, d->num_lmh_sids);
	if (ret)
		return ret;

	ret = rc_write_cfg(qrc, d->cfg_dfs_sid, RC_DFS_SID, d->num_dfs_sids);
	if (ret)
		return ret;

	ret = rc_write_cfg(qrc, d->cfg_link_sid, RC_LINK_SID, d->num_link_sids);
	if (ret)
		return ret;

	 
	return rc_ramp_ctrl_enable(qrc);
}

static const struct regmap_config qrc_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register =	0x68,
	.fast_io = true,
};

static const struct reg_sequence msm8976_cfg_dfs_sid[] = {
	{ 0x10, 0xfefebff7 },
	{ 0x14, 0xfdff7fef },
	{ 0x18, 0xfbffdefb },
	{ 0x1c, 0xb69b5555 },
	{ 0x20, 0x24929249 },
	{ 0x24, 0x49241112 },
	{ 0x28, 0x11112111 },
	{ 0x2c, 0x8102 }
};

static const struct reg_sequence msm8976_cfg_link_sid[] = {
	{ 0x40, 0xfc987 }
};

static const struct reg_sequence msm8976_cfg_lmh_sid[] = {
	{ 0x30, 0x77706db },
	{ 0x34, 0x5550249 },
	{ 0x38, 0x111 }
};

static const struct reg_sequence msm8976_cfg_ramp_en[] = {
	{ 0x50, 0x800 },  
	{ 0x50, 0xc00 },  
	{ 0x50, 0x400 }   
};

static const struct reg_sequence msm8976_cfg_ramp_dis[] = {
	{ 0x50, 0x0 }
};

static const struct qcom_ramp_controller_desc msm8976_rc_cfg = {
	.cfg_dfs_sid = msm8976_cfg_dfs_sid,
	.num_dfs_sids = ARRAY_SIZE(msm8976_cfg_dfs_sid),

	.cfg_link_sid = msm8976_cfg_link_sid,
	.num_link_sids = ARRAY_SIZE(msm8976_cfg_link_sid),

	.cfg_lmh_sid = msm8976_cfg_lmh_sid,
	.num_lmh_sids = ARRAY_SIZE(msm8976_cfg_lmh_sid),

	.cfg_ramp_en = msm8976_cfg_ramp_en,
	.num_ramp_en = ARRAY_SIZE(msm8976_cfg_ramp_en),

	.cfg_ramp_dis = msm8976_cfg_ramp_dis,
	.num_ramp_dis = ARRAY_SIZE(msm8976_cfg_ramp_dis),

	.cmd_reg = 0x0,
};

static int qcom_ramp_controller_probe(struct platform_device *pdev)
{
	struct qcom_ramp_controller *qrc;
	void __iomem *base;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	qrc = devm_kmalloc(&pdev->dev, sizeof(*qrc), GFP_KERNEL);
	if (!qrc)
		return -ENOMEM;

	qrc->desc = device_get_match_data(&pdev->dev);
	if (!qrc->desc)
		return -EINVAL;

	qrc->regmap = devm_regmap_init_mmio(&pdev->dev, base, &qrc_regmap_config);
	if (IS_ERR(qrc->regmap))
		return PTR_ERR(qrc->regmap);

	platform_set_drvdata(pdev, qrc);

	return qcom_ramp_controller_start(qrc);
}

static void qcom_ramp_controller_remove(struct platform_device *pdev)
{
	struct qcom_ramp_controller *qrc = platform_get_drvdata(pdev);
	int ret;

	ret = rc_write_cfg(qrc, qrc->desc->cfg_ramp_dis,
			   RC_DCVS_CFG_SID, qrc->desc->num_ramp_dis);
	if (ret)
		dev_err(&pdev->dev, "Failed to send disable sequence\n");
}

static const struct of_device_id qcom_ramp_controller_match_table[] = {
	{ .compatible = "qcom,msm8976-ramp-controller", .data = &msm8976_rc_cfg },
	{   }
};
MODULE_DEVICE_TABLE(of, qcom_ramp_controller_match_table);

static struct platform_driver qcom_ramp_controller_driver = {
	.driver = {
		.name = "qcom-ramp-controller",
		.of_match_table = qcom_ramp_controller_match_table,
		.suppress_bind_attrs = true,
	},
	.probe  = qcom_ramp_controller_probe,
	.remove_new = qcom_ramp_controller_remove,
};

static int __init qcom_ramp_controller_init(void)
{
	return platform_driver_register(&qcom_ramp_controller_driver);
}
arch_initcall(qcom_ramp_controller_init);

MODULE_AUTHOR("AngeloGioacchino Del Regno <angelogioacchino.delregno@collabora.com>");
MODULE_DESCRIPTION("Qualcomm Ramp Controller driver");
MODULE_LICENSE("GPL");
