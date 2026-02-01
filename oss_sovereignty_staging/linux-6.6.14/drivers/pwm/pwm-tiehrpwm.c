
 

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>

 

 
#define TBCTL			0x00
#define TBPRD			0x0A

#define TBCTL_PRDLD_MASK	BIT(3)
#define TBCTL_PRDLD_SHDW	0
#define TBCTL_PRDLD_IMDT	BIT(3)
#define TBCTL_CLKDIV_MASK	(BIT(12) | BIT(11) | BIT(10) | BIT(9) | \
				BIT(8) | BIT(7))
#define TBCTL_CTRMODE_MASK	(BIT(1) | BIT(0))
#define TBCTL_CTRMODE_UP	0
#define TBCTL_CTRMODE_DOWN	BIT(0)
#define TBCTL_CTRMODE_UPDOWN	BIT(1)
#define TBCTL_CTRMODE_FREEZE	(BIT(1) | BIT(0))

#define TBCTL_HSPCLKDIV_SHIFT	7
#define TBCTL_CLKDIV_SHIFT	10

#define CLKDIV_MAX		7
#define HSPCLKDIV_MAX		7
#define PERIOD_MAX		0xFFFF

 
#define CMPA			0x12
#define CMPB			0x14

 
#define AQCTLA			0x16
#define AQCTLB			0x18
#define AQSFRC			0x1A
#define AQCSFRC			0x1C

#define AQCTL_CBU_MASK		(BIT(9) | BIT(8))
#define AQCTL_CBU_FRCLOW	BIT(8)
#define AQCTL_CBU_FRCHIGH	BIT(9)
#define AQCTL_CBU_FRCTOGGLE	(BIT(9) | BIT(8))
#define AQCTL_CAU_MASK		(BIT(5) | BIT(4))
#define AQCTL_CAU_FRCLOW	BIT(4)
#define AQCTL_CAU_FRCHIGH	BIT(5)
#define AQCTL_CAU_FRCTOGGLE	(BIT(5) | BIT(4))
#define AQCTL_PRD_MASK		(BIT(3) | BIT(2))
#define AQCTL_PRD_FRCLOW	BIT(2)
#define AQCTL_PRD_FRCHIGH	BIT(3)
#define AQCTL_PRD_FRCTOGGLE	(BIT(3) | BIT(2))
#define AQCTL_ZRO_MASK		(BIT(1) | BIT(0))
#define AQCTL_ZRO_FRCLOW	BIT(0)
#define AQCTL_ZRO_FRCHIGH	BIT(1)
#define AQCTL_ZRO_FRCTOGGLE	(BIT(1) | BIT(0))

#define AQCTL_CHANA_POLNORMAL	(AQCTL_CAU_FRCLOW | AQCTL_PRD_FRCHIGH | \
				AQCTL_ZRO_FRCHIGH)
#define AQCTL_CHANA_POLINVERSED	(AQCTL_CAU_FRCHIGH | AQCTL_PRD_FRCLOW | \
				AQCTL_ZRO_FRCLOW)
#define AQCTL_CHANB_POLNORMAL	(AQCTL_CBU_FRCLOW | AQCTL_PRD_FRCHIGH | \
				AQCTL_ZRO_FRCHIGH)
#define AQCTL_CHANB_POLINVERSED	(AQCTL_CBU_FRCHIGH | AQCTL_PRD_FRCLOW | \
				AQCTL_ZRO_FRCLOW)

#define AQSFRC_RLDCSF_MASK	(BIT(7) | BIT(6))
#define AQSFRC_RLDCSF_ZRO	0
#define AQSFRC_RLDCSF_PRD	BIT(6)
#define AQSFRC_RLDCSF_ZROPRD	BIT(7)
#define AQSFRC_RLDCSF_IMDT	(BIT(7) | BIT(6))

#define AQCSFRC_CSFB_MASK	(BIT(3) | BIT(2))
#define AQCSFRC_CSFB_FRCDIS	0
#define AQCSFRC_CSFB_FRCLOW	BIT(2)
#define AQCSFRC_CSFB_FRCHIGH	BIT(3)
#define AQCSFRC_CSFB_DISSWFRC	(BIT(3) | BIT(2))
#define AQCSFRC_CSFA_MASK	(BIT(1) | BIT(0))
#define AQCSFRC_CSFA_FRCDIS	0
#define AQCSFRC_CSFA_FRCLOW	BIT(0)
#define AQCSFRC_CSFA_FRCHIGH	BIT(1)
#define AQCSFRC_CSFA_DISSWFRC	(BIT(1) | BIT(0))

#define NUM_PWM_CHANNEL		2	 

struct ehrpwm_context {
	u16 tbctl;
	u16 tbprd;
	u16 cmpa;
	u16 cmpb;
	u16 aqctla;
	u16 aqctlb;
	u16 aqsfrc;
	u16 aqcsfrc;
};

struct ehrpwm_pwm_chip {
	struct pwm_chip chip;
	unsigned long clk_rate;
	void __iomem *mmio_base;
	unsigned long period_cycles[NUM_PWM_CHANNEL];
	enum pwm_polarity polarity[NUM_PWM_CHANNEL];
	struct clk *tbclk;
	struct ehrpwm_context ctx;
};

static inline struct ehrpwm_pwm_chip *to_ehrpwm_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct ehrpwm_pwm_chip, chip);
}

static inline u16 ehrpwm_read(void __iomem *base, unsigned int offset)
{
	return readw(base + offset);
}

static inline void ehrpwm_write(void __iomem *base, unsigned int offset,
				u16 value)
{
	writew(value, base + offset);
}

static void ehrpwm_modify(void __iomem *base, unsigned int offset, u16 mask,
			  u16 value)
{
	unsigned short val;

	val = readw(base + offset);
	val &= ~mask;
	val |= value & mask;
	writew(val, base + offset);
}

 
static int set_prescale_div(unsigned long rqst_prescaler, u16 *prescale_div,
			    u16 *tb_clk_div)
{
	unsigned int clkdiv, hspclkdiv;

	for (clkdiv = 0; clkdiv <= CLKDIV_MAX; clkdiv++) {
		for (hspclkdiv = 0; hspclkdiv <= HSPCLKDIV_MAX; hspclkdiv++) {
			 

			*prescale_div = (1 << clkdiv) *
					(hspclkdiv ? (hspclkdiv * 2) : 1);
			if (*prescale_div > rqst_prescaler) {
				*tb_clk_div = (clkdiv << TBCTL_CLKDIV_SHIFT) |
					(hspclkdiv << TBCTL_HSPCLKDIV_SHIFT);
				return 0;
			}
		}
	}

	return 1;
}

static void configure_polarity(struct ehrpwm_pwm_chip *pc, int chan)
{
	u16 aqctl_val, aqctl_mask;
	unsigned int aqctl_reg;

	 
	if (chan == 1) {
		aqctl_reg = AQCTLB;
		aqctl_mask = AQCTL_CBU_MASK;

		if (pc->polarity[chan] == PWM_POLARITY_INVERSED)
			aqctl_val = AQCTL_CHANB_POLINVERSED;
		else
			aqctl_val = AQCTL_CHANB_POLNORMAL;
	} else {
		aqctl_reg = AQCTLA;
		aqctl_mask = AQCTL_CAU_MASK;

		if (pc->polarity[chan] == PWM_POLARITY_INVERSED)
			aqctl_val = AQCTL_CHANA_POLINVERSED;
		else
			aqctl_val = AQCTL_CHANA_POLNORMAL;
	}

	aqctl_mask |= AQCTL_PRD_MASK | AQCTL_ZRO_MASK;
	ehrpwm_modify(pc->mmio_base, aqctl_reg, aqctl_mask, aqctl_val);
}

 
static int ehrpwm_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			     u64 duty_ns, u64 period_ns)
{
	struct ehrpwm_pwm_chip *pc = to_ehrpwm_pwm_chip(chip);
	u32 period_cycles, duty_cycles;
	u16 ps_divval, tb_divval;
	unsigned int i, cmp_reg;
	unsigned long long c;

	if (period_ns > NSEC_PER_SEC)
		return -ERANGE;

	c = pc->clk_rate;
	c = c * period_ns;
	do_div(c, NSEC_PER_SEC);
	period_cycles = (unsigned long)c;

	if (period_cycles < 1) {
		period_cycles = 1;
		duty_cycles = 1;
	} else {
		c = pc->clk_rate;
		c = c * duty_ns;
		do_div(c, NSEC_PER_SEC);
		duty_cycles = (unsigned long)c;
	}

	 
	for (i = 0; i < NUM_PWM_CHANNEL; i++) {
		if (pc->period_cycles[i] &&
				(pc->period_cycles[i] != period_cycles)) {
			 
			if (i == pwm->hwpwm)
				continue;

			dev_err(chip->dev,
				"period value conflicts with channel %u\n",
				i);
			return -EINVAL;
		}
	}

	pc->period_cycles[pwm->hwpwm] = period_cycles;

	 
	if (set_prescale_div(period_cycles/PERIOD_MAX, &ps_divval,
			     &tb_divval)) {
		dev_err(chip->dev, "Unsupported values\n");
		return -EINVAL;
	}

	pm_runtime_get_sync(chip->dev);

	 
	ehrpwm_modify(pc->mmio_base, TBCTL, TBCTL_CLKDIV_MASK, tb_divval);

	 
	period_cycles = period_cycles / ps_divval;
	duty_cycles = duty_cycles / ps_divval;

	 
	ehrpwm_modify(pc->mmio_base, TBCTL, TBCTL_PRDLD_MASK, TBCTL_PRDLD_SHDW);

	ehrpwm_write(pc->mmio_base, TBPRD, period_cycles);

	 
	ehrpwm_modify(pc->mmio_base, TBCTL, TBCTL_CTRMODE_MASK,
		      TBCTL_CTRMODE_UP);

	if (pwm->hwpwm == 1)
		 
		cmp_reg = CMPB;
	else
		 
		cmp_reg = CMPA;

	ehrpwm_write(pc->mmio_base, cmp_reg, duty_cycles);

	pm_runtime_put_sync(chip->dev);

	return 0;
}

static int ehrpwm_pwm_set_polarity(struct pwm_chip *chip,
				   struct pwm_device *pwm,
				   enum pwm_polarity polarity)
{
	struct ehrpwm_pwm_chip *pc = to_ehrpwm_pwm_chip(chip);

	 
	pc->polarity[pwm->hwpwm] = polarity;

	return 0;
}

static int ehrpwm_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct ehrpwm_pwm_chip *pc = to_ehrpwm_pwm_chip(chip);
	u16 aqcsfrc_val, aqcsfrc_mask;
	int ret;

	 
	pm_runtime_get_sync(chip->dev);

	 
	if (pwm->hwpwm) {
		aqcsfrc_val = AQCSFRC_CSFB_FRCDIS;
		aqcsfrc_mask = AQCSFRC_CSFB_MASK;
	} else {
		aqcsfrc_val = AQCSFRC_CSFA_FRCDIS;
		aqcsfrc_mask = AQCSFRC_CSFA_MASK;
	}

	 
	ehrpwm_modify(pc->mmio_base, AQSFRC, AQSFRC_RLDCSF_MASK,
		      AQSFRC_RLDCSF_ZRO);

	ehrpwm_modify(pc->mmio_base, AQCSFRC, aqcsfrc_mask, aqcsfrc_val);

	 
	configure_polarity(pc, pwm->hwpwm);

	 
	ret = clk_enable(pc->tbclk);
	if (ret) {
		dev_err(chip->dev, "Failed to enable TBCLK for %s: %d\n",
			dev_name(pc->chip.dev), ret);
		return ret;
	}

	return 0;
}

static void ehrpwm_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct ehrpwm_pwm_chip *pc = to_ehrpwm_pwm_chip(chip);
	u16 aqcsfrc_val, aqcsfrc_mask;

	 
	if (pwm->hwpwm) {
		aqcsfrc_val = AQCSFRC_CSFB_FRCLOW;
		aqcsfrc_mask = AQCSFRC_CSFB_MASK;
	} else {
		aqcsfrc_val = AQCSFRC_CSFA_FRCLOW;
		aqcsfrc_mask = AQCSFRC_CSFA_MASK;
	}

	 
	ehrpwm_modify(pc->mmio_base, AQSFRC, AQSFRC_RLDCSF_MASK,
		      AQSFRC_RLDCSF_ZRO);
	ehrpwm_modify(pc->mmio_base, AQCSFRC, aqcsfrc_mask, aqcsfrc_val);
	 
	ehrpwm_modify(pc->mmio_base, AQSFRC, AQSFRC_RLDCSF_MASK,
		      AQSFRC_RLDCSF_IMDT);

	ehrpwm_modify(pc->mmio_base, AQCSFRC, aqcsfrc_mask, aqcsfrc_val);

	 
	clk_disable(pc->tbclk);

	 
	pm_runtime_put_sync(chip->dev);
}

static void ehrpwm_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct ehrpwm_pwm_chip *pc = to_ehrpwm_pwm_chip(chip);

	if (pwm_is_enabled(pwm)) {
		dev_warn(chip->dev, "Removing PWM device without disabling\n");
		pm_runtime_put_sync(chip->dev);
	}

	 
	pc->period_cycles[pwm->hwpwm] = 0;
}

static int ehrpwm_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			    const struct pwm_state *state)
{
	int err;
	bool enabled = pwm->state.enabled;

	if (state->polarity != pwm->state.polarity) {
		if (enabled) {
			ehrpwm_pwm_disable(chip, pwm);
			enabled = false;
		}

		err = ehrpwm_pwm_set_polarity(chip, pwm, state->polarity);
		if (err)
			return err;
	}

	if (!state->enabled) {
		if (enabled)
			ehrpwm_pwm_disable(chip, pwm);
		return 0;
	}

	err = ehrpwm_pwm_config(chip, pwm, state->duty_cycle, state->period);
	if (err)
		return err;

	if (!enabled)
		err = ehrpwm_pwm_enable(chip, pwm);

	return err;
}

static const struct pwm_ops ehrpwm_pwm_ops = {
	.free = ehrpwm_pwm_free,
	.apply = ehrpwm_pwm_apply,
	.owner = THIS_MODULE,
};

static const struct of_device_id ehrpwm_of_match[] = {
	{ .compatible = "ti,am3352-ehrpwm" },
	{ .compatible = "ti,am33xx-ehrpwm" },
	{},
};
MODULE_DEVICE_TABLE(of, ehrpwm_of_match);

static int ehrpwm_pwm_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct ehrpwm_pwm_chip *pc;
	struct clk *clk;
	int ret;

	pc = devm_kzalloc(&pdev->dev, sizeof(*pc), GFP_KERNEL);
	if (!pc)
		return -ENOMEM;

	clk = devm_clk_get(&pdev->dev, "fck");
	if (IS_ERR(clk)) {
		if (of_device_is_compatible(np, "ti,am33xx-ecap")) {
			dev_warn(&pdev->dev, "Binding is obsolete.\n");
			clk = devm_clk_get(pdev->dev.parent, "fck");
		}
	}

	if (IS_ERR(clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(clk), "Failed to get fck\n");

	pc->clk_rate = clk_get_rate(clk);
	if (!pc->clk_rate) {
		dev_err(&pdev->dev, "failed to get clock rate\n");
		return -EINVAL;
	}

	pc->chip.dev = &pdev->dev;
	pc->chip.ops = &ehrpwm_pwm_ops;
	pc->chip.npwm = NUM_PWM_CHANNEL;

	pc->mmio_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(pc->mmio_base))
		return PTR_ERR(pc->mmio_base);

	 
	pc->tbclk = devm_clk_get(&pdev->dev, "tbclk");
	if (IS_ERR(pc->tbclk))
		return dev_err_probe(&pdev->dev, PTR_ERR(pc->tbclk), "Failed to get tbclk\n");

	ret = clk_prepare(pc->tbclk);
	if (ret < 0) {
		dev_err(&pdev->dev, "clk_prepare() failed: %d\n", ret);
		return ret;
	}

	ret = pwmchip_add(&pc->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		goto err_clk_unprepare;
	}

	platform_set_drvdata(pdev, pc);
	pm_runtime_enable(&pdev->dev);

	return 0;

err_clk_unprepare:
	clk_unprepare(pc->tbclk);

	return ret;
}

static void ehrpwm_pwm_remove(struct platform_device *pdev)
{
	struct ehrpwm_pwm_chip *pc = platform_get_drvdata(pdev);

	pwmchip_remove(&pc->chip);

	clk_unprepare(pc->tbclk);

	pm_runtime_disable(&pdev->dev);
}

#ifdef CONFIG_PM_SLEEP
static void ehrpwm_pwm_save_context(struct ehrpwm_pwm_chip *pc)
{
	pm_runtime_get_sync(pc->chip.dev);

	pc->ctx.tbctl = ehrpwm_read(pc->mmio_base, TBCTL);
	pc->ctx.tbprd = ehrpwm_read(pc->mmio_base, TBPRD);
	pc->ctx.cmpa = ehrpwm_read(pc->mmio_base, CMPA);
	pc->ctx.cmpb = ehrpwm_read(pc->mmio_base, CMPB);
	pc->ctx.aqctla = ehrpwm_read(pc->mmio_base, AQCTLA);
	pc->ctx.aqctlb = ehrpwm_read(pc->mmio_base, AQCTLB);
	pc->ctx.aqsfrc = ehrpwm_read(pc->mmio_base, AQSFRC);
	pc->ctx.aqcsfrc = ehrpwm_read(pc->mmio_base, AQCSFRC);

	pm_runtime_put_sync(pc->chip.dev);
}

static void ehrpwm_pwm_restore_context(struct ehrpwm_pwm_chip *pc)
{
	ehrpwm_write(pc->mmio_base, TBPRD, pc->ctx.tbprd);
	ehrpwm_write(pc->mmio_base, CMPA, pc->ctx.cmpa);
	ehrpwm_write(pc->mmio_base, CMPB, pc->ctx.cmpb);
	ehrpwm_write(pc->mmio_base, AQCTLA, pc->ctx.aqctla);
	ehrpwm_write(pc->mmio_base, AQCTLB, pc->ctx.aqctlb);
	ehrpwm_write(pc->mmio_base, AQSFRC, pc->ctx.aqsfrc);
	ehrpwm_write(pc->mmio_base, AQCSFRC, pc->ctx.aqcsfrc);
	ehrpwm_write(pc->mmio_base, TBCTL, pc->ctx.tbctl);
}

static int ehrpwm_pwm_suspend(struct device *dev)
{
	struct ehrpwm_pwm_chip *pc = dev_get_drvdata(dev);
	unsigned int i;

	ehrpwm_pwm_save_context(pc);

	for (i = 0; i < pc->chip.npwm; i++) {
		struct pwm_device *pwm = &pc->chip.pwms[i];

		if (!pwm_is_enabled(pwm))
			continue;

		 
		pm_runtime_put_sync(dev);
	}

	return 0;
}

static int ehrpwm_pwm_resume(struct device *dev)
{
	struct ehrpwm_pwm_chip *pc = dev_get_drvdata(dev);
	unsigned int i;

	for (i = 0; i < pc->chip.npwm; i++) {
		struct pwm_device *pwm = &pc->chip.pwms[i];

		if (!pwm_is_enabled(pwm))
			continue;

		 
		pm_runtime_get_sync(dev);
	}

	ehrpwm_pwm_restore_context(pc);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(ehrpwm_pwm_pm_ops, ehrpwm_pwm_suspend,
			 ehrpwm_pwm_resume);

static struct platform_driver ehrpwm_pwm_driver = {
	.driver = {
		.name = "ehrpwm",
		.of_match_table = ehrpwm_of_match,
		.pm = &ehrpwm_pwm_pm_ops,
	},
	.probe = ehrpwm_pwm_probe,
	.remove_new = ehrpwm_pwm_remove,
};
module_platform_driver(ehrpwm_pwm_driver);

MODULE_DESCRIPTION("EHRPWM PWM driver");
MODULE_AUTHOR("Texas Instruments");
MODULE_LICENSE("GPL");
