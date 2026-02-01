
 
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/sysctl.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/completion.h>
#include <linux/hyperv.h>
#include <linux/kernel_stat.h>
#include <linux/of_address.h>
#include <linux/clockchips.h>
#include <linux/cpu.h>
#include <linux/sched/isolation.h>
#include <linux/sched/task_stack.h>

#include <linux/delay.h>
#include <linux/panic_notifier.h>
#include <linux/ptrace.h>
#include <linux/screen_info.h>
#include <linux/efi.h>
#include <linux/random.h>
#include <linux/kernel.h>
#include <linux/syscore_ops.h>
#include <linux/dma-map-ops.h>
#include <linux/pci.h>
#include <clocksource/hyperv_timer.h>
#include <asm/mshyperv.h>
#include "hyperv_vmbus.h"

struct vmbus_dynid {
	struct list_head node;
	struct hv_vmbus_device_id id;
};

static struct device  *hv_dev;

static int hyperv_cpuhp_online;

static long __percpu *vmbus_evt;

 
int vmbus_irq;
int vmbus_interrupt;

 
static int hv_panic_vmbus_unload(struct notifier_block *nb, unsigned long val,
			      void *args)
{
	vmbus_initiate_unload(true);
	return NOTIFY_DONE;
}
static struct notifier_block hyperv_panic_vmbus_unload_block = {
	.notifier_call	= hv_panic_vmbus_unload,
	.priority	= INT_MIN + 1,  
};

static const char *fb_mmio_name = "fb_range";
static struct resource *fb_mmio;
static struct resource *hyperv_mmio;
static DEFINE_MUTEX(hyperv_mmio_lock);

static int vmbus_exists(void)
{
	if (hv_dev == NULL)
		return -ENODEV;

	return 0;
}

static u8 channel_monitor_group(const struct vmbus_channel *channel)
{
	return (u8)channel->offermsg.monitorid / 32;
}

static u8 channel_monitor_offset(const struct vmbus_channel *channel)
{
	return (u8)channel->offermsg.monitorid % 32;
}

static u32 channel_pending(const struct vmbus_channel *channel,
			   const struct hv_monitor_page *monitor_page)
{
	u8 monitor_group = channel_monitor_group(channel);

	return monitor_page->trigger_group[monitor_group].pending;
}

static u32 channel_latency(const struct vmbus_channel *channel,
			   const struct hv_monitor_page *monitor_page)
{
	u8 monitor_group = channel_monitor_group(channel);
	u8 monitor_offset = channel_monitor_offset(channel);

	return monitor_page->latency[monitor_group][monitor_offset];
}

static u32 channel_conn_id(struct vmbus_channel *channel,
			   struct hv_monitor_page *monitor_page)
{
	u8 monitor_group = channel_monitor_group(channel);
	u8 monitor_offset = channel_monitor_offset(channel);

	return monitor_page->parameter[monitor_group][monitor_offset].connectionid.u.id;
}

static ssize_t id_show(struct device *dev, struct device_attribute *dev_attr,
		       char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	if (!hv_dev->channel)
		return -ENODEV;
	return sprintf(buf, "%d\n", hv_dev->channel->offermsg.child_relid);
}
static DEVICE_ATTR_RO(id);

static ssize_t state_show(struct device *dev, struct device_attribute *dev_attr,
			  char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	if (!hv_dev->channel)
		return -ENODEV;
	return sprintf(buf, "%d\n", hv_dev->channel->state);
}
static DEVICE_ATTR_RO(state);

static ssize_t monitor_id_show(struct device *dev,
			       struct device_attribute *dev_attr, char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	if (!hv_dev->channel)
		return -ENODEV;
	return sprintf(buf, "%d\n", hv_dev->channel->offermsg.monitorid);
}
static DEVICE_ATTR_RO(monitor_id);

static ssize_t class_id_show(struct device *dev,
			       struct device_attribute *dev_attr, char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	if (!hv_dev->channel)
		return -ENODEV;
	return sprintf(buf, "{%pUl}\n",
		       &hv_dev->channel->offermsg.offer.if_type);
}
static DEVICE_ATTR_RO(class_id);

static ssize_t device_id_show(struct device *dev,
			      struct device_attribute *dev_attr, char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	if (!hv_dev->channel)
		return -ENODEV;
	return sprintf(buf, "{%pUl}\n",
		       &hv_dev->channel->offermsg.offer.if_instance);
}
static DEVICE_ATTR_RO(device_id);

static ssize_t modalias_show(struct device *dev,
			     struct device_attribute *dev_attr, char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	return sprintf(buf, "vmbus:%*phN\n", UUID_SIZE, &hv_dev->dev_type);
}
static DEVICE_ATTR_RO(modalias);

#ifdef CONFIG_NUMA
static ssize_t numa_node_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	if (!hv_dev->channel)
		return -ENODEV;

	return sprintf(buf, "%d\n", cpu_to_node(hv_dev->channel->target_cpu));
}
static DEVICE_ATTR_RO(numa_node);
#endif

static ssize_t server_monitor_pending_show(struct device *dev,
					   struct device_attribute *dev_attr,
					   char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	if (!hv_dev->channel)
		return -ENODEV;
	return sprintf(buf, "%d\n",
		       channel_pending(hv_dev->channel,
				       vmbus_connection.monitor_pages[0]));
}
static DEVICE_ATTR_RO(server_monitor_pending);

static ssize_t client_monitor_pending_show(struct device *dev,
					   struct device_attribute *dev_attr,
					   char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	if (!hv_dev->channel)
		return -ENODEV;
	return sprintf(buf, "%d\n",
		       channel_pending(hv_dev->channel,
				       vmbus_connection.monitor_pages[1]));
}
static DEVICE_ATTR_RO(client_monitor_pending);

static ssize_t server_monitor_latency_show(struct device *dev,
					   struct device_attribute *dev_attr,
					   char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	if (!hv_dev->channel)
		return -ENODEV;
	return sprintf(buf, "%d\n",
		       channel_latency(hv_dev->channel,
				       vmbus_connection.monitor_pages[0]));
}
static DEVICE_ATTR_RO(server_monitor_latency);

static ssize_t client_monitor_latency_show(struct device *dev,
					   struct device_attribute *dev_attr,
					   char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	if (!hv_dev->channel)
		return -ENODEV;
	return sprintf(buf, "%d\n",
		       channel_latency(hv_dev->channel,
				       vmbus_connection.monitor_pages[1]));
}
static DEVICE_ATTR_RO(client_monitor_latency);

static ssize_t server_monitor_conn_id_show(struct device *dev,
					   struct device_attribute *dev_attr,
					   char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	if (!hv_dev->channel)
		return -ENODEV;
	return sprintf(buf, "%d\n",
		       channel_conn_id(hv_dev->channel,
				       vmbus_connection.monitor_pages[0]));
}
static DEVICE_ATTR_RO(server_monitor_conn_id);

static ssize_t client_monitor_conn_id_show(struct device *dev,
					   struct device_attribute *dev_attr,
					   char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	if (!hv_dev->channel)
		return -ENODEV;
	return sprintf(buf, "%d\n",
		       channel_conn_id(hv_dev->channel,
				       vmbus_connection.monitor_pages[1]));
}
static DEVICE_ATTR_RO(client_monitor_conn_id);

static ssize_t out_intr_mask_show(struct device *dev,
				  struct device_attribute *dev_attr, char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);
	struct hv_ring_buffer_debug_info outbound;
	int ret;

	if (!hv_dev->channel)
		return -ENODEV;

	ret = hv_ringbuffer_get_debuginfo(&hv_dev->channel->outbound,
					  &outbound);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", outbound.current_interrupt_mask);
}
static DEVICE_ATTR_RO(out_intr_mask);

static ssize_t out_read_index_show(struct device *dev,
				   struct device_attribute *dev_attr, char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);
	struct hv_ring_buffer_debug_info outbound;
	int ret;

	if (!hv_dev->channel)
		return -ENODEV;

	ret = hv_ringbuffer_get_debuginfo(&hv_dev->channel->outbound,
					  &outbound);
	if (ret < 0)
		return ret;
	return sprintf(buf, "%d\n", outbound.current_read_index);
}
static DEVICE_ATTR_RO(out_read_index);

static ssize_t out_write_index_show(struct device *dev,
				    struct device_attribute *dev_attr,
				    char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);
	struct hv_ring_buffer_debug_info outbound;
	int ret;

	if (!hv_dev->channel)
		return -ENODEV;

	ret = hv_ringbuffer_get_debuginfo(&hv_dev->channel->outbound,
					  &outbound);
	if (ret < 0)
		return ret;
	return sprintf(buf, "%d\n", outbound.current_write_index);
}
static DEVICE_ATTR_RO(out_write_index);

static ssize_t out_read_bytes_avail_show(struct device *dev,
					 struct device_attribute *dev_attr,
					 char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);
	struct hv_ring_buffer_debug_info outbound;
	int ret;

	if (!hv_dev->channel)
		return -ENODEV;

	ret = hv_ringbuffer_get_debuginfo(&hv_dev->channel->outbound,
					  &outbound);
	if (ret < 0)
		return ret;
	return sprintf(buf, "%d\n", outbound.bytes_avail_toread);
}
static DEVICE_ATTR_RO(out_read_bytes_avail);

static ssize_t out_write_bytes_avail_show(struct device *dev,
					  struct device_attribute *dev_attr,
					  char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);
	struct hv_ring_buffer_debug_info outbound;
	int ret;

	if (!hv_dev->channel)
		return -ENODEV;

	ret = hv_ringbuffer_get_debuginfo(&hv_dev->channel->outbound,
					  &outbound);
	if (ret < 0)
		return ret;
	return sprintf(buf, "%d\n", outbound.bytes_avail_towrite);
}
static DEVICE_ATTR_RO(out_write_bytes_avail);

static ssize_t in_intr_mask_show(struct device *dev,
				 struct device_attribute *dev_attr, char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);
	struct hv_ring_buffer_debug_info inbound;
	int ret;

	if (!hv_dev->channel)
		return -ENODEV;

	ret = hv_ringbuffer_get_debuginfo(&hv_dev->channel->inbound, &inbound);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", inbound.current_interrupt_mask);
}
static DEVICE_ATTR_RO(in_intr_mask);

static ssize_t in_read_index_show(struct device *dev,
				  struct device_attribute *dev_attr, char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);
	struct hv_ring_buffer_debug_info inbound;
	int ret;

	if (!hv_dev->channel)
		return -ENODEV;

	ret = hv_ringbuffer_get_debuginfo(&hv_dev->channel->inbound, &inbound);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", inbound.current_read_index);
}
static DEVICE_ATTR_RO(in_read_index);

static ssize_t in_write_index_show(struct device *dev,
				   struct device_attribute *dev_attr, char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);
	struct hv_ring_buffer_debug_info inbound;
	int ret;

	if (!hv_dev->channel)
		return -ENODEV;

	ret = hv_ringbuffer_get_debuginfo(&hv_dev->channel->inbound, &inbound);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", inbound.current_write_index);
}
static DEVICE_ATTR_RO(in_write_index);

static ssize_t in_read_bytes_avail_show(struct device *dev,
					struct device_attribute *dev_attr,
					char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);
	struct hv_ring_buffer_debug_info inbound;
	int ret;

	if (!hv_dev->channel)
		return -ENODEV;

	ret = hv_ringbuffer_get_debuginfo(&hv_dev->channel->inbound, &inbound);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", inbound.bytes_avail_toread);
}
static DEVICE_ATTR_RO(in_read_bytes_avail);

static ssize_t in_write_bytes_avail_show(struct device *dev,
					 struct device_attribute *dev_attr,
					 char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);
	struct hv_ring_buffer_debug_info inbound;
	int ret;

	if (!hv_dev->channel)
		return -ENODEV;

	ret = hv_ringbuffer_get_debuginfo(&hv_dev->channel->inbound, &inbound);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", inbound.bytes_avail_towrite);
}
static DEVICE_ATTR_RO(in_write_bytes_avail);

static ssize_t channel_vp_mapping_show(struct device *dev,
				       struct device_attribute *dev_attr,
				       char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);
	struct vmbus_channel *channel = hv_dev->channel, *cur_sc;
	int buf_size = PAGE_SIZE, n_written, tot_written;
	struct list_head *cur;

	if (!channel)
		return -ENODEV;

	mutex_lock(&vmbus_connection.channel_mutex);

	tot_written = snprintf(buf, buf_size, "%u:%u\n",
		channel->offermsg.child_relid, channel->target_cpu);

	list_for_each(cur, &channel->sc_list) {
		if (tot_written >= buf_size - 1)
			break;

		cur_sc = list_entry(cur, struct vmbus_channel, sc_list);
		n_written = scnprintf(buf + tot_written,
				     buf_size - tot_written,
				     "%u:%u\n",
				     cur_sc->offermsg.child_relid,
				     cur_sc->target_cpu);
		tot_written += n_written;
	}

	mutex_unlock(&vmbus_connection.channel_mutex);

	return tot_written;
}
static DEVICE_ATTR_RO(channel_vp_mapping);

static ssize_t vendor_show(struct device *dev,
			   struct device_attribute *dev_attr,
			   char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	return sprintf(buf, "0x%x\n", hv_dev->vendor_id);
}
static DEVICE_ATTR_RO(vendor);

static ssize_t device_show(struct device *dev,
			   struct device_attribute *dev_attr,
			   char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);

	return sprintf(buf, "0x%x\n", hv_dev->device_id);
}
static DEVICE_ATTR_RO(device);

static ssize_t driver_override_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);
	int ret;

	ret = driver_set_override(dev, &hv_dev->driver_override, buf, count);
	if (ret)
		return ret;

	return count;
}

static ssize_t driver_override_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct hv_device *hv_dev = device_to_hv_device(dev);
	ssize_t len;

	device_lock(dev);
	len = snprintf(buf, PAGE_SIZE, "%s\n", hv_dev->driver_override);
	device_unlock(dev);

	return len;
}
static DEVICE_ATTR_RW(driver_override);

 
static struct attribute *vmbus_dev_attrs[] = {
	&dev_attr_id.attr,
	&dev_attr_state.attr,
	&dev_attr_monitor_id.attr,
	&dev_attr_class_id.attr,
	&dev_attr_device_id.attr,
	&dev_attr_modalias.attr,
#ifdef CONFIG_NUMA
	&dev_attr_numa_node.attr,
#endif
	&dev_attr_server_monitor_pending.attr,
	&dev_attr_client_monitor_pending.attr,
	&dev_attr_server_monitor_latency.attr,
	&dev_attr_client_monitor_latency.attr,
	&dev_attr_server_monitor_conn_id.attr,
	&dev_attr_client_monitor_conn_id.attr,
	&dev_attr_out_intr_mask.attr,
	&dev_attr_out_read_index.attr,
	&dev_attr_out_write_index.attr,
	&dev_attr_out_read_bytes_avail.attr,
	&dev_attr_out_write_bytes_avail.attr,
	&dev_attr_in_intr_mask.attr,
	&dev_attr_in_read_index.attr,
	&dev_attr_in_write_index.attr,
	&dev_attr_in_read_bytes_avail.attr,
	&dev_attr_in_write_bytes_avail.attr,
	&dev_attr_channel_vp_mapping.attr,
	&dev_attr_vendor.attr,
	&dev_attr_device.attr,
	&dev_attr_driver_override.attr,
	NULL,
};

 
static umode_t vmbus_dev_attr_is_visible(struct kobject *kobj,
					 struct attribute *attr, int idx)
{
	struct device *dev = kobj_to_dev(kobj);
	const struct hv_device *hv_dev = device_to_hv_device(dev);

	 
	if (!hv_dev->channel->offermsg.monitor_allocated &&
	    (attr == &dev_attr_monitor_id.attr ||
	     attr == &dev_attr_server_monitor_pending.attr ||
	     attr == &dev_attr_client_monitor_pending.attr ||
	     attr == &dev_attr_server_monitor_latency.attr ||
	     attr == &dev_attr_client_monitor_latency.attr ||
	     attr == &dev_attr_server_monitor_conn_id.attr ||
	     attr == &dev_attr_client_monitor_conn_id.attr))
		return 0;

	return attr->mode;
}

static const struct attribute_group vmbus_dev_group = {
	.attrs = vmbus_dev_attrs,
	.is_visible = vmbus_dev_attr_is_visible
};
__ATTRIBUTE_GROUPS(vmbus_dev);

 
static ssize_t hibernation_show(const struct bus_type *bus, char *buf)
{
	return sprintf(buf, "%d\n", !!hv_is_hibernation_supported());
}

static BUS_ATTR_RO(hibernation);

static struct attribute *vmbus_bus_attrs[] = {
	&bus_attr_hibernation.attr,
	NULL,
};
static const struct attribute_group vmbus_bus_group = {
	.attrs = vmbus_bus_attrs,
};
__ATTRIBUTE_GROUPS(vmbus_bus);

 
static int vmbus_uevent(const struct device *device, struct kobj_uevent_env *env)
{
	const struct hv_device *dev = device_to_hv_device(device);
	const char *format = "MODALIAS=vmbus:%*phN";

	return add_uevent_var(env, format, UUID_SIZE, &dev->dev_type);
}

static const struct hv_vmbus_device_id *
hv_vmbus_dev_match(const struct hv_vmbus_device_id *id, const guid_t *guid)
{
	if (id == NULL)
		return NULL;  

	for (; !guid_is_null(&id->guid); id++)
		if (guid_equal(&id->guid, guid))
			return id;

	return NULL;
}

static const struct hv_vmbus_device_id *
hv_vmbus_dynid_match(struct hv_driver *drv, const guid_t *guid)
{
	const struct hv_vmbus_device_id *id = NULL;
	struct vmbus_dynid *dynid;

	spin_lock(&drv->dynids.lock);
	list_for_each_entry(dynid, &drv->dynids.list, node) {
		if (guid_equal(&dynid->id.guid, guid)) {
			id = &dynid->id;
			break;
		}
	}
	spin_unlock(&drv->dynids.lock);

	return id;
}

static const struct hv_vmbus_device_id vmbus_device_null;

 
static const struct hv_vmbus_device_id *hv_vmbus_get_id(struct hv_driver *drv,
							struct hv_device *dev)
{
	const guid_t *guid = &dev->dev_type;
	const struct hv_vmbus_device_id *id;

	 
	if (dev->driver_override && strcmp(dev->driver_override, drv->name))
		return NULL;

	 
	id = hv_vmbus_dynid_match(drv, guid);
	if (!id)
		id = hv_vmbus_dev_match(drv->id_table, guid);

	 
	if (!id && dev->driver_override)
		id = &vmbus_device_null;

	return id;
}

 
static int vmbus_add_dynid(struct hv_driver *drv, guid_t *guid)
{
	struct vmbus_dynid *dynid;

	dynid = kzalloc(sizeof(*dynid), GFP_KERNEL);
	if (!dynid)
		return -ENOMEM;

	dynid->id.guid = *guid;

	spin_lock(&drv->dynids.lock);
	list_add_tail(&dynid->node, &drv->dynids.list);
	spin_unlock(&drv->dynids.lock);

	return driver_attach(&drv->driver);
}

static void vmbus_free_dynids(struct hv_driver *drv)
{
	struct vmbus_dynid *dynid, *n;

	spin_lock(&drv->dynids.lock);
	list_for_each_entry_safe(dynid, n, &drv->dynids.list, node) {
		list_del(&dynid->node);
		kfree(dynid);
	}
	spin_unlock(&drv->dynids.lock);
}

 
static ssize_t new_id_store(struct device_driver *driver, const char *buf,
			    size_t count)
{
	struct hv_driver *drv = drv_to_hv_drv(driver);
	guid_t guid;
	ssize_t retval;

	retval = guid_parse(buf, &guid);
	if (retval)
		return retval;

	if (hv_vmbus_dynid_match(drv, &guid))
		return -EEXIST;

	retval = vmbus_add_dynid(drv, &guid);
	if (retval)
		return retval;
	return count;
}
static DRIVER_ATTR_WO(new_id);

 
static ssize_t remove_id_store(struct device_driver *driver, const char *buf,
			       size_t count)
{
	struct hv_driver *drv = drv_to_hv_drv(driver);
	struct vmbus_dynid *dynid, *n;
	guid_t guid;
	ssize_t retval;

	retval = guid_parse(buf, &guid);
	if (retval)
		return retval;

	retval = -ENODEV;
	spin_lock(&drv->dynids.lock);
	list_for_each_entry_safe(dynid, n, &drv->dynids.list, node) {
		struct hv_vmbus_device_id *id = &dynid->id;

		if (guid_equal(&id->guid, &guid)) {
			list_del(&dynid->node);
			kfree(dynid);
			retval = count;
			break;
		}
	}
	spin_unlock(&drv->dynids.lock);

	return retval;
}
static DRIVER_ATTR_WO(remove_id);

static struct attribute *vmbus_drv_attrs[] = {
	&driver_attr_new_id.attr,
	&driver_attr_remove_id.attr,
	NULL,
};
ATTRIBUTE_GROUPS(vmbus_drv);


 
static int vmbus_match(struct device *device, struct device_driver *driver)
{
	struct hv_driver *drv = drv_to_hv_drv(driver);
	struct hv_device *hv_dev = device_to_hv_device(device);

	 
	if (is_hvsock_channel(hv_dev->channel))
		return drv->hvsock;

	if (hv_vmbus_get_id(drv, hv_dev))
		return 1;

	return 0;
}

 
static int vmbus_probe(struct device *child_device)
{
	int ret = 0;
	struct hv_driver *drv =
			drv_to_hv_drv(child_device->driver);
	struct hv_device *dev = device_to_hv_device(child_device);
	const struct hv_vmbus_device_id *dev_id;

	dev_id = hv_vmbus_get_id(drv, dev);
	if (drv->probe) {
		ret = drv->probe(dev, dev_id);
		if (ret != 0)
			pr_err("probe failed for device %s (%d)\n",
			       dev_name(child_device), ret);

	} else {
		pr_err("probe not set for driver %s\n",
		       dev_name(child_device));
		ret = -ENODEV;
	}
	return ret;
}

 
static int vmbus_dma_configure(struct device *child_device)
{
	 
	hv_setup_dma_ops(child_device,
		device_get_dma_attr(hv_dev) == DEV_DMA_COHERENT);
	return 0;
}

 
static void vmbus_remove(struct device *child_device)
{
	struct hv_driver *drv;
	struct hv_device *dev = device_to_hv_device(child_device);

	if (child_device->driver) {
		drv = drv_to_hv_drv(child_device->driver);
		if (drv->remove)
			drv->remove(dev);
	}
}

 
static void vmbus_shutdown(struct device *child_device)
{
	struct hv_driver *drv;
	struct hv_device *dev = device_to_hv_device(child_device);


	 
	if (!child_device->driver)
		return;

	drv = drv_to_hv_drv(child_device->driver);

	if (drv->shutdown)
		drv->shutdown(dev);
}

#ifdef CONFIG_PM_SLEEP
 
static int vmbus_suspend(struct device *child_device)
{
	struct hv_driver *drv;
	struct hv_device *dev = device_to_hv_device(child_device);

	 
	if (!child_device->driver)
		return 0;

	drv = drv_to_hv_drv(child_device->driver);
	if (!drv->suspend)
		return -EOPNOTSUPP;

	return drv->suspend(dev);
}

 
static int vmbus_resume(struct device *child_device)
{
	struct hv_driver *drv;
	struct hv_device *dev = device_to_hv_device(child_device);

	 
	if (!child_device->driver)
		return 0;

	drv = drv_to_hv_drv(child_device->driver);
	if (!drv->resume)
		return -EOPNOTSUPP;

	return drv->resume(dev);
}
#else
#define vmbus_suspend NULL
#define vmbus_resume NULL
#endif  

 
static void vmbus_device_release(struct device *device)
{
	struct hv_device *hv_dev = device_to_hv_device(device);
	struct vmbus_channel *channel = hv_dev->channel;

	hv_debug_rm_dev_dir(hv_dev);

	mutex_lock(&vmbus_connection.channel_mutex);
	hv_process_channel_removal(channel);
	mutex_unlock(&vmbus_connection.channel_mutex);
	kfree(hv_dev);
}

 

static const struct dev_pm_ops vmbus_pm = {
	.suspend_noirq	= NULL,
	.resume_noirq	= NULL,
	.freeze_noirq	= vmbus_suspend,
	.thaw_noirq	= vmbus_resume,
	.poweroff_noirq	= vmbus_suspend,
	.restore_noirq	= vmbus_resume,
};

 
static struct bus_type  hv_bus = {
	.name =		"vmbus",
	.match =		vmbus_match,
	.shutdown =		vmbus_shutdown,
	.remove =		vmbus_remove,
	.probe =		vmbus_probe,
	.uevent =		vmbus_uevent,
	.dma_configure =	vmbus_dma_configure,
	.dev_groups =		vmbus_dev_groups,
	.drv_groups =		vmbus_drv_groups,
	.bus_groups =		vmbus_bus_groups,
	.pm =			&vmbus_pm,
};

struct onmessage_work_context {
	struct work_struct work;
	struct {
		struct hv_message_header header;
		u8 payload[];
	} msg;
};

static void vmbus_onmessage_work(struct work_struct *work)
{
	struct onmessage_work_context *ctx;

	 
	if (vmbus_connection.conn_state == DISCONNECTED)
		return;

	ctx = container_of(work, struct onmessage_work_context,
			   work);
	vmbus_onmessage((struct vmbus_channel_message_header *)
			&ctx->msg.payload);
	kfree(ctx);
}

void vmbus_on_msg_dpc(unsigned long data)
{
	struct hv_per_cpu_context *hv_cpu = (void *)data;
	void *page_addr = hv_cpu->synic_message_page;
	struct hv_message msg_copy, *msg = (struct hv_message *)page_addr +
				  VMBUS_MESSAGE_SINT;
	struct vmbus_channel_message_header *hdr;
	enum vmbus_channel_message_type msgtype;
	const struct vmbus_channel_message_table_entry *entry;
	struct onmessage_work_context *ctx;
	__u8 payload_size;
	u32 message_type;

	 
	BUILD_BUG_ON(sizeof(enum vmbus_channel_message_type) != sizeof(u32));

	 
	memcpy(&msg_copy, msg, sizeof(struct hv_message));

	message_type = msg_copy.header.message_type;
	if (message_type == HVMSG_NONE)
		 
		return;

	hdr = (struct vmbus_channel_message_header *)msg_copy.u.payload;
	msgtype = hdr->msgtype;

	trace_vmbus_on_msg_dpc(hdr);

	if (msgtype >= CHANNELMSG_COUNT) {
		WARN_ONCE(1, "unknown msgtype=%d\n", msgtype);
		goto msg_handled;
	}

	payload_size = msg_copy.header.payload_size;
	if (payload_size > HV_MESSAGE_PAYLOAD_BYTE_COUNT) {
		WARN_ONCE(1, "payload size is too large (%d)\n", payload_size);
		goto msg_handled;
	}

	entry = &channel_message_table[msgtype];

	if (!entry->message_handler)
		goto msg_handled;

	if (payload_size < entry->min_payload_len) {
		WARN_ONCE(1, "message too short: msgtype=%d len=%d\n", msgtype, payload_size);
		goto msg_handled;
	}

	if (entry->handler_type	== VMHT_BLOCKING) {
		ctx = kmalloc(struct_size(ctx, msg.payload, payload_size), GFP_ATOMIC);
		if (ctx == NULL)
			return;

		INIT_WORK(&ctx->work, vmbus_onmessage_work);
		ctx->msg.header = msg_copy.header;
		memcpy(&ctx->msg.payload, msg_copy.u.payload, payload_size);

		 
		switch (msgtype) {
		case CHANNELMSG_RESCIND_CHANNELOFFER:
			 
			if (vmbus_connection.ignore_any_offer_msg)
				break;
			queue_work(vmbus_connection.rescind_work_queue, &ctx->work);
			break;

		case CHANNELMSG_OFFERCHANNEL:
			 
			if (vmbus_connection.ignore_any_offer_msg)
				break;
			atomic_inc(&vmbus_connection.offer_in_progress);
			fallthrough;

		default:
			queue_work(vmbus_connection.work_queue, &ctx->work);
		}
	} else
		entry->message_handler(hdr);

msg_handled:
	vmbus_signal_eom(msg, message_type);
}

#ifdef CONFIG_PM_SLEEP
 
static void vmbus_force_channel_rescinded(struct vmbus_channel *channel)
{
	struct onmessage_work_context *ctx;
	struct vmbus_channel_rescind_offer *rescind;

	WARN_ON(!is_hvsock_channel(channel));

	 
	ctx = kzalloc(sizeof(*ctx) + sizeof(*rescind),
		      GFP_KERNEL | __GFP_NOFAIL);

	 
	ctx->msg.header.message_type = 1;
	ctx->msg.header.payload_size = sizeof(*rescind);

	 
	rescind = (struct vmbus_channel_rescind_offer *)ctx->msg.payload;
	rescind->header.msgtype = CHANNELMSG_RESCIND_CHANNELOFFER;
	rescind->child_relid = channel->offermsg.child_relid;

	INIT_WORK(&ctx->work, vmbus_onmessage_work);

	queue_work(vmbus_connection.work_queue, &ctx->work);
}
#endif  

 
static void vmbus_chan_sched(struct hv_per_cpu_context *hv_cpu)
{
	unsigned long *recv_int_page;
	u32 maxbits, relid;

	 
	void *page_addr = hv_cpu->synic_event_page;
	union hv_synic_event_flags *event
		= (union hv_synic_event_flags *)page_addr +
					 VMBUS_MESSAGE_SINT;

	maxbits = HV_EVENT_FLAGS_COUNT;
	recv_int_page = event->flags;

	if (unlikely(!recv_int_page))
		return;

	for_each_set_bit(relid, recv_int_page, maxbits) {
		void (*callback_fn)(void *context);
		struct vmbus_channel *channel;

		if (!sync_test_and_clear_bit(relid, recv_int_page))
			continue;

		 
		if (relid == 0)
			continue;

		 
		rcu_read_lock();

		 
		channel = relid2channel(relid);
		if (channel == NULL)
			goto sched_unlock_rcu;

		if (channel->rescind)
			goto sched_unlock_rcu;

		 
		spin_lock(&channel->sched_lock);

		callback_fn = channel->onchannel_callback;
		if (unlikely(callback_fn == NULL))
			goto sched_unlock;

		trace_vmbus_chan_sched(channel);

		++channel->interrupts;

		switch (channel->callback_mode) {
		case HV_CALL_ISR:
			(*callback_fn)(channel->channel_callback_context);
			break;

		case HV_CALL_BATCHED:
			hv_begin_read(&channel->inbound);
			fallthrough;
		case HV_CALL_DIRECT:
			tasklet_schedule(&channel->callback_event);
		}

sched_unlock:
		spin_unlock(&channel->sched_lock);
sched_unlock_rcu:
		rcu_read_unlock();
	}
}

static void vmbus_isr(void)
{
	struct hv_per_cpu_context *hv_cpu
		= this_cpu_ptr(hv_context.cpu_context);
	void *page_addr;
	struct hv_message *msg;

	vmbus_chan_sched(hv_cpu);

	page_addr = hv_cpu->synic_message_page;
	msg = (struct hv_message *)page_addr + VMBUS_MESSAGE_SINT;

	 
	if (msg->header.message_type != HVMSG_NONE) {
		if (msg->header.message_type == HVMSG_TIMER_EXPIRED) {
			hv_stimer0_isr();
			vmbus_signal_eom(msg, HVMSG_TIMER_EXPIRED);
		} else
			tasklet_schedule(&hv_cpu->msg_dpc);
	}

	add_interrupt_randomness(vmbus_interrupt);
}

static irqreturn_t vmbus_percpu_isr(int irq, void *dev_id)
{
	vmbus_isr();
	return IRQ_HANDLED;
}

 
static int vmbus_bus_init(void)
{
	int ret;

	ret = hv_init();
	if (ret != 0) {
		pr_err("Unable to initialize the hypervisor - 0x%x\n", ret);
		return ret;
	}

	ret = bus_register(&hv_bus);
	if (ret)
		return ret;

	 

	if (vmbus_irq == -1) {
		hv_setup_vmbus_handler(vmbus_isr);
	} else {
		vmbus_evt = alloc_percpu(long);
		ret = request_percpu_irq(vmbus_irq, vmbus_percpu_isr,
				"Hyper-V VMbus", vmbus_evt);
		if (ret) {
			pr_err("Can't request Hyper-V VMbus IRQ %d, Err %d",
					vmbus_irq, ret);
			free_percpu(vmbus_evt);
			goto err_setup;
		}
	}

	ret = hv_synic_alloc();
	if (ret)
		goto err_alloc;

	 
	ret = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "hyperv/vmbus:online",
				hv_synic_init, hv_synic_cleanup);
	if (ret < 0)
		goto err_alloc;
	hyperv_cpuhp_online = ret;

	ret = vmbus_connect();
	if (ret)
		goto err_connect;

	 
	atomic_notifier_chain_register(&panic_notifier_list,
			       &hyperv_panic_vmbus_unload_block);

	vmbus_request_offers();

	return 0;

err_connect:
	cpuhp_remove_state(hyperv_cpuhp_online);
err_alloc:
	hv_synic_free();
	if (vmbus_irq == -1) {
		hv_remove_vmbus_handler();
	} else {
		free_percpu_irq(vmbus_irq, vmbus_evt);
		free_percpu(vmbus_evt);
	}
err_setup:
	bus_unregister(&hv_bus);
	return ret;
}

 
int __vmbus_driver_register(struct hv_driver *hv_driver, struct module *owner, const char *mod_name)
{
	int ret;

	pr_info("registering driver %s\n", hv_driver->name);

	ret = vmbus_exists();
	if (ret < 0)
		return ret;

	hv_driver->driver.name = hv_driver->name;
	hv_driver->driver.owner = owner;
	hv_driver->driver.mod_name = mod_name;
	hv_driver->driver.bus = &hv_bus;

	spin_lock_init(&hv_driver->dynids.lock);
	INIT_LIST_HEAD(&hv_driver->dynids.list);

	ret = driver_register(&hv_driver->driver);

	return ret;
}
EXPORT_SYMBOL_GPL(__vmbus_driver_register);

 
void vmbus_driver_unregister(struct hv_driver *hv_driver)
{
	pr_info("unregistering driver %s\n", hv_driver->name);

	if (!vmbus_exists()) {
		driver_unregister(&hv_driver->driver);
		vmbus_free_dynids(hv_driver);
	}
}
EXPORT_SYMBOL_GPL(vmbus_driver_unregister);


 
static void vmbus_chan_release(struct kobject *kobj)
{
	struct vmbus_channel *channel
		= container_of(kobj, struct vmbus_channel, kobj);

	kfree_rcu(channel, rcu);
}

struct vmbus_chan_attribute {
	struct attribute attr;
	ssize_t (*show)(struct vmbus_channel *chan, char *buf);
	ssize_t (*store)(struct vmbus_channel *chan,
			 const char *buf, size_t count);
};
#define VMBUS_CHAN_ATTR(_name, _mode, _show, _store) \
	struct vmbus_chan_attribute chan_attr_##_name \
		= __ATTR(_name, _mode, _show, _store)
#define VMBUS_CHAN_ATTR_RW(_name) \
	struct vmbus_chan_attribute chan_attr_##_name = __ATTR_RW(_name)
#define VMBUS_CHAN_ATTR_RO(_name) \
	struct vmbus_chan_attribute chan_attr_##_name = __ATTR_RO(_name)
#define VMBUS_CHAN_ATTR_WO(_name) \
	struct vmbus_chan_attribute chan_attr_##_name = __ATTR_WO(_name)

static ssize_t vmbus_chan_attr_show(struct kobject *kobj,
				    struct attribute *attr, char *buf)
{
	const struct vmbus_chan_attribute *attribute
		= container_of(attr, struct vmbus_chan_attribute, attr);
	struct vmbus_channel *chan
		= container_of(kobj, struct vmbus_channel, kobj);

	if (!attribute->show)
		return -EIO;

	return attribute->show(chan, buf);
}

static ssize_t vmbus_chan_attr_store(struct kobject *kobj,
				     struct attribute *attr, const char *buf,
				     size_t count)
{
	const struct vmbus_chan_attribute *attribute
		= container_of(attr, struct vmbus_chan_attribute, attr);
	struct vmbus_channel *chan
		= container_of(kobj, struct vmbus_channel, kobj);

	if (!attribute->store)
		return -EIO;

	return attribute->store(chan, buf, count);
}

static const struct sysfs_ops vmbus_chan_sysfs_ops = {
	.show = vmbus_chan_attr_show,
	.store = vmbus_chan_attr_store,
};

static ssize_t out_mask_show(struct vmbus_channel *channel, char *buf)
{
	struct hv_ring_buffer_info *rbi = &channel->outbound;
	ssize_t ret;

	mutex_lock(&rbi->ring_buffer_mutex);
	if (!rbi->ring_buffer) {
		mutex_unlock(&rbi->ring_buffer_mutex);
		return -EINVAL;
	}

	ret = sprintf(buf, "%u\n", rbi->ring_buffer->interrupt_mask);
	mutex_unlock(&rbi->ring_buffer_mutex);
	return ret;
}
static VMBUS_CHAN_ATTR_RO(out_mask);

static ssize_t in_mask_show(struct vmbus_channel *channel, char *buf)
{
	struct hv_ring_buffer_info *rbi = &channel->inbound;
	ssize_t ret;

	mutex_lock(&rbi->ring_buffer_mutex);
	if (!rbi->ring_buffer) {
		mutex_unlock(&rbi->ring_buffer_mutex);
		return -EINVAL;
	}

	ret = sprintf(buf, "%u\n", rbi->ring_buffer->interrupt_mask);
	mutex_unlock(&rbi->ring_buffer_mutex);
	return ret;
}
static VMBUS_CHAN_ATTR_RO(in_mask);

static ssize_t read_avail_show(struct vmbus_channel *channel, char *buf)
{
	struct hv_ring_buffer_info *rbi = &channel->inbound;
	ssize_t ret;

	mutex_lock(&rbi->ring_buffer_mutex);
	if (!rbi->ring_buffer) {
		mutex_unlock(&rbi->ring_buffer_mutex);
		return -EINVAL;
	}

	ret = sprintf(buf, "%u\n", hv_get_bytes_to_read(rbi));
	mutex_unlock(&rbi->ring_buffer_mutex);
	return ret;
}
static VMBUS_CHAN_ATTR_RO(read_avail);

static ssize_t write_avail_show(struct vmbus_channel *channel, char *buf)
{
	struct hv_ring_buffer_info *rbi = &channel->outbound;
	ssize_t ret;

	mutex_lock(&rbi->ring_buffer_mutex);
	if (!rbi->ring_buffer) {
		mutex_unlock(&rbi->ring_buffer_mutex);
		return -EINVAL;
	}

	ret = sprintf(buf, "%u\n", hv_get_bytes_to_write(rbi));
	mutex_unlock(&rbi->ring_buffer_mutex);
	return ret;
}
static VMBUS_CHAN_ATTR_RO(write_avail);

static ssize_t target_cpu_show(struct vmbus_channel *channel, char *buf)
{
	return sprintf(buf, "%u\n", channel->target_cpu);
}
static ssize_t target_cpu_store(struct vmbus_channel *channel,
				const char *buf, size_t count)
{
	u32 target_cpu, origin_cpu;
	ssize_t ret = count;

	if (vmbus_proto_version < VERSION_WIN10_V4_1)
		return -EIO;

	if (sscanf(buf, "%uu", &target_cpu) != 1)
		return -EIO;

	 
	if (target_cpu >= nr_cpumask_bits)
		return -EINVAL;

	if (!cpumask_test_cpu(target_cpu, housekeeping_cpumask(HK_TYPE_MANAGED_IRQ)))
		return -EINVAL;

	 
	cpus_read_lock();

	if (!cpu_online(target_cpu)) {
		cpus_read_unlock();
		return -EINVAL;
	}

	 
	mutex_lock(&vmbus_connection.channel_mutex);

	 
	if (channel->state != CHANNEL_OPENED_STATE) {
		ret = -EIO;
		goto cpu_store_unlock;
	}

	origin_cpu = channel->target_cpu;
	if (target_cpu == origin_cpu)
		goto cpu_store_unlock;

	if (vmbus_send_modifychannel(channel,
				     hv_cpu_number_to_vp_number(target_cpu))) {
		ret = -EIO;
		goto cpu_store_unlock;
	}

	 

	channel->target_cpu = target_cpu;

	 
	if (hv_is_perf_channel(channel))
		hv_update_allocated_cpus(origin_cpu, target_cpu);

	 
	if (channel->change_target_cpu_callback) {
		(*channel->change_target_cpu_callback)(channel,
				origin_cpu, target_cpu);
	}

cpu_store_unlock:
	mutex_unlock(&vmbus_connection.channel_mutex);
	cpus_read_unlock();
	return ret;
}
static VMBUS_CHAN_ATTR(cpu, 0644, target_cpu_show, target_cpu_store);

static ssize_t channel_pending_show(struct vmbus_channel *channel,
				    char *buf)
{
	return sprintf(buf, "%d\n",
		       channel_pending(channel,
				       vmbus_connection.monitor_pages[1]));
}
static VMBUS_CHAN_ATTR(pending, 0444, channel_pending_show, NULL);

static ssize_t channel_latency_show(struct vmbus_channel *channel,
				    char *buf)
{
	return sprintf(buf, "%d\n",
		       channel_latency(channel,
				       vmbus_connection.monitor_pages[1]));
}
static VMBUS_CHAN_ATTR(latency, 0444, channel_latency_show, NULL);

static ssize_t channel_interrupts_show(struct vmbus_channel *channel, char *buf)
{
	return sprintf(buf, "%llu\n", channel->interrupts);
}
static VMBUS_CHAN_ATTR(interrupts, 0444, channel_interrupts_show, NULL);

static ssize_t channel_events_show(struct vmbus_channel *channel, char *buf)
{
	return sprintf(buf, "%llu\n", channel->sig_events);
}
static VMBUS_CHAN_ATTR(events, 0444, channel_events_show, NULL);

static ssize_t channel_intr_in_full_show(struct vmbus_channel *channel,
					 char *buf)
{
	return sprintf(buf, "%llu\n",
		       (unsigned long long)channel->intr_in_full);
}
static VMBUS_CHAN_ATTR(intr_in_full, 0444, channel_intr_in_full_show, NULL);

static ssize_t channel_intr_out_empty_show(struct vmbus_channel *channel,
					   char *buf)
{
	return sprintf(buf, "%llu\n",
		       (unsigned long long)channel->intr_out_empty);
}
static VMBUS_CHAN_ATTR(intr_out_empty, 0444, channel_intr_out_empty_show, NULL);

static ssize_t channel_out_full_first_show(struct vmbus_channel *channel,
					   char *buf)
{
	return sprintf(buf, "%llu\n",
		       (unsigned long long)channel->out_full_first);
}
static VMBUS_CHAN_ATTR(out_full_first, 0444, channel_out_full_first_show, NULL);

static ssize_t channel_out_full_total_show(struct vmbus_channel *channel,
					   char *buf)
{
	return sprintf(buf, "%llu\n",
		       (unsigned long long)channel->out_full_total);
}
static VMBUS_CHAN_ATTR(out_full_total, 0444, channel_out_full_total_show, NULL);

static ssize_t subchannel_monitor_id_show(struct vmbus_channel *channel,
					  char *buf)
{
	return sprintf(buf, "%u\n", channel->offermsg.monitorid);
}
static VMBUS_CHAN_ATTR(monitor_id, 0444, subchannel_monitor_id_show, NULL);

static ssize_t subchannel_id_show(struct vmbus_channel *channel,
				  char *buf)
{
	return sprintf(buf, "%u\n",
		       channel->offermsg.offer.sub_channel_index);
}
static VMBUS_CHAN_ATTR_RO(subchannel_id);

static struct attribute *vmbus_chan_attrs[] = {
	&chan_attr_out_mask.attr,
	&chan_attr_in_mask.attr,
	&chan_attr_read_avail.attr,
	&chan_attr_write_avail.attr,
	&chan_attr_cpu.attr,
	&chan_attr_pending.attr,
	&chan_attr_latency.attr,
	&chan_attr_interrupts.attr,
	&chan_attr_events.attr,
	&chan_attr_intr_in_full.attr,
	&chan_attr_intr_out_empty.attr,
	&chan_attr_out_full_first.attr,
	&chan_attr_out_full_total.attr,
	&chan_attr_monitor_id.attr,
	&chan_attr_subchannel_id.attr,
	NULL
};

 
static umode_t vmbus_chan_attr_is_visible(struct kobject *kobj,
					  struct attribute *attr, int idx)
{
	const struct vmbus_channel *channel =
		container_of(kobj, struct vmbus_channel, kobj);

	 
	if (!channel->offermsg.monitor_allocated &&
	    (attr == &chan_attr_pending.attr ||
	     attr == &chan_attr_latency.attr ||
	     attr == &chan_attr_monitor_id.attr))
		return 0;

	return attr->mode;
}

static struct attribute_group vmbus_chan_group = {
	.attrs = vmbus_chan_attrs,
	.is_visible = vmbus_chan_attr_is_visible
};

static struct kobj_type vmbus_chan_ktype = {
	.sysfs_ops = &vmbus_chan_sysfs_ops,
	.release = vmbus_chan_release,
};

 
int vmbus_add_channel_kobj(struct hv_device *dev, struct vmbus_channel *channel)
{
	const struct device *device = &dev->device;
	struct kobject *kobj = &channel->kobj;
	u32 relid = channel->offermsg.child_relid;
	int ret;

	kobj->kset = dev->channels_kset;
	ret = kobject_init_and_add(kobj, &vmbus_chan_ktype, NULL,
				   "%u", relid);
	if (ret) {
		kobject_put(kobj);
		return ret;
	}

	ret = sysfs_create_group(kobj, &vmbus_chan_group);

	if (ret) {
		 
		kobject_put(kobj);
		dev_err(device, "Unable to set up channel sysfs files\n");
		return ret;
	}

	kobject_uevent(kobj, KOBJ_ADD);

	return 0;
}

 
void vmbus_remove_channel_attr_group(struct vmbus_channel *channel)
{
	sysfs_remove_group(&channel->kobj, &vmbus_chan_group);
}

 
struct hv_device *vmbus_device_create(const guid_t *type,
				      const guid_t *instance,
				      struct vmbus_channel *channel)
{
	struct hv_device *child_device_obj;

	child_device_obj = kzalloc(sizeof(struct hv_device), GFP_KERNEL);
	if (!child_device_obj) {
		pr_err("Unable to allocate device object for child device\n");
		return NULL;
	}

	child_device_obj->channel = channel;
	guid_copy(&child_device_obj->dev_type, type);
	guid_copy(&child_device_obj->dev_instance, instance);
	child_device_obj->vendor_id = PCI_VENDOR_ID_MICROSOFT;

	return child_device_obj;
}

 
int vmbus_device_register(struct hv_device *child_device_obj)
{
	struct kobject *kobj = &child_device_obj->device.kobj;
	int ret;

	dev_set_name(&child_device_obj->device, "%pUl",
		     &child_device_obj->channel->offermsg.offer.if_instance);

	child_device_obj->device.bus = &hv_bus;
	child_device_obj->device.parent = hv_dev;
	child_device_obj->device.release = vmbus_device_release;

	child_device_obj->device.dma_parms = &child_device_obj->dma_parms;
	child_device_obj->device.dma_mask = &child_device_obj->dma_mask;
	dma_set_mask(&child_device_obj->device, DMA_BIT_MASK(64));

	 
	ret = device_register(&child_device_obj->device);
	if (ret) {
		pr_err("Unable to register child device\n");
		put_device(&child_device_obj->device);
		return ret;
	}

	child_device_obj->channels_kset = kset_create_and_add("channels",
							      NULL, kobj);
	if (!child_device_obj->channels_kset) {
		ret = -ENOMEM;
		goto err_dev_unregister;
	}

	ret = vmbus_add_channel_kobj(child_device_obj,
				     child_device_obj->channel);
	if (ret) {
		pr_err("Unable to register primary channeln");
		goto err_kset_unregister;
	}
	hv_debug_add_dev_dir(child_device_obj);

	return 0;

err_kset_unregister:
	kset_unregister(child_device_obj->channels_kset);

err_dev_unregister:
	device_unregister(&child_device_obj->device);
	return ret;
}

 
void vmbus_device_unregister(struct hv_device *device_obj)
{
	pr_debug("child device %s unregistered\n",
		dev_name(&device_obj->device));

	kset_unregister(device_obj->channels_kset);

	 
	device_unregister(&device_obj->device);
}

#ifdef CONFIG_ACPI
 
static acpi_status vmbus_walk_resources(struct acpi_resource *res, void *ctx)
{
	resource_size_t start = 0;
	resource_size_t end = 0;
	struct resource *new_res;
	struct resource **old_res = &hyperv_mmio;
	struct resource **prev_res = NULL;
	struct resource r;

	switch (res->type) {

	 
	case ACPI_RESOURCE_TYPE_ADDRESS32:
		start = res->data.address32.address.minimum;
		end = res->data.address32.address.maximum;
		break;

	case ACPI_RESOURCE_TYPE_ADDRESS64:
		start = res->data.address64.address.minimum;
		end = res->data.address64.address.maximum;
		break;

	 
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		if (!acpi_dev_resource_interrupt(res, 0, &r)) {
			pr_err("Unable to parse Hyper-V ACPI interrupt\n");
			return AE_ERROR;
		}
		 
		vmbus_interrupt = res->data.extended_irq.interrupts[0];
		 
		vmbus_irq = r.start;
		return AE_OK;

	default:
		 
		return AE_OK;

	}
	 
	if (end < 0x100000)
		return AE_OK;

	new_res = kzalloc(sizeof(*new_res), GFP_ATOMIC);
	if (!new_res)
		return AE_NO_MEMORY;

	 
	if (end > VTPM_BASE_ADDRESS && start < VTPM_BASE_ADDRESS)
		end = VTPM_BASE_ADDRESS;

	new_res->name = "hyperv mmio";
	new_res->flags = IORESOURCE_MEM;
	new_res->start = start;
	new_res->end = end;

	 
	do {
		if (!*old_res) {
			*old_res = new_res;
			break;
		}

		if (((*old_res)->end + 1) == new_res->start) {
			(*old_res)->end = new_res->end;
			kfree(new_res);
			break;
		}

		if ((*old_res)->start == new_res->end + 1) {
			(*old_res)->start = new_res->start;
			kfree(new_res);
			break;
		}

		if ((*old_res)->start > new_res->end) {
			new_res->sibling = *old_res;
			if (prev_res)
				(*prev_res)->sibling = new_res;
			*old_res = new_res;
			break;
		}

		prev_res = old_res;
		old_res = &(*old_res)->sibling;

	} while (1);

	return AE_OK;
}
#endif

static void vmbus_mmio_remove(void)
{
	struct resource *cur_res;
	struct resource *next_res;

	if (hyperv_mmio) {
		if (fb_mmio) {
			__release_region(hyperv_mmio, fb_mmio->start,
					 resource_size(fb_mmio));
			fb_mmio = NULL;
		}

		for (cur_res = hyperv_mmio; cur_res; cur_res = next_res) {
			next_res = cur_res->sibling;
			kfree(cur_res);
		}
	}
}

static void __maybe_unused vmbus_reserve_fb(void)
{
	resource_size_t start = 0, size;
	struct pci_dev *pdev;

	if (efi_enabled(EFI_BOOT)) {
		 
		start = screen_info.lfb_base;
		size = max_t(__u32, screen_info.lfb_size, 0x800000);
	} else {
		 
		pdev = pci_get_device(PCI_VENDOR_ID_MICROSOFT,
				      PCI_DEVICE_ID_HYPERV_VIDEO, NULL);
		if (!pdev)
			return;

		if (pdev->resource[0].flags & IORESOURCE_MEM) {
			start = pci_resource_start(pdev, 0);
			size = pci_resource_len(pdev, 0);
		}

		 
		pci_dev_put(pdev);
	}

	if (!start)
		return;

	 
	for (; !fb_mmio && (size >= 0x100000); size >>= 1)
		fb_mmio = __request_region(hyperv_mmio, start, size, fb_mmio_name, 0);
}

 
int vmbus_allocate_mmio(struct resource **new, struct hv_device *device_obj,
			resource_size_t min, resource_size_t max,
			resource_size_t size, resource_size_t align,
			bool fb_overlap_ok)
{
	struct resource *iter, *shadow;
	resource_size_t range_min, range_max, start, end;
	const char *dev_n = dev_name(&device_obj->device);
	int retval;

	retval = -ENXIO;
	mutex_lock(&hyperv_mmio_lock);

	 
	if (fb_overlap_ok && fb_mmio && !(min > fb_mmio->end) &&
	    !(max < fb_mmio->start)) {

		range_min = fb_mmio->start;
		range_max = fb_mmio->end;
		start = (range_min + align - 1) & ~(align - 1);
		for (; start + size - 1 <= range_max; start += align) {
			*new = request_mem_region_exclusive(start, size, dev_n);
			if (*new) {
				retval = 0;
				goto exit;
			}
		}
	}

	for (iter = hyperv_mmio; iter; iter = iter->sibling) {
		if ((iter->start >= max) || (iter->end <= min))
			continue;

		range_min = iter->start;
		range_max = iter->end;
		start = (range_min + align - 1) & ~(align - 1);
		for (; start + size - 1 <= range_max; start += align) {
			end = start + size - 1;

			 
			if (!fb_overlap_ok && fb_mmio &&
			    (((start >= fb_mmio->start) && (start <= fb_mmio->end)) ||
			     ((end >= fb_mmio->start) && (end <= fb_mmio->end))))
				continue;

			shadow = __request_region(iter, start, size, NULL,
						  IORESOURCE_BUSY);
			if (!shadow)
				continue;

			*new = request_mem_region_exclusive(start, size, dev_n);
			if (*new) {
				shadow->name = (char *)*new;
				retval = 0;
				goto exit;
			}

			__release_region(iter, start, size);
		}
	}

exit:
	mutex_unlock(&hyperv_mmio_lock);
	return retval;
}
EXPORT_SYMBOL_GPL(vmbus_allocate_mmio);

 
void vmbus_free_mmio(resource_size_t start, resource_size_t size)
{
	struct resource *iter;

	mutex_lock(&hyperv_mmio_lock);
	for (iter = hyperv_mmio; iter; iter = iter->sibling) {
		if ((iter->start >= start + size) || (iter->end <= start))
			continue;

		__release_region(iter, start, size);
	}
	release_mem_region(start, size);
	mutex_unlock(&hyperv_mmio_lock);

}
EXPORT_SYMBOL_GPL(vmbus_free_mmio);

#ifdef CONFIG_ACPI
static int vmbus_acpi_add(struct platform_device *pdev)
{
	acpi_status result;
	int ret_val = -ENODEV;
	struct acpi_device *ancestor;
	struct acpi_device *device = ACPI_COMPANION(&pdev->dev);

	hv_dev = &device->dev;

	 
	ACPI_COMPANION_SET(&device->dev, device);
	if (IS_ENABLED(CONFIG_ACPI_CCA_REQUIRED) &&
	    device_get_dma_attr(&device->dev) == DEV_DMA_NOT_SUPPORTED) {
		pr_info("No ACPI _CCA found; assuming coherent device I/O\n");
		device->flags.cca_seen = true;
		device->flags.coherent_dma = true;
	}

	result = acpi_walk_resources(device->handle, METHOD_NAME__CRS,
					vmbus_walk_resources, NULL);

	if (ACPI_FAILURE(result))
		goto acpi_walk_err;
	 
	for (ancestor = acpi_dev_parent(device);
	     ancestor && ancestor->handle != ACPI_ROOT_OBJECT;
	     ancestor = acpi_dev_parent(ancestor)) {
		result = acpi_walk_resources(ancestor->handle, METHOD_NAME__CRS,
					     vmbus_walk_resources, NULL);

		if (ACPI_FAILURE(result))
			continue;
		if (hyperv_mmio) {
			vmbus_reserve_fb();
			break;
		}
	}
	ret_val = 0;

acpi_walk_err:
	if (ret_val)
		vmbus_mmio_remove();
	return ret_val;
}
#else
static int vmbus_acpi_add(struct platform_device *pdev)
{
	return 0;
}
#endif

static int vmbus_device_add(struct platform_device *pdev)
{
	struct resource **cur_res = &hyperv_mmio;
	struct of_range range;
	struct of_range_parser parser;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	hv_dev = &pdev->dev;

	ret = of_range_parser_init(&parser, np);
	if (ret)
		return ret;

	for_each_of_range(&parser, &range) {
		struct resource *res;

		res = kzalloc(sizeof(*res), GFP_KERNEL);
		if (!res) {
			vmbus_mmio_remove();
			return -ENOMEM;
		}

		res->name = "hyperv mmio";
		res->flags = range.flags;
		res->start = range.cpu_addr;
		res->end = range.cpu_addr + range.size;

		*cur_res = res;
		cur_res = &res->sibling;
	}

	return ret;
}

static int vmbus_platform_driver_probe(struct platform_device *pdev)
{
	if (acpi_disabled)
		return vmbus_device_add(pdev);
	else
		return vmbus_acpi_add(pdev);
}

static int vmbus_platform_driver_remove(struct platform_device *pdev)
{
	vmbus_mmio_remove();
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int vmbus_bus_suspend(struct device *dev)
{
	struct hv_per_cpu_context *hv_cpu = per_cpu_ptr(
			hv_context.cpu_context, VMBUS_CONNECT_CPU);
	struct vmbus_channel *channel, *sc;

	tasklet_disable(&hv_cpu->msg_dpc);
	vmbus_connection.ignore_any_offer_msg = true;
	 
	tasklet_enable(&hv_cpu->msg_dpc);

	 
	drain_workqueue(vmbus_connection.rescind_work_queue);
	drain_workqueue(vmbus_connection.work_queue);
	drain_workqueue(vmbus_connection.handle_primary_chan_wq);
	drain_workqueue(vmbus_connection.handle_sub_chan_wq);

	mutex_lock(&vmbus_connection.channel_mutex);
	list_for_each_entry(channel, &vmbus_connection.chn_list, listentry) {
		if (!is_hvsock_channel(channel))
			continue;

		vmbus_force_channel_rescinded(channel);
	}
	mutex_unlock(&vmbus_connection.channel_mutex);

	 
	if (atomic_read(&vmbus_connection.nr_chan_close_on_suspend) > 0)
		wait_for_completion(&vmbus_connection.ready_for_suspend_event);

	if (atomic_read(&vmbus_connection.nr_chan_fixup_on_resume) != 0) {
		pr_err("Can not suspend due to a previous failed resuming\n");
		return -EBUSY;
	}

	mutex_lock(&vmbus_connection.channel_mutex);

	list_for_each_entry(channel, &vmbus_connection.chn_list, listentry) {
		 
		vmbus_channel_unmap_relid(channel);
		channel->offermsg.child_relid = INVALID_RELID;

		if (is_hvsock_channel(channel)) {
			if (!channel->rescind) {
				pr_err("hv_sock channel not rescinded!\n");
				WARN_ON_ONCE(1);
			}
			continue;
		}

		list_for_each_entry(sc, &channel->sc_list, sc_list) {
			pr_err("Sub-channel not deleted!\n");
			WARN_ON_ONCE(1);
		}

		atomic_inc(&vmbus_connection.nr_chan_fixup_on_resume);
	}

	mutex_unlock(&vmbus_connection.channel_mutex);

	vmbus_initiate_unload(false);

	 
	reinit_completion(&vmbus_connection.ready_for_resume_event);

	return 0;
}

static int vmbus_bus_resume(struct device *dev)
{
	struct vmbus_channel_msginfo *msginfo;
	size_t msgsize;
	int ret;

	vmbus_connection.ignore_any_offer_msg = false;

	 
	if (!vmbus_proto_version) {
		pr_err("Invalid proto version = 0x%x\n", vmbus_proto_version);
		return -EINVAL;
	}

	msgsize = sizeof(*msginfo) +
		  sizeof(struct vmbus_channel_initiate_contact);

	msginfo = kzalloc(msgsize, GFP_KERNEL);

	if (msginfo == NULL)
		return -ENOMEM;

	ret = vmbus_negotiate_version(msginfo, vmbus_proto_version);

	kfree(msginfo);

	if (ret != 0)
		return ret;

	WARN_ON(atomic_read(&vmbus_connection.nr_chan_fixup_on_resume) == 0);

	vmbus_request_offers();

	if (wait_for_completion_timeout(
		&vmbus_connection.ready_for_resume_event, 10 * HZ) == 0)
		pr_err("Some vmbus device is missing after suspending?\n");

	 
	reinit_completion(&vmbus_connection.ready_for_suspend_event);

	return 0;
}
#else
#define vmbus_bus_suspend NULL
#define vmbus_bus_resume NULL
#endif  

static const __maybe_unused struct of_device_id vmbus_of_match[] = {
	{
		.compatible = "microsoft,vmbus",
	},
	{
		 
	},
};
MODULE_DEVICE_TABLE(of, vmbus_of_match);

static const __maybe_unused struct acpi_device_id vmbus_acpi_device_ids[] = {
	{"VMBUS", 0},
	{"VMBus", 0},
	{"", 0},
};
MODULE_DEVICE_TABLE(acpi, vmbus_acpi_device_ids);

 

static const struct dev_pm_ops vmbus_bus_pm = {
	.suspend_noirq	= NULL,
	.resume_noirq	= NULL,
	.freeze_noirq	= vmbus_bus_suspend,
	.thaw_noirq	= vmbus_bus_resume,
	.poweroff_noirq	= vmbus_bus_suspend,
	.restore_noirq	= vmbus_bus_resume
};

static struct platform_driver vmbus_platform_driver = {
	.probe = vmbus_platform_driver_probe,
	.remove = vmbus_platform_driver_remove,
	.driver = {
		.name = "vmbus",
		.acpi_match_table = ACPI_PTR(vmbus_acpi_device_ids),
		.of_match_table = of_match_ptr(vmbus_of_match),
		.pm = &vmbus_bus_pm,
		.probe_type = PROBE_FORCE_SYNCHRONOUS,
	}
};

static void hv_kexec_handler(void)
{
	hv_stimer_global_cleanup();
	vmbus_initiate_unload(false);
	 
	mb();
	cpuhp_remove_state(hyperv_cpuhp_online);
};

static void hv_crash_handler(struct pt_regs *regs)
{
	int cpu;

	vmbus_initiate_unload(true);
	 
	cpu = smp_processor_id();
	hv_stimer_cleanup(cpu);
	hv_synic_disable_regs(cpu);
};

static int hv_synic_suspend(void)
{
	 

	hv_synic_disable_regs(0);

	return 0;
}

static void hv_synic_resume(void)
{
	hv_synic_enable_regs(0);

	 
}

 
static struct syscore_ops hv_synic_syscore_ops = {
	.suspend = hv_synic_suspend,
	.resume = hv_synic_resume,
};

static int __init hv_acpi_init(void)
{
	int ret;

	if (!hv_is_hyperv_initialized())
		return -ENODEV;

	if (hv_root_partition && !hv_nested)
		return 0;

	 
	ret = platform_driver_register(&vmbus_platform_driver);
	if (ret)
		return ret;

	if (!hv_dev) {
		ret = -ENODEV;
		goto cleanup;
	}

	 
#ifdef HYPERVISOR_CALLBACK_VECTOR
	vmbus_interrupt = HYPERVISOR_CALLBACK_VECTOR;
	vmbus_irq = -1;
#endif

	hv_debug_init();

	ret = vmbus_bus_init();
	if (ret)
		goto cleanup;

	hv_setup_kexec_handler(hv_kexec_handler);
	hv_setup_crash_handler(hv_crash_handler);

	register_syscore_ops(&hv_synic_syscore_ops);

	return 0;

cleanup:
	platform_driver_unregister(&vmbus_platform_driver);
	hv_dev = NULL;
	return ret;
}

static void __exit vmbus_exit(void)
{
	int cpu;

	unregister_syscore_ops(&hv_synic_syscore_ops);

	hv_remove_kexec_handler();
	hv_remove_crash_handler();
	vmbus_connection.conn_state = DISCONNECTED;
	hv_stimer_global_cleanup();
	vmbus_disconnect();
	if (vmbus_irq == -1) {
		hv_remove_vmbus_handler();
	} else {
		free_percpu_irq(vmbus_irq, vmbus_evt);
		free_percpu(vmbus_evt);
	}
	for_each_online_cpu(cpu) {
		struct hv_per_cpu_context *hv_cpu
			= per_cpu_ptr(hv_context.cpu_context, cpu);

		tasklet_kill(&hv_cpu->msg_dpc);
	}
	hv_debug_rm_all_dir();

	vmbus_free_channels();
	kfree(vmbus_connection.channels);

	 
	atomic_notifier_chain_unregister(&panic_notifier_list,
					&hyperv_panic_vmbus_unload_block);

	bus_unregister(&hv_bus);

	cpuhp_remove_state(hyperv_cpuhp_online);
	hv_synic_free();
	platform_driver_unregister(&vmbus_platform_driver);
}


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Microsoft Hyper-V VMBus Driver");

subsys_initcall(hv_acpi_init);
module_exit(vmbus_exit);
