#ifndef __MESON_CLK_CPU_DYNDIV_H
#define __MESON_CLK_CPU_DYNDIV_H
#include <linux/clk-provider.h>
#include "parm.h"
struct meson_clk_cpu_dyndiv_data {
	struct parm div;
	struct parm dyn;
};
extern const struct clk_ops meson_clk_cpu_dyndiv_ops;
#endif  
