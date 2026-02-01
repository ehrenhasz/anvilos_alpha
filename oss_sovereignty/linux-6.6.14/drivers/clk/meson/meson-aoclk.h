 
 

#ifndef __MESON_AOCLK_H__
#define __MESON_AOCLK_H__

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

#include "clk-regmap.h"
#include "meson-clkc-utils.h"

struct meson_aoclk_data {
	const unsigned int			reset_reg;
	const int				num_reset;
	const unsigned int			*reset;
	const int				num_clks;
	struct clk_regmap			**clks;
	struct meson_clk_hw_data		hw_clks;
};

struct meson_aoclk_reset_controller {
	struct reset_controller_dev		reset;
	const struct meson_aoclk_data		*data;
	struct regmap				*regmap;
};

int meson_aoclkc_probe(struct platform_device *pdev);
#endif
