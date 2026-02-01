
 

#include <linux/dmi.h>

#include "i915_drv.h"
#include "intel_display_types.h"
#include "intel_quirks.h"

static void intel_set_quirk(struct drm_i915_private *i915, enum intel_quirk_id quirk)
{
	i915->display.quirks.mask |= BIT(quirk);
}

 
static void quirk_ssc_force_disable(struct drm_i915_private *i915)
{
	intel_set_quirk(i915, QUIRK_LVDS_SSC_DISABLE);
	drm_info(&i915->drm, "applying lvds SSC disable quirk\n");
}

 
static void quirk_invert_brightness(struct drm_i915_private *i915)
{
	intel_set_quirk(i915, QUIRK_INVERT_BRIGHTNESS);
	drm_info(&i915->drm, "applying inverted panel brightness quirk\n");
}

 
static void quirk_backlight_present(struct drm_i915_private *i915)
{
	intel_set_quirk(i915, QUIRK_BACKLIGHT_PRESENT);
	drm_info(&i915->drm, "applying backlight present quirk\n");
}

 
static void quirk_increase_t12_delay(struct drm_i915_private *i915)
{
	intel_set_quirk(i915, QUIRK_INCREASE_T12_DELAY);
	drm_info(&i915->drm, "Applying T12 delay quirk\n");
}

 
static void quirk_increase_ddi_disabled_time(struct drm_i915_private *i915)
{
	intel_set_quirk(i915, QUIRK_INCREASE_DDI_DISABLED_TIME);
	drm_info(&i915->drm, "Applying Increase DDI Disabled quirk\n");
}

static void quirk_no_pps_backlight_power_hook(struct drm_i915_private *i915)
{
	intel_set_quirk(i915, QUIRK_NO_PPS_BACKLIGHT_POWER_HOOK);
	drm_info(&i915->drm, "Applying no pps backlight power quirk\n");
}

struct intel_quirk {
	int device;
	int subsystem_vendor;
	int subsystem_device;
	void (*hook)(struct drm_i915_private *i915);
};

 
struct intel_dmi_quirk {
	void (*hook)(struct drm_i915_private *i915);
	const struct dmi_system_id (*dmi_id_list)[];
};

static int intel_dmi_reverse_brightness(const struct dmi_system_id *id)
{
	DRM_INFO("Backlight polarity reversed on %s\n", id->ident);
	return 1;
}

static int intel_dmi_no_pps_backlight(const struct dmi_system_id *id)
{
	DRM_INFO("No pps backlight support on %s\n", id->ident);
	return 1;
}

static const struct intel_dmi_quirk intel_dmi_quirks[] = {
	{
		.dmi_id_list = &(const struct dmi_system_id[]) {
			{
				.callback = intel_dmi_reverse_brightness,
				.ident = "NCR Corporation",
				.matches = {DMI_MATCH(DMI_SYS_VENDOR, "NCR Corporation"),
					    DMI_MATCH(DMI_PRODUCT_NAME, ""),
				},
			},
			{
				.callback = intel_dmi_reverse_brightness,
				.ident = "Thundersoft TST178 tablet",
				 
				.matches = {DMI_EXACT_MATCH(DMI_BOARD_VENDOR, "AMI Corporation"),
					    DMI_EXACT_MATCH(DMI_BOARD_NAME, "Aptio CRB"),
					    DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "To be filled by O.E.M."),
					    DMI_EXACT_MATCH(DMI_BIOS_DATE, "04/15/2014"),
				},
			},
			{ }   
		},
		.hook = quirk_invert_brightness,
	},
	{
		.dmi_id_list = &(const struct dmi_system_id[]) {
			{
				.callback = intel_dmi_no_pps_backlight,
				.ident = "Google Lillipup sku524294",
				.matches = {DMI_EXACT_MATCH(DMI_BOARD_VENDOR, "Google"),
					    DMI_EXACT_MATCH(DMI_BOARD_NAME, "Lindar"),
					    DMI_EXACT_MATCH(DMI_PRODUCT_SKU, "sku524294"),
				},
			},
			{
				.callback = intel_dmi_no_pps_backlight,
				.ident = "Google Lillipup sku524295",
				.matches = {DMI_EXACT_MATCH(DMI_BOARD_VENDOR, "Google"),
					    DMI_EXACT_MATCH(DMI_BOARD_NAME, "Lindar"),
					    DMI_EXACT_MATCH(DMI_PRODUCT_SKU, "sku524295"),
				},
			},
			{ }
		},
		.hook = quirk_no_pps_backlight_power_hook,
	},
};

static struct intel_quirk intel_quirks[] = {
	 
	{ 0x0046, 0x17aa, 0x3920, quirk_ssc_force_disable },

	 
	{ 0x0046, 0x104d, 0x9076, quirk_ssc_force_disable },

	 
	{ 0x2a42, 0x1025, 0x0459, quirk_invert_brightness },

	 
	{ 0x2a42, 0x1025, 0x0210, quirk_invert_brightness },

	 
	{ 0x2a42, 0x1025, 0x0212, quirk_invert_brightness },

	 
	{ 0x2a42, 0x1025, 0x034b, quirk_invert_brightness },

	 
	{ 0x2a42, 0x1025, 0x0260, quirk_invert_brightness },

	 
	{ 0x2a42, 0x1025, 0x048a, quirk_invert_brightness },

	 
	{ 0x0a06, 0x1025, 0x0a11, quirk_backlight_present },

	 
	{ 0x0a16, 0x1025, 0x0a11, quirk_backlight_present },

	 
	{ 0x27a2, 0x8086, 0x7270, quirk_backlight_present },

	 
	{ 0x2a02, 0x106b, 0x00a1, quirk_backlight_present },

	 
	{ 0x0a06, 0x1179, 0x0a88, quirk_backlight_present },

	 
	{ 0x0a06, 0x103c, 0x21ed, quirk_backlight_present },

	 
	{ 0x0a06, 0x1028, 0x0a35, quirk_backlight_present },

	 
	{ 0x0a16, 0x1028, 0x0a35, quirk_backlight_present },

	 
	{ 0x191B, 0x1179, 0xF840, quirk_increase_t12_delay },

	 
	{ 0x3185, 0x8086, 0x2072, quirk_increase_ddi_disabled_time },
	{ 0x3184, 0x8086, 0x2072, quirk_increase_ddi_disabled_time },
	 
	{ 0x3185, 0x1849, 0x2212, quirk_increase_ddi_disabled_time },
	{ 0x3184, 0x1849, 0x2212, quirk_increase_ddi_disabled_time },
	 
	{ 0x3185, 0x1019, 0xa94d, quirk_increase_ddi_disabled_time },
	{ 0x3184, 0x1019, 0xa94d, quirk_increase_ddi_disabled_time },
	 
	{ 0x0f31, 0x103c, 0x220f, quirk_invert_brightness },
};

void intel_init_quirks(struct drm_i915_private *i915)
{
	struct pci_dev *d = to_pci_dev(i915->drm.dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(intel_quirks); i++) {
		struct intel_quirk *q = &intel_quirks[i];

		if (d->device == q->device &&
		    (d->subsystem_vendor == q->subsystem_vendor ||
		     q->subsystem_vendor == PCI_ANY_ID) &&
		    (d->subsystem_device == q->subsystem_device ||
		     q->subsystem_device == PCI_ANY_ID))
			q->hook(i915);
	}
	for (i = 0; i < ARRAY_SIZE(intel_dmi_quirks); i++) {
		if (dmi_check_system(*intel_dmi_quirks[i].dmi_id_list) != 0)
			intel_dmi_quirks[i].hook(i915);
	}
}

bool intel_has_quirk(struct drm_i915_private *i915, enum intel_quirk_id quirk)
{
	return i915->display.quirks.mask & BIT(quirk);
}
