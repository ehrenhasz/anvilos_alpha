


#ifndef __SIFIVE_CLK_FU540_PRCI_H
#define __SIFIVE_CLK_FU540_PRCI_H


#include <linux/module.h>

#include <dt-bindings/clock/sifive-fu540-prci.h>

#include "sifive-prci.h"



static struct __prci_wrpll_data sifive_fu540_prci_corepll_data = {
	.cfg0_offs = PRCI_COREPLLCFG0_OFFSET,
	.cfg1_offs = PRCI_COREPLLCFG1_OFFSET,
	.enable_bypass = sifive_prci_coreclksel_use_hfclk,
	.disable_bypass = sifive_prci_coreclksel_use_corepll,
};

static struct __prci_wrpll_data sifive_fu540_prci_ddrpll_data = {
	.cfg0_offs = PRCI_DDRPLLCFG0_OFFSET,
	.cfg1_offs = PRCI_DDRPLLCFG1_OFFSET,
};

static struct __prci_wrpll_data sifive_fu540_prci_gemgxlpll_data = {
	.cfg0_offs = PRCI_GEMGXLPLLCFG0_OFFSET,
	.cfg1_offs = PRCI_GEMGXLPLLCFG1_OFFSET,
};



static const struct clk_ops sifive_fu540_prci_wrpll_clk_ops = {
	.set_rate = sifive_prci_wrpll_set_rate,
	.round_rate = sifive_prci_wrpll_round_rate,
	.recalc_rate = sifive_prci_wrpll_recalc_rate,
	.enable = sifive_prci_clock_enable,
	.disable = sifive_prci_clock_disable,
	.is_enabled = sifive_clk_is_enabled,
};

static const struct clk_ops sifive_fu540_prci_wrpll_ro_clk_ops = {
	.recalc_rate = sifive_prci_wrpll_recalc_rate,
};

static const struct clk_ops sifive_fu540_prci_tlclksel_clk_ops = {
	.recalc_rate = sifive_prci_tlclksel_recalc_rate,
};


static struct __prci_clock __prci_init_clocks_fu540[] = {
	[FU540_PRCI_CLK_COREPLL] = {
		.name = "corepll",
		.parent_name = "hfclk",
		.ops = &sifive_fu540_prci_wrpll_clk_ops,
		.pwd = &sifive_fu540_prci_corepll_data,
	},
	[FU540_PRCI_CLK_DDRPLL] = {
		.name = "ddrpll",
		.parent_name = "hfclk",
		.ops = &sifive_fu540_prci_wrpll_ro_clk_ops,
		.pwd = &sifive_fu540_prci_ddrpll_data,
	},
	[FU540_PRCI_CLK_GEMGXLPLL] = {
		.name = "gemgxlpll",
		.parent_name = "hfclk",
		.ops = &sifive_fu540_prci_wrpll_clk_ops,
		.pwd = &sifive_fu540_prci_gemgxlpll_data,
	},
	[FU540_PRCI_CLK_TLCLK] = {
		.name = "tlclk",
		.parent_name = "corepll",
		.ops = &sifive_fu540_prci_tlclksel_clk_ops,
	},
};

static const struct prci_clk_desc prci_clk_fu540 = {
	.clks = __prci_init_clocks_fu540,
	.num_clks = ARRAY_SIZE(__prci_init_clocks_fu540),
};

#endif 
