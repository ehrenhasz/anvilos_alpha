
 

#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include "intel_pmic.h"

 
static const struct intel_pmic_opregion_data intel_chtcrc_pmic_opregion_data = {
	.lpat_raw_to_temp = acpi_lpat_raw_to_temp,
	.pmic_i2c_address = 0x6e,
};

static int intel_chtcrc_pmic_opregion_probe(struct platform_device *pdev)
{
	struct intel_soc_pmic *pmic = dev_get_drvdata(pdev->dev.parent);
	return intel_pmic_install_opregion_handler(&pdev->dev,
			ACPI_HANDLE(pdev->dev.parent), pmic->regmap,
			&intel_chtcrc_pmic_opregion_data);
}

static struct platform_driver intel_chtcrc_pmic_opregion_driver = {
	.probe = intel_chtcrc_pmic_opregion_probe,
	.driver = {
		.name = "cht_crystal_cove_pmic",
	},
};
builtin_platform_driver(intel_chtcrc_pmic_opregion_driver);
