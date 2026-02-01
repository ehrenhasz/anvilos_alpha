
 

#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>

#define PWM_IMX_TPM_PARAM	0x4
#define PWM_IMX_TPM_GLOBAL	0x8
#define PWM_IMX_TPM_SC		0x10
#define PWM_IMX_TPM_CNT		0x14
#define PWM_IMX_TPM_MOD		0x18
#define PWM_IMX_TPM_CnSC(n)	(0x20 + (n) * 0x8)
#define PWM_IMX_TPM_CnV(n)	(0x24 + (n) * 0x8)

#define PWM_IMX_TPM_PARAM_CHAN			GENMASK(7, 0)

#define PWM_IMX_TPM_SC_PS			GENMASK(2, 0)
#define PWM_IMX_TPM_SC_CMOD			GENMASK(4, 3)
#define PWM_IMX_TPM_SC_CMOD_INC_EVERY_CLK	FIELD_PREP(PWM_IMX_TPM_SC_CMOD, 1)
#define PWM_IMX_TPM_SC_CPWMS			BIT(5)

#define PWM_IMX_TPM_CnSC_CHF	BIT(7)
#define PWM_IMX_TPM_CnSC_MSB	BIT(5)
#define PWM_IMX_TPM_CnSC_MSA	BIT(4)

 
#define PWM_IMX_TPM_CnSC_ELS	GENMASK(3, 2)
#define PWM_IMX_TPM_CnSC_ELS_INVERSED	FIELD_PREP(PWM_IMX_TPM_CnSC_ELS, 1)
#define PWM_IMX_TPM_CnSC_ELS_NORMAL	FIELD_PREP(PWM_IMX_TPM_CnSC_ELS, 2)


#define PWM_IMX_TPM_MOD_WIDTH	16
#define PWM_IMX_TPM_MOD_MOD	GENMASK(PWM_IMX_TPM_MOD_WIDTH - 1, 0)

struct imx_tpm_pwm_chip {
	struct pwm_chip chip;
	struct clk *clk;
	void __iomem *base;
	struct mutex lock;
	u32 user_count;
	u32 enable_count;
	u32 real_period;
};

struct imx_tpm_pwm_param {
	u8 prescale;
	u32 mod;
	u32 val;
};

static inline struct imx_tpm_pwm_chip *
to_imx_tpm_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct imx_tpm_pwm_chip, chip);
}

 
static int pwm_imx_tpm_round_state(struct pwm_chip *chip,
				   struct imx_tpm_pwm_param *p,
				   struct pwm_state *real_state,
				   const struct pwm_state *state)
{
	struct imx_tpm_pwm_chip *tpm = to_imx_tpm_pwm_chip(chip);
	u32 rate, prescale, period_count, clock_unit;
	u64 tmp;

	rate = clk_get_rate(tpm->clk);
	tmp = (u64)state->period * rate;
	clock_unit = DIV_ROUND_CLOSEST_ULL(tmp, NSEC_PER_SEC);
	if (clock_unit <= PWM_IMX_TPM_MOD_MOD)
		prescale = 0;
	else
		prescale = ilog2(clock_unit) + 1 - PWM_IMX_TPM_MOD_WIDTH;

	if ((!FIELD_FIT(PWM_IMX_TPM_SC_PS, prescale)))
		return -ERANGE;
	p->prescale = prescale;

	period_count = (clock_unit + ((1 << prescale) >> 1)) >> prescale;
	p->mod = period_count;

	 
	tmp = (u64)period_count << prescale;
	tmp *= NSEC_PER_SEC;
	real_state->period = DIV_ROUND_CLOSEST_ULL(tmp, rate);

	 
	if (!state->enabled)
		real_state->duty_cycle = 0;
	else
		real_state->duty_cycle = state->duty_cycle;

	tmp = (u64)p->mod * real_state->duty_cycle;
	p->val = DIV64_U64_ROUND_CLOSEST(tmp, real_state->period);

	real_state->polarity = state->polarity;
	real_state->enabled = state->enabled;

	return 0;
}

static int pwm_imx_tpm_get_state(struct pwm_chip *chip,
				 struct pwm_device *pwm,
				 struct pwm_state *state)
{
	struct imx_tpm_pwm_chip *tpm = to_imx_tpm_pwm_chip(chip);
	u32 rate, val, prescale;
	u64 tmp;

	 
	state->period = tpm->real_period;

	 
	rate = clk_get_rate(tpm->clk);
	val = readl(tpm->base + PWM_IMX_TPM_SC);
	prescale = FIELD_GET(PWM_IMX_TPM_SC_PS, val);
	tmp = readl(tpm->base + PWM_IMX_TPM_CnV(pwm->hwpwm));
	tmp = (tmp << prescale) * NSEC_PER_SEC;
	state->duty_cycle = DIV_ROUND_CLOSEST_ULL(tmp, rate);

	 
	val = readl(tpm->base + PWM_IMX_TPM_CnSC(pwm->hwpwm));
	if ((val & PWM_IMX_TPM_CnSC_ELS) == PWM_IMX_TPM_CnSC_ELS_INVERSED)
		state->polarity = PWM_POLARITY_INVERSED;
	else
		 
		state->polarity = PWM_POLARITY_NORMAL;

	 
	state->enabled = FIELD_GET(PWM_IMX_TPM_CnSC_ELS, val) ? true : false;

	return 0;
}

 
static int pwm_imx_tpm_apply_hw(struct pwm_chip *chip,
				struct imx_tpm_pwm_param *p,
				struct pwm_state *state,
				struct pwm_device *pwm)
{
	struct imx_tpm_pwm_chip *tpm = to_imx_tpm_pwm_chip(chip);
	bool period_update = false;
	bool duty_update = false;
	u32 val, cmod, cur_prescale;
	unsigned long timeout;
	struct pwm_state c;

	if (state->period != tpm->real_period) {
		 
		if (tpm->user_count > 1)
			return -EBUSY;

		val = readl(tpm->base + PWM_IMX_TPM_SC);
		cmod = FIELD_GET(PWM_IMX_TPM_SC_CMOD, val);
		cur_prescale = FIELD_GET(PWM_IMX_TPM_SC_PS, val);
		if (cmod && cur_prescale != p->prescale)
			return -EBUSY;

		 
		val &= ~PWM_IMX_TPM_SC_PS;
		val |= FIELD_PREP(PWM_IMX_TPM_SC_PS, p->prescale);
		writel(val, tpm->base + PWM_IMX_TPM_SC);

		 
		writel(p->mod, tpm->base + PWM_IMX_TPM_MOD);
		tpm->real_period = state->period;
		period_update = true;
	}

	pwm_imx_tpm_get_state(chip, pwm, &c);

	 
	if (c.enabled && c.polarity != state->polarity)
		return -EBUSY;

	if (state->duty_cycle != c.duty_cycle) {
		 
		writel(p->val, tpm->base + PWM_IMX_TPM_CnV(pwm->hwpwm));
		duty_update = true;
	}

	 
	if (period_update || duty_update) {
		timeout = jiffies + msecs_to_jiffies(tpm->real_period /
						     NSEC_PER_MSEC + 1);
		while (readl(tpm->base + PWM_IMX_TPM_MOD) != p->mod
		       || readl(tpm->base + PWM_IMX_TPM_CnV(pwm->hwpwm))
		       != p->val) {
			if (time_after(jiffies, timeout))
				return -ETIME;
			cpu_relax();
		}
	}

	 
	val = readl(tpm->base + PWM_IMX_TPM_CnSC(pwm->hwpwm));
	val &= ~(PWM_IMX_TPM_CnSC_ELS | PWM_IMX_TPM_CnSC_MSA |
		 PWM_IMX_TPM_CnSC_MSB);
	if (state->enabled) {
		 
		val |= PWM_IMX_TPM_CnSC_MSB;
		val |= (state->polarity == PWM_POLARITY_NORMAL) ?
			PWM_IMX_TPM_CnSC_ELS_NORMAL :
			PWM_IMX_TPM_CnSC_ELS_INVERSED;
	}
	writel(val, tpm->base + PWM_IMX_TPM_CnSC(pwm->hwpwm));

	 
	if (state->enabled != c.enabled) {
		val = readl(tpm->base + PWM_IMX_TPM_SC);
		if (state->enabled) {
			if (++tpm->enable_count == 1)
				val |= PWM_IMX_TPM_SC_CMOD_INC_EVERY_CLK;
		} else {
			if (--tpm->enable_count == 0)
				val &= ~PWM_IMX_TPM_SC_CMOD;
		}
		writel(val, tpm->base + PWM_IMX_TPM_SC);
	}

	return 0;
}

static int pwm_imx_tpm_apply(struct pwm_chip *chip,
			     struct pwm_device *pwm,
			     const struct pwm_state *state)
{
	struct imx_tpm_pwm_chip *tpm = to_imx_tpm_pwm_chip(chip);
	struct imx_tpm_pwm_param param;
	struct pwm_state real_state;
	int ret;

	ret = pwm_imx_tpm_round_state(chip, &param, &real_state, state);
	if (ret)
		return ret;

	mutex_lock(&tpm->lock);
	ret = pwm_imx_tpm_apply_hw(chip, &param, &real_state, pwm);
	mutex_unlock(&tpm->lock);

	return ret;
}

static int pwm_imx_tpm_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct imx_tpm_pwm_chip *tpm = to_imx_tpm_pwm_chip(chip);

	mutex_lock(&tpm->lock);
	tpm->user_count++;
	mutex_unlock(&tpm->lock);

	return 0;
}

static void pwm_imx_tpm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct imx_tpm_pwm_chip *tpm = to_imx_tpm_pwm_chip(chip);

	mutex_lock(&tpm->lock);
	tpm->user_count--;
	mutex_unlock(&tpm->lock);
}

static const struct pwm_ops imx_tpm_pwm_ops = {
	.request = pwm_imx_tpm_request,
	.free = pwm_imx_tpm_free,
	.get_state = pwm_imx_tpm_get_state,
	.apply = pwm_imx_tpm_apply,
	.owner = THIS_MODULE,
};

static int pwm_imx_tpm_probe(struct platform_device *pdev)
{
	struct imx_tpm_pwm_chip *tpm;
	int ret;
	u32 val;

	tpm = devm_kzalloc(&pdev->dev, sizeof(*tpm), GFP_KERNEL);
	if (!tpm)
		return -ENOMEM;

	platform_set_drvdata(pdev, tpm);

	tpm->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(tpm->base))
		return PTR_ERR(tpm->base);

	tpm->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(tpm->clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(tpm->clk),
				     "failed to get PWM clock\n");

	ret = clk_prepare_enable(tpm->clk);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to prepare or enable clock: %d\n", ret);
		return ret;
	}

	tpm->chip.dev = &pdev->dev;
	tpm->chip.ops = &imx_tpm_pwm_ops;

	 
	val = readl(tpm->base + PWM_IMX_TPM_PARAM);
	tpm->chip.npwm = FIELD_GET(PWM_IMX_TPM_PARAM_CHAN, val);

	mutex_init(&tpm->lock);

	ret = pwmchip_add(&tpm->chip);
	if (ret) {
		dev_err(&pdev->dev, "failed to add PWM chip: %d\n", ret);
		clk_disable_unprepare(tpm->clk);
	}

	return ret;
}

static void pwm_imx_tpm_remove(struct platform_device *pdev)
{
	struct imx_tpm_pwm_chip *tpm = platform_get_drvdata(pdev);

	pwmchip_remove(&tpm->chip);

	clk_disable_unprepare(tpm->clk);
}

static int __maybe_unused pwm_imx_tpm_suspend(struct device *dev)
{
	struct imx_tpm_pwm_chip *tpm = dev_get_drvdata(dev);

	if (tpm->enable_count > 0)
		return -EBUSY;

	 
	tpm->real_period = 0;

	clk_disable_unprepare(tpm->clk);

	return 0;
}

static int __maybe_unused pwm_imx_tpm_resume(struct device *dev)
{
	struct imx_tpm_pwm_chip *tpm = dev_get_drvdata(dev);
	int ret = 0;

	ret = clk_prepare_enable(tpm->clk);
	if (ret)
		dev_err(dev, "failed to prepare or enable clock: %d\n", ret);

	return ret;
}

static SIMPLE_DEV_PM_OPS(imx_tpm_pwm_pm,
			 pwm_imx_tpm_suspend, pwm_imx_tpm_resume);

static const struct of_device_id imx_tpm_pwm_dt_ids[] = {
	{ .compatible = "fsl,imx7ulp-pwm", },
	{   }
};
MODULE_DEVICE_TABLE(of, imx_tpm_pwm_dt_ids);

static struct platform_driver imx_tpm_pwm_driver = {
	.driver = {
		.name = "imx7ulp-tpm-pwm",
		.of_match_table = imx_tpm_pwm_dt_ids,
		.pm = &imx_tpm_pwm_pm,
	},
	.probe	= pwm_imx_tpm_probe,
	.remove_new = pwm_imx_tpm_remove,
};
module_platform_driver(imx_tpm_pwm_driver);

MODULE_AUTHOR("Anson Huang <Anson.Huang@nxp.com>");
MODULE_DESCRIPTION("i.MX TPM PWM Driver");
MODULE_LICENSE("GPL v2");
