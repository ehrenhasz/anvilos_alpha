
 

#include <linux/dmi.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>

#include "x86-android-tablets.h"

const struct dmi_system_id x86_android_tablet_ids[] __initconst = {
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Insyde"),
			DMI_MATCH(DMI_PRODUCT_NAME, "VESPA2"),
		},
		.driver_data = (void *)&acer_b1_750_info,
	},
	{
		 
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Advantech"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "MICA-071"),
		},
		.driver_data = (void *)&advantech_mica_071_info,
	},
	{
		 
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "ME176C"),
		},
		.driver_data = (void *)&asus_me176c_info,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
			DMI_MATCH(DMI_PRODUCT_NAME, "TF103C"),
		},
		.driver_data = (void *)&asus_tf103c_info,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "Hampoo"),
			DMI_MATCH(DMI_BOARD_NAME, "BYT-PA03C"),
			DMI_MATCH(DMI_SYS_VENDOR, "ilife"),
			DMI_MATCH(DMI_PRODUCT_NAME, "S806"),
		},
		.driver_data = (void *)&chuwi_hi8_info,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "Default string"),
			DMI_MATCH(DMI_BOARD_NAME, "Cherry Trail CR"),
			 
			DMI_MATCH(DMI_PRODUCT_SKU, "20170531"),
			DMI_MATCH(DMI_BIOS_DATE, "07/12/2017"),
		},
		.driver_data = (void *)&cyberbook_t116_info,
	},
	{
		 
		.ident = "CZC ODEON TPC-10 (\"P10T\")",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "CZC"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ODEON*TPC-10"),
		},
		.driver_data = (void *)&czc_p10t,
	},
	{
		 
		.ident = "ViewSonic ViewPad 10",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ViewSonic"),
			DMI_MATCH(DMI_PRODUCT_NAME, "VPAD10"),
		},
		.driver_data = (void *)&czc_p10t,
	},
	{
		 
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Intel Corporation"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "CHERRYVIEW D1 PLATFORM"),
			DMI_EXACT_MATCH(DMI_PRODUCT_VERSION, "YETI-11"),
		},
		.driver_data = (void *)&lenovo_yogabook_x90_info,
	},
	{
		 
		.matches = {
			 
			DMI_MATCH(DMI_PRODUCT_NAME, "Lenovo YB1-X91"),
		},
		.driver_data = (void *)&lenovo_yogabook_x91_info,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Intel Corp."),
			DMI_MATCH(DMI_PRODUCT_NAME, "VALLEYVIEW C0 PLATFORM"),
			DMI_MATCH(DMI_BOARD_NAME, "BYT-T FFD8"),
			 
			DMI_MATCH(DMI_BIOS_VERSION, "BLADE_21"),
		},
		.driver_data = (void *)&lenovo_yoga_tab2_830_1050_info,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Intel Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "CHERRYVIEW D1 PLATFORM"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "Blade3-10A-001"),
		},
		.driver_data = (void *)&lenovo_yt3_info,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "AMI Corporation"),
			DMI_MATCH(DMI_BOARD_NAME, "Aptio CRB"),
			 
			DMI_MATCH(DMI_BIOS_DATE, "10/22/2015"),
		},
		.driver_data = (void *)&medion_lifetab_s10346_info,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Insyde"),
			DMI_MATCH(DMI_PRODUCT_NAME, "M890BAP"),
		},
		.driver_data = (void *)&nextbook_ares8_info,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Insyde"),
			DMI_MATCH(DMI_PRODUCT_NAME, "CherryTrail"),
			DMI_MATCH(DMI_BIOS_VERSION, "M882"),
		},
		.driver_data = (void *)&nextbook_ares8a_info,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "PEAQ"),
			DMI_MATCH(DMI_PRODUCT_NAME, "PEAQ PMM C1010 MD99187"),
		},
		.driver_data = (void *)&peaq_c1010_info,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "AMI Corporation"),
			DMI_MATCH(DMI_BOARD_NAME, "Aptio CRB"),
			 
			DMI_MATCH(DMI_BIOS_VERSION, "ZY-8-BI-PX4S70VTR400-X423B-005-D"),
		},
		.driver_data = (void *)&whitelabel_tm800a550l_info,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Xiaomi Inc"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Mipad2"),
		},
		.driver_data = (void *)&xiaomi_mipad2_info,
	},
	{ }
};
MODULE_DEVICE_TABLE(dmi, x86_android_tablet_ids);
