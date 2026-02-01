
 

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_opp.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/reset.h>

#include <soc/tegra/common.h>

#define PWM_ENABLE	(1 << 31)
#define PWM_DUTY_WIDTH	8
#define PWM_DUTY_SHIFT	16
#define PWM_SCALE_WIDTH	13
#define PWM_SCALE_SHIFT	0

struct tegra_pwm_soc {
	unsigned int num_channels;

	 
	unsigned long max_frequency;
};

struct tegra_pwm_chip {
	struct pwm_chip chip;
	struct device *dev;

	struct clk *clk;
	struct reset_control*rst;

	unsigned long clk_rate;
	unsigned long min_period_ns;

	void __iomem *regs;

	const struct tegra_pwm_soc *soc;
};

static inline struct tegra_pwm_chip *to_tegra_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct tegra_pwm_chip, chip);
}

static inline u32 pwm_readl(struct tegra_pwm_chip *pc, unsigned int offset)
{
	return readl(pc->regs + (offset << 4));
}

static inline void pwm_writel(struct tegra_pwm_chip *pc, unsigned int offset, u32 value)
{
	writel(value, pc->regs + (offset << 4));
}

static int tegra_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			    int duty_ns, int period_ns)
{
	struct tegra_pwm_chip *pc = to_tegra_pwm_chip(chip);
	unsigned long long c = duty_ns;
	unsigned long rate, required_clk_rate;
	u32 val = 0;
	int err;

	 
	c *= (1 << PWM_DUTY_WIDTH);
	c = DIV_ROUND_CLOSEST_ULL(c, period_ns);

	val = (u32)c << PWM_DUTY_SHIFT;

	 
	if (period_ns < pc->min_period_ns)
		return -EINVAL;

	 
	if (pc->soc->num_channels == 1) {
		 
		required_clk_rate = DIV_ROUND_UP_ULL((u64)NSEC_PER_SEC << PWM_DUTY_WIDTH,
						     period_ns);

		if (required_clk_rate > clk_round_rate(pc->clk, required_clk_rate))
			 
			required_clk_rate *= 2;

		err = dev_pm_opp_set_rate(pc->dev, required_clk_rate);
		if (err < 0)
			return -EINVAL;

		 
		pc->clk_rate = clk_get_rate(pc->clk);
	}

	 
	rate = mul_u64_u64_div_u64(pc->clk_rate, period_ns,
				   (u64)NSEC_PER_SEC << PWM_DUTY_WIDTH);

	 
	if (rate > 0)
		rate--;
	else
		return -EINVAL;

	 
	if (rate >> PWM_SCALE_WIDTH)
		return -EINVAL;

	val |= rate << PWM_SCALE_SHIFT;

	 
	if (!pwm_is_enabled(pwm)) {
		err = pm_runtime_resume_and_get(pc->dev);
		if (err)
			return err;
	} else
		val |= PWM_ENABLE;

	pwm_writel(pc, pwm->hwpwm, val);

	 
	if (!pwm_is_enabled(pwm))
		pm_runtime_put(pc->dev);

	return 0;
}

static int tegra_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct tegra_pwm_chip *pc = to_tegra_pwm_chip(chip);
	int rc = 0;
	u32 val;

	rc = pm_runtime_resume_and_get(pc->dev);
	if (rc)
		return rc;

	val = pwm_readl(pc, pwm->hwpwm);
	val |= PWM_ENABLE;
	pwm_writel(pc, pwm->hwpwm, val);

	return 0;
}

static void tegra_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct tegra_pwm_chip *pc = to_tegra_pwm_chip(chip);
	u32 val;

	val = pwm_readl(pc, pwm->hwpwm);
	val &= ~PWM_ENABLE;
	pwm_writel(pc, pwm->hwpwm, val);

	pm_runtime_put_sync(pc->dev);
}

static int tegra_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			   const struct pwm_state *state)
{
	int err;
	bool enabled = pwm->state.enabled;

	if (state->polarity != PWM_POLARITY_NORMAL)
		return -EINVAL;

	if (!state->enabled) {
		if (enabled)
			tegra_pwm_disable(chip, pwm);

		return 0;
	}

	err = tegra_pwm_config(pwm->chip, pwm, state->duty_cycle, state->period);
	if (err)
		return err;

	if (!enabled)
		err = tegra_pwm_enable(chip, pwm);

	return err;
}

static const struct pwm_ops tegra_pwm_ops = {
	.apply = tegra_pwm_apply,
	.owner = THIS_MODULE,
};

static int tegra_pwm_probe(struct platform_device *pdev)
{
	struct tegra_pwm_chip *pc;
	int ret;

	pc = devm_kzalloc(&pdev->dev, sizeof(*pc), GFP_KERNEL);
	if (!pc)
		return -ENOMEM;

	pc->soc = of_device_get_match_data(&pdev->dev);
	pc->dev = &pdev->dev;

	pc->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(pc->regs))
		return PTR_ERR(pc->regs);

	platform_set_drvdata(pdev, pc);

	pc->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(pc->clk))
		return PTR_ERR(pc->clk);

	ret = devm_tegra_core_dev_init_opp_table_common(&pdev->dev);
	if (ret)
		return ret;

	pm_runtime_enable(&pdev->dev);
	ret = pm_runtime_resume_and_get(&pdev->dev);
	if (ret)
		return ret;

	 
	ret = dev_pm_opp_set_rate(pc->dev, pc->soc->max_frequency);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to set max frequency: %d\n", ret);
		goto put_pm;
	}

	 
	pc->clk_rate = clk_get_rate(pc->clk);

	 
	pc->min_period_ns =
	    (NSEC_PER_SEC / (pc->soc->max_frequency >> PWM_DUTY_WIDTH)) + 1;

	pc->rst = devm_reset_control_get_exclusive(&pdev->dev, "pwm");
	if (IS_ERR(pc->rst)) {
		ret = PTR_ERR(pc->rst);
		dev_err(&pdev->dev, "Reset control is not found: %d\n", ret);
		goto put_pm;
	}

	reset_control_deassert(pc->rst);

	pc->chip.dev = &pdev->dev;
	pc->chip.ops = &tegra_pwm_ops;
	pc->chip.npwm = pc->soc->num_channels;

	ret = pwmchip_add(&pc->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		reset_control_assert(pc->rst);
		goto put_pm;
	}

	pm_runtime_put(&pdev->dev);

	return 0;
put_pm:
	pm_runtime_put_sync_suspend(&pdev->dev);
	pm_runtime_force_suspend(&pdev->dev);
	return ret;
}

static void tegra_pwm_remove(struct platform_device *pdev)
{
	struct tegra_pwm_chip *pc = platform_get_drvdata(pdev);

	pwmchip_remove(&pc->chip);

	reset_control_assert(pc->rst);

	pm_runtime_force_suspend(&pdev->dev);
}

static int __maybe_unused tegra_pwm_runtime_suspend(struct device *dev)
{
	struct tegra_pwm_chip *pc = dev_get_drvdata(dev);
	int err;

	clk_disable_unprepare(pc->clk);

	err = pinctrl_pm_select_sleep_state(dev);
	if (err) {
		clk_prepare_enable(pc->clk);
		return err;
	}

	return 0;
}

static int __maybe_unused tegra_pwm_runtime_resume(struct device *dev)
{
	struct tegra_pwm_chip *pc = dev_get_drvdata(dev);
	int err;

	err = pinctrl_pm_select_default_state(dev);
	if (err)
		return err;

	err = clk_prepare_enable(pc->clk);
	if (err) {
		pinctrl_pm_select_sleep_state(dev);
		return err;
	}

	return 0;
}

static const struct tegra_pwm_soc tegra20_pwm_soc = {
	.num_channels = 4,
	.max_frequency = 48000000UL,
};

static const struct tegra_pwm_soc tegra186_pwm_soc = {
	.num_channels = 1,
	.max_frequency = 102000000UL,
};

static const struct tegra_pwm_soc tegra194_pwm_soc = {
	.num_channels = 1,
	.max_frequency = 408000000UL,
};

static const struct of_device_id tegra_pwm_of_match[] = {
	{ .compatible = "nvidia,tegra20-pwm", .data = &tegra20_pwm_soc },
	{ .compatible = "nvidia,tegra186-pwm", .data = &tegra186_pwm_soc },
	{ .compatible = "nvidia,tegra194-pwm", .data = &tegra194_pwm_soc },
	{ }
};
MODULE_DEVICE_TABLE(of, tegra_pwm_of_match);

static const struct dev_pm_ops tegra_pwm_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra_pwm_runtime_suspend, tegra_pwm_runtime_resume,
			   NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
};

static struct platform_driver tegra_pwm_driver = {
	.driver = {
		.name = "tegra-pwm",
		.of_match_table = tegra_pwm_of_match,
		.pm = &tegra_pwm_pm_ops,
	},
	.probe = tegra_pwm_probe,
	.remove_new = tegra_pwm_remove,
};

module_platform_driver(tegra_pwm_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sandipan Patra <spatra@nvidia.com>");
MODULE_DESCRIPTION("Tegra PWM controller driver");
MODULE_ALIAS("platform:tegra-pwm");
