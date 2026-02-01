
 

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/kernel.h>
#include <linux/mfd/tps68470.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/platform_data/tps68470.h>
#include <linux/regmap.h>

#define TPS68470_CLK_NAME "tps68470-clk"

#define to_tps68470_clkdata(clkd) \
	container_of(clkd, struct tps68470_clkdata, clkout_hw)

static struct tps68470_clkout_freqs {
	unsigned long freq;
	unsigned int xtaldiv;
	unsigned int plldiv;
	unsigned int postdiv;
	unsigned int buckdiv;
	unsigned int boostdiv;
} clk_freqs[] = {
 
	{ 19200000, 170, 32, 1, 2, 3 },
	{ 20000000, 170, 40, 1, 3, 4 },
	{ 24000000, 170, 80, 1, 4, 8 },
};

struct tps68470_clkdata {
	struct clk_hw clkout_hw;
	struct regmap *regmap;
	unsigned long rate;
};

static int tps68470_clk_is_prepared(struct clk_hw *hw)
{
	struct tps68470_clkdata *clkdata = to_tps68470_clkdata(hw);
	int val;

	if (regmap_read(clkdata->regmap, TPS68470_REG_PLLCTL, &val))
		return 0;

	return val & TPS68470_PLL_EN_MASK;
}

static int tps68470_clk_prepare(struct clk_hw *hw)
{
	struct tps68470_clkdata *clkdata = to_tps68470_clkdata(hw);

	regmap_write(clkdata->regmap, TPS68470_REG_CLKCFG1,
			   (TPS68470_PLL_OUTPUT_ENABLE << TPS68470_OUTPUT_A_SHIFT) |
			   (TPS68470_PLL_OUTPUT_ENABLE << TPS68470_OUTPUT_B_SHIFT));

	regmap_update_bits(clkdata->regmap, TPS68470_REG_PLLCTL,
			   TPS68470_PLL_EN_MASK, TPS68470_PLL_EN_MASK);

	 
	usleep_range(4000, 5000);

	return 0;
}

static void tps68470_clk_unprepare(struct clk_hw *hw)
{
	struct tps68470_clkdata *clkdata = to_tps68470_clkdata(hw);

	 
	regmap_update_bits(clkdata->regmap, TPS68470_REG_PLLCTL, TPS68470_PLL_EN_MASK, 0);

	 
	regmap_write(clkdata->regmap, TPS68470_REG_CLKCFG1, 0);
}

static unsigned long tps68470_clk_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct tps68470_clkdata *clkdata = to_tps68470_clkdata(hw);

	return clkdata->rate;
}

 
static unsigned int tps68470_clk_cfg_lookup(unsigned long rate)
{
	long diff, best_diff = LONG_MAX;
	unsigned int i, best_idx = 0;

	for (i = 0; i < ARRAY_SIZE(clk_freqs); i++) {
		diff = clk_freqs[i].freq - rate;
		if (diff == 0)
			return i;

		diff = abs(diff);
		if (diff < best_diff) {
			best_diff = diff;
			best_idx = i;
		}
	}

	return best_idx;
}

static long tps68470_clk_round_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long *parent_rate)
{
	unsigned int idx = tps68470_clk_cfg_lookup(rate);

	return clk_freqs[idx].freq;
}

static int tps68470_clk_set_rate(struct clk_hw *hw, unsigned long rate,
				 unsigned long parent_rate)
{
	struct tps68470_clkdata *clkdata = to_tps68470_clkdata(hw);
	unsigned int idx = tps68470_clk_cfg_lookup(rate);

	if (rate != clk_freqs[idx].freq)
		return -EINVAL;

	regmap_write(clkdata->regmap, TPS68470_REG_BOOSTDIV, clk_freqs[idx].boostdiv);
	regmap_write(clkdata->regmap, TPS68470_REG_BUCKDIV, clk_freqs[idx].buckdiv);
	regmap_write(clkdata->regmap, TPS68470_REG_PLLSWR, TPS68470_PLLSWR_DEFAULT);
	regmap_write(clkdata->regmap, TPS68470_REG_XTALDIV, clk_freqs[idx].xtaldiv);
	regmap_write(clkdata->regmap, TPS68470_REG_PLLDIV, clk_freqs[idx].plldiv);
	regmap_write(clkdata->regmap, TPS68470_REG_POSTDIV, clk_freqs[idx].postdiv);
	regmap_write(clkdata->regmap, TPS68470_REG_POSTDIV2, clk_freqs[idx].postdiv);
	regmap_write(clkdata->regmap, TPS68470_REG_CLKCFG2, TPS68470_CLKCFG2_DRV_STR_2MA);

	regmap_write(clkdata->regmap, TPS68470_REG_PLLCTL,
		     TPS68470_OSC_EXT_CAP_DEFAULT << TPS68470_OSC_EXT_CAP_SHIFT |
		     TPS68470_CLK_SRC_XTAL << TPS68470_CLK_SRC_SHIFT);

	clkdata->rate = rate;

	return 0;
}

static const struct clk_ops tps68470_clk_ops = {
	.is_prepared = tps68470_clk_is_prepared,
	.prepare = tps68470_clk_prepare,
	.unprepare = tps68470_clk_unprepare,
	.recalc_rate = tps68470_clk_recalc_rate,
	.round_rate = tps68470_clk_round_rate,
	.set_rate = tps68470_clk_set_rate,
};

static int tps68470_clk_probe(struct platform_device *pdev)
{
	struct tps68470_clk_platform_data *pdata = pdev->dev.platform_data;
	struct clk_init_data tps68470_clk_initdata = {
		.name = TPS68470_CLK_NAME,
		.ops = &tps68470_clk_ops,
		 
		.flags = CLK_SET_RATE_GATE,
	};
	struct tps68470_clkdata *tps68470_clkdata;
	struct tps68470_clk_consumer *consumer;
	int ret;
	int i;

	tps68470_clkdata = devm_kzalloc(&pdev->dev, sizeof(*tps68470_clkdata),
					GFP_KERNEL);
	if (!tps68470_clkdata)
		return -ENOMEM;

	tps68470_clkdata->regmap = dev_get_drvdata(pdev->dev.parent);
	tps68470_clkdata->clkout_hw.init = &tps68470_clk_initdata;

	 
	tps68470_clk_set_rate(&tps68470_clkdata->clkout_hw, clk_freqs[0].freq, 0);

	ret = devm_clk_hw_register(&pdev->dev, &tps68470_clkdata->clkout_hw);
	if (ret)
		return ret;

	ret = devm_clk_hw_register_clkdev(&pdev->dev, &tps68470_clkdata->clkout_hw,
					  TPS68470_CLK_NAME, NULL);
	if (ret)
		return ret;

	if (pdata) {
		for (i = 0; i < pdata->n_consumers; i++) {
			consumer = &pdata->consumers[i];
			ret = devm_clk_hw_register_clkdev(&pdev->dev,
							  &tps68470_clkdata->clkout_hw,
							  consumer->consumer_con_id,
							  consumer->consumer_dev_name);
		}
	}

	return ret;
}

static struct platform_driver tps68470_clk_driver = {
	.driver = {
		.name = TPS68470_CLK_NAME,
	},
	.probe = tps68470_clk_probe,
};

 
static int __init tps68470_clk_init(void)
{
	return platform_driver_register(&tps68470_clk_driver);
}
subsys_initcall(tps68470_clk_init);

static void __exit tps68470_clk_exit(void)
{
	platform_driver_unregister(&tps68470_clk_driver);
}
module_exit(tps68470_clk_exit);

MODULE_ALIAS("platform:tps68470-clk");
MODULE_DESCRIPTION("clock driver for TPS68470 pmic");
MODULE_LICENSE("GPL");
