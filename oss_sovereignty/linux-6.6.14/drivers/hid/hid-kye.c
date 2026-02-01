
 

#include <asm-generic/unaligned.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>

#include "hid-ids.h"

 

static const __u8 easypen_m406_control_rdesc[] = {
	0x05, 0x0C,         
	0x09, 0x01,         
	0xA1, 0x01,         
	0x85, 0x12,         
	0x0A, 0x45, 0x02,   
	0x09, 0x40,         
	0x0A, 0x2F, 0x02,   
	0x0A, 0x46, 0x02,   
	0x0A, 0x1A, 0x02,   
	0x0A, 0x6A, 0x02,   
	0x0A, 0x24, 0x02,   
	0x0A, 0x25, 0x02,   
	0x14,               
	0x25, 0x01,         
	0x75, 0x01,         
	0x95, 0x08,         
	0x81, 0x02,         
	0x95, 0x30,         
	0x81, 0x01,         
	0xC0                
};

static const __u8 easypen_m506_control_rdesc[] = {
	0x05, 0x0C,         
	0x09, 0x01,         
	0xA1, 0x01,         
	0x85, 0x12,         
	0x0A, 0x6A, 0x02,   
	0x0A, 0x1A, 0x02,   
	0x0A, 0x2D, 0x02,   
	0x0A, 0x2E, 0x02,   
	0x14,               
	0x25, 0x01,         
	0x75, 0x01,         
	0x95, 0x04,         
	0x81, 0x02,         
	0x95, 0x34,         
	0x81, 0x01,         
	0xC0                
};

static const __u8 easypen_m406w_control_rdesc[] = {
	0x05, 0x0C,         
	0x09, 0x01,         
	0xA1, 0x01,         
	0x85, 0x12,         
	0x0A, 0x6A, 0x02,   
	0x0A, 0x1A, 0x02,   
	0x0A, 0x01, 0x02,   
	0x09, 0x40,         
	0x14,               
	0x25, 0x01,         
	0x75, 0x01,         
	0x95, 0x04,         
	0x81, 0x02,         
	0x95, 0x34,         
	0x81, 0x01,         
	0xC0                
};

static const __u8 easypen_m610x_control_rdesc[] = {
	0x05, 0x0C,         
	0x09, 0x01,         
	0xA1, 0x01,         
	0x85, 0x12,         
	0x0A, 0x1A, 0x02,   
	0x0A, 0x79, 0x02,   
	0x0A, 0x2D, 0x02,   
	0x0A, 0x2E, 0x02,   
	0x14,               
	0x25, 0x01,         
	0x75, 0x01,         
	0x95, 0x04,         
	0x81, 0x02,         
	0x95, 0x34,         
	0x81, 0x01,         
	0xC0                
};

static const __u8 pensketch_m912_control_rdesc[] = {
	0x05, 0x0C,         
	0x09, 0x01,         
	0xA1, 0x01,         
	0x85, 0x12,         
	0x14,               
	0x25, 0x01,         
	0x75, 0x01,         
	0x95, 0x08,         
	0x05, 0x0C,         
	0x0A, 0x6A, 0x02,   
	0x0A, 0x1A, 0x02,   
	0x0A, 0x01, 0x02,   
	0x0A, 0x2F, 0x02,   
	0x0A, 0x25, 0x02,   
	0x0A, 0x24, 0x02,   
	0x0A, 0x2D, 0x02,   
	0x0A, 0x2E, 0x02,   
	0x81, 0x02,         
	0x95, 0x30,         
	0x81, 0x03,         
	0xC0                
};

static const __u8 mousepen_m508wx_control_rdesc[] = {
	0x05, 0x0C,         
	0x09, 0x01,         
	0xA1, 0x01,         
	0x85, 0x12,         
	0x0A, 0x1A, 0x02,   
	0x0A, 0x6A, 0x02,   
	0x0A, 0x2D, 0x02,   
	0x0A, 0x2E, 0x02,   
	0x14,               
	0x25, 0x01,         
	0x75, 0x01,         
	0x95, 0x04,         
	0x81, 0x02,         
	0x95, 0x34,         
	0x81, 0x01,         
	0xC0                
};

static const __u8 mousepen_m508x_control_rdesc[] = {
	0x05, 0x0C,         
	0x09, 0x01,         
	0xA1, 0x01,         
	0x85, 0x12,         
	0x0A, 0x01, 0x02,   
	0x09, 0x40,         
	0x0A, 0x6A, 0x02,   
	0x0A, 0x1A, 0x02,   
	0x14,               
	0x25, 0x01,         
	0x75, 0x01,         
	0x95, 0x04,         
	0x81, 0x02,         
	0x81, 0x01,         
	0x15, 0xFF,         
	0x95, 0x10,         
	0x81, 0x01,         
	0x0A, 0x35, 0x02,   
	0x0A, 0x2F, 0x02,   
	0x0A, 0x38, 0x02,   
	0x75, 0x08,         
	0x95, 0x03,         
	0x81, 0x06,         
	0x95, 0x01,         
	0x81, 0x01,         
	0xC0                
};

static const __u8 easypen_m406xe_control_rdesc[] = {
	0x05, 0x0C,         
	0x09, 0x01,         
	0xA1, 0x01,         
	0x85, 0x12,         
	0x14,               
	0x25, 0x01,         
	0x75, 0x01,         
	0x95, 0x04,         
	0x0A, 0x79, 0x02,   
	0x0A, 0x1A, 0x02,   
	0x0A, 0x2D, 0x02,   
	0x0A, 0x2E, 0x02,   
	0x81, 0x02,         
	0x95, 0x34,         
	0x81, 0x03,         
	0xC0                
};

static const __u8 pensketch_t609a_control_rdesc[] = {
	0x05, 0x0C,         
	0x09, 0x01,         
	0xA1, 0x01,         
	0x85, 0x12,         
	0x0A, 0x6A, 0x02,   
	0x14,               
	0x25, 0x01,         
	0x75, 0x01,         
	0x95, 0x08,         
	0x81, 0x02,         
	0x95, 0x37,         
	0x81, 0x01,         
	0xC0                
};

 
static const __u8 kye_tablet_rdesc[] = {
	0x06, 0x00, 0xFF,              
	0x09, 0x01,                    
	0xA1, 0x01,                    
	0x85, 0x05,                    
	0x09, 0x01,                    
	0x15, 0x81,                    
	0x25, 0x7F,                    
	0x75, 0x08,                    
	0x95, 0x07,                    
	0xB1, 0x02,                    
	0xC0,                          
	0x05, 0x0D,                    
	0x09, 0x01,                    
	0xA1, 0x01,                    
	0x85, 0x10,                    
	0x09, 0x20,                    
	0xA0,                          
	0x09, 0x42,                    
	0x09, 0x44,                    
	0x09, 0x46,                    
	0x14,                          
	0x25, 0x01,                    
	0x75, 0x01,                    
	0x95, 0x03,                    
	0x81, 0x02,                    
	0x95, 0x04,                    
	0x81, 0x01,                    
	0x09, 0x32,                    
	0x95, 0x01,                    
	0x81, 0x02,                    
	0x75, 0x10,                    
	0xA4,                          
	0x05, 0x01,                    
	0x09, 0x30,                    
	0x27, 0xFF, 0x7F, 0x00, 0x00,  
	0x34,                          
	0x47, 0x00, 0x00, 0x00, 0x00,  
	0x65, 0x11,                    
	0x55, 0x00,                    
	0x75, 0x10,                    
	0x81, 0x02,                    
	0x09, 0x31,                    
	0x27, 0xFF, 0x7F, 0x00, 0x00,  
	0x47, 0x00, 0x00, 0x00, 0x00,  
	0x81, 0x02,                    
	0xB4,                          
	0x05, 0x0D,                    
	0x09, 0x30,                    
	0x27, 0xFF, 0x07, 0x00, 0x00,  
	0x81, 0x02,                    
	0xC0,                          
	0xC0,                          
	0x05, 0x0D,                    
	0x09, 0x21,                    
	0xA1, 0x01,                    
	0x85, 0x11,                    
	0x09, 0x21,                    
	0xA0,                          
	0x05, 0x09,                    
	0x19, 0x01,                    
	0x29, 0x03,                    
	0x14,                          
	0x25, 0x01,                    
	0x75, 0x01,                    
	0x95, 0x03,                    
	0x81, 0x02,                    
	0x95, 0x04,                    
	0x81, 0x01,                    
	0x05, 0x0D,                    
	0x09, 0x32,                    
	0x95, 0x01,                    
	0x81, 0x02,                    
	0x05, 0x01,                    
	0xA4,                          
	0x09, 0x30,                    
	0x27, 0xFF, 0x7F, 0x00, 0x00,  
	0x34,                          
	0x47, 0x00, 0x00, 0x00, 0x00,  
	0x65, 0x11,                    
	0x55, 0x00,                    
	0x75, 0x10,                    
	0x81, 0x02,                    
	0x09, 0x31,                    
	0x27, 0xFF, 0x7F, 0x00, 0x00,  
	0x47, 0x00, 0x00, 0x00, 0x00,  
	0x81, 0x02,                    
	0xB4,                          
	0x09, 0x38,                    
	0x15, 0xFF,                    
	0x75, 0x08,                    
	0x95, 0x01,                    
	0x81, 0x06,                    
	0x81, 0x01,                    
	0xC0,                          
	0xC0                           
};

static const struct kye_tablet_info {
	__u32 product;
	__s32 x_logical_maximum;
	__s32 y_logical_maximum;
	__s32 pressure_logical_maximum;
	__s32 x_physical_maximum;
	__s32 y_physical_maximum;
	__s8 unit_exponent;
	__s8 unit;
	bool has_punk;
	unsigned int control_rsize;
	const __u8 *control_rdesc;
} kye_tablets_info[] = {
	{USB_DEVICE_ID_KYE_EASYPEN_M406,   
		15360, 10240, 1023,    6,   4,  0, 0x13, false,
		sizeof(easypen_m406_control_rdesc), easypen_m406_control_rdesc},
	{USB_DEVICE_ID_KYE_EASYPEN_M506,   
		24576, 20480, 1023,    6,   5,  0, 0x13, false,
		sizeof(easypen_m506_control_rdesc), easypen_m506_control_rdesc},
	{USB_DEVICE_ID_KYE_EASYPEN_I405X,   
		14080, 10240, 1023,   55,  40, -1, 0x13, false},
	{USB_DEVICE_ID_KYE_MOUSEPEN_I608X,   
		20480, 15360, 2047,    8,   6,  0, 0x13,  true},
	{USB_DEVICE_ID_KYE_EASYPEN_M406W,   
		15360, 10240, 1023,    6,   4,  0, 0x13, false,
		sizeof(easypen_m406w_control_rdesc), easypen_m406w_control_rdesc},
	{USB_DEVICE_ID_KYE_EASYPEN_M610X,   
		40960, 25600, 1023, 1000, 625, -2, 0x13, false,
		sizeof(easypen_m610x_control_rdesc), easypen_m610x_control_rdesc},
	{USB_DEVICE_ID_KYE_EASYPEN_340,   
		10240,  7680, 1023,    4,   3,  0, 0x13, false},
	{USB_DEVICE_ID_KYE_PENSKETCH_M912,   
		61440, 46080, 2047,   12,   9,  0, 0x13,  true,
		sizeof(pensketch_m912_control_rdesc), pensketch_m912_control_rdesc},
	{USB_DEVICE_ID_KYE_MOUSEPEN_M508WX,   
		40960, 25600, 2047,    8,   5,  0, 0x13,  true,
		sizeof(mousepen_m508wx_control_rdesc), mousepen_m508wx_control_rdesc},
	{USB_DEVICE_ID_KYE_MOUSEPEN_M508X,   
		40960, 25600, 2047,    8,   5,  0, 0x13,  true,
		sizeof(mousepen_m508x_control_rdesc), mousepen_m508x_control_rdesc},
	{USB_DEVICE_ID_KYE_EASYPEN_M406XE,   
		15360, 10240, 1023,    6,   4,  0, 0x13, false,
		sizeof(easypen_m406xe_control_rdesc), easypen_m406xe_control_rdesc},
	{USB_DEVICE_ID_KYE_MOUSEPEN_I608X_V2,   
		40960, 30720, 2047,    8,   6,  0, 0x13,  true},
	{USB_DEVICE_ID_KYE_PENSKETCH_T609A,   
		43520, 28160, 1023,   85,  55, -1, 0x13, false,
		sizeof(pensketch_t609a_control_rdesc), pensketch_t609a_control_rdesc},
	{}
};

static __u8 *kye_consumer_control_fixup(struct hid_device *hdev, __u8 *rdesc,
		unsigned int *rsize, int offset, const char *device_name)
{
	 
	if (*rsize >= offset + 31 &&
	     
	    rdesc[offset] == 0x05 && rdesc[offset + 1] == 0x0c &&
	     
	    rdesc[offset + 2] == 0x09 && rdesc[offset + 3] == 0x01 &&
	     
	    rdesc[offset + 10] == 0x2a && rdesc[offset + 12] > 0x2f) {
		hid_info(hdev, "fixing up %s report descriptor\n", device_name);
		rdesc[offset + 12] = 0x2f;
	}
	return rdesc;
}

 
static __u8 *kye_tablet_fixup(struct hid_device *hdev, __u8 *rdesc, unsigned int *rsize)
{
	const struct kye_tablet_info *info;
	unsigned int newsize;

	if (*rsize < sizeof(kye_tablet_rdesc)) {
		hid_warn(hdev,
			 "tablet report size too small, or kye_tablet_rdesc unexpectedly large\n");
		return rdesc;
	}

	for (info = kye_tablets_info; info->product; info++) {
		if (hdev->product == info->product)
			break;
	}

	if (!info->product) {
		hid_err(hdev, "tablet unknown, someone forget to add kye_tablet_info entry?\n");
		return rdesc;
	}

	newsize = info->has_punk ? sizeof(kye_tablet_rdesc) : 112;
	memcpy(rdesc, kye_tablet_rdesc, newsize);

	put_unaligned_le32(info->x_logical_maximum, rdesc + 66);
	put_unaligned_le32(info->x_physical_maximum, rdesc + 72);
	rdesc[77] = info->unit;
	rdesc[79] = info->unit_exponent;
	put_unaligned_le32(info->y_logical_maximum, rdesc + 87);
	put_unaligned_le32(info->y_physical_maximum, rdesc + 92);
	put_unaligned_le32(info->pressure_logical_maximum, rdesc + 104);

	if (info->has_punk) {
		put_unaligned_le32(info->x_logical_maximum, rdesc + 156);
		put_unaligned_le32(info->x_physical_maximum, rdesc + 162);
		rdesc[167] = info->unit;
		rdesc[169] = info->unit_exponent;
		put_unaligned_le32(info->y_logical_maximum, rdesc + 177);
		put_unaligned_le32(info->y_physical_maximum, rdesc + 182);
	}

	if (info->control_rsize) {
		if (newsize + info->control_rsize > *rsize)
			hid_err(hdev, "control rdesc unexpectedly large");
		else {
			memcpy(rdesc + newsize, info->control_rdesc, info->control_rsize);
			newsize += info->control_rsize;
		}
	}

	*rsize = newsize;
	return rdesc;
}

static __u8 *kye_report_fixup(struct hid_device *hdev, __u8 *rdesc,
		unsigned int *rsize)
{
	switch (hdev->product) {
	case USB_DEVICE_ID_KYE_ERGO_525V:
		 
		if (*rsize >= 75 &&
			rdesc[61] == 0x05 && rdesc[62] == 0x08 &&
			rdesc[63] == 0x19 && rdesc[64] == 0x08 &&
			rdesc[65] == 0x29 && rdesc[66] == 0x0f &&
			rdesc[71] == 0x75 && rdesc[72] == 0x08 &&
			rdesc[73] == 0x95 && rdesc[74] == 0x01) {
			hid_info(hdev,
				 "fixing up Kye/Genius Ergo Mouse "
				 "report descriptor\n");
			rdesc[62] = 0x09;
			rdesc[64] = 0x04;
			rdesc[66] = 0x07;
			rdesc[72] = 0x01;
			rdesc[74] = 0x08;
		}
		break;
	case USB_DEVICE_ID_GENIUS_GILA_GAMING_MOUSE:
		rdesc = kye_consumer_control_fixup(hdev, rdesc, rsize, 104,
					"Genius Gila Gaming Mouse");
		break;
	case USB_DEVICE_ID_GENIUS_MANTICORE:
		rdesc = kye_consumer_control_fixup(hdev, rdesc, rsize, 104,
					"Genius Manticore Keyboard");
		break;
	case USB_DEVICE_ID_GENIUS_GX_IMPERATOR:
		rdesc = kye_consumer_control_fixup(hdev, rdesc, rsize, 83,
					"Genius Gx Imperator Keyboard");
		break;
	case USB_DEVICE_ID_KYE_EASYPEN_M406:
	case USB_DEVICE_ID_KYE_EASYPEN_M506:
	case USB_DEVICE_ID_KYE_EASYPEN_I405X:
	case USB_DEVICE_ID_KYE_MOUSEPEN_I608X:
	case USB_DEVICE_ID_KYE_EASYPEN_M406W:
	case USB_DEVICE_ID_KYE_EASYPEN_M610X:
	case USB_DEVICE_ID_KYE_EASYPEN_340:
	case USB_DEVICE_ID_KYE_PENSKETCH_M912:
	case USB_DEVICE_ID_KYE_MOUSEPEN_M508WX:
	case USB_DEVICE_ID_KYE_MOUSEPEN_M508X:
	case USB_DEVICE_ID_KYE_EASYPEN_M406XE:
	case USB_DEVICE_ID_KYE_MOUSEPEN_I608X_V2:
	case USB_DEVICE_ID_KYE_PENSKETCH_T609A:
		rdesc = kye_tablet_fixup(hdev, rdesc, rsize);
		break;
	}
	return rdesc;
}

static int kye_tablet_enable(struct hid_device *hdev)
{
	struct list_head *list;
	struct list_head *head;
	struct hid_report *report;
	__s32 *value;

	list = &hdev->report_enum[HID_FEATURE_REPORT].report_list;
	list_for_each(head, list) {
		report = list_entry(head, struct hid_report, list);
		if (report->id == 5)
			break;
	}

	if (head == list) {
		hid_err(hdev, "tablet-enabling feature report not found\n");
		return -ENODEV;
	}

	if (report->maxfield < 1 || report->field[0]->report_count < 7) {
		hid_err(hdev, "invalid tablet-enabling feature report\n");
		return -ENODEV;
	}

	value = report->field[0]->value;

	 
	value[0] = 0x12;
	value[1] = 0x10;
	value[2] = 0x11;
	value[3] = 0x12;
	value[4] = 0x00;
	value[5] = 0x00;
	value[6] = 0x00;
	hid_hw_request(hdev, report, HID_REQ_SET_REPORT);

	return 0;
}

static int kye_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err;
	}

	switch (id->product) {
	case USB_DEVICE_ID_GENIUS_MANTICORE:
		 
		if (hid_hw_open(hdev))
			hid_hw_close(hdev);
		break;
	case USB_DEVICE_ID_KYE_EASYPEN_M406:
	case USB_DEVICE_ID_KYE_EASYPEN_M506:
	case USB_DEVICE_ID_KYE_EASYPEN_I405X:
	case USB_DEVICE_ID_KYE_MOUSEPEN_I608X:
	case USB_DEVICE_ID_KYE_EASYPEN_M406W:
	case USB_DEVICE_ID_KYE_EASYPEN_M610X:
	case USB_DEVICE_ID_KYE_EASYPEN_340:
	case USB_DEVICE_ID_KYE_PENSKETCH_M912:
	case USB_DEVICE_ID_KYE_MOUSEPEN_M508WX:
	case USB_DEVICE_ID_KYE_MOUSEPEN_M508X:
	case USB_DEVICE_ID_KYE_EASYPEN_M406XE:
	case USB_DEVICE_ID_KYE_MOUSEPEN_I608X_V2:
	case USB_DEVICE_ID_KYE_PENSKETCH_T609A:
		ret = kye_tablet_enable(hdev);
		if (ret) {
			hid_err(hdev, "tablet enabling failed\n");
			goto enabling_err;
		}
		break;
	}

	return 0;
enabling_err:
	hid_hw_stop(hdev);
err:
	return ret;
}

static const struct hid_device_id kye_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE, USB_DEVICE_ID_KYE_ERGO_525V) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_GENIUS_GILA_GAMING_MOUSE) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_GENIUS_MANTICORE) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_GENIUS_GX_IMPERATOR) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_EASYPEN_M406) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_EASYPEN_M506) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_EASYPEN_I405X) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_MOUSEPEN_I608X) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_EASYPEN_M406W) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_EASYPEN_M610X) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_EASYPEN_340) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_PENSKETCH_M912) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_MOUSEPEN_M508WX) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_MOUSEPEN_M508X) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_EASYPEN_M406XE) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_MOUSEPEN_I608X_V2) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_PENSKETCH_T609A) },
	{ }
};
MODULE_DEVICE_TABLE(hid, kye_devices);

static struct hid_driver kye_driver = {
	.name = "kye",
	.id_table = kye_devices,
	.probe = kye_probe,
	.report_fixup = kye_report_fixup,
};
module_hid_driver(kye_driver);

MODULE_LICENSE("GPL");
