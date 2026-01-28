


#ifndef __DRIVERS_CLK_INGENIC_CGU_H__
#define __DRIVERS_CLK_INGENIC_CGU_H__

#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/spinlock.h>


struct ingenic_cgu_pll_info {
	unsigned reg;
	unsigned rate_multiplier;
	const s8 *od_encoding;
	u8 m_shift, m_bits, m_offset;
	u8 n_shift, n_bits, n_offset;
	u8 od_shift, od_bits, od_max;
	unsigned bypass_reg;
	s8 bypass_bit;
	s8 enable_bit;
	s8 stable_bit;
	void (*calc_m_n_od)(const struct ingenic_cgu_pll_info *pll_info,
			    unsigned long rate, unsigned long parent_rate,
			    unsigned int *m, unsigned int *n, unsigned int *od);
	void (*set_rate_hook)(const struct ingenic_cgu_pll_info *pll_info,
			      unsigned long rate, unsigned long parent_rate);
};


struct ingenic_cgu_mux_info {
	unsigned reg;
	u8 shift;
	u8 bits;
};


struct ingenic_cgu_div_info {
	unsigned reg;
	u8 shift;
	u8 div;
	u8 bits;
	s8 ce_bit;
	s8 busy_bit;
	s8 stop_bit;
	u8 bypass_mask;
	const u8 *div_table;
};


struct ingenic_cgu_fixdiv_info {
	unsigned div;
};


struct ingenic_cgu_gate_info {
	unsigned reg;
	u8 bit;
	bool clear_to_gate;
	u16 delay_us;
};


struct ingenic_cgu_custom_info {
	const struct clk_ops *clk_ops;
};


struct ingenic_cgu_clk_info {
	const char *name;

	enum {
		CGU_CLK_NONE		= 0,
		CGU_CLK_EXT		= BIT(0),
		CGU_CLK_PLL		= BIT(1),
		CGU_CLK_GATE		= BIT(2),
		CGU_CLK_MUX		= BIT(3),
		CGU_CLK_MUX_GLITCHFREE	= BIT(4),
		CGU_CLK_DIV		= BIT(5),
		CGU_CLK_FIXDIV		= BIT(6),
		CGU_CLK_CUSTOM		= BIT(7),
	} type;

	unsigned long flags;

	int parents[4];

	union {
		struct ingenic_cgu_pll_info pll;

		struct {
			struct ingenic_cgu_gate_info gate;
			struct ingenic_cgu_mux_info mux;
			struct ingenic_cgu_div_info div;
			struct ingenic_cgu_fixdiv_info fixdiv;
		};

		struct ingenic_cgu_custom_info custom;
	};
};


struct ingenic_cgu {
	struct device_node *np;
	void __iomem *base;

	const struct ingenic_cgu_clk_info *clock_info;
	struct clk_onecell_data clocks;

	spinlock_t lock;
};


struct ingenic_clk {
	struct clk_hw hw;
	struct ingenic_cgu *cgu;
	unsigned idx;
};

#define to_ingenic_clk(_hw) container_of(_hw, struct ingenic_clk, hw)


struct ingenic_cgu *
ingenic_cgu_new(const struct ingenic_cgu_clk_info *clock_info,
		unsigned num_clocks, struct device_node *np);


int ingenic_cgu_register_clocks(struct ingenic_cgu *cgu);

#endif 
