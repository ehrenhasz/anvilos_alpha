


#include <linux/clk-provider.h>
#include <linux/kernel.h>
#include <linux/reset-controller.h>
#include <linux/spinlock.h>

struct clk_div_table;
struct regmap;


struct aspeed_gate_data {
	u8		clock_idx;
	s8		reset_idx;
	const char	*name;
	const char	*parent_name;
	unsigned long	flags;
};


struct aspeed_clk_gate {
	struct clk_hw	hw;
	struct regmap	*map;
	u8		clock_idx;
	s8		reset_idx;
	u8		flags;
	spinlock_t	*lock;
};

#define to_aspeed_clk_gate(_hw) container_of(_hw, struct aspeed_clk_gate, hw)


struct aspeed_reset {
	struct regmap			*map;
	struct reset_controller_dev	rcdev;
};

#define to_aspeed_reset(p) container_of((p), struct aspeed_reset, rcdev)


struct aspeed_clk_soc_data {
	const struct clk_div_table *div_table;
	const struct clk_div_table *eclk_div_table;
	const struct clk_div_table *mac_div_table;
	struct clk_hw *(*calc_pll)(const char *name, u32 val);
};
