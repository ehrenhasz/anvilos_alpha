
 

#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>
#include <linux/slab.h>
#include "usb-wwan.h"

#define DRIVER_AUTHOR "Qualcomm Inc"
#define DRIVER_DESC "Qualcomm USB Serial driver"

#define QUECTEL_EC20_PID	0x9215

 
enum qcserial_layouts {
	QCSERIAL_G2K = 0,	 
	QCSERIAL_G1K = 1,	 
	QCSERIAL_SWI = 2,	 
	QCSERIAL_HWI = 3,	 
};

#define DEVICE_G1K(v, p) \
	USB_DEVICE(v, p), .driver_info = QCSERIAL_G1K
#define DEVICE_SWI(v, p) \
	USB_DEVICE(v, p), .driver_info = QCSERIAL_SWI
#define DEVICE_HWI(v, p) \
	USB_DEVICE(v, p), .driver_info = QCSERIAL_HWI

static const struct usb_device_id id_table[] = {
	 
	{DEVICE_G1K(0x05c6, 0x9211)},	 
	{DEVICE_G1K(0x05c6, 0x9212)},	 
	{DEVICE_G1K(0x03f0, 0x1f1d)},	 
	{DEVICE_G1K(0x03f0, 0x201d)},	 
	{DEVICE_G1K(0x04da, 0x250d)},	 
	{DEVICE_G1K(0x04da, 0x250c)},	 
	{DEVICE_G1K(0x413c, 0x8172)},	 
	{DEVICE_G1K(0x413c, 0x8171)},	 
	{DEVICE_G1K(0x1410, 0xa001)},	 
	{DEVICE_G1K(0x1410, 0xa002)},	 
	{DEVICE_G1K(0x1410, 0xa003)},	 
	{DEVICE_G1K(0x1410, 0xa004)},	 
	{DEVICE_G1K(0x1410, 0xa005)},	 
	{DEVICE_G1K(0x1410, 0xa006)},	 
	{DEVICE_G1K(0x1410, 0xa007)},	 
	{DEVICE_G1K(0x1410, 0xa008)},	 
	{DEVICE_G1K(0x0b05, 0x1776)},	 
	{DEVICE_G1K(0x0b05, 0x1774)},	 
	{DEVICE_G1K(0x19d2, 0xfff3)},	 
	{DEVICE_G1K(0x19d2, 0xfff2)},	 
	{DEVICE_G1K(0x1557, 0x0a80)},	 
	{DEVICE_G1K(0x05c6, 0x9001)},    
	{DEVICE_G1K(0x05c6, 0x9002)},	 
	{DEVICE_G1K(0x05c6, 0x9202)},	 
	{DEVICE_G1K(0x05c6, 0x9203)},	 
	{DEVICE_G1K(0x05c6, 0x9222)},	 
	{DEVICE_G1K(0x05c6, 0x9008)},	 
	{DEVICE_G1K(0x05c6, 0x9009)},	 
	{DEVICE_G1K(0x05c6, 0x9201)},	 
	{DEVICE_G1K(0x05c6, 0x9221)},	 
	{DEVICE_G1K(0x05c6, 0x9231)},	 
	{DEVICE_G1K(0x1f45, 0x0001)},	 
	{DEVICE_G1K(0x1bc7, 0x900e)},	 

	 
	{USB_DEVICE(0x1410, 0xa010)},	 
	{USB_DEVICE(0x1410, 0xa011)},	 
	{USB_DEVICE(0x1410, 0xa012)},	 
	{USB_DEVICE(0x1410, 0xa013)},	 
	{USB_DEVICE(0x1410, 0xa014)},	 
	{USB_DEVICE(0x413c, 0x8185)},	 
	{USB_DEVICE(0x413c, 0x8186)},	 
	{USB_DEVICE(0x05c6, 0x9208)},	 
	{USB_DEVICE(0x05c6, 0x920b)},	 
	{USB_DEVICE(0x05c6, 0x9224)},	 
	{USB_DEVICE(0x05c6, 0x9225)},	 
	{USB_DEVICE(0x05c6, 0x9244)},	 
	{USB_DEVICE(0x05c6, 0x9245)},	 
	{USB_DEVICE(0x03f0, 0x241d)},	 
	{USB_DEVICE(0x03f0, 0x251d)},	 
	{USB_DEVICE(0x05c6, 0x9214)},	 
	{USB_DEVICE(0x05c6, 0x9215)},	 
	{USB_DEVICE(0x05c6, 0x9264)},	 
	{USB_DEVICE(0x05c6, 0x9265)},	 
	{USB_DEVICE(0x05c6, 0x9234)},	 
	{USB_DEVICE(0x05c6, 0x9235)},	 
	{USB_DEVICE(0x05c6, 0x9274)},	 
	{USB_DEVICE(0x05c6, 0x9275)},	 
	{USB_DEVICE(0x1199, 0x9000)},	 
	{USB_DEVICE(0x1199, 0x9001)},	 
	{USB_DEVICE(0x1199, 0x9002)},	 
	{USB_DEVICE(0x1199, 0x9003)},	 
	{USB_DEVICE(0x1199, 0x9004)},	 
	{USB_DEVICE(0x1199, 0x9005)},	 
	{USB_DEVICE(0x1199, 0x9006)},	 
	{USB_DEVICE(0x1199, 0x9007)},	 
	{USB_DEVICE(0x1199, 0x9008)},	 
	{USB_DEVICE(0x1199, 0x9009)},	 
	{USB_DEVICE(0x1199, 0x900a)},	 
	{USB_DEVICE(0x1199, 0x9011)},    
	{USB_DEVICE(0x16d8, 0x8001)},	 
	{USB_DEVICE(0x16d8, 0x8002)},	 
	{USB_DEVICE(0x05c6, 0x9204)},	 
	{USB_DEVICE(0x05c6, 0x9205)},	 

	 
	{USB_DEVICE(0x03f0, 0x371d)},	 
	{USB_DEVICE(0x05c6, 0x920c)},	 
	{USB_DEVICE(0x05c6, 0x920d)},	 
	{USB_DEVICE(0x1410, 0xa020)},    
	{USB_DEVICE(0x1410, 0xa021)},	 
	{USB_DEVICE(0x413c, 0x8193)},	 
	{USB_DEVICE(0x413c, 0x8194)},	 
	{USB_DEVICE(0x413c, 0x81a6)},	 
	{USB_DEVICE(0x1199, 0x68a4)},	 
	{USB_DEVICE(0x1199, 0x68a5)},	 
	{USB_DEVICE(0x1199, 0x68a8)},	 
	{USB_DEVICE(0x1199, 0x68a9)},	 
	{USB_DEVICE(0x1199, 0x9010)},	 
	{USB_DEVICE(0x1199, 0x9012)},	 
	{USB_DEVICE(0x1199, 0x9013)},	 
	{USB_DEVICE(0x1199, 0x9014)},	 
	{USB_DEVICE(0x1199, 0x9015)},	 
	{USB_DEVICE(0x1199, 0x9018)},	 
	{USB_DEVICE(0x1199, 0x9019)},	 
	{USB_DEVICE(0x1199, 0x901b)},	 
	{USB_DEVICE(0x12D1, 0x14F0)},	 
	{USB_DEVICE(0x12D1, 0x14F1)},	 
	{USB_DEVICE(0x0AF0, 0x8120)},	 

	 
	{DEVICE_SWI(0x03f0, 0x4e1d)},	 
	{DEVICE_SWI(0x0f3d, 0x68a2)},	 
	{DEVICE_SWI(0x114f, 0x68a2)},	 
	{DEVICE_SWI(0x1199, 0x68a2)},	 
	{DEVICE_SWI(0x1199, 0x68c0)},	 
	{DEVICE_SWI(0x1199, 0x901c)},	 
	{DEVICE_SWI(0x1199, 0x901e)},	 
	{DEVICE_SWI(0x1199, 0x901f)},	 
	{DEVICE_SWI(0x1199, 0x9040)},	 
	{DEVICE_SWI(0x1199, 0x9041)},	 
	{DEVICE_SWI(0x1199, 0x9051)},	 
	{DEVICE_SWI(0x1199, 0x9053)},	 
	{DEVICE_SWI(0x1199, 0x9054)},	 
	{DEVICE_SWI(0x1199, 0x9055)},	 
	{DEVICE_SWI(0x1199, 0x9056)},	 
	{DEVICE_SWI(0x1199, 0x9060)},	 
	{DEVICE_SWI(0x1199, 0x9061)},	 
	{DEVICE_SWI(0x1199, 0x9062)},	 
	{DEVICE_SWI(0x1199, 0x9063)},	 
	{DEVICE_SWI(0x1199, 0x9070)},	 
	{DEVICE_SWI(0x1199, 0x9071)},	 
	{DEVICE_SWI(0x1199, 0x9078)},	 
	{DEVICE_SWI(0x1199, 0x9079)},	 
	{DEVICE_SWI(0x1199, 0x907a)},	 
	{DEVICE_SWI(0x1199, 0x907b)},	 
	{DEVICE_SWI(0x1199, 0x9090)},	 
	{DEVICE_SWI(0x1199, 0x9091)},	 
	{DEVICE_SWI(0x1199, 0x90d2)},	 
	{DEVICE_SWI(0x1199, 0xc080)},	 
	{DEVICE_SWI(0x1199, 0xc081)},	 
	{DEVICE_SWI(0x413c, 0x81a2)},	 
	{DEVICE_SWI(0x413c, 0x81a3)},	 
	{DEVICE_SWI(0x413c, 0x81a4)},	 
	{DEVICE_SWI(0x413c, 0x81a8)},	 
	{DEVICE_SWI(0x413c, 0x81a9)},	 
	{DEVICE_SWI(0x413c, 0x81b1)},	 
	{DEVICE_SWI(0x413c, 0x81b3)},	 
	{DEVICE_SWI(0x413c, 0x81b5)},	 
	{DEVICE_SWI(0x413c, 0x81b6)},	 
	{DEVICE_SWI(0x413c, 0x81c2)},	 
	{DEVICE_SWI(0x413c, 0x81cb)},	 
	{DEVICE_SWI(0x413c, 0x81cc)},	 
	{DEVICE_SWI(0x413c, 0x81cf)},    
	{DEVICE_SWI(0x413c, 0x81d0)},    
	{DEVICE_SWI(0x413c, 0x81d1)},    
	{DEVICE_SWI(0x413c, 0x81d2)},    

	 
	{DEVICE_HWI(0x03f0, 0x581d)},	 

	{ }				 
};
MODULE_DEVICE_TABLE(usb, id_table);

static int handle_quectel_ec20(struct device *dev, int ifnum)
{
	int altsetting = 0;

	 
	switch (ifnum) {
	case 0:
		dev_dbg(dev, "Quectel EC20 DM/DIAG interface found\n");
		break;
	case 1:
		dev_dbg(dev, "Quectel EC20 NMEA GPS interface found\n");
		break;
	case 2:
	case 3:
		dev_dbg(dev, "Quectel EC20 Modem port found\n");
		break;
	case 4:
		 
		altsetting = -1;
		break;
	}

	return altsetting;
}

static int qcprobe(struct usb_serial *serial, const struct usb_device_id *id)
{
	struct usb_host_interface *intf = serial->interface->cur_altsetting;
	struct device *dev = &serial->dev->dev;
	int retval = -ENODEV;
	__u8 nintf;
	__u8 ifnum;
	int altsetting = -1;
	bool sendsetup = false;

	 
	if (intf->desc.bInterfaceClass != USB_CLASS_VENDOR_SPEC)
		goto done;

	nintf = serial->dev->actconfig->desc.bNumInterfaces;
	dev_dbg(dev, "Num Interfaces = %d\n", nintf);
	ifnum = intf->desc.bInterfaceNumber;
	dev_dbg(dev, "This Interface = %d\n", ifnum);

	if (nintf == 1) {
		 
		 
		if (serial->interface->num_altsetting == 2)
			intf = usb_altnum_to_altsetting(serial->interface, 1);
		else if (serial->interface->num_altsetting > 2)
			goto done;

		if (intf && intf->desc.bNumEndpoints == 2 &&
		    usb_endpoint_is_bulk_in(&intf->endpoint[0].desc) &&
		    usb_endpoint_is_bulk_out(&intf->endpoint[1].desc)) {
			dev_dbg(dev, "QDL port found\n");

			if (serial->interface->num_altsetting == 1)
				retval = 0;  
			else
				altsetting = 1;
		}
		goto done;

	}

	 
	altsetting = 0;

	 

	switch (id->driver_info) {
	case QCSERIAL_G1K:
		 
		if (nintf < 3 || nintf > 4) {
			dev_err(dev, "unknown number of interfaces: %d\n", nintf);
			altsetting = -1;
			goto done;
		}

		if (ifnum == 0) {
			dev_dbg(dev, "Gobi 1K DM/DIAG interface found\n");
			altsetting = 1;
		} else if (ifnum == 2)
			dev_dbg(dev, "Modem port found\n");
		else
			altsetting = -1;
		break;
	case QCSERIAL_G2K:
		 
		if (nintf == 5 && id->idProduct == QUECTEL_EC20_PID) {
			altsetting = handle_quectel_ec20(dev, ifnum);
			goto done;
		}

		 
		if (nintf < 3 || nintf > 4) {
			dev_err(dev, "unknown number of interfaces: %d\n", nintf);
			altsetting = -1;
			goto done;
		}

		switch (ifnum) {
		case 0:
			 
			altsetting = -1;
			break;
		case 1:
			dev_dbg(dev, "Gobi 2K+ DM/DIAG interface found\n");
			break;
		case 2:
			dev_dbg(dev, "Modem port found\n");
			break;
		case 3:
			 
			dev_dbg(dev, "Gobi 2K+ NMEA GPS interface found\n");
			break;
		}
		break;
	case QCSERIAL_SWI:
		 
		switch (ifnum) {
		case 0:
			dev_dbg(dev, "DM/DIAG interface found\n");
			break;
		case 2:
			dev_dbg(dev, "NMEA GPS interface found\n");
			sendsetup = true;
			break;
		case 3:
			dev_dbg(dev, "Modem port found\n");
			sendsetup = true;
			break;
		default:
			 
			altsetting = -1;
			break;
		}
		break;
	case QCSERIAL_HWI:
		 
		switch (intf->desc.bInterfaceProtocol) {
			 
		case 0x07:
		case 0x37:
		case 0x67:
			 
		case 0x08:
		case 0x38:
		case 0x68:
			 
		case 0x09:
		case 0x39:
		case 0x69:
			 
		case 0x16:
		case 0x46:
		case 0x76:
			altsetting = -1;
			break;
		default:
			dev_dbg(dev, "Huawei type serial port found (%02x/%02x/%02x)\n",
				intf->desc.bInterfaceClass,
				intf->desc.bInterfaceSubClass,
				intf->desc.bInterfaceProtocol);
		}
		break;
	default:
		dev_err(dev, "unsupported device layout type: %lu\n",
			id->driver_info);
		break;
	}

done:
	if (altsetting >= 0) {
		retval = usb_set_interface(serial->dev, ifnum, altsetting);
		if (retval < 0) {
			dev_err(dev,
				"Could not set interface, error %d\n",
				retval);
			retval = -ENODEV;
		}
	}

	if (!retval)
		usb_set_serial_data(serial, (void *)(unsigned long)sendsetup);

	return retval;
}

static int qc_attach(struct usb_serial *serial)
{
	struct usb_wwan_intf_private *data;
	bool sendsetup;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	sendsetup = !!(unsigned long)(usb_get_serial_data(serial));
	if (sendsetup)
		data->use_send_setup = 1;

	spin_lock_init(&data->susp_lock);

	usb_set_serial_data(serial, data);

	return 0;
}

static void qc_release(struct usb_serial *serial)
{
	struct usb_wwan_intf_private *priv = usb_get_serial_data(serial);

	usb_set_serial_data(serial, NULL);
	kfree(priv);
}

static struct usb_serial_driver qcdevice = {
	.driver = {
		.owner     = THIS_MODULE,
		.name      = "qcserial",
	},
	.description         = "Qualcomm USB modem",
	.id_table            = id_table,
	.num_ports           = 1,
	.probe               = qcprobe,
	.open		     = usb_wwan_open,
	.close		     = usb_wwan_close,
	.dtr_rts	     = usb_wwan_dtr_rts,
	.write		     = usb_wwan_write,
	.write_room	     = usb_wwan_write_room,
	.chars_in_buffer     = usb_wwan_chars_in_buffer,
	.tiocmget            = usb_wwan_tiocmget,
	.tiocmset            = usb_wwan_tiocmset,
	.attach              = qc_attach,
	.release	     = qc_release,
	.port_probe          = usb_wwan_port_probe,
	.port_remove	     = usb_wwan_port_remove,
#ifdef CONFIG_PM
	.suspend	     = usb_wwan_suspend,
	.resume		     = usb_wwan_resume,
#endif
};

static struct usb_serial_driver * const serial_drivers[] = {
	&qcdevice, NULL
};

module_usb_serial_driver(serial_drivers, id_table);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");
