
 
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>

#include "hid-ids.h"

static __u8 *aureal_report_fixup(struct hid_device *hdev, __u8 *rdesc,
		unsigned int *rsize)
{
	if (*rsize >= 54 && rdesc[52] == 0x25 && rdesc[53] == 0x01) {
		dev_info(&hdev->dev, "fixing Aureal Cy se W-01RN USB_V3.1 report descriptor.\n");
		rdesc[53] = 0x65;
	}
	return rdesc;
}

static const struct hid_device_id aureal_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_AUREAL, USB_DEVICE_ID_AUREAL_W01RN) },
	{ }
};
MODULE_DEVICE_TABLE(hid, aureal_devices);

static struct hid_driver aureal_driver = {
	.name = "aureal",
	.id_table = aureal_devices,
	.report_fixup = aureal_report_fixup,
};
module_hid_driver(aureal_driver);

MODULE_LICENSE("GPL");
