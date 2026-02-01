
 

 

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/etherdevice.h>

#include <linux/atomic.h>

#include "u_ether.h"
#include "u_ether_configfs.h"
#include "u_rndis.h"
#include "rndis.h"
#include "configfs.h"

 

struct f_rndis {
	struct gether			port;
	u8				ctrl_id, data_id;
	u8				ethaddr[ETH_ALEN];
	u32				vendorID;
	const char			*manufacturer;
	struct rndis_params		*params;

	struct usb_ep			*notify;
	struct usb_request		*notify_req;
	atomic_t			notify_count;
};

static inline struct f_rndis *func_to_rndis(struct usb_function *f)
{
	return container_of(f, struct f_rndis, port.func);
}

 

 

#define RNDIS_STATUS_INTERVAL_MS	32
#define STATUS_BYTECOUNT		8	 


 

static struct usb_interface_descriptor rndis_control_intf = {
	.bLength =		sizeof rndis_control_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	 
	 
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_COMM,
	.bInterfaceSubClass =   USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol =   USB_CDC_ACM_PROTO_VENDOR,
	 
};

static struct usb_cdc_header_desc header_desc = {
	.bLength =		sizeof header_desc,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,

	.bcdCDC =		cpu_to_le16(0x0110),
};

static struct usb_cdc_call_mgmt_descriptor call_mgmt_descriptor = {
	.bLength =		sizeof call_mgmt_descriptor,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_CALL_MANAGEMENT_TYPE,

	.bmCapabilities =	0x00,
	.bDataInterface =	0x01,
};

static struct usb_cdc_acm_descriptor rndis_acm_descriptor = {
	.bLength =		sizeof rndis_acm_descriptor,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_ACM_TYPE,

	.bmCapabilities =	0x00,
};

static struct usb_cdc_union_desc rndis_union_desc = {
	.bLength =		sizeof(rndis_union_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_UNION_TYPE,
	 
	 
};

 

static struct usb_interface_descriptor rndis_data_intf = {
	.bLength =		sizeof rndis_data_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	 
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	 
};


static struct usb_interface_assoc_descriptor
rndis_iad_descriptor = {
	.bLength =		sizeof rndis_iad_descriptor,
	.bDescriptorType =	USB_DT_INTERFACE_ASSOCIATION,

	.bFirstInterface =	0,  
	.bInterfaceCount = 	2,	
	.bFunctionClass =	USB_CLASS_COMM,
	.bFunctionSubClass =	USB_CDC_SUBCLASS_ETHERNET,
	.bFunctionProtocol =	USB_CDC_PROTO_NONE,
	 
};

 

static struct usb_endpoint_descriptor fs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(STATUS_BYTECOUNT),
	.bInterval =		RNDIS_STATUS_INTERVAL_MS,
};

static struct usb_endpoint_descriptor fs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *eth_fs_function[] = {
	(struct usb_descriptor_header *) &rndis_iad_descriptor,

	 
	(struct usb_descriptor_header *) &rndis_control_intf,
	(struct usb_descriptor_header *) &header_desc,
	(struct usb_descriptor_header *) &call_mgmt_descriptor,
	(struct usb_descriptor_header *) &rndis_acm_descriptor,
	(struct usb_descriptor_header *) &rndis_union_desc,
	(struct usb_descriptor_header *) &fs_notify_desc,

	 
	(struct usb_descriptor_header *) &rndis_data_intf,
	(struct usb_descriptor_header *) &fs_in_desc,
	(struct usb_descriptor_header *) &fs_out_desc,
	NULL,
};

 

static struct usb_endpoint_descriptor hs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(STATUS_BYTECOUNT),
	.bInterval =		USB_MS_TO_HS_INTERVAL(RNDIS_STATUS_INTERVAL_MS)
};

static struct usb_endpoint_descriptor hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *eth_hs_function[] = {
	(struct usb_descriptor_header *) &rndis_iad_descriptor,

	 
	(struct usb_descriptor_header *) &rndis_control_intf,
	(struct usb_descriptor_header *) &header_desc,
	(struct usb_descriptor_header *) &call_mgmt_descriptor,
	(struct usb_descriptor_header *) &rndis_acm_descriptor,
	(struct usb_descriptor_header *) &rndis_union_desc,
	(struct usb_descriptor_header *) &hs_notify_desc,

	 
	(struct usb_descriptor_header *) &rndis_data_intf,
	(struct usb_descriptor_header *) &hs_in_desc,
	(struct usb_descriptor_header *) &hs_out_desc,
	NULL,
};

 

static struct usb_endpoint_descriptor ss_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(STATUS_BYTECOUNT),
	.bInterval =		USB_MS_TO_HS_INTERVAL(RNDIS_STATUS_INTERVAL_MS)
};

static struct usb_ss_ep_comp_descriptor ss_intr_comp_desc = {
	.bLength =		sizeof ss_intr_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	 
	 
	 
	.wBytesPerInterval =	cpu_to_le16(STATUS_BYTECOUNT),
};

static struct usb_endpoint_descriptor ss_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor ss_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_bulk_comp_desc = {
	.bLength =		sizeof ss_bulk_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	 
	 
	 
};

static struct usb_descriptor_header *eth_ss_function[] = {
	(struct usb_descriptor_header *) &rndis_iad_descriptor,

	 
	(struct usb_descriptor_header *) &rndis_control_intf,
	(struct usb_descriptor_header *) &header_desc,
	(struct usb_descriptor_header *) &call_mgmt_descriptor,
	(struct usb_descriptor_header *) &rndis_acm_descriptor,
	(struct usb_descriptor_header *) &rndis_union_desc,
	(struct usb_descriptor_header *) &ss_notify_desc,
	(struct usb_descriptor_header *) &ss_intr_comp_desc,

	 
	(struct usb_descriptor_header *) &rndis_data_intf,
	(struct usb_descriptor_header *) &ss_in_desc,
	(struct usb_descriptor_header *) &ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &ss_out_desc,
	(struct usb_descriptor_header *) &ss_bulk_comp_desc,
	NULL,
};

 

static struct usb_string rndis_string_defs[] = {
	[0].s = "RNDIS Communications Control",
	[1].s = "RNDIS Ethernet Data",
	[2].s = "RNDIS",
	{  }  
};

static struct usb_gadget_strings rndis_string_table = {
	.language =		0x0409,	 
	.strings =		rndis_string_defs,
};

static struct usb_gadget_strings *rndis_strings[] = {
	&rndis_string_table,
	NULL,
};

 

static struct sk_buff *rndis_add_header(struct gether *port,
					struct sk_buff *skb)
{
	struct sk_buff *skb2;

	if (!skb)
		return NULL;

	skb2 = skb_realloc_headroom(skb, sizeof(struct rndis_packet_msg_type));
	rndis_add_hdr(skb2);

	dev_kfree_skb(skb);
	return skb2;
}

static void rndis_response_available(void *_rndis)
{
	struct f_rndis			*rndis = _rndis;
	struct usb_request		*req = rndis->notify_req;
	struct usb_composite_dev	*cdev = rndis->port.func.config->cdev;
	__le32				*data = req->buf;
	int				status;

	if (atomic_inc_return(&rndis->notify_count) != 1)
		return;

	 
	data[0] = cpu_to_le32(1);
	data[1] = cpu_to_le32(0);

	status = usb_ep_queue(rndis->notify, req, GFP_ATOMIC);
	if (status) {
		atomic_dec(&rndis->notify_count);
		DBG(cdev, "notify/0 --> %d\n", status);
	}
}

static void rndis_response_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_rndis			*rndis = req->context;
	struct usb_composite_dev	*cdev = rndis->port.func.config->cdev;
	int				status = req->status;

	 
	switch (status) {
	case -ECONNRESET:
	case -ESHUTDOWN:
		 
		atomic_set(&rndis->notify_count, 0);
		break;
	default:
		DBG(cdev, "RNDIS %s response error %d, %d/%d\n",
			ep->name, status,
			req->actual, req->length);
		fallthrough;
	case 0:
		if (ep != rndis->notify)
			break;

		 
		if (atomic_dec_and_test(&rndis->notify_count))
			break;
		status = usb_ep_queue(rndis->notify, req, GFP_ATOMIC);
		if (status) {
			atomic_dec(&rndis->notify_count);
			DBG(cdev, "notify/1 --> %d\n", status);
		}
		break;
	}
}

static void rndis_command_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_rndis			*rndis = req->context;
	int				status;

	 
 
	status = rndis_msg_parser(rndis->params, (u8 *) req->buf);
	if (status < 0)
		pr_err("RNDIS command error %d, %d/%d\n",
			status, req->actual, req->length);
 
}

static int
rndis_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct f_rndis		*rndis = func_to_rndis(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	 
	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {

	 
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_SEND_ENCAPSULATED_COMMAND:
		if (w_value || w_index != rndis->ctrl_id)
			goto invalid;
		 
		value = w_length;
		req->complete = rndis_command_complete;
		req->context = rndis;
		 
		break;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_GET_ENCAPSULATED_RESPONSE:
		if (w_value || w_index != rndis->ctrl_id)
			goto invalid;
		else {
			u8 *buf;
			u32 n;

			 
			buf = rndis_get_next_response(rndis->params, &n);
			if (buf) {
				memcpy(req->buf, buf, n);
				req->complete = rndis_response_complete;
				req->context = rndis;
				rndis_free_response(rndis->params, buf);
				value = n;
			}
			 
		}
		break;

	default:
invalid:
		VDBG(cdev, "invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	 
	if (value >= 0) {
		DBG(cdev, "rndis req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = (value < w_length);
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			ERROR(cdev, "rndis response on err %d\n", value);
	}

	 
	return value;
}


static int rndis_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_rndis		*rndis = func_to_rndis(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	 

	if (intf == rndis->ctrl_id) {
		VDBG(cdev, "reset rndis control %d\n", intf);
		usb_ep_disable(rndis->notify);

		if (!rndis->notify->desc) {
			VDBG(cdev, "init rndis ctrl %d\n", intf);
			if (config_ep_by_speed(cdev->gadget, f, rndis->notify))
				goto fail;
		}
		usb_ep_enable(rndis->notify);

	} else if (intf == rndis->data_id) {
		struct net_device	*net;

		if (rndis->port.in_ep->enabled) {
			DBG(cdev, "reset rndis\n");
			gether_disconnect(&rndis->port);
		}

		if (!rndis->port.in_ep->desc || !rndis->port.out_ep->desc) {
			DBG(cdev, "init rndis\n");
			if (config_ep_by_speed(cdev->gadget, f,
					       rndis->port.in_ep) ||
			    config_ep_by_speed(cdev->gadget, f,
					       rndis->port.out_ep)) {
				rndis->port.in_ep->desc = NULL;
				rndis->port.out_ep->desc = NULL;
				goto fail;
			}
		}

		 
		rndis->port.is_zlp_ok = false;

		 
		rndis->port.cdc_filter = 0;

		DBG(cdev, "RNDIS RX/TX early activation ... \n");
		net = gether_connect(&rndis->port);
		if (IS_ERR(net))
			return PTR_ERR(net);

		rndis_set_param_dev(rndis->params, net,
				&rndis->port.cdc_filter);
	} else
		goto fail;

	return 0;
fail:
	return -EINVAL;
}

static void rndis_disable(struct usb_function *f)
{
	struct f_rndis		*rndis = func_to_rndis(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	if (!rndis->notify->enabled)
		return;

	DBG(cdev, "rndis deactivated\n");

	rndis_uninit(rndis->params);
	gether_disconnect(&rndis->port);

	usb_ep_disable(rndis->notify);
	rndis->notify->desc = NULL;
}

 

 

static void rndis_open(struct gether *geth)
{
	struct f_rndis		*rndis = func_to_rndis(&geth->func);
	struct usb_composite_dev *cdev = geth->func.config->cdev;

	DBG(cdev, "%s\n", __func__);

	rndis_set_param_medium(rndis->params, RNDIS_MEDIUM_802_3,
				gether_bitrate(cdev->gadget) / 100);
	rndis_signal_connect(rndis->params);
}

static void rndis_close(struct gether *geth)
{
	struct f_rndis		*rndis = func_to_rndis(&geth->func);

	DBG(geth->func.config->cdev, "%s\n", __func__);

	rndis_set_param_medium(rndis->params, RNDIS_MEDIUM_802_3, 0);
	rndis_signal_disconnect(rndis->params);
}

 

 
static inline bool can_support_rndis(struct usb_configuration *c)
{
	 
	return true;
}

 

static int
rndis_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_rndis		*rndis = func_to_rndis(f);
	struct usb_string	*us;
	int			status;
	struct usb_ep		*ep;

	struct f_rndis_opts *rndis_opts;

	if (!can_support_rndis(c))
		return -EINVAL;

	rndis_opts = container_of(f->fi, struct f_rndis_opts, func_inst);

	if (cdev->use_os_string) {
		f->os_desc_table = kzalloc(sizeof(*f->os_desc_table),
					   GFP_KERNEL);
		if (!f->os_desc_table)
			return -ENOMEM;
		f->os_desc_n = 1;
		f->os_desc_table[0].os_desc = &rndis_opts->rndis_os_desc;
	}

	rndis_iad_descriptor.bFunctionClass = rndis_opts->class;
	rndis_iad_descriptor.bFunctionSubClass = rndis_opts->subclass;
	rndis_iad_descriptor.bFunctionProtocol = rndis_opts->protocol;

	 
	if (!rndis_opts->bound) {
		gether_set_gadget(rndis_opts->net, cdev->gadget);
		status = gether_register_netdev(rndis_opts->net);
		if (status)
			goto fail;
		rndis_opts->bound = true;
	}

	us = usb_gstrings_attach(cdev, rndis_strings,
				 ARRAY_SIZE(rndis_string_defs));
	if (IS_ERR(us)) {
		status = PTR_ERR(us);
		goto fail;
	}
	rndis_control_intf.iInterface = us[0].id;
	rndis_data_intf.iInterface = us[1].id;
	rndis_iad_descriptor.iFunction = us[2].id;

	 
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	rndis->ctrl_id = status;
	rndis_iad_descriptor.bFirstInterface = status;

	rndis_control_intf.bInterfaceNumber = status;
	rndis_union_desc.bMasterInterface0 = status;

	if (cdev->use_os_string)
		f->os_desc_table[0].if_id =
			rndis_iad_descriptor.bFirstInterface;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	rndis->data_id = status;

	rndis_data_intf.bInterfaceNumber = status;
	rndis_union_desc.bSlaveInterface0 = status;

	status = -ENODEV;

	 
	ep = usb_ep_autoconfig(cdev->gadget, &fs_in_desc);
	if (!ep)
		goto fail;
	rndis->port.in_ep = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &fs_out_desc);
	if (!ep)
		goto fail;
	rndis->port.out_ep = ep;

	 
	ep = usb_ep_autoconfig(cdev->gadget, &fs_notify_desc);
	if (!ep)
		goto fail;
	rndis->notify = ep;

	status = -ENOMEM;

	 
	rndis->notify_req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!rndis->notify_req)
		goto fail;
	rndis->notify_req->buf = kmalloc(STATUS_BYTECOUNT, GFP_KERNEL);
	if (!rndis->notify_req->buf)
		goto fail;
	rndis->notify_req->length = STATUS_BYTECOUNT;
	rndis->notify_req->context = rndis;
	rndis->notify_req->complete = rndis_response_complete;

	 
	hs_in_desc.bEndpointAddress = fs_in_desc.bEndpointAddress;
	hs_out_desc.bEndpointAddress = fs_out_desc.bEndpointAddress;
	hs_notify_desc.bEndpointAddress = fs_notify_desc.bEndpointAddress;

	ss_in_desc.bEndpointAddress = fs_in_desc.bEndpointAddress;
	ss_out_desc.bEndpointAddress = fs_out_desc.bEndpointAddress;
	ss_notify_desc.bEndpointAddress = fs_notify_desc.bEndpointAddress;

	status = usb_assign_descriptors(f, eth_fs_function, eth_hs_function,
			eth_ss_function, eth_ss_function);
	if (status)
		goto fail;

	rndis->port.open = rndis_open;
	rndis->port.close = rndis_close;

	rndis_set_param_medium(rndis->params, RNDIS_MEDIUM_802_3, 0);
	rndis_set_host_mac(rndis->params, rndis->ethaddr);

	if (rndis->manufacturer && rndis->vendorID &&
			rndis_set_param_vendor(rndis->params, rndis->vendorID,
					       rndis->manufacturer)) {
		status = -EINVAL;
		goto fail_free_descs;
	}

	 

	DBG(cdev, "RNDIS: IN/%s OUT/%s NOTIFY/%s\n",
			rndis->port.in_ep->name, rndis->port.out_ep->name,
			rndis->notify->name);
	return 0;

fail_free_descs:
	usb_free_all_descriptors(f);
fail:
	kfree(f->os_desc_table);
	f->os_desc_n = 0;

	if (rndis->notify_req) {
		kfree(rndis->notify_req->buf);
		usb_ep_free_request(rndis->notify, rndis->notify_req);
	}

	ERROR(cdev, "%s: can't bind, err %d\n", f->name, status);

	return status;
}

void rndis_borrow_net(struct usb_function_instance *f, struct net_device *net)
{
	struct f_rndis_opts *opts;

	opts = container_of(f, struct f_rndis_opts, func_inst);
	if (opts->bound)
		gether_cleanup(netdev_priv(opts->net));
	else
		free_netdev(opts->net);
	opts->borrowed_net = opts->bound = true;
	opts->net = net;
}
EXPORT_SYMBOL_GPL(rndis_borrow_net);

static inline struct f_rndis_opts *to_f_rndis_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_rndis_opts,
			    func_inst.group);
}

 
USB_ETHERNET_CONFIGFS_ITEM(rndis);

 
USB_ETHERNET_CONFIGFS_ITEM_ATTR_DEV_ADDR(rndis);

 
USB_ETHERNET_CONFIGFS_ITEM_ATTR_HOST_ADDR(rndis);

 
USB_ETHERNET_CONFIGFS_ITEM_ATTR_QMULT(rndis);

 
USB_ETHERNET_CONFIGFS_ITEM_ATTR_IFNAME(rndis);

 
USB_ETHER_CONFIGFS_ITEM_ATTR_U8_RW(rndis, class);

 
USB_ETHER_CONFIGFS_ITEM_ATTR_U8_RW(rndis, subclass);

 
USB_ETHER_CONFIGFS_ITEM_ATTR_U8_RW(rndis, protocol);

static struct configfs_attribute *rndis_attrs[] = {
	&rndis_opts_attr_dev_addr,
	&rndis_opts_attr_host_addr,
	&rndis_opts_attr_qmult,
	&rndis_opts_attr_ifname,
	&rndis_opts_attr_class,
	&rndis_opts_attr_subclass,
	&rndis_opts_attr_protocol,
	NULL,
};

static const struct config_item_type rndis_func_type = {
	.ct_item_ops	= &rndis_item_ops,
	.ct_attrs	= rndis_attrs,
	.ct_owner	= THIS_MODULE,
};

static void rndis_free_inst(struct usb_function_instance *f)
{
	struct f_rndis_opts *opts;

	opts = container_of(f, struct f_rndis_opts, func_inst);
	if (!opts->borrowed_net) {
		if (opts->bound)
			gether_cleanup(netdev_priv(opts->net));
		else
			free_netdev(opts->net);
	}

	kfree(opts->rndis_interf_group);	 
	kfree(opts);
}

static struct usb_function_instance *rndis_alloc_inst(void)
{
	struct f_rndis_opts *opts;
	struct usb_os_desc *descs[1];
	char *names[1];
	struct config_group *rndis_interf_group;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);
	opts->rndis_os_desc.ext_compat_id = opts->rndis_ext_compat_id;

	mutex_init(&opts->lock);
	opts->func_inst.free_func_inst = rndis_free_inst;
	opts->net = gether_setup_default();
	if (IS_ERR(opts->net)) {
		struct net_device *net = opts->net;
		kfree(opts);
		return ERR_CAST(net);
	}
	INIT_LIST_HEAD(&opts->rndis_os_desc.ext_prop);

	opts->class = rndis_iad_descriptor.bFunctionClass;
	opts->subclass = rndis_iad_descriptor.bFunctionSubClass;
	opts->protocol = rndis_iad_descriptor.bFunctionProtocol;

	descs[0] = &opts->rndis_os_desc;
	names[0] = "rndis";
	config_group_init_type_name(&opts->func_inst.group, "",
				    &rndis_func_type);
	rndis_interf_group =
		usb_os_desc_prepare_interf_dir(&opts->func_inst.group, 1, descs,
					       names, THIS_MODULE);
	if (IS_ERR(rndis_interf_group)) {
		rndis_free_inst(&opts->func_inst);
		return ERR_CAST(rndis_interf_group);
	}
	opts->rndis_interf_group = rndis_interf_group;

	return &opts->func_inst;
}

static void rndis_free(struct usb_function *f)
{
	struct f_rndis *rndis;
	struct f_rndis_opts *opts;

	rndis = func_to_rndis(f);
	rndis_deregister(rndis->params);
	opts = container_of(f->fi, struct f_rndis_opts, func_inst);
	kfree(rndis);
	mutex_lock(&opts->lock);
	opts->refcnt--;
	mutex_unlock(&opts->lock);
}

static void rndis_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_rndis		*rndis = func_to_rndis(f);

	kfree(f->os_desc_table);
	f->os_desc_n = 0;
	usb_free_all_descriptors(f);

	kfree(rndis->notify_req->buf);
	usb_ep_free_request(rndis->notify, rndis->notify_req);
}

static struct usb_function *rndis_alloc(struct usb_function_instance *fi)
{
	struct f_rndis	*rndis;
	struct f_rndis_opts *opts;
	struct rndis_params *params;

	 
	rndis = kzalloc(sizeof(*rndis), GFP_KERNEL);
	if (!rndis)
		return ERR_PTR(-ENOMEM);

	opts = container_of(fi, struct f_rndis_opts, func_inst);
	mutex_lock(&opts->lock);
	opts->refcnt++;

	gether_get_host_addr_u8(opts->net, rndis->ethaddr);
	rndis->vendorID = opts->vendor_id;
	rndis->manufacturer = opts->manufacturer;

	rndis->port.ioport = netdev_priv(opts->net);
	mutex_unlock(&opts->lock);
	 
	rndis->port.cdc_filter = 0;

	 
	rndis->port.header_len = sizeof(struct rndis_packet_msg_type);
	rndis->port.wrap = rndis_add_header;
	rndis->port.unwrap = rndis_rm_hdr;

	rndis->port.func.name = "rndis";
	 
	rndis->port.func.bind = rndis_bind;
	rndis->port.func.unbind = rndis_unbind;
	rndis->port.func.set_alt = rndis_set_alt;
	rndis->port.func.setup = rndis_setup;
	rndis->port.func.disable = rndis_disable;
	rndis->port.func.free_func = rndis_free;

	params = rndis_register(rndis_response_available, rndis);
	if (IS_ERR(params)) {
		kfree(rndis);
		return ERR_CAST(params);
	}
	rndis->params = params;

	return &rndis->port.func;
}

DECLARE_USB_FUNCTION_INIT(rndis, rndis_alloc_inst, rndis_alloc);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Brownell");
