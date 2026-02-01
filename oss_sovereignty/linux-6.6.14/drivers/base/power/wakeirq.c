
 
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeirq.h>

#include "power.h"

 
static int dev_pm_attach_wake_irq(struct device *dev, struct wake_irq *wirq)
{
	unsigned long flags;

	if (!dev || !wirq)
		return -EINVAL;

	spin_lock_irqsave(&dev->power.lock, flags);
	if (dev_WARN_ONCE(dev, dev->power.wakeirq,
			  "wake irq already initialized\n")) {
		spin_unlock_irqrestore(&dev->power.lock, flags);
		return -EEXIST;
	}

	dev->power.wakeirq = wirq;
	device_wakeup_attach_irq(dev, wirq);

	spin_unlock_irqrestore(&dev->power.lock, flags);
	return 0;
}

 
int dev_pm_set_wake_irq(struct device *dev, int irq)
{
	struct wake_irq *wirq;
	int err;

	if (irq < 0)
		return -EINVAL;

	wirq = kzalloc(sizeof(*wirq), GFP_KERNEL);
	if (!wirq)
		return -ENOMEM;

	wirq->dev = dev;
	wirq->irq = irq;

	err = dev_pm_attach_wake_irq(dev, wirq);
	if (err)
		kfree(wirq);

	return err;
}
EXPORT_SYMBOL_GPL(dev_pm_set_wake_irq);

 
void dev_pm_clear_wake_irq(struct device *dev)
{
	struct wake_irq *wirq = dev->power.wakeirq;
	unsigned long flags;

	if (!wirq)
		return;

	spin_lock_irqsave(&dev->power.lock, flags);
	device_wakeup_detach_irq(dev);
	dev->power.wakeirq = NULL;
	spin_unlock_irqrestore(&dev->power.lock, flags);

	if (wirq->status & WAKE_IRQ_DEDICATED_ALLOCATED) {
		free_irq(wirq->irq, wirq);
		wirq->status &= ~WAKE_IRQ_DEDICATED_MASK;
	}
	kfree(wirq->name);
	kfree(wirq);
}
EXPORT_SYMBOL_GPL(dev_pm_clear_wake_irq);

 
static irqreturn_t handle_threaded_wake_irq(int irq, void *_wirq)
{
	struct wake_irq *wirq = _wirq;
	int res;

	 
	if (irqd_is_wakeup_set(irq_get_irq_data(irq))) {
		pm_wakeup_event(wirq->dev, 0);

		return IRQ_HANDLED;
	}

	 
	res = pm_runtime_resume(wirq->dev);
	if (res < 0)
		dev_warn(wirq->dev,
			 "wake IRQ with no resume: %i\n", res);

	return IRQ_HANDLED;
}

static int __dev_pm_set_dedicated_wake_irq(struct device *dev, int irq, unsigned int flag)
{
	struct wake_irq *wirq;
	int err;

	if (irq < 0)
		return -EINVAL;

	wirq = kzalloc(sizeof(*wirq), GFP_KERNEL);
	if (!wirq)
		return -ENOMEM;

	wirq->name = kasprintf(GFP_KERNEL, "%s:wakeup", dev_name(dev));
	if (!wirq->name) {
		err = -ENOMEM;
		goto err_free;
	}

	wirq->dev = dev;
	wirq->irq = irq;

	 
	irq_set_status_flags(irq, IRQ_DISABLE_UNLAZY);

	 
	err = request_threaded_irq(irq, NULL, handle_threaded_wake_irq,
				   IRQF_ONESHOT | IRQF_NO_AUTOEN,
				   wirq->name, wirq);
	if (err)
		goto err_free_name;

	err = dev_pm_attach_wake_irq(dev, wirq);
	if (err)
		goto err_free_irq;

	wirq->status = WAKE_IRQ_DEDICATED_ALLOCATED | flag;

	return err;

err_free_irq:
	free_irq(irq, wirq);
err_free_name:
	kfree(wirq->name);
err_free:
	kfree(wirq);

	return err;
}

 
int dev_pm_set_dedicated_wake_irq(struct device *dev, int irq)
{
	return __dev_pm_set_dedicated_wake_irq(dev, irq, 0);
}
EXPORT_SYMBOL_GPL(dev_pm_set_dedicated_wake_irq);

 
int dev_pm_set_dedicated_wake_irq_reverse(struct device *dev, int irq)
{
	return __dev_pm_set_dedicated_wake_irq(dev, irq, WAKE_IRQ_DEDICATED_REVERSE);
}
EXPORT_SYMBOL_GPL(dev_pm_set_dedicated_wake_irq_reverse);

 
void dev_pm_enable_wake_irq_check(struct device *dev,
				  bool can_change_status)
{
	struct wake_irq *wirq = dev->power.wakeirq;

	if (!wirq || !(wirq->status & WAKE_IRQ_DEDICATED_MASK))
		return;

	if (likely(wirq->status & WAKE_IRQ_DEDICATED_MANAGED)) {
		goto enable;
	} else if (can_change_status) {
		wirq->status |= WAKE_IRQ_DEDICATED_MANAGED;
		goto enable;
	}

	return;

enable:
	if (!can_change_status || !(wirq->status & WAKE_IRQ_DEDICATED_REVERSE)) {
		enable_irq(wirq->irq);
		wirq->status |= WAKE_IRQ_DEDICATED_ENABLED;
	}
}

 
void dev_pm_disable_wake_irq_check(struct device *dev, bool cond_disable)
{
	struct wake_irq *wirq = dev->power.wakeirq;

	if (!wirq || !(wirq->status & WAKE_IRQ_DEDICATED_MASK))
		return;

	if (cond_disable && (wirq->status & WAKE_IRQ_DEDICATED_REVERSE))
		return;

	if (wirq->status & WAKE_IRQ_DEDICATED_MANAGED) {
		wirq->status &= ~WAKE_IRQ_DEDICATED_ENABLED;
		disable_irq_nosync(wirq->irq);
	}
}

 
void dev_pm_enable_wake_irq_complete(struct device *dev)
{
	struct wake_irq *wirq = dev->power.wakeirq;

	if (!wirq || !(wirq->status & WAKE_IRQ_DEDICATED_MASK))
		return;

	if (wirq->status & WAKE_IRQ_DEDICATED_MANAGED &&
	    wirq->status & WAKE_IRQ_DEDICATED_REVERSE)
		enable_irq(wirq->irq);
}

 
void dev_pm_arm_wake_irq(struct wake_irq *wirq)
{
	if (!wirq)
		return;

	if (device_may_wakeup(wirq->dev)) {
		if (wirq->status & WAKE_IRQ_DEDICATED_ALLOCATED &&
		    !(wirq->status & WAKE_IRQ_DEDICATED_ENABLED))
			enable_irq(wirq->irq);

		enable_irq_wake(wirq->irq);
	}
}

 
void dev_pm_disarm_wake_irq(struct wake_irq *wirq)
{
	if (!wirq)
		return;

	if (device_may_wakeup(wirq->dev)) {
		disable_irq_wake(wirq->irq);

		if (wirq->status & WAKE_IRQ_DEDICATED_ALLOCATED &&
		    !(wirq->status & WAKE_IRQ_DEDICATED_ENABLED))
			disable_irq_nosync(wirq->irq);
	}
}
