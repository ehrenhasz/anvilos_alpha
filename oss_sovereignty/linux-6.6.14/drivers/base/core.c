
 

#include <linux/acpi.h>
#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fwnode.h>
#include <linux/init.h>
#include <linux/kstrtox.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kdev_t.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/blkdev.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/netdevice.h>
#include <linux/sched/signal.h>
#include <linux/sched/mm.h>
#include <linux/string_helpers.h>
#include <linux/swiotlb.h>
#include <linux/sysfs.h>
#include <linux/dma-map-ops.h>  

#include "base.h"
#include "physical_location.h"
#include "power/power.h"

 
static LIST_HEAD(deferred_sync);
static unsigned int defer_sync_state_count = 1;
static DEFINE_MUTEX(fwnode_link_lock);
static bool fw_devlink_is_permissive(void);
static void __fw_devlink_link_to_consumers(struct device *dev);
static bool fw_devlink_drv_reg_done;
static bool fw_devlink_best_effort;

 
static int __fwnode_link_add(struct fwnode_handle *con,
			     struct fwnode_handle *sup, u8 flags)
{
	struct fwnode_link *link;

	list_for_each_entry(link, &sup->consumers, s_hook)
		if (link->consumer == con) {
			link->flags |= flags;
			return 0;
		}

	link = kzalloc(sizeof(*link), GFP_KERNEL);
	if (!link)
		return -ENOMEM;

	link->supplier = sup;
	INIT_LIST_HEAD(&link->s_hook);
	link->consumer = con;
	INIT_LIST_HEAD(&link->c_hook);
	link->flags = flags;

	list_add(&link->s_hook, &sup->consumers);
	list_add(&link->c_hook, &con->suppliers);
	pr_debug("%pfwf Linked as a fwnode consumer to %pfwf\n",
		 con, sup);

	return 0;
}

int fwnode_link_add(struct fwnode_handle *con, struct fwnode_handle *sup)
{
	int ret;

	mutex_lock(&fwnode_link_lock);
	ret = __fwnode_link_add(con, sup, 0);
	mutex_unlock(&fwnode_link_lock);
	return ret;
}

 
static void __fwnode_link_del(struct fwnode_link *link)
{
	pr_debug("%pfwf Dropping the fwnode link to %pfwf\n",
		 link->consumer, link->supplier);
	list_del(&link->s_hook);
	list_del(&link->c_hook);
	kfree(link);
}

 
static void __fwnode_link_cycle(struct fwnode_link *link)
{
	pr_debug("%pfwf: Relaxing link with %pfwf\n",
		 link->consumer, link->supplier);
	link->flags |= FWLINK_FLAG_CYCLE;
}

 
static void fwnode_links_purge_suppliers(struct fwnode_handle *fwnode)
{
	struct fwnode_link *link, *tmp;

	mutex_lock(&fwnode_link_lock);
	list_for_each_entry_safe(link, tmp, &fwnode->suppliers, c_hook)
		__fwnode_link_del(link);
	mutex_unlock(&fwnode_link_lock);
}

 
static void fwnode_links_purge_consumers(struct fwnode_handle *fwnode)
{
	struct fwnode_link *link, *tmp;

	mutex_lock(&fwnode_link_lock);
	list_for_each_entry_safe(link, tmp, &fwnode->consumers, s_hook)
		__fwnode_link_del(link);
	mutex_unlock(&fwnode_link_lock);
}

 
void fwnode_links_purge(struct fwnode_handle *fwnode)
{
	fwnode_links_purge_suppliers(fwnode);
	fwnode_links_purge_consumers(fwnode);
}

void fw_devlink_purge_absent_suppliers(struct fwnode_handle *fwnode)
{
	struct fwnode_handle *child;

	 
	if (fwnode->dev)
		return;

	fwnode->flags |= FWNODE_FLAG_NOT_DEVICE;
	fwnode_links_purge_consumers(fwnode);

	fwnode_for_each_available_child_node(fwnode, child)
		fw_devlink_purge_absent_suppliers(child);
}
EXPORT_SYMBOL_GPL(fw_devlink_purge_absent_suppliers);

 
static void __fwnode_links_move_consumers(struct fwnode_handle *from,
					  struct fwnode_handle *to)
{
	struct fwnode_link *link, *tmp;

	list_for_each_entry_safe(link, tmp, &from->consumers, s_hook) {
		__fwnode_link_add(link->consumer, to, link->flags);
		__fwnode_link_del(link);
	}
}

 
static void __fw_devlink_pickup_dangling_consumers(struct fwnode_handle *fwnode,
						   struct fwnode_handle *new_sup)
{
	struct fwnode_handle *child;

	if (fwnode->dev && fwnode->dev->bus)
		return;

	fwnode->flags |= FWNODE_FLAG_NOT_DEVICE;
	__fwnode_links_move_consumers(fwnode, new_sup);

	fwnode_for_each_available_child_node(fwnode, child)
		__fw_devlink_pickup_dangling_consumers(child, new_sup);
}

static DEFINE_MUTEX(device_links_lock);
DEFINE_STATIC_SRCU(device_links_srcu);

static inline void device_links_write_lock(void)
{
	mutex_lock(&device_links_lock);
}

static inline void device_links_write_unlock(void)
{
	mutex_unlock(&device_links_lock);
}

int device_links_read_lock(void) __acquires(&device_links_srcu)
{
	return srcu_read_lock(&device_links_srcu);
}

void device_links_read_unlock(int idx) __releases(&device_links_srcu)
{
	srcu_read_unlock(&device_links_srcu, idx);
}

int device_links_read_lock_held(void)
{
	return srcu_read_lock_held(&device_links_srcu);
}

static void device_link_synchronize_removal(void)
{
	synchronize_srcu(&device_links_srcu);
}

static void device_link_remove_from_lists(struct device_link *link)
{
	list_del_rcu(&link->s_node);
	list_del_rcu(&link->c_node);
}

static bool device_is_ancestor(struct device *dev, struct device *target)
{
	while (target->parent) {
		target = target->parent;
		if (dev == target)
			return true;
	}
	return false;
}

static inline bool device_link_flag_is_sync_state_only(u32 flags)
{
	return (flags & ~(DL_FLAG_INFERRED | DL_FLAG_CYCLE)) ==
		(DL_FLAG_SYNC_STATE_ONLY | DL_FLAG_MANAGED);
}

 
int device_is_dependent(struct device *dev, void *target)
{
	struct device_link *link;
	int ret;

	 
	if (dev == target || device_is_ancestor(dev, target))
		return 1;

	ret = device_for_each_child(dev, target, device_is_dependent);
	if (ret)
		return ret;

	list_for_each_entry(link, &dev->links.consumers, s_node) {
		if (device_link_flag_is_sync_state_only(link->flags))
			continue;

		if (link->consumer == target)
			return 1;

		ret = device_is_dependent(link->consumer, target);
		if (ret)
			break;
	}
	return ret;
}

static void device_link_init_status(struct device_link *link,
				    struct device *consumer,
				    struct device *supplier)
{
	switch (supplier->links.status) {
	case DL_DEV_PROBING:
		switch (consumer->links.status) {
		case DL_DEV_PROBING:
			 
			link->status = DL_STATE_CONSUMER_PROBE;
			break;
		default:
			link->status = DL_STATE_DORMANT;
			break;
		}
		break;
	case DL_DEV_DRIVER_BOUND:
		switch (consumer->links.status) {
		case DL_DEV_PROBING:
			link->status = DL_STATE_CONSUMER_PROBE;
			break;
		case DL_DEV_DRIVER_BOUND:
			link->status = DL_STATE_ACTIVE;
			break;
		default:
			link->status = DL_STATE_AVAILABLE;
			break;
		}
		break;
	case DL_DEV_UNBINDING:
		link->status = DL_STATE_SUPPLIER_UNBIND;
		break;
	default:
		link->status = DL_STATE_DORMANT;
		break;
	}
}

static int device_reorder_to_tail(struct device *dev, void *not_used)
{
	struct device_link *link;

	 
	if (device_is_registered(dev))
		devices_kset_move_last(dev);

	if (device_pm_initialized(dev))
		device_pm_move_last(dev);

	device_for_each_child(dev, NULL, device_reorder_to_tail);
	list_for_each_entry(link, &dev->links.consumers, s_node) {
		if (device_link_flag_is_sync_state_only(link->flags))
			continue;
		device_reorder_to_tail(link->consumer, NULL);
	}

	return 0;
}

 
void device_pm_move_to_tail(struct device *dev)
{
	int idx;

	idx = device_links_read_lock();
	device_pm_lock();
	device_reorder_to_tail(dev, NULL);
	device_pm_unlock();
	device_links_read_unlock(idx);
}

#define to_devlink(dev)	container_of((dev), struct device_link, link_dev)

static ssize_t status_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	const char *output;

	switch (to_devlink(dev)->status) {
	case DL_STATE_NONE:
		output = "not tracked";
		break;
	case DL_STATE_DORMANT:
		output = "dormant";
		break;
	case DL_STATE_AVAILABLE:
		output = "available";
		break;
	case DL_STATE_CONSUMER_PROBE:
		output = "consumer probing";
		break;
	case DL_STATE_ACTIVE:
		output = "active";
		break;
	case DL_STATE_SUPPLIER_UNBIND:
		output = "supplier unbinding";
		break;
	default:
		output = "unknown";
		break;
	}

	return sysfs_emit(buf, "%s\n", output);
}
static DEVICE_ATTR_RO(status);

static ssize_t auto_remove_on_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct device_link *link = to_devlink(dev);
	const char *output;

	if (link->flags & DL_FLAG_AUTOREMOVE_SUPPLIER)
		output = "supplier unbind";
	else if (link->flags & DL_FLAG_AUTOREMOVE_CONSUMER)
		output = "consumer unbind";
	else
		output = "never";

	return sysfs_emit(buf, "%s\n", output);
}
static DEVICE_ATTR_RO(auto_remove_on);

static ssize_t runtime_pm_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct device_link *link = to_devlink(dev);

	return sysfs_emit(buf, "%d\n", !!(link->flags & DL_FLAG_PM_RUNTIME));
}
static DEVICE_ATTR_RO(runtime_pm);

static ssize_t sync_state_only_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct device_link *link = to_devlink(dev);

	return sysfs_emit(buf, "%d\n",
			  !!(link->flags & DL_FLAG_SYNC_STATE_ONLY));
}
static DEVICE_ATTR_RO(sync_state_only);

static struct attribute *devlink_attrs[] = {
	&dev_attr_status.attr,
	&dev_attr_auto_remove_on.attr,
	&dev_attr_runtime_pm.attr,
	&dev_attr_sync_state_only.attr,
	NULL,
};
ATTRIBUTE_GROUPS(devlink);

static void device_link_release_fn(struct work_struct *work)
{
	struct device_link *link = container_of(work, struct device_link, rm_work);

	 
	device_link_synchronize_removal();

	pm_runtime_release_supplier(link);
	 
	if (link->supplier_preactivated)
		pm_runtime_put_noidle(link->supplier);

	pm_request_idle(link->supplier);

	put_device(link->consumer);
	put_device(link->supplier);
	kfree(link);
}

static void devlink_dev_release(struct device *dev)
{
	struct device_link *link = to_devlink(dev);

	INIT_WORK(&link->rm_work, device_link_release_fn);
	 
	queue_work(system_long_wq, &link->rm_work);
}

static struct class devlink_class = {
	.name = "devlink",
	.dev_groups = devlink_groups,
	.dev_release = devlink_dev_release,
};

static int devlink_add_symlinks(struct device *dev)
{
	int ret;
	size_t len;
	struct device_link *link = to_devlink(dev);
	struct device *sup = link->supplier;
	struct device *con = link->consumer;
	char *buf;

	len = max(strlen(dev_bus_name(sup)) + strlen(dev_name(sup)),
		  strlen(dev_bus_name(con)) + strlen(dev_name(con)));
	len += strlen(":");
	len += strlen("supplier:") + 1;
	buf = kzalloc(len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = sysfs_create_link(&link->link_dev.kobj, &sup->kobj, "supplier");
	if (ret)
		goto out;

	ret = sysfs_create_link(&link->link_dev.kobj, &con->kobj, "consumer");
	if (ret)
		goto err_con;

	snprintf(buf, len, "consumer:%s:%s", dev_bus_name(con), dev_name(con));
	ret = sysfs_create_link(&sup->kobj, &link->link_dev.kobj, buf);
	if (ret)
		goto err_con_dev;

	snprintf(buf, len, "supplier:%s:%s", dev_bus_name(sup), dev_name(sup));
	ret = sysfs_create_link(&con->kobj, &link->link_dev.kobj, buf);
	if (ret)
		goto err_sup_dev;

	goto out;

err_sup_dev:
	snprintf(buf, len, "consumer:%s:%s", dev_bus_name(con), dev_name(con));
	sysfs_remove_link(&sup->kobj, buf);
err_con_dev:
	sysfs_remove_link(&link->link_dev.kobj, "consumer");
err_con:
	sysfs_remove_link(&link->link_dev.kobj, "supplier");
out:
	kfree(buf);
	return ret;
}

static void devlink_remove_symlinks(struct device *dev)
{
	struct device_link *link = to_devlink(dev);
	size_t len;
	struct device *sup = link->supplier;
	struct device *con = link->consumer;
	char *buf;

	sysfs_remove_link(&link->link_dev.kobj, "consumer");
	sysfs_remove_link(&link->link_dev.kobj, "supplier");

	len = max(strlen(dev_bus_name(sup)) + strlen(dev_name(sup)),
		  strlen(dev_bus_name(con)) + strlen(dev_name(con)));
	len += strlen(":");
	len += strlen("supplier:") + 1;
	buf = kzalloc(len, GFP_KERNEL);
	if (!buf) {
		WARN(1, "Unable to properly free device link symlinks!\n");
		return;
	}

	if (device_is_registered(con)) {
		snprintf(buf, len, "supplier:%s:%s", dev_bus_name(sup), dev_name(sup));
		sysfs_remove_link(&con->kobj, buf);
	}
	snprintf(buf, len, "consumer:%s:%s", dev_bus_name(con), dev_name(con));
	sysfs_remove_link(&sup->kobj, buf);
	kfree(buf);
}

static struct class_interface devlink_class_intf = {
	.class = &devlink_class,
	.add_dev = devlink_add_symlinks,
	.remove_dev = devlink_remove_symlinks,
};

static int __init devlink_class_init(void)
{
	int ret;

	ret = class_register(&devlink_class);
	if (ret)
		return ret;

	ret = class_interface_register(&devlink_class_intf);
	if (ret)
		class_unregister(&devlink_class);

	return ret;
}
postcore_initcall(devlink_class_init);

#define DL_MANAGED_LINK_FLAGS (DL_FLAG_AUTOREMOVE_CONSUMER | \
			       DL_FLAG_AUTOREMOVE_SUPPLIER | \
			       DL_FLAG_AUTOPROBE_CONSUMER  | \
			       DL_FLAG_SYNC_STATE_ONLY | \
			       DL_FLAG_INFERRED | \
			       DL_FLAG_CYCLE)

#define DL_ADD_VALID_FLAGS (DL_MANAGED_LINK_FLAGS | DL_FLAG_STATELESS | \
			    DL_FLAG_PM_RUNTIME | DL_FLAG_RPM_ACTIVE)

 
struct device_link *device_link_add(struct device *consumer,
				    struct device *supplier, u32 flags)
{
	struct device_link *link;

	if (!consumer || !supplier || consumer == supplier ||
	    flags & ~DL_ADD_VALID_FLAGS ||
	    (flags & DL_FLAG_STATELESS && flags & DL_MANAGED_LINK_FLAGS) ||
	    (flags & DL_FLAG_AUTOPROBE_CONSUMER &&
	     flags & (DL_FLAG_AUTOREMOVE_CONSUMER |
		      DL_FLAG_AUTOREMOVE_SUPPLIER)))
		return NULL;

	if (flags & DL_FLAG_PM_RUNTIME && flags & DL_FLAG_RPM_ACTIVE) {
		if (pm_runtime_get_sync(supplier) < 0) {
			pm_runtime_put_noidle(supplier);
			return NULL;
		}
	}

	if (!(flags & DL_FLAG_STATELESS))
		flags |= DL_FLAG_MANAGED;

	if (flags & DL_FLAG_SYNC_STATE_ONLY &&
	    !device_link_flag_is_sync_state_only(flags))
		return NULL;

	device_links_write_lock();
	device_pm_lock();

	 
	if (!device_pm_initialized(supplier)
	    || (!(flags & DL_FLAG_SYNC_STATE_ONLY) &&
		  device_is_dependent(consumer, supplier))) {
		link = NULL;
		goto out;
	}

	 
	if (flags & DL_FLAG_SYNC_STATE_ONLY &&
	    consumer->links.status != DL_DEV_NO_DRIVER &&
	    consumer->links.status != DL_DEV_PROBING) {
		link = NULL;
		goto out;
	}

	 
	if (flags & DL_FLAG_AUTOREMOVE_SUPPLIER)
		flags &= ~DL_FLAG_AUTOREMOVE_CONSUMER;

	list_for_each_entry(link, &supplier->links.consumers, s_node) {
		if (link->consumer != consumer)
			continue;

		if (link->flags & DL_FLAG_INFERRED &&
		    !(flags & DL_FLAG_INFERRED))
			link->flags &= ~DL_FLAG_INFERRED;

		if (flags & DL_FLAG_PM_RUNTIME) {
			if (!(link->flags & DL_FLAG_PM_RUNTIME)) {
				pm_runtime_new_link(consumer);
				link->flags |= DL_FLAG_PM_RUNTIME;
			}
			if (flags & DL_FLAG_RPM_ACTIVE)
				refcount_inc(&link->rpm_active);
		}

		if (flags & DL_FLAG_STATELESS) {
			kref_get(&link->kref);
			if (link->flags & DL_FLAG_SYNC_STATE_ONLY &&
			    !(link->flags & DL_FLAG_STATELESS)) {
				link->flags |= DL_FLAG_STATELESS;
				goto reorder;
			} else {
				link->flags |= DL_FLAG_STATELESS;
				goto out;
			}
		}

		 
		if (flags & DL_FLAG_AUTOREMOVE_SUPPLIER) {
			if (link->flags & DL_FLAG_AUTOREMOVE_CONSUMER) {
				link->flags &= ~DL_FLAG_AUTOREMOVE_CONSUMER;
				link->flags |= DL_FLAG_AUTOREMOVE_SUPPLIER;
			}
		} else if (!(flags & DL_FLAG_AUTOREMOVE_CONSUMER)) {
			link->flags &= ~(DL_FLAG_AUTOREMOVE_CONSUMER |
					 DL_FLAG_AUTOREMOVE_SUPPLIER);
		}
		if (!(link->flags & DL_FLAG_MANAGED)) {
			kref_get(&link->kref);
			link->flags |= DL_FLAG_MANAGED;
			device_link_init_status(link, consumer, supplier);
		}
		if (link->flags & DL_FLAG_SYNC_STATE_ONLY &&
		    !(flags & DL_FLAG_SYNC_STATE_ONLY)) {
			link->flags &= ~DL_FLAG_SYNC_STATE_ONLY;
			goto reorder;
		}

		goto out;
	}

	link = kzalloc(sizeof(*link), GFP_KERNEL);
	if (!link)
		goto out;

	refcount_set(&link->rpm_active, 1);

	get_device(supplier);
	link->supplier = supplier;
	INIT_LIST_HEAD(&link->s_node);
	get_device(consumer);
	link->consumer = consumer;
	INIT_LIST_HEAD(&link->c_node);
	link->flags = flags;
	kref_init(&link->kref);

	link->link_dev.class = &devlink_class;
	device_set_pm_not_required(&link->link_dev);
	dev_set_name(&link->link_dev, "%s:%s--%s:%s",
		     dev_bus_name(supplier), dev_name(supplier),
		     dev_bus_name(consumer), dev_name(consumer));
	if (device_register(&link->link_dev)) {
		put_device(&link->link_dev);
		link = NULL;
		goto out;
	}

	if (flags & DL_FLAG_PM_RUNTIME) {
		if (flags & DL_FLAG_RPM_ACTIVE)
			refcount_inc(&link->rpm_active);

		pm_runtime_new_link(consumer);
	}

	 
	if (flags & DL_FLAG_STATELESS)
		link->status = DL_STATE_NONE;
	else
		device_link_init_status(link, consumer, supplier);

	 
	if (link->status == DL_STATE_CONSUMER_PROBE &&
	    flags & DL_FLAG_PM_RUNTIME)
		pm_runtime_resume(supplier);

	list_add_tail_rcu(&link->s_node, &supplier->links.consumers);
	list_add_tail_rcu(&link->c_node, &consumer->links.suppliers);

	if (flags & DL_FLAG_SYNC_STATE_ONLY) {
		dev_dbg(consumer,
			"Linked as a sync state only consumer to %s\n",
			dev_name(supplier));
		goto out;
	}

reorder:
	 
	device_reorder_to_tail(consumer, NULL);

	dev_dbg(consumer, "Linked as a consumer to %s\n", dev_name(supplier));

out:
	device_pm_unlock();
	device_links_write_unlock();

	if ((flags & DL_FLAG_PM_RUNTIME && flags & DL_FLAG_RPM_ACTIVE) && !link)
		pm_runtime_put(supplier);

	return link;
}
EXPORT_SYMBOL_GPL(device_link_add);

static void __device_link_del(struct kref *kref)
{
	struct device_link *link = container_of(kref, struct device_link, kref);

	dev_dbg(link->consumer, "Dropping the link to %s\n",
		dev_name(link->supplier));

	pm_runtime_drop_link(link);

	device_link_remove_from_lists(link);
	device_unregister(&link->link_dev);
}

static void device_link_put_kref(struct device_link *link)
{
	if (link->flags & DL_FLAG_STATELESS)
		kref_put(&link->kref, __device_link_del);
	else if (!device_is_registered(link->consumer))
		__device_link_del(&link->kref);
	else
		WARN(1, "Unable to drop a managed device link reference\n");
}

 
void device_link_del(struct device_link *link)
{
	device_links_write_lock();
	device_link_put_kref(link);
	device_links_write_unlock();
}
EXPORT_SYMBOL_GPL(device_link_del);

 
void device_link_remove(void *consumer, struct device *supplier)
{
	struct device_link *link;

	if (WARN_ON(consumer == supplier))
		return;

	device_links_write_lock();

	list_for_each_entry(link, &supplier->links.consumers, s_node) {
		if (link->consumer == consumer) {
			device_link_put_kref(link);
			break;
		}
	}

	device_links_write_unlock();
}
EXPORT_SYMBOL_GPL(device_link_remove);

static void device_links_missing_supplier(struct device *dev)
{
	struct device_link *link;

	list_for_each_entry(link, &dev->links.suppliers, c_node) {
		if (link->status != DL_STATE_CONSUMER_PROBE)
			continue;

		if (link->supplier->links.status == DL_DEV_DRIVER_BOUND) {
			WRITE_ONCE(link->status, DL_STATE_AVAILABLE);
		} else {
			WARN_ON(!(link->flags & DL_FLAG_SYNC_STATE_ONLY));
			WRITE_ONCE(link->status, DL_STATE_DORMANT);
		}
	}
}

static bool dev_is_best_effort(struct device *dev)
{
	return (fw_devlink_best_effort && dev->can_match) ||
		(dev->fwnode && (dev->fwnode->flags & FWNODE_FLAG_BEST_EFFORT));
}

static struct fwnode_handle *fwnode_links_check_suppliers(
						struct fwnode_handle *fwnode)
{
	struct fwnode_link *link;

	if (!fwnode || fw_devlink_is_permissive())
		return NULL;

	list_for_each_entry(link, &fwnode->suppliers, c_hook)
		if (!(link->flags & FWLINK_FLAG_CYCLE))
			return link->supplier;

	return NULL;
}

 
int device_links_check_suppliers(struct device *dev)
{
	struct device_link *link;
	int ret = 0, fwnode_ret = 0;
	struct fwnode_handle *sup_fw;

	 
	mutex_lock(&fwnode_link_lock);
	sup_fw = fwnode_links_check_suppliers(dev->fwnode);
	if (sup_fw) {
		if (!dev_is_best_effort(dev)) {
			fwnode_ret = -EPROBE_DEFER;
			dev_err_probe(dev, -EPROBE_DEFER,
				    "wait for supplier %pfwf\n", sup_fw);
		} else {
			fwnode_ret = -EAGAIN;
		}
	}
	mutex_unlock(&fwnode_link_lock);
	if (fwnode_ret == -EPROBE_DEFER)
		return fwnode_ret;

	device_links_write_lock();

	list_for_each_entry(link, &dev->links.suppliers, c_node) {
		if (!(link->flags & DL_FLAG_MANAGED))
			continue;

		if (link->status != DL_STATE_AVAILABLE &&
		    !(link->flags & DL_FLAG_SYNC_STATE_ONLY)) {

			if (dev_is_best_effort(dev) &&
			    link->flags & DL_FLAG_INFERRED &&
			    !link->supplier->can_match) {
				ret = -EAGAIN;
				continue;
			}

			device_links_missing_supplier(dev);
			dev_err_probe(dev, -EPROBE_DEFER,
				      "supplier %s not ready\n",
				      dev_name(link->supplier));
			ret = -EPROBE_DEFER;
			break;
		}
		WRITE_ONCE(link->status, DL_STATE_CONSUMER_PROBE);
	}
	dev->links.status = DL_DEV_PROBING;

	device_links_write_unlock();

	return ret ? ret : fwnode_ret;
}

 
static void __device_links_queue_sync_state(struct device *dev,
					    struct list_head *list)
{
	struct device_link *link;

	if (!dev_has_sync_state(dev))
		return;
	if (dev->state_synced)
		return;

	list_for_each_entry(link, &dev->links.consumers, s_node) {
		if (!(link->flags & DL_FLAG_MANAGED))
			continue;
		if (link->status != DL_STATE_ACTIVE)
			return;
	}

	 
	dev->state_synced = true;

	if (WARN_ON(!list_empty(&dev->links.defer_sync)))
		return;

	get_device(dev);
	list_add_tail(&dev->links.defer_sync, list);
}

 
static void device_links_flush_sync_list(struct list_head *list,
					 struct device *dont_lock_dev)
{
	struct device *dev, *tmp;

	list_for_each_entry_safe(dev, tmp, list, links.defer_sync) {
		list_del_init(&dev->links.defer_sync);

		if (dev != dont_lock_dev)
			device_lock(dev);

		dev_sync_state(dev);

		if (dev != dont_lock_dev)
			device_unlock(dev);

		put_device(dev);
	}
}

void device_links_supplier_sync_state_pause(void)
{
	device_links_write_lock();
	defer_sync_state_count++;
	device_links_write_unlock();
}

void device_links_supplier_sync_state_resume(void)
{
	struct device *dev, *tmp;
	LIST_HEAD(sync_list);

	device_links_write_lock();
	if (!defer_sync_state_count) {
		WARN(true, "Unmatched sync_state pause/resume!");
		goto out;
	}
	defer_sync_state_count--;
	if (defer_sync_state_count)
		goto out;

	list_for_each_entry_safe(dev, tmp, &deferred_sync, links.defer_sync) {
		 
		list_del_init(&dev->links.defer_sync);
		__device_links_queue_sync_state(dev, &sync_list);
	}
out:
	device_links_write_unlock();

	device_links_flush_sync_list(&sync_list, NULL);
}

static int sync_state_resume_initcall(void)
{
	device_links_supplier_sync_state_resume();
	return 0;
}
late_initcall(sync_state_resume_initcall);

static void __device_links_supplier_defer_sync(struct device *sup)
{
	if (list_empty(&sup->links.defer_sync) && dev_has_sync_state(sup))
		list_add_tail(&sup->links.defer_sync, &deferred_sync);
}

static void device_link_drop_managed(struct device_link *link)
{
	link->flags &= ~DL_FLAG_MANAGED;
	WRITE_ONCE(link->status, DL_STATE_NONE);
	kref_put(&link->kref, __device_link_del);
}

static ssize_t waiting_for_supplier_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	bool val;

	device_lock(dev);
	mutex_lock(&fwnode_link_lock);
	val = !!fwnode_links_check_suppliers(dev->fwnode);
	mutex_unlock(&fwnode_link_lock);
	device_unlock(dev);
	return sysfs_emit(buf, "%u\n", val);
}
static DEVICE_ATTR_RO(waiting_for_supplier);

 
void device_links_force_bind(struct device *dev)
{
	struct device_link *link, *ln;

	device_links_write_lock();

	list_for_each_entry_safe(link, ln, &dev->links.suppliers, c_node) {
		if (!(link->flags & DL_FLAG_MANAGED))
			continue;

		if (link->status != DL_STATE_AVAILABLE) {
			device_link_drop_managed(link);
			continue;
		}
		WRITE_ONCE(link->status, DL_STATE_CONSUMER_PROBE);
	}
	dev->links.status = DL_DEV_PROBING;

	device_links_write_unlock();
}

 
void device_links_driver_bound(struct device *dev)
{
	struct device_link *link, *ln;
	LIST_HEAD(sync_list);

	 
	if (dev->fwnode && dev->fwnode->dev == dev) {
		struct fwnode_handle *child;
		fwnode_links_purge_suppliers(dev->fwnode);
		mutex_lock(&fwnode_link_lock);
		fwnode_for_each_available_child_node(dev->fwnode, child)
			__fw_devlink_pickup_dangling_consumers(child,
							       dev->fwnode);
		__fw_devlink_link_to_consumers(dev);
		mutex_unlock(&fwnode_link_lock);
	}
	device_remove_file(dev, &dev_attr_waiting_for_supplier);

	device_links_write_lock();

	list_for_each_entry(link, &dev->links.consumers, s_node) {
		if (!(link->flags & DL_FLAG_MANAGED))
			continue;

		 
		if (link->status == DL_STATE_CONSUMER_PROBE ||
		    link->status == DL_STATE_ACTIVE)
			continue;

		WARN_ON(link->status != DL_STATE_DORMANT);
		WRITE_ONCE(link->status, DL_STATE_AVAILABLE);

		if (link->flags & DL_FLAG_AUTOPROBE_CONSUMER)
			driver_deferred_probe_add(link->consumer);
	}

	if (defer_sync_state_count)
		__device_links_supplier_defer_sync(dev);
	else
		__device_links_queue_sync_state(dev, &sync_list);

	list_for_each_entry_safe(link, ln, &dev->links.suppliers, c_node) {
		struct device *supplier;

		if (!(link->flags & DL_FLAG_MANAGED))
			continue;

		supplier = link->supplier;
		if (link->flags & DL_FLAG_SYNC_STATE_ONLY) {
			 
			device_link_drop_managed(link);
		} else if (dev_is_best_effort(dev) &&
			   link->flags & DL_FLAG_INFERRED &&
			   link->status != DL_STATE_CONSUMER_PROBE &&
			   !link->supplier->can_match) {
			 
			device_link_drop_managed(link);
		} else {
			WARN_ON(link->status != DL_STATE_CONSUMER_PROBE);
			WRITE_ONCE(link->status, DL_STATE_ACTIVE);
		}

		 
		if (defer_sync_state_count)
			__device_links_supplier_defer_sync(supplier);
		else
			__device_links_queue_sync_state(supplier, &sync_list);
	}

	dev->links.status = DL_DEV_DRIVER_BOUND;

	device_links_write_unlock();

	device_links_flush_sync_list(&sync_list, dev);
}

 
static void __device_links_no_driver(struct device *dev)
{
	struct device_link *link, *ln;

	list_for_each_entry_safe_reverse(link, ln, &dev->links.suppliers, c_node) {
		if (!(link->flags & DL_FLAG_MANAGED))
			continue;

		if (link->flags & DL_FLAG_AUTOREMOVE_CONSUMER) {
			device_link_drop_managed(link);
			continue;
		}

		if (link->status != DL_STATE_CONSUMER_PROBE &&
		    link->status != DL_STATE_ACTIVE)
			continue;

		if (link->supplier->links.status == DL_DEV_DRIVER_BOUND) {
			WRITE_ONCE(link->status, DL_STATE_AVAILABLE);
		} else {
			WARN_ON(!(link->flags & DL_FLAG_SYNC_STATE_ONLY));
			WRITE_ONCE(link->status, DL_STATE_DORMANT);
		}
	}

	dev->links.status = DL_DEV_NO_DRIVER;
}

 
void device_links_no_driver(struct device *dev)
{
	struct device_link *link;

	device_links_write_lock();

	list_for_each_entry(link, &dev->links.consumers, s_node) {
		if (!(link->flags & DL_FLAG_MANAGED))
			continue;

		 
		if (link->status == DL_STATE_CONSUMER_PROBE ||
		    link->status == DL_STATE_ACTIVE)
			WRITE_ONCE(link->status, DL_STATE_DORMANT);
	}

	__device_links_no_driver(dev);

	device_links_write_unlock();
}

 
void device_links_driver_cleanup(struct device *dev)
{
	struct device_link *link, *ln;

	device_links_write_lock();

	list_for_each_entry_safe(link, ln, &dev->links.consumers, s_node) {
		if (!(link->flags & DL_FLAG_MANAGED))
			continue;

		WARN_ON(link->flags & DL_FLAG_AUTOREMOVE_CONSUMER);
		WARN_ON(link->status != DL_STATE_SUPPLIER_UNBIND);

		 
		if (link->status == DL_STATE_SUPPLIER_UNBIND &&
		    link->flags & DL_FLAG_AUTOREMOVE_SUPPLIER)
			device_link_drop_managed(link);

		WRITE_ONCE(link->status, DL_STATE_DORMANT);
	}

	list_del_init(&dev->links.defer_sync);
	__device_links_no_driver(dev);

	device_links_write_unlock();
}

 
bool device_links_busy(struct device *dev)
{
	struct device_link *link;
	bool ret = false;

	device_links_write_lock();

	list_for_each_entry(link, &dev->links.consumers, s_node) {
		if (!(link->flags & DL_FLAG_MANAGED))
			continue;

		if (link->status == DL_STATE_CONSUMER_PROBE
		    || link->status == DL_STATE_ACTIVE) {
			ret = true;
			break;
		}
		WRITE_ONCE(link->status, DL_STATE_SUPPLIER_UNBIND);
	}

	dev->links.status = DL_DEV_UNBINDING;

	device_links_write_unlock();
	return ret;
}

 
void device_links_unbind_consumers(struct device *dev)
{
	struct device_link *link;

 start:
	device_links_write_lock();

	list_for_each_entry(link, &dev->links.consumers, s_node) {
		enum device_link_state status;

		if (!(link->flags & DL_FLAG_MANAGED) ||
		    link->flags & DL_FLAG_SYNC_STATE_ONLY)
			continue;

		status = link->status;
		if (status == DL_STATE_CONSUMER_PROBE) {
			device_links_write_unlock();

			wait_for_device_probe();
			goto start;
		}
		WRITE_ONCE(link->status, DL_STATE_SUPPLIER_UNBIND);
		if (status == DL_STATE_ACTIVE) {
			struct device *consumer = link->consumer;

			get_device(consumer);

			device_links_write_unlock();

			device_release_driver_internal(consumer, NULL,
						       consumer->parent);
			put_device(consumer);
			goto start;
		}
	}

	device_links_write_unlock();
}

 
static void device_links_purge(struct device *dev)
{
	struct device_link *link, *ln;

	if (dev->class == &devlink_class)
		return;

	 
	device_links_write_lock();

	list_for_each_entry_safe_reverse(link, ln, &dev->links.suppliers, c_node) {
		WARN_ON(link->status == DL_STATE_ACTIVE);
		__device_link_del(&link->kref);
	}

	list_for_each_entry_safe_reverse(link, ln, &dev->links.consumers, s_node) {
		WARN_ON(link->status != DL_STATE_DORMANT &&
			link->status != DL_STATE_NONE);
		__device_link_del(&link->kref);
	}

	device_links_write_unlock();
}

#define FW_DEVLINK_FLAGS_PERMISSIVE	(DL_FLAG_INFERRED | \
					 DL_FLAG_SYNC_STATE_ONLY)
#define FW_DEVLINK_FLAGS_ON		(DL_FLAG_INFERRED | \
					 DL_FLAG_AUTOPROBE_CONSUMER)
#define FW_DEVLINK_FLAGS_RPM		(FW_DEVLINK_FLAGS_ON | \
					 DL_FLAG_PM_RUNTIME)

static u32 fw_devlink_flags = FW_DEVLINK_FLAGS_ON;
static int __init fw_devlink_setup(char *arg)
{
	if (!arg)
		return -EINVAL;

	if (strcmp(arg, "off") == 0) {
		fw_devlink_flags = 0;
	} else if (strcmp(arg, "permissive") == 0) {
		fw_devlink_flags = FW_DEVLINK_FLAGS_PERMISSIVE;
	} else if (strcmp(arg, "on") == 0) {
		fw_devlink_flags = FW_DEVLINK_FLAGS_ON;
	} else if (strcmp(arg, "rpm") == 0) {
		fw_devlink_flags = FW_DEVLINK_FLAGS_RPM;
	}
	return 0;
}
early_param("fw_devlink", fw_devlink_setup);

static bool fw_devlink_strict;
static int __init fw_devlink_strict_setup(char *arg)
{
	return kstrtobool(arg, &fw_devlink_strict);
}
early_param("fw_devlink.strict", fw_devlink_strict_setup);

#define FW_DEVLINK_SYNC_STATE_STRICT	0
#define FW_DEVLINK_SYNC_STATE_TIMEOUT	1

#ifndef CONFIG_FW_DEVLINK_SYNC_STATE_TIMEOUT
static int fw_devlink_sync_state;
#else
static int fw_devlink_sync_state = FW_DEVLINK_SYNC_STATE_TIMEOUT;
#endif

static int __init fw_devlink_sync_state_setup(char *arg)
{
	if (!arg)
		return -EINVAL;

	if (strcmp(arg, "strict") == 0) {
		fw_devlink_sync_state = FW_DEVLINK_SYNC_STATE_STRICT;
		return 0;
	} else if (strcmp(arg, "timeout") == 0) {
		fw_devlink_sync_state = FW_DEVLINK_SYNC_STATE_TIMEOUT;
		return 0;
	}
	return -EINVAL;
}
early_param("fw_devlink.sync_state", fw_devlink_sync_state_setup);

static inline u32 fw_devlink_get_flags(u8 fwlink_flags)
{
	if (fwlink_flags & FWLINK_FLAG_CYCLE)
		return FW_DEVLINK_FLAGS_PERMISSIVE | DL_FLAG_CYCLE;

	return fw_devlink_flags;
}

static bool fw_devlink_is_permissive(void)
{
	return fw_devlink_flags == FW_DEVLINK_FLAGS_PERMISSIVE;
}

bool fw_devlink_is_strict(void)
{
	return fw_devlink_strict && !fw_devlink_is_permissive();
}

static void fw_devlink_parse_fwnode(struct fwnode_handle *fwnode)
{
	if (fwnode->flags & FWNODE_FLAG_LINKS_ADDED)
		return;

	fwnode_call_int_op(fwnode, add_links);
	fwnode->flags |= FWNODE_FLAG_LINKS_ADDED;
}

static void fw_devlink_parse_fwtree(struct fwnode_handle *fwnode)
{
	struct fwnode_handle *child = NULL;

	fw_devlink_parse_fwnode(fwnode);

	while ((child = fwnode_get_next_available_child_node(fwnode, child)))
		fw_devlink_parse_fwtree(child);
}

static void fw_devlink_relax_link(struct device_link *link)
{
	if (!(link->flags & DL_FLAG_INFERRED))
		return;

	if (device_link_flag_is_sync_state_only(link->flags))
		return;

	pm_runtime_drop_link(link);
	link->flags = DL_FLAG_MANAGED | FW_DEVLINK_FLAGS_PERMISSIVE;
	dev_dbg(link->consumer, "Relaxing link with %s\n",
		dev_name(link->supplier));
}

static int fw_devlink_no_driver(struct device *dev, void *data)
{
	struct device_link *link = to_devlink(dev);

	if (!link->supplier->can_match)
		fw_devlink_relax_link(link);

	return 0;
}

void fw_devlink_drivers_done(void)
{
	fw_devlink_drv_reg_done = true;
	device_links_write_lock();
	class_for_each_device(&devlink_class, NULL, NULL,
			      fw_devlink_no_driver);
	device_links_write_unlock();
}

static int fw_devlink_dev_sync_state(struct device *dev, void *data)
{
	struct device_link *link = to_devlink(dev);
	struct device *sup = link->supplier;

	if (!(link->flags & DL_FLAG_MANAGED) ||
	    link->status == DL_STATE_ACTIVE || sup->state_synced ||
	    !dev_has_sync_state(sup))
		return 0;

	if (fw_devlink_sync_state == FW_DEVLINK_SYNC_STATE_STRICT) {
		dev_warn(sup, "sync_state() pending due to %s\n",
			 dev_name(link->consumer));
		return 0;
	}

	if (!list_empty(&sup->links.defer_sync))
		return 0;

	dev_warn(sup, "Timed out. Forcing sync_state()\n");
	sup->state_synced = true;
	get_device(sup);
	list_add_tail(&sup->links.defer_sync, data);

	return 0;
}

void fw_devlink_probing_done(void)
{
	LIST_HEAD(sync_list);

	device_links_write_lock();
	class_for_each_device(&devlink_class, NULL, &sync_list,
			      fw_devlink_dev_sync_state);
	device_links_write_unlock();
	device_links_flush_sync_list(&sync_list, NULL);
}

 
void __init wait_for_init_devices_probe(void)
{
	if (!fw_devlink_flags || fw_devlink_is_permissive())
		return;

	 
	wait_for_device_probe();

	pr_info("Trying to probe devices needed for running init ...\n");
	fw_devlink_best_effort = true;
	driver_deferred_probe_trigger();

	 
	wait_for_device_probe();
	fw_devlink_best_effort = false;
}

static void fw_devlink_unblock_consumers(struct device *dev)
{
	struct device_link *link;

	if (!fw_devlink_flags || fw_devlink_is_permissive())
		return;

	device_links_write_lock();
	list_for_each_entry(link, &dev->links.consumers, s_node)
		fw_devlink_relax_link(link);
	device_links_write_unlock();
}


static bool fwnode_init_without_drv(struct fwnode_handle *fwnode)
{
	struct device *dev;
	bool ret;

	if (!(fwnode->flags & FWNODE_FLAG_INITIALIZED))
		return false;

	dev = get_dev_from_fwnode(fwnode);
	ret = !dev || dev->links.status == DL_DEV_NO_DRIVER;
	put_device(dev);

	return ret;
}

static bool fwnode_ancestor_init_without_drv(struct fwnode_handle *fwnode)
{
	struct fwnode_handle *parent;

	fwnode_for_each_parent_node(fwnode, parent) {
		if (fwnode_init_without_drv(parent)) {
			fwnode_handle_put(parent);
			return true;
		}
	}

	return false;
}

 
static bool __fw_devlink_relax_cycles(struct device *con,
				 struct fwnode_handle *sup_handle)
{
	struct device *sup_dev = NULL, *par_dev = NULL;
	struct fwnode_link *link;
	struct device_link *dev_link;
	bool ret = false;

	if (!sup_handle)
		return false;

	 
	if (sup_handle->flags & FWNODE_FLAG_VISITED)
		return false;

	sup_handle->flags |= FWNODE_FLAG_VISITED;

	sup_dev = get_dev_from_fwnode(sup_handle);

	 
	if (sup_dev == con) {
		ret = true;
		goto out;
	}

	 
	if (sup_dev && sup_dev->links.status ==  DL_DEV_DRIVER_BOUND &&
	    con->links.status == DL_DEV_NO_DRIVER) {
		ret = false;
		goto out;
	}

	list_for_each_entry(link, &sup_handle->suppliers, c_hook) {
		if (__fw_devlink_relax_cycles(con, link->supplier)) {
			__fwnode_link_cycle(link);
			ret = true;
		}
	}

	 
	if (sup_dev)
		par_dev = get_device(sup_dev->parent);
	else
		par_dev = fwnode_get_next_parent_dev(sup_handle);

	if (par_dev && __fw_devlink_relax_cycles(con, par_dev->fwnode))
		ret = true;

	if (!sup_dev)
		goto out;

	list_for_each_entry(dev_link, &sup_dev->links.suppliers, c_node) {
		 
		if (device_link_flag_is_sync_state_only(dev_link->flags) &&
		    !(dev_link->flags & DL_FLAG_CYCLE))
			continue;

		if (__fw_devlink_relax_cycles(con,
					      dev_link->supplier->fwnode)) {
			fw_devlink_relax_link(dev_link);
			dev_link->flags |= DL_FLAG_CYCLE;
			ret = true;
		}
	}

out:
	sup_handle->flags &= ~FWNODE_FLAG_VISITED;
	put_device(sup_dev);
	put_device(par_dev);
	return ret;
}

 
static int fw_devlink_create_devlink(struct device *con,
				     struct fwnode_handle *sup_handle,
				     struct fwnode_link *link)
{
	struct device *sup_dev;
	int ret = 0;
	u32 flags;

	if (con->fwnode == link->consumer)
		flags = fw_devlink_get_flags(link->flags);
	else
		flags = FW_DEVLINK_FLAGS_PERMISSIVE;

	 
	if (sup_handle->flags & FWNODE_FLAG_NEEDS_CHILD_BOUND_ON_ADD &&
	    fwnode_is_ancestor_of(sup_handle, con->fwnode))
		return -EINVAL;

	 
	if (!(flags & DL_FLAG_SYNC_STATE_ONLY)) {
		device_links_write_lock();
		if (__fw_devlink_relax_cycles(con, sup_handle)) {
			__fwnode_link_cycle(link);
			flags = fw_devlink_get_flags(link->flags);
			dev_info(con, "Fixed dependency cycle(s) with %pfwf\n",
				 sup_handle);
		}
		device_links_write_unlock();
	}

	if (sup_handle->flags & FWNODE_FLAG_NOT_DEVICE)
		sup_dev = fwnode_get_next_parent_dev(sup_handle);
	else
		sup_dev = get_dev_from_fwnode(sup_handle);

	if (sup_dev) {
		 
		if (sup_dev->links.status == DL_DEV_NO_DRIVER &&
		    sup_handle->flags & FWNODE_FLAG_INITIALIZED) {
			dev_dbg(con,
				"Not linking %pfwf - dev might never probe\n",
				sup_handle);
			ret = -EINVAL;
			goto out;
		}

		if (con != sup_dev && !device_link_add(con, sup_dev, flags)) {
			dev_err(con, "Failed to create device link (0x%x) with %s\n",
				flags, dev_name(sup_dev));
			ret = -EINVAL;
		}

		goto out;
	}

	 
	if (fwnode_init_without_drv(sup_handle) ||
	    fwnode_ancestor_init_without_drv(sup_handle)) {
		dev_dbg(con, "Not linking %pfwf - might never become dev\n",
			sup_handle);
		return -EINVAL;
	}

	ret = -EAGAIN;
out:
	put_device(sup_dev);
	return ret;
}

 
static void __fw_devlink_link_to_consumers(struct device *dev)
{
	struct fwnode_handle *fwnode = dev->fwnode;
	struct fwnode_link *link, *tmp;

	list_for_each_entry_safe(link, tmp, &fwnode->consumers, s_hook) {
		struct device *con_dev;
		bool own_link = true;
		int ret;

		con_dev = get_dev_from_fwnode(link->consumer);
		 
		if (!con_dev) {
			con_dev = fwnode_get_next_parent_dev(link->consumer);
			 
			if (con_dev &&
			    fwnode_is_ancestor_of(con_dev->fwnode, fwnode)) {
				put_device(con_dev);
				con_dev = NULL;
			} else {
				own_link = false;
			}
		}

		if (!con_dev)
			continue;

		ret = fw_devlink_create_devlink(con_dev, fwnode, link);
		put_device(con_dev);
		if (!own_link || ret == -EAGAIN)
			continue;

		__fwnode_link_del(link);
	}
}

 
static void __fw_devlink_link_to_suppliers(struct device *dev,
					   struct fwnode_handle *fwnode)
{
	bool own_link = (dev->fwnode == fwnode);
	struct fwnode_link *link, *tmp;
	struct fwnode_handle *child = NULL;

	list_for_each_entry_safe(link, tmp, &fwnode->suppliers, c_hook) {
		int ret;
		struct fwnode_handle *sup = link->supplier;

		ret = fw_devlink_create_devlink(dev, sup, link);
		if (!own_link || ret == -EAGAIN)
			continue;

		__fwnode_link_del(link);
	}

	 
	while ((child = fwnode_get_next_available_child_node(fwnode, child)))
		__fw_devlink_link_to_suppliers(dev, child);
}

static void fw_devlink_link_device(struct device *dev)
{
	struct fwnode_handle *fwnode = dev->fwnode;

	if (!fw_devlink_flags)
		return;

	fw_devlink_parse_fwtree(fwnode);

	mutex_lock(&fwnode_link_lock);
	__fw_devlink_link_to_consumers(dev);
	__fw_devlink_link_to_suppliers(dev, fwnode);
	mutex_unlock(&fwnode_link_lock);
}

 

int (*platform_notify)(struct device *dev) = NULL;
int (*platform_notify_remove)(struct device *dev) = NULL;
static struct kobject *dev_kobj;

 
static struct kobject *sysfs_dev_char_kobj;

 
static struct kobject *sysfs_dev_block_kobj;

static DEFINE_MUTEX(device_hotplug_lock);

void lock_device_hotplug(void)
{
	mutex_lock(&device_hotplug_lock);
}

void unlock_device_hotplug(void)
{
	mutex_unlock(&device_hotplug_lock);
}

int lock_device_hotplug_sysfs(void)
{
	if (mutex_trylock(&device_hotplug_lock))
		return 0;

	 
	msleep(5);
	return restart_syscall();
}

#ifdef CONFIG_BLOCK
static inline int device_is_not_partition(struct device *dev)
{
	return !(dev->type == &part_type);
}
#else
static inline int device_is_not_partition(struct device *dev)
{
	return 1;
}
#endif

static void device_platform_notify(struct device *dev)
{
	acpi_device_notify(dev);

	software_node_notify(dev);

	if (platform_notify)
		platform_notify(dev);
}

static void device_platform_notify_remove(struct device *dev)
{
	if (platform_notify_remove)
		platform_notify_remove(dev);

	software_node_notify_remove(dev);

	acpi_device_notify_remove(dev);
}

 
const char *dev_driver_string(const struct device *dev)
{
	struct device_driver *drv;

	 
	drv = READ_ONCE(dev->driver);
	return drv ? drv->name : dev_bus_name(dev);
}
EXPORT_SYMBOL(dev_driver_string);

#define to_dev_attr(_attr) container_of(_attr, struct device_attribute, attr)

static ssize_t dev_attr_show(struct kobject *kobj, struct attribute *attr,
			     char *buf)
{
	struct device_attribute *dev_attr = to_dev_attr(attr);
	struct device *dev = kobj_to_dev(kobj);
	ssize_t ret = -EIO;

	if (dev_attr->show)
		ret = dev_attr->show(dev, dev_attr, buf);
	if (ret >= (ssize_t)PAGE_SIZE) {
		printk("dev_attr_show: %pS returned bad count\n",
				dev_attr->show);
	}
	return ret;
}

static ssize_t dev_attr_store(struct kobject *kobj, struct attribute *attr,
			      const char *buf, size_t count)
{
	struct device_attribute *dev_attr = to_dev_attr(attr);
	struct device *dev = kobj_to_dev(kobj);
	ssize_t ret = -EIO;

	if (dev_attr->store)
		ret = dev_attr->store(dev, dev_attr, buf, count);
	return ret;
}

static const struct sysfs_ops dev_sysfs_ops = {
	.show	= dev_attr_show,
	.store	= dev_attr_store,
};

#define to_ext_attr(x) container_of(x, struct dev_ext_attribute, attr)

ssize_t device_store_ulong(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	struct dev_ext_attribute *ea = to_ext_attr(attr);
	int ret;
	unsigned long new;

	ret = kstrtoul(buf, 0, &new);
	if (ret)
		return ret;
	*(unsigned long *)(ea->var) = new;
	 
	return size;
}
EXPORT_SYMBOL_GPL(device_store_ulong);

ssize_t device_show_ulong(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct dev_ext_attribute *ea = to_ext_attr(attr);
	return sysfs_emit(buf, "%lx\n", *(unsigned long *)(ea->var));
}
EXPORT_SYMBOL_GPL(device_show_ulong);

ssize_t device_store_int(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t size)
{
	struct dev_ext_attribute *ea = to_ext_attr(attr);
	int ret;
	long new;

	ret = kstrtol(buf, 0, &new);
	if (ret)
		return ret;

	if (new > INT_MAX || new < INT_MIN)
		return -EINVAL;
	*(int *)(ea->var) = new;
	 
	return size;
}
EXPORT_SYMBOL_GPL(device_store_int);

ssize_t device_show_int(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct dev_ext_attribute *ea = to_ext_attr(attr);

	return sysfs_emit(buf, "%d\n", *(int *)(ea->var));
}
EXPORT_SYMBOL_GPL(device_show_int);

ssize_t device_store_bool(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t size)
{
	struct dev_ext_attribute *ea = to_ext_attr(attr);

	if (kstrtobool(buf, ea->var) < 0)
		return -EINVAL;

	return size;
}
EXPORT_SYMBOL_GPL(device_store_bool);

ssize_t device_show_bool(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct dev_ext_attribute *ea = to_ext_attr(attr);

	return sysfs_emit(buf, "%d\n", *(bool *)(ea->var));
}
EXPORT_SYMBOL_GPL(device_show_bool);

 
static void device_release(struct kobject *kobj)
{
	struct device *dev = kobj_to_dev(kobj);
	struct device_private *p = dev->p;

	 
	devres_release_all(dev);

	kfree(dev->dma_range_map);

	if (dev->release)
		dev->release(dev);
	else if (dev->type && dev->type->release)
		dev->type->release(dev);
	else if (dev->class && dev->class->dev_release)
		dev->class->dev_release(dev);
	else
		WARN(1, KERN_ERR "Device '%s' does not have a release() function, it is broken and must be fixed. See Documentation/core-api/kobject.rst.\n",
			dev_name(dev));
	kfree(p);
}

static const void *device_namespace(const struct kobject *kobj)
{
	const struct device *dev = kobj_to_dev(kobj);
	const void *ns = NULL;

	if (dev->class && dev->class->ns_type)
		ns = dev->class->namespace(dev);

	return ns;
}

static void device_get_ownership(const struct kobject *kobj, kuid_t *uid, kgid_t *gid)
{
	const struct device *dev = kobj_to_dev(kobj);

	if (dev->class && dev->class->get_ownership)
		dev->class->get_ownership(dev, uid, gid);
}

static const struct kobj_type device_ktype = {
	.release	= device_release,
	.sysfs_ops	= &dev_sysfs_ops,
	.namespace	= device_namespace,
	.get_ownership	= device_get_ownership,
};


static int dev_uevent_filter(const struct kobject *kobj)
{
	const struct kobj_type *ktype = get_ktype(kobj);

	if (ktype == &device_ktype) {
		const struct device *dev = kobj_to_dev(kobj);
		if (dev->bus)
			return 1;
		if (dev->class)
			return 1;
	}
	return 0;
}

static const char *dev_uevent_name(const struct kobject *kobj)
{
	const struct device *dev = kobj_to_dev(kobj);

	if (dev->bus)
		return dev->bus->name;
	if (dev->class)
		return dev->class->name;
	return NULL;
}

static int dev_uevent(const struct kobject *kobj, struct kobj_uevent_env *env)
{
	const struct device *dev = kobj_to_dev(kobj);
	int retval = 0;

	 
	if (MAJOR(dev->devt)) {
		const char *tmp;
		const char *name;
		umode_t mode = 0;
		kuid_t uid = GLOBAL_ROOT_UID;
		kgid_t gid = GLOBAL_ROOT_GID;

		add_uevent_var(env, "MAJOR=%u", MAJOR(dev->devt));
		add_uevent_var(env, "MINOR=%u", MINOR(dev->devt));
		name = device_get_devnode(dev, &mode, &uid, &gid, &tmp);
		if (name) {
			add_uevent_var(env, "DEVNAME=%s", name);
			if (mode)
				add_uevent_var(env, "DEVMODE=%#o", mode & 0777);
			if (!uid_eq(uid, GLOBAL_ROOT_UID))
				add_uevent_var(env, "DEVUID=%u", from_kuid(&init_user_ns, uid));
			if (!gid_eq(gid, GLOBAL_ROOT_GID))
				add_uevent_var(env, "DEVGID=%u", from_kgid(&init_user_ns, gid));
			kfree(tmp);
		}
	}

	if (dev->type && dev->type->name)
		add_uevent_var(env, "DEVTYPE=%s", dev->type->name);

	if (dev->driver)
		add_uevent_var(env, "DRIVER=%s", dev->driver->name);

	 
	of_device_uevent(dev, env);

	 
	if (dev->bus && dev->bus->uevent) {
		retval = dev->bus->uevent(dev, env);
		if (retval)
			pr_debug("device: '%s': %s: bus uevent() returned %d\n",
				 dev_name(dev), __func__, retval);
	}

	 
	if (dev->class && dev->class->dev_uevent) {
		retval = dev->class->dev_uevent(dev, env);
		if (retval)
			pr_debug("device: '%s': %s: class uevent() "
				 "returned %d\n", dev_name(dev),
				 __func__, retval);
	}

	 
	if (dev->type && dev->type->uevent) {
		retval = dev->type->uevent(dev, env);
		if (retval)
			pr_debug("device: '%s': %s: dev_type uevent() "
				 "returned %d\n", dev_name(dev),
				 __func__, retval);
	}

	return retval;
}

static const struct kset_uevent_ops device_uevent_ops = {
	.filter =	dev_uevent_filter,
	.name =		dev_uevent_name,
	.uevent =	dev_uevent,
};

static ssize_t uevent_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct kobject *top_kobj;
	struct kset *kset;
	struct kobj_uevent_env *env = NULL;
	int i;
	int len = 0;
	int retval;

	 
	top_kobj = &dev->kobj;
	while (!top_kobj->kset && top_kobj->parent)
		top_kobj = top_kobj->parent;
	if (!top_kobj->kset)
		goto out;

	kset = top_kobj->kset;
	if (!kset->uevent_ops || !kset->uevent_ops->uevent)
		goto out;

	 
	if (kset->uevent_ops && kset->uevent_ops->filter)
		if (!kset->uevent_ops->filter(&dev->kobj))
			goto out;

	env = kzalloc(sizeof(struct kobj_uevent_env), GFP_KERNEL);
	if (!env)
		return -ENOMEM;

	 
	retval = kset->uevent_ops->uevent(&dev->kobj, env);
	if (retval)
		goto out;

	 
	for (i = 0; i < env->envp_idx; i++)
		len += sysfs_emit_at(buf, len, "%s\n", env->envp[i]);
out:
	kfree(env);
	return len;
}

static ssize_t uevent_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int rc;

	rc = kobject_synth_uevent(&dev->kobj, buf, count);

	if (rc) {
		dev_err(dev, "uevent: failed to send synthetic uevent: %d\n", rc);
		return rc;
	}

	return count;
}
static DEVICE_ATTR_RW(uevent);

static ssize_t online_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	bool val;

	device_lock(dev);
	val = !dev->offline;
	device_unlock(dev);
	return sysfs_emit(buf, "%u\n", val);
}

static ssize_t online_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	bool val;
	int ret;

	ret = kstrtobool(buf, &val);
	if (ret < 0)
		return ret;

	ret = lock_device_hotplug_sysfs();
	if (ret)
		return ret;

	ret = val ? device_online(dev) : device_offline(dev);
	unlock_device_hotplug();
	return ret < 0 ? ret : count;
}
static DEVICE_ATTR_RW(online);

static ssize_t removable_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	const char *loc;

	switch (dev->removable) {
	case DEVICE_REMOVABLE:
		loc = "removable";
		break;
	case DEVICE_FIXED:
		loc = "fixed";
		break;
	default:
		loc = "unknown";
	}
	return sysfs_emit(buf, "%s\n", loc);
}
static DEVICE_ATTR_RO(removable);

int device_add_groups(struct device *dev, const struct attribute_group **groups)
{
	return sysfs_create_groups(&dev->kobj, groups);
}
EXPORT_SYMBOL_GPL(device_add_groups);

void device_remove_groups(struct device *dev,
			  const struct attribute_group **groups)
{
	sysfs_remove_groups(&dev->kobj, groups);
}
EXPORT_SYMBOL_GPL(device_remove_groups);

union device_attr_group_devres {
	const struct attribute_group *group;
	const struct attribute_group **groups;
};

static void devm_attr_group_remove(struct device *dev, void *res)
{
	union device_attr_group_devres *devres = res;
	const struct attribute_group *group = devres->group;

	dev_dbg(dev, "%s: removing group %p\n", __func__, group);
	sysfs_remove_group(&dev->kobj, group);
}

static void devm_attr_groups_remove(struct device *dev, void *res)
{
	union device_attr_group_devres *devres = res;
	const struct attribute_group **groups = devres->groups;

	dev_dbg(dev, "%s: removing groups %p\n", __func__, groups);
	sysfs_remove_groups(&dev->kobj, groups);
}

 
int devm_device_add_group(struct device *dev, const struct attribute_group *grp)
{
	union device_attr_group_devres *devres;
	int error;

	devres = devres_alloc(devm_attr_group_remove,
			      sizeof(*devres), GFP_KERNEL);
	if (!devres)
		return -ENOMEM;

	error = sysfs_create_group(&dev->kobj, grp);
	if (error) {
		devres_free(devres);
		return error;
	}

	devres->group = grp;
	devres_add(dev, devres);
	return 0;
}
EXPORT_SYMBOL_GPL(devm_device_add_group);

 
int devm_device_add_groups(struct device *dev,
			   const struct attribute_group **groups)
{
	union device_attr_group_devres *devres;
	int error;

	devres = devres_alloc(devm_attr_groups_remove,
			      sizeof(*devres), GFP_KERNEL);
	if (!devres)
		return -ENOMEM;

	error = sysfs_create_groups(&dev->kobj, groups);
	if (error) {
		devres_free(devres);
		return error;
	}

	devres->groups = groups;
	devres_add(dev, devres);
	return 0;
}
EXPORT_SYMBOL_GPL(devm_device_add_groups);

static int device_add_attrs(struct device *dev)
{
	const struct class *class = dev->class;
	const struct device_type *type = dev->type;
	int error;

	if (class) {
		error = device_add_groups(dev, class->dev_groups);
		if (error)
			return error;
	}

	if (type) {
		error = device_add_groups(dev, type->groups);
		if (error)
			goto err_remove_class_groups;
	}

	error = device_add_groups(dev, dev->groups);
	if (error)
		goto err_remove_type_groups;

	if (device_supports_offline(dev) && !dev->offline_disabled) {
		error = device_create_file(dev, &dev_attr_online);
		if (error)
			goto err_remove_dev_groups;
	}

	if (fw_devlink_flags && !fw_devlink_is_permissive() && dev->fwnode) {
		error = device_create_file(dev, &dev_attr_waiting_for_supplier);
		if (error)
			goto err_remove_dev_online;
	}

	if (dev_removable_is_valid(dev)) {
		error = device_create_file(dev, &dev_attr_removable);
		if (error)
			goto err_remove_dev_waiting_for_supplier;
	}

	if (dev_add_physical_location(dev)) {
		error = device_add_group(dev,
			&dev_attr_physical_location_group);
		if (error)
			goto err_remove_dev_removable;
	}

	return 0;

 err_remove_dev_removable:
	device_remove_file(dev, &dev_attr_removable);
 err_remove_dev_waiting_for_supplier:
	device_remove_file(dev, &dev_attr_waiting_for_supplier);
 err_remove_dev_online:
	device_remove_file(dev, &dev_attr_online);
 err_remove_dev_groups:
	device_remove_groups(dev, dev->groups);
 err_remove_type_groups:
	if (type)
		device_remove_groups(dev, type->groups);
 err_remove_class_groups:
	if (class)
		device_remove_groups(dev, class->dev_groups);

	return error;
}

static void device_remove_attrs(struct device *dev)
{
	const struct class *class = dev->class;
	const struct device_type *type = dev->type;

	if (dev->physical_location) {
		device_remove_group(dev, &dev_attr_physical_location_group);
		kfree(dev->physical_location);
	}

	device_remove_file(dev, &dev_attr_removable);
	device_remove_file(dev, &dev_attr_waiting_for_supplier);
	device_remove_file(dev, &dev_attr_online);
	device_remove_groups(dev, dev->groups);

	if (type)
		device_remove_groups(dev, type->groups);

	if (class)
		device_remove_groups(dev, class->dev_groups);
}

static ssize_t dev_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return print_dev_t(buf, dev->devt);
}
static DEVICE_ATTR_RO(dev);

 
struct kset *devices_kset;

 
static void devices_kset_move_before(struct device *deva, struct device *devb)
{
	if (!devices_kset)
		return;
	pr_debug("devices_kset: Moving %s before %s\n",
		 dev_name(deva), dev_name(devb));
	spin_lock(&devices_kset->list_lock);
	list_move_tail(&deva->kobj.entry, &devb->kobj.entry);
	spin_unlock(&devices_kset->list_lock);
}

 
static void devices_kset_move_after(struct device *deva, struct device *devb)
{
	if (!devices_kset)
		return;
	pr_debug("devices_kset: Moving %s after %s\n",
		 dev_name(deva), dev_name(devb));
	spin_lock(&devices_kset->list_lock);
	list_move(&deva->kobj.entry, &devb->kobj.entry);
	spin_unlock(&devices_kset->list_lock);
}

 
void devices_kset_move_last(struct device *dev)
{
	if (!devices_kset)
		return;
	pr_debug("devices_kset: Moving %s to end of list\n", dev_name(dev));
	spin_lock(&devices_kset->list_lock);
	list_move_tail(&dev->kobj.entry, &devices_kset->list);
	spin_unlock(&devices_kset->list_lock);
}

 
int device_create_file(struct device *dev,
		       const struct device_attribute *attr)
{
	int error = 0;

	if (dev) {
		WARN(((attr->attr.mode & S_IWUGO) && !attr->store),
			"Attribute %s: write permission without 'store'\n",
			attr->attr.name);
		WARN(((attr->attr.mode & S_IRUGO) && !attr->show),
			"Attribute %s: read permission without 'show'\n",
			attr->attr.name);
		error = sysfs_create_file(&dev->kobj, &attr->attr);
	}

	return error;
}
EXPORT_SYMBOL_GPL(device_create_file);

 
void device_remove_file(struct device *dev,
			const struct device_attribute *attr)
{
	if (dev)
		sysfs_remove_file(&dev->kobj, &attr->attr);
}
EXPORT_SYMBOL_GPL(device_remove_file);

 
bool device_remove_file_self(struct device *dev,
			     const struct device_attribute *attr)
{
	if (dev)
		return sysfs_remove_file_self(&dev->kobj, &attr->attr);
	else
		return false;
}
EXPORT_SYMBOL_GPL(device_remove_file_self);

 
int device_create_bin_file(struct device *dev,
			   const struct bin_attribute *attr)
{
	int error = -EINVAL;
	if (dev)
		error = sysfs_create_bin_file(&dev->kobj, attr);
	return error;
}
EXPORT_SYMBOL_GPL(device_create_bin_file);

 
void device_remove_bin_file(struct device *dev,
			    const struct bin_attribute *attr)
{
	if (dev)
		sysfs_remove_bin_file(&dev->kobj, attr);
}
EXPORT_SYMBOL_GPL(device_remove_bin_file);

static void klist_children_get(struct klist_node *n)
{
	struct device_private *p = to_device_private_parent(n);
	struct device *dev = p->device;

	get_device(dev);
}

static void klist_children_put(struct klist_node *n)
{
	struct device_private *p = to_device_private_parent(n);
	struct device *dev = p->device;

	put_device(dev);
}

 
void device_initialize(struct device *dev)
{
	dev->kobj.kset = devices_kset;
	kobject_init(&dev->kobj, &device_ktype);
	INIT_LIST_HEAD(&dev->dma_pools);
	mutex_init(&dev->mutex);
	lockdep_set_novalidate_class(&dev->mutex);
	spin_lock_init(&dev->devres_lock);
	INIT_LIST_HEAD(&dev->devres_head);
	device_pm_init(dev);
	set_dev_node(dev, NUMA_NO_NODE);
	INIT_LIST_HEAD(&dev->links.consumers);
	INIT_LIST_HEAD(&dev->links.suppliers);
	INIT_LIST_HEAD(&dev->links.defer_sync);
	dev->links.status = DL_DEV_NO_DRIVER;
#if defined(CONFIG_ARCH_HAS_SYNC_DMA_FOR_DEVICE) || \
    defined(CONFIG_ARCH_HAS_SYNC_DMA_FOR_CPU) || \
    defined(CONFIG_ARCH_HAS_SYNC_DMA_FOR_CPU_ALL)
	dev->dma_coherent = dma_default_coherent;
#endif
	swiotlb_dev_init(dev);
}
EXPORT_SYMBOL_GPL(device_initialize);

struct kobject *virtual_device_parent(struct device *dev)
{
	static struct kobject *virtual_dir = NULL;

	if (!virtual_dir)
		virtual_dir = kobject_create_and_add("virtual",
						     &devices_kset->kobj);

	return virtual_dir;
}

struct class_dir {
	struct kobject kobj;
	const struct class *class;
};

#define to_class_dir(obj) container_of(obj, struct class_dir, kobj)

static void class_dir_release(struct kobject *kobj)
{
	struct class_dir *dir = to_class_dir(kobj);
	kfree(dir);
}

static const
struct kobj_ns_type_operations *class_dir_child_ns_type(const struct kobject *kobj)
{
	const struct class_dir *dir = to_class_dir(kobj);
	return dir->class->ns_type;
}

static const struct kobj_type class_dir_ktype = {
	.release	= class_dir_release,
	.sysfs_ops	= &kobj_sysfs_ops,
	.child_ns_type	= class_dir_child_ns_type
};

static struct kobject *class_dir_create_and_add(struct subsys_private *sp,
						struct kobject *parent_kobj)
{
	struct class_dir *dir;
	int retval;

	dir = kzalloc(sizeof(*dir), GFP_KERNEL);
	if (!dir)
		return ERR_PTR(-ENOMEM);

	dir->class = sp->class;
	kobject_init(&dir->kobj, &class_dir_ktype);

	dir->kobj.kset = &sp->glue_dirs;

	retval = kobject_add(&dir->kobj, parent_kobj, "%s", sp->class->name);
	if (retval < 0) {
		kobject_put(&dir->kobj);
		return ERR_PTR(retval);
	}
	return &dir->kobj;
}

static DEFINE_MUTEX(gdp_mutex);

static struct kobject *get_device_parent(struct device *dev,
					 struct device *parent)
{
	struct subsys_private *sp = class_to_subsys(dev->class);
	struct kobject *kobj = NULL;

	if (sp) {
		struct kobject *parent_kobj;
		struct kobject *k;

		 
		if (parent == NULL)
			parent_kobj = virtual_device_parent(dev);
		else if (parent->class && !dev->class->ns_type) {
			subsys_put(sp);
			return &parent->kobj;
		} else {
			parent_kobj = &parent->kobj;
		}

		mutex_lock(&gdp_mutex);

		 
		spin_lock(&sp->glue_dirs.list_lock);
		list_for_each_entry(k, &sp->glue_dirs.list, entry)
			if (k->parent == parent_kobj) {
				kobj = kobject_get(k);
				break;
			}
		spin_unlock(&sp->glue_dirs.list_lock);
		if (kobj) {
			mutex_unlock(&gdp_mutex);
			subsys_put(sp);
			return kobj;
		}

		 
		k = class_dir_create_and_add(sp, parent_kobj);
		 
		mutex_unlock(&gdp_mutex);
		subsys_put(sp);
		return k;
	}

	 
	if (!parent && dev->bus) {
		struct device *dev_root = bus_get_dev_root(dev->bus);

		if (dev_root) {
			kobj = &dev_root->kobj;
			put_device(dev_root);
			return kobj;
		}
	}

	if (parent)
		return &parent->kobj;
	return NULL;
}

static inline bool live_in_glue_dir(struct kobject *kobj,
				    struct device *dev)
{
	struct subsys_private *sp;
	bool retval;

	if (!kobj || !dev->class)
		return false;

	sp = class_to_subsys(dev->class);
	if (!sp)
		return false;

	if (kobj->kset == &sp->glue_dirs)
		retval = true;
	else
		retval = false;

	subsys_put(sp);
	return retval;
}

static inline struct kobject *get_glue_dir(struct device *dev)
{
	return dev->kobj.parent;
}

 
static inline bool kobject_has_children(struct kobject *kobj)
{
	WARN_ON_ONCE(kref_read(&kobj->kref) == 0);

	return kobj->sd && kobj->sd->dir.subdirs;
}

 
static void cleanup_glue_dir(struct device *dev, struct kobject *glue_dir)
{
	unsigned int ref;

	 
	if (!live_in_glue_dir(glue_dir, dev))
		return;

	mutex_lock(&gdp_mutex);
	 
	ref = kref_read(&glue_dir->kref);
	if (!kobject_has_children(glue_dir) && !--ref)
		kobject_del(glue_dir);
	kobject_put(glue_dir);
	mutex_unlock(&gdp_mutex);
}

static int device_add_class_symlinks(struct device *dev)
{
	struct device_node *of_node = dev_of_node(dev);
	struct subsys_private *sp;
	int error;

	if (of_node) {
		error = sysfs_create_link(&dev->kobj, of_node_kobj(of_node), "of_node");
		if (error)
			dev_warn(dev, "Error %d creating of_node link\n",error);
		 
	}

	sp = class_to_subsys(dev->class);
	if (!sp)
		return 0;

	error = sysfs_create_link(&dev->kobj, &sp->subsys.kobj, "subsystem");
	if (error)
		goto out_devnode;

	if (dev->parent && device_is_not_partition(dev)) {
		error = sysfs_create_link(&dev->kobj, &dev->parent->kobj,
					  "device");
		if (error)
			goto out_subsys;
	}

	 
	error = sysfs_create_link(&sp->subsys.kobj, &dev->kobj, dev_name(dev));
	if (error)
		goto out_device;
	goto exit;

out_device:
	sysfs_remove_link(&dev->kobj, "device");
out_subsys:
	sysfs_remove_link(&dev->kobj, "subsystem");
out_devnode:
	sysfs_remove_link(&dev->kobj, "of_node");
exit:
	subsys_put(sp);
	return error;
}

static void device_remove_class_symlinks(struct device *dev)
{
	struct subsys_private *sp = class_to_subsys(dev->class);

	if (dev_of_node(dev))
		sysfs_remove_link(&dev->kobj, "of_node");

	if (!sp)
		return;

	if (dev->parent && device_is_not_partition(dev))
		sysfs_remove_link(&dev->kobj, "device");
	sysfs_remove_link(&dev->kobj, "subsystem");
	sysfs_delete_link(&sp->subsys.kobj, &dev->kobj, dev_name(dev));
	subsys_put(sp);
}

 
int dev_set_name(struct device *dev, const char *fmt, ...)
{
	va_list vargs;
	int err;

	va_start(vargs, fmt);
	err = kobject_set_name_vargs(&dev->kobj, fmt, vargs);
	va_end(vargs);
	return err;
}
EXPORT_SYMBOL_GPL(dev_set_name);

 
static struct kobject *device_to_dev_kobj(struct device *dev)
{
	if (is_blockdev(dev))
		return sysfs_dev_block_kobj;
	else
		return sysfs_dev_char_kobj;
}

static int device_create_sys_dev_entry(struct device *dev)
{
	struct kobject *kobj = device_to_dev_kobj(dev);
	int error = 0;
	char devt_str[15];

	if (kobj) {
		format_dev_t(devt_str, dev->devt);
		error = sysfs_create_link(kobj, &dev->kobj, devt_str);
	}

	return error;
}

static void device_remove_sys_dev_entry(struct device *dev)
{
	struct kobject *kobj = device_to_dev_kobj(dev);
	char devt_str[15];

	if (kobj) {
		format_dev_t(devt_str, dev->devt);
		sysfs_remove_link(kobj, devt_str);
	}
}

static int device_private_init(struct device *dev)
{
	dev->p = kzalloc(sizeof(*dev->p), GFP_KERNEL);
	if (!dev->p)
		return -ENOMEM;
	dev->p->device = dev;
	klist_init(&dev->p->klist_children, klist_children_get,
		   klist_children_put);
	INIT_LIST_HEAD(&dev->p->deferred_probe);
	return 0;
}

 
int device_add(struct device *dev)
{
	struct subsys_private *sp;
	struct device *parent;
	struct kobject *kobj;
	struct class_interface *class_intf;
	int error = -EINVAL;
	struct kobject *glue_dir = NULL;

	dev = get_device(dev);
	if (!dev)
		goto done;

	if (!dev->p) {
		error = device_private_init(dev);
		if (error)
			goto done;
	}

	 
	if (dev->init_name) {
		error = dev_set_name(dev, "%s", dev->init_name);
		dev->init_name = NULL;
	}

	if (dev_name(dev))
		error = 0;
	 
	else if (dev->bus && dev->bus->dev_name)
		error = dev_set_name(dev, "%s%u", dev->bus->dev_name, dev->id);
	else
		error = -EINVAL;
	if (error)
		goto name_error;

	pr_debug("device: '%s': %s\n", dev_name(dev), __func__);

	parent = get_device(dev->parent);
	kobj = get_device_parent(dev, parent);
	if (IS_ERR(kobj)) {
		error = PTR_ERR(kobj);
		goto parent_error;
	}
	if (kobj)
		dev->kobj.parent = kobj;

	 
	if (parent && (dev_to_node(dev) == NUMA_NO_NODE))
		set_dev_node(dev, dev_to_node(parent));

	 
	 
	error = kobject_add(&dev->kobj, dev->kobj.parent, NULL);
	if (error) {
		glue_dir = kobj;
		goto Error;
	}

	 
	device_platform_notify(dev);

	error = device_create_file(dev, &dev_attr_uevent);
	if (error)
		goto attrError;

	error = device_add_class_symlinks(dev);
	if (error)
		goto SymlinkError;
	error = device_add_attrs(dev);
	if (error)
		goto AttrsError;
	error = bus_add_device(dev);
	if (error)
		goto BusError;
	error = dpm_sysfs_add(dev);
	if (error)
		goto DPMError;
	device_pm_add(dev);

	if (MAJOR(dev->devt)) {
		error = device_create_file(dev, &dev_attr_dev);
		if (error)
			goto DevAttrError;

		error = device_create_sys_dev_entry(dev);
		if (error)
			goto SysEntryError;

		devtmpfs_create_node(dev);
	}

	 
	bus_notify(dev, BUS_NOTIFY_ADD_DEVICE);
	kobject_uevent(&dev->kobj, KOBJ_ADD);

	 
	if (dev->fwnode && !dev->fwnode->dev) {
		dev->fwnode->dev = dev;
		fw_devlink_link_device(dev);
	}

	bus_probe_device(dev);

	 
	if (dev->fwnode && fw_devlink_drv_reg_done && !dev->can_match)
		fw_devlink_unblock_consumers(dev);

	if (parent)
		klist_add_tail(&dev->p->knode_parent,
			       &parent->p->klist_children);

	sp = class_to_subsys(dev->class);
	if (sp) {
		mutex_lock(&sp->mutex);
		 
		klist_add_tail(&dev->p->knode_class, &sp->klist_devices);

		 
		list_for_each_entry(class_intf, &sp->interfaces, node)
			if (class_intf->add_dev)
				class_intf->add_dev(dev);
		mutex_unlock(&sp->mutex);
		subsys_put(sp);
	}
done:
	put_device(dev);
	return error;
 SysEntryError:
	if (MAJOR(dev->devt))
		device_remove_file(dev, &dev_attr_dev);
 DevAttrError:
	device_pm_remove(dev);
	dpm_sysfs_remove(dev);
 DPMError:
	dev->driver = NULL;
	bus_remove_device(dev);
 BusError:
	device_remove_attrs(dev);
 AttrsError:
	device_remove_class_symlinks(dev);
 SymlinkError:
	device_remove_file(dev, &dev_attr_uevent);
 attrError:
	device_platform_notify_remove(dev);
	kobject_uevent(&dev->kobj, KOBJ_REMOVE);
	glue_dir = get_glue_dir(dev);
	kobject_del(&dev->kobj);
 Error:
	cleanup_glue_dir(dev, glue_dir);
parent_error:
	put_device(parent);
name_error:
	kfree(dev->p);
	dev->p = NULL;
	goto done;
}
EXPORT_SYMBOL_GPL(device_add);

 
int device_register(struct device *dev)
{
	device_initialize(dev);
	return device_add(dev);
}
EXPORT_SYMBOL_GPL(device_register);

 
struct device *get_device(struct device *dev)
{
	return dev ? kobj_to_dev(kobject_get(&dev->kobj)) : NULL;
}
EXPORT_SYMBOL_GPL(get_device);

 
void put_device(struct device *dev)
{
	 
	if (dev)
		kobject_put(&dev->kobj);
}
EXPORT_SYMBOL_GPL(put_device);

bool kill_device(struct device *dev)
{
	 
	device_lock_assert(dev);

	if (dev->p->dead)
		return false;
	dev->p->dead = true;
	return true;
}
EXPORT_SYMBOL_GPL(kill_device);

 
void device_del(struct device *dev)
{
	struct subsys_private *sp;
	struct device *parent = dev->parent;
	struct kobject *glue_dir = NULL;
	struct class_interface *class_intf;
	unsigned int noio_flag;

	device_lock(dev);
	kill_device(dev);
	device_unlock(dev);

	if (dev->fwnode && dev->fwnode->dev == dev)
		dev->fwnode->dev = NULL;

	 
	noio_flag = memalloc_noio_save();
	bus_notify(dev, BUS_NOTIFY_DEL_DEVICE);

	dpm_sysfs_remove(dev);
	if (parent)
		klist_del(&dev->p->knode_parent);
	if (MAJOR(dev->devt)) {
		devtmpfs_delete_node(dev);
		device_remove_sys_dev_entry(dev);
		device_remove_file(dev, &dev_attr_dev);
	}

	sp = class_to_subsys(dev->class);
	if (sp) {
		device_remove_class_symlinks(dev);

		mutex_lock(&sp->mutex);
		 
		list_for_each_entry(class_intf, &sp->interfaces, node)
			if (class_intf->remove_dev)
				class_intf->remove_dev(dev);
		 
		klist_del(&dev->p->knode_class);
		mutex_unlock(&sp->mutex);
		subsys_put(sp);
	}
	device_remove_file(dev, &dev_attr_uevent);
	device_remove_attrs(dev);
	bus_remove_device(dev);
	device_pm_remove(dev);
	driver_deferred_probe_del(dev);
	device_platform_notify_remove(dev);
	device_links_purge(dev);

	 
	devres_release_all(dev);

	bus_notify(dev, BUS_NOTIFY_REMOVED_DEVICE);
	kobject_uevent(&dev->kobj, KOBJ_REMOVE);
	glue_dir = get_glue_dir(dev);
	kobject_del(&dev->kobj);
	cleanup_glue_dir(dev, glue_dir);
	memalloc_noio_restore(noio_flag);
	put_device(parent);
}
EXPORT_SYMBOL_GPL(device_del);

 
void device_unregister(struct device *dev)
{
	pr_debug("device: '%s': %s\n", dev_name(dev), __func__);
	device_del(dev);
	put_device(dev);
}
EXPORT_SYMBOL_GPL(device_unregister);

static struct device *prev_device(struct klist_iter *i)
{
	struct klist_node *n = klist_prev(i);
	struct device *dev = NULL;
	struct device_private *p;

	if (n) {
		p = to_device_private_parent(n);
		dev = p->device;
	}
	return dev;
}

static struct device *next_device(struct klist_iter *i)
{
	struct klist_node *n = klist_next(i);
	struct device *dev = NULL;
	struct device_private *p;

	if (n) {
		p = to_device_private_parent(n);
		dev = p->device;
	}
	return dev;
}

 
const char *device_get_devnode(const struct device *dev,
			       umode_t *mode, kuid_t *uid, kgid_t *gid,
			       const char **tmp)
{
	char *s;

	*tmp = NULL;

	 
	if (dev->type && dev->type->devnode)
		*tmp = dev->type->devnode(dev, mode, uid, gid);
	if (*tmp)
		return *tmp;

	 
	if (dev->class && dev->class->devnode)
		*tmp = dev->class->devnode(dev, mode);
	if (*tmp)
		return *tmp;

	 
	if (strchr(dev_name(dev), '!') == NULL)
		return dev_name(dev);

	 
	s = kstrdup_and_replace(dev_name(dev), '!', '/', GFP_KERNEL);
	if (!s)
		return NULL;
	return *tmp = s;
}

 
int device_for_each_child(struct device *parent, void *data,
			  int (*fn)(struct device *dev, void *data))
{
	struct klist_iter i;
	struct device *child;
	int error = 0;

	if (!parent->p)
		return 0;

	klist_iter_init(&parent->p->klist_children, &i);
	while (!error && (child = next_device(&i)))
		error = fn(child, data);
	klist_iter_exit(&i);
	return error;
}
EXPORT_SYMBOL_GPL(device_for_each_child);

 
int device_for_each_child_reverse(struct device *parent, void *data,
				  int (*fn)(struct device *dev, void *data))
{
	struct klist_iter i;
	struct device *child;
	int error = 0;

	if (!parent->p)
		return 0;

	klist_iter_init(&parent->p->klist_children, &i);
	while ((child = prev_device(&i)) && !error)
		error = fn(child, data);
	klist_iter_exit(&i);
	return error;
}
EXPORT_SYMBOL_GPL(device_for_each_child_reverse);

 
struct device *device_find_child(struct device *parent, void *data,
				 int (*match)(struct device *dev, void *data))
{
	struct klist_iter i;
	struct device *child;

	if (!parent)
		return NULL;

	klist_iter_init(&parent->p->klist_children, &i);
	while ((child = next_device(&i)))
		if (match(child, data) && get_device(child))
			break;
	klist_iter_exit(&i);
	return child;
}
EXPORT_SYMBOL_GPL(device_find_child);

 
struct device *device_find_child_by_name(struct device *parent,
					 const char *name)
{
	struct klist_iter i;
	struct device *child;

	if (!parent)
		return NULL;

	klist_iter_init(&parent->p->klist_children, &i);
	while ((child = next_device(&i)))
		if (sysfs_streq(dev_name(child), name) && get_device(child))
			break;
	klist_iter_exit(&i);
	return child;
}
EXPORT_SYMBOL_GPL(device_find_child_by_name);

static int match_any(struct device *dev, void *unused)
{
	return 1;
}

 
struct device *device_find_any_child(struct device *parent)
{
	return device_find_child(parent, NULL, match_any);
}
EXPORT_SYMBOL_GPL(device_find_any_child);

int __init devices_init(void)
{
	devices_kset = kset_create_and_add("devices", &device_uevent_ops, NULL);
	if (!devices_kset)
		return -ENOMEM;
	dev_kobj = kobject_create_and_add("dev", NULL);
	if (!dev_kobj)
		goto dev_kobj_err;
	sysfs_dev_block_kobj = kobject_create_and_add("block", dev_kobj);
	if (!sysfs_dev_block_kobj)
		goto block_kobj_err;
	sysfs_dev_char_kobj = kobject_create_and_add("char", dev_kobj);
	if (!sysfs_dev_char_kobj)
		goto char_kobj_err;

	return 0;

 char_kobj_err:
	kobject_put(sysfs_dev_block_kobj);
 block_kobj_err:
	kobject_put(dev_kobj);
 dev_kobj_err:
	kset_unregister(devices_kset);
	return -ENOMEM;
}

static int device_check_offline(struct device *dev, void *not_used)
{
	int ret;

	ret = device_for_each_child(dev, NULL, device_check_offline);
	if (ret)
		return ret;

	return device_supports_offline(dev) && !dev->offline ? -EBUSY : 0;
}

 
int device_offline(struct device *dev)
{
	int ret;

	if (dev->offline_disabled)
		return -EPERM;

	ret = device_for_each_child(dev, NULL, device_check_offline);
	if (ret)
		return ret;

	device_lock(dev);
	if (device_supports_offline(dev)) {
		if (dev->offline) {
			ret = 1;
		} else {
			ret = dev->bus->offline(dev);
			if (!ret) {
				kobject_uevent(&dev->kobj, KOBJ_OFFLINE);
				dev->offline = true;
			}
		}
	}
	device_unlock(dev);

	return ret;
}

 
int device_online(struct device *dev)
{
	int ret = 0;

	device_lock(dev);
	if (device_supports_offline(dev)) {
		if (dev->offline) {
			ret = dev->bus->online(dev);
			if (!ret) {
				kobject_uevent(&dev->kobj, KOBJ_ONLINE);
				dev->offline = false;
			}
		} else {
			ret = 1;
		}
	}
	device_unlock(dev);

	return ret;
}

struct root_device {
	struct device dev;
	struct module *owner;
};

static inline struct root_device *to_root_device(struct device *d)
{
	return container_of(d, struct root_device, dev);
}

static void root_device_release(struct device *dev)
{
	kfree(to_root_device(dev));
}

 
struct device *__root_device_register(const char *name, struct module *owner)
{
	struct root_device *root;
	int err = -ENOMEM;

	root = kzalloc(sizeof(struct root_device), GFP_KERNEL);
	if (!root)
		return ERR_PTR(err);

	err = dev_set_name(&root->dev, "%s", name);
	if (err) {
		kfree(root);
		return ERR_PTR(err);
	}

	root->dev.release = root_device_release;

	err = device_register(&root->dev);
	if (err) {
		put_device(&root->dev);
		return ERR_PTR(err);
	}

#ifdef CONFIG_MODULES	 
	if (owner) {
		struct module_kobject *mk = &owner->mkobj;

		err = sysfs_create_link(&root->dev.kobj, &mk->kobj, "module");
		if (err) {
			device_unregister(&root->dev);
			return ERR_PTR(err);
		}
		root->owner = owner;
	}
#endif

	return &root->dev;
}
EXPORT_SYMBOL_GPL(__root_device_register);

 
void root_device_unregister(struct device *dev)
{
	struct root_device *root = to_root_device(dev);

	if (root->owner)
		sysfs_remove_link(&root->dev.kobj, "module");

	device_unregister(dev);
}
EXPORT_SYMBOL_GPL(root_device_unregister);


static void device_create_release(struct device *dev)
{
	pr_debug("device: '%s': %s\n", dev_name(dev), __func__);
	kfree(dev);
}

static __printf(6, 0) struct device *
device_create_groups_vargs(const struct class *class, struct device *parent,
			   dev_t devt, void *drvdata,
			   const struct attribute_group **groups,
			   const char *fmt, va_list args)
{
	struct device *dev = NULL;
	int retval = -ENODEV;

	if (IS_ERR_OR_NULL(class))
		goto error;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		retval = -ENOMEM;
		goto error;
	}

	device_initialize(dev);
	dev->devt = devt;
	dev->class = class;
	dev->parent = parent;
	dev->groups = groups;
	dev->release = device_create_release;
	dev_set_drvdata(dev, drvdata);

	retval = kobject_set_name_vargs(&dev->kobj, fmt, args);
	if (retval)
		goto error;

	retval = device_add(dev);
	if (retval)
		goto error;

	return dev;

error:
	put_device(dev);
	return ERR_PTR(retval);
}

 
struct device *device_create(const struct class *class, struct device *parent,
			     dev_t devt, void *drvdata, const char *fmt, ...)
{
	va_list vargs;
	struct device *dev;

	va_start(vargs, fmt);
	dev = device_create_groups_vargs(class, parent, devt, drvdata, NULL,
					  fmt, vargs);
	va_end(vargs);
	return dev;
}
EXPORT_SYMBOL_GPL(device_create);

 
struct device *device_create_with_groups(const struct class *class,
					 struct device *parent, dev_t devt,
					 void *drvdata,
					 const struct attribute_group **groups,
					 const char *fmt, ...)
{
	va_list vargs;
	struct device *dev;

	va_start(vargs, fmt);
	dev = device_create_groups_vargs(class, parent, devt, drvdata, groups,
					 fmt, vargs);
	va_end(vargs);
	return dev;
}
EXPORT_SYMBOL_GPL(device_create_with_groups);

 
void device_destroy(const struct class *class, dev_t devt)
{
	struct device *dev;

	dev = class_find_device_by_devt(class, devt);
	if (dev) {
		put_device(dev);
		device_unregister(dev);
	}
}
EXPORT_SYMBOL_GPL(device_destroy);

 
int device_rename(struct device *dev, const char *new_name)
{
	struct kobject *kobj = &dev->kobj;
	char *old_device_name = NULL;
	int error;

	dev = get_device(dev);
	if (!dev)
		return -EINVAL;

	dev_dbg(dev, "renaming to %s\n", new_name);

	old_device_name = kstrdup(dev_name(dev), GFP_KERNEL);
	if (!old_device_name) {
		error = -ENOMEM;
		goto out;
	}

	if (dev->class) {
		struct subsys_private *sp = class_to_subsys(dev->class);

		if (!sp) {
			error = -EINVAL;
			goto out;
		}

		error = sysfs_rename_link_ns(&sp->subsys.kobj, kobj, old_device_name,
					     new_name, kobject_namespace(kobj));
		subsys_put(sp);
		if (error)
			goto out;
	}

	error = kobject_rename(kobj, new_name);
	if (error)
		goto out;

out:
	put_device(dev);

	kfree(old_device_name);

	return error;
}
EXPORT_SYMBOL_GPL(device_rename);

static int device_move_class_links(struct device *dev,
				   struct device *old_parent,
				   struct device *new_parent)
{
	int error = 0;

	if (old_parent)
		sysfs_remove_link(&dev->kobj, "device");
	if (new_parent)
		error = sysfs_create_link(&dev->kobj, &new_parent->kobj,
					  "device");
	return error;
}

 
int device_move(struct device *dev, struct device *new_parent,
		enum dpm_order dpm_order)
{
	int error;
	struct device *old_parent;
	struct kobject *new_parent_kobj;

	dev = get_device(dev);
	if (!dev)
		return -EINVAL;

	device_pm_lock();
	new_parent = get_device(new_parent);
	new_parent_kobj = get_device_parent(dev, new_parent);
	if (IS_ERR(new_parent_kobj)) {
		error = PTR_ERR(new_parent_kobj);
		put_device(new_parent);
		goto out;
	}

	pr_debug("device: '%s': %s: moving to '%s'\n", dev_name(dev),
		 __func__, new_parent ? dev_name(new_parent) : "<NULL>");
	error = kobject_move(&dev->kobj, new_parent_kobj);
	if (error) {
		cleanup_glue_dir(dev, new_parent_kobj);
		put_device(new_parent);
		goto out;
	}
	old_parent = dev->parent;
	dev->parent = new_parent;
	if (old_parent)
		klist_remove(&dev->p->knode_parent);
	if (new_parent) {
		klist_add_tail(&dev->p->knode_parent,
			       &new_parent->p->klist_children);
		set_dev_node(dev, dev_to_node(new_parent));
	}

	if (dev->class) {
		error = device_move_class_links(dev, old_parent, new_parent);
		if (error) {
			 
			device_move_class_links(dev, new_parent, old_parent);
			if (!kobject_move(&dev->kobj, &old_parent->kobj)) {
				if (new_parent)
					klist_remove(&dev->p->knode_parent);
				dev->parent = old_parent;
				if (old_parent) {
					klist_add_tail(&dev->p->knode_parent,
						       &old_parent->p->klist_children);
					set_dev_node(dev, dev_to_node(old_parent));
				}
			}
			cleanup_glue_dir(dev, new_parent_kobj);
			put_device(new_parent);
			goto out;
		}
	}
	switch (dpm_order) {
	case DPM_ORDER_NONE:
		break;
	case DPM_ORDER_DEV_AFTER_PARENT:
		device_pm_move_after(dev, new_parent);
		devices_kset_move_after(dev, new_parent);
		break;
	case DPM_ORDER_PARENT_BEFORE_DEV:
		device_pm_move_before(new_parent, dev);
		devices_kset_move_before(new_parent, dev);
		break;
	case DPM_ORDER_DEV_LAST:
		device_pm_move_last(dev);
		devices_kset_move_last(dev);
		break;
	}

	put_device(old_parent);
out:
	device_pm_unlock();
	put_device(dev);
	return error;
}
EXPORT_SYMBOL_GPL(device_move);

static int device_attrs_change_owner(struct device *dev, kuid_t kuid,
				     kgid_t kgid)
{
	struct kobject *kobj = &dev->kobj;
	const struct class *class = dev->class;
	const struct device_type *type = dev->type;
	int error;

	if (class) {
		 
		error = sysfs_groups_change_owner(kobj, class->dev_groups, kuid,
						  kgid);
		if (error)
			return error;
	}

	if (type) {
		 
		error = sysfs_groups_change_owner(kobj, type->groups, kuid,
						  kgid);
		if (error)
			return error;
	}

	 
	error = sysfs_groups_change_owner(kobj, dev->groups, kuid, kgid);
	if (error)
		return error;

	if (device_supports_offline(dev) && !dev->offline_disabled) {
		 
		error = sysfs_file_change_owner(kobj, dev_attr_online.attr.name,
						kuid, kgid);
		if (error)
			return error;
	}

	return 0;
}

 
int device_change_owner(struct device *dev, kuid_t kuid, kgid_t kgid)
{
	int error;
	struct kobject *kobj = &dev->kobj;
	struct subsys_private *sp;

	dev = get_device(dev);
	if (!dev)
		return -EINVAL;

	 
	error = sysfs_change_owner(kobj, kuid, kgid);
	if (error)
		goto out;

	 
	error = sysfs_file_change_owner(kobj, dev_attr_uevent.attr.name, kuid,
					kgid);
	if (error)
		goto out;

	 
	error = device_attrs_change_owner(dev, kuid, kgid);
	if (error)
		goto out;

	error = dpm_sysfs_change_owner(dev, kuid, kgid);
	if (error)
		goto out;

	 
	sp = class_to_subsys(dev->class);
	if (!sp) {
		error = -EINVAL;
		goto out;
	}
	error = sysfs_link_change_owner(&sp->subsys.kobj, &dev->kobj, dev_name(dev), kuid, kgid);
	subsys_put(sp);

out:
	put_device(dev);
	return error;
}
EXPORT_SYMBOL_GPL(device_change_owner);

 
void device_shutdown(void)
{
	struct device *dev, *parent;

	wait_for_device_probe();
	device_block_probing();

	cpufreq_suspend();

	spin_lock(&devices_kset->list_lock);
	 
	while (!list_empty(&devices_kset->list)) {
		dev = list_entry(devices_kset->list.prev, struct device,
				kobj.entry);

		 
		parent = get_device(dev->parent);
		get_device(dev);
		 
		list_del_init(&dev->kobj.entry);
		spin_unlock(&devices_kset->list_lock);

		 
		if (parent)
			device_lock(parent);
		device_lock(dev);

		 
		pm_runtime_get_noresume(dev);
		pm_runtime_barrier(dev);

		if (dev->class && dev->class->shutdown_pre) {
			if (initcall_debug)
				dev_info(dev, "shutdown_pre\n");
			dev->class->shutdown_pre(dev);
		}
		if (dev->bus && dev->bus->shutdown) {
			if (initcall_debug)
				dev_info(dev, "shutdown\n");
			dev->bus->shutdown(dev);
		} else if (dev->driver && dev->driver->shutdown) {
			if (initcall_debug)
				dev_info(dev, "shutdown\n");
			dev->driver->shutdown(dev);
		}

		device_unlock(dev);
		if (parent)
			device_unlock(parent);

		put_device(dev);
		put_device(parent);

		spin_lock(&devices_kset->list_lock);
	}
	spin_unlock(&devices_kset->list_lock);
}

 

#ifdef CONFIG_PRINTK
static void
set_dev_info(const struct device *dev, struct dev_printk_info *dev_info)
{
	const char *subsys;

	memset(dev_info, 0, sizeof(*dev_info));

	if (dev->class)
		subsys = dev->class->name;
	else if (dev->bus)
		subsys = dev->bus->name;
	else
		return;

	strscpy(dev_info->subsystem, subsys, sizeof(dev_info->subsystem));

	 
	if (MAJOR(dev->devt)) {
		char c;

		if (strcmp(subsys, "block") == 0)
			c = 'b';
		else
			c = 'c';

		snprintf(dev_info->device, sizeof(dev_info->device),
			 "%c%u:%u", c, MAJOR(dev->devt), MINOR(dev->devt));
	} else if (strcmp(subsys, "net") == 0) {
		struct net_device *net = to_net_dev(dev);

		snprintf(dev_info->device, sizeof(dev_info->device),
			 "n%u", net->ifindex);
	} else {
		snprintf(dev_info->device, sizeof(dev_info->device),
			 "+%s:%s", subsys, dev_name(dev));
	}
}

int dev_vprintk_emit(int level, const struct device *dev,
		     const char *fmt, va_list args)
{
	struct dev_printk_info dev_info;

	set_dev_info(dev, &dev_info);

	return vprintk_emit(0, level, &dev_info, fmt, args);
}
EXPORT_SYMBOL(dev_vprintk_emit);

int dev_printk_emit(int level, const struct device *dev, const char *fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);

	r = dev_vprintk_emit(level, dev, fmt, args);

	va_end(args);

	return r;
}
EXPORT_SYMBOL(dev_printk_emit);

static void __dev_printk(const char *level, const struct device *dev,
			struct va_format *vaf)
{
	if (dev)
		dev_printk_emit(level[1] - '0', dev, "%s %s: %pV",
				dev_driver_string(dev), dev_name(dev), vaf);
	else
		printk("%s(NULL device *): %pV", level, vaf);
}

void _dev_printk(const char *level, const struct device *dev,
		 const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	__dev_printk(level, dev, &vaf);

	va_end(args);
}
EXPORT_SYMBOL(_dev_printk);

#define define_dev_printk_level(func, kern_level)		\
void func(const struct device *dev, const char *fmt, ...)	\
{								\
	struct va_format vaf;					\
	va_list args;						\
								\
	va_start(args, fmt);					\
								\
	vaf.fmt = fmt;						\
	vaf.va = &args;						\
								\
	__dev_printk(kern_level, dev, &vaf);			\
								\
	va_end(args);						\
}								\
EXPORT_SYMBOL(func);

define_dev_printk_level(_dev_emerg, KERN_EMERG);
define_dev_printk_level(_dev_alert, KERN_ALERT);
define_dev_printk_level(_dev_crit, KERN_CRIT);
define_dev_printk_level(_dev_err, KERN_ERR);
define_dev_printk_level(_dev_warn, KERN_WARNING);
define_dev_printk_level(_dev_notice, KERN_NOTICE);
define_dev_printk_level(_dev_info, KERN_INFO);

#endif

 
int dev_err_probe(const struct device *dev, int err, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;

	if (err != -EPROBE_DEFER) {
		dev_err(dev, "error %pe: %pV", ERR_PTR(err), &vaf);
	} else {
		device_set_deferred_probe_reason(dev, &vaf);
		dev_dbg(dev, "error %pe: %pV", ERR_PTR(err), &vaf);
	}

	va_end(args);

	return err;
}
EXPORT_SYMBOL_GPL(dev_err_probe);

static inline bool fwnode_is_primary(struct fwnode_handle *fwnode)
{
	return fwnode && !IS_ERR(fwnode->secondary);
}

 
void set_primary_fwnode(struct device *dev, struct fwnode_handle *fwnode)
{
	struct device *parent = dev->parent;
	struct fwnode_handle *fn = dev->fwnode;

	if (fwnode) {
		if (fwnode_is_primary(fn))
			fn = fn->secondary;

		if (fn) {
			WARN_ON(fwnode->secondary);
			fwnode->secondary = fn;
		}
		dev->fwnode = fwnode;
	} else {
		if (fwnode_is_primary(fn)) {
			dev->fwnode = fn->secondary;

			 
			if (parent && fn == parent->fwnode)
				return;

			 
			fn->secondary = NULL;
		} else {
			dev->fwnode = NULL;
		}
	}
}
EXPORT_SYMBOL_GPL(set_primary_fwnode);

 
void set_secondary_fwnode(struct device *dev, struct fwnode_handle *fwnode)
{
	if (fwnode)
		fwnode->secondary = ERR_PTR(-ENODEV);

	if (fwnode_is_primary(dev->fwnode))
		dev->fwnode->secondary = fwnode;
	else
		dev->fwnode = fwnode;
}
EXPORT_SYMBOL_GPL(set_secondary_fwnode);

 
void device_set_of_node_from_dev(struct device *dev, const struct device *dev2)
{
	of_node_put(dev->of_node);
	dev->of_node = of_node_get(dev2->of_node);
	dev->of_node_reused = true;
}
EXPORT_SYMBOL_GPL(device_set_of_node_from_dev);

void device_set_node(struct device *dev, struct fwnode_handle *fwnode)
{
	dev->fwnode = fwnode;
	dev->of_node = to_of_node(fwnode);
}
EXPORT_SYMBOL_GPL(device_set_node);

int device_match_name(struct device *dev, const void *name)
{
	return sysfs_streq(dev_name(dev), name);
}
EXPORT_SYMBOL_GPL(device_match_name);

int device_match_of_node(struct device *dev, const void *np)
{
	return dev->of_node == np;
}
EXPORT_SYMBOL_GPL(device_match_of_node);

int device_match_fwnode(struct device *dev, const void *fwnode)
{
	return dev_fwnode(dev) == fwnode;
}
EXPORT_SYMBOL_GPL(device_match_fwnode);

int device_match_devt(struct device *dev, const void *pdevt)
{
	return dev->devt == *(dev_t *)pdevt;
}
EXPORT_SYMBOL_GPL(device_match_devt);

int device_match_acpi_dev(struct device *dev, const void *adev)
{
	return ACPI_COMPANION(dev) == adev;
}
EXPORT_SYMBOL(device_match_acpi_dev);

int device_match_acpi_handle(struct device *dev, const void *handle)
{
	return ACPI_HANDLE(dev) == handle;
}
EXPORT_SYMBOL(device_match_acpi_handle);

int device_match_any(struct device *dev, const void *unused)
{
	return 1;
}
EXPORT_SYMBOL_GPL(device_match_any);
