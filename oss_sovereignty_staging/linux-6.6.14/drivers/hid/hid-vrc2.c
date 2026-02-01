
 

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>

 
#define USB_VENDOR_ID_VRC2	(0x07c0)
#define USB_DEVICE_ID_VRC2	(0x1125)

static __u8 vrc2_rdesc_fixed[] = {
	0x05, 0x01,        
	0x09, 0x04,        
	0xA1, 0x01,        
	0x09, 0x01,        
	0xA1, 0x00,        
	0x09, 0x30,        
	0x09, 0x31,        
	0x15, 0x00,        
	0x26, 0xFF, 0x07,  
	0x35, 0x00,        
	0x46, 0xFF, 0x00,  
	0x75, 0x10,        
	0x95, 0x02,        
	0x81, 0x02,        
	0xC0,              
	0x75, 0x08,        
	0x95, 0x03,        
	0x81, 0x03,        
	0xC0,              
};

static __u8 *vrc2_report_fixup(struct hid_device *hdev, __u8 *rdesc,
				unsigned int *rsize)
{
	hid_info(hdev, "fixing up VRC-2 report descriptor\n");
	*rsize = sizeof(vrc2_rdesc_fixed);
	return vrc2_rdesc_fixed;
}

static int vrc2_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;

	 
	if (hdev->dev_rsize == 23)
		return -ENODEV;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		return ret;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		return ret;
	}

	return 0;
}

static const struct hid_device_id vrc2_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_VRC2, USB_DEVICE_ID_VRC2) },
	{   }
};
MODULE_DEVICE_TABLE(hid, vrc2_devices);

static struct hid_driver vrc2_driver = {
	.name = "vrc2",
	.id_table = vrc2_devices,
	.report_fixup = vrc2_report_fixup,
	.probe = vrc2_probe,
};
module_hid_driver(vrc2_driver);

MODULE_AUTHOR("Marcus Folkesson <marcus.folkesson@gmail.com>");
MODULE_DESCRIPTION("HID driver for VRC-2 2-axis Car controller");
MODULE_LICENSE("GPL");
