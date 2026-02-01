
 

#include <linux/device.h>
#include <linux/input.h>
#include <linux/hid.h>
#include <linux/module.h>

#include "hid-ids.h"

static int evision_input_mapping(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	if ((usage->hid & HID_USAGE_PAGE) != HID_UP_CONSUMER)
		return 0;

	 
	if ((usage->hid & HID_USAGE) >> 8 == 0x05)
		return -1;
	 
	if ((usage->hid & HID_USAGE) >> 8 == 0x06)
		return -1;

	switch (usage->hid & HID_USAGE) {
	 
	case 0x0401: return -1;
	 
	case 0x0402: return -1;
	}
	return 0;
}

static const struct hid_device_id evision_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_EVISION, USB_DEVICE_ID_EVISION_ICL01) },
	{ }
};
MODULE_DEVICE_TABLE(hid, evision_devices);

static struct hid_driver evision_driver = {
	.name = "evision",
	.id_table = evision_devices,
	.input_mapping = evision_input_mapping,
};
module_hid_driver(evision_driver);

MODULE_LICENSE("GPL");
