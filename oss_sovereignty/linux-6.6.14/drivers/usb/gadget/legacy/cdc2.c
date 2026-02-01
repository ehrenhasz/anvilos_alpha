
 

#include <linux/kernel.h>
#include <linux/module.h>

#include "u_ether.h"
#include "u_serial.h"
#include "u_ecm.h"


#define DRIVER_DESC		"CDC Composite Gadget"
#define DRIVER_VERSION		"King Kamehameha Day 2008"

 

 

 
#define CDC_VENDOR_NUM		0x0525	 
#define CDC_PRODUCT_NUM		0xa4aa	 

USB_GADGET_COMPOSITE_OPTIONS();

USB_ETHERNET_MODULE_PARAMETERS();

 

static struct usb_device_descriptor device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	 

	.bDeviceClass =		USB_CLASS_COMM,
	.bDeviceSubClass =	0,
	.bDeviceProtocol =	0,
	 

	 
	.idVendor =		cpu_to_le16(CDC_VENDOR_NUM),
	.idProduct =		cpu_to_le16(CDC_PRODUCT_NUM),
	 
	 
	 
	 
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

 
static struct usb_function *f_acm;
static struct usb_function_instance *fi_serial;

static struct usb_function *f_ecm;
static struct usb_function_instance *fi_ecm;

 
static int cdc_do_config(struct usb_configuration *c)
{
	int	status;

	if (gadget_is_otg(c->cdev->gadget)) {
		c->descriptors = otg_desc;
		c->bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	f_ecm = usb_get_function(fi_ecm);
	if (IS_ERR(f_ecm)) {
		status = PTR_ERR(f_ecm);
		goto err_get_ecm;
	}

	status = usb_add_function(c, f_ecm);
	if (status)
		goto err_add_ecm;

	f_acm = usb_get_function(fi_serial);
	if (IS_ERR(f_acm)) {
		status = PTR_ERR(f_acm);
		goto err_get_acm;
	}

	status = usb_add_function(c, f_acm);
	if (status)
		goto err_add_acm;
	return 0;

err_add_acm:
	usb_put_function(f_acm);
err_get_acm:
	usb_remove_function(c, f_ecm);
err_add_ecm:
	usb_put_function(f_ecm);
err_get_ecm:
	return status;
}

static struct usb_configuration cdc_config_driver = {
	.label			= "CDC Composite (ECM + ACM)",
	.bConfigurationValue	= 1,
	 
	.bmAttributes		= USB_CONFIG_ATT_SELFPOWER,
};

 

static int cdc_bind(struct usb_composite_dev *cdev)
{
	struct usb_gadget	*gadget = cdev->gadget;
	struct f_ecm_opts	*ecm_opts;
	int			status;

	if (!can_support_ecm(cdev->gadget)) {
		dev_err(&gadget->dev, "controller '%s' not usable\n",
				gadget->name);
		return -EINVAL;
	}

	fi_ecm = usb_get_function_instance("ecm");
	if (IS_ERR(fi_ecm))
		return PTR_ERR(fi_ecm);

	ecm_opts = container_of(fi_ecm, struct f_ecm_opts, func_inst);

	gether_set_qmult(ecm_opts->net, qmult);
	if (!gether_set_host_addr(ecm_opts->net, host_addr))
		pr_info("using host ethernet address: %s", host_addr);
	if (!gether_set_dev_addr(ecm_opts->net, dev_addr))
		pr_info("using self ethernet address: %s", dev_addr);

	fi_serial = usb_get_function_instance("acm");
	if (IS_ERR(fi_serial)) {
		status = PTR_ERR(fi_serial);
		goto fail;
	}

	 

	status = usb_string_ids_tab(cdev, strings_dev);
	if (status < 0)
		goto fail1;
	device_desc.iManufacturer = strings_dev[USB_GADGET_MANUFACTURER_IDX].id;
	device_desc.iProduct = strings_dev[USB_GADGET_PRODUCT_IDX].id;

	if (gadget_is_otg(gadget) && !otg_desc[0]) {
		struct usb_descriptor_header *usb_desc;

		usb_desc = usb_otg_descriptor_alloc(gadget);
		if (!usb_desc) {
			status = -ENOMEM;
			goto fail1;
		}
		usb_otg_descriptor_init(gadget, usb_desc);
		otg_desc[0] = usb_desc;
		otg_desc[1] = NULL;
	}

	 
	status = usb_add_config(cdev, &cdc_config_driver, cdc_do_config);
	if (status < 0)
		goto fail2;

	usb_composite_overwrite_options(cdev, &coverwrite);
	dev_info(&gadget->dev, "%s, version: " DRIVER_VERSION "\n",
			DRIVER_DESC);

	return 0;

fail2:
	kfree(otg_desc[0]);
	otg_desc[0] = NULL;
fail1:
	usb_put_function_instance(fi_serial);
fail:
	usb_put_function_instance(fi_ecm);
	return status;
}

static int cdc_unbind(struct usb_composite_dev *cdev)
{
	usb_put_function(f_acm);
	usb_put_function_instance(fi_serial);
	if (!IS_ERR_OR_NULL(f_ecm))
		usb_put_function(f_ecm);
	if (!IS_ERR_OR_NULL(fi_ecm))
		usb_put_function_instance(fi_ecm);
	kfree(otg_desc[0]);
	otg_desc[0] = NULL;

	return 0;
}

static struct usb_composite_driver cdc_driver = {
	.name		= "g_cdc",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.max_speed	= USB_SPEED_SUPER,
	.bind		= cdc_bind,
	.unbind		= cdc_unbind,
};

module_usb_composite_driver(cdc_driver);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("David Brownell");
MODULE_LICENSE("GPL");
