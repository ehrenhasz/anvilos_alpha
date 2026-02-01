
 

 

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>

#include "hid-ids.h"

#define GEMBIRD_START_FAULTY_RDESC	8

static const __u8 gembird_jpd_faulty_rdesc[] = {
	0x75, 0x08,			 
	0x95, 0x05,			 
	0x15, 0x00,			 
	0x26, 0xff, 0x00,		 
	0x35, 0x00,			 
	0x46, 0xff, 0x00,		 
	0x09, 0x30,			 
	0x09, 0x31,			 
	0x09, 0x32,			 
	0x09, 0x32,			 
	0x09, 0x35,			 
	0x81, 0x02,			 
};

 
static const __u8 gembird_jpd_fixed_rdesc[] = {
	0x75, 0x08,			 
	0x95, 0x02,			 
	0x15, 0x00,			 
	0x26, 0xff, 0x00,		 
	0x35, 0x00,			 
	0x46, 0xff, 0x00,		 
	0x09, 0x30,			 
	0x09, 0x31,			 
	0x81, 0x02,			 
	0x95, 0x01,			 
	0x09, 0x32,			 
	0x81, 0x01,			 
	0x95, 0x02,			 
	0x09, 0x33,			 
	0x09, 0x34,			 
	0x81, 0x02,			 
};

static __u8 *gembird_report_fixup(struct hid_device *hdev, __u8 *rdesc,
		unsigned int *rsize)
{
	__u8 *new_rdesc;
	 
	size_t delta_size = sizeof(gembird_jpd_fixed_rdesc) -
			    sizeof(gembird_jpd_faulty_rdesc);
	size_t new_size = *rsize + delta_size;

	if (*rsize >= 31 && !memcmp(&rdesc[GEMBIRD_START_FAULTY_RDESC],
				    gembird_jpd_faulty_rdesc,
				    sizeof(gembird_jpd_faulty_rdesc))) {
		new_rdesc = devm_kzalloc(&hdev->dev, new_size, GFP_KERNEL);
		if (new_rdesc == NULL)
			return rdesc;

		dev_info(&hdev->dev,
			 "fixing Gembird JPD-DualForce 2 report descriptor.\n");

		 
		memcpy(new_rdesc + delta_size, rdesc, *rsize);

		 
		memcpy(new_rdesc, rdesc, GEMBIRD_START_FAULTY_RDESC);

		 
		memcpy(new_rdesc + GEMBIRD_START_FAULTY_RDESC,
		       gembird_jpd_fixed_rdesc,
		       sizeof(gembird_jpd_fixed_rdesc));

		*rsize = new_size;
		rdesc = new_rdesc;
	}

	return rdesc;
}

static const struct hid_device_id gembird_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_GEMBIRD,
			 USB_DEVICE_ID_GEMBIRD_JPD_DUALFORCE2) },
	{ }
};
MODULE_DEVICE_TABLE(hid, gembird_devices);

static struct hid_driver gembird_driver = {
	.name = "gembird",
	.id_table = gembird_devices,
	.report_fixup = gembird_report_fixup,
};
module_hid_driver(gembird_driver);

MODULE_AUTHOR("Benjamin Tissoires <benjamin.tissoires@gmail.com>");
MODULE_DESCRIPTION("HID Gembird joypad driver");
MODULE_LICENSE("GPL");
