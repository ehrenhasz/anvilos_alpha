
 

#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include "clk.h"

struct rockchip_mmc_clock {
	struct clk_hw	hw;
	void __iomem	*reg;
	int		id;
	int		shift;
	int		cached_phase;
	struct notifier_block clk_rate_change_nb;
};

#define to_mmc_clock(_hw) container_of(_hw, struct rockchip_mmc_clock, hw)

#define RK3288_MMC_CLKGEN_DIV 2

static unsigned long rockchip_mmc_recalc(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	return parent_rate / RK3288_MMC_CLKGEN_DIV;
}

#define ROCKCHIP_MMC_DELAY_SEL BIT(10)
#define ROCKCHIP_MMC_DEGREE_MASK 0x3
#define ROCKCHIP_MMC_DELAYNUM_OFFSET 2
#define ROCKCHIP_MMC_DELAYNUM_MASK (0xff << ROCKCHIP_MMC_DELAYNUM_OFFSET)

#define PSECS_PER_SEC 1000000000000LL

 
#define ROCKCHIP_MMC_DELAY_ELEMENT_PSEC 60

static int rockchip_mmc_get_phase(struct clk_hw *hw)
{
	struct rockchip_mmc_clock *mmc_clock = to_mmc_clock(hw);
	unsigned long rate = clk_hw_get_rate(hw);
	u32 raw_value;
	u16 degrees;
	u32 delay_num = 0;

	 
	if (!rate)
		return 0;

	raw_value = readl(mmc_clock->reg) >> (mmc_clock->shift);

	degrees = (raw_value & ROCKCHIP_MMC_DEGREE_MASK) * 90;

	if (raw_value & ROCKCHIP_MMC_DELAY_SEL) {
		 
		unsigned long factor = (ROCKCHIP_MMC_DELAY_ELEMENT_PSEC / 10) *
					36 * (rate / 10000);

		delay_num = (raw_value & ROCKCHIP_MMC_DELAYNUM_MASK);
		delay_num >>= ROCKCHIP_MMC_DELAYNUM_OFFSET;
		degrees += DIV_ROUND_CLOSEST(delay_num * factor, 1000000);
	}

	return degrees % 360;
}

static int rockchip_mmc_set_phase(struct clk_hw *hw, int degrees)
{
	struct rockchip_mmc_clock *mmc_clock = to_mmc_clock(hw);
	unsigned long rate = clk_hw_get_rate(hw);
	u8 nineties, remainder;
	u8 delay_num;
	u32 raw_value;
	u32 delay;

	 
	if (!rate) {
		pr_err("%s: invalid clk rate\n", __func__);
		return -EINVAL;
	}

	nineties = degrees / 90;
	remainder = (degrees % 90);

	 

	 
	delay = 10000000;  
	delay *= remainder;
	delay = DIV_ROUND_CLOSEST(delay,
			(rate / 1000) * 36 *
				(ROCKCHIP_MMC_DELAY_ELEMENT_PSEC / 10));

	delay_num = (u8) min_t(u32, delay, 255);

	raw_value = delay_num ? ROCKCHIP_MMC_DELAY_SEL : 0;
	raw_value |= delay_num << ROCKCHIP_MMC_DELAYNUM_OFFSET;
	raw_value |= nineties;
	writel(HIWORD_UPDATE(raw_value, 0x07ff, mmc_clock->shift),
	       mmc_clock->reg);

	pr_debug("%s->set_phase(%d) delay_nums=%u reg[0x%p]=0x%03x actual_degrees=%d\n",
		clk_hw_get_name(hw), degrees, delay_num,
		mmc_clock->reg, raw_value>>(mmc_clock->shift),
		rockchip_mmc_get_phase(hw)
	);

	return 0;
}

static const struct clk_ops rockchip_mmc_clk_ops = {
	.recalc_rate	= rockchip_mmc_recalc,
	.get_phase	= rockchip_mmc_get_phase,
	.set_phase	= rockchip_mmc_set_phase,
};

#define to_rockchip_mmc_clock(x) \
	container_of(x, struct rockchip_mmc_clock, clk_rate_change_nb)
static int rockchip_mmc_clk_rate_notify(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct rockchip_mmc_clock *mmc_clock = to_rockchip_mmc_clock(nb);
	struct clk_notifier_data *ndata = data;

	 
	if (ndata->old_rate <= ndata->new_rate)
		return NOTIFY_DONE;

	if (event == PRE_RATE_CHANGE)
		mmc_clock->cached_phase =
			rockchip_mmc_get_phase(&mmc_clock->hw);
	else if (mmc_clock->cached_phase != -EINVAL &&
		 event == POST_RATE_CHANGE)
		rockchip_mmc_set_phase(&mmc_clock->hw, mmc_clock->cached_phase);

	return NOTIFY_DONE;
}

struct clk *rockchip_clk_register_mmc(const char *name,
				const char *const *parent_names, u8 num_parents,
				void __iomem *reg, int shift)
{
	struct clk_init_data init;
	struct rockchip_mmc_clock *mmc_clock;
	struct clk *clk;
	int ret;

	mmc_clock = kmalloc(sizeof(*mmc_clock), GFP_KERNEL);
	if (!mmc_clock)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.flags = 0;
	init.num_parents = num_parents;
	init.parent_names = parent_names;
	init.ops = &rockchip_mmc_clk_ops;

	mmc_clock->hw.init = &init;
	mmc_clock->reg = reg;
	mmc_clock->shift = shift;

	clk = clk_register(NULL, &mmc_clock->hw);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		goto err_register;
	}

	mmc_clock->clk_rate_change_nb.notifier_call =
				&rockchip_mmc_clk_rate_notify;
	ret = clk_notifier_register(clk, &mmc_clock->clk_rate_change_nb);
	if (ret)
		goto err_notifier;

	return clk;
err_notifier:
	clk_unregister(clk);
err_register:
	kfree(mmc_clock);
	return ERR_PTR(ret);
}
