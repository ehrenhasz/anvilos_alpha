
 

#include <linux/export.h>
#include <linux/jiffies.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/soc/mediatek/infracfg.h>
#include <asm/processor.h>

#define MTK_POLL_DELAY_US   10
#define MTK_POLL_TIMEOUT    (jiffies_to_usecs(HZ))

 
int mtk_infracfg_set_bus_protection(struct regmap *infracfg, u32 mask,
		bool reg_update)
{
	u32 val;
	int ret;

	if (reg_update)
		regmap_update_bits(infracfg, INFRA_TOPAXI_PROTECTEN, mask,
				mask);
	else
		regmap_write(infracfg, INFRA_TOPAXI_PROTECTEN_SET, mask);

	ret = regmap_read_poll_timeout(infracfg, INFRA_TOPAXI_PROTECTSTA1,
				       val, (val & mask) == mask,
				       MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);

	return ret;
}

 

int mtk_infracfg_clear_bus_protection(struct regmap *infracfg, u32 mask,
		bool reg_update)
{
	int ret;
	u32 val;

	if (reg_update)
		regmap_update_bits(infracfg, INFRA_TOPAXI_PROTECTEN, mask, 0);
	else
		regmap_write(infracfg, INFRA_TOPAXI_PROTECTEN_CLR, mask);

	ret = regmap_read_poll_timeout(infracfg, INFRA_TOPAXI_PROTECTSTA1,
				       val, !(val & mask),
				       MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);

	return ret;
}

static int __init mtk_infracfg_init(void)
{
	struct regmap *infracfg;

	 
	infracfg = syscon_regmap_lookup_by_compatible("mediatek,mt8192-infracfg");
	if (!IS_ERR(infracfg))
		regmap_set_bits(infracfg, MT8192_INFRA_CTRL,
				MT8192_INFRA_CTRL_DISABLE_MFG2ACP);
	return 0;
}
postcore_initcall(mtk_infracfg_init);
