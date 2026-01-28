#ifndef __QCOM_CLK_RCG_H__
#define __QCOM_CLK_RCG_H__
#include <linux/clk-provider.h>
#include "clk-regmap.h"
#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }
struct freq_tbl {
	unsigned long freq;
	u8 src;
	u8 pre_div;
	u16 m;
	u16 n;
};
struct mn {
	u8		mnctr_en_bit;
	u8		mnctr_reset_bit;
	u8		mnctr_mode_shift;
#define MNCTR_MODE_DUAL 0x2
#define MNCTR_MODE_MASK 0x3
	u8		n_val_shift;
	u8		m_val_shift;
	u8		width;
	bool		reset_in_cc;
};
struct pre_div {
	u8		pre_div_shift;
	u8		pre_div_width;
};
struct src_sel {
	u8		src_sel_shift;
#define SRC_SEL_MASK	0x7
	const struct parent_map	*parent_map;
};
struct clk_rcg {
	u32		ns_reg;
	u32		md_reg;
	struct mn	mn;
	struct pre_div	p;
	struct src_sel	s;
	const struct freq_tbl	*freq_tbl;
	struct clk_regmap	clkr;
};
extern const struct clk_ops clk_rcg_ops;
extern const struct clk_ops clk_rcg_floor_ops;
extern const struct clk_ops clk_rcg_bypass_ops;
extern const struct clk_ops clk_rcg_bypass2_ops;
extern const struct clk_ops clk_rcg_pixel_ops;
extern const struct clk_ops clk_rcg_esc_ops;
extern const struct clk_ops clk_rcg_lcc_ops;
#define to_clk_rcg(_hw) container_of(to_clk_regmap(_hw), struct clk_rcg, clkr)
struct clk_dyn_rcg {
	u32	ns_reg[2];
	u32	md_reg[2];
	u32	bank_reg;
	u8	mux_sel_bit;
	struct mn	mn[2];
	struct pre_div	p[2];
	struct src_sel	s[2];
	const struct freq_tbl *freq_tbl;
	struct clk_regmap clkr;
};
extern const struct clk_ops clk_dyn_rcg_ops;
#define to_clk_dyn_rcg(_hw) \
	container_of(to_clk_regmap(_hw), struct clk_dyn_rcg, clkr)
struct clk_rcg2 {
	u32			cmd_rcgr;
	u8			mnd_width;
	u8			hid_width;
	u8			safe_src_index;
	const struct parent_map	*parent_map;
	const struct freq_tbl	*freq_tbl;
	struct clk_regmap	clkr;
	u8			cfg_off;
	u32			parked_cfg;
	bool			hw_clk_ctrl;
};
#define to_clk_rcg2(_hw) container_of(to_clk_regmap(_hw), struct clk_rcg2, clkr)
struct clk_rcg2_gfx3d {
	u8 div;
	struct clk_rcg2 rcg;
	struct clk_hw **hws;
};
#define to_clk_rcg2_gfx3d(_hw) \
	container_of(to_clk_rcg2(_hw), struct clk_rcg2_gfx3d, rcg)
extern const struct clk_ops clk_rcg2_ops;
extern const struct clk_ops clk_rcg2_floor_ops;
extern const struct clk_ops clk_rcg2_mux_closest_ops;
extern const struct clk_ops clk_edp_pixel_ops;
extern const struct clk_ops clk_byte_ops;
extern const struct clk_ops clk_byte2_ops;
extern const struct clk_ops clk_pixel_ops;
extern const struct clk_ops clk_gfx3d_ops;
extern const struct clk_ops clk_rcg2_shared_ops;
extern const struct clk_ops clk_dp_ops;
struct clk_rcg_dfs_data {
	struct clk_rcg2 *rcg;
	struct clk_init_data *init;
};
#define DEFINE_RCG_DFS(r) \
	{ .rcg = &r, .init = &r##_init }
extern int qcom_cc_register_rcg_dfs(struct regmap *regmap,
				    const struct clk_rcg_dfs_data *rcgs,
				    size_t len);
#endif
