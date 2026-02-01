
 

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/math.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>

#define MCHPCOREPWM_PRESCALE_MAX	0xff
#define MCHPCOREPWM_PERIOD_STEPS_MAX	0xfe
#define MCHPCOREPWM_PERIOD_MAX		0xff00

#define MCHPCOREPWM_PRESCALE	0x00
#define MCHPCOREPWM_PERIOD	0x04
#define MCHPCOREPWM_EN(i)	(0x08 + 0x04 * (i))  
#define MCHPCOREPWM_POSEDGE(i)	(0x10 + 0x08 * (i))  
#define MCHPCOREPWM_NEGEDGE(i)	(0x14 + 0x08 * (i))  
#define MCHPCOREPWM_SYNC_UPD	0xe4
#define MCHPCOREPWM_TIMEOUT_MS	100u

struct mchp_core_pwm_chip {
	struct pwm_chip chip;
	struct clk *clk;
	void __iomem *base;
	struct mutex lock;  
	ktime_t update_timestamp;
	u32 sync_update_mask;
	u16 channel_enabled;
};

static inline struct mchp_core_pwm_chip *to_mchp_core_pwm(struct pwm_chip *chip)
{
	return container_of(chip, struct mchp_core_pwm_chip, chip);
}

static void mchp_core_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm,
				 bool enable, u64 period)
{
	struct mchp_core_pwm_chip *mchp_core_pwm = to_mchp_core_pwm(chip);
	u8 channel_enable, reg_offset, shift;

	 
	reg_offset = MCHPCOREPWM_EN(pwm->hwpwm >> 3);
	shift = pwm->hwpwm & 7;

	channel_enable = readb_relaxed(mchp_core_pwm->base + reg_offset);
	channel_enable &= ~(1 << shift);
	channel_enable |= (enable << shift);

	writel_relaxed(channel_enable, mchp_core_pwm->base + reg_offset);
	mchp_core_pwm->channel_enabled &= ~BIT(pwm->hwpwm);
	mchp_core_pwm->channel_enabled |= enable << pwm->hwpwm;

	 
	if (mchp_core_pwm->sync_update_mask & (1 << pwm->hwpwm))
		mchp_core_pwm->update_timestamp = ktime_add_ns(ktime_get(), period);
}

static void mchp_core_pwm_wait_for_sync_update(struct mchp_core_pwm_chip *mchp_core_pwm,
					       unsigned int channel)
{
	 

	if (mchp_core_pwm->sync_update_mask & (1 << channel)) {
		ktime_t current_time = ktime_get();
		s64 remaining_ns;
		u32 delay_us;

		remaining_ns = ktime_to_ns(ktime_sub(mchp_core_pwm->update_timestamp,
						     current_time));

		 
		if (remaining_ns <= 0)
			return;

		delay_us = DIV_ROUND_UP_ULL(remaining_ns, NSEC_PER_USEC);
		fsleep(delay_us);
	}
}

static u64 mchp_core_pwm_calc_duty(const struct pwm_state *state, u64 clk_rate,
				   u8 prescale, u8 period_steps)
{
	u64 duty_steps, tmp;

	 
	tmp = (((u64)prescale) + 1) * NSEC_PER_SEC;
	duty_steps = mul_u64_u64_div_u64(state->duty_cycle, clk_rate, tmp);

	return duty_steps;
}

static void mchp_core_pwm_apply_duty(struct pwm_chip *chip, struct pwm_device *pwm,
				     const struct pwm_state *state, u64 duty_steps,
				     u16 period_steps)
{
	struct mchp_core_pwm_chip *mchp_core_pwm = to_mchp_core_pwm(chip);
	u8 posedge, negedge;
	u8 first_edge = 0, second_edge = duty_steps;

	 
	if (duty_steps == 0)
		first_edge = period_steps + 1;

	if (state->polarity == PWM_POLARITY_INVERSED) {
		negedge = first_edge;
		posedge = second_edge;
	} else {
		posedge = first_edge;
		negedge = second_edge;
	}

	 
	writel_relaxed(posedge, mchp_core_pwm->base + MCHPCOREPWM_POSEDGE(pwm->hwpwm));
	writel_relaxed(negedge, mchp_core_pwm->base + MCHPCOREPWM_NEGEDGE(pwm->hwpwm));
}

static int mchp_core_pwm_calc_period(const struct pwm_state *state, unsigned long clk_rate,
				     u16 *prescale, u16 *period_steps)
{
	u64 tmp;

	 
	tmp = mul_u64_u64_div_u64(state->period, clk_rate, NSEC_PER_SEC);
	if (tmp >= MCHPCOREPWM_PERIOD_MAX) {
		*prescale = MCHPCOREPWM_PRESCALE_MAX;
		*period_steps = MCHPCOREPWM_PERIOD_STEPS_MAX;

		return 0;
	}

	 
	if (tmp < MCHPCOREPWM_PERIOD_STEPS_MAX + 1)
		return -EINVAL;

	 
	*prescale = ((u16)tmp) / (MCHPCOREPWM_PERIOD_STEPS_MAX + 1) - 1;

	 
	*period_steps = MCHPCOREPWM_PERIOD_STEPS_MAX;

	return 0;
}

static int mchp_core_pwm_apply_locked(struct pwm_chip *chip, struct pwm_device *pwm,
				      const struct pwm_state *state)
{
	struct mchp_core_pwm_chip *mchp_core_pwm = to_mchp_core_pwm(chip);
	bool period_locked;
	unsigned long clk_rate;
	u64 duty_steps;
	u16 prescale, period_steps;
	int ret;

	if (!state->enabled) {
		mchp_core_pwm_enable(chip, pwm, false, pwm->state.period);
		return 0;
	}

	 
	clk_rate = clk_get_rate(mchp_core_pwm->clk);
	if (clk_rate >= NSEC_PER_SEC)
		return -EINVAL;

	ret = mchp_core_pwm_calc_period(state, clk_rate, &prescale, &period_steps);
	if (ret)
		return ret;

	 
	period_locked = mchp_core_pwm->channel_enabled & ~(1 << pwm->hwpwm);

	if (period_locked) {
		u16 hw_prescale;
		u16 hw_period_steps;

		hw_prescale = readb_relaxed(mchp_core_pwm->base + MCHPCOREPWM_PRESCALE);
		hw_period_steps = readb_relaxed(mchp_core_pwm->base + MCHPCOREPWM_PERIOD);

		if ((period_steps + 1) * (prescale + 1) <
		    (hw_period_steps + 1) * (hw_prescale + 1))
			return -EINVAL;

		 
		if (hw_period_steps == MCHPCOREPWM_PERIOD_STEPS_MAX)
			return -EINVAL;

		prescale = hw_prescale;
		period_steps = hw_period_steps;
	}

	duty_steps = mchp_core_pwm_calc_duty(state, clk_rate, prescale, period_steps);

	 
	if (duty_steps > period_steps)
		duty_steps = period_steps + 1;

	if (!period_locked) {
		writel_relaxed(prescale, mchp_core_pwm->base + MCHPCOREPWM_PRESCALE);
		writel_relaxed(period_steps, mchp_core_pwm->base + MCHPCOREPWM_PERIOD);
	}

	mchp_core_pwm_apply_duty(chip, pwm, state, duty_steps, period_steps);

	mchp_core_pwm_enable(chip, pwm, true, pwm->state.period);

	return 0;
}

static int mchp_core_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			       const struct pwm_state *state)
{
	struct mchp_core_pwm_chip *mchp_core_pwm = to_mchp_core_pwm(chip);
	int ret;

	mutex_lock(&mchp_core_pwm->lock);

	mchp_core_pwm_wait_for_sync_update(mchp_core_pwm, pwm->hwpwm);

	ret = mchp_core_pwm_apply_locked(chip, pwm, state);

	mutex_unlock(&mchp_core_pwm->lock);

	return ret;
}

static int mchp_core_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
				   struct pwm_state *state)
{
	struct mchp_core_pwm_chip *mchp_core_pwm = to_mchp_core_pwm(chip);
	u64 rate;
	u16 prescale, period_steps;
	u8 duty_steps, posedge, negedge;

	mutex_lock(&mchp_core_pwm->lock);

	mchp_core_pwm_wait_for_sync_update(mchp_core_pwm, pwm->hwpwm);

	if (mchp_core_pwm->channel_enabled & (1 << pwm->hwpwm))
		state->enabled = true;
	else
		state->enabled = false;

	rate = clk_get_rate(mchp_core_pwm->clk);

	 
	prescale = readb_relaxed(mchp_core_pwm->base + MCHPCOREPWM_PRESCALE);
	period_steps = readb_relaxed(mchp_core_pwm->base + MCHPCOREPWM_PERIOD);

	state->period = (period_steps + 1) * (prescale + 1);
	state->period *= NSEC_PER_SEC;
	state->period = DIV64_U64_ROUND_UP(state->period, rate);

	posedge = readb_relaxed(mchp_core_pwm->base + MCHPCOREPWM_POSEDGE(pwm->hwpwm));
	negedge = readb_relaxed(mchp_core_pwm->base + MCHPCOREPWM_NEGEDGE(pwm->hwpwm));

	mutex_unlock(&mchp_core_pwm->lock);

	if (negedge == posedge) {
		state->duty_cycle = state->period;
		state->period *= 2;
	} else {
		duty_steps = abs((s16)posedge - (s16)negedge);
		state->duty_cycle = duty_steps * (prescale + 1) * NSEC_PER_SEC;
		state->duty_cycle = DIV64_U64_ROUND_UP(state->duty_cycle, rate);
	}

	state->polarity = negedge < posedge ? PWM_POLARITY_INVERSED : PWM_POLARITY_NORMAL;

	return 0;
}

static const struct pwm_ops mchp_core_pwm_ops = {
	.apply = mchp_core_pwm_apply,
	.get_state = mchp_core_pwm_get_state,
	.owner = THIS_MODULE,
};

static const struct of_device_id mchp_core_of_match[] = {
	{
		.compatible = "microchip,corepwm-rtl-v4",
	},
	{   }
};
MODULE_DEVICE_TABLE(of, mchp_core_of_match);

static int mchp_core_pwm_probe(struct platform_device *pdev)
{
	struct mchp_core_pwm_chip *mchp_core_pwm;
	struct resource *regs;
	int ret;

	mchp_core_pwm = devm_kzalloc(&pdev->dev, sizeof(*mchp_core_pwm), GFP_KERNEL);
	if (!mchp_core_pwm)
		return -ENOMEM;

	mchp_core_pwm->base = devm_platform_get_and_ioremap_resource(pdev, 0, &regs);
	if (IS_ERR(mchp_core_pwm->base))
		return PTR_ERR(mchp_core_pwm->base);

	mchp_core_pwm->clk = devm_clk_get_enabled(&pdev->dev, NULL);
	if (IS_ERR(mchp_core_pwm->clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(mchp_core_pwm->clk),
				     "failed to get PWM clock\n");

	if (of_property_read_u32(pdev->dev.of_node, "microchip,sync-update-mask",
				 &mchp_core_pwm->sync_update_mask))
		mchp_core_pwm->sync_update_mask = 0;

	mutex_init(&mchp_core_pwm->lock);

	mchp_core_pwm->chip.dev = &pdev->dev;
	mchp_core_pwm->chip.ops = &mchp_core_pwm_ops;
	mchp_core_pwm->chip.npwm = 16;

	mchp_core_pwm->channel_enabled = readb_relaxed(mchp_core_pwm->base + MCHPCOREPWM_EN(0));
	mchp_core_pwm->channel_enabled |=
		readb_relaxed(mchp_core_pwm->base + MCHPCOREPWM_EN(1)) << 8;

	 
	writel_relaxed(1U, mchp_core_pwm->base + MCHPCOREPWM_SYNC_UPD);
	mchp_core_pwm->update_timestamp = ktime_get();

	ret = devm_pwmchip_add(&pdev->dev, &mchp_core_pwm->chip);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "Failed to add pwmchip\n");

	return 0;
}

static struct platform_driver mchp_core_pwm_driver = {
	.driver = {
		.name = "mchp-core-pwm",
		.of_match_table = mchp_core_of_match,
	},
	.probe = mchp_core_pwm_probe,
};
module_platform_driver(mchp_core_pwm_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Conor Dooley <conor.dooley@microchip.com>");
MODULE_DESCRIPTION("corePWM driver for Microchip FPGAs");
