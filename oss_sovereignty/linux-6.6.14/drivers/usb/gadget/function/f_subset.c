
 

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/etherdevice.h>

#include "u_ether.h"
#include "u_ether_configfs.h"
#include "u_gether.h"

 

struct f_gether {
	struct gether			port;

	char				ethaddr[14];
};

static inline struct f_gether *func_to_geth(struct usb_function *f)
{
	return container_of(f, struct f_gether, port.func);
}

 

 

 

static struct usb_interface_descriptor subset_data_intf = {
	.bLength =		sizeof subset_data_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	 
	.bAlternateSetting =	0,
	.bNumEndpoints =	2,
	.bInterfaceClass =      USB_CLASS_COMM,
	.bInterfaceSubClass =	USB_CDC_SUBCLASS_MDLM,
	.bInterfaceProtocol =	0,
	 
};

static struct usb_cdc_header_desc mdlm_header_desc = {
	.bLength =		sizeof mdlm_header_desc,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,

	.bcdCDC =		cpu_to_le16(0x0110),
};

static struct usb_cdc_mdlm_desc mdlm_desc = {
	.bLength =		sizeof mdlm_desc,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_MDLM_TYPE,

	.bcdVersion =		cpu_to_le16(0x0100),
	.bGUID = {
		0x5d, 0x34, 0xcf, 0x66, 0x11, 0x18, 0x11, 0xd6,
		0xa2, 0x1a, 0x00, 0x01, 0x02, 0xca, 0x9a, 0x7f,
	},
};

 
static u8 mdlm_detail_desc[] = {
	6,
	USB_DT_CS_INTERFACE,
	USB_CDC_MDLM_DETAIL_TYPE,

	0,	 
	0,	 
	0,	 
};

static struct usb_cdc_ether_desc ether_desc = {
	.bLength =		sizeof ether_desc,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_ETHERNET_TYPE,

	 
	 
	.bmEthernetStatistics =	cpu_to_le32(0),  
	.wMaxSegmentSize =	cpu_to_le16(ETH_FRAME_LEN),
	.wNumberMCFilters =	cpu_to_le16(0),
	.bNumberPowerFilters =	0,
};

 

static struct usb_endpoint_descriptor fs_subset_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor fs_subset_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_eth_function[] = {
	(struct usb_descriptor_header *) &subset_data_intf,
	(struct usb_descriptor_header *) &mdlm_header_desc,
	(struct usb_descriptor_header *) &mdlm_desc,
	(struct usb_descriptor_header *) &mdlm_detail_desc,
	(struct usb_descriptor_header *) &ether_desc,
	(struct usb_descriptor_header *) &fs_subset_in_desc,
	(struct usb_descriptor_header *) &fs_subset_out_desc,
	NULL,
};

 

static struct usb_endpoint_descriptor hs_subset_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor hs_subset_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *hs_eth_function[] = {
	(struct usb_descriptor_header *) &subset_data_intf,
	(struct usb_descriptor_header *) &mdlm_header_desc,
	(struct usb_descriptor_header *) &mdlm_desc,
	(struct usb_descriptor_header *) &mdlm_detail_desc,
	(struct usb_descriptor_header *) &ether_desc,
	(struct usb_descriptor_header *) &hs_subset_in_desc,
	(struct usb_descriptor_header *) &hs_subset_out_desc,
	NULL,
};

 

static struct usb_endpoint_descriptor ss_subset_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor ss_subset_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_subset_bulk_comp_desc = {
	.bLength =		sizeof ss_subset_bulk_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	 
	 
	 
};

static struct usb_descriptor_header *ss_eth_function[] = {
	(struct usb_descriptor_header *) &subset_data_intf,
	(struct usb_descriptor_header *) &mdlm_header_desc,
	(struct usb_descriptor_header *) &mdlm_desc,
	(struct usb_descriptor_header *) &mdlm_detail_desc,
	(struct usb_descriptor_header *) &ether_desc,
	(struct usb_descriptor_header *) &ss_subset_in_desc,
	(struct usb_descriptor_header *) &ss_subset_bulk_comp_desc,
	(struct usb_descriptor_header *) &ss_subset_out_desc,
	(struct usb_descriptor_header *) &ss_subset_bulk_comp_desc,
	NULL,
};

 

static struct usb_string geth_string_defs[] = {
	[0].s = "CDC Ethernet Subset/SAFE",
	[1].s = "",
	{  }  
};

static struct usb_gadget_strings geth_string_table = {
	.language =		0x0409,	 
	.strings =		geth_string_defs,
};

static struct usb_gadget_strings *geth_strings[] = {
	&geth_string_table,
	NULL,
};

 

static int geth_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_gether		*geth = func_to_geth(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct net_device	*net;

	 

	if (geth->port.in_ep->enabled) {
		DBG(cdev, "reset cdc subset\n");
		gether_disconnect(&geth->port);
	}

	DBG(cdev, "init + activate cdc subset\n");
	if (config_ep_by_speed(cdev->gadget, f, geth->port.in_ep) ||
	    config_ep_by_speed(cdev->gadget, f, geth->port.out_ep)) {
		geth->port.in_ep->desc = NULL;
		geth->port.out_ep->desc = NULL;
		return -EINVAL;
	}

	net = gether_connect(&geth->port);
	return PTR_ERR_OR_ZERO(net);
}

static void geth_disable(struct usb_function *f)
{
	struct f_gether	*geth = func_to_geth(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	DBG(cdev, "net deactivated\n");
	gether_disconnect(&geth->port);
}

 

 

static int
geth_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_gether		*geth = func_to_geth(f);
	struct usb_string	*us;
	int			status;
	struct usb_ep		*ep;

	struct f_gether_opts	*gether_opts;

	gether_opts = container_of(f->fi, struct f_gether_opts, func_inst);

	 
	if (!gether_opts->bound) {
		mutex_lock(&gether_opts->lock);
		gether_set_gadget(gether_opts->net, cdev->gadget);
		status = gether_register_netdev(gether_opts->net);
		mutex_unlock(&gether_opts->lock);
		if (status)
			return status;
		gether_opts->bound = true;
	}

	us = usb_gstrings_attach(cdev, geth_strings,
				 ARRAY_SIZE(geth_string_defs));
	if (IS_ERR(us))
		return PTR_ERR(us);

	subset_data_intf.iInterface = us[0].id;
	ether_desc.iMACAddress = us[1].id;

	 
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	subset_data_intf.bInterfaceNumber = status;

	status = -ENODEV;

	 
	ep = usb_ep_autoconfig(cdev->gadget, &fs_subset_in_desc);
	if (!ep)
		goto fail;
	geth->port.in_ep = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &fs_subset_out_desc);
	if (!ep)
		goto fail;
	geth->port.out_ep = ep;

	 
	hs_subset_in_desc.bEndpointAddress = fs_subset_in_desc.bEndpointAddress;
	hs_subset_out_desc.bEndpointAddress =
		fs_subset_out_desc.bEndpointAddress;

	ss_subset_in_desc.bEndpointAddress = fs_subset_in_desc.bEndpointAddress;
	ss_subset_out_desc.bEndpointAddress =
		fs_subset_out_desc.bEndpointAddress;

	status = usb_assign_descriptors(f, fs_eth_function, hs_eth_function,
			ss_eth_function, ss_eth_function);
	if (status)
		goto fail;

	 

	DBG(cdev, "CDC Subset: IN/%s OUT/%s\n",
			geth->port.in_ep->name, geth->port.out_ep->name);
	return 0;

fail:
	ERROR(cdev, "%s: can't bind, err %d\n", f->name, status);

	return status;
}

static inline struct f_gether_opts *to_f_gether_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_gether_opts,
			    func_inst.group);
}

 
USB_ETHERNET_CONFIGFS_ITEM(gether);

 
USB_ETHERNET_CONFIGFS_ITEM_ATTR_DEV_ADDR(gether);

 
USB_ETHERNET_CONFIGFS_ITEM_ATTR_HOST_ADDR(gether);

 
USB_ETHERNET_CONFIGFS_ITEM_ATTR_QMULT(gether);

 
USB_ETHERNET_CONFIGFS_ITEM_ATTR_IFNAME(gether);

static struct configfs_attribute *gether_attrs[] = {
	&gether_opts_attr_dev_addr,
	&gether_opts_attr_host_addr,
	&gether_opts_attr_qmult,
	&gether_opts_attr_ifname,
	NULL,
};

static const struct config_item_type gether_func_type = {
	.ct_item_ops	= &gether_item_ops,
	.ct_attrs	= gether_attrs,
	.ct_owner	= THIS_MODULE,
};

static void geth_free_inst(struct usb_function_instance *f)
{
	struct f_gether_opts *opts;

	opts = container_of(f, struct f_gether_opts, func_inst);
	if (opts->bound)
		gether_cleanup(netdev_priv(opts->net));
	else
		free_netdev(opts->net);
	kfree(opts);
}

static struct usb_function_instance *geth_alloc_inst(void)
{
	struct f_gether_opts *opts;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);
	mutex_init(&opts->lock);
	opts->func_inst.free_func_inst = geth_free_inst;
	opts->net = gether_setup_default();
	if (IS_ERR(opts->net)) {
		struct net_device *net = opts->net;
		kfree(opts);
		return ERR_CAST(net);
	}

	config_group_init_type_name(&opts->func_inst.group, "",
				    &gether_func_type);

	return &opts->func_inst;
}

static void geth_free(struct usb_function *f)
{
	struct f_gether *eth;

	eth = func_to_geth(f);
	kfree(eth);
}

static void geth_unbind(struct usb_configuration *c, struct usb_function *f)
{
	geth_string_defs[0].id = 0;
	usb_free_all_descriptors(f);
}

static struct usb_function *geth_alloc(struct usb_function_instance *fi)
{
	struct f_gether	*geth;
	struct f_gether_opts *opts;
	int status;

	 
	geth = kzalloc(sizeof(*geth), GFP_KERNEL);
	if (!geth)
		return ERR_PTR(-ENOMEM);

	opts = container_of(fi, struct f_gether_opts, func_inst);

	mutex_lock(&opts->lock);
	opts->refcnt++;
	 
	status = gether_get_host_addr_cdc(opts->net, geth->ethaddr,
					  sizeof(geth->ethaddr));
	if (status < 12) {
		kfree(geth);
		mutex_unlock(&opts->lock);
		return ERR_PTR(-EINVAL);
	}
	geth_string_defs[1].s = geth->ethaddr;

	geth->port.ioport = netdev_priv(opts->net);
	mutex_unlock(&opts->lock);
	geth->port.cdc_filter = DEFAULT_FILTER;

	geth->port.func.name = "cdc_subset";
	geth->port.func.bind = geth_bind;
	geth->port.func.unbind = geth_unbind;
	geth->port.func.set_alt = geth_set_alt;
	geth->port.func.disable = geth_disable;
	geth->port.func.free_func = geth_free;

	return &geth->port.func;
}

DECLARE_USB_FUNCTION_INIT(geth, geth_alloc_inst, geth_alloc);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Brownell");
