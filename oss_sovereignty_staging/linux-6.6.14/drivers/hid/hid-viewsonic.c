
 

 

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>

#include "hid-ids.h"

 
#define PD1011_RDESC_ORIG_SIZE	408

 
static __u8 pd1011_rdesc_fixed[] = {
	0x05, 0x0D,              
	0x09, 0x01,              
	0xA1, 0x01,              
	0x85, 0x02,              
	0x09, 0x20,              
	0xA0,                    
	0x75, 0x10,              
	0x95, 0x01,              
	0xA4,                    
	0x05, 0x01,              
	0x65, 0x13,              
	0x55, 0xFD,              
	0x34,                    
	0x09, 0x30,              
	0x46, 0x5D, 0x21,        
	0x27, 0x80, 0xA9,
		0x00, 0x00,      
	0x81, 0x02,              
	0x09, 0x31,              
	0x46, 0xDA, 0x14,        
	0x26, 0xF0, 0x69,        
	0x81, 0x02,              
	0xB4,                    
	0x14,                    
	0x25, 0x01,              
	0x75, 0x01,              
	0x95, 0x01,              
	0x81, 0x03,              
	0x09, 0x32,              
	0x09, 0x42,              
	0x95, 0x02,              
	0x81, 0x02,              
	0x95, 0x05,              
	0x81, 0x03,              
	0x75, 0x10,              
	0x95, 0x01,              
	0x09, 0x30,              
	0x15, 0x05,              
	0x26, 0xFF, 0x07,        
	0x81, 0x02,              
	0x75, 0x10,              
	0x95, 0x01,              
	0x81, 0x03,              
	0xC0,                    
	0xC0                     
};

static __u8 *viewsonic_report_fixup(struct hid_device *hdev, __u8 *rdesc,
				    unsigned int *rsize)
{
	switch (hdev->product) {
	case USB_DEVICE_ID_VIEWSONIC_PD1011:
	case USB_DEVICE_ID_SIGNOTEC_VIEWSONIC_PD1011:
		if (*rsize == PD1011_RDESC_ORIG_SIZE) {
			rdesc = pd1011_rdesc_fixed;
			*rsize = sizeof(pd1011_rdesc_fixed);
		}
		break;
	}

	return rdesc;
}

static const struct hid_device_id viewsonic_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_VIEWSONIC,
				USB_DEVICE_ID_VIEWSONIC_PD1011) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_SIGNOTEC,
				USB_DEVICE_ID_SIGNOTEC_VIEWSONIC_PD1011) },
	{ }
};
MODULE_DEVICE_TABLE(hid, viewsonic_devices);

static struct hid_driver viewsonic_driver = {
	.name = "viewsonic",
	.id_table = viewsonic_devices,
	.report_fixup = viewsonic_report_fixup,
};
module_hid_driver(viewsonic_driver);

MODULE_LICENSE("GPL");
