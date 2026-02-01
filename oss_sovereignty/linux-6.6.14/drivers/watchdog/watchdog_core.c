
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>	 
#include <linux/types.h>	 
#include <linux/errno.h>	 
#include <linux/kernel.h>	 
#include <linux/reboot.h>	 
#include <linux/watchdog.h>	 
#include <linux/init.h>		 
#include <linux/idr.h>		 
#include <linux/err.h>		 
#include <linux/of.h>		 
#include <linux/suspend.h>

#include "watchdog_core.h"	 

#define CREATE_TRACE_POINTS
#include <trace/events/watchdog.h>

static DEFINE_IDA(watchdog_ida);

static int stop_on_reboot = -1;
module_param(stop_on_reboot, int, 0444);
MODULE_PARM_DESC(stop_on_reboot, "Stop watchdogs on reboot (0=keep watching, 1=stop)");

 

static DEFINE_MUTEX(wtd_deferred_reg_mutex);
static LIST_HEAD(wtd_deferred_reg_list);
static bool wtd_deferred_reg_done;

static void watchdog_deferred_registration_add(struct watchdog_device *wdd)
{
	list_add_tail(&wdd->deferred,
		      &wtd_deferred_reg_list);
}

static void watchdog_deferred_registration_del(struct watchdog_device *wdd)
{
	struct list_head *p, *n;
	struct watchdog_device *wdd_tmp;

	list_for_each_safe(p, n, &wtd_deferred_reg_list) {
		wdd_tmp = list_entry(p, struct watchdog_device,
				     deferred);
		if (wdd_tmp == wdd) {
			list_del(&wdd_tmp->deferred);
			break;
		}
	}
}

static void watchdog_check_min_max_timeout(struct watchdog_device *wdd)
{
	 
	if (!wdd->max_hw_heartbeat_ms && wdd->min_timeout > wdd->max_timeout) {
		pr_info("Invalid min and max timeout values, resetting to 0!\n");
		wdd->min_timeout = 0;
		wdd->max_timeout = 0;
	}
}

 
int watchdog_init_timeout(struct watchdog_device *wdd,
				unsigned int timeout_parm, struct device *dev)
{
	const char *dev_str = wdd->parent ? dev_name(wdd->parent) :
			      (const char *)wdd->info->identity;
	unsigned int t = 0;
	int ret = 0;

	watchdog_check_min_max_timeout(wdd);

	 
	if (timeout_parm) {
		if (!watchdog_timeout_invalid(wdd, timeout_parm)) {
			wdd->timeout = timeout_parm;
			return 0;
		}
		pr_err("%s: driver supplied timeout (%u) out of range\n",
			dev_str, timeout_parm);
		ret = -EINVAL;
	}

	 
	if (dev && dev->of_node &&
	    of_property_read_u32(dev->of_node, "timeout-sec", &t) == 0) {
		if (t && !watchdog_timeout_invalid(wdd, t)) {
			wdd->timeout = t;
			return 0;
		}
		pr_err("%s: DT supplied timeout (%u) out of range\n", dev_str, t);
		ret = -EINVAL;
	}

	if (ret < 0 && wdd->timeout)
		pr_warn("%s: falling back to default timeout (%u)\n", dev_str,
			wdd->timeout);

	return ret;
}
EXPORT_SYMBOL_GPL(watchdog_init_timeout);

static int watchdog_reboot_notifier(struct notifier_block *nb,
				    unsigned long code, void *data)
{
	struct watchdog_device *wdd;

	wdd = container_of(nb, struct watchdog_device, reboot_nb);
	if (code == SYS_DOWN || code == SYS_HALT || code == SYS_POWER_OFF) {
		if (watchdog_hw_running(wdd)) {
			int ret;

			ret = wdd->ops->stop(wdd);
			trace_watchdog_stop(wdd, ret);
			if (ret)
				return NOTIFY_BAD;
		}
	}

	return NOTIFY_DONE;
}

static int watchdog_restart_notifier(struct notifier_block *nb,
				     unsigned long action, void *data)
{
	struct watchdog_device *wdd = container_of(nb, struct watchdog_device,
						   restart_nb);

	int ret;

	ret = wdd->ops->restart(wdd, action, data);
	if (ret)
		return NOTIFY_BAD;

	return NOTIFY_DONE;
}

static int watchdog_pm_notifier(struct notifier_block *nb, unsigned long mode,
				void *data)
{
	struct watchdog_device *wdd;
	int ret = 0;

	wdd = container_of(nb, struct watchdog_device, pm_nb);

	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
	case PM_SUSPEND_PREPARE:
		ret = watchdog_dev_suspend(wdd);
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		ret = watchdog_dev_resume(wdd);
		break;
	}

	if (ret)
		return NOTIFY_BAD;

	return NOTIFY_DONE;
}

 
void watchdog_set_restart_priority(struct watchdog_device *wdd, int priority)
{
	wdd->restart_nb.priority = priority;
}
EXPORT_SYMBOL_GPL(watchdog_set_restart_priority);

static int __watchdog_register_device(struct watchdog_device *wdd)
{
	int ret, id = -1;

	if (wdd == NULL || wdd->info == NULL || wdd->ops == NULL)
		return -EINVAL;

	 
	if (!wdd->ops->start || (!wdd->ops->stop && !wdd->max_hw_heartbeat_ms))
		return -EINVAL;

	watchdog_check_min_max_timeout(wdd);

	 

	 
	if (wdd->parent) {
		ret = of_alias_get_id(wdd->parent->of_node, "watchdog");
		if (ret >= 0)
			id = ida_simple_get(&watchdog_ida, ret,
					    ret + 1, GFP_KERNEL);
	}

	if (id < 0)
		id = ida_simple_get(&watchdog_ida, 0, MAX_DOGS, GFP_KERNEL);

	if (id < 0)
		return id;
	wdd->id = id;

	ret = watchdog_dev_register(wdd);
	if (ret) {
		ida_simple_remove(&watchdog_ida, id);
		if (!(id == 0 && ret == -EBUSY))
			return ret;

		 
		id = ida_simple_get(&watchdog_ida, 1, MAX_DOGS, GFP_KERNEL);
		if (id < 0)
			return id;
		wdd->id = id;

		ret = watchdog_dev_register(wdd);
		if (ret) {
			ida_simple_remove(&watchdog_ida, id);
			return ret;
		}
	}

	 
	if (stop_on_reboot != -1) {
		if (stop_on_reboot)
			set_bit(WDOG_STOP_ON_REBOOT, &wdd->status);
		else
			clear_bit(WDOG_STOP_ON_REBOOT, &wdd->status);
	}

	if (test_bit(WDOG_STOP_ON_REBOOT, &wdd->status)) {
		if (!wdd->ops->stop)
			pr_warn("watchdog%d: stop_on_reboot not supported\n", wdd->id);
		else {
			wdd->reboot_nb.notifier_call = watchdog_reboot_notifier;

			ret = register_reboot_notifier(&wdd->reboot_nb);
			if (ret) {
				pr_err("watchdog%d: Cannot register reboot notifier (%d)\n",
					wdd->id, ret);
				watchdog_dev_unregister(wdd);
				ida_simple_remove(&watchdog_ida, id);
				return ret;
			}
		}
	}

	if (wdd->ops->restart) {
		wdd->restart_nb.notifier_call = watchdog_restart_notifier;

		ret = register_restart_handler(&wdd->restart_nb);
		if (ret)
			pr_warn("watchdog%d: Cannot register restart handler (%d)\n",
				wdd->id, ret);
	}

	if (test_bit(WDOG_NO_PING_ON_SUSPEND, &wdd->status)) {
		wdd->pm_nb.notifier_call = watchdog_pm_notifier;

		ret = register_pm_notifier(&wdd->pm_nb);
		if (ret)
			pr_warn("watchdog%d: Cannot register pm handler (%d)\n",
				wdd->id, ret);
	}

	return 0;
}

 

int watchdog_register_device(struct watchdog_device *wdd)
{
	const char *dev_str;
	int ret = 0;

	mutex_lock(&wtd_deferred_reg_mutex);
	if (wtd_deferred_reg_done)
		ret = __watchdog_register_device(wdd);
	else
		watchdog_deferred_registration_add(wdd);
	mutex_unlock(&wtd_deferred_reg_mutex);

	if (ret) {
		dev_str = wdd->parent ? dev_name(wdd->parent) :
			  (const char *)wdd->info->identity;
		pr_err("%s: failed to register watchdog device (err = %d)\n",
			dev_str, ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(watchdog_register_device);

static void __watchdog_unregister_device(struct watchdog_device *wdd)
{
	if (wdd == NULL)
		return;

	if (wdd->ops->restart)
		unregister_restart_handler(&wdd->restart_nb);

	if (test_bit(WDOG_STOP_ON_REBOOT, &wdd->status))
		unregister_reboot_notifier(&wdd->reboot_nb);

	watchdog_dev_unregister(wdd);
	ida_simple_remove(&watchdog_ida, wdd->id);
}

 

void watchdog_unregister_device(struct watchdog_device *wdd)
{
	mutex_lock(&wtd_deferred_reg_mutex);
	if (wtd_deferred_reg_done)
		__watchdog_unregister_device(wdd);
	else
		watchdog_deferred_registration_del(wdd);
	mutex_unlock(&wtd_deferred_reg_mutex);
}

EXPORT_SYMBOL_GPL(watchdog_unregister_device);

static void devm_watchdog_unregister_device(struct device *dev, void *res)
{
	watchdog_unregister_device(*(struct watchdog_device **)res);
}

 
int devm_watchdog_register_device(struct device *dev,
				struct watchdog_device *wdd)
{
	struct watchdog_device **rcwdd;
	int ret;

	rcwdd = devres_alloc(devm_watchdog_unregister_device, sizeof(*rcwdd),
			     GFP_KERNEL);
	if (!rcwdd)
		return -ENOMEM;

	ret = watchdog_register_device(wdd);
	if (!ret) {
		*rcwdd = wdd;
		devres_add(dev, rcwdd);
	} else {
		devres_free(rcwdd);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(devm_watchdog_register_device);

static int __init watchdog_deferred_registration(void)
{
	mutex_lock(&wtd_deferred_reg_mutex);
	wtd_deferred_reg_done = true;
	while (!list_empty(&wtd_deferred_reg_list)) {
		struct watchdog_device *wdd;

		wdd = list_first_entry(&wtd_deferred_reg_list,
				       struct watchdog_device, deferred);
		list_del(&wdd->deferred);
		__watchdog_register_device(wdd);
	}
	mutex_unlock(&wtd_deferred_reg_mutex);
	return 0;
}

static int __init watchdog_init(void)
{
	int err;

	err = watchdog_dev_init();
	if (err < 0)
		return err;

	watchdog_deferred_registration();
	return 0;
}

static void __exit watchdog_exit(void)
{
	watchdog_dev_exit();
	ida_destroy(&watchdog_ida);
}

subsys_initcall_sync(watchdog_init);
module_exit(watchdog_exit);

MODULE_AUTHOR("Alan Cox <alan@lxorguk.ukuu.org.uk>");
MODULE_AUTHOR("Wim Van Sebroeck <wim@iguana.be>");
MODULE_DESCRIPTION("WatchDog Timer Driver Core");
MODULE_LICENSE("GPL");
