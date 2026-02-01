 
 

#ifndef __DRIVERS_CLK_TEGRA_CLK_DFLL_H
#define __DRIVERS_CLK_TEGRA_CLK_DFLL_H

#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/types.h>

#include "cvb.h"

 
struct tegra_dfll_soc_data {
	struct device *dev;
	unsigned long max_freq;
	const struct cvb_table *cvb;
	struct rail_alignment alignment;

	void (*init_clock_trimmers)(void);
	void (*set_clock_trimmers_high)(void);
	void (*set_clock_trimmers_low)(void);
};

int tegra_dfll_register(struct platform_device *pdev,
			struct tegra_dfll_soc_data *soc);
struct tegra_dfll_soc_data *tegra_dfll_unregister(struct platform_device *pdev);
int tegra_dfll_runtime_suspend(struct device *dev);
int tegra_dfll_runtime_resume(struct device *dev);
int tegra_dfll_suspend(struct device *dev);
int tegra_dfll_resume(struct device *dev);

#endif  
