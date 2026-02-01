
 

 

#include "pinmux-aspeed.h"

static const char *const aspeed_pinmux_ips[] = {
	[ASPEED_IP_SCU] = "SCU",
	[ASPEED_IP_GFX] = "GFX",
	[ASPEED_IP_LPC] = "LPC",
};

static inline void aspeed_sig_desc_print_val(
		const struct aspeed_sig_desc *desc, bool enable, u32 rv)
{
	pr_debug("Want %s%X[0x%08X]=0x%X, got 0x%X from 0x%08X\n",
			aspeed_pinmux_ips[desc->ip], desc->reg,
			desc->mask, enable ? desc->enable : desc->disable,
			(rv & desc->mask) >> __ffs(desc->mask), rv);
}

 
int aspeed_sig_desc_eval(const struct aspeed_sig_desc *desc,
			 bool enabled, struct regmap *map)
{
	int ret;
	unsigned int raw;
	u32 want;

	if (!map)
		return -ENODEV;

	ret = regmap_read(map, desc->reg, &raw);
	if (ret)
		return ret;

	aspeed_sig_desc_print_val(desc, enabled, raw);
	want = enabled ? desc->enable : desc->disable;

	return ((raw & desc->mask) >> __ffs(desc->mask)) == want;
}

 
int aspeed_sig_expr_eval(struct aspeed_pinmux_data *ctx,
			 const struct aspeed_sig_expr *expr, bool enabled)
{
	int ret;
	int i;

	if (ctx->ops->eval)
		return ctx->ops->eval(ctx, expr, enabled);

	for (i = 0; i < expr->ndescs; i++) {
		const struct aspeed_sig_desc *desc = &expr->descs[i];

		ret = aspeed_sig_desc_eval(desc, enabled, ctx->maps[desc->ip]);
		if (ret <= 0)
			return ret;
	}

	return 1;
}
