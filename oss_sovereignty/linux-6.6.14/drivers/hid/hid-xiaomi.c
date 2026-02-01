
 

#include <linux/init.h>
#include <linux/module.h>
#include <linux/hid.h>

#include "hid-ids.h"

 
 
#define MI_SILENT_MOUSE_ORIG_RDESC_LENGTH   87
static __u8 mi_silent_mouse_rdesc_fixed[] = {
	0x05, 0x01,          
	0x09, 0x02,          
	0xA1, 0x01,          
	0x85, 0x03,          
	0x09, 0x01,          
	0xA1, 0x00,          
	0x05, 0x09,          
	0x19, 0x01,          
	0x29, 0x05,    
	0x15, 0x00,          
	0x25, 0x01,          
	0x75, 0x01,          
	0x95, 0x05,          
	0x81, 0x02,          
	0x75, 0x03,          
	0x95, 0x01,          
	0x81, 0x01,          
	0x05, 0x01,          
	0x09, 0x30,          
	0x09, 0x31,          
	0x15, 0x81,          
	0x25, 0x7F,          
	0x75, 0x08,          
	0x95, 0x02,          
	0x81, 0x06,          
	0x09, 0x38,          
	0x15, 0x81,          
	0x25, 0x7F,          
	0x75, 0x08,          
	0x95, 0x01,          
	0x81, 0x06,          
	0xC0,                
	0xC0,                
	0x06, 0x01, 0xFF,    
	0x09, 0x01,          
	0xA1, 0x01,          
	0x85, 0x05,          
	0x09, 0x05,          
	0x15, 0x00,          
	0x26, 0xFF, 0x00,    
	0x75, 0x08,          
	0x95, 0x04,          
	0xB1, 0x02,          
	0xC0                 
};

static __u8 *xiaomi_report_fixup(struct hid_device *hdev, __u8 *rdesc,
				 unsigned int *rsize)
{
	switch (hdev->product) {
	case USB_DEVICE_ID_MI_SILENT_MOUSE:
		if (*rsize == MI_SILENT_MOUSE_ORIG_RDESC_LENGTH) {
			hid_info(hdev, "fixing up Mi Silent Mouse report descriptor\n");
			rdesc = mi_silent_mouse_rdesc_fixed;
			*rsize = sizeof(mi_silent_mouse_rdesc_fixed);
		}
		break;
	}
	return rdesc;
}

static const struct hid_device_id xiaomi_devices[] = {
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_XIAOMI, USB_DEVICE_ID_MI_SILENT_MOUSE) },
	{ }
};
MODULE_DEVICE_TABLE(hid, xiaomi_devices);

static struct hid_driver xiaomi_driver = {
	.name = "xiaomi",
	.id_table = xiaomi_devices,
	.report_fixup = xiaomi_report_fixup,
};
module_hid_driver(xiaomi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ilya Skriblovsky <IlyaSkriblovsky@gmail.com>");
MODULE_DESCRIPTION("Fixing side buttons of Xiaomi Mi Silent Mouse");
