#ifndef __CLK_REGMAP_H
#define __CLK_REGMAP_H
#include <linux/clk-provider.h>
#include <linux/regmap.h>
struct clk_regmap {
	struct clk_hw	hw;
	struct regmap	*map;
	void		*data;
};
static inline struct clk_regmap *to_clk_regmap(struct clk_hw *hw)
{
	return container_of(hw, struct clk_regmap, hw);
}
struct clk_regmap_gate_data {
	unsigned int	offset;
	u8		bit_idx;
	u8		flags;
};
static inline struct clk_regmap_gate_data *
clk_get_regmap_gate_data(struct clk_regmap *clk)
{
	return (struct clk_regmap_gate_data *)clk->data;
}
extern const struct clk_ops clk_regmap_gate_ops;
extern const struct clk_ops clk_regmap_gate_ro_ops;
struct clk_regmap_div_data {
	unsigned int	offset;
	u8		shift;
	u8		width;
	u8		flags;
	const struct clk_div_table	*table;
};
static inline struct clk_regmap_div_data *
clk_get_regmap_div_data(struct clk_regmap *clk)
{
	return (struct clk_regmap_div_data *)clk->data;
}
extern const struct clk_ops clk_regmap_divider_ops;
extern const struct clk_ops clk_regmap_divider_ro_ops;
struct clk_regmap_mux_data {
	unsigned int	offset;
	u32		*table;
	u32		mask;
	u8		shift;
	u8		flags;
};
static inline struct clk_regmap_mux_data *
clk_get_regmap_mux_data(struct clk_regmap *clk)
{
	return (struct clk_regmap_mux_data *)clk->data;
}
extern const struct clk_ops clk_regmap_mux_ops;
extern const struct clk_ops clk_regmap_mux_ro_ops;
#define __MESON_PCLK(_name, _reg, _bit, _ops, _pname)			\
struct clk_regmap _name = {						\
	.data = &(struct clk_regmap_gate_data){				\
		.offset = (_reg),					\
		.bit_idx = (_bit),					\
	},								\
	.hw.init = &(struct clk_init_data) {				\
		.name = #_name,						\
		.ops = _ops,						\
		.parent_hws = (const struct clk_hw *[]) { _pname },	\
		.num_parents = 1,					\
		.flags = (CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED),	\
	},								\
}
#define MESON_PCLK(_name, _reg, _bit, _pname)	\
	__MESON_PCLK(_name, _reg, _bit, &clk_regmap_gate_ops, _pname)
#define MESON_PCLK_RO(_name, _reg, _bit, _pname)	\
	__MESON_PCLK(_name, _reg, _bit, &clk_regmap_gate_ro_ops, _pname)
#endif  
