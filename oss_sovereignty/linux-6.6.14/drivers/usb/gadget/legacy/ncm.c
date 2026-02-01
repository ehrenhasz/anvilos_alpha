
 

 
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb/composite.h>

#include "u_ether.h"
#include "u_ncm.h"

#define DRIVER_DESC		"NCM Gadget"

 

 

 
#define CDC_VENDOR_NUM		0x0525	 
#define CDC_PRODUCT_NUM		0xa4a1	 

 
USB_GADGET_COMPOSITE_OPTIONS();

USB_ETHERNET_MODULE_PARAMETERS();

static struct usb_device_descriptor device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	 

	.bDeviceClass =		USB_CLASS_COMM,
	.bDeviceSubClass =	0,
	.bDeviceProtocol =	0,
	 

	 
	.idVendor =		cpu_to_le16 (CDC_VENDOR_NUM),
	.idProduct =		cpu_to_le16 (CDC_PRODUCT_NUM),
	 
	 
	 
	 
	.bNumConfigurations =	1,
};

static const struct usb_descriptor_header *otg_desc[2];

 
static struct usb_string strings_dev[] = {
	[USB_GADGET_MANUFACTURER_IDX].s = "",
	[USB_GADGET_PRODUCT_IDX].s = DRIVER_DESC,
	[USB_GADGET_SERIAL_IDX].s = "",
	{  }  
};

static struct usb_gadget_strings stringtab_dev = {
	.language	= 0x0409,	 
	.strings	= strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
	&stringtab_dev,
	NULL,
};

static struct usb_function_instance *f_ncm_inst;
static struct usb_function *f_ncm;

 

static int ncm_do_config(struct usb_configuration *c)
{
	int status;

	 

	if (gadget_is_otg(c->cdev->gadget)) {
		c->descriptors = otg_desc;
		c->bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	f_ncm = usb_get_function(f_ncm_inst);
	if (IS_ERR(f_ncm))
		return PTR_ERR(f_ncm);

	status = usb_add_function(c, f_ncm);
	if (status < 0) {
		usb_put_function(f_ncm);
		return status;
	}

	return 0;
}

static struct usb_configuration ncm_config_driver = {
	 
	.label			= "CDC Ethernet (NCM)",
	.bConfigurationValue	= 1,
	 
	.bmAttributes		= USB_CONFIG_ATT_SELFPOWER,
};

 

static int gncm_bind(struct usb_composite_dev *cdev)
{
	struct usb_gadget	*gadget = cdev->gadget;
	struct f_ncm_opts	*ncm_opts;
	int			status;

	f_ncm_inst = usb_get_function_instance("ncm");
	if (IS_ERR(f_ncm_inst))
		return PTR_ERR(f_ncm_inst);

	ncm_opts = container_of(f_ncm_inst, struct f_ncm_opts, func_inst);

	gether_set_qmult(ncm_opts->net, qmult);
	if (!gether_set_host_addr(ncm_opts->net, host_addr))
		pr_info("using host ethernet address: %s", host_addr);
	if (!gether_set_dev_addr(ncm_opts->net, dev_addr))
		pr_info("using self ethernet address: %s", dev_addr);

	 

	status = usb_string_ids_tab(cdev, strings_dev);
	if (status < 0)
		goto fail;
	device_desc.iManufacturer = strings_dev[USB_GADGET_MANUFACTURER_IDX].id;
	device_desc.iProduct = strings_dev[USB_GADGET_PRODUCT_IDX].id;

	if (gadget_is_otg(gadget) && !otg_desc[0]) {
		struct usb_descriptor_header *usb_desc;

		usb_desc = usb_otg_descriptor_alloc(gadget);
		if (!usb_desc) {
			status = -ENOMEM;
			goto fail;
		}
		usb_otg_descriptor_init(gadget, usb_desc);
		otg_desc[0] = usb_desc;
		otg_desc[1] = NULL;
	}

	status = usb_add_config(cdev, &ncm_config_driver,
				ncm_do_config);
	if (status < 0)
		goto fail1;

	usb_composite_overwrite_options(cdev, &coverwrite);
	dev_info(&gadget->dev, "%s\n", DRIVER_DESC);

	return 0;

fail1:
	kfree(otg_desc[0]);
	otg_desc[0] = NULL;
fail:
	usb_put_function_instance(f_ncm_inst);
	return status;
}

static int gncm_unbind(struct usb_composite_dev *cdev)
{
	if (!IS_ERR_OR_NULL(f_ncm))
		usb_put_function(f_ncm);
	if (!IS_ERR_OR_NULL(f_ncm_inst))
		usb_put_function_instance(f_ncm_inst);
	kfree(otg_desc[0]);
	otg_desc[0] = NULL;

	return 0;
}

static struct usb_composite_driver ncm_driver = {
	.name		= "g_ncm",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.max_speed	= USB_SPEED_SUPER,
	.bind		= gncm_bind,
	.unbind		= gncm_unbind,
};

module_usb_composite_driver(ncm_driver);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Yauheni Kaliuta");
MODULE_LICENSE("GPL");
