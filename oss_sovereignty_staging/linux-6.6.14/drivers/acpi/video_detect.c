 

#include <linux/export.h>
#include <linux/acpi.h>
#include <linux/apple-gmux.h>
#include <linux/backlight.h>
#include <linux/dmi.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/platform_data/x86/nvidia-wmi-ec-backlight.h>
#include <linux/pnp.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <acpi/video.h>

static enum acpi_backlight_type acpi_backlight_cmdline = acpi_backlight_undef;
static enum acpi_backlight_type acpi_backlight_dmi = acpi_backlight_undef;

static void acpi_video_parse_cmdline(void)
{
	if (!strcmp("vendor", acpi_video_backlight_string))
		acpi_backlight_cmdline = acpi_backlight_vendor;
	if (!strcmp("video", acpi_video_backlight_string))
		acpi_backlight_cmdline = acpi_backlight_video;
	if (!strcmp("native", acpi_video_backlight_string))
		acpi_backlight_cmdline = acpi_backlight_native;
	if (!strcmp("nvidia_wmi_ec", acpi_video_backlight_string))
		acpi_backlight_cmdline = acpi_backlight_nvidia_wmi_ec;
	if (!strcmp("apple_gmux", acpi_video_backlight_string))
		acpi_backlight_cmdline = acpi_backlight_apple_gmux;
	if (!strcmp("none", acpi_video_backlight_string))
		acpi_backlight_cmdline = acpi_backlight_none;
}

static acpi_status
find_video(acpi_handle handle, u32 lvl, void *context, void **rv)
{
	struct acpi_device *acpi_dev = acpi_fetch_acpi_dev(handle);
	long *cap = context;
	struct pci_dev *dev;

	static const struct acpi_device_id video_ids[] = {
		{ACPI_VIDEO_HID, 0},
		{"", 0},
	};

	if (acpi_dev && !acpi_match_device_ids(acpi_dev, video_ids)) {
		dev = acpi_get_pci_dev(handle);
		if (!dev)
			return AE_OK;
		pci_dev_put(dev);
		*cap |= acpi_is_video_device(handle);
	}
	return AE_OK;
}

 
#ifdef CONFIG_X86
static bool nvidia_wmi_ec_supported(void)
{
	struct wmi_brightness_args args = {
		.mode = WMI_BRIGHTNESS_MODE_GET,
		.val = 0,
		.ret = 0,
	};
	struct acpi_buffer buf = { (acpi_size)sizeof(args), &args };
	acpi_status status;

	status = wmi_evaluate_method(WMI_BRIGHTNESS_GUID, 0,
				     WMI_BRIGHTNESS_METHOD_SOURCE, &buf, &buf);
	if (ACPI_FAILURE(status))
		return false;

	 
	return args.ret == WMI_BRIGHTNESS_SOURCE_EC;
}
#else
static bool nvidia_wmi_ec_supported(void)
{
	return false;
}
#endif

 
static int video_detect_force_vendor(const struct dmi_system_id *d)
{
	acpi_backlight_dmi = acpi_backlight_vendor;
	return 0;
}

static int video_detect_force_video(const struct dmi_system_id *d)
{
	acpi_backlight_dmi = acpi_backlight_video;
	return 0;
}

static int video_detect_force_native(const struct dmi_system_id *d)
{
	acpi_backlight_dmi = acpi_backlight_native;
	return 0;
}

static int video_detect_portege_r100(const struct dmi_system_id *d)
{
	struct pci_dev *dev;
	 
	dev = pci_get_device(PCI_VENDOR_ID_TRIDENT, 0x2100, NULL);
	if (dev)
		acpi_backlight_dmi = acpi_backlight_vendor;
	return 0;
}

static const struct dmi_system_id video_detect_dmi_table[] = {
	 
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
		DMI_MATCH(DMI_PRODUCT_NAME, "KAV80"),
		},
	},
	{
	 .callback = video_detect_force_vendor,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK Computer Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "UL30VT"),
		},
	},
	{
	 .callback = video_detect_force_vendor,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK Computer Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "UL30A"),
		},
	},
	{
	 .callback = video_detect_force_vendor,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
		DMI_MATCH(DMI_PRODUCT_NAME, "X55U"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
		DMI_MATCH(DMI_PRODUCT_NAME, "X101CH"),
		},
	},
	{
	 .callback = video_detect_force_vendor,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
		DMI_MATCH(DMI_PRODUCT_NAME, "X401U"),
		},
	},
	{
	 .callback = video_detect_force_vendor,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
		DMI_MATCH(DMI_PRODUCT_NAME, "X501U"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
		DMI_MATCH(DMI_PRODUCT_NAME, "1015CX"),
		},
	},
	{
	 .callback = video_detect_force_vendor,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		DMI_MATCH(DMI_PRODUCT_NAME, "N150/N210/N220"),
		DMI_MATCH(DMI_BOARD_NAME, "N150/N210/N220"),
		},
	},
	{
	 .callback = video_detect_force_vendor,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		DMI_MATCH(DMI_PRODUCT_NAME, "NF110/NF210/NF310"),
		DMI_MATCH(DMI_BOARD_NAME, "NF110/NF210/NF310"),
		},
	},
	{
	 .callback = video_detect_force_vendor,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		DMI_MATCH(DMI_PRODUCT_NAME, "NC210/NC110"),
		DMI_MATCH(DMI_BOARD_NAME, "NC210/NC110"),
		},
	},
	{
	 .callback = video_detect_force_vendor,
	  
	 .matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Xiaomi Inc"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Mipad2"),
		},
	},

	 
	{
	 .callback = video_detect_force_vendor,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Sony Corporation"),
		DMI_MATCH(DMI_PRODUCT_NAME, "PCG-FRV35"),
		},
	},

	 
	{
	 .callback = video_detect_force_vendor,
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
		DMI_MATCH(DMI_PRODUCT_NAME, "PORTEGE R500"),
		},
	},
	{
	 .callback = video_detect_force_vendor,
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
		DMI_MATCH(DMI_PRODUCT_NAME, "PORTEGE R600"),
		},
	},

	 
	{
	 .callback = video_detect_portege_r100,
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
		DMI_MATCH(DMI_PRODUCT_NAME, "Portable PC"),
		DMI_MATCH(DMI_PRODUCT_VERSION, "Version 1.0"),
		DMI_MATCH(DMI_BOARD_NAME, "Portable PC")
		},
	},

	 
	{
	 .callback = video_detect_force_video,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Apple Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "iMac14,1"),
		},
	},
	{
	 .callback = video_detect_force_video,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Apple Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "iMac14,2"),
		},
	},

	 
	{
	 .callback = video_detect_force_video,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
		DMI_MATCH(DMI_PRODUCT_VERSION, "ThinkPad T420"),
		},
	},
	{
	 .callback = video_detect_force_video,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
		DMI_MATCH(DMI_PRODUCT_VERSION, "ThinkPad T520"),
		},
	},
	{
	 .callback = video_detect_force_video,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
		DMI_MATCH(DMI_PRODUCT_VERSION, "ThinkPad X201s"),
		},
	},
	{
	 .callback = video_detect_force_video,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
		DMI_MATCH(DMI_PRODUCT_VERSION, "ThinkPad X201T"),
		},
	},

	 
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
		DMI_MATCH(DMI_PRODUCT_NAME, "HP ENVY 15 Notebook PC"),
		},
	},
	{
	 .callback = video_detect_force_video,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		DMI_MATCH(DMI_PRODUCT_NAME, "870Z5E/880Z5E/680Z5E"),
		},
	},
	{
	 .callback = video_detect_force_video,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		DMI_MATCH(DMI_PRODUCT_NAME,
			  "370R4E/370R4V/370R5E/3570RE/370R5V"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		DMI_MATCH(DMI_PRODUCT_NAME,
			  "3570R/370R/470R/450R/510R/4450RV"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		DMI_MATCH(DMI_PRODUCT_NAME, "670Z5E"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		DMI_MATCH(DMI_PRODUCT_NAME, "730U3E/740U3E"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		DMI_MATCH(DMI_PRODUCT_NAME,
			  "900X3C/900X3D/900X3E/900X4C/900X4D"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "XPS L421X"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "XPS L521X"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		DMI_MATCH(DMI_PRODUCT_NAME, "530U4E/540U4E"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
		DMI_MATCH(DMI_PRODUCT_NAME, "HP 635 Notebook PC"),
		},
	},

	 
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
		DMI_MATCH(DMI_BOARD_NAME, "Lenovo IdeaPad S405"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
		DMI_MATCH(DMI_PRODUCT_VERSION, "IdeaPad Z470"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
		DMI_MATCH(DMI_PRODUCT_VERSION, "Ideapad Z570"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
		DMI_MATCH(DMI_PRODUCT_NAME, "81FS"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
		DMI_MATCH(DMI_PRODUCT_NAME, "82BK"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
		DMI_MATCH(DMI_PRODUCT_NAME, "3371"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Apple Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "iMac11,3"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Apple Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "iMac12,1"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Apple Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "iMac12,2"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Apple Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "MacBookPro12,1"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "Inspiron N4010"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "Vostro V131"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "Dell System XPS L702X"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "Precision 7510"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "Studio 1569"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
		DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 3830TG"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
		DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 4810T"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
		DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5738"),
		DMI_MATCH(DMI_BOARD_NAME, "JV50"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_BOARD_VENDOR, "Acer"),
		DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5741"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_BOARD_VENDOR, "Acer"),
		DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5750"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_BOARD_VENDOR, "Acer"),
		DMI_MATCH(DMI_PRODUCT_NAME, "Extensa 5235"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_BOARD_VENDOR, "Acer"),
		DMI_MATCH(DMI_PRODUCT_NAME, "TravelMate 4750"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
		DMI_MATCH(DMI_PRODUCT_NAME, "TravelMate 5735Z"),
		DMI_MATCH(DMI_BOARD_NAME, "BA51_MV"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_BOARD_VENDOR, "Acer"),
		DMI_MATCH(DMI_PRODUCT_NAME, "TravelMate 5760"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
		DMI_MATCH(DMI_PRODUCT_NAME, "GA401"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
		DMI_MATCH(DMI_PRODUCT_NAME, "GA502"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
		DMI_MATCH(DMI_PRODUCT_NAME, "GA503"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK Computer Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "U46E"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
		DMI_MATCH(DMI_PRODUCT_NAME, "UX303UB"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
		DMI_MATCH(DMI_PRODUCT_NAME, "HP EliteBook 8460p"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
		DMI_MATCH(DMI_PRODUCT_NAME, "HP Pavilion g6 Notebook PC"),
		DMI_MATCH(DMI_PRODUCT_SKU, "B4U19UA"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		DMI_MATCH(DMI_PRODUCT_NAME, "N150P"),
		DMI_MATCH(DMI_BOARD_NAME, "N150P"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		DMI_MATCH(DMI_PRODUCT_NAME, "N145P/N250P/N260P"),
		DMI_MATCH(DMI_BOARD_NAME, "N145P/N250P/N260P"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		DMI_MATCH(DMI_PRODUCT_NAME, "N250P"),
		DMI_MATCH(DMI_BOARD_NAME, "N250P"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Sony Corporation"),
		DMI_MATCH(DMI_PRODUCT_NAME, "VPCEH3U1E"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Sony Corporation"),
		DMI_MATCH(DMI_PRODUCT_NAME, "VPCY11S1E"),
		},
	},

	 
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
		DMI_MATCH(DMI_PRODUCT_NAME, "PORTEGE R700"),
		},
	},
	{
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
		DMI_MATCH(DMI_PRODUCT_NAME, "R830"),
		},
	},
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
		DMI_MATCH(DMI_PRODUCT_NAME, "Z830"),
		},
	},

	 
	{
	 .callback = video_detect_force_native,
	  
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "Dell G15 5515"),
		},
	},
	{
	 .callback = video_detect_force_native,
	 .matches = {
		DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "Vostro 15 3535"),
		},
	},
	{ },
};

static bool google_cros_ec_present(void)
{
	return acpi_dev_found("GOOG0004") || acpi_dev_found("GOOG000C");
}

 
static bool prefer_native_over_acpi_video(void)
{
	return acpi_osi_is_win8() || google_cros_ec_present();
}

 
enum acpi_backlight_type __acpi_video_get_backlight_type(bool native, bool *auto_detect)
{
	static DEFINE_MUTEX(init_mutex);
	static bool nvidia_wmi_ec_present;
	static bool apple_gmux_present;
	static bool native_available;
	static bool init_done;
	static long video_caps;

	 
	mutex_lock(&init_mutex);
	if (!init_done) {
		acpi_video_parse_cmdline();
		dmi_check_system(video_detect_dmi_table);
		acpi_walk_namespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
				    ACPI_UINT32_MAX, find_video, NULL,
				    &video_caps, NULL);
		nvidia_wmi_ec_present = nvidia_wmi_ec_supported();
		apple_gmux_present = apple_gmux_detect(NULL, NULL);
		init_done = true;
	}
	if (native)
		native_available = true;
	mutex_unlock(&init_mutex);

	if (auto_detect)
		*auto_detect = false;

	 
	if (acpi_backlight_cmdline != acpi_backlight_undef)
		return acpi_backlight_cmdline;

	 
	if (acpi_backlight_dmi != acpi_backlight_undef)
		return acpi_backlight_dmi;

	if (auto_detect)
		*auto_detect = true;

	 
	if (nvidia_wmi_ec_present)
		return acpi_backlight_nvidia_wmi_ec;

	if (apple_gmux_present)
		return acpi_backlight_apple_gmux;

	 
	if ((video_caps & ACPI_VIDEO_BACKLIGHT) &&
	     !(native_available && prefer_native_over_acpi_video()))
		return acpi_backlight_video;

	 
	if (native_available)
		return acpi_backlight_native;

	 
	if (acpi_osi_is_win8())
		return acpi_backlight_none;

	 
	return acpi_backlight_vendor;
}
EXPORT_SYMBOL(__acpi_video_get_backlight_type);
