

 

#include <linux/types.h>
#include <linux/dmi.h>
#include <linux/mod_devicetable.h>
#include <linux/hid.h>

#include "i2c-hid.h"
#include "../hid-ids.h"


struct i2c_hid_desc_override {
	union {
		struct i2c_hid_desc *i2c_hid_desc;
		uint8_t             *i2c_hid_desc_buffer;
	};
	uint8_t              *hid_report_desc;
	unsigned int          hid_report_desc_size;
	uint8_t              *i2c_name;
};


 

static const struct i2c_hid_desc_override sipodev_desc = {
	.i2c_hid_desc_buffer = (uint8_t [])
	{0x1e, 0x00,                   
	 0x00, 0x01,                   
	 0xdb, 0x01,                   
	 0x21, 0x00,                   
	 0x24, 0x00,                   
	 0x1b, 0x00,                   
	 0x25, 0x00,                   
	 0x11, 0x00,                   
	 0x22, 0x00,                   
	 0x23, 0x00,                   
	 0x11, 0x09,                   
	 0x88, 0x52,                   
	 0x06, 0x00,                   
	 0x00, 0x00, 0x00, 0x00        
	},

	.hid_report_desc = (uint8_t [])
	{0x05, 0x01,                   
	 0x09, 0x02,                   
	 0xA1, 0x01,                   
	 0x85, 0x01,                   
	 0x09, 0x01,                   
	 0xA1, 0x00,                   
	 0x05, 0x09,                   
	 0x19, 0x01,                   
	 0x29, 0x02,                   
	 0x25, 0x01,                   
	 0x75, 0x01,                   
	 0x95, 0x02,                   
	 0x81, 0x02,                   
	 0x95, 0x06,                   
	 0x81, 0x01,                   
	 0x05, 0x01,                   
	 0x09, 0x30,                   
	 0x09, 0x31,                   
	 0x15, 0x81,                   
	 0x25, 0x7F,                   
	 0x75, 0x08,                   
	 0x95, 0x02,                   
	 0x81, 0x06,                   
	 0xC0,                         
	 0xC0,                         
	 0x05, 0x0D,                   
	 0x09, 0x05,                   
	 0xA1, 0x01,                   
	 0x85, 0x04,                   
	 0x05, 0x0D,                   
	 0x09, 0x22,                   
	 0xA1, 0x02,                   
	 0x15, 0x00,                   
	 0x25, 0x01,                   
	 0x09, 0x47,                   
	 0x09, 0x42,                   
	 0x95, 0x02,                   
	 0x75, 0x01,                   
	 0x81, 0x02,                   
	 0x95, 0x01,                   
	 0x75, 0x03,                   
	 0x25, 0x05,                   
	 0x09, 0x51,                   
	 0x81, 0x02,                   
	 0x75, 0x01,                   
	 0x95, 0x03,                   
	 0x81, 0x03,                   
	 0x05, 0x01,                   
	 0x26, 0x44, 0x0A,             
	 0x75, 0x10,                   
	 0x55, 0x0E,                   
	 0x65, 0x11,                   
	 0x09, 0x30,                   
	 0x46, 0x1A, 0x04,             
	 0x95, 0x01,                   
	 0x81, 0x02,                   
	 0x46, 0xBC, 0x02,             
	 0x26, 0x34, 0x05,             
	 0x09, 0x31,                   
	 0x81, 0x02,                   
	 0xC0,                         
	 0x05, 0x0D,                   
	 0x09, 0x22,                   
	 0xA1, 0x02,                   
	 0x25, 0x01,                   
	 0x09, 0x47,                   
	 0x09, 0x42,                   
	 0x95, 0x02,                   
	 0x75, 0x01,                   
	 0x81, 0x02,                   
	 0x95, 0x01,                   
	 0x75, 0x03,                   
	 0x25, 0x05,                   
	 0x09, 0x51,                   
	 0x81, 0x02,                   
	 0x75, 0x01,                   
	 0x95, 0x03,                   
	 0x81, 0x03,                   
	 0x05, 0x01,                   
	 0x26, 0x44, 0x0A,             
	 0x75, 0x10,                   
	 0x09, 0x30,                   
	 0x46, 0x1A, 0x04,             
	 0x95, 0x01,                   
	 0x81, 0x02,                   
	 0x46, 0xBC, 0x02,             
	 0x26, 0x34, 0x05,             
	 0x09, 0x31,                   
	 0x81, 0x02,                   
	 0xC0,                         
	 0x05, 0x0D,                   
	 0x09, 0x22,                   
	 0xA1, 0x02,                   
	 0x25, 0x01,                   
	 0x09, 0x47,                   
	 0x09, 0x42,                   
	 0x95, 0x02,                   
	 0x75, 0x01,                   
	 0x81, 0x02,                   
	 0x95, 0x01,                   
	 0x75, 0x03,                   
	 0x25, 0x05,                   
	 0x09, 0x51,                   
	 0x81, 0x02,                   
	 0x75, 0x01,                   
	 0x95, 0x03,                   
	 0x81, 0x03,                   
	 0x05, 0x01,                   
	 0x26, 0x44, 0x0A,             
	 0x75, 0x10,                   
	 0x09, 0x30,                   
	 0x46, 0x1A, 0x04,             
	 0x95, 0x01,                   
	 0x81, 0x02,                   
	 0x46, 0xBC, 0x02,             
	 0x26, 0x34, 0x05,             
	 0x09, 0x31,                   
	 0x81, 0x02,                   
	 0xC0,                         
	 0x05, 0x0D,                   
	 0x09, 0x22,                   
	 0xA1, 0x02,                   
	 0x25, 0x01,                   
	 0x09, 0x47,                   
	 0x09, 0x42,                   
	 0x95, 0x02,                   
	 0x75, 0x01,                   
	 0x81, 0x02,                   
	 0x95, 0x01,                   
	 0x75, 0x03,                   
	 0x25, 0x05,                   
	 0x09, 0x51,                   
	 0x81, 0x02,                   
	 0x75, 0x01,                   
	 0x95, 0x03,                   
	 0x81, 0x03,                   
	 0x05, 0x01,                   
	 0x26, 0x44, 0x0A,             
	 0x75, 0x10,                   
	 0x09, 0x30,                   
	 0x46, 0x1A, 0x04,             
	 0x95, 0x01,                   
	 0x81, 0x02,                   
	 0x46, 0xBC, 0x02,             
	 0x26, 0x34, 0x05,             
	 0x09, 0x31,                   
	 0x81, 0x02,                   
	 0xC0,                         
	 0x05, 0x0D,                   
	 0x55, 0x0C,                   
	 0x66, 0x01, 0x10,             
	 0x47, 0xFF, 0xFF, 0x00, 0x00, 
	 0x27, 0xFF, 0xFF, 0x00, 0x00, 
	 0x75, 0x10,                   
	 0x95, 0x01,                   
	 0x09, 0x56,                   
	 0x81, 0x02,                   
	 0x09, 0x54,                   
	 0x25, 0x7F,                   
	 0x75, 0x08,                   
	 0x81, 0x02,                   
	 0x05, 0x09,                   
	 0x09, 0x01,                   
	 0x25, 0x01,                   
	 0x75, 0x01,                   
	 0x95, 0x01,                   
	 0x81, 0x02,                   
	 0x95, 0x07,                   
	 0x81, 0x03,                   
	 0x05, 0x0D,                   
	 0x85, 0x02,                   
	 0x09, 0x55,                   
	 0x09, 0x59,                   
	 0x75, 0x04,                   
	 0x95, 0x02,                   
	 0x25, 0x0F,                   
	 0xB1, 0x02,                   
	 0x05, 0x0D,                   
	 0x85, 0x07,                   
	 0x09, 0x60,                   
	 0x75, 0x01,                   
	 0x95, 0x01,                   
	 0x25, 0x01,                   
	 0xB1, 0x02,                   
	 0x95, 0x07,                   
	 0xB1, 0x03,                   
	 0x85, 0x06,                   
	 0x06, 0x00, 0xFF,             
	 0x09, 0xC5,                   
	 0x26, 0xFF, 0x00,             
	 0x75, 0x08,                   
	 0x96, 0x00, 0x01,             
	 0xB1, 0x02,                   
	 0xC0,                         
	 0x06, 0x00, 0xFF,             
	 0x09, 0x01,                   
	 0xA1, 0x01,                   
	 0x85, 0x0D,                   
	 0x26, 0xFF, 0x00,             
	 0x19, 0x01,                   
	 0x29, 0x02,                   
	 0x75, 0x08,                   
	 0x95, 0x02,                   
	 0xB1, 0x02,                   
	 0xC0,                         
	 0x05, 0x0D,                   
	 0x09, 0x0E,                   
	 0xA1, 0x01,                   
	 0x85, 0x03,                   
	 0x09, 0x22,                   
	 0xA1, 0x02,                   
	 0x09, 0x52,                   
	 0x25, 0x0A,                   
	 0x95, 0x01,                   
	 0xB1, 0x02,                   
	 0xC0,                         
	 0x09, 0x22,                   
	 0xA1, 0x00,                   
	 0x85, 0x05,                   
	 0x09, 0x57,                   
	 0x09, 0x58,                   
	 0x75, 0x01,                   
	 0x95, 0x02,                   
	 0x25, 0x01,                   
	 0xB1, 0x02,                   
	 0x95, 0x06,                   
	 0xB1, 0x03,                   
	 0xC0,                         
	 0xC0                          
	},
	.hid_report_desc_size = 475,
	.i2c_name = "SYNA3602:00"
};


static const struct dmi_system_id i2c_hid_dmi_desc_override_table[] = {
	{
		.ident = "Teclast F6 Pro",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "TECLAST"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "F6 Pro"),
		},
		.driver_data = (void *)&sipodev_desc
	},
	{
		.ident = "Teclast F7",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "TECLAST"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "F7"),
		},
		.driver_data = (void *)&sipodev_desc
	},
	{
		.ident = "Trekstor Primebook C13",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "TREKSTOR"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Primebook C13"),
		},
		.driver_data = (void *)&sipodev_desc
	},
	{
		.ident = "Trekstor Primebook C11",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "TREKSTOR"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Primebook C11"),
		},
		.driver_data = (void *)&sipodev_desc
	},
	{
		 
		.ident = "Trekstor Primebook C11B",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "TREKSTOR"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Primebook C11B"),
		},
		.driver_data = (void *)&sipodev_desc
	},
	{
		.ident = "Trekstor SURFBOOK E11B",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "TREKSTOR"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "SURFBOOK E11B"),
		},
		.driver_data = (void *)&sipodev_desc
	},
	{
		.ident = "Direkt-Tek DTLAPY116-2",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Direkt-Tek"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "DTLAPY116-2"),
		},
		.driver_data = (void *)&sipodev_desc
	},
	{
		.ident = "Direkt-Tek DTLAPY133-1",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Direkt-Tek"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "DTLAPY133-1"),
		},
		.driver_data = (void *)&sipodev_desc
	},
	{
		.ident = "Mediacom Flexbook Edge 11",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "MEDIACOM"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "FlexBook edge11 - M-FBE11"),
		},
		.driver_data = (void *)&sipodev_desc
	},
	{
		.ident = "Mediacom FlexBook edge 13",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "MEDIACOM"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "FlexBook_edge13-M-FBE13"),
		},
		.driver_data = (void *)&sipodev_desc
	},
	{
		.ident = "Odys Winbook 13",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "AXDIA International GmbH"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "WINBOOK 13"),
		},
		.driver_data = (void *)&sipodev_desc
	},
	{
		.ident = "iBall Aer3",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "iBall"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "Aer3"),
		},
		.driver_data = (void *)&sipodev_desc
	},
	{
		.ident = "Schneider SCL142ALM",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "SCHNEIDER"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "SCL142ALM"),
		},
		.driver_data = (void *)&sipodev_desc
	},
	{
		.ident = "Vero K147",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "VERO"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "K147"),
		},
		.driver_data = (void *)&sipodev_desc
	},
	{ }	 
};

static const struct hid_device_id i2c_hid_elan_flipped_quirks = {
	HID_DEVICE(BUS_I2C, HID_GROUP_MULTITOUCH_WIN_8, USB_VENDOR_ID_ELAN, 0x2dcd),
		HID_QUIRK_X_INVERT | HID_QUIRK_Y_INVERT
};

 
static const struct dmi_system_id i2c_hid_dmi_quirk_table[] = {
	{
		.ident = "DynaBook K50/FR",
		.matches = {
			DMI_EXACT_MATCH(DMI_SYS_VENDOR, "Dynabook Inc."),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "dynabook K50/FR"),
		},
		.driver_data = (void *)&i2c_hid_elan_flipped_quirks,
	},
	{ }	 
};


struct i2c_hid_desc *i2c_hid_get_dmi_i2c_hid_desc_override(uint8_t *i2c_name)
{
	struct i2c_hid_desc_override *override;
	const struct dmi_system_id *system_id;

	system_id = dmi_first_match(i2c_hid_dmi_desc_override_table);
	if (!system_id)
		return NULL;

	override = system_id->driver_data;
	if (strcmp(override->i2c_name, i2c_name))
		return NULL;

	return override->i2c_hid_desc;
}

char *i2c_hid_get_dmi_hid_report_desc_override(uint8_t *i2c_name,
					       unsigned int *size)
{
	struct i2c_hid_desc_override *override;
	const struct dmi_system_id *system_id;

	system_id = dmi_first_match(i2c_hid_dmi_desc_override_table);
	if (!system_id)
		return NULL;

	override = system_id->driver_data;
	if (strcmp(override->i2c_name, i2c_name))
		return NULL;

	*size = override->hid_report_desc_size;
	return override->hid_report_desc;
}

u32 i2c_hid_get_dmi_quirks(const u16 vendor, const u16 product)
{
	u32 quirks = 0;
	const struct dmi_system_id *system_id =
			dmi_first_match(i2c_hid_dmi_quirk_table);

	if (system_id) {
		const struct hid_device_id *device_id =
				(struct hid_device_id *)(system_id->driver_data);

		if (device_id && device_id->vendor == vendor &&
		    device_id->product == product)
			quirks = device_id->driver_data;
	}

	return quirks;
}
