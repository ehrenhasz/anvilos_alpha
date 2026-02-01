
 

 
 
#define pr_fmt(fmt) "ACPI: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/platform_data/x86/apple.h>

#include "internal.h"


#define OSI_STRING_LENGTH_MAX	64
#define OSI_STRING_ENTRIES_MAX	16

struct acpi_osi_entry {
	char string[OSI_STRING_LENGTH_MAX];
	bool enable;
};

static struct acpi_osi_config {
	u8		default_disabling;
	unsigned int	linux_enable:1;
	unsigned int	linux_dmi:1;
	unsigned int	linux_cmdline:1;
	unsigned int	darwin_enable:1;
	unsigned int	darwin_dmi:1;
	unsigned int	darwin_cmdline:1;
} osi_config;

static struct acpi_osi_config osi_config;
static struct acpi_osi_entry
osi_setup_entries[OSI_STRING_ENTRIES_MAX] __initdata = {
	{"Module Device", true},
	{"Processor Device", true},
	{"3.0 _SCP Extensions", true},
	{"Processor Aggregator Device", true},
};

static u32 acpi_osi_handler(acpi_string interface, u32 supported)
{
	if (!strcmp("Linux", interface)) {
		pr_notice_once(FW_BUG
			"BIOS _OSI(Linux) query %s%s\n",
			osi_config.linux_enable ? "honored" : "ignored",
			osi_config.linux_cmdline ? " via cmdline" :
			osi_config.linux_dmi ? " via DMI" : "");
	}
	if (!strcmp("Darwin", interface)) {
		pr_notice_once(
			"BIOS _OSI(Darwin) query %s%s\n",
			osi_config.darwin_enable ? "honored" : "ignored",
			osi_config.darwin_cmdline ? " via cmdline" :
			osi_config.darwin_dmi ? " via DMI" : "");
	}

	return supported;
}

void __init acpi_osi_setup(char *str)
{
	struct acpi_osi_entry *osi;
	bool enable = true;
	int i;

	if (!acpi_gbl_create_osi_method)
		return;

	if (str == NULL || *str == '\0') {
		pr_info("_OSI method disabled\n");
		acpi_gbl_create_osi_method = FALSE;
		return;
	}

	if (*str == '!') {
		str++;
		if (*str == '\0') {
			 
			if (!osi_config.default_disabling)
				osi_config.default_disabling =
					ACPI_DISABLE_ALL_VENDOR_STRINGS;
			return;
		} else if (*str == '*') {
			osi_config.default_disabling = ACPI_DISABLE_ALL_STRINGS;
			for (i = 0; i < OSI_STRING_ENTRIES_MAX; i++) {
				osi = &osi_setup_entries[i];
				osi->enable = false;
			}
			return;
		} else if (*str == '!') {
			osi_config.default_disabling = 0;
			return;
		}
		enable = false;
	}

	for (i = 0; i < OSI_STRING_ENTRIES_MAX; i++) {
		osi = &osi_setup_entries[i];
		if (!strcmp(osi->string, str)) {
			osi->enable = enable;
			break;
		} else if (osi->string[0] == '\0') {
			osi->enable = enable;
			strncpy(osi->string, str, OSI_STRING_LENGTH_MAX);
			break;
		}
	}
}

static void __init __acpi_osi_setup_darwin(bool enable)
{
	osi_config.darwin_enable = !!enable;
	if (enable) {
		acpi_osi_setup("!");
		acpi_osi_setup("Darwin");
	} else {
		acpi_osi_setup("!!");
		acpi_osi_setup("!Darwin");
	}
}

static void __init acpi_osi_setup_darwin(bool enable)
{
	 
	osi_config.darwin_dmi = 0;
	osi_config.darwin_cmdline = 1;
	__acpi_osi_setup_darwin(enable);
}

 
static void __init __acpi_osi_setup_linux(bool enable)
{
	osi_config.linux_enable = !!enable;
	if (enable)
		acpi_osi_setup("Linux");
	else
		acpi_osi_setup("!Linux");
}

static void __init acpi_osi_setup_linux(bool enable)
{
	 
	osi_config.linux_dmi = 0;
	osi_config.linux_cmdline = 1;
	__acpi_osi_setup_linux(enable);
}

 
static void __init acpi_osi_setup_late(void)
{
	struct acpi_osi_entry *osi;
	char *str;
	int i;
	acpi_status status;

	if (osi_config.default_disabling) {
		status = acpi_update_interfaces(osi_config.default_disabling);
		if (ACPI_SUCCESS(status))
			pr_info("Disabled all _OSI OS vendors%s\n",
				osi_config.default_disabling ==
				ACPI_DISABLE_ALL_STRINGS ?
				" and feature groups" : "");
	}

	for (i = 0; i < OSI_STRING_ENTRIES_MAX; i++) {
		osi = &osi_setup_entries[i];
		str = osi->string;
		if (*str == '\0')
			break;
		if (osi->enable) {
			status = acpi_install_interface(str);
			if (ACPI_SUCCESS(status))
				pr_info("Added _OSI(%s)\n", str);
		} else {
			status = acpi_remove_interface(str);
			if (ACPI_SUCCESS(status))
				pr_info("Deleted _OSI(%s)\n", str);
		}
	}
}

static int __init osi_setup(char *str)
{
	if (str && !strcmp("Linux", str))
		acpi_osi_setup_linux(true);
	else if (str && !strcmp("!Linux", str))
		acpi_osi_setup_linux(false);
	else if (str && !strcmp("Darwin", str))
		acpi_osi_setup_darwin(true);
	else if (str && !strcmp("!Darwin", str))
		acpi_osi_setup_darwin(false);
	else
		acpi_osi_setup(str);

	return 1;
}
__setup("acpi_osi=", osi_setup);

bool acpi_osi_is_win8(void)
{
	return acpi_gbl_osi_data >= ACPI_OSI_WIN_8;
}
EXPORT_SYMBOL(acpi_osi_is_win8);

static void __init acpi_osi_dmi_darwin(void)
{
	pr_notice("DMI detected to setup _OSI(\"Darwin\"): Apple hardware\n");
	osi_config.darwin_dmi = 1;
	__acpi_osi_setup_darwin(true);
}

static void __init acpi_osi_dmi_linux(bool enable,
				      const struct dmi_system_id *d)
{
	pr_notice("DMI detected to setup _OSI(\"Linux\"): %s\n", d->ident);
	osi_config.linux_dmi = 1;
	__acpi_osi_setup_linux(enable);
}

static int __init dmi_enable_osi_linux(const struct dmi_system_id *d)
{
	acpi_osi_dmi_linux(true, d);

	return 0;
}

static int __init dmi_disable_osi_vista(const struct dmi_system_id *d)
{
	pr_notice("DMI detected: %s\n", d->ident);
	acpi_osi_setup("!Windows 2006");
	acpi_osi_setup("!Windows 2006 SP1");
	acpi_osi_setup("!Windows 2006 SP2");

	return 0;
}

static int __init dmi_disable_osi_win7(const struct dmi_system_id *d)
{
	pr_notice("DMI detected: %s\n", d->ident);
	acpi_osi_setup("!Windows 2009");

	return 0;
}

static int __init dmi_disable_osi_win8(const struct dmi_system_id *d)
{
	pr_notice("DMI detected: %s\n", d->ident);
	acpi_osi_setup("!Windows 2012");

	return 0;
}

 
static const struct dmi_system_id acpi_osi_dmi_table[] __initconst = {
	{
	.callback = dmi_disable_osi_vista,
	.ident = "Fujitsu Siemens",
	.matches = {
		     DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU SIEMENS"),
		     DMI_MATCH(DMI_PRODUCT_NAME, "ESPRIMO Mobile V5505"),
		},
	},
	{
	 
	.callback = dmi_disable_osi_vista,
	.ident = "MSI GX723",
	.matches = {
		     DMI_MATCH(DMI_SYS_VENDOR, "Micro-Star International"),
		     DMI_MATCH(DMI_PRODUCT_NAME, "GX723"),
		},
	},
	{
	.callback = dmi_disable_osi_vista,
	.ident = "Sony VGN-NS10J_S",
	.matches = {
		     DMI_MATCH(DMI_SYS_VENDOR, "Sony Corporation"),
		     DMI_MATCH(DMI_PRODUCT_NAME, "VGN-NS10J_S"),
		},
	},
	{
	.callback = dmi_disable_osi_vista,
	.ident = "Sony VGN-SR290J",
	.matches = {
		     DMI_MATCH(DMI_SYS_VENDOR, "Sony Corporation"),
		     DMI_MATCH(DMI_PRODUCT_NAME, "VGN-SR290J"),
		},
	},
	{
	.callback = dmi_disable_osi_vista,
	.ident = "VGN-NS50B_L",
	.matches = {
		     DMI_MATCH(DMI_SYS_VENDOR, "Sony Corporation"),
		     DMI_MATCH(DMI_PRODUCT_NAME, "VGN-NS50B_L"),
		},
	},
	{
	.callback = dmi_disable_osi_vista,
	.ident = "VGN-SR19XN",
	.matches = {
		     DMI_MATCH(DMI_SYS_VENDOR, "Sony Corporation"),
		     DMI_MATCH(DMI_PRODUCT_NAME, "VGN-SR19XN"),
		},
	},
	{
	.callback = dmi_disable_osi_vista,
	.ident = "Toshiba Satellite L355",
	.matches = {
		     DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
		     DMI_MATCH(DMI_PRODUCT_VERSION, "Satellite L355"),
		},
	},
	{
	.callback = dmi_disable_osi_win7,
	.ident = "ASUS K50IJ",
	.matches = {
		     DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK Computer Inc."),
		     DMI_MATCH(DMI_PRODUCT_NAME, "K50IJ"),
		},
	},
	{
	.callback = dmi_disable_osi_vista,
	.ident = "Toshiba P305D",
	.matches = {
		     DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
		     DMI_MATCH(DMI_PRODUCT_NAME, "Satellite P305D"),
		},
	},
	{
	.callback = dmi_disable_osi_vista,
	.ident = "Toshiba NB100",
	.matches = {
		     DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
		     DMI_MATCH(DMI_PRODUCT_NAME, "NB100"),
		},
	},

	 
	{
	.callback = dmi_disable_osi_win8,
	.ident = "Dell Inspiron 7737",
	.matches = {
		    DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		    DMI_MATCH(DMI_PRODUCT_NAME, "Inspiron 7737"),
		},
	},
	{
	.callback = dmi_disable_osi_win8,
	.ident = "Dell Inspiron 7537",
	.matches = {
		    DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		    DMI_MATCH(DMI_PRODUCT_NAME, "Inspiron 7537"),
		},
	},
	{
	.callback = dmi_disable_osi_win8,
	.ident = "Dell Inspiron 5437",
	.matches = {
		    DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		    DMI_MATCH(DMI_PRODUCT_NAME, "Inspiron 5437"),
		},
	},
	{
	.callback = dmi_disable_osi_win8,
	.ident = "Dell Inspiron 3437",
	.matches = {
		    DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		    DMI_MATCH(DMI_PRODUCT_NAME, "Inspiron 3437"),
		},
	},
	{
	.callback = dmi_disable_osi_win8,
	.ident = "Dell Vostro 3446",
	.matches = {
		    DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		    DMI_MATCH(DMI_PRODUCT_NAME, "Vostro 3446"),
		},
	},
	{
	.callback = dmi_disable_osi_win8,
	.ident = "Dell Vostro 3546",
	.matches = {
		    DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
		    DMI_MATCH(DMI_PRODUCT_NAME, "Vostro 3546"),
		},
	},

	 

	 
	{
	.callback = dmi_enable_osi_linux,
	.ident = "Asus EEE PC 1015PX",
	.matches = {
		     DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK Computer INC."),
		     DMI_MATCH(DMI_PRODUCT_NAME, "1015PX"),
		},
	},
	{}
};

static __init void acpi_osi_dmi_blacklisted(void)
{
	dmi_check_system(acpi_osi_dmi_table);

	 
	if (x86_apple_machine)
		acpi_osi_dmi_darwin();
}

int __init early_acpi_osi_init(void)
{
	acpi_osi_dmi_blacklisted();

	return 0;
}

int __init acpi_osi_init(void)
{
	acpi_install_interface_handler(acpi_osi_handler);
	acpi_osi_setup_late();

	return 0;
}
