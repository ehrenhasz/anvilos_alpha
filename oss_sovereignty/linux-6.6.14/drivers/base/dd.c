
 

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/dma-map-ops.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/async.h>
#include <linux/pm_runtime.h>
#include <linux/pinctrl/devinfo.h>
#include <linux/slab.h>

#include "base.h"
#include "power/power.h"

 
static DEFINE_MUTEX(deferred_probe_mutex);
static LIST_HEAD(deferred_probe_pending_list);
static LIST_HEAD(deferred_probe_active_list);
static atomic_t deferred_trigger_count = ATOMIC_INIT(0);
static bool initcalls_done;

 
#define ASYNC_DRV_NAMES_MAX_LEN	256
static char async_probe_drv_names[ASYNC_DRV_NAMES_MAX_LEN];
static bool async_probe_default;

 
static bool defer_all_probes;

static void __device_set_deferred_probe_reason(const struct device *dev, char *reason)
{
	kfree(dev->p->deferred_probe_reason);
	dev->p->deferred_probe_reason = reason;
}

 
static void deferred_probe_work_func(struct work_struct *work)
{
	struct device *dev;
	struct device_private *private;
	 
	mutex_lock(&deferred_probe_mutex);
	while (!list_empty(&deferred_probe_active_list)) {
		private = list_first_entry(&deferred_probe_active_list,
					typeof(*dev->p), deferred_probe);
		dev = private->device;
		list_del_init(&private->deferred_probe);

		get_device(dev);

		__device_set_deferred_probe_reason(dev, NULL);

		 
		mutex_unlock(&deferred_probe_mutex);

		 
		device_pm_move_to_tail(dev);

		dev_dbg(dev, "Retrying from deferred list\n");
		bus_probe_device(dev);
		mutex_lock(&deferred_probe_mutex);

		put_device(dev);
	}
	mutex_unlock(&deferred_probe_mutex);
}
static DECLARE_WORK(deferred_probe_work, deferred_probe_work_func);

void driver_deferred_probe_add(struct device *dev)
{
	if (!dev->can_match)
		return;

	mutex_lock(&deferred_probe_mutex);
	if (list_empty(&dev->p->deferred_probe)) {
		dev_dbg(dev, "Added to deferred list\n");
		list_add_tail(&dev->p->deferred_probe, &deferred_probe_pending_list);
	}
	mutex_unlock(&deferred_probe_mutex);
}

void driver_deferred_probe_del(struct device *dev)
{
	mutex_lock(&deferred_probe_mutex);
	if (!list_empty(&dev->p->deferred_probe)) {
		dev_dbg(dev, "Removed from deferred list\n");
		list_del_init(&dev->p->deferred_probe);
		__device_set_deferred_probe_reason(dev, NULL);
	}
	mutex_unlock(&deferred_probe_mutex);
}

static bool driver_deferred_probe_enable;
 
void driver_deferred_probe_trigger(void)
{
	if (!driver_deferred_probe_enable)
		return;

	 
	mutex_lock(&deferred_probe_mutex);
	atomic_inc(&deferred_trigger_count);
	list_splice_tail_init(&deferred_probe_pending_list,
			      &deferred_probe_active_list);
	mutex_unlock(&deferred_probe_mutex);

	 
	queue_work(system_unbound_wq, &deferred_probe_work);
}

 
void device_block_probing(void)
{
	defer_all_probes = true;
	 
	wait_for_device_probe();
}

 
void device_unblock_probing(void)
{
	defer_all_probes = false;
	driver_deferred_probe_trigger();
}

 
void device_set_deferred_probe_reason(const struct device *dev, struct va_format *vaf)
{
	const char *drv = dev_driver_string(dev);
	char *reason;

	mutex_lock(&deferred_probe_mutex);

	reason = kasprintf(GFP_KERNEL, "%s: %pV", drv, vaf);
	__device_set_deferred_probe_reason(dev, reason);

	mutex_unlock(&deferred_probe_mutex);
}

 
static int deferred_devs_show(struct seq_file *s, void *data)
{
	struct device_private *curr;

	mutex_lock(&deferred_probe_mutex);

	list_for_each_entry(curr, &deferred_probe_pending_list, deferred_probe)
		seq_printf(s, "%s\t%s", dev_name(curr->device),
			   curr->device->p->deferred_probe_reason ?: "\n");

	mutex_unlock(&deferred_probe_mutex);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(deferred_devs);

#ifdef CONFIG_MODULES
static int driver_deferred_probe_timeout = 10;
#else
static int driver_deferred_probe_timeout;
#endif

static int __init deferred_probe_timeout_setup(char *str)
{
	int timeout;

	if (!kstrtoint(str, 10, &timeout))
		driver_deferred_probe_timeout = timeout;
	return 1;
}
__setup("deferred_probe_timeout=", deferred_probe_timeout_setup);

 
int driver_deferred_probe_check_state(struct device *dev)
{
	if (!IS_ENABLED(CONFIG_MODULES) && initcalls_done) {
		dev_warn(dev, "ignoring dependency for device, assuming no driver\n");
		return -ENODEV;
	}

	if (!driver_deferred_probe_timeout && initcalls_done) {
		dev_warn(dev, "deferred probe timeout, ignoring dependency\n");
		return -ETIMEDOUT;
	}

	return -EPROBE_DEFER;
}
EXPORT_SYMBOL_GPL(driver_deferred_probe_check_state);

static void deferred_probe_timeout_work_func(struct work_struct *work)
{
	struct device_private *p;

	fw_devlink_drivers_done();

	driver_deferred_probe_timeout = 0;
	driver_deferred_probe_trigger();
	flush_work(&deferred_probe_work);

	mutex_lock(&deferred_probe_mutex);
	list_for_each_entry(p, &deferred_probe_pending_list, deferred_probe)
		dev_info(p->device, "deferred probe pending\n");
	mutex_unlock(&deferred_probe_mutex);

	fw_devlink_probing_done();
}
static DECLARE_DELAYED_WORK(deferred_probe_timeout_work, deferred_probe_timeout_work_func);

void deferred_probe_extend_timeout(void)
{
	 
	if (cancel_delayed_work(&deferred_probe_timeout_work)) {
		schedule_delayed_work(&deferred_probe_timeout_work,
				driver_deferred_probe_timeout * HZ);
		pr_debug("Extended deferred probe timeout by %d secs\n",
					driver_deferred_probe_timeout);
	}
}

 
static int deferred_probe_initcall(void)
{
	debugfs_create_file("devices_deferred", 0444, NULL, NULL,
			    &deferred_devs_fops);

	driver_deferred_probe_enable = true;
	driver_deferred_probe_trigger();
	 
	flush_work(&deferred_probe_work);
	initcalls_done = true;

	if (!IS_ENABLED(CONFIG_MODULES))
		fw_devlink_drivers_done();

	 
	driver_deferred_probe_trigger();
	flush_work(&deferred_probe_work);

	if (driver_deferred_probe_timeout > 0) {
		schedule_delayed_work(&deferred_probe_timeout_work,
			driver_deferred_probe_timeout * HZ);
	}

	if (!IS_ENABLED(CONFIG_MODULES))
		fw_devlink_probing_done();

	return 0;
}
late_initcall(deferred_probe_initcall);

static void __exit deferred_probe_exit(void)
{
	debugfs_lookup_and_remove("devices_deferred", NULL);
}
__exitcall(deferred_probe_exit);

 
bool device_is_bound(struct device *dev)
{
	return dev->p && klist_node_attached(&dev->p->knode_driver);
}

static void driver_bound(struct device *dev)
{
	if (device_is_bound(dev)) {
		pr_warn("%s: device %s already bound\n",
			__func__, kobject_name(&dev->kobj));
		return;
	}

	pr_debug("driver: '%s': %s: bound to device '%s'\n", dev->driver->name,
		 __func__, dev_name(dev));

	klist_add_tail(&dev->p->knode_driver, &dev->driver->p->klist_devices);
	device_links_driver_bound(dev);

	device_pm_check_callbacks(dev);

	 
	driver_deferred_probe_del(dev);
	driver_deferred_probe_trigger();

	bus_notify(dev, BUS_NOTIFY_BOUND_DRIVER);
	kobject_uevent(&dev->kobj, KOBJ_BIND);
}

static ssize_t coredump_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	device_lock(dev);
	dev->driver->coredump(dev);
	device_unlock(dev);

	return count;
}
static DEVICE_ATTR_WO(coredump);

static int driver_sysfs_add(struct device *dev)
{
	int ret;

	bus_notify(dev, BUS_NOTIFY_BIND_DRIVER);

	ret = sysfs_create_link(&dev->driver->p->kobj, &dev->kobj,
				kobject_name(&dev->kobj));
	if (ret)
		goto fail;

	ret = sysfs_create_link(&dev->kobj, &dev->driver->p->kobj,
				"driver");
	if (ret)
		goto rm_dev;

	if (!IS_ENABLED(CONFIG_DEV_COREDUMP) || !dev->driver->coredump)
		return 0;

	ret = device_create_file(dev, &dev_attr_coredump);
	if (!ret)
		return 0;

	sysfs_remove_link(&dev->kobj, "driver");

rm_dev:
	sysfs_remove_link(&dev->driver->p->kobj,
			  kobject_name(&dev->kobj));

fail:
	return ret;
}

static void driver_sysfs_remove(struct device *dev)
{
	struct device_driver *drv = dev->driver;

	if (drv) {
		if (drv->coredump)
			device_remove_file(dev, &dev_attr_coredump);
		sysfs_remove_link(&drv->p->kobj, kobject_name(&dev->kobj));
		sysfs_remove_link(&dev->kobj, "driver");
	}
}

 
int device_bind_driver(struct device *dev)
{
	int ret;

	ret = driver_sysfs_add(dev);
	if (!ret) {
		device_links_force_bind(dev);
		driver_bound(dev);
	}
	else
		bus_notify(dev, BUS_NOTIFY_DRIVER_NOT_BOUND);
	return ret;
}
EXPORT_SYMBOL_GPL(device_bind_driver);

static atomic_t probe_count = ATOMIC_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(probe_waitqueue);

static ssize_t state_synced_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int ret = 0;

	if (strcmp("1", buf))
		return -EINVAL;

	device_lock(dev);
	if (!dev->state_synced) {
		dev->state_synced = true;
		dev_sync_state(dev);
	} else {
		ret = -EINVAL;
	}
	device_unlock(dev);

	return ret ? ret : count;
}

static ssize_t state_synced_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	bool val;

	device_lock(dev);
	val = dev->state_synced;
	device_unlock(dev);

	return sysfs_emit(buf, "%u\n", val);
}
static DEVICE_ATTR_RW(state_synced);

static void device_unbind_cleanup(struct device *dev)
{
	devres_release_all(dev);
	arch_teardown_dma_ops(dev);
	kfree(dev->dma_range_map);
	dev->dma_range_map = NULL;
	dev->driver = NULL;
	dev_set_drvdata(dev, NULL);
	if (dev->pm_domain && dev->pm_domain->dismiss)
		dev->pm_domain->dismiss(dev);
	pm_runtime_reinit(dev);
	dev_pm_set_driver_flags(dev, 0);
}

static void device_remove(struct device *dev)
{
	device_remove_file(dev, &dev_attr_state_synced);
	device_remove_groups(dev, dev->driver->dev_groups);

	if (dev->bus && dev->bus->remove)
		dev->bus->remove(dev);
	else if (dev->driver->remove)
		dev->driver->remove(dev);
}

static int call_driver_probe(struct device *dev, struct device_driver *drv)
{
	int ret = 0;

	if (dev->bus->probe)
		ret = dev->bus->probe(dev);
	else if (drv->probe)
		ret = drv->probe(dev);

	switch (ret) {
	case 0:
		break;
	case -EPROBE_DEFER:
		 
		dev_dbg(dev, "Driver %s requests probe deferral\n", drv->name);
		break;
	case -ENODEV:
	case -ENXIO:
		pr_debug("%s: probe of %s rejects match %d\n",
			 drv->name, dev_name(dev), ret);
		break;
	default:
		 
		pr_warn("%s: probe of %s failed with error %d\n",
			drv->name, dev_name(dev), ret);
		break;
	}

	return ret;
}

static int really_probe(struct device *dev, struct device_driver *drv)
{
	bool test_remove = IS_ENABLED(CONFIG_DEBUG_TEST_DRIVER_REMOVE) &&
			   !drv->suppress_bind_attrs;
	int ret, link_ret;

	if (defer_all_probes) {
		 
		dev_dbg(dev, "Driver %s force probe deferral\n", drv->name);
		return -EPROBE_DEFER;
	}

	link_ret = device_links_check_suppliers(dev);
	if (link_ret == -EPROBE_DEFER)
		return link_ret;

	pr_debug("bus: '%s': %s: probing driver %s with device %s\n",
		 drv->bus->name, __func__, drv->name, dev_name(dev));
	if (!list_empty(&dev->devres_head)) {
		dev_crit(dev, "Resources present before probing\n");
		ret = -EBUSY;
		goto done;
	}

re_probe:
	dev->driver = drv;

	 
	ret = pinctrl_bind_pins(dev);
	if (ret)
		goto pinctrl_bind_failed;

	if (dev->bus->dma_configure) {
		ret = dev->bus->dma_configure(dev);
		if (ret)
			goto pinctrl_bind_failed;
	}

	ret = driver_sysfs_add(dev);
	if (ret) {
		pr_err("%s: driver_sysfs_add(%s) failed\n",
		       __func__, dev_name(dev));
		goto sysfs_failed;
	}

	if (dev->pm_domain && dev->pm_domain->activate) {
		ret = dev->pm_domain->activate(dev);
		if (ret)
			goto probe_failed;
	}

	ret = call_driver_probe(dev, drv);
	if (ret) {
		 
		if (link_ret == -EAGAIN)
			ret = -EPROBE_DEFER;

		 
		ret = -ret;
		goto probe_failed;
	}

	ret = device_add_groups(dev, drv->dev_groups);
	if (ret) {
		dev_err(dev, "device_add_groups() failed\n");
		goto dev_groups_failed;
	}

	if (dev_has_sync_state(dev)) {
		ret = device_create_file(dev, &dev_attr_state_synced);
		if (ret) {
			dev_err(dev, "state_synced sysfs add failed\n");
			goto dev_sysfs_state_synced_failed;
		}
	}

	if (test_remove) {
		test_remove = false;

		device_remove(dev);
		driver_sysfs_remove(dev);
		if (dev->bus && dev->bus->dma_cleanup)
			dev->bus->dma_cleanup(dev);
		device_unbind_cleanup(dev);

		goto re_probe;
	}

	pinctrl_init_done(dev);

	if (dev->pm_domain && dev->pm_domain->sync)
		dev->pm_domain->sync(dev);

	driver_bound(dev);
	pr_debug("bus: '%s': %s: bound device %s to driver %s\n",
		 drv->bus->name, __func__, dev_name(dev), drv->name);
	goto done;

dev_sysfs_state_synced_failed:
dev_groups_failed:
	device_remove(dev);
probe_failed:
	driver_sysfs_remove(dev);
sysfs_failed:
	bus_notify(dev, BUS_NOTIFY_DRIVER_NOT_BOUND);
	if (dev->bus && dev->bus->dma_cleanup)
		dev->bus->dma_cleanup(dev);
pinctrl_bind_failed:
	device_links_no_driver(dev);
	device_unbind_cleanup(dev);
done:
	return ret;
}

 
static int really_probe_debug(struct device *dev, struct device_driver *drv)
{
	ktime_t calltime, rettime;
	int ret;

	calltime = ktime_get();
	ret = really_probe(dev, drv);
	rettime = ktime_get();
	 
	printk(KERN_DEBUG "probe of %s returned %d after %lld usecs\n",
		 dev_name(dev), ret, ktime_us_delta(rettime, calltime));
	return ret;
}

 
bool __init driver_probe_done(void)
{
	int local_probe_count = atomic_read(&probe_count);

	pr_debug("%s: probe_count = %d\n", __func__, local_probe_count);
	return !local_probe_count;
}

 
void wait_for_device_probe(void)
{
	 
	flush_work(&deferred_probe_work);

	 
	wait_event(probe_waitqueue, atomic_read(&probe_count) == 0);
	async_synchronize_full();
}
EXPORT_SYMBOL_GPL(wait_for_device_probe);

static int __driver_probe_device(struct device_driver *drv, struct device *dev)
{
	int ret = 0;

	if (dev->p->dead || !device_is_registered(dev))
		return -ENODEV;
	if (dev->driver)
		return -EBUSY;

	dev->can_match = true;
	pr_debug("bus: '%s': %s: matched device %s with driver %s\n",
		 drv->bus->name, __func__, dev_name(dev), drv->name);

	pm_runtime_get_suppliers(dev);
	if (dev->parent)
		pm_runtime_get_sync(dev->parent);

	pm_runtime_barrier(dev);
	if (initcall_debug)
		ret = really_probe_debug(dev, drv);
	else
		ret = really_probe(dev, drv);
	pm_request_idle(dev);

	if (dev->parent)
		pm_runtime_put(dev->parent);

	pm_runtime_put_suppliers(dev);
	return ret;
}

 
static int driver_probe_device(struct device_driver *drv, struct device *dev)
{
	int trigger_count = atomic_read(&deferred_trigger_count);
	int ret;

	atomic_inc(&probe_count);
	ret = __driver_probe_device(drv, dev);
	if (ret == -EPROBE_DEFER || ret == EPROBE_DEFER) {
		driver_deferred_probe_add(dev);

		 
		if (trigger_count != atomic_read(&deferred_trigger_count) &&
		    !defer_all_probes)
			driver_deferred_probe_trigger();
	}
	atomic_dec(&probe_count);
	wake_up_all(&probe_waitqueue);
	return ret;
}

static inline bool cmdline_requested_async_probing(const char *drv_name)
{
	bool async_drv;

	async_drv = parse_option_str(async_probe_drv_names, drv_name);

	return (async_probe_default != async_drv);
}

 
static int __init save_async_options(char *buf)
{
	if (strlen(buf) >= ASYNC_DRV_NAMES_MAX_LEN)
		pr_warn("Too long list of driver names for 'driver_async_probe'!\n");

	strscpy(async_probe_drv_names, buf, ASYNC_DRV_NAMES_MAX_LEN);
	async_probe_default = parse_option_str(async_probe_drv_names, "*");

	return 1;
}
__setup("driver_async_probe=", save_async_options);

static bool driver_allows_async_probing(struct device_driver *drv)
{
	switch (drv->probe_type) {
	case PROBE_PREFER_ASYNCHRONOUS:
		return true;

	case PROBE_FORCE_SYNCHRONOUS:
		return false;

	default:
		if (cmdline_requested_async_probing(drv->name))
			return true;

		if (module_requested_async_probing(drv->owner))
			return true;

		return false;
	}
}

struct device_attach_data {
	struct device *dev;

	 
	bool check_async;

	 
	bool want_async;

	 
	bool have_async;
};

static int __device_attach_driver(struct device_driver *drv, void *_data)
{
	struct device_attach_data *data = _data;
	struct device *dev = data->dev;
	bool async_allowed;
	int ret;

	ret = driver_match_device(drv, dev);
	if (ret == 0) {
		 
		return 0;
	} else if (ret == -EPROBE_DEFER) {
		dev_dbg(dev, "Device match requests probe deferral\n");
		dev->can_match = true;
		driver_deferred_probe_add(dev);
		 
		return ret;
	} else if (ret < 0) {
		dev_dbg(dev, "Bus failed to match device: %d\n", ret);
		return ret;
	}  

	async_allowed = driver_allows_async_probing(drv);

	if (async_allowed)
		data->have_async = true;

	if (data->check_async && async_allowed != data->want_async)
		return 0;

	 
	ret = driver_probe_device(drv, dev);
	if (ret < 0)
		return ret;
	return ret == 0;
}

static void __device_attach_async_helper(void *_dev, async_cookie_t cookie)
{
	struct device *dev = _dev;
	struct device_attach_data data = {
		.dev		= dev,
		.check_async	= true,
		.want_async	= true,
	};

	device_lock(dev);

	 
	if (dev->p->dead || dev->driver)
		goto out_unlock;

	if (dev->parent)
		pm_runtime_get_sync(dev->parent);

	bus_for_each_drv(dev->bus, NULL, &data, __device_attach_driver);
	dev_dbg(dev, "async probe completed\n");

	pm_request_idle(dev);

	if (dev->parent)
		pm_runtime_put(dev->parent);
out_unlock:
	device_unlock(dev);

	put_device(dev);
}

static int __device_attach(struct device *dev, bool allow_async)
{
	int ret = 0;
	bool async = false;

	device_lock(dev);
	if (dev->p->dead) {
		goto out_unlock;
	} else if (dev->driver) {
		if (device_is_bound(dev)) {
			ret = 1;
			goto out_unlock;
		}
		ret = device_bind_driver(dev);
		if (ret == 0)
			ret = 1;
		else {
			dev->driver = NULL;
			ret = 0;
		}
	} else {
		struct device_attach_data data = {
			.dev = dev,
			.check_async = allow_async,
			.want_async = false,
		};

		if (dev->parent)
			pm_runtime_get_sync(dev->parent);

		ret = bus_for_each_drv(dev->bus, NULL, &data,
					__device_attach_driver);
		if (!ret && allow_async && data.have_async) {
			 
			dev_dbg(dev, "scheduling asynchronous probe\n");
			get_device(dev);
			async = true;
		} else {
			pm_request_idle(dev);
		}

		if (dev->parent)
			pm_runtime_put(dev->parent);
	}
out_unlock:
	device_unlock(dev);
	if (async)
		async_schedule_dev(__device_attach_async_helper, dev);
	return ret;
}

 
int device_attach(struct device *dev)
{
	return __device_attach(dev, false);
}
EXPORT_SYMBOL_GPL(device_attach);

void device_initial_probe(struct device *dev)
{
	__device_attach(dev, true);
}

 
static void __device_driver_lock(struct device *dev, struct device *parent)
{
	if (parent && dev->bus->need_parent_lock)
		device_lock(parent);
	device_lock(dev);
}

 
static void __device_driver_unlock(struct device *dev, struct device *parent)
{
	device_unlock(dev);
	if (parent && dev->bus->need_parent_lock)
		device_unlock(parent);
}

 
int device_driver_attach(struct device_driver *drv, struct device *dev)
{
	int ret;

	__device_driver_lock(dev, dev->parent);
	ret = __driver_probe_device(drv, dev);
	__device_driver_unlock(dev, dev->parent);

	 
	if (ret > 0)
		ret = -ret;
	if (ret == -EPROBE_DEFER)
		return -EAGAIN;
	return ret;
}
EXPORT_SYMBOL_GPL(device_driver_attach);

static void __driver_attach_async_helper(void *_dev, async_cookie_t cookie)
{
	struct device *dev = _dev;
	struct device_driver *drv;
	int ret;

	__device_driver_lock(dev, dev->parent);
	drv = dev->p->async_driver;
	dev->p->async_driver = NULL;
	ret = driver_probe_device(drv, dev);
	__device_driver_unlock(dev, dev->parent);

	dev_dbg(dev, "driver %s async attach completed: %d\n", drv->name, ret);

	put_device(dev);
}

static int __driver_attach(struct device *dev, void *data)
{
	struct device_driver *drv = data;
	bool async = false;
	int ret;

	 

	ret = driver_match_device(drv, dev);
	if (ret == 0) {
		 
		return 0;
	} else if (ret == -EPROBE_DEFER) {
		dev_dbg(dev, "Device match requests probe deferral\n");
		dev->can_match = true;
		driver_deferred_probe_add(dev);
		 
		return 0;
	} else if (ret < 0) {
		dev_dbg(dev, "Bus failed to match device: %d\n", ret);
		 
		return 0;
	}  

	if (driver_allows_async_probing(drv)) {
		 
		dev_dbg(dev, "probing driver %s asynchronously\n", drv->name);
		device_lock(dev);
		if (!dev->driver && !dev->p->async_driver) {
			get_device(dev);
			dev->p->async_driver = drv;
			async = true;
		}
		device_unlock(dev);
		if (async)
			async_schedule_dev(__driver_attach_async_helper, dev);
		return 0;
	}

	__device_driver_lock(dev, dev->parent);
	driver_probe_device(drv, dev);
	__device_driver_unlock(dev, dev->parent);

	return 0;
}

 
int driver_attach(struct device_driver *drv)
{
	return bus_for_each_dev(drv->bus, NULL, drv, __driver_attach);
}
EXPORT_SYMBOL_GPL(driver_attach);

 
static void __device_release_driver(struct device *dev, struct device *parent)
{
	struct device_driver *drv;

	drv = dev->driver;
	if (drv) {
		pm_runtime_get_sync(dev);

		while (device_links_busy(dev)) {
			__device_driver_unlock(dev, parent);

			device_links_unbind_consumers(dev);

			__device_driver_lock(dev, parent);
			 
			if (dev->driver != drv) {
				pm_runtime_put(dev);
				return;
			}
		}

		driver_sysfs_remove(dev);

		bus_notify(dev, BUS_NOTIFY_UNBIND_DRIVER);

		pm_runtime_put_sync(dev);

		device_remove(dev);

		if (dev->bus && dev->bus->dma_cleanup)
			dev->bus->dma_cleanup(dev);

		device_unbind_cleanup(dev);
		device_links_driver_cleanup(dev);

		klist_remove(&dev->p->knode_driver);
		device_pm_check_callbacks(dev);

		bus_notify(dev, BUS_NOTIFY_UNBOUND_DRIVER);
		kobject_uevent(&dev->kobj, KOBJ_UNBIND);
	}
}

void device_release_driver_internal(struct device *dev,
				    struct device_driver *drv,
				    struct device *parent)
{
	__device_driver_lock(dev, parent);

	if (!drv || drv == dev->driver)
		__device_release_driver(dev, parent);

	__device_driver_unlock(dev, parent);
}

 
void device_release_driver(struct device *dev)
{
	 
	device_release_driver_internal(dev, NULL, NULL);
}
EXPORT_SYMBOL_GPL(device_release_driver);

 
void device_driver_detach(struct device *dev)
{
	device_release_driver_internal(dev, NULL, dev->parent);
}

 
void driver_detach(struct device_driver *drv)
{
	struct device_private *dev_prv;
	struct device *dev;

	if (driver_allows_async_probing(drv))
		async_synchronize_full();

	for (;;) {
		spin_lock(&drv->p->klist_devices.k_lock);
		if (list_empty(&drv->p->klist_devices.k_list)) {
			spin_unlock(&drv->p->klist_devices.k_lock);
			break;
		}
		dev_prv = list_last_entry(&drv->p->klist_devices.k_list,
				     struct device_private,
				     knode_driver.n_node);
		dev = dev_prv->device;
		get_device(dev);
		spin_unlock(&drv->p->klist_devices.k_lock);
		device_release_driver_internal(dev, drv, dev->parent);
		put_device(dev);
	}
}
