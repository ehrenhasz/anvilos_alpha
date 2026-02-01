 
 

#ifndef _CLK_IPROC_H
#define _CLK_IPROC_H

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/clk-provider.h>

#define IPROC_CLK_NAME_LEN 25
#define IPROC_CLK_INVALID_OFFSET 0xffffffff
#define bit_mask(width) ((1 << (width)) - 1)

 
#define IPROC_CLK_AON BIT(0)

 
#define IPROC_CLK_PLL_ASIU BIT(1)

 
#define IPROC_CLK_PLL_HAS_NDIV_FRAC BIT(2)

 
#define IPROC_CLK_NEEDS_READ_BACK BIT(3)

 
#define IPROC_CLK_PLL_NEEDS_SW_CFG BIT(4)

 
#define IPROC_CLK_EMBED_PWRCTRL BIT(5)

 
#define IPROC_CLK_PLL_SPLIT_STAT_CTRL BIT(6)

 
#define IPROC_CLK_MCLK_DIV_BY_2 BIT(7)

 
#define IPROC_CLK_PLL_USER_MODE_ON BIT(8)

 
#define IPROC_CLK_PLL_RESET_ACTIVE_LOW BIT(9)

 
#define IPROC_CLK_PLL_CALC_PARAM BIT(10)

 
struct iproc_pll_vco_param {
	unsigned long rate;
	unsigned int ndiv_int;
	unsigned int ndiv_frac;
	unsigned int pdiv;
};

struct iproc_clk_reg_op {
	unsigned int offset;
	unsigned int shift;
	unsigned int width;
};

 
struct iproc_asiu_gate {
	unsigned int offset;
	unsigned int en_shift;
};

 
struct iproc_pll_aon_pwr_ctrl {
	unsigned int offset;
	unsigned int pwr_width;
	unsigned int pwr_shift;
	unsigned int iso_shift;
};

 
struct iproc_pll_reset_ctrl {
	unsigned int offset;
	unsigned int reset_shift;
	unsigned int p_reset_shift;
};

 
struct iproc_pll_dig_filter_ctrl {
	unsigned int offset;
	unsigned int ki_shift;
	unsigned int ki_width;
	unsigned int kp_shift;
	unsigned int kp_width;
	unsigned int ka_shift;
	unsigned int ka_width;
};

 
struct iproc_pll_sw_ctrl {
	unsigned int offset;
	unsigned int shift;
};

struct iproc_pll_vco_ctrl {
	unsigned int u_offset;
	unsigned int l_offset;
};

 
struct iproc_pll_ctrl {
	unsigned long flags;
	struct iproc_pll_aon_pwr_ctrl aon;
	struct iproc_asiu_gate asiu;
	struct iproc_pll_reset_ctrl reset;
	struct iproc_pll_dig_filter_ctrl dig_filter;
	struct iproc_pll_sw_ctrl sw_ctrl;
	struct iproc_clk_reg_op ndiv_int;
	struct iproc_clk_reg_op ndiv_frac;
	struct iproc_clk_reg_op pdiv;
	struct iproc_pll_vco_ctrl vco_ctrl;
	struct iproc_clk_reg_op status;
	struct iproc_clk_reg_op macro_mode;
};

 
struct iproc_clk_enable_ctrl {
	unsigned int offset;
	unsigned int enable_shift;
	unsigned int hold_shift;
	unsigned int bypass_shift;
};

 
struct iproc_clk_ctrl {
	unsigned int channel;
	unsigned long flags;
	struct iproc_clk_enable_ctrl enable;
	struct iproc_clk_reg_op mdiv;
};

 
struct iproc_asiu_div {
	unsigned int offset;
	unsigned int en_shift;
	unsigned int high_shift;
	unsigned int high_width;
	unsigned int low_shift;
	unsigned int low_width;
};

void iproc_armpll_setup(struct device_node *node);
void iproc_pll_clk_setup(struct device_node *node,
			 const struct iproc_pll_ctrl *pll_ctrl,
			 const struct iproc_pll_vco_param *vco,
			 unsigned int num_vco_entries,
			 const struct iproc_clk_ctrl *clk_ctrl,
			 unsigned int num_clks);
void iproc_asiu_setup(struct device_node *node,
		      const struct iproc_asiu_div *div,
		      const struct iproc_asiu_gate *gate,
		      unsigned int num_clks);

#endif  
