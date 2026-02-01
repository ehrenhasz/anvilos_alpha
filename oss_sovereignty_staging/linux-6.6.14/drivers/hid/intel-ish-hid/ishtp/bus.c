
 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include "bus.h"
#include "ishtp-dev.h"
#include "client.h"
#include "hbm.h"

static int ishtp_use_dma;
module_param_named(ishtp_use_dma, ishtp_use_dma, int, 0600);
MODULE_PARM_DESC(ishtp_use_dma, "Use DMA to send messages");

#define to_ishtp_cl_driver(d) container_of(d, struct ishtp_cl_driver, driver)
#define to_ishtp_cl_device(d) container_of(d, struct ishtp_cl_device, dev)
static bool ishtp_device_ready;

 
void ishtp_recv(struct ishtp_device *dev)
{
	uint32_t	msg_hdr;
	struct ishtp_msg_hdr	*ishtp_hdr;

	 
	msg_hdr = dev->ops->ishtp_read_hdr(dev);
	if (!msg_hdr)
		return;

	dev->ops->sync_fw_clock(dev);

	ishtp_hdr = (struct ishtp_msg_hdr *)&msg_hdr;
	dev->ishtp_msg_hdr = msg_hdr;

	 
	if (ishtp_hdr->length > dev->mtu) {
		dev_err(dev->devc,
			"ISHTP hdr - bad length: %u; dropped [%08X]\n",
			(unsigned int)ishtp_hdr->length, msg_hdr);
		return;
	}

	 
	if (!ishtp_hdr->host_addr && !ishtp_hdr->fw_addr)
		recv_hbm(dev, ishtp_hdr);
	 
	else if (!ishtp_hdr->host_addr)
		recv_fixed_cl_msg(dev, ishtp_hdr);
	else
		 
		recv_ishtp_cl_msg(dev, ishtp_hdr);
}
EXPORT_SYMBOL(ishtp_recv);

 
int ishtp_send_msg(struct ishtp_device *dev, struct ishtp_msg_hdr *hdr,
		       void *msg, void(*ipc_send_compl)(void *),
		       void *ipc_send_compl_prm)
{
	unsigned char	ipc_msg[IPC_FULL_MSG_SIZE];
	uint32_t	drbl_val;

	drbl_val = dev->ops->ipc_get_header(dev, hdr->length +
					    sizeof(struct ishtp_msg_hdr),
					    1);

	memcpy(ipc_msg, &drbl_val, sizeof(uint32_t));
	memcpy(ipc_msg + sizeof(uint32_t), hdr, sizeof(uint32_t));
	memcpy(ipc_msg + 2 * sizeof(uint32_t), msg, hdr->length);
	return	dev->ops->write(dev, ipc_send_compl, ipc_send_compl_prm,
				ipc_msg, 2 * sizeof(uint32_t) + hdr->length);
}

 
int ishtp_write_message(struct ishtp_device *dev, struct ishtp_msg_hdr *hdr,
			void *buf)
{
	return ishtp_send_msg(dev, hdr, buf, NULL, NULL);
}

 
int ishtp_fw_cl_by_uuid(struct ishtp_device *dev, const guid_t *uuid)
{
	unsigned int i;

	for (i = 0; i < dev->fw_clients_num; ++i) {
		if (guid_equal(uuid, &dev->fw_clients[i].props.protocol_name))
			return i;
	}
	return -ENOENT;
}
EXPORT_SYMBOL(ishtp_fw_cl_by_uuid);

 
struct ishtp_fw_client *ishtp_fw_cl_get_client(struct ishtp_device *dev,
					       const guid_t *uuid)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&dev->fw_clients_lock, flags);
	i = ishtp_fw_cl_by_uuid(dev, uuid);
	spin_unlock_irqrestore(&dev->fw_clients_lock, flags);
	if (i < 0 || dev->fw_clients[i].props.fixed_address)
		return NULL;

	return &dev->fw_clients[i];
}
EXPORT_SYMBOL(ishtp_fw_cl_get_client);

 
int ishtp_get_fw_client_id(struct ishtp_fw_client *fw_client)
{
	return fw_client->client_id;
}
EXPORT_SYMBOL(ishtp_get_fw_client_id);

 
int ishtp_fw_cl_by_id(struct ishtp_device *dev, uint8_t client_id)
{
	int i, res = -ENOENT;
	unsigned long	flags;

	spin_lock_irqsave(&dev->fw_clients_lock, flags);
	for (i = 0; i < dev->fw_clients_num; i++) {
		if (dev->fw_clients[i].client_id == client_id) {
			res = i;
			break;
		}
	}
	spin_unlock_irqrestore(&dev->fw_clients_lock, flags);

	return res;
}

 
static int ishtp_cl_device_probe(struct device *dev)
{
	struct ishtp_cl_device *device = to_ishtp_cl_device(dev);
	struct ishtp_cl_driver *driver;

	if (!device)
		return 0;

	driver = to_ishtp_cl_driver(dev->driver);
	if (!driver || !driver->probe)
		return -ENODEV;

	return driver->probe(device);
}

 
static int ishtp_cl_bus_match(struct device *dev, struct device_driver *drv)
{
	struct ishtp_cl_device *device = to_ishtp_cl_device(dev);
	struct ishtp_cl_driver *driver = to_ishtp_cl_driver(drv);

	return(device->fw_client ? guid_equal(&driver->id[0].guid,
	       &device->fw_client->props.protocol_name) : 0);
}

 
static void ishtp_cl_device_remove(struct device *dev)
{
	struct ishtp_cl_device *device = to_ishtp_cl_device(dev);
	struct ishtp_cl_driver *driver = to_ishtp_cl_driver(dev->driver);

	if (device->event_cb) {
		device->event_cb = NULL;
		cancel_work_sync(&device->event_work);
	}

	if (driver->remove)
		driver->remove(device);
}

 
static int ishtp_cl_device_suspend(struct device *dev)
{
	struct ishtp_cl_device *device = to_ishtp_cl_device(dev);
	struct ishtp_cl_driver *driver;
	int ret = 0;

	if (!device)
		return 0;

	driver = to_ishtp_cl_driver(dev->driver);
	if (driver && driver->driver.pm) {
		if (driver->driver.pm->suspend)
			ret = driver->driver.pm->suspend(dev);
	}

	return ret;
}

 
static int ishtp_cl_device_resume(struct device *dev)
{
	struct ishtp_cl_device *device = to_ishtp_cl_device(dev);
	struct ishtp_cl_driver *driver;
	int ret = 0;

	if (!device)
		return 0;

	driver = to_ishtp_cl_driver(dev->driver);
	if (driver && driver->driver.pm) {
		if (driver->driver.pm->resume)
			ret = driver->driver.pm->resume(dev);
	}

	return ret;
}

 
static int ishtp_cl_device_reset(struct ishtp_cl_device *device)
{
	struct ishtp_cl_driver *driver;
	int ret = 0;

	device->event_cb = NULL;
	cancel_work_sync(&device->event_work);

	driver = to_ishtp_cl_driver(device->dev.driver);
	if (driver && driver->reset)
		ret = driver->reset(device);

	return ret;
}

static ssize_t modalias_show(struct device *dev, struct device_attribute *a,
	char *buf)
{
	int len;

	len = snprintf(buf, PAGE_SIZE, ISHTP_MODULE_PREFIX "%s\n", dev_name(dev));
	return (len >= PAGE_SIZE) ? (PAGE_SIZE - 1) : len;
}
static DEVICE_ATTR_RO(modalias);

static struct attribute *ishtp_cl_dev_attrs[] = {
	&dev_attr_modalias.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ishtp_cl_dev);

static int ishtp_cl_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	if (add_uevent_var(env, "MODALIAS=" ISHTP_MODULE_PREFIX "%s", dev_name(dev)))
		return -ENOMEM;
	return 0;
}

static const struct dev_pm_ops ishtp_cl_bus_dev_pm_ops = {
	 
	.suspend = ishtp_cl_device_suspend,
	.resume = ishtp_cl_device_resume,
	 
	.freeze = ishtp_cl_device_suspend,
	.thaw = ishtp_cl_device_resume,
	.restore = ishtp_cl_device_resume,
};

static struct bus_type ishtp_cl_bus_type = {
	.name		= "ishtp",
	.dev_groups	= ishtp_cl_dev_groups,
	.probe		= ishtp_cl_device_probe,
	.match		= ishtp_cl_bus_match,
	.remove		= ishtp_cl_device_remove,
	.pm		= &ishtp_cl_bus_dev_pm_ops,
	.uevent		= ishtp_cl_uevent,
};

static void ishtp_cl_dev_release(struct device *dev)
{
	kfree(to_ishtp_cl_device(dev));
}

static const struct device_type ishtp_cl_device_type = {
	.release	= ishtp_cl_dev_release,
};

 
static struct ishtp_cl_device *ishtp_bus_add_device(struct ishtp_device *dev,
						    guid_t uuid, char *name)
{
	struct ishtp_cl_device *device;
	int status;
	unsigned long flags;

	spin_lock_irqsave(&dev->device_list_lock, flags);
	list_for_each_entry(device, &dev->device_list, device_link) {
		if (!strcmp(name, dev_name(&device->dev))) {
			device->fw_client = &dev->fw_clients[
				dev->fw_client_presentation_num - 1];
			spin_unlock_irqrestore(&dev->device_list_lock, flags);
			ishtp_cl_device_reset(device);
			return device;
		}
	}
	spin_unlock_irqrestore(&dev->device_list_lock, flags);

	device = kzalloc(sizeof(struct ishtp_cl_device), GFP_KERNEL);
	if (!device)
		return NULL;

	device->dev.parent = dev->devc;
	device->dev.bus = &ishtp_cl_bus_type;
	device->dev.type = &ishtp_cl_device_type;
	device->ishtp_dev = dev;

	device->fw_client =
		&dev->fw_clients[dev->fw_client_presentation_num - 1];

	dev_set_name(&device->dev, "%s", name);

	spin_lock_irqsave(&dev->device_list_lock, flags);
	list_add_tail(&device->device_link, &dev->device_list);
	spin_unlock_irqrestore(&dev->device_list_lock, flags);

	status = device_register(&device->dev);
	if (status) {
		spin_lock_irqsave(&dev->device_list_lock, flags);
		list_del(&device->device_link);
		spin_unlock_irqrestore(&dev->device_list_lock, flags);
		dev_err(dev->devc, "Failed to register ISHTP client device\n");
		put_device(&device->dev);
		return NULL;
	}

	ishtp_device_ready = true;

	return device;
}

 
static void ishtp_bus_remove_device(struct ishtp_cl_device *device)
{
	device_unregister(&device->dev);
}

 
int ishtp_cl_driver_register(struct ishtp_cl_driver *driver,
			     struct module *owner)
{
	if (!ishtp_device_ready)
		return -ENODEV;

	driver->driver.name = driver->name;
	driver->driver.owner = owner;
	driver->driver.bus = &ishtp_cl_bus_type;

	return driver_register(&driver->driver);
}
EXPORT_SYMBOL(ishtp_cl_driver_register);

 
void ishtp_cl_driver_unregister(struct ishtp_cl_driver *driver)
{
	driver_unregister(&driver->driver);
}
EXPORT_SYMBOL(ishtp_cl_driver_unregister);

 
static void ishtp_bus_event_work(struct work_struct *work)
{
	struct ishtp_cl_device *device;

	device = container_of(work, struct ishtp_cl_device, event_work);

	if (device->event_cb)
		device->event_cb(device);
}

 
void ishtp_cl_bus_rx_event(struct ishtp_cl_device *device)
{
	if (!device || !device->event_cb)
		return;

	if (device->event_cb)
		schedule_work(&device->event_work);
}

 
int ishtp_register_event_cb(struct ishtp_cl_device *device,
	void (*event_cb)(struct ishtp_cl_device *))
{
	if (device->event_cb)
		return -EALREADY;

	device->event_cb = event_cb;
	INIT_WORK(&device->event_work, ishtp_bus_event_work);

	return 0;
}
EXPORT_SYMBOL(ishtp_register_event_cb);

 
void ishtp_get_device(struct ishtp_cl_device *cl_device)
{
	cl_device->reference_count++;
}
EXPORT_SYMBOL(ishtp_get_device);

 
void ishtp_put_device(struct ishtp_cl_device *cl_device)
{
	cl_device->reference_count--;
}
EXPORT_SYMBOL(ishtp_put_device);

 
void ishtp_set_drvdata(struct ishtp_cl_device *cl_device, void *data)
{
	cl_device->driver_data = data;
}
EXPORT_SYMBOL(ishtp_set_drvdata);

 
void *ishtp_get_drvdata(struct ishtp_cl_device *cl_device)
{
	return cl_device->driver_data;
}
EXPORT_SYMBOL(ishtp_get_drvdata);

 
struct ishtp_cl_device *ishtp_dev_to_cl_device(struct device *device)
{
	return to_ishtp_cl_device(device);
}
EXPORT_SYMBOL(ishtp_dev_to_cl_device);

 
int ishtp_bus_new_client(struct ishtp_device *dev)
{
	int	i;
	char	*dev_name;
	struct ishtp_cl_device	*cl_device;
	guid_t	device_uuid;

	 
	i = dev->fw_client_presentation_num - 1;
	device_uuid = dev->fw_clients[i].props.protocol_name;
	dev_name = kasprintf(GFP_KERNEL, "{%pUL}", &device_uuid);
	if (!dev_name)
		return	-ENOMEM;

	cl_device = ishtp_bus_add_device(dev, device_uuid, dev_name);
	if (!cl_device) {
		kfree(dev_name);
		return	-ENOENT;
	}

	kfree(dev_name);

	return	0;
}

 
int ishtp_cl_device_bind(struct ishtp_cl *cl)
{
	struct ishtp_cl_device	*cl_device;
	unsigned long flags;
	int	rv;

	if (!cl->fw_client_id || cl->state != ISHTP_CL_CONNECTED)
		return	-EFAULT;

	rv = -ENOENT;
	spin_lock_irqsave(&cl->dev->device_list_lock, flags);
	list_for_each_entry(cl_device, &cl->dev->device_list,
			device_link) {
		if (cl_device->fw_client &&
		    cl_device->fw_client->client_id == cl->fw_client_id) {
			cl->device = cl_device;
			rv = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&cl->dev->device_list_lock, flags);
	return	rv;
}

 
void ishtp_bus_remove_all_clients(struct ishtp_device *ishtp_dev,
				  bool warm_reset)
{
	struct ishtp_cl_device	*cl_device, *n;
	struct ishtp_cl	*cl;
	unsigned long	flags;

	spin_lock_irqsave(&ishtp_dev->cl_list_lock, flags);
	list_for_each_entry(cl, &ishtp_dev->cl_list, link) {
		cl->state = ISHTP_CL_DISCONNECTED;

		 
		wake_up_interruptible(&cl->wait_ctrl_res);

		 
		ishtp_cl_flush_queues(cl);

		 
		ishtp_cl_free_rx_ring(cl);
		ishtp_cl_free_tx_ring(cl);

		 
	}
	spin_unlock_irqrestore(&ishtp_dev->cl_list_lock, flags);

	 
	ishtp_cl_free_dma_buf(ishtp_dev);

	 
	spin_lock_irqsave(&ishtp_dev->device_list_lock, flags);
	list_for_each_entry_safe(cl_device, n, &ishtp_dev->device_list,
				 device_link) {
		cl_device->fw_client = NULL;
		if (warm_reset && cl_device->reference_count)
			continue;

		list_del(&cl_device->device_link);
		spin_unlock_irqrestore(&ishtp_dev->device_list_lock, flags);
		ishtp_bus_remove_device(cl_device);
		spin_lock_irqsave(&ishtp_dev->device_list_lock, flags);
	}
	spin_unlock_irqrestore(&ishtp_dev->device_list_lock, flags);

	 
	spin_lock_irqsave(&ishtp_dev->fw_clients_lock, flags);
	kfree(ishtp_dev->fw_clients);
	ishtp_dev->fw_clients = NULL;
	ishtp_dev->fw_clients_num = 0;
	ishtp_dev->fw_client_presentation_num = 0;
	ishtp_dev->fw_client_index = 0;
	bitmap_zero(ishtp_dev->fw_clients_map, ISHTP_CLIENTS_MAX);
	spin_unlock_irqrestore(&ishtp_dev->fw_clients_lock, flags);
}
EXPORT_SYMBOL(ishtp_bus_remove_all_clients);

 
void ishtp_reset_handler(struct ishtp_device *dev)
{
	unsigned long	flags;

	 
	dev->dev_state = ISHTP_DEV_RESETTING;

	 
	spin_lock_irqsave(&dev->rd_msg_spinlock, flags);
	dev->rd_msg_fifo_head = dev->rd_msg_fifo_tail = 0;
	spin_unlock_irqrestore(&dev->rd_msg_spinlock, flags);

	 
	ishtp_bus_remove_all_clients(dev, true);
}
EXPORT_SYMBOL(ishtp_reset_handler);

 
void ishtp_reset_compl_handler(struct ishtp_device *dev)
{
	dev->dev_state = ISHTP_DEV_INIT_CLIENTS;
	dev->hbm_state = ISHTP_HBM_START;
	ishtp_hbm_start_req(dev);
}
EXPORT_SYMBOL(ishtp_reset_compl_handler);

 
int ishtp_use_dma_transfer(void)
{
	return ishtp_use_dma;
}

 
struct device *ishtp_device(struct ishtp_cl_device *device)
{
	return &device->dev;
}
EXPORT_SYMBOL(ishtp_device);

 
bool ishtp_wait_resume(struct ishtp_device *dev)
{
	 
	#define WAIT_FOR_RESUME_ACK_MS		50

	 
	if (dev->resume_flag)
		wait_event_interruptible_timeout(dev->resume_wait,
						 !dev->resume_flag,
						 msecs_to_jiffies(WAIT_FOR_RESUME_ACK_MS));

	return (!dev->resume_flag);
}
EXPORT_SYMBOL_GPL(ishtp_wait_resume);

 
struct device *ishtp_get_pci_device(struct ishtp_cl_device *device)
{
	return device->ishtp_dev->devc;
}
EXPORT_SYMBOL(ishtp_get_pci_device);

 
ishtp_print_log ishtp_trace_callback(struct ishtp_cl_device *cl_device)
{
	return cl_device->ishtp_dev->print_log;
}
EXPORT_SYMBOL(ishtp_trace_callback);

 
int ish_hw_reset(struct ishtp_device *dev)
{
	return dev->ops->hw_reset(dev);
}
EXPORT_SYMBOL(ish_hw_reset);

 
static int  __init ishtp_bus_register(void)
{
	return bus_register(&ishtp_cl_bus_type);
}

 
static void __exit ishtp_bus_unregister(void)
{
	bus_unregister(&ishtp_cl_bus_type);
}

module_init(ishtp_bus_register);
module_exit(ishtp_bus_unregister);

MODULE_LICENSE("GPL");
