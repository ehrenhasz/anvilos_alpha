
 
#undef DEBUG

#include <linux/kernel.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/clk/ti.h>

#include "clock.h"

 
#define OMAP24XX_CM_FCLKEN2		0x04
#define CM_AUTOIDLE			0x30
#define CM_ICLKEN			0x10
#define CM_IDLEST			0x20

#define OMAP24XX_CM_IDLEST_VAL		0

 

 
void omap2_clkt_iclk_allow_idle(struct clk_hw_omap *clk)
{
	u32 v;
	struct clk_omap_reg r;

	memcpy(&r, &clk->enable_reg, sizeof(r));
	r.offset ^= (CM_AUTOIDLE ^ CM_ICLKEN);

	v = ti_clk_ll_ops->clk_readl(&r);
	v |= (1 << clk->enable_bit);
	ti_clk_ll_ops->clk_writel(v, &r);
}

 
void omap2_clkt_iclk_deny_idle(struct clk_hw_omap *clk)
{
	u32 v;
	struct clk_omap_reg r;

	memcpy(&r, &clk->enable_reg, sizeof(r));

	r.offset ^= (CM_AUTOIDLE ^ CM_ICLKEN);

	v = ti_clk_ll_ops->clk_readl(&r);
	v &= ~(1 << clk->enable_bit);
	ti_clk_ll_ops->clk_writel(v, &r);
}

 
static void omap2430_clk_i2chs_find_idlest(struct clk_hw_omap *clk,
					   struct clk_omap_reg *idlest_reg,
					   u8 *idlest_bit,
					   u8 *idlest_val)
{
	memcpy(idlest_reg, &clk->enable_reg, sizeof(*idlest_reg));
	idlest_reg->offset ^= (OMAP24XX_CM_FCLKEN2 ^ CM_IDLEST);
	*idlest_bit = clk->enable_bit;
	*idlest_val = OMAP24XX_CM_IDLEST_VAL;
}

 

const struct clk_hw_omap_ops clkhwops_iclk = {
	.allow_idle	= omap2_clkt_iclk_allow_idle,
	.deny_idle	= omap2_clkt_iclk_deny_idle,
};

const struct clk_hw_omap_ops clkhwops_iclk_wait = {
	.allow_idle	= omap2_clkt_iclk_allow_idle,
	.deny_idle	= omap2_clkt_iclk_deny_idle,
	.find_idlest	= omap2_clk_dflt_find_idlest,
	.find_companion	= omap2_clk_dflt_find_companion,
};

 
const struct clk_hw_omap_ops clkhwops_omap2430_i2chs_wait = {
	.find_idlest	= omap2430_clk_i2chs_find_idlest,
	.find_companion	= omap2_clk_dflt_find_companion,
};
