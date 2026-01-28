#ifndef __QCOM_CLK_REGMAP_PHY_MUX_H__
#define __QCOM_CLK_REGMAP_PHY_MUX_H__
#include "clk-regmap.h"
struct clk_regmap_phy_mux {
	u32			reg;
	struct clk_regmap	clkr;
};
extern const struct clk_ops clk_regmap_phy_mux_ops;
#endif
