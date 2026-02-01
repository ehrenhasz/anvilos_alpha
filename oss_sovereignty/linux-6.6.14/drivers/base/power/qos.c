
 

#include <linux/pm_qos.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/export.h>
#include <linux/pm_runtime.h>
#include <linux/err.h>
#include <trace/events/power.h>

#include "power.h"

static DEFINE_MUTEX(dev_pm_qos_mtx);
static DEFINE_MUTEX(dev_pm_qos_sysfs_mtx);

 
enum pm_qos_flags_status __dev_pm_qos_flags(struct device *dev, s32 mask)
{
	struct dev_pm_qos *qos = dev->power.qos;
	struct pm_qos_flags *pqf;
	s32 val;

	lockdep_assert_held(&dev->power.lock);

	if (IS_ERR_OR_NULL(qos))
		return PM_QOS_FLAGS_UNDEFINED;

	pqf = &qos->flags;
	if (list_empty(&pqf->list))
		return PM_QOS_FLAGS_UNDEFINED;

	val = pqf->effective_flags & mask;
	if (val)
		return (val == mask) ? PM_QOS_FLAGS_ALL : PM_QOS_FLAGS_SOME;

	return PM_QOS_FLAGS_NONE;
}

 
enum pm_qos_flags_status dev_pm_qos_flags(struct device *dev, s32 mask)
{
	unsigned long irqflags;
	enum pm_qos_flags_status ret;

	spin_lock_irqsave(&dev->power.lock, irqflags);
	ret = __dev_pm_qos_flags(dev, mask);
	spin_unlock_irqrestore(&dev->power.lock, irqflags);

	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_qos_flags);

 
s32 __dev_pm_qos_resume_latency(struct device *dev)
{
	lockdep_assert_held(&dev->power.lock);

	return dev_pm_qos_raw_resume_latency(dev);
}

 
s32 dev_pm_qos_read_value(struct device *dev, enum dev_pm_qos_req_type type)
{
	struct dev_pm_qos *qos = dev->power.qos;
	unsigned long flags;
	s32 ret;

	spin_lock_irqsave(&dev->power.lock, flags);

	switch (type) {
	case DEV_PM_QOS_RESUME_LATENCY:
		ret = IS_ERR_OR_NULL(qos) ? PM_QOS_RESUME_LATENCY_NO_CONSTRAINT
			: pm_qos_read_value(&qos->resume_latency);
		break;
	case DEV_PM_QOS_MIN_FREQUENCY:
		ret = IS_ERR_OR_NULL(qos) ? PM_QOS_MIN_FREQUENCY_DEFAULT_VALUE
			: freq_qos_read_value(&qos->freq, FREQ_QOS_MIN);
		break;
	case DEV_PM_QOS_MAX_FREQUENCY:
		ret = IS_ERR_OR_NULL(qos) ? PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE
			: freq_qos_read_value(&qos->freq, FREQ_QOS_MAX);
		break;
	default:
		WARN_ON(1);
		ret = 0;
	}

	spin_unlock_irqrestore(&dev->power.lock, flags);

	return ret;
}

 
static int apply_constraint(struct dev_pm_qos_request *req,
			    enum pm_qos_req_action action, s32 value)
{
	struct dev_pm_qos *qos = req->dev->power.qos;
	int ret;

	switch(req->type) {
	case DEV_PM_QOS_RESUME_LATENCY:
		if (WARN_ON(action != PM_QOS_REMOVE_REQ && value < 0))
			value = 0;

		ret = pm_qos_update_target(&qos->resume_latency,
					   &req->data.pnode, action, value);
		break;
	case DEV_PM_QOS_LATENCY_TOLERANCE:
		ret = pm_qos_update_target(&qos->latency_tolerance,
					   &req->data.pnode, action, value);
		if (ret) {
			value = pm_qos_read_value(&qos->latency_tolerance);
			req->dev->power.set_latency_tolerance(req->dev, value);
		}
		break;
	case DEV_PM_QOS_MIN_FREQUENCY:
	case DEV_PM_QOS_MAX_FREQUENCY:
		ret = freq_qos_apply(&req->data.freq, action, value);
		break;
	case DEV_PM_QOS_FLAGS:
		ret = pm_qos_update_flags(&qos->flags, &req->data.flr,
					  action, value);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

 
static int dev_pm_qos_constraints_allocate(struct device *dev)
{
	struct dev_pm_qos *qos;
	struct pm_qos_constraints *c;
	struct blocking_notifier_head *n;

	qos = kzalloc(sizeof(*qos), GFP_KERNEL);
	if (!qos)
		return -ENOMEM;

	n = kzalloc(3 * sizeof(*n), GFP_KERNEL);
	if (!n) {
		kfree(qos);
		return -ENOMEM;
	}

	c = &qos->resume_latency;
	plist_head_init(&c->list);
	c->target_value = PM_QOS_RESUME_LATENCY_DEFAULT_VALUE;
	c->default_value = PM_QOS_RESUME_LATENCY_DEFAULT_VALUE;
	c->no_constraint_value = PM_QOS_RESUME_LATENCY_NO_CONSTRAINT;
	c->type = PM_QOS_MIN;
	c->notifiers = n;
	BLOCKING_INIT_NOTIFIER_HEAD(n);

	c = &qos->latency_tolerance;
	plist_head_init(&c->list);
	c->target_value = PM_QOS_LATENCY_TOLERANCE_DEFAULT_VALUE;
	c->default_value = PM_QOS_LATENCY_TOLERANCE_DEFAULT_VALUE;
	c->no_constraint_value = PM_QOS_LATENCY_TOLERANCE_NO_CONSTRAINT;
	c->type = PM_QOS_MIN;

	freq_constraints_init(&qos->freq);

	INIT_LIST_HEAD(&qos->flags.list);

	spin_lock_irq(&dev->power.lock);
	dev->power.qos = qos;
	spin_unlock_irq(&dev->power.lock);

	return 0;
}

static void __dev_pm_qos_hide_latency_limit(struct device *dev);
static void __dev_pm_qos_hide_flags(struct device *dev);

 
void dev_pm_qos_constraints_destroy(struct device *dev)
{
	struct dev_pm_qos *qos;
	struct dev_pm_qos_request *req, *tmp;
	struct pm_qos_constraints *c;
	struct pm_qos_flags *f;

	mutex_lock(&dev_pm_qos_sysfs_mtx);

	 
	pm_qos_sysfs_remove_resume_latency(dev);
	pm_qos_sysfs_remove_flags(dev);

	mutex_lock(&dev_pm_qos_mtx);

	__dev_pm_qos_hide_latency_limit(dev);
	__dev_pm_qos_hide_flags(dev);

	qos = dev->power.qos;
	if (!qos)
		goto out;

	 
	c = &qos->resume_latency;
	plist_for_each_entry_safe(req, tmp, &c->list, data.pnode) {
		 
		apply_constraint(req, PM_QOS_REMOVE_REQ, PM_QOS_DEFAULT_VALUE);
		memset(req, 0, sizeof(*req));
	}

	c = &qos->latency_tolerance;
	plist_for_each_entry_safe(req, tmp, &c->list, data.pnode) {
		apply_constraint(req, PM_QOS_REMOVE_REQ, PM_QOS_DEFAULT_VALUE);
		memset(req, 0, sizeof(*req));
	}

	c = &qos->freq.min_freq;
	plist_for_each_entry_safe(req, tmp, &c->list, data.freq.pnode) {
		apply_constraint(req, PM_QOS_REMOVE_REQ,
				 PM_QOS_MIN_FREQUENCY_DEFAULT_VALUE);
		memset(req, 0, sizeof(*req));
	}

	c = &qos->freq.max_freq;
	plist_for_each_entry_safe(req, tmp, &c->list, data.freq.pnode) {
		apply_constraint(req, PM_QOS_REMOVE_REQ,
				 PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
		memset(req, 0, sizeof(*req));
	}

	f = &qos->flags;
	list_for_each_entry_safe(req, tmp, &f->list, data.flr.node) {
		apply_constraint(req, PM_QOS_REMOVE_REQ, PM_QOS_DEFAULT_VALUE);
		memset(req, 0, sizeof(*req));
	}

	spin_lock_irq(&dev->power.lock);
	dev->power.qos = ERR_PTR(-ENODEV);
	spin_unlock_irq(&dev->power.lock);

	kfree(qos->resume_latency.notifiers);
	kfree(qos);

 out:
	mutex_unlock(&dev_pm_qos_mtx);

	mutex_unlock(&dev_pm_qos_sysfs_mtx);
}

static bool dev_pm_qos_invalid_req_type(struct device *dev,
					enum dev_pm_qos_req_type type)
{
	return type == DEV_PM_QOS_LATENCY_TOLERANCE &&
	       !dev->power.set_latency_tolerance;
}

static int __dev_pm_qos_add_request(struct device *dev,
				    struct dev_pm_qos_request *req,
				    enum dev_pm_qos_req_type type, s32 value)
{
	int ret = 0;

	if (!dev || !req || dev_pm_qos_invalid_req_type(dev, type))
		return -EINVAL;

	if (WARN(dev_pm_qos_request_active(req),
		 "%s() called for already added request\n", __func__))
		return -EINVAL;

	if (IS_ERR(dev->power.qos))
		ret = -ENODEV;
	else if (!dev->power.qos)
		ret = dev_pm_qos_constraints_allocate(dev);

	trace_dev_pm_qos_add_request(dev_name(dev), type, value);
	if (ret)
		return ret;

	req->dev = dev;
	req->type = type;
	if (req->type == DEV_PM_QOS_MIN_FREQUENCY)
		ret = freq_qos_add_request(&dev->power.qos->freq,
					   &req->data.freq,
					   FREQ_QOS_MIN, value);
	else if (req->type == DEV_PM_QOS_MAX_FREQUENCY)
		ret = freq_qos_add_request(&dev->power.qos->freq,
					   &req->data.freq,
					   FREQ_QOS_MAX, value);
	else
		ret = apply_constraint(req, PM_QOS_ADD_REQ, value);

	return ret;
}

 
int dev_pm_qos_add_request(struct device *dev, struct dev_pm_qos_request *req,
			   enum dev_pm_qos_req_type type, s32 value)
{
	int ret;

	mutex_lock(&dev_pm_qos_mtx);
	ret = __dev_pm_qos_add_request(dev, req, type, value);
	mutex_unlock(&dev_pm_qos_mtx);
	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_qos_add_request);

 
static int __dev_pm_qos_update_request(struct dev_pm_qos_request *req,
				       s32 new_value)
{
	s32 curr_value;
	int ret = 0;

	if (!req)  
		return -EINVAL;

	if (WARN(!dev_pm_qos_request_active(req),
		 "%s() called for unknown object\n", __func__))
		return -EINVAL;

	if (IS_ERR_OR_NULL(req->dev->power.qos))
		return -ENODEV;

	switch(req->type) {
	case DEV_PM_QOS_RESUME_LATENCY:
	case DEV_PM_QOS_LATENCY_TOLERANCE:
		curr_value = req->data.pnode.prio;
		break;
	case DEV_PM_QOS_MIN_FREQUENCY:
	case DEV_PM_QOS_MAX_FREQUENCY:
		curr_value = req->data.freq.pnode.prio;
		break;
	case DEV_PM_QOS_FLAGS:
		curr_value = req->data.flr.flags;
		break;
	default:
		return -EINVAL;
	}

	trace_dev_pm_qos_update_request(dev_name(req->dev), req->type,
					new_value);
	if (curr_value != new_value)
		ret = apply_constraint(req, PM_QOS_UPDATE_REQ, new_value);

	return ret;
}

 
int dev_pm_qos_update_request(struct dev_pm_qos_request *req, s32 new_value)
{
	int ret;

	mutex_lock(&dev_pm_qos_mtx);
	ret = __dev_pm_qos_update_request(req, new_value);
	mutex_unlock(&dev_pm_qos_mtx);
	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_qos_update_request);

static int __dev_pm_qos_remove_request(struct dev_pm_qos_request *req)
{
	int ret;

	if (!req)  
		return -EINVAL;

	if (WARN(!dev_pm_qos_request_active(req),
		 "%s() called for unknown object\n", __func__))
		return -EINVAL;

	if (IS_ERR_OR_NULL(req->dev->power.qos))
		return -ENODEV;

	trace_dev_pm_qos_remove_request(dev_name(req->dev), req->type,
					PM_QOS_DEFAULT_VALUE);
	ret = apply_constraint(req, PM_QOS_REMOVE_REQ, PM_QOS_DEFAULT_VALUE);
	memset(req, 0, sizeof(*req));
	return ret;
}

 
int dev_pm_qos_remove_request(struct dev_pm_qos_request *req)
{
	int ret;

	mutex_lock(&dev_pm_qos_mtx);
	ret = __dev_pm_qos_remove_request(req);
	mutex_unlock(&dev_pm_qos_mtx);
	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_qos_remove_request);

 
int dev_pm_qos_add_notifier(struct device *dev, struct notifier_block *notifier,
			    enum dev_pm_qos_req_type type)
{
	int ret = 0;

	mutex_lock(&dev_pm_qos_mtx);

	if (IS_ERR(dev->power.qos))
		ret = -ENODEV;
	else if (!dev->power.qos)
		ret = dev_pm_qos_constraints_allocate(dev);

	if (ret)
		goto unlock;

	switch (type) {
	case DEV_PM_QOS_RESUME_LATENCY:
		ret = blocking_notifier_chain_register(dev->power.qos->resume_latency.notifiers,
						       notifier);
		break;
	case DEV_PM_QOS_MIN_FREQUENCY:
		ret = freq_qos_add_notifier(&dev->power.qos->freq,
					    FREQ_QOS_MIN, notifier);
		break;
	case DEV_PM_QOS_MAX_FREQUENCY:
		ret = freq_qos_add_notifier(&dev->power.qos->freq,
					    FREQ_QOS_MAX, notifier);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

unlock:
	mutex_unlock(&dev_pm_qos_mtx);
	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_qos_add_notifier);

 
int dev_pm_qos_remove_notifier(struct device *dev,
			       struct notifier_block *notifier,
			       enum dev_pm_qos_req_type type)
{
	int ret = 0;

	mutex_lock(&dev_pm_qos_mtx);

	 
	if (IS_ERR_OR_NULL(dev->power.qos))
		goto unlock;

	switch (type) {
	case DEV_PM_QOS_RESUME_LATENCY:
		ret = blocking_notifier_chain_unregister(dev->power.qos->resume_latency.notifiers,
							 notifier);
		break;
	case DEV_PM_QOS_MIN_FREQUENCY:
		ret = freq_qos_remove_notifier(&dev->power.qos->freq,
					       FREQ_QOS_MIN, notifier);
		break;
	case DEV_PM_QOS_MAX_FREQUENCY:
		ret = freq_qos_remove_notifier(&dev->power.qos->freq,
					       FREQ_QOS_MAX, notifier);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

unlock:
	mutex_unlock(&dev_pm_qos_mtx);
	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_qos_remove_notifier);

 
int dev_pm_qos_add_ancestor_request(struct device *dev,
				    struct dev_pm_qos_request *req,
				    enum dev_pm_qos_req_type type, s32 value)
{
	struct device *ancestor = dev->parent;
	int ret = -ENODEV;

	switch (type) {
	case DEV_PM_QOS_RESUME_LATENCY:
		while (ancestor && !ancestor->power.ignore_children)
			ancestor = ancestor->parent;

		break;
	case DEV_PM_QOS_LATENCY_TOLERANCE:
		while (ancestor && !ancestor->power.set_latency_tolerance)
			ancestor = ancestor->parent;

		break;
	default:
		ancestor = NULL;
	}
	if (ancestor)
		ret = dev_pm_qos_add_request(ancestor, req, type, value);

	if (ret < 0)
		req->dev = NULL;

	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_qos_add_ancestor_request);

static void __dev_pm_qos_drop_user_request(struct device *dev,
					   enum dev_pm_qos_req_type type)
{
	struct dev_pm_qos_request *req = NULL;

	switch(type) {
	case DEV_PM_QOS_RESUME_LATENCY:
		req = dev->power.qos->resume_latency_req;
		dev->power.qos->resume_latency_req = NULL;
		break;
	case DEV_PM_QOS_LATENCY_TOLERANCE:
		req = dev->power.qos->latency_tolerance_req;
		dev->power.qos->latency_tolerance_req = NULL;
		break;
	case DEV_PM_QOS_FLAGS:
		req = dev->power.qos->flags_req;
		dev->power.qos->flags_req = NULL;
		break;
	default:
		WARN_ON(1);
		return;
	}
	__dev_pm_qos_remove_request(req);
	kfree(req);
}

static void dev_pm_qos_drop_user_request(struct device *dev,
					 enum dev_pm_qos_req_type type)
{
	mutex_lock(&dev_pm_qos_mtx);
	__dev_pm_qos_drop_user_request(dev, type);
	mutex_unlock(&dev_pm_qos_mtx);
}

 
int dev_pm_qos_expose_latency_limit(struct device *dev, s32 value)
{
	struct dev_pm_qos_request *req;
	int ret;

	if (!device_is_registered(dev) || value < 0)
		return -EINVAL;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	ret = dev_pm_qos_add_request(dev, req, DEV_PM_QOS_RESUME_LATENCY, value);
	if (ret < 0) {
		kfree(req);
		return ret;
	}

	mutex_lock(&dev_pm_qos_sysfs_mtx);

	mutex_lock(&dev_pm_qos_mtx);

	if (IS_ERR_OR_NULL(dev->power.qos))
		ret = -ENODEV;
	else if (dev->power.qos->resume_latency_req)
		ret = -EEXIST;

	if (ret < 0) {
		__dev_pm_qos_remove_request(req);
		kfree(req);
		mutex_unlock(&dev_pm_qos_mtx);
		goto out;
	}
	dev->power.qos->resume_latency_req = req;

	mutex_unlock(&dev_pm_qos_mtx);

	ret = pm_qos_sysfs_add_resume_latency(dev);
	if (ret)
		dev_pm_qos_drop_user_request(dev, DEV_PM_QOS_RESUME_LATENCY);

 out:
	mutex_unlock(&dev_pm_qos_sysfs_mtx);
	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_qos_expose_latency_limit);

static void __dev_pm_qos_hide_latency_limit(struct device *dev)
{
	if (!IS_ERR_OR_NULL(dev->power.qos) && dev->power.qos->resume_latency_req)
		__dev_pm_qos_drop_user_request(dev, DEV_PM_QOS_RESUME_LATENCY);
}

 
void dev_pm_qos_hide_latency_limit(struct device *dev)
{
	mutex_lock(&dev_pm_qos_sysfs_mtx);

	pm_qos_sysfs_remove_resume_latency(dev);

	mutex_lock(&dev_pm_qos_mtx);
	__dev_pm_qos_hide_latency_limit(dev);
	mutex_unlock(&dev_pm_qos_mtx);

	mutex_unlock(&dev_pm_qos_sysfs_mtx);
}
EXPORT_SYMBOL_GPL(dev_pm_qos_hide_latency_limit);

 
int dev_pm_qos_expose_flags(struct device *dev, s32 val)
{
	struct dev_pm_qos_request *req;
	int ret;

	if (!device_is_registered(dev))
		return -EINVAL;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	ret = dev_pm_qos_add_request(dev, req, DEV_PM_QOS_FLAGS, val);
	if (ret < 0) {
		kfree(req);
		return ret;
	}

	pm_runtime_get_sync(dev);
	mutex_lock(&dev_pm_qos_sysfs_mtx);

	mutex_lock(&dev_pm_qos_mtx);

	if (IS_ERR_OR_NULL(dev->power.qos))
		ret = -ENODEV;
	else if (dev->power.qos->flags_req)
		ret = -EEXIST;

	if (ret < 0) {
		__dev_pm_qos_remove_request(req);
		kfree(req);
		mutex_unlock(&dev_pm_qos_mtx);
		goto out;
	}
	dev->power.qos->flags_req = req;

	mutex_unlock(&dev_pm_qos_mtx);

	ret = pm_qos_sysfs_add_flags(dev);
	if (ret)
		dev_pm_qos_drop_user_request(dev, DEV_PM_QOS_FLAGS);

 out:
	mutex_unlock(&dev_pm_qos_sysfs_mtx);
	pm_runtime_put(dev);
	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_qos_expose_flags);

static void __dev_pm_qos_hide_flags(struct device *dev)
{
	if (!IS_ERR_OR_NULL(dev->power.qos) && dev->power.qos->flags_req)
		__dev_pm_qos_drop_user_request(dev, DEV_PM_QOS_FLAGS);
}

 
void dev_pm_qos_hide_flags(struct device *dev)
{
	pm_runtime_get_sync(dev);
	mutex_lock(&dev_pm_qos_sysfs_mtx);

	pm_qos_sysfs_remove_flags(dev);

	mutex_lock(&dev_pm_qos_mtx);
	__dev_pm_qos_hide_flags(dev);
	mutex_unlock(&dev_pm_qos_mtx);

	mutex_unlock(&dev_pm_qos_sysfs_mtx);
	pm_runtime_put(dev);
}
EXPORT_SYMBOL_GPL(dev_pm_qos_hide_flags);

 
int dev_pm_qos_update_flags(struct device *dev, s32 mask, bool set)
{
	s32 value;
	int ret;

	pm_runtime_get_sync(dev);
	mutex_lock(&dev_pm_qos_mtx);

	if (IS_ERR_OR_NULL(dev->power.qos) || !dev->power.qos->flags_req) {
		ret = -EINVAL;
		goto out;
	}

	value = dev_pm_qos_requested_flags(dev);
	if (set)
		value |= mask;
	else
		value &= ~mask;

	ret = __dev_pm_qos_update_request(dev->power.qos->flags_req, value);

 out:
	mutex_unlock(&dev_pm_qos_mtx);
	pm_runtime_put(dev);
	return ret;
}

 
s32 dev_pm_qos_get_user_latency_tolerance(struct device *dev)
{
	s32 ret;

	mutex_lock(&dev_pm_qos_mtx);
	ret = IS_ERR_OR_NULL(dev->power.qos)
		|| !dev->power.qos->latency_tolerance_req ?
			PM_QOS_LATENCY_TOLERANCE_NO_CONSTRAINT :
			dev->power.qos->latency_tolerance_req->data.pnode.prio;
	mutex_unlock(&dev_pm_qos_mtx);
	return ret;
}

 
int dev_pm_qos_update_user_latency_tolerance(struct device *dev, s32 val)
{
	int ret;

	mutex_lock(&dev_pm_qos_mtx);

	if (IS_ERR_OR_NULL(dev->power.qos)
	    || !dev->power.qos->latency_tolerance_req) {
		struct dev_pm_qos_request *req;

		if (val < 0) {
			if (val == PM_QOS_LATENCY_TOLERANCE_NO_CONSTRAINT)
				ret = 0;
			else
				ret = -EINVAL;
			goto out;
		}
		req = kzalloc(sizeof(*req), GFP_KERNEL);
		if (!req) {
			ret = -ENOMEM;
			goto out;
		}
		ret = __dev_pm_qos_add_request(dev, req, DEV_PM_QOS_LATENCY_TOLERANCE, val);
		if (ret < 0) {
			kfree(req);
			goto out;
		}
		dev->power.qos->latency_tolerance_req = req;
	} else {
		if (val < 0) {
			__dev_pm_qos_drop_user_request(dev, DEV_PM_QOS_LATENCY_TOLERANCE);
			ret = 0;
		} else {
			ret = __dev_pm_qos_update_request(dev->power.qos->latency_tolerance_req, val);
		}
	}

 out:
	mutex_unlock(&dev_pm_qos_mtx);
	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_qos_update_user_latency_tolerance);

 
int dev_pm_qos_expose_latency_tolerance(struct device *dev)
{
	int ret;

	if (!dev->power.set_latency_tolerance)
		return -EINVAL;

	mutex_lock(&dev_pm_qos_sysfs_mtx);
	ret = pm_qos_sysfs_add_latency_tolerance(dev);
	mutex_unlock(&dev_pm_qos_sysfs_mtx);

	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_qos_expose_latency_tolerance);

 
void dev_pm_qos_hide_latency_tolerance(struct device *dev)
{
	mutex_lock(&dev_pm_qos_sysfs_mtx);
	pm_qos_sysfs_remove_latency_tolerance(dev);
	mutex_unlock(&dev_pm_qos_sysfs_mtx);

	 
	pm_runtime_get_sync(dev);
	dev_pm_qos_update_user_latency_tolerance(dev,
		PM_QOS_LATENCY_TOLERANCE_NO_CONSTRAINT);
	pm_runtime_put(dev);
}
EXPORT_SYMBOL_GPL(dev_pm_qos_hide_latency_tolerance);
