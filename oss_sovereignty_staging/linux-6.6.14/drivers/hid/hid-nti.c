
 

 

#include <linux/device.h>
#include <linux/input.h>
#include <linux/hid.h>
#include <linux/module.h>

#include "hid-ids.h"

MODULE_AUTHOR("Jonathan Klabunde Tomer <jktomer@google.com>");
MODULE_DESCRIPTION("HID driver for Network Technologies USB-SUN keyboard adapter");

 
static __u8 *nti_usbsun_report_fixup(struct hid_device *hdev, __u8 *rdesc,
		unsigned int *rsize)
{
	if (*rsize >= 60 && rdesc[53] == 0x65 && rdesc[59] == 0x65) {
		hid_info(hdev, "fixing up NTI USB-SUN keyboard adapter report descriptor\n");
		rdesc[53] = rdesc[59] = 0xe7;
	}
	return rdesc;
}

static const struct hid_device_id nti_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_NTI, USB_DEVICE_ID_USB_SUN) },
	{ }
};
MODULE_DEVICE_TABLE(hid, nti_devices);

static struct hid_driver nti_driver = {
	.name = "nti",
	.id_table = nti_devices,
	.report_fixup = nti_usbsun_report_fixup
};

module_hid_driver(nti_driver);

MODULE_LICENSE("GPL");
