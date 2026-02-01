
 

#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/property.h>

#include "pwm-lpss.h"


static int pwm_lpss_probe_platform(struct platform_device *pdev)
{
	const struct pwm_lpss_boardinfo *info;
	struct pwm_lpss_chip *lpwm;
	void __iomem *base;

	info = device_get_match_data(&pdev->dev);
	if (!info)
		return -ENODEV;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	lpwm = devm_pwm_lpss_probe(&pdev->dev, base, info);
	if (IS_ERR(lpwm))
		return PTR_ERR(lpwm);

	platform_set_drvdata(pdev, lpwm);

	 
	if (info->other_devices_aml_touches_pwm_regs)
		dev_pm_set_driver_flags(&pdev->dev, DPM_FLAG_NO_DIRECT_COMPLETE|
						    DPM_FLAG_SMART_SUSPEND);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;
}

static void pwm_lpss_remove_platform(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
}

static const struct acpi_device_id pwm_lpss_acpi_match[] = {
	{ "80860F09", (unsigned long)&pwm_lpss_byt_info },
	{ "80862288", (unsigned long)&pwm_lpss_bsw_info },
	{ "80862289", (unsigned long)&pwm_lpss_bsw_info },
	{ "80865AC8", (unsigned long)&pwm_lpss_bxt_info },
	{ },
};
MODULE_DEVICE_TABLE(acpi, pwm_lpss_acpi_match);

static struct platform_driver pwm_lpss_driver_platform = {
	.driver = {
		.name = "pwm-lpss",
		.acpi_match_table = pwm_lpss_acpi_match,
	},
	.probe = pwm_lpss_probe_platform,
	.remove_new = pwm_lpss_remove_platform,
};
module_platform_driver(pwm_lpss_driver_platform);

MODULE_DESCRIPTION("PWM platform driver for Intel LPSS");
MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(PWM_LPSS);
MODULE_ALIAS("platform:pwm-lpss");
