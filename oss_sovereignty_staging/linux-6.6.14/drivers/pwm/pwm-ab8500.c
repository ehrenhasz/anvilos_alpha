
 
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pwm.h>
#include <linux/mfd/abx500.h>
#include <linux/mfd/abx500/ab8500.h>
#include <linux/module.h>

 
#define AB8500_PWM_OUT_CTRL1_REG	0x60
#define AB8500_PWM_OUT_CTRL2_REG	0x61
#define AB8500_PWM_OUT_CTRL7_REG	0x66

#define AB8500_PWM_CLKRATE 9600000

struct ab8500_pwm_chip {
	struct pwm_chip chip;
	unsigned int hwid;
};

static struct ab8500_pwm_chip *ab8500_pwm_from_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct ab8500_pwm_chip, chip);
}

static int ab8500_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			    const struct pwm_state *state)
{
	int ret;
	u8 reg;
	u8 higher_val, lower_val;
	unsigned int duty_steps, div;
	struct ab8500_pwm_chip *ab8500 = ab8500_pwm_from_chip(chip);

	if (state->polarity != PWM_POLARITY_NORMAL)
		return -EINVAL;

	if (state->enabled) {
		 
		div = min_t(u64, mul_u64_u64_div_u64(state->period,
						     AB8500_PWM_CLKRATE >> 10,
						     NSEC_PER_SEC), 32);  
		if (div <= 16)
			 
			return -EINVAL;

		duty_steps = max_t(u64, mul_u64_u64_div_u64(state->duty_cycle,
							    AB8500_PWM_CLKRATE,
							    (u64)NSEC_PER_SEC * div), 1024);
	}

	 
	if (!state->enabled || duty_steps == 0) {
		ret = abx500_mask_and_set_register_interruptible(chip->dev,
					AB8500_MISC, AB8500_PWM_OUT_CTRL7_REG,
					1 << ab8500->hwid, 0);

		if (ret < 0)
			dev_err(chip->dev, "%s: Failed to disable PWM, Error %d\n",
								pwm->label, ret);
		return ret;
	}

	 
	lower_val = (duty_steps - 1) & 0x00ff;
	 
	higher_val = ((duty_steps - 1) & 0x0300) >> 8 | (32 - div) << 4;

	reg = AB8500_PWM_OUT_CTRL1_REG + (ab8500->hwid * 2);

	ret = abx500_set_register_interruptible(chip->dev, AB8500_MISC,
			reg, lower_val);
	if (ret < 0)
		return ret;

	ret = abx500_set_register_interruptible(chip->dev, AB8500_MISC,
			(reg + 1), higher_val);
	if (ret < 0)
		return ret;

	 
	ret = abx500_mask_and_set_register_interruptible(chip->dev,
				AB8500_MISC, AB8500_PWM_OUT_CTRL7_REG,
				1 << ab8500->hwid, 1 << ab8500->hwid);
	if (ret < 0)
		dev_err(chip->dev, "%s: Failed to enable PWM, Error %d\n",
							pwm->label, ret);

	return ret;
}

static int ab8500_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
				struct pwm_state *state)
{
	u8 ctrl7, lower_val, higher_val;
	int ret;
	struct ab8500_pwm_chip *ab8500 = ab8500_pwm_from_chip(chip);
	unsigned int div, duty_steps;

	ret = abx500_get_register_interruptible(chip->dev, AB8500_MISC,
						AB8500_PWM_OUT_CTRL7_REG,
						&ctrl7);
	if (ret)
		return ret;

	state->polarity = PWM_POLARITY_NORMAL;

	if (!(ctrl7 & 1 << ab8500->hwid)) {
		state->enabled = false;
		return 0;
	}

	ret = abx500_get_register_interruptible(chip->dev, AB8500_MISC,
						AB8500_PWM_OUT_CTRL1_REG + (ab8500->hwid * 2),
						&lower_val);
	if (ret)
		return ret;

	ret = abx500_get_register_interruptible(chip->dev, AB8500_MISC,
						AB8500_PWM_OUT_CTRL2_REG + (ab8500->hwid * 2),
						&higher_val);
	if (ret)
		return ret;

	div = 32 - ((higher_val & 0xf0) >> 4);
	duty_steps = ((higher_val & 3) << 8 | lower_val) + 1;

	state->period = DIV64_U64_ROUND_UP((u64)div << 10, AB8500_PWM_CLKRATE);
	state->duty_cycle = DIV64_U64_ROUND_UP((u64)div * duty_steps, AB8500_PWM_CLKRATE);

	return 0;
}

static const struct pwm_ops ab8500_pwm_ops = {
	.apply = ab8500_pwm_apply,
	.get_state = ab8500_pwm_get_state,
	.owner = THIS_MODULE,
};

static int ab8500_pwm_probe(struct platform_device *pdev)
{
	struct ab8500_pwm_chip *ab8500;
	int err;

	if (pdev->id < 1 || pdev->id > 31)
		return dev_err_probe(&pdev->dev, -EINVAL, "Invalid device id %d\n", pdev->id);

	 
	ab8500 = devm_kzalloc(&pdev->dev, sizeof(*ab8500), GFP_KERNEL);
	if (ab8500 == NULL)
		return -ENOMEM;

	ab8500->chip.dev = &pdev->dev;
	ab8500->chip.ops = &ab8500_pwm_ops;
	ab8500->chip.npwm = 1;
	ab8500->hwid = pdev->id - 1;

	err = devm_pwmchip_add(&pdev->dev, &ab8500->chip);
	if (err < 0)
		return dev_err_probe(&pdev->dev, err, "Failed to add pwm chip\n");

	dev_dbg(&pdev->dev, "pwm probe successful\n");

	return 0;
}

static struct platform_driver ab8500_pwm_driver = {
	.driver = {
		.name = "ab8500-pwm",
	},
	.probe = ab8500_pwm_probe,
};
module_platform_driver(ab8500_pwm_driver);

MODULE_AUTHOR("Arun MURTHY <arun.murthy@stericsson.com>");
MODULE_DESCRIPTION("AB8500 Pulse Width Modulation Driver");
MODULE_ALIAS("platform:ab8500-pwm");
MODULE_LICENSE("GPL v2");
