
 

 

#include <linux/hid.h>
#include <linux/module.h>
#include <linux/usb.h>

#include "hid-ids.h"

 

static __u8 *holtek_mouse_report_fixup(struct hid_device *hdev, __u8 *rdesc,
		unsigned int *rsize)
{
	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);

	if (intf->cur_altsetting->desc.bInterfaceNumber == 1) {
		 
		switch (hdev->product) {
		case USB_DEVICE_ID_HOLTEK_ALT_MOUSE_A067:
		case USB_DEVICE_ID_HOLTEK_ALT_MOUSE_A072:
		case USB_DEVICE_ID_HOLTEK_ALT_MOUSE_A0C2:
			if (*rsize >= 122 && rdesc[115] == 0xff && rdesc[116] == 0x7f
					&& rdesc[120] == 0xff && rdesc[121] == 0x7f) {
				hid_info(hdev, "Fixing up report descriptor\n");
				rdesc[116] = rdesc[121] = 0x2f;
			}
			break;
		case USB_DEVICE_ID_HOLTEK_ALT_MOUSE_A04A:
		case USB_DEVICE_ID_HOLTEK_ALT_MOUSE_A070:
		case USB_DEVICE_ID_HOLTEK_ALT_MOUSE_A081:
			if (*rsize >= 113 && rdesc[106] == 0xff && rdesc[107] == 0x7f
					&& rdesc[111] == 0xff && rdesc[112] == 0x7f) {
				hid_info(hdev, "Fixing up report descriptor\n");
				rdesc[107] = rdesc[112] = 0x2f;
			}
			break;
		}

	}
	return rdesc;
}

static int holtek_mouse_probe(struct hid_device *hdev,
			      const struct hid_device_id *id)
{
	int ret;

	if (!hid_is_usb(hdev))
		return -EINVAL;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "hid parse failed: %d\n", ret);
		return ret;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "hw start failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct hid_device_id holtek_mouse_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_HOLTEK_ALT,
			USB_DEVICE_ID_HOLTEK_ALT_MOUSE_A067) },
        { HID_USB_DEVICE(USB_VENDOR_ID_HOLTEK_ALT,
			USB_DEVICE_ID_HOLTEK_ALT_MOUSE_A070) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_HOLTEK_ALT,
			USB_DEVICE_ID_HOLTEK_ALT_MOUSE_A04A) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_HOLTEK_ALT,
			USB_DEVICE_ID_HOLTEK_ALT_MOUSE_A072) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_HOLTEK_ALT,
			USB_DEVICE_ID_HOLTEK_ALT_MOUSE_A081) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_HOLTEK_ALT,
			USB_DEVICE_ID_HOLTEK_ALT_MOUSE_A0C2) },
	{ }
};
MODULE_DEVICE_TABLE(hid, holtek_mouse_devices);

static struct hid_driver holtek_mouse_driver = {
	.name = "holtek_mouse",
	.id_table = holtek_mouse_devices,
	.report_fixup = holtek_mouse_report_fixup,
	.probe = holtek_mouse_probe,
};

module_hid_driver(holtek_mouse_driver);
MODULE_LICENSE("GPL");
