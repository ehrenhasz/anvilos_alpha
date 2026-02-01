


#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/types.h>

 

#define PWM_CONTROL_OFFSET			0x00000000
#define PWM_CONTROL_SMOOTH_SHIFT(chan)		(24 + (chan))
#define PWM_CONTROL_TYPE_SHIFT(chan)		(16 + (chan))
#define PWM_CONTROL_POLARITY_SHIFT(chan)	(8 + (chan))
#define PWM_CONTROL_TRIGGER_SHIFT(chan)		(chan)

#define PRESCALE_OFFSET				0x00000004
#define PRESCALE_SHIFT(chan)			((chan) << 2)
#define PRESCALE_MASK(chan)			(0x7 << PRESCALE_SHIFT(chan))
#define PRESCALE_MIN				0x00000000
#define PRESCALE_MAX				0x00000007

#define PERIOD_COUNT_OFFSET(chan)		(0x00000008 + ((chan) << 3))
#define PERIOD_COUNT_MIN			0x00000002
#define PERIOD_COUNT_MAX			0x00ffffff

#define DUTY_CYCLE_HIGH_OFFSET(chan)		(0x0000000c + ((chan) << 3))
#define DUTY_CYCLE_HIGH_MIN			0x00000000
#define DUTY_CYCLE_HIGH_MAX			0x00ffffff

struct kona_pwmc {
	struct pwm_chip chip;
	void __iomem *base;
	struct clk *clk;
};

static inline struct kona_pwmc *to_kona_pwmc(struct pwm_chip *chip)
{
	return container_of(chip, struct kona_pwmc, chip);
}

 
static void kona_pwmc_prepare_for_settings(struct kona_pwmc *kp,
	unsigned int chan)
{
	unsigned int value = readl(kp->base + PWM_CONTROL_OFFSET);

	value |= 1 << PWM_CONTROL_SMOOTH_SHIFT(chan);
	value &= ~(1 << PWM_CONTROL_TRIGGER_SHIFT(chan));
	writel(value, kp->base + PWM_CONTROL_OFFSET);

	 
	ndelay(400);
}

static void kona_pwmc_apply_settings(struct kona_pwmc *kp, unsigned int chan)
{
	unsigned int value = readl(kp->base + PWM_CONTROL_OFFSET);

	 
	value &= ~(1 << PWM_CONTROL_SMOOTH_SHIFT(chan));
	value |= 1 << PWM_CONTROL_TRIGGER_SHIFT(chan);
	writel(value, kp->base + PWM_CONTROL_OFFSET);

	 
	ndelay(400);
}

static int kona_pwmc_config(struct pwm_chip *chip, struct pwm_device *pwm,
			    u64 duty_ns, u64 period_ns)
{
	struct kona_pwmc *kp = to_kona_pwmc(chip);
	u64 div, rate;
	unsigned long prescale = PRESCALE_MIN, pc, dc;
	unsigned int value, chan = pwm->hwpwm;

	 

	rate = clk_get_rate(kp->clk);

	while (1) {
		div = 1000000000;
		div *= 1 + prescale;
		pc = mul_u64_u64_div_u64(rate, period_ns, div);
		dc = mul_u64_u64_div_u64(rate, duty_ns, div);

		 
		if (pc < PERIOD_COUNT_MIN)
			return -EINVAL;

		 
		if (pc <= PERIOD_COUNT_MAX && dc <= DUTY_CYCLE_HIGH_MAX)
			break;

		 
		if (++prescale > PRESCALE_MAX)
			return -EINVAL;
	}

	kona_pwmc_prepare_for_settings(kp, chan);

	value = readl(kp->base + PRESCALE_OFFSET);
	value &= ~PRESCALE_MASK(chan);
	value |= prescale << PRESCALE_SHIFT(chan);
	writel(value, kp->base + PRESCALE_OFFSET);

	writel(pc, kp->base + PERIOD_COUNT_OFFSET(chan));

	writel(dc, kp->base + DUTY_CYCLE_HIGH_OFFSET(chan));

	kona_pwmc_apply_settings(kp, chan);

	return 0;
}

static int kona_pwmc_set_polarity(struct pwm_chip *chip, struct pwm_device *pwm,
				  enum pwm_polarity polarity)
{
	struct kona_pwmc *kp = to_kona_pwmc(chip);
	unsigned int chan = pwm->hwpwm;
	unsigned int value;
	int ret;

	ret = clk_prepare_enable(kp->clk);
	if (ret < 0) {
		dev_err(chip->dev, "failed to enable clock: %d\n", ret);
		return ret;
	}

	kona_pwmc_prepare_for_settings(kp, chan);

	value = readl(kp->base + PWM_CONTROL_OFFSET);

	if (polarity == PWM_POLARITY_NORMAL)
		value |= 1 << PWM_CONTROL_POLARITY_SHIFT(chan);
	else
		value &= ~(1 << PWM_CONTROL_POLARITY_SHIFT(chan));

	writel(value, kp->base + PWM_CONTROL_OFFSET);

	kona_pwmc_apply_settings(kp, chan);

	clk_disable_unprepare(kp->clk);

	return 0;
}

static int kona_pwmc_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct kona_pwmc *kp = to_kona_pwmc(chip);
	int ret;

	ret = clk_prepare_enable(kp->clk);
	if (ret < 0) {
		dev_err(chip->dev, "failed to enable clock: %d\n", ret);
		return ret;
	}

	return 0;
}

static void kona_pwmc_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct kona_pwmc *kp = to_kona_pwmc(chip);
	unsigned int chan = pwm->hwpwm;
	unsigned int value;

	kona_pwmc_prepare_for_settings(kp, chan);

	 
	writel(0, kp->base + DUTY_CYCLE_HIGH_OFFSET(chan));
	writel(0, kp->base + PERIOD_COUNT_OFFSET(chan));

	 
	value = readl(kp->base + PRESCALE_OFFSET);
	value &= ~PRESCALE_MASK(chan);
	writel(value, kp->base + PRESCALE_OFFSET);

	kona_pwmc_apply_settings(kp, chan);

	clk_disable_unprepare(kp->clk);
}

static int kona_pwmc_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			   const struct pwm_state *state)
{
	int err;
	struct kona_pwmc *kp = to_kona_pwmc(chip);
	bool enabled = pwm->state.enabled;

	if (state->polarity != pwm->state.polarity) {
		if (enabled) {
			kona_pwmc_disable(chip, pwm);
			enabled = false;
		}

		err = kona_pwmc_set_polarity(chip, pwm, state->polarity);
		if (err)
			return err;

		pwm->state.polarity = state->polarity;
	}

	if (!state->enabled) {
		if (enabled)
			kona_pwmc_disable(chip, pwm);
		return 0;
	} else if (!enabled) {
		 
		err = kona_pwmc_enable(chip, pwm);
		if (err)
			return err;
	}

	err = kona_pwmc_config(pwm->chip, pwm, state->duty_cycle, state->period);
	if (err && !pwm->state.enabled)
		clk_disable_unprepare(kp->clk);

	return err;
}

static const struct pwm_ops kona_pwm_ops = {
	.apply = kona_pwmc_apply,
	.owner = THIS_MODULE,
};

static int kona_pwmc_probe(struct platform_device *pdev)
{
	struct kona_pwmc *kp;
	unsigned int chan;
	unsigned int value = 0;
	int ret = 0;

	kp = devm_kzalloc(&pdev->dev, sizeof(*kp), GFP_KERNEL);
	if (kp == NULL)
		return -ENOMEM;

	kp->chip.dev = &pdev->dev;
	kp->chip.ops = &kona_pwm_ops;
	kp->chip.npwm = 6;

	kp->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(kp->base))
		return PTR_ERR(kp->base);

	kp->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(kp->clk)) {
		dev_err(&pdev->dev, "failed to get clock: %ld\n",
			PTR_ERR(kp->clk));
		return PTR_ERR(kp->clk);
	}

	ret = clk_prepare_enable(kp->clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable clock: %d\n", ret);
		return ret;
	}

	 
	for (chan = 0; chan < kp->chip.npwm; chan++)
		value |= (1 << PWM_CONTROL_TYPE_SHIFT(chan));

	writel(value, kp->base + PWM_CONTROL_OFFSET);

	clk_disable_unprepare(kp->clk);

	ret = devm_pwmchip_add(&pdev->dev, &kp->chip);
	if (ret < 0)
		dev_err(&pdev->dev, "failed to add PWM chip: %d\n", ret);

	return ret;
}

static const struct of_device_id bcm_kona_pwmc_dt[] = {
	{ .compatible = "brcm,kona-pwm" },
	{ },
};
MODULE_DEVICE_TABLE(of, bcm_kona_pwmc_dt);

static struct platform_driver kona_pwmc_driver = {
	.driver = {
		.name = "bcm-kona-pwm",
		.of_match_table = bcm_kona_pwmc_dt,
	},
	.probe = kona_pwmc_probe,
};
module_platform_driver(kona_pwmc_driver);

MODULE_AUTHOR("Broadcom Corporation <bcm-kernel-feedback-list@broadcom.com>");
MODULE_AUTHOR("Tim Kryger <tkryger@broadcom.com>");
MODULE_DESCRIPTION("Broadcom Kona PWM driver");
MODULE_LICENSE("GPL v2");
