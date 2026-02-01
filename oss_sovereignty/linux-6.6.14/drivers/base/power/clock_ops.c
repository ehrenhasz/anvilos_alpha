
 

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/pm.h>
#include <linux/pm_clock.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/of_clk.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>

#ifdef CONFIG_PM_CLK

enum pce_status {
	PCE_STATUS_NONE = 0,
	PCE_STATUS_ACQUIRED,
	PCE_STATUS_PREPARED,
	PCE_STATUS_ENABLED,
	PCE_STATUS_ERROR,
};

struct pm_clock_entry {
	struct list_head node;
	char *con_id;
	struct clk *clk;
	enum pce_status status;
	bool enabled_when_prepared;
};

 
static void pm_clk_list_lock(struct pm_subsys_data *psd)
	__acquires(&psd->lock)
{
	mutex_lock(&psd->clock_mutex);
	spin_lock_irq(&psd->lock);
}

 
static void pm_clk_list_unlock(struct pm_subsys_data *psd)
	__releases(&psd->lock)
{
	spin_unlock_irq(&psd->lock);
	mutex_unlock(&psd->clock_mutex);
}

 
static int pm_clk_op_lock(struct pm_subsys_data *psd, unsigned long *flags,
			  const char *fn)
	 
{
	bool atomic_context = in_atomic() || irqs_disabled();

try_again:
	spin_lock_irqsave(&psd->lock, *flags);
	if (!psd->clock_op_might_sleep) {
		 
		__release(&psd->lock);
		return 0;
	}

	 
	if (atomic_context) {
		pr_err("%s: atomic context with clock_ops_might_sleep = %d",
		       fn, psd->clock_op_might_sleep);
		spin_unlock_irqrestore(&psd->lock, *flags);
		might_sleep();
		return -EPERM;
	}

	 
	spin_unlock_irqrestore(&psd->lock, *flags);
	mutex_lock(&psd->clock_mutex);

	 
	if (likely(psd->clock_op_might_sleep))
		return 0;

	mutex_unlock(&psd->clock_mutex);
	goto try_again;
}

 
static void pm_clk_op_unlock(struct pm_subsys_data *psd, unsigned long *flags)
	 
{
	if (psd->clock_op_might_sleep) {
		mutex_unlock(&psd->clock_mutex);
	} else {
		 
		__acquire(&psd->lock);
		spin_unlock_irqrestore(&psd->lock, *flags);
	}
}

 
static inline void __pm_clk_enable(struct device *dev, struct pm_clock_entry *ce)
{
	int ret;

	switch (ce->status) {
	case PCE_STATUS_ACQUIRED:
		ret = clk_prepare_enable(ce->clk);
		break;
	case PCE_STATUS_PREPARED:
		ret = clk_enable(ce->clk);
		break;
	default:
		return;
	}
	if (!ret)
		ce->status = PCE_STATUS_ENABLED;
	else
		dev_err(dev, "%s: failed to enable clk %p, error %d\n",
			__func__, ce->clk, ret);
}

 
static void pm_clk_acquire(struct device *dev, struct pm_clock_entry *ce)
{
	if (!ce->clk)
		ce->clk = clk_get(dev, ce->con_id);
	if (IS_ERR(ce->clk)) {
		ce->status = PCE_STATUS_ERROR;
		return;
	} else if (clk_is_enabled_when_prepared(ce->clk)) {
		 
		ce->status = PCE_STATUS_ACQUIRED;
		ce->enabled_when_prepared = true;
	} else if (clk_prepare(ce->clk)) {
		ce->status = PCE_STATUS_ERROR;
		dev_err(dev, "clk_prepare() failed\n");
		return;
	} else {
		ce->status = PCE_STATUS_PREPARED;
	}
	dev_dbg(dev, "Clock %pC con_id %s managed by runtime PM.\n",
		ce->clk, ce->con_id);
}

static int __pm_clk_add(struct device *dev, const char *con_id,
			struct clk *clk)
{
	struct pm_subsys_data *psd = dev_to_psd(dev);
	struct pm_clock_entry *ce;

	if (!psd)
		return -EINVAL;

	ce = kzalloc(sizeof(*ce), GFP_KERNEL);
	if (!ce)
		return -ENOMEM;

	if (con_id) {
		ce->con_id = kstrdup(con_id, GFP_KERNEL);
		if (!ce->con_id) {
			kfree(ce);
			return -ENOMEM;
		}
	} else {
		if (IS_ERR(clk)) {
			kfree(ce);
			return -ENOENT;
		}
		ce->clk = clk;
	}

	pm_clk_acquire(dev, ce);

	pm_clk_list_lock(psd);
	list_add_tail(&ce->node, &psd->clock_list);
	if (ce->enabled_when_prepared)
		psd->clock_op_might_sleep++;
	pm_clk_list_unlock(psd);
	return 0;
}

 
int pm_clk_add(struct device *dev, const char *con_id)
{
	return __pm_clk_add(dev, con_id, NULL);
}
EXPORT_SYMBOL_GPL(pm_clk_add);

 
int pm_clk_add_clk(struct device *dev, struct clk *clk)
{
	return __pm_clk_add(dev, NULL, clk);
}
EXPORT_SYMBOL_GPL(pm_clk_add_clk);


 
int of_pm_clk_add_clk(struct device *dev, const char *name)
{
	struct clk *clk;
	int ret;

	if (!dev || !dev->of_node || !name)
		return -EINVAL;

	clk = of_clk_get_by_name(dev->of_node, name);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	ret = pm_clk_add_clk(dev, clk);
	if (ret) {
		clk_put(clk);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(of_pm_clk_add_clk);

 
int of_pm_clk_add_clks(struct device *dev)
{
	struct clk **clks;
	int i, count;
	int ret;

	if (!dev || !dev->of_node)
		return -EINVAL;

	count = of_clk_get_parent_count(dev->of_node);
	if (count <= 0)
		return -ENODEV;

	clks = kcalloc(count, sizeof(*clks), GFP_KERNEL);
	if (!clks)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		clks[i] = of_clk_get(dev->of_node, i);
		if (IS_ERR(clks[i])) {
			ret = PTR_ERR(clks[i]);
			goto error;
		}

		ret = pm_clk_add_clk(dev, clks[i]);
		if (ret) {
			clk_put(clks[i]);
			goto error;
		}
	}

	kfree(clks);

	return i;

error:
	while (i--)
		pm_clk_remove_clk(dev, clks[i]);

	kfree(clks);

	return ret;
}
EXPORT_SYMBOL_GPL(of_pm_clk_add_clks);

 
static void __pm_clk_remove(struct pm_clock_entry *ce)
{
	if (!ce)
		return;

	switch (ce->status) {
	case PCE_STATUS_ENABLED:
		clk_disable(ce->clk);
		fallthrough;
	case PCE_STATUS_PREPARED:
		clk_unprepare(ce->clk);
		fallthrough;
	case PCE_STATUS_ACQUIRED:
	case PCE_STATUS_ERROR:
		if (!IS_ERR(ce->clk))
			clk_put(ce->clk);
		break;
	default:
		break;
	}

	kfree(ce->con_id);
	kfree(ce);
}

 
void pm_clk_remove(struct device *dev, const char *con_id)
{
	struct pm_subsys_data *psd = dev_to_psd(dev);
	struct pm_clock_entry *ce;

	if (!psd)
		return;

	pm_clk_list_lock(psd);

	list_for_each_entry(ce, &psd->clock_list, node) {
		if (!con_id && !ce->con_id)
			goto remove;
		else if (!con_id || !ce->con_id)
			continue;
		else if (!strcmp(con_id, ce->con_id))
			goto remove;
	}

	pm_clk_list_unlock(psd);
	return;

 remove:
	list_del(&ce->node);
	if (ce->enabled_when_prepared)
		psd->clock_op_might_sleep--;
	pm_clk_list_unlock(psd);

	__pm_clk_remove(ce);
}
EXPORT_SYMBOL_GPL(pm_clk_remove);

 
void pm_clk_remove_clk(struct device *dev, struct clk *clk)
{
	struct pm_subsys_data *psd = dev_to_psd(dev);
	struct pm_clock_entry *ce;

	if (!psd || !clk)
		return;

	pm_clk_list_lock(psd);

	list_for_each_entry(ce, &psd->clock_list, node) {
		if (clk == ce->clk)
			goto remove;
	}

	pm_clk_list_unlock(psd);
	return;

 remove:
	list_del(&ce->node);
	if (ce->enabled_when_prepared)
		psd->clock_op_might_sleep--;
	pm_clk_list_unlock(psd);

	__pm_clk_remove(ce);
}
EXPORT_SYMBOL_GPL(pm_clk_remove_clk);

 
void pm_clk_init(struct device *dev)
{
	struct pm_subsys_data *psd = dev_to_psd(dev);
	if (psd) {
		INIT_LIST_HEAD(&psd->clock_list);
		mutex_init(&psd->clock_mutex);
		psd->clock_op_might_sleep = 0;
	}
}
EXPORT_SYMBOL_GPL(pm_clk_init);

 
int pm_clk_create(struct device *dev)
{
	return dev_pm_get_subsys_data(dev);
}
EXPORT_SYMBOL_GPL(pm_clk_create);

 
void pm_clk_destroy(struct device *dev)
{
	struct pm_subsys_data *psd = dev_to_psd(dev);
	struct pm_clock_entry *ce, *c;
	struct list_head list;

	if (!psd)
		return;

	INIT_LIST_HEAD(&list);

	pm_clk_list_lock(psd);

	list_for_each_entry_safe_reverse(ce, c, &psd->clock_list, node)
		list_move(&ce->node, &list);
	psd->clock_op_might_sleep = 0;

	pm_clk_list_unlock(psd);

	dev_pm_put_subsys_data(dev);

	list_for_each_entry_safe_reverse(ce, c, &list, node) {
		list_del(&ce->node);
		__pm_clk_remove(ce);
	}
}
EXPORT_SYMBOL_GPL(pm_clk_destroy);

static void pm_clk_destroy_action(void *data)
{
	pm_clk_destroy(data);
}

int devm_pm_clk_create(struct device *dev)
{
	int ret;

	ret = pm_clk_create(dev);
	if (ret)
		return ret;

	return devm_add_action_or_reset(dev, pm_clk_destroy_action, dev);
}
EXPORT_SYMBOL_GPL(devm_pm_clk_create);

 
int pm_clk_suspend(struct device *dev)
{
	struct pm_subsys_data *psd = dev_to_psd(dev);
	struct pm_clock_entry *ce;
	unsigned long flags;
	int ret;

	dev_dbg(dev, "%s()\n", __func__);

	if (!psd)
		return 0;

	ret = pm_clk_op_lock(psd, &flags, __func__);
	if (ret)
		return ret;

	list_for_each_entry_reverse(ce, &psd->clock_list, node) {
		if (ce->status == PCE_STATUS_ENABLED) {
			if (ce->enabled_when_prepared) {
				clk_disable_unprepare(ce->clk);
				ce->status = PCE_STATUS_ACQUIRED;
			} else {
				clk_disable(ce->clk);
				ce->status = PCE_STATUS_PREPARED;
			}
		}
	}

	pm_clk_op_unlock(psd, &flags);

	return 0;
}
EXPORT_SYMBOL_GPL(pm_clk_suspend);

 
int pm_clk_resume(struct device *dev)
{
	struct pm_subsys_data *psd = dev_to_psd(dev);
	struct pm_clock_entry *ce;
	unsigned long flags;
	int ret;

	dev_dbg(dev, "%s()\n", __func__);

	if (!psd)
		return 0;

	ret = pm_clk_op_lock(psd, &flags, __func__);
	if (ret)
		return ret;

	list_for_each_entry(ce, &psd->clock_list, node)
		__pm_clk_enable(dev, ce);

	pm_clk_op_unlock(psd, &flags);

	return 0;
}
EXPORT_SYMBOL_GPL(pm_clk_resume);

 
static int pm_clk_notify(struct notifier_block *nb,
				 unsigned long action, void *data)
{
	struct pm_clk_notifier_block *clknb;
	struct device *dev = data;
	char **con_id;
	int error;

	dev_dbg(dev, "%s() %ld\n", __func__, action);

	clknb = container_of(nb, struct pm_clk_notifier_block, nb);

	switch (action) {
	case BUS_NOTIFY_ADD_DEVICE:
		if (dev->pm_domain)
			break;

		error = pm_clk_create(dev);
		if (error)
			break;

		dev_pm_domain_set(dev, clknb->pm_domain);
		if (clknb->con_ids[0]) {
			for (con_id = clknb->con_ids; *con_id; con_id++)
				pm_clk_add(dev, *con_id);
		} else {
			pm_clk_add(dev, NULL);
		}

		break;
	case BUS_NOTIFY_DEL_DEVICE:
		if (dev->pm_domain != clknb->pm_domain)
			break;

		dev_pm_domain_set(dev, NULL);
		pm_clk_destroy(dev);
		break;
	}

	return 0;
}

int pm_clk_runtime_suspend(struct device *dev)
{
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	ret = pm_generic_runtime_suspend(dev);
	if (ret) {
		dev_err(dev, "failed to suspend device\n");
		return ret;
	}

	ret = pm_clk_suspend(dev);
	if (ret) {
		dev_err(dev, "failed to suspend clock\n");
		pm_generic_runtime_resume(dev);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pm_clk_runtime_suspend);

int pm_clk_runtime_resume(struct device *dev)
{
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	ret = pm_clk_resume(dev);
	if (ret) {
		dev_err(dev, "failed to resume clock\n");
		return ret;
	}

	return pm_generic_runtime_resume(dev);
}
EXPORT_SYMBOL_GPL(pm_clk_runtime_resume);

#else  

 
static void enable_clock(struct device *dev, const char *con_id)
{
	struct clk *clk;

	clk = clk_get(dev, con_id);
	if (!IS_ERR(clk)) {
		clk_prepare_enable(clk);
		clk_put(clk);
		dev_info(dev, "Runtime PM disabled, clock forced on.\n");
	}
}

 
static void disable_clock(struct device *dev, const char *con_id)
{
	struct clk *clk;

	clk = clk_get(dev, con_id);
	if (!IS_ERR(clk)) {
		clk_disable_unprepare(clk);
		clk_put(clk);
		dev_info(dev, "Runtime PM disabled, clock forced off.\n");
	}
}

 
static int pm_clk_notify(struct notifier_block *nb,
				 unsigned long action, void *data)
{
	struct pm_clk_notifier_block *clknb;
	struct device *dev = data;
	char **con_id;

	dev_dbg(dev, "%s() %ld\n", __func__, action);

	clknb = container_of(nb, struct pm_clk_notifier_block, nb);

	switch (action) {
	case BUS_NOTIFY_BIND_DRIVER:
		if (clknb->con_ids[0]) {
			for (con_id = clknb->con_ids; *con_id; con_id++)
				enable_clock(dev, *con_id);
		} else {
			enable_clock(dev, NULL);
		}
		break;
	case BUS_NOTIFY_DRIVER_NOT_BOUND:
	case BUS_NOTIFY_UNBOUND_DRIVER:
		if (clknb->con_ids[0]) {
			for (con_id = clknb->con_ids; *con_id; con_id++)
				disable_clock(dev, *con_id);
		} else {
			disable_clock(dev, NULL);
		}
		break;
	}

	return 0;
}

#endif  

 
void pm_clk_add_notifier(struct bus_type *bus,
				 struct pm_clk_notifier_block *clknb)
{
	if (!bus || !clknb)
		return;

	clknb->nb.notifier_call = pm_clk_notify;
	bus_register_notifier(bus, &clknb->nb);
}
EXPORT_SYMBOL_GPL(pm_clk_add_notifier);
