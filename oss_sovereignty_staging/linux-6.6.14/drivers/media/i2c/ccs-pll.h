 
 

#ifndef CCS_PLL_H
#define CCS_PLL_H

#include <linux/bits.h>

 
#define CCS_PLL_BUS_TYPE_CSI2_DPHY				0x00
#define CCS_PLL_BUS_TYPE_CSI2_CPHY				0x01

 
 
#define CCS_PLL_FLAG_OP_PIX_CLOCK_PER_LANE			BIT(0)
#define CCS_PLL_FLAG_NO_OP_CLOCKS				BIT(1)
 
#define CCS_PLL_FLAG_LANE_SPEED_MODEL				BIT(2)
#define CCS_PLL_FLAG_LINK_DECOUPLED				BIT(3)
#define CCS_PLL_FLAG_EXT_IP_PLL_DIVIDER				BIT(4)
#define CCS_PLL_FLAG_FLEXIBLE_OP_PIX_CLK_DIV			BIT(5)
#define CCS_PLL_FLAG_FIFO_DERATING				BIT(6)
#define CCS_PLL_FLAG_FIFO_OVERRATING				BIT(7)
#define CCS_PLL_FLAG_DUAL_PLL					BIT(8)
#define CCS_PLL_FLAG_OP_SYS_DDR					BIT(9)
#define CCS_PLL_FLAG_OP_PIX_DDR					BIT(10)

 
struct ccs_pll_branch_fr {
	u16 pre_pll_clk_div;
	u16 pll_multiplier;
	u32 pll_ip_clk_freq_hz;
	u32 pll_op_clk_freq_hz;
};

 
struct ccs_pll_branch_bk {
	u16 sys_clk_div;
	u16 pix_clk_div;
	u32 sys_clk_freq_hz;
	u32 pix_clk_freq_hz;
};

 
struct ccs_pll {
	 
	u8 bus_type;
	u8 op_lanes;
	u8 vt_lanes;
	struct {
		u8 lanes;
	} csi2;
	u8 binning_horizontal;
	u8 binning_vertical;
	u8 scale_m;
	u8 scale_n;
	u8 bits_per_pixel;
	u8 op_bits_per_lane;
	u16 flags;
	u32 link_freq;
	u32 ext_clk_freq_hz;

	 
	struct ccs_pll_branch_fr vt_fr;
	struct ccs_pll_branch_bk vt_bk;
	struct ccs_pll_branch_fr op_fr;
	struct ccs_pll_branch_bk op_bk;

	u32 pixel_rate_csi;
	u32 pixel_rate_pixel_array;
};

 
struct ccs_pll_branch_limits_fr {
	u16 min_pre_pll_clk_div;
	u16 max_pre_pll_clk_div;
	u32 min_pll_ip_clk_freq_hz;
	u32 max_pll_ip_clk_freq_hz;
	u16 min_pll_multiplier;
	u16 max_pll_multiplier;
	u32 min_pll_op_clk_freq_hz;
	u32 max_pll_op_clk_freq_hz;
};

 
struct ccs_pll_branch_limits_bk {
	u16 min_sys_clk_div;
	u16 max_sys_clk_div;
	u32 min_sys_clk_freq_hz;
	u32 max_sys_clk_freq_hz;
	u16 min_pix_clk_div;
	u16 max_pix_clk_div;
	u32 min_pix_clk_freq_hz;
	u32 max_pix_clk_freq_hz;
};

 
struct ccs_pll_limits {
	 
	u32 min_ext_clk_freq_hz;
	u32 max_ext_clk_freq_hz;

	struct ccs_pll_branch_limits_fr vt_fr;
	struct ccs_pll_branch_limits_bk vt_bk;
	struct ccs_pll_branch_limits_fr op_fr;
	struct ccs_pll_branch_limits_bk op_bk;

	 
	u32 min_line_length_pck_bin;
	u32 min_line_length_pck;
};

struct device;

 
int ccs_pll_calculate(struct device *dev, const struct ccs_pll_limits *limits,
		      struct ccs_pll *pll);

#endif  
