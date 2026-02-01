
 

#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

 

#define REG_LPF_LOW_L		0x140
#define REG_LPF_LOW_H		0x144
#define REG_LPF_HIGH_BOTTOM	0x148
#define REG_LPF_HIGH_TOP	0x14c
#define REG_LPF_TOGGLE		0x150
#define REG_LPF_MYSTERYTWO	0x154
#define REG_LPF_UPDATE_COUNT	0x15c
#define REG_LPF_MYSTERYONE	0x160
#define REG_LPF_TRANSITIONCTRL	0x164
#define REG_LPF_LOCK		0x174
#define REG_CURRENT		0x180

#define LPF_LOCK_TIMEOUT	100000000

#define MULTIPLIER_1		16
#define MULTIPLIER_2		524288
#define MULTIPLIER		(MULTIPLIER_1 * MULTIPLIER_2)

struct msc313_cpupll {
	void __iomem *base;
	struct clk_hw clk_hw;
};

#define to_cpupll(_hw) container_of(_hw, struct msc313_cpupll, clk_hw)

static u32 msc313_cpupll_reg_read32(struct msc313_cpupll *cpupll, unsigned int reg)
{
	u32 value;

	value = ioread16(cpupll->base + reg + 4) << 16;
	value |= ioread16(cpupll->base + reg);

	return value;
}

static void msc313_cpupll_reg_write32(struct msc313_cpupll *cpupll, unsigned int reg, u32 value)
{
	u16 l = value & 0xffff, h = (value >> 16) & 0xffff;

	iowrite16(l, cpupll->base + reg);
	iowrite16(h, cpupll->base + reg + 4);
}

static void msc313_cpupll_setfreq(struct msc313_cpupll *cpupll, u32 regvalue)
{
	ktime_t timeout;

	msc313_cpupll_reg_write32(cpupll, REG_LPF_HIGH_BOTTOM, regvalue);

	iowrite16(0x1, cpupll->base + REG_LPF_MYSTERYONE);
	iowrite16(0x6, cpupll->base + REG_LPF_MYSTERYTWO);
	iowrite16(0x8, cpupll->base + REG_LPF_UPDATE_COUNT);
	iowrite16(BIT(12), cpupll->base + REG_LPF_TRANSITIONCTRL);

	iowrite16(0, cpupll->base + REG_LPF_TOGGLE);
	iowrite16(1, cpupll->base + REG_LPF_TOGGLE);

	timeout = ktime_add_ns(ktime_get(), LPF_LOCK_TIMEOUT);
	while (!(ioread16(cpupll->base + REG_LPF_LOCK))) {
		if (ktime_after(ktime_get(), timeout)) {
			pr_err("timeout waiting for LPF_LOCK\n");
			return;
		}
		cpu_relax();
	}

	iowrite16(0, cpupll->base + REG_LPF_TOGGLE);

	msc313_cpupll_reg_write32(cpupll, REG_LPF_LOW_L, regvalue);
}

static unsigned long msc313_cpupll_frequencyforreg(u32 reg, unsigned long parent_rate)
{
	unsigned long long prescaled = ((unsigned long long)parent_rate) * MULTIPLIER;

	if (prescaled == 0 || reg == 0)
		return 0;
	return DIV_ROUND_DOWN_ULL(prescaled, reg);
}

static u32 msc313_cpupll_regforfrequecy(unsigned long rate, unsigned long parent_rate)
{
	unsigned long long prescaled = ((unsigned long long)parent_rate) * MULTIPLIER;

	if (prescaled == 0 || rate == 0)
		return 0;
	return DIV_ROUND_UP_ULL(prescaled, rate);
}

static unsigned long msc313_cpupll_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct msc313_cpupll *cpupll = to_cpupll(hw);

	return msc313_cpupll_frequencyforreg(msc313_cpupll_reg_read32(cpupll, REG_LPF_LOW_L),
					     parent_rate);
}

static long msc313_cpupll_round_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long *parent_rate)
{
	u32 reg = msc313_cpupll_regforfrequecy(rate, *parent_rate);
	long rounded = msc313_cpupll_frequencyforreg(reg, *parent_rate);

	 
	for (; rounded >= rate && reg > 0; reg--)
		rounded = msc313_cpupll_frequencyforreg(reg, *parent_rate);

	return rounded;
}

static int msc313_cpupll_set_rate(struct clk_hw *hw, unsigned long rate, unsigned long parent_rate)
{
	struct msc313_cpupll *cpupll = to_cpupll(hw);
	u32 reg = msc313_cpupll_regforfrequecy(rate, parent_rate);

	msc313_cpupll_setfreq(cpupll, reg);

	return 0;
}

static const struct clk_ops msc313_cpupll_ops = {
	.recalc_rate	= msc313_cpupll_recalc_rate,
	.round_rate	= msc313_cpupll_round_rate,
	.set_rate	= msc313_cpupll_set_rate,
};

static const struct of_device_id msc313_cpupll_of_match[] = {
	{ .compatible = "mstar,msc313-cpupll" },
	{}
};

static int msc313_cpupll_probe(struct platform_device *pdev)
{
	struct clk_init_data clk_init = {};
	struct clk_parent_data cpupll_parent = { .index	= 0 };
	struct device *dev = &pdev->dev;
	struct msc313_cpupll *cpupll;
	int ret;

	cpupll = devm_kzalloc(&pdev->dev, sizeof(*cpupll), GFP_KERNEL);
	if (!cpupll)
		return -ENOMEM;

	cpupll->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(cpupll->base))
		return PTR_ERR(cpupll->base);

	 
	msc313_cpupll_reg_write32(cpupll, REG_LPF_LOW_L,
				  msc313_cpupll_reg_read32(cpupll, REG_CURRENT));

	clk_init.name = dev_name(dev);
	clk_init.ops = &msc313_cpupll_ops;
	clk_init.parent_data = &cpupll_parent;
	clk_init.num_parents = 1;
	cpupll->clk_hw.init = &clk_init;

	ret = devm_clk_hw_register(dev, &cpupll->clk_hw);
	if (ret)
		return ret;

	return devm_of_clk_add_hw_provider(&pdev->dev, of_clk_hw_simple_get, &cpupll->clk_hw);
}

static struct platform_driver msc313_cpupll_driver = {
	.driver = {
		.name = "mstar-msc313-cpupll",
		.of_match_table = msc313_cpupll_of_match,
	},
	.probe = msc313_cpupll_probe,
};
builtin_platform_driver(msc313_cpupll_driver);
