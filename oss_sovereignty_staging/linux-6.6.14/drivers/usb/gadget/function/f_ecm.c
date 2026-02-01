
 

 

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/etherdevice.h>

#include "u_ether.h"
#include "u_ether_configfs.h"
#include "u_ecm.h"


 


enum ecm_notify_state {
	ECM_NOTIFY_NONE,		 
	ECM_NOTIFY_CONNECT,		 
	ECM_NOTIFY_SPEED,		 
};

struct f_ecm {
	struct gether			port;
	u8				ctrl_id, data_id;

	char				ethaddr[14];

	struct usb_ep			*notify;
	struct usb_request		*notify_req;
	u8				notify_state;
	atomic_t			notify_count;
	bool				is_open;

	 
};

static inline struct f_ecm *func_to_ecm(struct usb_function *f)
{
	return container_of(f, struct f_ecm, port.func);
}

 

 

#define ECM_STATUS_INTERVAL_MS		32
#define ECM_STATUS_BYTECOUNT		16	 


 

static struct usb_interface_assoc_descriptor
ecm_iad_descriptor = {
	.bLength =		sizeof ecm_iad_descriptor,
	.bDescriptorType =	USB_DT_INTERFACE_ASSOCIATION,

	 
	.bInterfaceCount =	2,	 
	.bFunctionClass =	USB_CLASS_COMM,
	.bFunctionSubClass =	USB_CDC_SUBCLASS_ETHERNET,
	.bFunctionProtocol =	USB_CDC_PROTO_NONE,
	 
};


static struct usb_interface_descriptor ecm_control_intf = {
	.bLength =		sizeof ecm_control_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	 
	 
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_COMM,
	.bInterfaceSubClass =	USB_CDC_SUBCLASS_ETHERNET,
	.bInterfaceProtocol =	USB_CDC_PROTO_NONE,
	 
};

static struct usb_cdc_header_desc ecm_header_desc = {
	.bLength =		sizeof ecm_header_desc,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,

	.bcdCDC =		cpu_to_le16(0x0110),
};

static struct usb_cdc_union_desc ecm_union_desc = {
	.bLength =		sizeof(ecm_union_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_UNION_TYPE,
	 
	 
};

static struct usb_cdc_ether_desc ecm_desc = {
	.bLength =		sizeof ecm_desc,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_ETHERNET_TYPE,

	 
	 
	.bmEthernetStatistics =	cpu_to_le32(0),  
	.wMaxSegmentSize =	cpu_to_le16(ETH_FRAME_LEN),
	.wNumberMCFilters =	cpu_to_le16(0),
	.bNumberPowerFilters =	0,
};

 

static struct usb_interface_descriptor ecm_data_nop_intf = {
	.bLength =		sizeof ecm_data_nop_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	.bInterfaceNumber =	1,
	.bAlternateSetting =	0,
	.bNumEndpoints =	0,
	.bInterfaceClass =	USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	 
};

 

static struct usb_interface_descriptor ecm_data_intf = {
	.bLength =		sizeof ecm_data_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	.bInterfaceNumber =	1,
	.bAlternateSetting =	1,
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	 
};

 

static struct usb_endpoint_descriptor fs_ecm_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(ECM_STATUS_BYTECOUNT),
	.bInterval =		ECM_STATUS_INTERVAL_MS,
};

static struct usb_endpoint_descriptor fs_ecm_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor fs_ecm_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *ecm_fs_function[] = {
	 
	(struct usb_descriptor_header *) &ecm_iad_descriptor,
	(struct usb_descriptor_header *) &ecm_control_intf,
	(struct usb_descriptor_header *) &ecm_header_desc,
	(struct usb_descriptor_header *) &ecm_union_desc,
	(struct usb_descriptor_header *) &ecm_desc,

	 
	(struct usb_descriptor_header *) &fs_ecm_notify_desc,

	 
	(struct usb_descriptor_header *) &ecm_data_nop_intf,
	(struct usb_descriptor_header *) &ecm_data_intf,
	(struct usb_descriptor_header *) &fs_ecm_in_desc,
	(struct usb_descriptor_header *) &fs_ecm_out_desc,
	NULL,
};

 

static struct usb_endpoint_descriptor hs_ecm_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(ECM_STATUS_BYTECOUNT),
	.bInterval =		USB_MS_TO_HS_INTERVAL(ECM_STATUS_INTERVAL_MS),
};

static struct usb_endpoint_descriptor hs_ecm_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor hs_ecm_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *ecm_hs_function[] = {
	 
	(struct usb_descriptor_header *) &ecm_iad_descriptor,
	(struct usb_descriptor_header *) &ecm_control_intf,
	(struct usb_descriptor_header *) &ecm_header_desc,
	(struct usb_descriptor_header *) &ecm_union_desc,
	(struct usb_descriptor_header *) &ecm_desc,

	 
	(struct usb_descriptor_header *) &hs_ecm_notify_desc,

	 
	(struct usb_descriptor_header *) &ecm_data_nop_intf,
	(struct usb_descriptor_header *) &ecm_data_intf,
	(struct usb_descriptor_header *) &hs_ecm_in_desc,
	(struct usb_descriptor_header *) &hs_ecm_out_desc,
	NULL,
};

 

static struct usb_endpoint_descriptor ss_ecm_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(ECM_STATUS_BYTECOUNT),
	.bInterval =		USB_MS_TO_HS_INTERVAL(ECM_STATUS_INTERVAL_MS),
};

static struct usb_ss_ep_comp_descriptor ss_ecm_intr_comp_desc = {
	.bLength =		sizeof ss_ecm_intr_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	 
	 
	 
	.wBytesPerInterval =	cpu_to_le16(ECM_STATUS_BYTECOUNT),
};

static struct usb_endpoint_descriptor ss_ecm_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor ss_ecm_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_ecm_bulk_comp_desc = {
	.bLength =		sizeof ss_ecm_bulk_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	 
	 
	 
};

static struct usb_descriptor_header *ecm_ss_function[] = {
	 
	(struct usb_descriptor_header *) &ecm_iad_descriptor,
	(struct usb_descriptor_header *) &ecm_control_intf,
	(struct usb_descriptor_header *) &ecm_header_desc,
	(struct usb_descriptor_header *) &ecm_union_desc,
	(struct usb_descriptor_header *) &ecm_desc,

	 
	(struct usb_descriptor_header *) &ss_ecm_notify_desc,
	(struct usb_descriptor_header *) &ss_ecm_intr_comp_desc,

	 
	(struct usb_descriptor_header *) &ecm_data_nop_intf,
	(struct usb_descriptor_header *) &ecm_data_intf,
	(struct usb_descriptor_header *) &ss_ecm_in_desc,
	(struct usb_descriptor_header *) &ss_ecm_bulk_comp_desc,
	(struct usb_descriptor_header *) &ss_ecm_out_desc,
	(struct usb_descriptor_header *) &ss_ecm_bulk_comp_desc,
	NULL,
};

 

static struct usb_string ecm_string_defs[] = {
	[0].s = "CDC Ethernet Control Model (ECM)",
	[1].s = "",
	[2].s = "CDC Ethernet Data",
	[3].s = "CDC ECM",
	{  }  
};

static struct usb_gadget_strings ecm_string_table = {
	.language =		0x0409,	 
	.strings =		ecm_string_defs,
};

static struct usb_gadget_strings *ecm_strings[] = {
	&ecm_string_table,
	NULL,
};

 

static void ecm_do_notify(struct f_ecm *ecm)
{
	struct usb_request		*req = ecm->notify_req;
	struct usb_cdc_notification	*event;
	struct usb_composite_dev	*cdev = ecm->port.func.config->cdev;
	__le32				*data;
	int				status;

	 
	if (atomic_read(&ecm->notify_count))
		return;

	event = req->buf;
	switch (ecm->notify_state) {
	case ECM_NOTIFY_NONE:
		return;

	case ECM_NOTIFY_CONNECT:
		event->bNotificationType = USB_CDC_NOTIFY_NETWORK_CONNECTION;
		if (ecm->is_open)
			event->wValue = cpu_to_le16(1);
		else
			event->wValue = cpu_to_le16(0);
		event->wLength = 0;
		req->length = sizeof *event;

		DBG(cdev, "notify connect %s\n",
				ecm->is_open ? "true" : "false");
		ecm->notify_state = ECM_NOTIFY_SPEED;
		break;

	case ECM_NOTIFY_SPEED:
		event->bNotificationType = USB_CDC_NOTIFY_SPEED_CHANGE;
		event->wValue = cpu_to_le16(0);
		event->wLength = cpu_to_le16(8);
		req->length = ECM_STATUS_BYTECOUNT;

		 
		data = req->buf + sizeof *event;
		data[0] = cpu_to_le32(gether_bitrate(cdev->gadget));
		data[1] = data[0];

		DBG(cdev, "notify speed %d\n", gether_bitrate(cdev->gadget));
		ecm->notify_state = ECM_NOTIFY_NONE;
		break;
	}
	event->bmRequestType = 0xA1;
	event->wIndex = cpu_to_le16(ecm->ctrl_id);

	atomic_inc(&ecm->notify_count);
	status = usb_ep_queue(ecm->notify, req, GFP_ATOMIC);
	if (status < 0) {
		atomic_dec(&ecm->notify_count);
		DBG(cdev, "notify --> %d\n", status);
	}
}

static void ecm_notify(struct f_ecm *ecm)
{
	 
	ecm->notify_state = ECM_NOTIFY_CONNECT;
	ecm_do_notify(ecm);
}

static void ecm_notify_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_ecm			*ecm = req->context;
	struct usb_composite_dev	*cdev = ecm->port.func.config->cdev;
	struct usb_cdc_notification	*event = req->buf;

	switch (req->status) {
	case 0:
		 
		atomic_dec(&ecm->notify_count);
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
		atomic_set(&ecm->notify_count, 0);
		ecm->notify_state = ECM_NOTIFY_NONE;
		break;
	default:
		DBG(cdev, "event %02x --> %d\n",
			event->bNotificationType, req->status);
		atomic_dec(&ecm->notify_count);
		break;
	}
	ecm_do_notify(ecm);
}

static int ecm_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct f_ecm		*ecm = func_to_ecm(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	 
	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_SET_ETHERNET_PACKET_FILTER:
		 
		if (w_length != 0 || w_index != ecm->ctrl_id)
			goto invalid;
		DBG(cdev, "packet filter %02x\n", w_value);
		 
		ecm->port.cdc_filter = w_value;
		value = 0;
		break;

	 

	default:
invalid:
		DBG(cdev, "invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	 
	if (value >= 0) {
		DBG(cdev, "ecm req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = 0;
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			ERROR(cdev, "ecm req %02x.%02x response err %d\n",
					ctrl->bRequestType, ctrl->bRequest,
					value);
	}

	 
	return value;
}


static int ecm_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_ecm		*ecm = func_to_ecm(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	 
	if (intf == ecm->ctrl_id) {
		if (alt != 0)
			goto fail;

		VDBG(cdev, "reset ecm control %d\n", intf);
		usb_ep_disable(ecm->notify);
		if (!(ecm->notify->desc)) {
			VDBG(cdev, "init ecm ctrl %d\n", intf);
			if (config_ep_by_speed(cdev->gadget, f, ecm->notify))
				goto fail;
		}
		usb_ep_enable(ecm->notify);

	 
	} else if (intf == ecm->data_id) {
		if (alt > 1)
			goto fail;

		if (ecm->port.in_ep->enabled) {
			DBG(cdev, "reset ecm\n");
			gether_disconnect(&ecm->port);
		}

		if (!ecm->port.in_ep->desc ||
		    !ecm->port.out_ep->desc) {
			DBG(cdev, "init ecm\n");
			if (config_ep_by_speed(cdev->gadget, f,
					       ecm->port.in_ep) ||
			    config_ep_by_speed(cdev->gadget, f,
					       ecm->port.out_ep)) {
				ecm->port.in_ep->desc = NULL;
				ecm->port.out_ep->desc = NULL;
				goto fail;
			}
		}

		 
		if (alt == 1) {
			struct net_device	*net;

			 
			ecm->port.is_zlp_ok =
				gadget_is_zlp_supported(cdev->gadget);
			ecm->port.cdc_filter = DEFAULT_FILTER;
			DBG(cdev, "activate ecm\n");
			net = gether_connect(&ecm->port);
			if (IS_ERR(net))
				return PTR_ERR(net);
		}

		 
		ecm_notify(ecm);
	} else
		goto fail;

	return 0;
fail:
	return -EINVAL;
}

 
static int ecm_get_alt(struct usb_function *f, unsigned intf)
{
	struct f_ecm		*ecm = func_to_ecm(f);

	if (intf == ecm->ctrl_id)
		return 0;
	return ecm->port.in_ep->enabled ? 1 : 0;
}

static void ecm_disable(struct usb_function *f)
{
	struct f_ecm		*ecm = func_to_ecm(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	DBG(cdev, "ecm deactivated\n");

	if (ecm->port.in_ep->enabled) {
		gether_disconnect(&ecm->port);
	} else {
		ecm->port.in_ep->desc = NULL;
		ecm->port.out_ep->desc = NULL;
	}

	usb_ep_disable(ecm->notify);
	ecm->notify->desc = NULL;
}

 

 

static void ecm_open(struct gether *geth)
{
	struct f_ecm		*ecm = func_to_ecm(&geth->func);

	DBG(ecm->port.func.config->cdev, "%s\n", __func__);

	ecm->is_open = true;
	ecm_notify(ecm);
}

static void ecm_close(struct gether *geth)
{
	struct f_ecm		*ecm = func_to_ecm(&geth->func);

	DBG(ecm->port.func.config->cdev, "%s\n", __func__);

	ecm->is_open = false;
	ecm_notify(ecm);
}

 

 

static int
ecm_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_ecm		*ecm = func_to_ecm(f);
	struct usb_string	*us;
	int			status = 0;
	struct usb_ep		*ep;

	struct f_ecm_opts	*ecm_opts;

	if (!can_support_ecm(cdev->gadget))
		return -EINVAL;

	ecm_opts = container_of(f->fi, struct f_ecm_opts, func_inst);

	mutex_lock(&ecm_opts->lock);

	gether_set_gadget(ecm_opts->net, cdev->gadget);

	if (!ecm_opts->bound) {
		status = gether_register_netdev(ecm_opts->net);
		ecm_opts->bound = true;
	}

	mutex_unlock(&ecm_opts->lock);
	if (status)
		return status;

	ecm_string_defs[1].s = ecm->ethaddr;

	us = usb_gstrings_attach(cdev, ecm_strings,
				 ARRAY_SIZE(ecm_string_defs));
	if (IS_ERR(us))
		return PTR_ERR(us);
	ecm_control_intf.iInterface = us[0].id;
	ecm_data_intf.iInterface = us[2].id;
	ecm_desc.iMACAddress = us[1].id;
	ecm_iad_descriptor.iFunction = us[3].id;

	 
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	ecm->ctrl_id = status;
	ecm_iad_descriptor.bFirstInterface = status;

	ecm_control_intf.bInterfaceNumber = status;
	ecm_union_desc.bMasterInterface0 = status;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	ecm->data_id = status;

	ecm_data_nop_intf.bInterfaceNumber = status;
	ecm_data_intf.bInterfaceNumber = status;
	ecm_union_desc.bSlaveInterface0 = status;

	status = -ENODEV;

	 
	ep = usb_ep_autoconfig(cdev->gadget, &fs_ecm_in_desc);
	if (!ep)
		goto fail;
	ecm->port.in_ep = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &fs_ecm_out_desc);
	if (!ep)
		goto fail;
	ecm->port.out_ep = ep;

	 
	ep = usb_ep_autoconfig(cdev->gadget, &fs_ecm_notify_desc);
	if (!ep)
		goto fail;
	ecm->notify = ep;

	status = -ENOMEM;

	 
	ecm->notify_req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!ecm->notify_req)
		goto fail;
	ecm->notify_req->buf = kmalloc(ECM_STATUS_BYTECOUNT, GFP_KERNEL);
	if (!ecm->notify_req->buf)
		goto fail;
	ecm->notify_req->context = ecm;
	ecm->notify_req->complete = ecm_notify_complete;

	 
	hs_ecm_in_desc.bEndpointAddress = fs_ecm_in_desc.bEndpointAddress;
	hs_ecm_out_desc.bEndpointAddress = fs_ecm_out_desc.bEndpointAddress;
	hs_ecm_notify_desc.bEndpointAddress =
		fs_ecm_notify_desc.bEndpointAddress;

	ss_ecm_in_desc.bEndpointAddress = fs_ecm_in_desc.bEndpointAddress;
	ss_ecm_out_desc.bEndpointAddress = fs_ecm_out_desc.bEndpointAddress;
	ss_ecm_notify_desc.bEndpointAddress =
		fs_ecm_notify_desc.bEndpointAddress;

	status = usb_assign_descriptors(f, ecm_fs_function, ecm_hs_function,
			ecm_ss_function, ecm_ss_function);
	if (status)
		goto fail;

	 

	ecm->port.open = ecm_open;
	ecm->port.close = ecm_close;

	DBG(cdev, "CDC Ethernet: IN/%s OUT/%s NOTIFY/%s\n",
			ecm->port.in_ep->name, ecm->port.out_ep->name,
			ecm->notify->name);
	return 0;

fail:
	if (ecm->notify_req) {
		kfree(ecm->notify_req->buf);
		usb_ep_free_request(ecm->notify, ecm->notify_req);
	}

	ERROR(cdev, "%s: can't bind, err %d\n", f->name, status);

	return status;
}

static inline struct f_ecm_opts *to_f_ecm_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_ecm_opts,
			    func_inst.group);
}

 
USB_ETHERNET_CONFIGFS_ITEM(ecm);

 
USB_ETHERNET_CONFIGFS_ITEM_ATTR_DEV_ADDR(ecm);

 
USB_ETHERNET_CONFIGFS_ITEM_ATTR_HOST_ADDR(ecm);

 
USB_ETHERNET_CONFIGFS_ITEM_ATTR_QMULT(ecm);

 
USB_ETHERNET_CONFIGFS_ITEM_ATTR_IFNAME(ecm);

static struct configfs_attribute *ecm_attrs[] = {
	&ecm_opts_attr_dev_addr,
	&ecm_opts_attr_host_addr,
	&ecm_opts_attr_qmult,
	&ecm_opts_attr_ifname,
	NULL,
};

static const struct config_item_type ecm_func_type = {
	.ct_item_ops	= &ecm_item_ops,
	.ct_attrs	= ecm_attrs,
	.ct_owner	= THIS_MODULE,
};

static void ecm_free_inst(struct usb_function_instance *f)
{
	struct f_ecm_opts *opts;

	opts = container_of(f, struct f_ecm_opts, func_inst);
	if (opts->bound)
		gether_cleanup(netdev_priv(opts->net));
	else
		free_netdev(opts->net);
	kfree(opts);
}

static struct usb_function_instance *ecm_alloc_inst(void)
{
	struct f_ecm_opts *opts;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);
	mutex_init(&opts->lock);
	opts->func_inst.free_func_inst = ecm_free_inst;
	opts->net = gether_setup_default();
	if (IS_ERR(opts->net)) {
		struct net_device *net = opts->net;
		kfree(opts);
		return ERR_CAST(net);
	}

	config_group_init_type_name(&opts->func_inst.group, "", &ecm_func_type);

	return &opts->func_inst;
}

static void ecm_suspend(struct usb_function *f)
{
	struct f_ecm *ecm = func_to_ecm(f);
	struct usb_composite_dev *cdev = ecm->port.func.config->cdev;

	DBG(cdev, "ECM Suspend\n");

	gether_suspend(&ecm->port);
}

static void ecm_resume(struct usb_function *f)
{
	struct f_ecm *ecm = func_to_ecm(f);
	struct usb_composite_dev *cdev = ecm->port.func.config->cdev;

	DBG(cdev, "ECM Resume\n");

	gether_resume(&ecm->port);
}

static void ecm_free(struct usb_function *f)
{
	struct f_ecm *ecm;
	struct f_ecm_opts *opts;

	ecm = func_to_ecm(f);
	opts = container_of(f->fi, struct f_ecm_opts, func_inst);
	kfree(ecm);
	mutex_lock(&opts->lock);
	opts->refcnt--;
	mutex_unlock(&opts->lock);
}

static void ecm_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_ecm		*ecm = func_to_ecm(f);

	DBG(c->cdev, "ecm unbind\n");

	usb_free_all_descriptors(f);

	if (atomic_read(&ecm->notify_count)) {
		usb_ep_dequeue(ecm->notify, ecm->notify_req);
		atomic_set(&ecm->notify_count, 0);
	}

	kfree(ecm->notify_req->buf);
	usb_ep_free_request(ecm->notify, ecm->notify_req);
}

static struct usb_function *ecm_alloc(struct usb_function_instance *fi)
{
	struct f_ecm	*ecm;
	struct f_ecm_opts *opts;
	int status;

	 
	ecm = kzalloc(sizeof(*ecm), GFP_KERNEL);
	if (!ecm)
		return ERR_PTR(-ENOMEM);

	opts = container_of(fi, struct f_ecm_opts, func_inst);
	mutex_lock(&opts->lock);
	opts->refcnt++;

	 
	status = gether_get_host_addr_cdc(opts->net, ecm->ethaddr,
					  sizeof(ecm->ethaddr));
	if (status < 12) {
		kfree(ecm);
		mutex_unlock(&opts->lock);
		return ERR_PTR(-EINVAL);
	}

	ecm->port.ioport = netdev_priv(opts->net);
	mutex_unlock(&opts->lock);
	ecm->port.cdc_filter = DEFAULT_FILTER;

	ecm->port.func.name = "cdc_ethernet";
	 
	ecm->port.func.bind = ecm_bind;
	ecm->port.func.unbind = ecm_unbind;
	ecm->port.func.set_alt = ecm_set_alt;
	ecm->port.func.get_alt = ecm_get_alt;
	ecm->port.func.setup = ecm_setup;
	ecm->port.func.disable = ecm_disable;
	ecm->port.func.free_func = ecm_free;
	ecm->port.func.suspend = ecm_suspend;
	ecm->port.func.resume = ecm_resume;

	return &ecm->port.func;
}

DECLARE_USB_FUNCTION_INIT(ecm, ecm_alloc_inst, ecm_alloc);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Brownell");
