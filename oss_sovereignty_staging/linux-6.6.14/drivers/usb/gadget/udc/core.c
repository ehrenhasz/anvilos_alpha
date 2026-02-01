
 

#define pr_fmt(fmt)	"UDC core: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/idr.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/sched/task_stack.h>
#include <linux/workqueue.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb.h>

#include "trace.h"

static DEFINE_IDA(gadget_id_numbers);

static const struct bus_type gadget_bus_type;

 
struct usb_udc {
	struct usb_gadget_driver	*driver;
	struct usb_gadget		*gadget;
	struct device			dev;
	struct list_head		list;
	bool				vbus;
	bool				started;
	bool				allow_connect;
	struct work_struct		vbus_work;
	struct mutex			connect_lock;
};

static const struct class udc_class;
static LIST_HEAD(udc_list);

 
static DEFINE_MUTEX(udc_lock);

 

 
void usb_ep_set_maxpacket_limit(struct usb_ep *ep,
					      unsigned maxpacket_limit)
{
	ep->maxpacket_limit = maxpacket_limit;
	ep->maxpacket = maxpacket_limit;

	trace_usb_ep_set_maxpacket_limit(ep, 0);
}
EXPORT_SYMBOL_GPL(usb_ep_set_maxpacket_limit);

 
int usb_ep_enable(struct usb_ep *ep)
{
	int ret = 0;

	if (ep->enabled)
		goto out;

	 
	if (usb_endpoint_maxp(ep->desc) == 0) {
		 
		ret = -EINVAL;
		goto out;
	}

	ret = ep->ops->enable(ep, ep->desc);
	if (ret)
		goto out;

	ep->enabled = true;

out:
	trace_usb_ep_enable(ep, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_ep_enable);

 
int usb_ep_disable(struct usb_ep *ep)
{
	int ret = 0;

	if (!ep->enabled)
		goto out;

	ret = ep->ops->disable(ep);
	if (ret)
		goto out;

	ep->enabled = false;

out:
	trace_usb_ep_disable(ep, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_ep_disable);

 
struct usb_request *usb_ep_alloc_request(struct usb_ep *ep,
						       gfp_t gfp_flags)
{
	struct usb_request *req = NULL;

	req = ep->ops->alloc_request(ep, gfp_flags);

	trace_usb_ep_alloc_request(ep, req, req ? 0 : -ENOMEM);

	return req;
}
EXPORT_SYMBOL_GPL(usb_ep_alloc_request);

 
void usb_ep_free_request(struct usb_ep *ep,
				       struct usb_request *req)
{
	trace_usb_ep_free_request(ep, req, 0);
	ep->ops->free_request(ep, req);
}
EXPORT_SYMBOL_GPL(usb_ep_free_request);

 
int usb_ep_queue(struct usb_ep *ep,
			       struct usb_request *req, gfp_t gfp_flags)
{
	int ret = 0;

	if (WARN_ON_ONCE(!ep->enabled && ep->address)) {
		ret = -ESHUTDOWN;
		goto out;
	}

	ret = ep->ops->queue(ep, req, gfp_flags);

out:
	trace_usb_ep_queue(ep, req, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_ep_queue);

 
int usb_ep_dequeue(struct usb_ep *ep, struct usb_request *req)
{
	int ret;

	ret = ep->ops->dequeue(ep, req);
	trace_usb_ep_dequeue(ep, req, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_ep_dequeue);

 
int usb_ep_set_halt(struct usb_ep *ep)
{
	int ret;

	ret = ep->ops->set_halt(ep, 1);
	trace_usb_ep_set_halt(ep, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_ep_set_halt);

 
int usb_ep_clear_halt(struct usb_ep *ep)
{
	int ret;

	ret = ep->ops->set_halt(ep, 0);
	trace_usb_ep_clear_halt(ep, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_ep_clear_halt);

 
int usb_ep_set_wedge(struct usb_ep *ep)
{
	int ret;

	if (ep->ops->set_wedge)
		ret = ep->ops->set_wedge(ep);
	else
		ret = ep->ops->set_halt(ep, 1);

	trace_usb_ep_set_wedge(ep, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_ep_set_wedge);

 
int usb_ep_fifo_status(struct usb_ep *ep)
{
	int ret;

	if (ep->ops->fifo_status)
		ret = ep->ops->fifo_status(ep);
	else
		ret = -EOPNOTSUPP;

	trace_usb_ep_fifo_status(ep, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_ep_fifo_status);

 
void usb_ep_fifo_flush(struct usb_ep *ep)
{
	if (ep->ops->fifo_flush)
		ep->ops->fifo_flush(ep);

	trace_usb_ep_fifo_flush(ep, 0);
}
EXPORT_SYMBOL_GPL(usb_ep_fifo_flush);

 

 
int usb_gadget_frame_number(struct usb_gadget *gadget)
{
	int ret;

	ret = gadget->ops->get_frame(gadget);

	trace_usb_gadget_frame_number(gadget, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_frame_number);

 
int usb_gadget_wakeup(struct usb_gadget *gadget)
{
	int ret = 0;

	if (!gadget->ops->wakeup) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = gadget->ops->wakeup(gadget);

out:
	trace_usb_gadget_wakeup(gadget, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_wakeup);

 
int usb_gadget_set_remote_wakeup(struct usb_gadget *gadget, int set)
{
	int ret = 0;

	if (!gadget->ops->set_remote_wakeup) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = gadget->ops->set_remote_wakeup(gadget, set);

out:
	trace_usb_gadget_set_remote_wakeup(gadget, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_set_remote_wakeup);

 
int usb_gadget_set_selfpowered(struct usb_gadget *gadget)
{
	int ret = 0;

	if (!gadget->ops->set_selfpowered) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = gadget->ops->set_selfpowered(gadget, 1);

out:
	trace_usb_gadget_set_selfpowered(gadget, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_set_selfpowered);

 
int usb_gadget_clear_selfpowered(struct usb_gadget *gadget)
{
	int ret = 0;

	if (!gadget->ops->set_selfpowered) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = gadget->ops->set_selfpowered(gadget, 0);

out:
	trace_usb_gadget_clear_selfpowered(gadget, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_clear_selfpowered);

 
int usb_gadget_vbus_connect(struct usb_gadget *gadget)
{
	int ret = 0;

	if (!gadget->ops->vbus_session) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = gadget->ops->vbus_session(gadget, 1);

out:
	trace_usb_gadget_vbus_connect(gadget, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_vbus_connect);

 
int usb_gadget_vbus_draw(struct usb_gadget *gadget, unsigned mA)
{
	int ret = 0;

	if (!gadget->ops->vbus_draw) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = gadget->ops->vbus_draw(gadget, mA);
	if (!ret)
		gadget->mA = mA;

out:
	trace_usb_gadget_vbus_draw(gadget, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_vbus_draw);

 
int usb_gadget_vbus_disconnect(struct usb_gadget *gadget)
{
	int ret = 0;

	if (!gadget->ops->vbus_session) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	ret = gadget->ops->vbus_session(gadget, 0);

out:
	trace_usb_gadget_vbus_disconnect(gadget, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_vbus_disconnect);

static int usb_gadget_connect_locked(struct usb_gadget *gadget)
	__must_hold(&gadget->udc->connect_lock)
{
	int ret = 0;

	if (!gadget->ops->pullup) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (gadget->deactivated || !gadget->udc->allow_connect || !gadget->udc->started) {
		 
		gadget->connected = true;
		goto out;
	}

	ret = gadget->ops->pullup(gadget, 1);
	if (!ret)
		gadget->connected = 1;

out:
	trace_usb_gadget_connect(gadget, ret);

	return ret;
}

 
int usb_gadget_connect(struct usb_gadget *gadget)
{
	int ret;

	mutex_lock(&gadget->udc->connect_lock);
	ret = usb_gadget_connect_locked(gadget);
	mutex_unlock(&gadget->udc->connect_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_connect);

static int usb_gadget_disconnect_locked(struct usb_gadget *gadget)
	__must_hold(&gadget->udc->connect_lock)
{
	int ret = 0;

	if (!gadget->ops->pullup) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (!gadget->connected)
		goto out;

	if (gadget->deactivated || !gadget->udc->started) {
		 
		gadget->connected = false;
		goto out;
	}

	ret = gadget->ops->pullup(gadget, 0);
	if (!ret)
		gadget->connected = 0;

	mutex_lock(&udc_lock);
	if (gadget->udc->driver)
		gadget->udc->driver->disconnect(gadget);
	mutex_unlock(&udc_lock);

out:
	trace_usb_gadget_disconnect(gadget, ret);

	return ret;
}

 
int usb_gadget_disconnect(struct usb_gadget *gadget)
{
	int ret;

	mutex_lock(&gadget->udc->connect_lock);
	ret = usb_gadget_disconnect_locked(gadget);
	mutex_unlock(&gadget->udc->connect_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_disconnect);

 
int usb_gadget_deactivate(struct usb_gadget *gadget)
{
	int ret = 0;

	mutex_lock(&gadget->udc->connect_lock);
	if (gadget->deactivated)
		goto unlock;

	if (gadget->connected) {
		ret = usb_gadget_disconnect_locked(gadget);
		if (ret)
			goto unlock;

		 
		gadget->connected = true;
	}
	gadget->deactivated = true;

unlock:
	mutex_unlock(&gadget->udc->connect_lock);
	trace_usb_gadget_deactivate(gadget, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_deactivate);

 
int usb_gadget_activate(struct usb_gadget *gadget)
{
	int ret = 0;

	mutex_lock(&gadget->udc->connect_lock);
	if (!gadget->deactivated)
		goto unlock;

	gadget->deactivated = false;

	 
	if (gadget->connected)
		ret = usb_gadget_connect_locked(gadget);

unlock:
	mutex_unlock(&gadget->udc->connect_lock);
	trace_usb_gadget_activate(gadget, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_activate);

 

#ifdef	CONFIG_HAS_DMA

int usb_gadget_map_request_by_dev(struct device *dev,
		struct usb_request *req, int is_in)
{
	if (req->length == 0)
		return 0;

	if (req->num_sgs) {
		int     mapped;

		mapped = dma_map_sg(dev, req->sg, req->num_sgs,
				is_in ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
		if (mapped == 0) {
			dev_err(dev, "failed to map SGs\n");
			return -EFAULT;
		}

		req->num_mapped_sgs = mapped;
	} else {
		if (is_vmalloc_addr(req->buf)) {
			dev_err(dev, "buffer is not dma capable\n");
			return -EFAULT;
		} else if (object_is_on_stack(req->buf)) {
			dev_err(dev, "buffer is on stack\n");
			return -EFAULT;
		}

		req->dma = dma_map_single(dev, req->buf, req->length,
				is_in ? DMA_TO_DEVICE : DMA_FROM_DEVICE);

		if (dma_mapping_error(dev, req->dma)) {
			dev_err(dev, "failed to map buffer\n");
			return -EFAULT;
		}

		req->dma_mapped = 1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(usb_gadget_map_request_by_dev);

int usb_gadget_map_request(struct usb_gadget *gadget,
		struct usb_request *req, int is_in)
{
	return usb_gadget_map_request_by_dev(gadget->dev.parent, req, is_in);
}
EXPORT_SYMBOL_GPL(usb_gadget_map_request);

void usb_gadget_unmap_request_by_dev(struct device *dev,
		struct usb_request *req, int is_in)
{
	if (req->length == 0)
		return;

	if (req->num_mapped_sgs) {
		dma_unmap_sg(dev, req->sg, req->num_sgs,
				is_in ? DMA_TO_DEVICE : DMA_FROM_DEVICE);

		req->num_mapped_sgs = 0;
	} else if (req->dma_mapped) {
		dma_unmap_single(dev, req->dma, req->length,
				is_in ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
		req->dma_mapped = 0;
	}
}
EXPORT_SYMBOL_GPL(usb_gadget_unmap_request_by_dev);

void usb_gadget_unmap_request(struct usb_gadget *gadget,
		struct usb_request *req, int is_in)
{
	usb_gadget_unmap_request_by_dev(gadget->dev.parent, req, is_in);
}
EXPORT_SYMBOL_GPL(usb_gadget_unmap_request);

#endif	 

 

 
void usb_gadget_giveback_request(struct usb_ep *ep,
		struct usb_request *req)
{
	if (likely(req->status == 0))
		usb_led_activity(USB_LED_EVENT_GADGET);

	trace_usb_gadget_giveback_request(ep, req, 0);

	req->complete(ep, req);
}
EXPORT_SYMBOL_GPL(usb_gadget_giveback_request);

 

 
struct usb_ep *gadget_find_ep_by_name(struct usb_gadget *g, const char *name)
{
	struct usb_ep *ep;

	gadget_for_each_ep(ep, g) {
		if (!strcmp(ep->name, name))
			return ep;
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(gadget_find_ep_by_name);

 

int usb_gadget_ep_match_desc(struct usb_gadget *gadget,
		struct usb_ep *ep, struct usb_endpoint_descriptor *desc,
		struct usb_ss_ep_comp_descriptor *ep_comp)
{
	u8		type;
	u16		max;
	int		num_req_streams = 0;

	 
	if (ep->claimed)
		return 0;

	type = usb_endpoint_type(desc);
	max = usb_endpoint_maxp(desc);

	if (usb_endpoint_dir_in(desc) && !ep->caps.dir_in)
		return 0;
	if (usb_endpoint_dir_out(desc) && !ep->caps.dir_out)
		return 0;

	if (max > ep->maxpacket_limit)
		return 0;

	 
	if (!gadget_is_dualspeed(gadget) && usb_endpoint_maxp_mult(desc) > 1)
		return 0;

	switch (type) {
	case USB_ENDPOINT_XFER_CONTROL:
		 
		return 0;
	case USB_ENDPOINT_XFER_ISOC:
		if (!ep->caps.type_iso)
			return 0;
		 
		if (!gadget_is_dualspeed(gadget) && max > 1023)
			return 0;
		break;
	case USB_ENDPOINT_XFER_BULK:
		if (!ep->caps.type_bulk)
			return 0;
		if (ep_comp && gadget_is_superspeed(gadget)) {
			 
			num_req_streams = ep_comp->bmAttributes & 0x1f;
			if (num_req_streams > ep->max_streams)
				return 0;
		}
		break;
	case USB_ENDPOINT_XFER_INT:
		 
		if (!ep->caps.type_int && !ep->caps.type_bulk)
			return 0;
		 
		if (!gadget_is_dualspeed(gadget) && max > 64)
			return 0;
		break;
	}

	return 1;
}
EXPORT_SYMBOL_GPL(usb_gadget_ep_match_desc);

 
int usb_gadget_check_config(struct usb_gadget *gadget)
{
	if (gadget->ops->check_config)
		return gadget->ops->check_config(gadget);
	return 0;
}
EXPORT_SYMBOL_GPL(usb_gadget_check_config);

 

static void usb_gadget_state_work(struct work_struct *work)
{
	struct usb_gadget *gadget = work_to_gadget(work);
	struct usb_udc *udc = gadget->udc;

	if (udc)
		sysfs_notify(&udc->dev.kobj, NULL, "state");
}

void usb_gadget_set_state(struct usb_gadget *gadget,
		enum usb_device_state state)
{
	gadget->state = state;
	schedule_work(&gadget->work);
}
EXPORT_SYMBOL_GPL(usb_gadget_set_state);

 

 
static void usb_udc_connect_control_locked(struct usb_udc *udc) __must_hold(&udc->connect_lock)
{
	if (udc->vbus)
		usb_gadget_connect_locked(udc->gadget);
	else
		usb_gadget_disconnect_locked(udc->gadget);
}

static void vbus_event_work(struct work_struct *work)
{
	struct usb_udc *udc = container_of(work, struct usb_udc, vbus_work);

	mutex_lock(&udc->connect_lock);
	usb_udc_connect_control_locked(udc);
	mutex_unlock(&udc->connect_lock);
}

 
void usb_udc_vbus_handler(struct usb_gadget *gadget, bool status)
{
	struct usb_udc *udc = gadget->udc;

	if (udc) {
		udc->vbus = status;
		schedule_work(&udc->vbus_work);
	}
}
EXPORT_SYMBOL_GPL(usb_udc_vbus_handler);

 
void usb_gadget_udc_reset(struct usb_gadget *gadget,
		struct usb_gadget_driver *driver)
{
	driver->reset(gadget);
	usb_gadget_set_state(gadget, USB_STATE_DEFAULT);
}
EXPORT_SYMBOL_GPL(usb_gadget_udc_reset);

 
static inline int usb_gadget_udc_start_locked(struct usb_udc *udc)
	__must_hold(&udc->connect_lock)
{
	int ret;

	if (udc->started) {
		dev_err(&udc->dev, "UDC had already started\n");
		return -EBUSY;
	}

	ret = udc->gadget->ops->udc_start(udc->gadget, udc->driver);
	if (!ret)
		udc->started = true;

	return ret;
}

 
static inline void usb_gadget_udc_stop_locked(struct usb_udc *udc)
	__must_hold(&udc->connect_lock)
{
	if (!udc->started) {
		dev_err(&udc->dev, "UDC had already stopped\n");
		return;
	}

	udc->gadget->ops->udc_stop(udc->gadget);
	udc->started = false;
}

 
static inline void usb_gadget_udc_set_speed(struct usb_udc *udc,
					    enum usb_device_speed speed)
{
	struct usb_gadget *gadget = udc->gadget;
	enum usb_device_speed s;

	if (speed == USB_SPEED_UNKNOWN)
		s = gadget->max_speed;
	else
		s = min(speed, gadget->max_speed);

	if (s == USB_SPEED_SUPER_PLUS && gadget->ops->udc_set_ssp_rate)
		gadget->ops->udc_set_ssp_rate(gadget, gadget->max_ssp_rate);
	else if (gadget->ops->udc_set_speed)
		gadget->ops->udc_set_speed(gadget, s);
}

 
static inline void usb_gadget_enable_async_callbacks(struct usb_udc *udc)
{
	struct usb_gadget *gadget = udc->gadget;

	if (gadget->ops->udc_async_callbacks)
		gadget->ops->udc_async_callbacks(gadget, true);
}

 
static inline void usb_gadget_disable_async_callbacks(struct usb_udc *udc)
{
	struct usb_gadget *gadget = udc->gadget;

	if (gadget->ops->udc_async_callbacks)
		gadget->ops->udc_async_callbacks(gadget, false);
}

 
static void usb_udc_release(struct device *dev)
{
	struct usb_udc *udc;

	udc = container_of(dev, struct usb_udc, dev);
	dev_dbg(dev, "releasing '%s'\n", dev_name(dev));
	kfree(udc);
}

static const struct attribute_group *usb_udc_attr_groups[];

static void usb_udc_nop_release(struct device *dev)
{
	dev_vdbg(dev, "%s\n", __func__);
}

 
void usb_initialize_gadget(struct device *parent, struct usb_gadget *gadget,
		void (*release)(struct device *dev))
{
	INIT_WORK(&gadget->work, usb_gadget_state_work);
	gadget->dev.parent = parent;

	if (release)
		gadget->dev.release = release;
	else
		gadget->dev.release = usb_udc_nop_release;

	device_initialize(&gadget->dev);
	gadget->dev.bus = &gadget_bus_type;
}
EXPORT_SYMBOL_GPL(usb_initialize_gadget);

 
int usb_add_gadget(struct usb_gadget *gadget)
{
	struct usb_udc		*udc;
	int			ret = -ENOMEM;

	udc = kzalloc(sizeof(*udc), GFP_KERNEL);
	if (!udc)
		goto error;

	device_initialize(&udc->dev);
	udc->dev.release = usb_udc_release;
	udc->dev.class = &udc_class;
	udc->dev.groups = usb_udc_attr_groups;
	udc->dev.parent = gadget->dev.parent;
	ret = dev_set_name(&udc->dev, "%s",
			kobject_name(&gadget->dev.parent->kobj));
	if (ret)
		goto err_put_udc;

	udc->gadget = gadget;
	gadget->udc = udc;
	mutex_init(&udc->connect_lock);

	udc->started = false;

	mutex_lock(&udc_lock);
	list_add_tail(&udc->list, &udc_list);
	mutex_unlock(&udc_lock);
	INIT_WORK(&udc->vbus_work, vbus_event_work);

	ret = device_add(&udc->dev);
	if (ret)
		goto err_unlist_udc;

	usb_gadget_set_state(gadget, USB_STATE_NOTATTACHED);
	udc->vbus = true;

	ret = ida_alloc(&gadget_id_numbers, GFP_KERNEL);
	if (ret < 0)
		goto err_del_udc;
	gadget->id_number = ret;
	dev_set_name(&gadget->dev, "gadget.%d", ret);

	ret = device_add(&gadget->dev);
	if (ret)
		goto err_free_id;

	return 0;

 err_free_id:
	ida_free(&gadget_id_numbers, gadget->id_number);

 err_del_udc:
	flush_work(&gadget->work);
	device_del(&udc->dev);

 err_unlist_udc:
	mutex_lock(&udc_lock);
	list_del(&udc->list);
	mutex_unlock(&udc_lock);

 err_put_udc:
	put_device(&udc->dev);

 error:
	return ret;
}
EXPORT_SYMBOL_GPL(usb_add_gadget);

 
int usb_add_gadget_udc_release(struct device *parent, struct usb_gadget *gadget,
		void (*release)(struct device *dev))
{
	int	ret;

	usb_initialize_gadget(parent, gadget, release);
	ret = usb_add_gadget(gadget);
	if (ret)
		usb_put_gadget(gadget);
	return ret;
}
EXPORT_SYMBOL_GPL(usb_add_gadget_udc_release);

 
char *usb_get_gadget_udc_name(void)
{
	struct usb_udc *udc;
	char *name = NULL;

	 
	mutex_lock(&udc_lock);
	list_for_each_entry(udc, &udc_list, list) {
		if (!udc->driver) {
			name = kstrdup(udc->gadget->name, GFP_KERNEL);
			break;
		}
	}
	mutex_unlock(&udc_lock);
	return name;
}
EXPORT_SYMBOL_GPL(usb_get_gadget_udc_name);

 
int usb_add_gadget_udc(struct device *parent, struct usb_gadget *gadget)
{
	return usb_add_gadget_udc_release(parent, gadget, NULL);
}
EXPORT_SYMBOL_GPL(usb_add_gadget_udc);

 
void usb_del_gadget(struct usb_gadget *gadget)
{
	struct usb_udc *udc = gadget->udc;

	if (!udc)
		return;

	dev_vdbg(gadget->dev.parent, "unregistering gadget\n");

	mutex_lock(&udc_lock);
	list_del(&udc->list);
	mutex_unlock(&udc_lock);

	kobject_uevent(&udc->dev.kobj, KOBJ_REMOVE);
	flush_work(&gadget->work);
	device_del(&gadget->dev);
	ida_free(&gadget_id_numbers, gadget->id_number);
	cancel_work_sync(&udc->vbus_work);
	device_unregister(&udc->dev);
}
EXPORT_SYMBOL_GPL(usb_del_gadget);

 
void usb_del_gadget_udc(struct usb_gadget *gadget)
{
	usb_del_gadget(gadget);
	usb_put_gadget(gadget);
}
EXPORT_SYMBOL_GPL(usb_del_gadget_udc);

 

static int gadget_match_driver(struct device *dev, struct device_driver *drv)
{
	struct usb_gadget *gadget = dev_to_usb_gadget(dev);
	struct usb_udc *udc = gadget->udc;
	struct usb_gadget_driver *driver = container_of(drv,
			struct usb_gadget_driver, driver);

	 
	if (driver->udc_name &&
			strcmp(driver->udc_name, dev_name(&udc->dev)) != 0)
		return 0;

	 
	if (driver->is_bound)
		return 0;

	 
	return 1;
}

static int gadget_bind_driver(struct device *dev)
{
	struct usb_gadget *gadget = dev_to_usb_gadget(dev);
	struct usb_udc *udc = gadget->udc;
	struct usb_gadget_driver *driver = container_of(dev->driver,
			struct usb_gadget_driver, driver);
	int ret = 0;

	mutex_lock(&udc_lock);
	if (driver->is_bound) {
		mutex_unlock(&udc_lock);
		return -ENXIO;		 
	}
	driver->is_bound = true;
	udc->driver = driver;
	mutex_unlock(&udc_lock);

	dev_dbg(&udc->dev, "binding gadget driver [%s]\n", driver->function);

	usb_gadget_udc_set_speed(udc, driver->max_speed);

	ret = driver->bind(udc->gadget, driver);
	if (ret)
		goto err_bind;

	mutex_lock(&udc->connect_lock);
	ret = usb_gadget_udc_start_locked(udc);
	if (ret) {
		mutex_unlock(&udc->connect_lock);
		goto err_start;
	}
	usb_gadget_enable_async_callbacks(udc);
	udc->allow_connect = true;
	usb_udc_connect_control_locked(udc);
	mutex_unlock(&udc->connect_lock);

	kobject_uevent(&udc->dev.kobj, KOBJ_CHANGE);
	return 0;

 err_start:
	driver->unbind(udc->gadget);

 err_bind:
	if (ret != -EISNAM)
		dev_err(&udc->dev, "failed to start %s: %d\n",
			driver->function, ret);

	mutex_lock(&udc_lock);
	udc->driver = NULL;
	driver->is_bound = false;
	mutex_unlock(&udc_lock);

	return ret;
}

static void gadget_unbind_driver(struct device *dev)
{
	struct usb_gadget *gadget = dev_to_usb_gadget(dev);
	struct usb_udc *udc = gadget->udc;
	struct usb_gadget_driver *driver = udc->driver;

	dev_dbg(&udc->dev, "unbinding gadget driver [%s]\n", driver->function);

	udc->allow_connect = false;
	cancel_work_sync(&udc->vbus_work);
	mutex_lock(&udc->connect_lock);
	usb_gadget_disconnect_locked(gadget);
	usb_gadget_disable_async_callbacks(udc);
	if (gadget->irq)
		synchronize_irq(gadget->irq);
	mutex_unlock(&udc->connect_lock);

	udc->driver->unbind(gadget);

	mutex_lock(&udc->connect_lock);
	usb_gadget_udc_stop_locked(udc);
	mutex_unlock(&udc->connect_lock);

	mutex_lock(&udc_lock);
	driver->is_bound = false;
	udc->driver = NULL;
	mutex_unlock(&udc_lock);

	kobject_uevent(&udc->dev.kobj, KOBJ_CHANGE);
}

 

int usb_gadget_register_driver_owner(struct usb_gadget_driver *driver,
		struct module *owner, const char *mod_name)
{
	int ret;

	if (!driver || !driver->bind || !driver->setup)
		return -EINVAL;

	driver->driver.bus = &gadget_bus_type;
	driver->driver.owner = owner;
	driver->driver.mod_name = mod_name;
	ret = driver_register(&driver->driver);
	if (ret) {
		pr_warn("%s: driver registration failed: %d\n",
				driver->function, ret);
		return ret;
	}

	mutex_lock(&udc_lock);
	if (!driver->is_bound) {
		if (driver->match_existing_only) {
			pr_warn("%s: couldn't find an available UDC or it's busy\n",
					driver->function);
			ret = -EBUSY;
		} else {
			pr_info("%s: couldn't find an available UDC\n",
					driver->function);
			ret = 0;
		}
	}
	mutex_unlock(&udc_lock);

	if (ret)
		driver_unregister(&driver->driver);
	return ret;
}
EXPORT_SYMBOL_GPL(usb_gadget_register_driver_owner);

int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
	if (!driver || !driver->unbind)
		return -EINVAL;

	driver_unregister(&driver->driver);
	return 0;
}
EXPORT_SYMBOL_GPL(usb_gadget_unregister_driver);

 

static ssize_t srp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	struct usb_udc		*udc = container_of(dev, struct usb_udc, dev);

	if (sysfs_streq(buf, "1"))
		usb_gadget_wakeup(udc->gadget);

	return n;
}
static DEVICE_ATTR_WO(srp);

static ssize_t soft_connect_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	struct usb_udc		*udc = container_of(dev, struct usb_udc, dev);
	ssize_t			ret;

	device_lock(&udc->gadget->dev);
	if (!udc->driver) {
		dev_err(dev, "soft-connect without a gadget driver\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (sysfs_streq(buf, "connect")) {
		mutex_lock(&udc->connect_lock);
		usb_gadget_udc_start_locked(udc);
		usb_gadget_connect_locked(udc->gadget);
		mutex_unlock(&udc->connect_lock);
	} else if (sysfs_streq(buf, "disconnect")) {
		mutex_lock(&udc->connect_lock);
		usb_gadget_disconnect_locked(udc->gadget);
		usb_gadget_udc_stop_locked(udc);
		mutex_unlock(&udc->connect_lock);
	} else {
		dev_err(dev, "unsupported command '%s'\n", buf);
		ret = -EINVAL;
		goto out;
	}

	ret = n;
out:
	device_unlock(&udc->gadget->dev);
	return ret;
}
static DEVICE_ATTR_WO(soft_connect);

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct usb_udc		*udc = container_of(dev, struct usb_udc, dev);
	struct usb_gadget	*gadget = udc->gadget;

	return sprintf(buf, "%s\n", usb_state_string(gadget->state));
}
static DEVICE_ATTR_RO(state);

static ssize_t function_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct usb_udc		*udc = container_of(dev, struct usb_udc, dev);
	struct usb_gadget_driver *drv;
	int			rc = 0;

	mutex_lock(&udc_lock);
	drv = udc->driver;
	if (drv && drv->function)
		rc = scnprintf(buf, PAGE_SIZE, "%s\n", drv->function);
	mutex_unlock(&udc_lock);
	return rc;
}
static DEVICE_ATTR_RO(function);

#define USB_UDC_SPEED_ATTR(name, param)					\
ssize_t name##_show(struct device *dev,					\
		struct device_attribute *attr, char *buf)		\
{									\
	struct usb_udc *udc = container_of(dev, struct usb_udc, dev);	\
	return scnprintf(buf, PAGE_SIZE, "%s\n",			\
			usb_speed_string(udc->gadget->param));		\
}									\
static DEVICE_ATTR_RO(name)

static USB_UDC_SPEED_ATTR(current_speed, speed);
static USB_UDC_SPEED_ATTR(maximum_speed, max_speed);

#define USB_UDC_ATTR(name)					\
ssize_t name##_show(struct device *dev,				\
		struct device_attribute *attr, char *buf)	\
{								\
	struct usb_udc		*udc = container_of(dev, struct usb_udc, dev); \
	struct usb_gadget	*gadget = udc->gadget;		\
								\
	return scnprintf(buf, PAGE_SIZE, "%d\n", gadget->name);	\
}								\
static DEVICE_ATTR_RO(name)

static USB_UDC_ATTR(is_otg);
static USB_UDC_ATTR(is_a_peripheral);
static USB_UDC_ATTR(b_hnp_enable);
static USB_UDC_ATTR(a_hnp_support);
static USB_UDC_ATTR(a_alt_hnp_support);
static USB_UDC_ATTR(is_selfpowered);

static struct attribute *usb_udc_attrs[] = {
	&dev_attr_srp.attr,
	&dev_attr_soft_connect.attr,
	&dev_attr_state.attr,
	&dev_attr_function.attr,
	&dev_attr_current_speed.attr,
	&dev_attr_maximum_speed.attr,

	&dev_attr_is_otg.attr,
	&dev_attr_is_a_peripheral.attr,
	&dev_attr_b_hnp_enable.attr,
	&dev_attr_a_hnp_support.attr,
	&dev_attr_a_alt_hnp_support.attr,
	&dev_attr_is_selfpowered.attr,
	NULL,
};

static const struct attribute_group usb_udc_attr_group = {
	.attrs = usb_udc_attrs,
};

static const struct attribute_group *usb_udc_attr_groups[] = {
	&usb_udc_attr_group,
	NULL,
};

static int usb_udc_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	const struct usb_udc	*udc = container_of(dev, struct usb_udc, dev);
	int			ret;

	ret = add_uevent_var(env, "USB_UDC_NAME=%s", udc->gadget->name);
	if (ret) {
		dev_err(dev, "failed to add uevent USB_UDC_NAME\n");
		return ret;
	}

	mutex_lock(&udc_lock);
	if (udc->driver)
		ret = add_uevent_var(env, "USB_UDC_DRIVER=%s",
				udc->driver->function);
	mutex_unlock(&udc_lock);
	if (ret) {
		dev_err(dev, "failed to add uevent USB_UDC_DRIVER\n");
		return ret;
	}

	return 0;
}

static const struct class udc_class = {
	.name		= "udc",
	.dev_uevent	= usb_udc_uevent,
};

static const struct bus_type gadget_bus_type = {
	.name = "gadget",
	.probe = gadget_bind_driver,
	.remove = gadget_unbind_driver,
	.match = gadget_match_driver,
};

static int __init usb_udc_init(void)
{
	int rc;

	rc = class_register(&udc_class);
	if (rc)
		return rc;

	rc = bus_register(&gadget_bus_type);
	if (rc)
		class_unregister(&udc_class);
	return rc;
}
subsys_initcall(usb_udc_init);

static void __exit usb_udc_exit(void)
{
	bus_unregister(&gadget_bus_type);
	class_unregister(&udc_class);
}
module_exit(usb_udc_exit);

MODULE_DESCRIPTION("UDC Framework");
MODULE_AUTHOR("Felipe Balbi <balbi@ti.com>");
MODULE_LICENSE("GPL v2");
