
 

#include <linux/kernel.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/of.h>
#include "common.h"
#include "dove-divider.h"

 

#define SAR_DOVE_CPU_FREQ		5
#define SAR_DOVE_CPU_FREQ_MASK		0xf
#define SAR_DOVE_L2_RATIO		9
#define SAR_DOVE_L2_RATIO_MASK		0x7
#define SAR_DOVE_DDR_RATIO		12
#define SAR_DOVE_DDR_RATIO_MASK		0xf
#define SAR_DOVE_TCLK_FREQ		23
#define SAR_DOVE_TCLK_FREQ_MASK		0x3

enum { DOVE_CPU_TO_L2, DOVE_CPU_TO_DDR };

static const struct coreclk_ratio dove_coreclk_ratios[] __initconst = {
	{ .id = DOVE_CPU_TO_L2, .name = "l2clk", },
	{ .id = DOVE_CPU_TO_DDR, .name = "ddrclk", }
};

static const u32 dove_tclk_freqs[] __initconst = {
	166666667,
	125000000,
	0, 0
};

static u32 __init dove_get_tclk_freq(void __iomem *sar)
{
	u32 opt = (readl(sar) >> SAR_DOVE_TCLK_FREQ) &
		SAR_DOVE_TCLK_FREQ_MASK;
	return dove_tclk_freqs[opt];
}

static const u32 dove_cpu_freqs[] __initconst = {
	0, 0, 0, 0, 0,
	1000000000,
	933333333, 933333333,
	800000000, 800000000, 800000000,
	1066666667,
	666666667,
	533333333,
	400000000,
	333333333
};

static u32 __init dove_get_cpu_freq(void __iomem *sar)
{
	u32 opt = (readl(sar) >> SAR_DOVE_CPU_FREQ) &
		SAR_DOVE_CPU_FREQ_MASK;
	return dove_cpu_freqs[opt];
}

static const int dove_cpu_l2_ratios[8][2] __initconst = {
	{ 1, 1 }, { 0, 1 }, { 1, 2 }, { 0, 1 },
	{ 1, 3 }, { 0, 1 }, { 1, 4 }, { 0, 1 }
};

static const int dove_cpu_ddr_ratios[16][2] __initconst = {
	{ 1, 1 }, { 0, 1 }, { 1, 2 }, { 2, 5 },
	{ 1, 3 }, { 0, 1 }, { 1, 4 }, { 0, 1 },
	{ 1, 5 }, { 0, 1 }, { 1, 6 }, { 0, 1 },
	{ 1, 7 }, { 0, 1 }, { 1, 8 }, { 1, 10 }
};

static void __init dove_get_clk_ratio(
	void __iomem *sar, int id, int *mult, int *div)
{
	switch (id) {
	case DOVE_CPU_TO_L2:
	{
		u32 opt = (readl(sar) >> SAR_DOVE_L2_RATIO) &
			SAR_DOVE_L2_RATIO_MASK;
		*mult = dove_cpu_l2_ratios[opt][0];
		*div = dove_cpu_l2_ratios[opt][1];
		break;
	}
	case DOVE_CPU_TO_DDR:
	{
		u32 opt = (readl(sar) >> SAR_DOVE_DDR_RATIO) &
			SAR_DOVE_DDR_RATIO_MASK;
		*mult = dove_cpu_ddr_ratios[opt][0];
		*div = dove_cpu_ddr_ratios[opt][1];
		break;
	}
	}
}

static const struct coreclk_soc_desc dove_coreclks = {
	.get_tclk_freq = dove_get_tclk_freq,
	.get_cpu_freq = dove_get_cpu_freq,
	.get_clk_ratio = dove_get_clk_ratio,
	.ratios = dove_coreclk_ratios,
	.num_ratios = ARRAY_SIZE(dove_coreclk_ratios),
};

 

static const struct clk_gating_soc_desc dove_gating_desc[] __initconst = {
	{ "usb0", NULL, 0, 0 },
	{ "usb1", NULL, 1, 0 },
	{ "ge",	"gephy", 2, 0 },
	{ "sata", NULL, 3, 0 },
	{ "pex0", NULL, 4, 0 },
	{ "pex1", NULL, 5, 0 },
	{ "sdio0", NULL, 8, 0 },
	{ "sdio1", NULL, 9, 0 },
	{ "nand", NULL, 10, 0 },
	{ "camera", NULL, 11, 0 },
	{ "i2s0", NULL, 12, 0 },
	{ "i2s1", NULL, 13, 0 },
	{ "crypto", NULL, 15, 0 },
	{ "ac97", NULL, 21, 0 },
	{ "pdma", NULL, 22, 0 },
	{ "xor0", NULL, 23, 0 },
	{ "xor1", NULL, 24, 0 },
	{ "gephy", NULL, 30, 0 },
	{ }
};

static void __init dove_clk_init(struct device_node *np)
{
	struct device_node *cgnp =
		of_find_compatible_node(NULL, NULL, "marvell,dove-gating-clock");
	struct device_node *ddnp =
		of_find_compatible_node(NULL, NULL, "marvell,dove-divider-clock");

	mvebu_coreclk_setup(np, &dove_coreclks);

	if (ddnp) {
		dove_divider_clk_init(ddnp);
		of_node_put(ddnp);
	}

	if (cgnp) {
		mvebu_clk_gating_setup(cgnp, dove_gating_desc);
		of_node_put(cgnp);
	}
}
CLK_OF_DECLARE(dove_clk, "marvell,dove-core-clock", dove_clk_init);
