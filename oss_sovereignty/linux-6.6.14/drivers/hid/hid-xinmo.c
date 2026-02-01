
 

 

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "hid-ids.h"

 
static int xinmo_event(struct hid_device *hdev, struct hid_field *field,
		struct hid_usage *usage, __s32 value)
{
	switch (usage->code) {
	case ABS_X:
	case ABS_Y:
	case ABS_Z:
	case ABS_RX:
		if (value < -1) {
			input_event(field->hidinput->input, usage->type,
				usage->code, -1);
			return 1;
		}
		break;
	}

	return 0;
}

static const struct hid_device_id xinmo_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_XIN_MO, USB_DEVICE_ID_XIN_MO_DUAL_ARCADE) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_XIN_MO, USB_DEVICE_ID_THT_2P_ARCADE) },
	{ }
};

MODULE_DEVICE_TABLE(hid, xinmo_devices);

static struct hid_driver xinmo_driver = {
	.name = "xinmo",
	.id_table = xinmo_devices,
	.event = xinmo_event
};

module_hid_driver(xinmo_driver);
MODULE_LICENSE("GPL");
