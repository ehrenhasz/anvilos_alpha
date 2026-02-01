
 

#define pr_fmt(fmt) "ACPI: " fmt

#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/platform_device.h>
#include <asm/cpu_device_id.h>
#include <asm/intel-family.h>
#include "../internal.h"

 
struct override_status_id {
	struct acpi_device_id hid[2];
	struct x86_cpu_id cpu_ids[2];
	struct dmi_system_id dmi_ids[2];  
	const char *uid;
	const char *path;
	unsigned long long status;
};

#define ENTRY(status, hid, uid, path, cpu_model, dmi...) {		\
	{ { hid, }, {} },						\
	{ X86_MATCH_INTEL_FAM6_MODEL(cpu_model, NULL), {} },		\
	{ { .matches = dmi }, {} },					\
	uid,								\
	path,								\
	status,								\
}

#define PRESENT_ENTRY_HID(hid, uid, cpu_model, dmi...) \
	ENTRY(ACPI_STA_DEFAULT, hid, uid, NULL, cpu_model, dmi)

#define NOT_PRESENT_ENTRY_HID(hid, uid, cpu_model, dmi...) \
	ENTRY(0, hid, uid, NULL, cpu_model, dmi)

#define PRESENT_ENTRY_PATH(path, cpu_model, dmi...) \
	ENTRY(ACPI_STA_DEFAULT, "", NULL, path, cpu_model, dmi)

#define NOT_PRESENT_ENTRY_PATH(path, cpu_model, dmi...) \
	ENTRY(0, "", NULL, path, cpu_model, dmi)

static const struct override_status_id override_status_ids[] = {
	 
	PRESENT_ENTRY_HID("80860F09", "1", ATOM_SILVERMONT, {}),
	PRESENT_ENTRY_HID("80862288", "1", ATOM_AIRMONT, {}),

	 
	PRESENT_ENTRY_HID("80862289", "2", ATOM_AIRMONT, {
		DMI_MATCH(DMI_SYS_VENDOR, "Xiaomi Inc"),
		DMI_MATCH(DMI_PRODUCT_NAME, "Mipad2"),
	      }),

	 
	PRESENT_ENTRY_HID("INT0002", "1", ATOM_AIRMONT, {}),
	 
	PRESENT_ENTRY_HID("SYNA7500", "1", HASWELL_L, {
		DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "Venue 11 Pro 7130"),
	      }),
	PRESENT_ENTRY_HID("SYNA7500", "1", HASWELL_L, {
		DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		DMI_MATCH(DMI_PRODUCT_NAME, "Venue 11 Pro 7139"),
	      }),

	 
	PRESENT_ENTRY_HID("KIOX000A", "1", ATOM_AIRMONT, {
		DMI_MATCH(DMI_BOARD_VENDOR, "AMI Corporation"),
		DMI_MATCH(DMI_BOARD_NAME, "Default string"),
		DMI_MATCH(DMI_PRODUCT_NAME, "Default string"),
		DMI_MATCH(DMI_BIOS_DATE, "02/21/2017")
	      }),
	PRESENT_ENTRY_HID("KIOX000A", "1", ATOM_AIRMONT, {
		DMI_MATCH(DMI_BOARD_VENDOR, "AMI Corporation"),
		DMI_MATCH(DMI_BOARD_NAME, "Default string"),
		DMI_MATCH(DMI_PRODUCT_NAME, "Default string"),
		DMI_MATCH(DMI_BIOS_DATE, "03/20/2017")
	      }),
	PRESENT_ENTRY_HID("KIOX000A", "1", ATOM_AIRMONT, {
		DMI_MATCH(DMI_BOARD_VENDOR, "AMI Corporation"),
		DMI_MATCH(DMI_BOARD_NAME, "Default string"),
		DMI_MATCH(DMI_PRODUCT_NAME, "Default string"),
		DMI_MATCH(DMI_BIOS_DATE, "05/25/2017")
	      }),

	 
	NOT_PRESENT_ENTRY_PATH("\\_SB_.PCI0.SDHB.BRC1", ATOM_AIRMONT, {
		DMI_EXACT_MATCH(DMI_BOARD_VENDOR, "AMI Corporation"),
		DMI_EXACT_MATCH(DMI_BOARD_NAME, "Default string"),
		DMI_EXACT_MATCH(DMI_BOARD_SERIAL, "Default string"),
		DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Default string"),
	      }),

	 
	NOT_PRESENT_ENTRY_HID("MAGN0001", "1", ATOM_SILVERMONT, {
		DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
		DMI_MATCH(DMI_PRODUCT_FAMILY, "YOGATablet2"),
	      }),
};

bool acpi_device_override_status(struct acpi_device *adev, unsigned long long *status)
{
	bool ret = false;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(override_status_ids); i++) {
		if (!x86_match_cpu(override_status_ids[i].cpu_ids))
			continue;

		if (override_status_ids[i].dmi_ids[0].matches[0].slot &&
		    !dmi_check_system(override_status_ids[i].dmi_ids))
			continue;

		if (override_status_ids[i].path) {
			struct acpi_buffer path = { ACPI_ALLOCATE_BUFFER, NULL };
			bool match;

			if (acpi_get_name(adev->handle, ACPI_FULL_PATHNAME, &path))
				continue;

			match = strcmp((char *)path.pointer, override_status_ids[i].path) == 0;
			kfree(path.pointer);

			if (!match)
				continue;
		} else {
			if (acpi_match_device_ids(adev, override_status_ids[i].hid))
				continue;

			if (!adev->pnp.unique_id ||
			    strcmp(adev->pnp.unique_id, override_status_ids[i].uid))
				continue;
		}

		*status = override_status_ids[i].status;
		ret = true;
		break;
	}

	return ret;
}

 
static const struct x86_cpu_id storage_d3_cpu_ids[] = {
	X86_MATCH_VENDOR_FAM_MODEL(AMD, 23, 24, NULL),   
	X86_MATCH_VENDOR_FAM_MODEL(AMD, 23, 96, NULL),	 
	X86_MATCH_VENDOR_FAM_MODEL(AMD, 23, 104, NULL),	 
	X86_MATCH_VENDOR_FAM_MODEL(AMD, 25, 80, NULL),	 
	{}
};

bool force_storage_d3(void)
{
	return x86_match_cpu(storage_d3_cpu_ids);
}

 
#define ACPI_QUIRK_SKIP_I2C_CLIENTS				BIT(0)
#define ACPI_QUIRK_UART1_SKIP					BIT(1)
#define ACPI_QUIRK_UART1_TTY_UART2_SKIP				BIT(2)
#define ACPI_QUIRK_SKIP_ACPI_AC_AND_BATTERY			BIT(3)
#define ACPI_QUIRK_USE_ACPI_AC_AND_BATTERY			BIT(4)
#define ACPI_QUIRK_SKIP_GPIO_EVENT_HANDLERS			BIT(5)

static const struct dmi_system_id acpi_quirk_skip_dmi_ids[] = {
	 
	{
		 
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "EF20EA"),
		},
		.driver_data = (void *)ACPI_QUIRK_USE_ACPI_AC_AND_BATTERY
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
			DMI_MATCH(DMI_PRODUCT_NAME, "80XF"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "Lenovo MIIX 320-10ICR"),
		},
		.driver_data = (void *)ACPI_QUIRK_USE_ACPI_AC_AND_BATTERY
	},

	 
#if IS_ENABLED(CONFIG_X86_ANDROID_TABLETS)
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Insyde"),
			DMI_MATCH(DMI_PRODUCT_NAME, "VESPA2"),
		},
		.driver_data = (void *)(ACPI_QUIRK_SKIP_I2C_CLIENTS |
					ACPI_QUIRK_SKIP_ACPI_AC_AND_BATTERY |
					ACPI_QUIRK_SKIP_GPIO_EVENT_HANDLERS),
	},
	{
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "ME176C"),
		},
		.driver_data = (void *)(ACPI_QUIRK_SKIP_I2C_CLIENTS |
					ACPI_QUIRK_UART1_TTY_UART2_SKIP |
					ACPI_QUIRK_SKIP_ACPI_AC_AND_BATTERY |
					ACPI_QUIRK_SKIP_GPIO_EVENT_HANDLERS),
	},
	{
		 
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Intel Corporation"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "CHERRYVIEW D1 PLATFORM"),
			DMI_EXACT_MATCH(DMI_PRODUCT_VERSION, "YETI-11"),
		},
		.driver_data = (void *)(ACPI_QUIRK_SKIP_I2C_CLIENTS |
					ACPI_QUIRK_UART1_SKIP |
					ACPI_QUIRK_SKIP_ACPI_AC_AND_BATTERY |
					ACPI_QUIRK_SKIP_GPIO_EVENT_HANDLERS),
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
			DMI_MATCH(DMI_PRODUCT_NAME, "TF103C"),
		},
		.driver_data = (void *)(ACPI_QUIRK_SKIP_I2C_CLIENTS |
					ACPI_QUIRK_SKIP_ACPI_AC_AND_BATTERY |
					ACPI_QUIRK_SKIP_GPIO_EVENT_HANDLERS),
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Intel Corp."),
			DMI_MATCH(DMI_PRODUCT_NAME, "VALLEYVIEW C0 PLATFORM"),
			DMI_MATCH(DMI_BOARD_NAME, "BYT-T FFD8"),
			 
			DMI_MATCH(DMI_BIOS_VERSION, "BLADE_21"),
		},
		.driver_data = (void *)(ACPI_QUIRK_SKIP_I2C_CLIENTS |
					ACPI_QUIRK_SKIP_ACPI_AC_AND_BATTERY),
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Intel Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "CHERRYVIEW D1 PLATFORM"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "Blade3-10A-001"),
		},
		.driver_data = (void *)(ACPI_QUIRK_SKIP_I2C_CLIENTS |
					ACPI_QUIRK_SKIP_ACPI_AC_AND_BATTERY),
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "AMI Corporation"),
			DMI_MATCH(DMI_BOARD_NAME, "Aptio CRB"),
			 
			DMI_MATCH(DMI_BIOS_DATE, "10/22/2015"),
		},
		.driver_data = (void *)(ACPI_QUIRK_SKIP_I2C_CLIENTS |
					ACPI_QUIRK_SKIP_ACPI_AC_AND_BATTERY),
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Insyde"),
			DMI_MATCH(DMI_PRODUCT_NAME, "M890BAP"),
		},
		.driver_data = (void *)(ACPI_QUIRK_SKIP_I2C_CLIENTS |
					ACPI_QUIRK_SKIP_ACPI_AC_AND_BATTERY |
					ACPI_QUIRK_SKIP_GPIO_EVENT_HANDLERS),
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Insyde"),
			DMI_MATCH(DMI_PRODUCT_NAME, "CherryTrail"),
			DMI_MATCH(DMI_BIOS_VERSION, "M882"),
		},
		.driver_data = (void *)(ACPI_QUIRK_SKIP_I2C_CLIENTS |
					ACPI_QUIRK_SKIP_ACPI_AC_AND_BATTERY),
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "AMI Corporation"),
			DMI_MATCH(DMI_BOARD_NAME, "Aptio CRB"),
			 
			DMI_MATCH(DMI_BIOS_VERSION, "ZY-8-BI-PX4S70VTR400-X423B-005-D"),
		},
		.driver_data = (void *)(ACPI_QUIRK_SKIP_I2C_CLIENTS |
					ACPI_QUIRK_SKIP_ACPI_AC_AND_BATTERY),
	},
#endif
	{}
};

#if IS_ENABLED(CONFIG_X86_ANDROID_TABLETS)
static const struct acpi_device_id i2c_acpi_known_good_ids[] = {
	{ "10EC5640", 0 },  
	{ "10EC5651", 0 },  
	{ "INT33F4", 0 },   
	{ "INT33FD", 0 },   
	{ "INT34D3", 0 },   
	{ "NPCE69A", 0 },   
	{}
};

bool acpi_quirk_skip_i2c_client_enumeration(struct acpi_device *adev)
{
	const struct dmi_system_id *dmi_id;
	long quirks;

	dmi_id = dmi_first_match(acpi_quirk_skip_dmi_ids);
	if (!dmi_id)
		return false;

	quirks = (unsigned long)dmi_id->driver_data;
	if (!(quirks & ACPI_QUIRK_SKIP_I2C_CLIENTS))
		return false;

	return acpi_match_device_ids(adev, i2c_acpi_known_good_ids);
}
EXPORT_SYMBOL_GPL(acpi_quirk_skip_i2c_client_enumeration);

int acpi_quirk_skip_serdev_enumeration(struct device *controller_parent, bool *skip)
{
	struct acpi_device *adev = ACPI_COMPANION(controller_parent);
	const struct dmi_system_id *dmi_id;
	long quirks = 0;
	u64 uid;
	int ret;

	*skip = false;

	ret = acpi_dev_uid_to_integer(adev, &uid);
	if (ret)
		return 0;

	 
	if (!dev_is_platform(controller_parent))
		return 0;

	dmi_id = dmi_first_match(acpi_quirk_skip_dmi_ids);
	if (dmi_id)
		quirks = (unsigned long)dmi_id->driver_data;

	if ((quirks & ACPI_QUIRK_UART1_SKIP) && uid == 1)
		*skip = true;

	if (quirks & ACPI_QUIRK_UART1_TTY_UART2_SKIP) {
		if (uid == 1)
			return -ENODEV;  

		if (uid == 2)
			*skip = true;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(acpi_quirk_skip_serdev_enumeration);

bool acpi_quirk_skip_gpio_event_handlers(void)
{
	const struct dmi_system_id *dmi_id;
	long quirks;

	dmi_id = dmi_first_match(acpi_quirk_skip_dmi_ids);
	if (!dmi_id)
		return false;

	quirks = (unsigned long)dmi_id->driver_data;
	return (quirks & ACPI_QUIRK_SKIP_GPIO_EVENT_HANDLERS);
}
EXPORT_SYMBOL_GPL(acpi_quirk_skip_gpio_event_handlers);
#endif

 
static const struct {
	const char *hid;
	int hrv;
} acpi_skip_ac_and_battery_pmic_ids[] = {
	{ "INT33F4", -1 },  
	{ "INT34D3",  3 },  
};

bool acpi_quirk_skip_acpi_ac_and_battery(void)
{
	const struct dmi_system_id *dmi_id;
	long quirks = 0;
	int i;

	dmi_id = dmi_first_match(acpi_quirk_skip_dmi_ids);
	if (dmi_id)
		quirks = (unsigned long)dmi_id->driver_data;

	if (quirks & ACPI_QUIRK_SKIP_ACPI_AC_AND_BATTERY)
		return true;

	if (quirks & ACPI_QUIRK_USE_ACPI_AC_AND_BATTERY)
		return false;

	for (i = 0; i < ARRAY_SIZE(acpi_skip_ac_and_battery_pmic_ids); i++) {
		if (acpi_dev_present(acpi_skip_ac_and_battery_pmic_ids[i].hid, "1",
				     acpi_skip_ac_and_battery_pmic_ids[i].hrv)) {
			pr_info_once("found native %s PMIC, skipping ACPI AC and battery devices\n",
				     acpi_skip_ac_and_battery_pmic_ids[i].hid);
			return true;
		}
	}

	return false;
}
EXPORT_SYMBOL_GPL(acpi_quirk_skip_acpi_ac_and_battery);

 
static int __init acpi_proc_quirk_set_no_mwait(const struct dmi_system_id *id)
{
	pr_notice("%s detected - disabling mwait for CPU C-states\n",
		  id->ident);
	boot_option_idle_override = IDLE_NOMWAIT;
	return 0;
}

static const struct dmi_system_id acpi_proc_quirk_mwait_dmi_table[] __initconst = {
	{
		.callback = acpi_proc_quirk_set_no_mwait,
		.ident = "Extensa 5220",
		.matches =  {
			DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "0100"),
			DMI_MATCH(DMI_BOARD_NAME, "Columbia"),
		},
		.driver_data = NULL,
	},
	{}
};

void __init acpi_proc_quirk_mwait_check(void)
{
	 
	dmi_check_system(acpi_proc_quirk_mwait_dmi_table);
}
