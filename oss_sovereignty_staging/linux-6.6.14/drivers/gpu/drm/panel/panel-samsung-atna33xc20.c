
 

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>

#include <drm/display/drm_dp_aux_bus.h>
#include <drm/display/drm_dp_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_panel.h>

 
#define HPD_MAX_MS	200
#define HPD_MAX_US	(HPD_MAX_MS * 1000)

struct atana33xc20_panel {
	struct drm_panel base;
	bool prepared;
	bool enabled;
	bool el3_was_on;

	bool no_hpd;
	struct gpio_desc *hpd_gpio;

	struct regulator *supply;
	struct gpio_desc *el_on3_gpio;
	struct drm_dp_aux *aux;

	struct edid *edid;

	ktime_t powered_off_time;
	ktime_t powered_on_time;
	ktime_t el_on3_off_time;
};

static inline struct atana33xc20_panel *to_atana33xc20(struct drm_panel *panel)
{
	return container_of(panel, struct atana33xc20_panel, base);
}

static void atana33xc20_wait(ktime_t start_ktime, unsigned int min_ms)
{
	ktime_t now_ktime, min_ktime;

	min_ktime = ktime_add(start_ktime, ms_to_ktime(min_ms));
	now_ktime = ktime_get_boottime();

	if (ktime_before(now_ktime, min_ktime))
		msleep(ktime_to_ms(ktime_sub(min_ktime, now_ktime)) + 1);
}

static int atana33xc20_suspend(struct device *dev)
{
	struct atana33xc20_panel *p = dev_get_drvdata(dev);
	int ret;

	 
	if (p->el3_was_on)
		atana33xc20_wait(p->el_on3_off_time, 150);

	ret = regulator_disable(p->supply);
	if (ret)
		return ret;
	p->powered_off_time = ktime_get_boottime();
	p->el3_was_on = false;

	return 0;
}

static int atana33xc20_resume(struct device *dev)
{
	struct atana33xc20_panel *p = dev_get_drvdata(dev);
	int hpd_asserted;
	int ret;

	 
	atana33xc20_wait(p->powered_off_time, 500);

	ret = regulator_enable(p->supply);
	if (ret)
		return ret;
	p->powered_on_time = ktime_get_boottime();

	if (p->no_hpd) {
		msleep(HPD_MAX_MS);
		return 0;
	}

	if (p->hpd_gpio) {
		ret = readx_poll_timeout(gpiod_get_value_cansleep, p->hpd_gpio,
					 hpd_asserted, hpd_asserted,
					 1000, HPD_MAX_US);
		if (hpd_asserted < 0)
			ret = hpd_asserted;

		if (ret)
			dev_warn(dev, "Error waiting for HPD GPIO: %d\n", ret);

		return ret;
	}

	if (p->aux->wait_hpd_asserted) {
		ret = p->aux->wait_hpd_asserted(p->aux, HPD_MAX_US);

		if (ret)
			dev_warn(dev, "Controller error waiting for HPD: %d\n", ret);

		return ret;
	}

	 
	return 0;
}

static int atana33xc20_disable(struct drm_panel *panel)
{
	struct atana33xc20_panel *p = to_atana33xc20(panel);

	 
	if (!p->enabled)
		return 0;

	gpiod_set_value_cansleep(p->el_on3_gpio, 0);
	p->el_on3_off_time = ktime_get_boottime();
	p->enabled = false;

	 
	p->el3_was_on = true;

	 
	msleep(20);

	return 0;
}

static int atana33xc20_enable(struct drm_panel *panel)
{
	struct atana33xc20_panel *p = to_atana33xc20(panel);

	 
	if (p->enabled)
		return 0;

	 
	if (WARN_ON(p->el3_was_on))
		return -EIO;

	 
	atana33xc20_wait(p->powered_on_time, 400);

	gpiod_set_value_cansleep(p->el_on3_gpio, 1);
	p->enabled = true;

	return 0;
}

static int atana33xc20_unprepare(struct drm_panel *panel)
{
	struct atana33xc20_panel *p = to_atana33xc20(panel);
	int ret;

	 
	if (!p->prepared)
		return 0;

	 
	ret = pm_runtime_put_sync_suspend(panel->dev);
	if (ret < 0)
		return ret;
	p->prepared = false;

	return 0;
}

static int atana33xc20_prepare(struct drm_panel *panel)
{
	struct atana33xc20_panel *p = to_atana33xc20(panel);
	int ret;

	 
	if (p->prepared)
		return 0;

	ret = pm_runtime_get_sync(panel->dev);
	if (ret < 0) {
		pm_runtime_put_autosuspend(panel->dev);
		return ret;
	}
	p->prepared = true;

	return 0;
}

static int atana33xc20_get_modes(struct drm_panel *panel,
				 struct drm_connector *connector)
{
	struct atana33xc20_panel *p = to_atana33xc20(panel);
	struct dp_aux_ep_device *aux_ep = to_dp_aux_ep_dev(panel->dev);
	int num = 0;

	pm_runtime_get_sync(panel->dev);

	if (!p->edid)
		p->edid = drm_get_edid(connector, &aux_ep->aux->ddc);
	num = drm_add_edid_modes(connector, p->edid);

	pm_runtime_mark_last_busy(panel->dev);
	pm_runtime_put_autosuspend(panel->dev);

	return num;
}

static const struct drm_panel_funcs atana33xc20_funcs = {
	.disable = atana33xc20_disable,
	.enable = atana33xc20_enable,
	.unprepare = atana33xc20_unprepare,
	.prepare = atana33xc20_prepare,
	.get_modes = atana33xc20_get_modes,
};

static void atana33xc20_runtime_disable(void *data)
{
	pm_runtime_disable(data);
}

static void atana33xc20_dont_use_autosuspend(void *data)
{
	pm_runtime_dont_use_autosuspend(data);
}

static int atana33xc20_probe(struct dp_aux_ep_device *aux_ep)
{
	struct atana33xc20_panel *panel;
	struct device *dev = &aux_ep->dev;
	int ret;

	panel = devm_kzalloc(dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;
	dev_set_drvdata(dev, panel);

	panel->aux = aux_ep->aux;

	panel->supply = devm_regulator_get(dev, "power");
	if (IS_ERR(panel->supply))
		return dev_err_probe(dev, PTR_ERR(panel->supply),
				     "Failed to get power supply\n");

	panel->el_on3_gpio = devm_gpiod_get(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(panel->el_on3_gpio))
		return dev_err_probe(dev, PTR_ERR(panel->el_on3_gpio),
				     "Failed to get enable GPIO\n");

	panel->no_hpd = of_property_read_bool(dev->of_node, "no-hpd");
	if (!panel->no_hpd) {
		panel->hpd_gpio = devm_gpiod_get_optional(dev, "hpd", GPIOD_IN);
		if (IS_ERR(panel->hpd_gpio))
			return dev_err_probe(dev, PTR_ERR(panel->hpd_gpio),
					     "Failed to get HPD GPIO\n");
	}

	pm_runtime_enable(dev);
	ret = devm_add_action_or_reset(dev,  atana33xc20_runtime_disable, dev);
	if (ret)
		return ret;
	pm_runtime_set_autosuspend_delay(dev, 2000);
	pm_runtime_use_autosuspend(dev);
	ret = devm_add_action_or_reset(dev,  atana33xc20_dont_use_autosuspend, dev);
	if (ret)
		return ret;

	drm_panel_init(&panel->base, dev, &atana33xc20_funcs, DRM_MODE_CONNECTOR_eDP);

	pm_runtime_get_sync(dev);
	ret = drm_panel_dp_aux_backlight(&panel->base, aux_ep->aux);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to register dp aux backlight\n");

	drm_panel_add(&panel->base);

	return 0;
}

static void atana33xc20_remove(struct dp_aux_ep_device *aux_ep)
{
	struct device *dev = &aux_ep->dev;
	struct atana33xc20_panel *panel = dev_get_drvdata(dev);

	drm_panel_remove(&panel->base);
	drm_panel_disable(&panel->base);
	drm_panel_unprepare(&panel->base);

	kfree(panel->edid);
}

static void atana33xc20_shutdown(struct dp_aux_ep_device *aux_ep)
{
	struct device *dev = &aux_ep->dev;
	struct atana33xc20_panel *panel = dev_get_drvdata(dev);

	drm_panel_disable(&panel->base);
	drm_panel_unprepare(&panel->base);
}

static const struct of_device_id atana33xc20_dt_match[] = {
	{ .compatible = "samsung,atna33xc20", },
	{   }
};
MODULE_DEVICE_TABLE(of, atana33xc20_dt_match);

static const struct dev_pm_ops atana33xc20_pm_ops = {
	SET_RUNTIME_PM_OPS(atana33xc20_suspend, atana33xc20_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
};

static struct dp_aux_ep_driver atana33xc20_driver = {
	.driver = {
		.name		= "samsung_atana33xc20",
		.of_match_table = atana33xc20_dt_match,
		.pm		= &atana33xc20_pm_ops,
	},
	.probe = atana33xc20_probe,
	.remove = atana33xc20_remove,
	.shutdown = atana33xc20_shutdown,
};

static int __init atana33xc20_init(void)
{
	return dp_aux_dp_driver_register(&atana33xc20_driver);
}
module_init(atana33xc20_init);

static void __exit atana33xc20_exit(void)
{
	dp_aux_dp_driver_unregister(&atana33xc20_driver);
}
module_exit(atana33xc20_exit);

MODULE_DESCRIPTION("Samsung ATANA33XC20 Panel Driver");
MODULE_LICENSE("GPL v2");
