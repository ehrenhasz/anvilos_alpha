
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb_usual.h>


 
#define UNUSUAL_DEV(id_vendor, id_product, bcdDeviceMin, bcdDeviceMax, \
		    vendorName, productName, useProtocol, useTransport, \
		    initFunction, flags) \
{ USB_DEVICE_VER(id_vendor, id_product, bcdDeviceMin, bcdDeviceMax), \
  .driver_info = (flags) }

#define COMPLIANT_DEV	UNUSUAL_DEV

#define USUAL_DEV(useProto, useTrans) \
{ USB_INTERFACE_INFO(USB_CLASS_MASS_STORAGE, useProto, useTrans) }

 
#define UNUSUAL_VENDOR_INTF(id_vendor, cl, sc, pr, \
			vendorName, productName, useProtocol, useTransport, \
			initFunction, flags) \
{ \
	.match_flags = USB_DEVICE_ID_MATCH_INT_INFO \
				| USB_DEVICE_ID_MATCH_VENDOR, \
	.idVendor    = (id_vendor), \
	.bInterfaceClass = (cl), \
	.bInterfaceSubClass = (sc), \
	.bInterfaceProtocol = (pr), \
	.driver_info = (flags) \
}

const struct usb_device_id usb_storage_usb_ids[] = {
#	include "unusual_devs.h"
	{ }		 
};
MODULE_DEVICE_TABLE(usb, usb_storage_usb_ids);

#undef UNUSUAL_DEV
#undef COMPLIANT_DEV
#undef USUAL_DEV
#undef UNUSUAL_VENDOR_INTF

 
struct ignore_entry {
	u16	vid, pid, bcdmin, bcdmax;
};

#define UNUSUAL_DEV(id_vendor, id_product, bcdDeviceMin, bcdDeviceMax, \
		    vendorName, productName, useProtocol, useTransport, \
		    initFunction, flags) \
{					\
	.vid	= id_vendor,		\
	.pid 	= id_product,		\
	.bcdmin	= bcdDeviceMin,		\
	.bcdmax = bcdDeviceMax,		\
}

static const struct ignore_entry ignore_ids[] = {
#	include "unusual_alauda.h"
#	include "unusual_cypress.h"
#	include "unusual_datafab.h"
#	include "unusual_ene_ub6250.h"
#	include "unusual_freecom.h"
#	include "unusual_isd200.h"
#	include "unusual_jumpshot.h"
#	include "unusual_karma.h"
#	include "unusual_onetouch.h"
#	include "unusual_realtek.h"
#	include "unusual_sddr09.h"
#	include "unusual_sddr55.h"
#	include "unusual_usbat.h"
	{ }		 
};

#undef UNUSUAL_DEV

 
int usb_usual_ignore_device(struct usb_interface *intf)
{
	struct usb_device *udev;
	unsigned vid, pid, bcd;
	const struct ignore_entry *p;

	udev = interface_to_usbdev(intf);
	vid = le16_to_cpu(udev->descriptor.idVendor);
	pid = le16_to_cpu(udev->descriptor.idProduct);
	bcd = le16_to_cpu(udev->descriptor.bcdDevice);

	for (p = ignore_ids; p->vid; ++p) {
		if (p->vid == vid && p->pid == pid &&
				p->bcdmin <= bcd && p->bcdmax >= bcd)
			return -ENXIO;
	}
	return 0;
}
