
 

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/time.h>

#include "ufshcd-pltfrm.h"

#define CDNS_UFS_REG_HCLKDIV	0xFC
#define CDNS_UFS_REG_PHY_XCFGD1	0x113C
#define CDNS_UFS_MAX_L4_ATTRS 12

struct cdns_ufs_host {
	 
	u32 cdns_ufs_dme_attr_val[CDNS_UFS_MAX_L4_ATTRS];
};

 
static void cdns_ufs_get_l4_attr(struct ufs_hba *hba)
{
	struct cdns_ufs_host *host = ufshcd_get_variant(hba);

	ufshcd_dme_get(hba, UIC_ARG_MIB(T_PEERDEVICEID),
		       &host->cdns_ufs_dme_attr_val[0]);
	ufshcd_dme_get(hba, UIC_ARG_MIB(T_PEERCPORTID),
		       &host->cdns_ufs_dme_attr_val[1]);
	ufshcd_dme_get(hba, UIC_ARG_MIB(T_TRAFFICCLASS),
		       &host->cdns_ufs_dme_attr_val[2]);
	ufshcd_dme_get(hba, UIC_ARG_MIB(T_PROTOCOLID),
		       &host->cdns_ufs_dme_attr_val[3]);
	ufshcd_dme_get(hba, UIC_ARG_MIB(T_CPORTFLAGS),
		       &host->cdns_ufs_dme_attr_val[4]);
	ufshcd_dme_get(hba, UIC_ARG_MIB(T_TXTOKENVALUE),
		       &host->cdns_ufs_dme_attr_val[5]);
	ufshcd_dme_get(hba, UIC_ARG_MIB(T_RXTOKENVALUE),
		       &host->cdns_ufs_dme_attr_val[6]);
	ufshcd_dme_get(hba, UIC_ARG_MIB(T_LOCALBUFFERSPACE),
		       &host->cdns_ufs_dme_attr_val[7]);
	ufshcd_dme_get(hba, UIC_ARG_MIB(T_PEERBUFFERSPACE),
		       &host->cdns_ufs_dme_attr_val[8]);
	ufshcd_dme_get(hba, UIC_ARG_MIB(T_CREDITSTOSEND),
		       &host->cdns_ufs_dme_attr_val[9]);
	ufshcd_dme_get(hba, UIC_ARG_MIB(T_CPORTMODE),
		       &host->cdns_ufs_dme_attr_val[10]);
	ufshcd_dme_get(hba, UIC_ARG_MIB(T_CONNECTIONSTATE),
		       &host->cdns_ufs_dme_attr_val[11]);
}

 
static void cdns_ufs_set_l4_attr(struct ufs_hba *hba)
{
	struct cdns_ufs_host *host = ufshcd_get_variant(hba);

	ufshcd_dme_set(hba, UIC_ARG_MIB(T_CONNECTIONSTATE), 0);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_PEERDEVICEID),
		       host->cdns_ufs_dme_attr_val[0]);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_PEERCPORTID),
		       host->cdns_ufs_dme_attr_val[1]);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_TRAFFICCLASS),
		       host->cdns_ufs_dme_attr_val[2]);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_PROTOCOLID),
		       host->cdns_ufs_dme_attr_val[3]);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_CPORTFLAGS),
		       host->cdns_ufs_dme_attr_val[4]);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_TXTOKENVALUE),
		       host->cdns_ufs_dme_attr_val[5]);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_RXTOKENVALUE),
		       host->cdns_ufs_dme_attr_val[6]);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_LOCALBUFFERSPACE),
		       host->cdns_ufs_dme_attr_val[7]);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_PEERBUFFERSPACE),
		       host->cdns_ufs_dme_attr_val[8]);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_CREDITSTOSEND),
		       host->cdns_ufs_dme_attr_val[9]);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_CPORTMODE),
		       host->cdns_ufs_dme_attr_val[10]);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_CONNECTIONSTATE),
		       host->cdns_ufs_dme_attr_val[11]);
}

 
static int cdns_ufs_set_hclkdiv(struct ufs_hba *hba)
{
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;
	unsigned long core_clk_rate = 0;
	u32 core_clk_div = 0;

	if (list_empty(head))
		return 0;

	list_for_each_entry(clki, head, list) {
		if (IS_ERR_OR_NULL(clki->clk))
			continue;
		if (!strcmp(clki->name, "core_clk"))
			core_clk_rate = clk_get_rate(clki->clk);
	}

	if (!core_clk_rate) {
		dev_err(hba->dev, "%s: unable to find core_clk rate\n",
			__func__);
		return -EINVAL;
	}

	core_clk_div = core_clk_rate / USEC_PER_SEC;

	ufshcd_writel(hba, core_clk_div, CDNS_UFS_REG_HCLKDIV);
	 
	mb();

	return 0;
}

 
static int cdns_ufs_hce_enable_notify(struct ufs_hba *hba,
				      enum ufs_notify_change_status status)
{
	if (status != PRE_CHANGE)
		return 0;

	return cdns_ufs_set_hclkdiv(hba);
}

 
static void cdns_ufs_hibern8_notify(struct ufs_hba *hba, enum uic_cmd_dme cmd,
				    enum ufs_notify_change_status status)
{
	if (status == PRE_CHANGE && cmd == UIC_CMD_DME_HIBER_ENTER)
		cdns_ufs_get_l4_attr(hba);
	if (status == POST_CHANGE && cmd == UIC_CMD_DME_HIBER_EXIT)
		cdns_ufs_set_l4_attr(hba);
}

 
static int cdns_ufs_link_startup_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status status)
{
	if (status != PRE_CHANGE)
		return 0;

	 
	ufshcd_disable_host_tx_lcc(hba);

	 
	hba->ahit = 0;

	return 0;
}

 
static int cdns_ufs_init(struct ufs_hba *hba)
{
	int status = 0;
	struct cdns_ufs_host *host;
	struct device *dev = hba->dev;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);

	if (!host)
		return -ENOMEM;
	ufshcd_set_variant(hba, host);

	status = ufshcd_vops_phy_initialization(hba);

	return status;
}

 
static int cdns_ufs_m31_16nm_phy_initialization(struct ufs_hba *hba)
{
	u32 data;

	 
	data = ufshcd_readl(hba, CDNS_UFS_REG_PHY_XCFGD1);
	data |= BIT(24);
	ufshcd_writel(hba, data, CDNS_UFS_REG_PHY_XCFGD1);

	return 0;
}

static const struct ufs_hba_variant_ops cdns_ufs_pltfm_hba_vops = {
	.name = "cdns-ufs-pltfm",
	.init = cdns_ufs_init,
	.hce_enable_notify = cdns_ufs_hce_enable_notify,
	.link_startup_notify = cdns_ufs_link_startup_notify,
	.hibern8_notify = cdns_ufs_hibern8_notify,
};

static const struct ufs_hba_variant_ops cdns_ufs_m31_16nm_pltfm_hba_vops = {
	.name = "cdns-ufs-pltfm",
	.init = cdns_ufs_init,
	.hce_enable_notify = cdns_ufs_hce_enable_notify,
	.link_startup_notify = cdns_ufs_link_startup_notify,
	.phy_initialization = cdns_ufs_m31_16nm_phy_initialization,
	.hibern8_notify = cdns_ufs_hibern8_notify,
};

static const struct of_device_id cdns_ufs_of_match[] = {
	{
		.compatible = "cdns,ufshc",
		.data =  &cdns_ufs_pltfm_hba_vops,
	},
	{
		.compatible = "cdns,ufshc-m31-16nm",
		.data =  &cdns_ufs_m31_16nm_pltfm_hba_vops,
	},
	{ },
};

MODULE_DEVICE_TABLE(of, cdns_ufs_of_match);

 
static int cdns_ufs_pltfrm_probe(struct platform_device *pdev)
{
	int err;
	const struct of_device_id *of_id;
	struct ufs_hba_variant_ops *vops;
	struct device *dev = &pdev->dev;

	of_id = of_match_node(cdns_ufs_of_match, dev->of_node);
	vops = (struct ufs_hba_variant_ops *)of_id->data;

	 
	err = ufshcd_pltfrm_init(pdev, vops);
	if (err)
		dev_err(dev, "ufshcd_pltfrm_init() failed %d\n", err);

	return err;
}

 
static int cdns_ufs_pltfrm_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	ufshcd_remove(hba);
	return 0;
}

static const struct dev_pm_ops cdns_ufs_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ufshcd_system_suspend, ufshcd_system_resume)
	SET_RUNTIME_PM_OPS(ufshcd_runtime_suspend, ufshcd_runtime_resume, NULL)
	.prepare	 = ufshcd_suspend_prepare,
	.complete	 = ufshcd_resume_complete,
};

static struct platform_driver cdns_ufs_pltfrm_driver = {
	.probe	= cdns_ufs_pltfrm_probe,
	.remove	= cdns_ufs_pltfrm_remove,
	.driver	= {
		.name   = "cdns-ufshcd",
		.pm     = &cdns_ufs_dev_pm_ops,
		.of_match_table = cdns_ufs_of_match,
	},
};

module_platform_driver(cdns_ufs_pltfrm_driver);

MODULE_AUTHOR("Jan Kotas <jank@cadence.com>");
MODULE_DESCRIPTION("Cadence UFS host controller platform driver");
MODULE_LICENSE("GPL v2");
