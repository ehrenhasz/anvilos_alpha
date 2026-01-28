#ifndef _SND_SOC_INTEL_QUIRKS_H
#define _SND_SOC_INTEL_QUIRKS_H
#include <linux/platform_data/x86/soc.h>
#if IS_ENABLED(CONFIG_X86)
#include <linux/dmi.h>
#include <asm/iosf_mbi.h>
static inline bool soc_intel_is_byt_cr(struct platform_device *pdev)
{
	static const struct dmi_system_id force_bytcr_table[] = {
		{	 
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
				DMI_MATCH(DMI_PRODUCT_FAMILY, "YOGATablet2"),
			},
		},
		{}
	};
	struct device *dev = &pdev->dev;
	int status = 0;
	if (!soc_intel_is_byt())
		return false;
	if (dmi_check_system(force_bytcr_table))
		return true;
	if (iosf_mbi_available()) {
		u32 bios_status;
		status = iosf_mbi_read(BT_MBI_UNIT_PMC,  
				       MBI_REG_READ,  
				       0x006,  
				       &bios_status);
		if (status) {
			dev_err(dev, "could not read PUNIT BIOS_CONFIG\n");
		} else {
			bios_status = (bios_status >> 26) & 3;
			if (bios_status == 1 || bios_status == 3) {
				dev_info(dev, "Detected Baytrail-CR platform\n");
				return true;
			}
			dev_info(dev, "BYT-CR not detected\n");
		}
	} else {
		dev_info(dev, "IOSF_MBI not available, no BYT-CR detection\n");
	}
	if (!platform_get_resource(pdev, IORESOURCE_IRQ, 5)) {
		dev_info(dev, "Falling back to Baytrail-CR platform\n");
		return true;
	}
	return false;
}
#else
static inline bool soc_intel_is_byt_cr(struct platform_device *pdev)
{
	return false;
}
#endif
#endif  
