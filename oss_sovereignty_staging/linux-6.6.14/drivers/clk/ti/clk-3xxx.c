
 

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clk/ti.h>

#include "clock.h"

#define OMAP3430ES2_ST_DSS_IDLE_SHIFT			1
#define OMAP3430ES2_ST_HSOTGUSB_IDLE_SHIFT		5
#define OMAP3430ES2_ST_SSI_IDLE_SHIFT			8

#define OMAP34XX_CM_IDLEST_VAL				1

 
#define AM35XX_IPSS_ICK_MASK			0xF
#define AM35XX_IPSS_ICK_EN_ACK_OFFSET		0x4
#define AM35XX_IPSS_ICK_FCK_OFFSET		0x8
#define AM35XX_IPSS_CLK_IDLEST_VAL		0

#define AM35XX_ST_IPSS_SHIFT			5

 
static void omap3430es2_clk_ssi_find_idlest(struct clk_hw_omap *clk,
					    struct clk_omap_reg *idlest_reg,
					    u8 *idlest_bit,
					    u8 *idlest_val)
{
	memcpy(idlest_reg, &clk->enable_reg, sizeof(*idlest_reg));
	idlest_reg->offset &= ~0xf0;
	idlest_reg->offset |= 0x20;
	*idlest_bit = OMAP3430ES2_ST_SSI_IDLE_SHIFT;
	*idlest_val = OMAP34XX_CM_IDLEST_VAL;
}

const struct clk_hw_omap_ops clkhwops_omap3430es2_iclk_ssi_wait = {
	.allow_idle	= omap2_clkt_iclk_allow_idle,
	.deny_idle	= omap2_clkt_iclk_deny_idle,
	.find_idlest	= omap3430es2_clk_ssi_find_idlest,
	.find_companion	= omap2_clk_dflt_find_companion,
};

 
static void
omap3430es2_clk_dss_usbhost_find_idlest(struct clk_hw_omap *clk,
					struct clk_omap_reg *idlest_reg,
					u8 *idlest_bit, u8 *idlest_val)
{
	memcpy(idlest_reg, &clk->enable_reg, sizeof(*idlest_reg));

	idlest_reg->offset &= ~0xf0;
	idlest_reg->offset |= 0x20;
	 
	*idlest_bit = OMAP3430ES2_ST_DSS_IDLE_SHIFT;
	*idlest_val = OMAP34XX_CM_IDLEST_VAL;
}

const struct clk_hw_omap_ops clkhwops_omap3430es2_dss_usbhost_wait = {
	.find_idlest	= omap3430es2_clk_dss_usbhost_find_idlest,
	.find_companion	= omap2_clk_dflt_find_companion,
};

const struct clk_hw_omap_ops clkhwops_omap3430es2_iclk_dss_usbhost_wait = {
	.allow_idle	= omap2_clkt_iclk_allow_idle,
	.deny_idle	= omap2_clkt_iclk_deny_idle,
	.find_idlest	= omap3430es2_clk_dss_usbhost_find_idlest,
	.find_companion	= omap2_clk_dflt_find_companion,
};

 
static void
omap3430es2_clk_hsotgusb_find_idlest(struct clk_hw_omap *clk,
				     struct clk_omap_reg *idlest_reg,
				     u8 *idlest_bit,
				     u8 *idlest_val)
{
	memcpy(idlest_reg, &clk->enable_reg, sizeof(*idlest_reg));
	idlest_reg->offset &= ~0xf0;
	idlest_reg->offset |= 0x20;
	*idlest_bit = OMAP3430ES2_ST_HSOTGUSB_IDLE_SHIFT;
	*idlest_val = OMAP34XX_CM_IDLEST_VAL;
}

const struct clk_hw_omap_ops clkhwops_omap3430es2_iclk_hsotgusb_wait = {
	.allow_idle	= omap2_clkt_iclk_allow_idle,
	.deny_idle	= omap2_clkt_iclk_deny_idle,
	.find_idlest	= omap3430es2_clk_hsotgusb_find_idlest,
	.find_companion	= omap2_clk_dflt_find_companion,
};

 
static void am35xx_clk_find_idlest(struct clk_hw_omap *clk,
				   struct clk_omap_reg *idlest_reg,
				   u8 *idlest_bit,
				   u8 *idlest_val)
{
	memcpy(idlest_reg, &clk->enable_reg, sizeof(*idlest_reg));
	*idlest_bit = clk->enable_bit + AM35XX_IPSS_ICK_EN_ACK_OFFSET;
	*idlest_val = AM35XX_IPSS_CLK_IDLEST_VAL;
}

 
static void am35xx_clk_find_companion(struct clk_hw_omap *clk,
				      struct clk_omap_reg *other_reg,
				      u8 *other_bit)
{
	memcpy(other_reg, &clk->enable_reg, sizeof(*other_reg));
	if (clk->enable_bit & AM35XX_IPSS_ICK_MASK)
		*other_bit = clk->enable_bit + AM35XX_IPSS_ICK_FCK_OFFSET;
	else
	*other_bit = clk->enable_bit - AM35XX_IPSS_ICK_FCK_OFFSET;
}

const struct clk_hw_omap_ops clkhwops_am35xx_ipss_module_wait = {
	.find_idlest	= am35xx_clk_find_idlest,
	.find_companion	= am35xx_clk_find_companion,
};

 
static void am35xx_clk_ipss_find_idlest(struct clk_hw_omap *clk,
					struct clk_omap_reg *idlest_reg,
					u8 *idlest_bit,
					u8 *idlest_val)
{
	memcpy(idlest_reg, &clk->enable_reg, sizeof(*idlest_reg));

	idlest_reg->offset &= ~0xf0;
	idlest_reg->offset |= 0x20;
	*idlest_bit = AM35XX_ST_IPSS_SHIFT;
	*idlest_val = OMAP34XX_CM_IDLEST_VAL;
}

const struct clk_hw_omap_ops clkhwops_am35xx_ipss_wait = {
	.allow_idle	= omap2_clkt_iclk_allow_idle,
	.deny_idle	= omap2_clkt_iclk_deny_idle,
	.find_idlest	= am35xx_clk_ipss_find_idlest,
	.find_companion	= omap2_clk_dflt_find_companion,
};

static struct ti_dt_clk omap3xxx_clks[] = {
	DT_CLK(NULL, "timer_32k_ck", "omap_32k_fck"),
	DT_CLK(NULL, "timer_sys_ck", "sys_ck"),
	{ .node_name = NULL },
};

static struct ti_dt_clk omap36xx_omap3430es2plus_clks[] = {
	DT_CLK(NULL, "ssi_ssr_fck", "ssi_ssr_fck_3430es2"),
	DT_CLK(NULL, "ssi_sst_fck", "ssi_sst_fck_3430es2"),
	DT_CLK(NULL, "hsotgusb_ick", "hsotgusb_ick_3430es2"),
	DT_CLK(NULL, "ssi_ick", "ssi_ick_3430es2"),
	{ .node_name = NULL },
};

static struct ti_dt_clk omap3430es1_clks[] = {
	DT_CLK(NULL, "ssi_ssr_fck", "ssi_ssr_fck_3430es1"),
	DT_CLK(NULL, "ssi_sst_fck", "ssi_sst_fck_3430es1"),
	DT_CLK(NULL, "hsotgusb_ick", "hsotgusb_ick_3430es1"),
	DT_CLK(NULL, "ssi_ick", "ssi_ick_3430es1"),
	DT_CLK(NULL, "dss1_alwon_fck", "dss1_alwon_fck_3430es1"),
	DT_CLK(NULL, "dss_ick", "dss_ick_3430es1"),
	{ .node_name = NULL },
};

static struct ti_dt_clk omap36xx_am35xx_omap3430es2plus_clks[] = {
	DT_CLK(NULL, "dss1_alwon_fck", "dss1_alwon_fck_3430es2"),
	DT_CLK(NULL, "dss_ick", "dss_ick_3430es2"),
	{ .node_name = NULL },
};

static struct ti_dt_clk am35xx_clks[] = {
	DT_CLK(NULL, "hsotgusb_ick", "hsotgusb_ick_am35xx"),
	DT_CLK(NULL, "hsotgusb_fck", "hsotgusb_fck_am35xx"),
	DT_CLK(NULL, "uart4_ick", "uart4_ick_am35xx"),
	DT_CLK(NULL, "uart4_fck", "uart4_fck_am35xx"),
	{ .node_name = NULL },
};

static const char *enable_init_clks[] = {
	"sdrc_ick",
	"gpmc_fck",
	"omapctrl_ick",
};

enum {
	OMAP3_SOC_AM35XX,
	OMAP3_SOC_OMAP3430_ES1,
	OMAP3_SOC_OMAP3430_ES2_PLUS,
	OMAP3_SOC_OMAP3630,
};

 
void __init omap3_clk_lock_dpll5(void)
{
	struct clk *dpll5_clk;
	struct clk *dpll5_m2_clk;

	 
	dpll5_clk = clk_get(NULL, "dpll5_ck");
	clk_set_rate(dpll5_clk, OMAP3_DPLL5_FREQ_FOR_USBHOST * 8);
	clk_prepare_enable(dpll5_clk);

	 
	dpll5_m2_clk = clk_get(NULL, "dpll5_m2_ck");
	clk_prepare_enable(dpll5_m2_clk);
	clk_set_rate(dpll5_m2_clk, OMAP3_DPLL5_FREQ_FOR_USBHOST);

	clk_disable_unprepare(dpll5_m2_clk);
	clk_disable_unprepare(dpll5_clk);
}

static int __init omap3xxx_dt_clk_init(int soc_type)
{
	if (soc_type == OMAP3_SOC_AM35XX || soc_type == OMAP3_SOC_OMAP3630 ||
	    soc_type == OMAP3_SOC_OMAP3430_ES1 ||
	    soc_type == OMAP3_SOC_OMAP3430_ES2_PLUS)
		ti_dt_clocks_register(omap3xxx_clks);

	if (soc_type == OMAP3_SOC_AM35XX)
		ti_dt_clocks_register(am35xx_clks);

	if (soc_type == OMAP3_SOC_OMAP3630 || soc_type == OMAP3_SOC_AM35XX ||
	    soc_type == OMAP3_SOC_OMAP3430_ES2_PLUS)
		ti_dt_clocks_register(omap36xx_am35xx_omap3430es2plus_clks);

	if (soc_type == OMAP3_SOC_OMAP3430_ES1)
		ti_dt_clocks_register(omap3430es1_clks);

	if (soc_type == OMAP3_SOC_OMAP3430_ES2_PLUS ||
	    soc_type == OMAP3_SOC_OMAP3630)
		ti_dt_clocks_register(omap36xx_omap3430es2plus_clks);

	omap2_clk_disable_autoidle_all();

	ti_clk_add_aliases();

	omap2_clk_enable_init_clocks(enable_init_clks,
				     ARRAY_SIZE(enable_init_clks));

	pr_info("Clocking rate (Crystal/Core/MPU): %ld.%01ld/%ld/%ld MHz\n",
		(clk_get_rate(clk_get_sys(NULL, "osc_sys_ck")) / 1000000),
		(clk_get_rate(clk_get_sys(NULL, "osc_sys_ck")) / 100000) % 10,
		(clk_get_rate(clk_get_sys(NULL, "core_ck")) / 1000000),
		(clk_get_rate(clk_get_sys(NULL, "arm_fck")) / 1000000));

	if (soc_type != OMAP3_SOC_OMAP3430_ES1)
		omap3_clk_lock_dpll5();

	return 0;
}

int __init omap3430_dt_clk_init(void)
{
	return omap3xxx_dt_clk_init(OMAP3_SOC_OMAP3430_ES2_PLUS);
}

int __init omap3630_dt_clk_init(void)
{
	return omap3xxx_dt_clk_init(OMAP3_SOC_OMAP3630);
}

int __init am35xx_dt_clk_init(void)
{
	return omap3xxx_dt_clk_init(OMAP3_SOC_AM35XX);
}
