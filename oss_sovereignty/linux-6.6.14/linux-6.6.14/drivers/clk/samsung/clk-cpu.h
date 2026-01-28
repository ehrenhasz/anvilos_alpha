#ifndef __SAMSUNG_CLK_CPU_H
#define __SAMSUNG_CLK_CPU_H
#include "clk.h"
struct exynos_cpuclk_cfg_data {
	unsigned long	prate;
	unsigned long	div0;
	unsigned long	div1;
};
struct exynos_cpuclk {
	struct clk_hw				hw;
	const struct clk_hw			*alt_parent;
	void __iomem				*ctrl_base;
	spinlock_t				*lock;
	const struct exynos_cpuclk_cfg_data	*cfg;
	const unsigned long			num_cfgs;
	struct notifier_block			clk_nb;
	unsigned long				flags;
#define CLK_CPU_HAS_DIV1		(1 << 0)
#define CLK_CPU_NEEDS_DEBUG_ALT_DIV	(1 << 1)
#define CLK_CPU_HAS_E5433_REGS_LAYOUT	(1 << 2)
};
#endif  
