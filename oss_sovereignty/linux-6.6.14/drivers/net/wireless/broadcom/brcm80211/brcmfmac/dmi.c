
 

#include <linux/dmi.h>
#include <linux/mod_devicetable.h>
#include "core.h"
#include "common.h"
#include "brcm_hw_ids.h"

 
static char dmi_board_type[128];

struct brcmf_dmi_data {
	u32 chip;
	u32 chiprev;
	const char *board_type;
};

 

static const struct brcmf_dmi_data acepc_t8_data = {
	BRCM_CC_4345_CHIP_ID, 6, "acepc-t8"
};

 
static const struct brcmf_dmi_data chuwi_hi8_pro_data = {
	BRCM_CC_43430_CHIP_ID, 0, "ilife-S806"
};

static const struct brcmf_dmi_data gpd_win_pocket_data = {
	BRCM_CC_4356_CHIP_ID, 2, "gpd-win-pocket"
};

static const struct brcmf_dmi_data jumper_ezpad_mini3_data = {
	BRCM_CC_43430_CHIP_ID, 0, "jumper-ezpad-mini3"
};

static const struct brcmf_dmi_data meegopad_t08_data = {
	BRCM_CC_43340_CHIP_ID, 2, "meegopad-t08"
};

static const struct brcmf_dmi_data pov_tab_p1006w_data = {
	BRCM_CC_43340_CHIP_ID, 2, "pov-tab-p1006w-data"
};

static const struct brcmf_dmi_data predia_basic_data = {
	BRCM_CC_43341_CHIP_ID, 2, "predia-basic"
};

 
static const struct brcmf_dmi_data voyo_winpad_a15_data = {
	BRCM_CC_4330_CHIP_ID, 4, "Prowise-PT301"
};

static const struct dmi_system_id dmi_platform_data[] = {
	{
		 
		.matches = {
			DMI_EXACT_MATCH(DMI_BOARD_VENDOR, "To be filled by O.E.M."),
			DMI_EXACT_MATCH(DMI_BOARD_NAME, "Cherry Trail CR"),
			DMI_EXACT_MATCH(DMI_PRODUCT_SKU, "T8"),
			 
			DMI_EXACT_MATCH(DMI_BIOS_VERSION, "1.000"),
		},
		.driver_data = (void *)&acepc_t8_data,
	},
	{
		 
		.matches = {
			DMI_EXACT_MATCH(DMI_BOARD_VENDOR, "To be filled by O.E.M."),
			DMI_EXACT_MATCH(DMI_BOARD_NAME, "Cherry Trail CR"),
			DMI_EXACT_MATCH(DMI_PRODUCT_SKU, "T11"),
			 
			DMI_EXACT_MATCH(DMI_BIOS_VERSION, "1.000"),
		},
		.driver_data = (void *)&acepc_t8_data,
	},
	{
		 
		.matches = {
			DMI_EXACT_MATCH(DMI_BOARD_VENDOR, "Hampoo"),
			DMI_EXACT_MATCH(DMI_BOARD_NAME, "Cherry Trail CR"),
			DMI_EXACT_MATCH(DMI_PRODUCT_SKU, "MRD"),
			 
			DMI_MATCH(DMI_BIOS_DATE, "05/10/2016"),
		},
		.driver_data = (void *)&chuwi_hi8_pro_data,
	},
	{
		 
		.matches = {
			DMI_EXACT_MATCH(DMI_BOARD_VENDOR, "Default string"),
			DMI_EXACT_MATCH(DMI_BOARD_NAME, "Cherry Trail CR"),
			DMI_EXACT_MATCH(DMI_PRODUCT_SKU, "20170531"),
		},
		 
		.driver_data = (void *)&acepc_t8_data,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "AMI Corporation"),
			DMI_MATCH(DMI_BOARD_NAME, "Default string"),
			DMI_MATCH(DMI_BOARD_SERIAL, "Default string"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Default string"),
		},
		.driver_data = (void *)&gpd_win_pocket_data,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Insyde"),
			DMI_MATCH(DMI_PRODUCT_NAME, "CherryTrail"),
			 
			DMI_MATCH(DMI_BIOS_VERSION, "jumperx.T87.KFBNEEA"),
		},
		.driver_data = (void *)&jumper_ezpad_mini3_data,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Default string"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Default string"),
			DMI_MATCH(DMI_BOARD_NAME, "T3 MRD"),
			DMI_MATCH(DMI_BOARD_VERSION, "V1.1"),
		},
		.driver_data = (void *)&meegopad_t08_data,
	},
	{
		 
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Insyde"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "BayTrail"),
			 
			DMI_EXACT_MATCH(DMI_BOARD_VENDOR, "105B"),
			DMI_EXACT_MATCH(DMI_BOARD_NAME, "0E57"),
		},
		.driver_data = (void *)&pov_tab_p1006w_data,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Insyde"),
			DMI_MATCH(DMI_PRODUCT_NAME, "CherryTrail"),
			 
			DMI_MATCH(DMI_BIOS_VERSION, "Mx.WT107.KUBNGEA"),
		},
		.driver_data = (void *)&predia_basic_data,
	},
	{
		 
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "AMI Corporation"),
			DMI_MATCH(DMI_BOARD_NAME, "Aptio CRB"),
			 
			DMI_MATCH(DMI_BIOS_DATE, "11/20/2014"),
		},
		.driver_data = (void *)&voyo_winpad_a15_data,
	},
	{}
};

void brcmf_dmi_probe(struct brcmf_mp_device *settings, u32 chip, u32 chiprev)
{
	const struct dmi_system_id *match;
	const struct brcmf_dmi_data *data;
	const char *sys_vendor;
	const char *product_name;

	 
	for (match = dmi_first_match(dmi_platform_data);
	     match;
	     match = dmi_first_match(match + 1)) {
		data = match->driver_data;

		if (data->chip == chip && data->chiprev == chiprev) {
			settings->board_type = data->board_type;
			return;
		}
	}

	 
	sys_vendor = dmi_get_system_info(DMI_SYS_VENDOR);
	product_name = dmi_get_system_info(DMI_PRODUCT_NAME);
	if (sys_vendor && product_name) {
		snprintf(dmi_board_type, sizeof(dmi_board_type), "%s-%s",
			 sys_vendor, product_name);
		settings->board_type = dmi_board_type;
	}
}
