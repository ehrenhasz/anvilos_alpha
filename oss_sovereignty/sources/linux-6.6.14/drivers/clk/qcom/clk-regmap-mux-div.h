


#ifndef __QCOM_CLK_REGMAP_MUX_DIV_H__
#define __QCOM_CLK_REGMAP_MUX_DIV_H__

#include <linux/clk-provider.h>
#include "clk-regmap.h"


struct clk_regmap_mux_div {
	u32				reg_offset;
	u32				hid_width;
	u32				hid_shift;
	u32				src_width;
	u32				src_shift;
	u32				div;
	u32				src;
	const u32			*parent_map;
	struct clk_regmap		clkr;
	struct clk			*pclk;
	struct notifier_block		clk_nb;
};

extern const struct clk_ops clk_regmap_mux_div_ops;
extern int mux_div_set_src_div(struct clk_regmap_mux_div *md, u32 src, u32 div);

#endif
