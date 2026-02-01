








#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/regulator/driver.h>

#include "internal.h"

#define REGULATOR_FORCED_SAFETY_SHUTDOWN_WAIT_MS 10000

struct regulator_irq {
	struct regulator_irq_data rdata;
	struct regulator_irq_desc desc;
	int irq;
	int retry_cnt;
	struct delayed_work isr_work;
};

 
static void rdev_flag_err(struct regulator_dev *rdev, int err)
{
	spin_lock(&rdev->err_lock);
	rdev->cached_err |= err;
	spin_unlock(&rdev->err_lock);
}

static void rdev_clear_err(struct regulator_dev *rdev, int err)
{
	spin_lock(&rdev->err_lock);
	rdev->cached_err &= ~err;
	spin_unlock(&rdev->err_lock);
}

static void regulator_notifier_isr_work(struct work_struct *work)
{
	struct regulator_irq *h;
	struct regulator_irq_desc *d;
	struct regulator_irq_data *rid;
	int ret = 0;
	int tmo, i;
	int num_rdevs;

	h = container_of(work, struct regulator_irq,
			    isr_work.work);
	d = &h->desc;
	rid = &h->rdata;
	num_rdevs = rid->num_states;

reread:
	if (d->fatal_cnt && h->retry_cnt > d->fatal_cnt) {
		if (!d->die)
			return hw_protection_shutdown("Regulator HW failure? - no IC recovery",
						      REGULATOR_FORCED_SAFETY_SHUTDOWN_WAIT_MS);
		ret = d->die(rid);
		 
		if (ret)
			return hw_protection_shutdown("Regulator HW failure. IC recovery failed",
						      REGULATOR_FORCED_SAFETY_SHUTDOWN_WAIT_MS);

		 
		goto enable_out;
	}
	if (d->renable) {
		ret = d->renable(rid);

		if (ret == REGULATOR_FAILED_RETRY) {
			 
			h->retry_cnt++;
			if (!d->reread_ms)
				goto reread;

			tmo = d->reread_ms;
			goto reschedule;
		}

		if (ret) {
			 
			for (i = 0; i < num_rdevs; i++) {
				struct regulator_err_state *stat;
				struct regulator_dev *rdev;

				stat = &rid->states[i];
				rdev = stat->rdev;
				rdev_clear_err(rdev, (~stat->errors) &
						      stat->possible_errs);
			}
			h->retry_cnt++;
			 
			tmo = d->irq_off_ms;
			goto reschedule;
		}
	}

	 
	for (i = 0; i < num_rdevs; i++) {
		struct regulator_err_state *stat;
		struct regulator_dev *rdev;

		stat = &rid->states[i];
		rdev = stat->rdev;
		rdev_clear_err(rdev, stat->possible_errs);
	}

	 
	h->retry_cnt = 0;

enable_out:
	enable_irq(h->irq);

	return;

reschedule:
	if (!d->high_prio)
		mod_delayed_work(system_wq, &h->isr_work,
				 msecs_to_jiffies(tmo));
	else
		mod_delayed_work(system_highpri_wq, &h->isr_work,
				 msecs_to_jiffies(tmo));
}

static irqreturn_t regulator_notifier_isr(int irq, void *data)
{
	struct regulator_irq *h = data;
	struct regulator_irq_desc *d;
	struct regulator_irq_data *rid;
	unsigned long rdev_map = 0;
	int num_rdevs;
	int ret, i;

	d = &h->desc;
	rid = &h->rdata;
	num_rdevs = rid->num_states;

	if (d->fatal_cnt)
		h->retry_cnt++;

	 
	ret = d->map_event(irq, rid, &rdev_map);

	 
	if (unlikely(ret == REGULATOR_FAILED_RETRY))
		goto fail_out;

	h->retry_cnt = 0;
	 
	if (ret || !rdev_map)
		return IRQ_NONE;

	 
	if (d->skip_off) {
		for_each_set_bit(i, &rdev_map, num_rdevs) {
			struct regulator_dev *rdev;
			const struct regulator_ops *ops;

			rdev = rid->states[i].rdev;
			ops = rdev->desc->ops;

			 
			if (ops->is_enabled(rdev))
				break;
		}
		if (i == num_rdevs)
			return IRQ_NONE;
	}

	 
	if (d->irq_off_ms)
		disable_irq_nosync(irq);

	 
	for_each_set_bit(i, &rdev_map, num_rdevs) {
		struct regulator_err_state *stat;
		struct regulator_dev *rdev;

		stat = &rid->states[i];
		rdev = stat->rdev;

		rdev_dbg(rdev, "Sending regulator notification EVT 0x%lx\n",
			 stat->notifs);

		regulator_notifier_call_chain(rdev, stat->notifs, NULL);
		rdev_flag_err(rdev, stat->errors);
	}

	if (d->irq_off_ms) {
		if (!d->high_prio)
			schedule_delayed_work(&h->isr_work,
					      msecs_to_jiffies(d->irq_off_ms));
		else
			mod_delayed_work(system_highpri_wq,
					 &h->isr_work,
					 msecs_to_jiffies(d->irq_off_ms));
	}

	return IRQ_HANDLED;

fail_out:
	if (d->fatal_cnt && h->retry_cnt > d->fatal_cnt) {
		 
		if (!d->die) {
			hw_protection_shutdown("Regulator failure. Retry count exceeded",
					       REGULATOR_FORCED_SAFETY_SHUTDOWN_WAIT_MS);
		} else {
			ret = d->die(rid);
			 
			if (ret)
				hw_protection_shutdown("Regulator failure. Recovery failed",
						       REGULATOR_FORCED_SAFETY_SHUTDOWN_WAIT_MS);
		}
	}

	return IRQ_NONE;
}

static int init_rdev_state(struct device *dev, struct regulator_irq *h,
			   struct regulator_dev **rdev, int common_err,
			   int *rdev_err, int rdev_amount)
{
	int i;

	h->rdata.states = devm_kzalloc(dev, sizeof(*h->rdata.states) *
				       rdev_amount, GFP_KERNEL);
	if (!h->rdata.states)
		return -ENOMEM;

	h->rdata.num_states = rdev_amount;
	h->rdata.data = h->desc.data;

	for (i = 0; i < rdev_amount; i++) {
		h->rdata.states[i].possible_errs = common_err;
		if (rdev_err)
			h->rdata.states[i].possible_errs |= *rdev_err++;
		h->rdata.states[i].rdev = *rdev++;
	}

	return 0;
}

static void init_rdev_errors(struct regulator_irq *h)
{
	int i;

	for (i = 0; i < h->rdata.num_states; i++)
		if (h->rdata.states[i].possible_errs)
			h->rdata.states[i].rdev->use_cached_err = true;
}

 
void *regulator_irq_helper(struct device *dev,
			   const struct regulator_irq_desc *d, int irq,
			   int irq_flags, int common_errs, int *per_rdev_errs,
			   struct regulator_dev **rdev, int rdev_amount)
{
	struct regulator_irq *h;
	int ret;

	if (!rdev_amount || !d || !d->map_event || !d->name)
		return ERR_PTR(-EINVAL);

	h = devm_kzalloc(dev, sizeof(*h), GFP_KERNEL);
	if (!h)
		return ERR_PTR(-ENOMEM);

	h->irq = irq;
	h->desc = *d;

	ret = init_rdev_state(dev, h, rdev, common_errs, per_rdev_errs,
			      rdev_amount);
	if (ret)
		return ERR_PTR(ret);

	init_rdev_errors(h);

	if (h->desc.irq_off_ms)
		INIT_DELAYED_WORK(&h->isr_work, regulator_notifier_isr_work);

	ret = request_threaded_irq(h->irq, NULL, regulator_notifier_isr,
				   IRQF_ONESHOT | irq_flags, h->desc.name, h);
	if (ret) {
		dev_err(dev, "Failed to request IRQ %d\n", irq);

		return ERR_PTR(ret);
	}

	return h;
}
EXPORT_SYMBOL_GPL(regulator_irq_helper);

 
void regulator_irq_helper_cancel(void **handle)
{
	if (handle && *handle) {
		struct regulator_irq *h = *handle;

		free_irq(h->irq, h);
		if (h->desc.irq_off_ms)
			cancel_delayed_work_sync(&h->isr_work);

		h = NULL;
	}
}
EXPORT_SYMBOL_GPL(regulator_irq_helper_cancel);

 
int regulator_irq_map_event_simple(int irq, struct regulator_irq_data *rid,
			    unsigned long *dev_mask)
{
	int err = rid->states[0].possible_errs;

	*dev_mask = 1;
	 
	if (WARN_ON(rid->num_states != 1 || hweight32(err) != 1))
		return 0;

	rid->states[0].errors = err;
	rid->states[0].notifs = regulator_err2notif(err);

	return 0;
}
EXPORT_SYMBOL_GPL(regulator_irq_map_event_simple);

