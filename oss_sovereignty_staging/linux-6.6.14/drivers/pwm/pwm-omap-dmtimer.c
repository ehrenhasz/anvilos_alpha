
 

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <clocksource/timer-ti-dm.h>
#include <linux/platform_data/dmtimer-omap.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/time.h>

#define DM_TIMER_LOAD_MIN 0xfffffffe
#define DM_TIMER_MAX      0xffffffff

 
struct pwm_omap_dmtimer_chip {
	struct pwm_chip chip;
	 
	struct mutex mutex;
	struct omap_dm_timer *dm_timer;
	const struct omap_dm_timer_ops *pdata;
	struct platform_device *dm_timer_pdev;
};

static inline struct pwm_omap_dmtimer_chip *
to_pwm_omap_dmtimer_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct pwm_omap_dmtimer_chip, chip);
}

 
static u32 pwm_omap_dmtimer_get_clock_cycles(unsigned long clk_rate, int ns)
{
	return DIV_ROUND_CLOSEST_ULL((u64)clk_rate * ns, NSEC_PER_SEC);
}

 
static void pwm_omap_dmtimer_start(struct pwm_omap_dmtimer_chip *omap)
{
	 
	omap->pdata->enable(omap->dm_timer);
	omap->pdata->write_counter(omap->dm_timer, DM_TIMER_LOAD_MIN);
	omap->pdata->disable(omap->dm_timer);

	omap->pdata->start(omap->dm_timer);
}

 
static bool pwm_omap_dmtimer_is_enabled(struct pwm_omap_dmtimer_chip *omap)
{
	u32 status;

	status = omap->pdata->get_pwm_status(omap->dm_timer);

	return !!(status & OMAP_TIMER_CTRL_ST);
}

 
static int pwm_omap_dmtimer_polarity(struct pwm_omap_dmtimer_chip *omap)
{
	u32 status;

	status = omap->pdata->get_pwm_status(omap->dm_timer);

	return !!(status & OMAP_TIMER_CTRL_SCPWM);
}

 
static int pwm_omap_dmtimer_config(struct pwm_chip *chip,
				   struct pwm_device *pwm,
				   int duty_ns, int period_ns)
{
	struct pwm_omap_dmtimer_chip *omap = to_pwm_omap_dmtimer_chip(chip);
	u32 period_cycles, duty_cycles;
	u32 load_value, match_value;
	unsigned long clk_rate;
	struct clk *fclk;

	dev_dbg(chip->dev, "requested duty cycle: %d ns, period: %d ns\n",
		duty_ns, period_ns);

	if (duty_ns == pwm_get_duty_cycle(pwm) &&
	    period_ns == pwm_get_period(pwm))
		return 0;

	fclk = omap->pdata->get_fclk(omap->dm_timer);
	if (!fclk) {
		dev_err(chip->dev, "invalid pmtimer fclk\n");
		return -EINVAL;
	}

	clk_rate = clk_get_rate(fclk);
	if (!clk_rate) {
		dev_err(chip->dev, "invalid pmtimer fclk rate\n");
		return -EINVAL;
	}

	dev_dbg(chip->dev, "clk rate: %luHz\n", clk_rate);

	 
	period_cycles = pwm_omap_dmtimer_get_clock_cycles(clk_rate, period_ns);
	duty_cycles = pwm_omap_dmtimer_get_clock_cycles(clk_rate, duty_ns);

	if (period_cycles < 2) {
		dev_info(chip->dev,
			 "period %d ns too short for clock rate %lu Hz\n",
			 period_ns, clk_rate);
		return -EINVAL;
	}

	if (duty_cycles < 1) {
		dev_dbg(chip->dev,
			"duty cycle %d ns is too short for clock rate %lu Hz\n",
			duty_ns, clk_rate);
		dev_dbg(chip->dev, "using minimum of 1 clock cycle\n");
		duty_cycles = 1;
	} else if (duty_cycles >= period_cycles) {
		dev_dbg(chip->dev,
			"duty cycle %d ns is too long for period %d ns at clock rate %lu Hz\n",
			duty_ns, period_ns, clk_rate);
		dev_dbg(chip->dev, "using maximum of 1 clock cycle less than period\n");
		duty_cycles = period_cycles - 1;
	}

	dev_dbg(chip->dev, "effective duty cycle: %lld ns, period: %lld ns\n",
		DIV_ROUND_CLOSEST_ULL((u64)NSEC_PER_SEC * duty_cycles,
				      clk_rate),
		DIV_ROUND_CLOSEST_ULL((u64)NSEC_PER_SEC * period_cycles,
				      clk_rate));

	load_value = (DM_TIMER_MAX - period_cycles) + 1;
	match_value = load_value + duty_cycles - 1;

	omap->pdata->set_load(omap->dm_timer, load_value);
	omap->pdata->set_match(omap->dm_timer, true, match_value);

	dev_dbg(chip->dev, "load value: %#08x (%d), match value: %#08x (%d)\n",
		load_value, load_value,	match_value, match_value);

	return 0;
}

 
static void pwm_omap_dmtimer_set_polarity(struct pwm_chip *chip,
					  struct pwm_device *pwm,
					  enum pwm_polarity polarity)
{
	struct pwm_omap_dmtimer_chip *omap = to_pwm_omap_dmtimer_chip(chip);
	bool enabled;

	 
	enabled = pwm_omap_dmtimer_is_enabled(omap);
	if (enabled)
		omap->pdata->stop(omap->dm_timer);

	omap->pdata->set_pwm(omap->dm_timer,
			     polarity == PWM_POLARITY_INVERSED,
			     true, OMAP_TIMER_TRIGGER_OVERFLOW_AND_COMPARE,
			     true);

	if (enabled)
		pwm_omap_dmtimer_start(omap);
}

 
static int pwm_omap_dmtimer_apply(struct pwm_chip *chip,
				  struct pwm_device *pwm,
				  const struct pwm_state *state)
{
	struct pwm_omap_dmtimer_chip *omap = to_pwm_omap_dmtimer_chip(chip);
	int ret = 0;

	mutex_lock(&omap->mutex);

	if (pwm_omap_dmtimer_is_enabled(omap) && !state->enabled) {
		omap->pdata->stop(omap->dm_timer);
		goto unlock_mutex;
	}

	if (pwm_omap_dmtimer_polarity(omap) != state->polarity)
		pwm_omap_dmtimer_set_polarity(chip, pwm, state->polarity);

	ret = pwm_omap_dmtimer_config(chip, pwm, state->duty_cycle,
				      state->period);
	if (ret)
		goto unlock_mutex;

	if (!pwm_omap_dmtimer_is_enabled(omap) && state->enabled) {
		omap->pdata->set_pwm(omap->dm_timer,
				     state->polarity == PWM_POLARITY_INVERSED,
				     true,
				     OMAP_TIMER_TRIGGER_OVERFLOW_AND_COMPARE,
				     true);
		pwm_omap_dmtimer_start(omap);
	}

unlock_mutex:
	mutex_unlock(&omap->mutex);

	return ret;
}

static const struct pwm_ops pwm_omap_dmtimer_ops = {
	.apply = pwm_omap_dmtimer_apply,
	.owner = THIS_MODULE,
};

static int pwm_omap_dmtimer_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct dmtimer_platform_data *timer_pdata;
	const struct omap_dm_timer_ops *pdata;
	struct platform_device *timer_pdev;
	struct pwm_omap_dmtimer_chip *omap;
	struct omap_dm_timer *dm_timer;
	struct device_node *timer;
	int ret = 0;
	u32 v;

	timer = of_parse_phandle(np, "ti,timers", 0);
	if (!timer)
		return -ENODEV;

	timer_pdev = of_find_device_by_node(timer);
	if (!timer_pdev) {
		dev_err(&pdev->dev, "Unable to find Timer pdev\n");
		ret = -ENODEV;
		goto err_find_timer_pdev;
	}

	timer_pdata = dev_get_platdata(&timer_pdev->dev);
	if (!timer_pdata) {
		dev_dbg(&pdev->dev,
			 "dmtimer pdata structure NULL, deferring probe\n");
		ret = -EPROBE_DEFER;
		goto err_platdata;
	}

	pdata = timer_pdata->timer_ops;

	if (!pdata || !pdata->request_by_node ||
	    !pdata->free ||
	    !pdata->enable ||
	    !pdata->disable ||
	    !pdata->get_fclk ||
	    !pdata->start ||
	    !pdata->stop ||
	    !pdata->set_load ||
	    !pdata->set_match ||
	    !pdata->set_pwm ||
	    !pdata->get_pwm_status ||
	    !pdata->set_prescaler ||
	    !pdata->write_counter) {
		dev_err(&pdev->dev, "Incomplete dmtimer pdata structure\n");
		ret = -EINVAL;
		goto err_platdata;
	}

	if (!of_get_property(timer, "ti,timer-pwm", NULL)) {
		dev_err(&pdev->dev, "Missing ti,timer-pwm capability\n");
		ret = -ENODEV;
		goto err_timer_property;
	}

	dm_timer = pdata->request_by_node(timer);
	if (!dm_timer) {
		ret = -EPROBE_DEFER;
		goto err_request_timer;
	}

	omap = devm_kzalloc(&pdev->dev, sizeof(*omap), GFP_KERNEL);
	if (!omap) {
		ret = -ENOMEM;
		goto err_alloc_omap;
	}

	omap->pdata = pdata;
	omap->dm_timer = dm_timer;
	omap->dm_timer_pdev = timer_pdev;

	 
	if (pm_runtime_active(&omap->dm_timer_pdev->dev))
		omap->pdata->stop(omap->dm_timer);

	if (!of_property_read_u32(pdev->dev.of_node, "ti,prescaler", &v))
		omap->pdata->set_prescaler(omap->dm_timer, v);

	 
	if (!of_property_read_u32(pdev->dev.of_node, "ti,clock-source", &v))
		omap->pdata->set_source(omap->dm_timer, v);

	omap->chip.dev = &pdev->dev;
	omap->chip.ops = &pwm_omap_dmtimer_ops;
	omap->chip.npwm = 1;

	mutex_init(&omap->mutex);

	ret = pwmchip_add(&omap->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register PWM\n");
		goto err_pwmchip_add;
	}

	of_node_put(timer);

	platform_set_drvdata(pdev, omap);

	return 0;

err_pwmchip_add:

	 
err_alloc_omap:

	pdata->free(dm_timer);
err_request_timer:

err_timer_property:
err_platdata:

	put_device(&timer_pdev->dev);
err_find_timer_pdev:

	of_node_put(timer);

	return ret;
}

static void pwm_omap_dmtimer_remove(struct platform_device *pdev)
{
	struct pwm_omap_dmtimer_chip *omap = platform_get_drvdata(pdev);

	pwmchip_remove(&omap->chip);

	if (pm_runtime_active(&omap->dm_timer_pdev->dev))
		omap->pdata->stop(omap->dm_timer);

	omap->pdata->free(omap->dm_timer);

	put_device(&omap->dm_timer_pdev->dev);

	mutex_destroy(&omap->mutex);
}

static const struct of_device_id pwm_omap_dmtimer_of_match[] = {
	{.compatible = "ti,omap-dmtimer-pwm"},
	{}
};
MODULE_DEVICE_TABLE(of, pwm_omap_dmtimer_of_match);

static struct platform_driver pwm_omap_dmtimer_driver = {
	.driver = {
		.name = "omap-dmtimer-pwm",
		.of_match_table = of_match_ptr(pwm_omap_dmtimer_of_match),
	},
	.probe = pwm_omap_dmtimer_probe,
	.remove_new = pwm_omap_dmtimer_remove,
};
module_platform_driver(pwm_omap_dmtimer_driver);

MODULE_AUTHOR("Grant Erickson <marathon96@gmail.com>");
MODULE_AUTHOR("NeilBrown <neilb@suse.de>");
MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("OMAP PWM Driver using Dual-mode Timers");
