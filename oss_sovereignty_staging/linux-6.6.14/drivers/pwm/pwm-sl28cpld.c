
 

#include <linux/bitfield.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/pwm.h>
#include <linux/regmap.h>

 
#define SL28CPLD_PWM_CTRL			0x00
#define   SL28CPLD_PWM_CTRL_ENABLE		BIT(7)
#define   SL28CPLD_PWM_CTRL_PRESCALER_MASK	GENMASK(1, 0)
#define SL28CPLD_PWM_CYCLE			0x01
#define   SL28CPLD_PWM_CYCLE_MAX		GENMASK(6, 0)

#define SL28CPLD_PWM_CLK			32000  
#define SL28CPLD_PWM_MAX_DUTY_CYCLE(prescaler)	(1 << (7 - (prescaler)))
#define SL28CPLD_PWM_PERIOD(prescaler) \
	(NSEC_PER_SEC / SL28CPLD_PWM_CLK * SL28CPLD_PWM_MAX_DUTY_CYCLE(prescaler))

 
#define SL28CPLD_PWM_TO_DUTY_CYCLE(reg) \
	(NSEC_PER_SEC / SL28CPLD_PWM_CLK * (reg))
#define SL28CPLD_PWM_FROM_DUTY_CYCLE(duty_cycle) \
	(DIV_ROUND_DOWN_ULL((duty_cycle), NSEC_PER_SEC / SL28CPLD_PWM_CLK))

#define sl28cpld_pwm_read(priv, reg, val) \
	regmap_read((priv)->regmap, (priv)->offset + (reg), (val))
#define sl28cpld_pwm_write(priv, reg, val) \
	regmap_write((priv)->regmap, (priv)->offset + (reg), (val))

struct sl28cpld_pwm {
	struct pwm_chip chip;
	struct regmap *regmap;
	u32 offset;
};

static inline struct sl28cpld_pwm *sl28cpld_pwm_from_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct sl28cpld_pwm, chip);
}

static int sl28cpld_pwm_get_state(struct pwm_chip *chip,
				  struct pwm_device *pwm,
				  struct pwm_state *state)
{
	struct sl28cpld_pwm *priv = sl28cpld_pwm_from_chip(chip);
	unsigned int reg;
	int prescaler;

	sl28cpld_pwm_read(priv, SL28CPLD_PWM_CTRL, &reg);

	state->enabled = reg & SL28CPLD_PWM_CTRL_ENABLE;

	prescaler = FIELD_GET(SL28CPLD_PWM_CTRL_PRESCALER_MASK, reg);
	state->period = SL28CPLD_PWM_PERIOD(prescaler);

	sl28cpld_pwm_read(priv, SL28CPLD_PWM_CYCLE, &reg);
	state->duty_cycle = SL28CPLD_PWM_TO_DUTY_CYCLE(reg);
	state->polarity = PWM_POLARITY_NORMAL;

	 
	state->duty_cycle = min(state->duty_cycle, state->period);

	return 0;
}

static int sl28cpld_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			      const struct pwm_state *state)
{
	struct sl28cpld_pwm *priv = sl28cpld_pwm_from_chip(chip);
	unsigned int cycle, prescaler;
	bool write_duty_cycle_first;
	int ret;
	u8 ctrl;

	 
	if (state->polarity != PWM_POLARITY_NORMAL)
		return -EINVAL;

	 
	prescaler = DIV_ROUND_UP_ULL(SL28CPLD_PWM_PERIOD(0), state->period);
	prescaler = order_base_2(prescaler);

	if (prescaler > field_max(SL28CPLD_PWM_CTRL_PRESCALER_MASK))
		return -ERANGE;

	ctrl = FIELD_PREP(SL28CPLD_PWM_CTRL_PRESCALER_MASK, prescaler);
	if (state->enabled)
		ctrl |= SL28CPLD_PWM_CTRL_ENABLE;

	cycle = SL28CPLD_PWM_FROM_DUTY_CYCLE(state->duty_cycle);
	cycle = min_t(unsigned int, cycle, SL28CPLD_PWM_MAX_DUTY_CYCLE(prescaler));

	 
	if (cycle == SL28CPLD_PWM_MAX_DUTY_CYCLE(0)) {
		ctrl &= ~SL28CPLD_PWM_CTRL_PRESCALER_MASK;
		ctrl |= FIELD_PREP(SL28CPLD_PWM_CTRL_PRESCALER_MASK, 1);
		cycle = SL28CPLD_PWM_MAX_DUTY_CYCLE(1);
	}

	 
	write_duty_cycle_first = pwm->state.period > state->period;

	if (write_duty_cycle_first) {
		ret = sl28cpld_pwm_write(priv, SL28CPLD_PWM_CYCLE, cycle);
		if (ret)
			return ret;
	}

	ret = sl28cpld_pwm_write(priv, SL28CPLD_PWM_CTRL, ctrl);
	if (ret)
		return ret;

	if (!write_duty_cycle_first) {
		ret = sl28cpld_pwm_write(priv, SL28CPLD_PWM_CYCLE, cycle);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct pwm_ops sl28cpld_pwm_ops = {
	.apply = sl28cpld_pwm_apply,
	.get_state = sl28cpld_pwm_get_state,
	.owner = THIS_MODULE,
};

static int sl28cpld_pwm_probe(struct platform_device *pdev)
{
	struct sl28cpld_pwm *priv;
	struct pwm_chip *chip;
	int ret;

	if (!pdev->dev.parent) {
		dev_err(&pdev->dev, "no parent device\n");
		return -ENODEV;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!priv->regmap) {
		dev_err(&pdev->dev, "could not get parent regmap\n");
		return -ENODEV;
	}

	ret = device_property_read_u32(&pdev->dev, "reg", &priv->offset);
	if (ret) {
		dev_err(&pdev->dev, "no 'reg' property found (%pe)\n",
			ERR_PTR(ret));
		return -EINVAL;
	}

	 
	chip = &priv->chip;
	chip->dev = &pdev->dev;
	chip->ops = &sl28cpld_pwm_ops;
	chip->npwm = 1;

	ret = devm_pwmchip_add(&pdev->dev, chip);
	if (ret) {
		dev_err(&pdev->dev, "failed to add PWM chip (%pe)",
			ERR_PTR(ret));
		return ret;
	}

	return 0;
}

static const struct of_device_id sl28cpld_pwm_of_match[] = {
	{ .compatible = "kontron,sl28cpld-pwm" },
	{}
};
MODULE_DEVICE_TABLE(of, sl28cpld_pwm_of_match);

static struct platform_driver sl28cpld_pwm_driver = {
	.probe = sl28cpld_pwm_probe,
	.driver = {
		.name = "sl28cpld-pwm",
		.of_match_table = sl28cpld_pwm_of_match,
	},
};
module_platform_driver(sl28cpld_pwm_driver);

MODULE_DESCRIPTION("sl28cpld PWM Driver");
MODULE_AUTHOR("Michael Walle <michael@walle.cc>");
MODULE_LICENSE("GPL");
