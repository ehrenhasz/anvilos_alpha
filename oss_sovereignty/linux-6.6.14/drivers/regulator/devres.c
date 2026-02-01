
 

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/module.h>

#include "internal.h"

static void devm_regulator_release(struct device *dev, void *res)
{
	regulator_put(*(struct regulator **)res);
}

static struct regulator *_devm_regulator_get(struct device *dev, const char *id,
					     int get_type)
{
	struct regulator **ptr, *regulator;

	ptr = devres_alloc(devm_regulator_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	regulator = _regulator_get(dev, id, get_type);
	if (!IS_ERR(regulator)) {
		*ptr = regulator;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return regulator;
}

 
struct regulator *devm_regulator_get(struct device *dev, const char *id)
{
	return _devm_regulator_get(dev, id, NORMAL_GET);
}
EXPORT_SYMBOL_GPL(devm_regulator_get);

 
struct regulator *devm_regulator_get_exclusive(struct device *dev,
					       const char *id)
{
	return _devm_regulator_get(dev, id, EXCLUSIVE_GET);
}
EXPORT_SYMBOL_GPL(devm_regulator_get_exclusive);

static void regulator_action_disable(void *d)
{
	struct regulator *r = (struct regulator *)d;

	regulator_disable(r);
}

static int _devm_regulator_get_enable(struct device *dev, const char *id,
				      int get_type)
{
	struct regulator *r;
	int ret;

	r = _devm_regulator_get(dev, id, get_type);
	if (IS_ERR(r))
		return PTR_ERR(r);

	ret = regulator_enable(r);
	if (!ret)
		ret = devm_add_action_or_reset(dev, &regulator_action_disable, r);

	if (ret)
		devm_regulator_put(r);

	return ret;
}

 
int devm_regulator_get_enable_optional(struct device *dev, const char *id)
{
	return _devm_regulator_get_enable(dev, id, OPTIONAL_GET);
}
EXPORT_SYMBOL_GPL(devm_regulator_get_enable_optional);

 
int devm_regulator_get_enable(struct device *dev, const char *id)
{
	return _devm_regulator_get_enable(dev, id, NORMAL_GET);
}
EXPORT_SYMBOL_GPL(devm_regulator_get_enable);

 
struct regulator *devm_regulator_get_optional(struct device *dev,
					      const char *id)
{
	return _devm_regulator_get(dev, id, OPTIONAL_GET);
}
EXPORT_SYMBOL_GPL(devm_regulator_get_optional);

static int devm_regulator_match(struct device *dev, void *res, void *data)
{
	struct regulator **r = res;
	if (!r || !*r) {
		WARN_ON(!r || !*r);
		return 0;
	}
	return *r == data;
}

 
void devm_regulator_put(struct regulator *regulator)
{
	int rc;

	rc = devres_release(regulator->dev, devm_regulator_release,
			    devm_regulator_match, regulator);
	if (rc != 0)
		WARN_ON(rc);
}
EXPORT_SYMBOL_GPL(devm_regulator_put);

struct regulator_bulk_devres {
	struct regulator_bulk_data *consumers;
	int num_consumers;
};

static void devm_regulator_bulk_release(struct device *dev, void *res)
{
	struct regulator_bulk_devres *devres = res;

	regulator_bulk_free(devres->num_consumers, devres->consumers);
}

static int _devm_regulator_bulk_get(struct device *dev, int num_consumers,
				    struct regulator_bulk_data *consumers,
				    enum regulator_get_type get_type)
{
	struct regulator_bulk_devres *devres;
	int ret;

	devres = devres_alloc(devm_regulator_bulk_release,
			      sizeof(*devres), GFP_KERNEL);
	if (!devres)
		return -ENOMEM;

	ret = _regulator_bulk_get(dev, num_consumers, consumers, get_type);
	if (!ret) {
		devres->consumers = consumers;
		devres->num_consumers = num_consumers;
		devres_add(dev, devres);
	} else {
		devres_free(devres);
	}

	return ret;
}

 
int devm_regulator_bulk_get(struct device *dev, int num_consumers,
			    struct regulator_bulk_data *consumers)
{
	return _devm_regulator_bulk_get(dev, num_consumers, consumers, NORMAL_GET);
}
EXPORT_SYMBOL_GPL(devm_regulator_bulk_get);

 
int devm_regulator_bulk_get_exclusive(struct device *dev, int num_consumers,
				      struct regulator_bulk_data *consumers)
{
	return _devm_regulator_bulk_get(dev, num_consumers, consumers, EXCLUSIVE_GET);
}
EXPORT_SYMBOL_GPL(devm_regulator_bulk_get_exclusive);

 
int devm_regulator_bulk_get_const(struct device *dev, int num_consumers,
				  const struct regulator_bulk_data *in_consumers,
				  struct regulator_bulk_data **out_consumers)
{
	*out_consumers = devm_kmemdup(dev, in_consumers,
				      num_consumers * sizeof(*in_consumers),
				      GFP_KERNEL);
	if (*out_consumers == NULL)
		return -ENOMEM;

	return devm_regulator_bulk_get(dev, num_consumers, *out_consumers);
}
EXPORT_SYMBOL_GPL(devm_regulator_bulk_get_const);

static int devm_regulator_bulk_match(struct device *dev, void *res,
				     void *data)
{
	struct regulator_bulk_devres *match = res;
	struct regulator_bulk_data *target = data;

	 
	return match->consumers == target;
}

 
void devm_regulator_bulk_put(struct regulator_bulk_data *consumers)
{
	int rc;
	struct regulator *regulator = consumers[0].consumer;

	rc = devres_release(regulator->dev, devm_regulator_bulk_release,
			    devm_regulator_bulk_match, consumers);
	if (rc != 0)
		WARN_ON(rc);
}
EXPORT_SYMBOL_GPL(devm_regulator_bulk_put);

static void devm_regulator_bulk_disable(void *res)
{
	struct regulator_bulk_devres *devres = res;
	int i;

	for (i = 0; i < devres->num_consumers; i++)
		regulator_disable(devres->consumers[i].consumer);
}

 
int devm_regulator_bulk_get_enable(struct device *dev, int num_consumers,
				   const char * const *id)
{
	struct regulator_bulk_devres *devres;
	struct regulator_bulk_data *consumers;
	int i, ret;

	devres = devm_kmalloc(dev, sizeof(*devres), GFP_KERNEL);
	if (!devres)
		return -ENOMEM;

	devres->consumers = devm_kcalloc(dev, num_consumers, sizeof(*consumers),
					 GFP_KERNEL);
	consumers = devres->consumers;
	if (!consumers)
		return -ENOMEM;

	devres->num_consumers = num_consumers;

	for (i = 0; i < num_consumers; i++)
		consumers[i].supply = id[i];

	ret = devm_regulator_bulk_get(dev, num_consumers, consumers);
	if (ret)
		return ret;

	for (i = 0; i < num_consumers; i++) {
		ret = regulator_enable(consumers[i].consumer);
		if (ret)
			goto unwind;
	}

	ret = devm_add_action(dev, devm_regulator_bulk_disable, devres);
	if (!ret)
		return 0;

unwind:
	while (--i >= 0)
		regulator_disable(consumers[i].consumer);

	devm_regulator_bulk_put(consumers);

	return ret;
}
EXPORT_SYMBOL_GPL(devm_regulator_bulk_get_enable);

static void devm_rdev_release(struct device *dev, void *res)
{
	regulator_unregister(*(struct regulator_dev **)res);
}

 
struct regulator_dev *devm_regulator_register(struct device *dev,
				  const struct regulator_desc *regulator_desc,
				  const struct regulator_config *config)
{
	struct regulator_dev **ptr, *rdev;

	ptr = devres_alloc(devm_rdev_release, sizeof(*ptr),
			   GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	rdev = regulator_register(dev, regulator_desc, config);
	if (!IS_ERR(rdev)) {
		*ptr = rdev;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return rdev;
}
EXPORT_SYMBOL_GPL(devm_regulator_register);

struct regulator_supply_alias_match {
	struct device *dev;
	const char *id;
};

static int devm_regulator_match_supply_alias(struct device *dev, void *res,
					     void *data)
{
	struct regulator_supply_alias_match *match = res;
	struct regulator_supply_alias_match *target = data;

	return match->dev == target->dev && strcmp(match->id, target->id) == 0;
}

static void devm_regulator_destroy_supply_alias(struct device *dev, void *res)
{
	struct regulator_supply_alias_match *match = res;

	regulator_unregister_supply_alias(match->dev, match->id);
}

 
int devm_regulator_register_supply_alias(struct device *dev, const char *id,
					 struct device *alias_dev,
					 const char *alias_id)
{
	struct regulator_supply_alias_match *match;
	int ret;

	match = devres_alloc(devm_regulator_destroy_supply_alias,
			   sizeof(struct regulator_supply_alias_match),
			   GFP_KERNEL);
	if (!match)
		return -ENOMEM;

	match->dev = dev;
	match->id = id;

	ret = regulator_register_supply_alias(dev, id, alias_dev, alias_id);
	if (ret < 0) {
		devres_free(match);
		return ret;
	}

	devres_add(dev, match);

	return 0;
}
EXPORT_SYMBOL_GPL(devm_regulator_register_supply_alias);

static void devm_regulator_unregister_supply_alias(struct device *dev,
						   const char *id)
{
	struct regulator_supply_alias_match match;
	int rc;

	match.dev = dev;
	match.id = id;

	rc = devres_release(dev, devm_regulator_destroy_supply_alias,
			    devm_regulator_match_supply_alias, &match);
	if (rc != 0)
		WARN_ON(rc);
}

 
int devm_regulator_bulk_register_supply_alias(struct device *dev,
					      const char *const *id,
					      struct device *alias_dev,
					      const char *const *alias_id,
					      int num_id)
{
	int i;
	int ret;

	for (i = 0; i < num_id; ++i) {
		ret = devm_regulator_register_supply_alias(dev, id[i],
							   alias_dev,
							   alias_id[i]);
		if (ret < 0)
			goto err;
	}

	return 0;

err:
	dev_err(dev,
		"Failed to create supply alias %s,%s -> %s,%s\n",
		id[i], dev_name(dev), alias_id[i], dev_name(alias_dev));

	while (--i >= 0)
		devm_regulator_unregister_supply_alias(dev, id[i]);

	return ret;
}
EXPORT_SYMBOL_GPL(devm_regulator_bulk_register_supply_alias);

struct regulator_notifier_match {
	struct regulator *regulator;
	struct notifier_block *nb;
};

static int devm_regulator_match_notifier(struct device *dev, void *res,
					 void *data)
{
	struct regulator_notifier_match *match = res;
	struct regulator_notifier_match *target = data;

	return match->regulator == target->regulator && match->nb == target->nb;
}

static void devm_regulator_destroy_notifier(struct device *dev, void *res)
{
	struct regulator_notifier_match *match = res;

	regulator_unregister_notifier(match->regulator, match->nb);
}

 
int devm_regulator_register_notifier(struct regulator *regulator,
				     struct notifier_block *nb)
{
	struct regulator_notifier_match *match;
	int ret;

	match = devres_alloc(devm_regulator_destroy_notifier,
			     sizeof(struct regulator_notifier_match),
			     GFP_KERNEL);
	if (!match)
		return -ENOMEM;

	match->regulator = regulator;
	match->nb = nb;

	ret = regulator_register_notifier(regulator, nb);
	if (ret < 0) {
		devres_free(match);
		return ret;
	}

	devres_add(regulator->dev, match);

	return 0;
}
EXPORT_SYMBOL_GPL(devm_regulator_register_notifier);

 
void devm_regulator_unregister_notifier(struct regulator *regulator,
					struct notifier_block *nb)
{
	struct regulator_notifier_match match;
	int rc;

	match.regulator = regulator;
	match.nb = nb;

	rc = devres_release(regulator->dev, devm_regulator_destroy_notifier,
			    devm_regulator_match_notifier, &match);
	if (rc != 0)
		WARN_ON(rc);
}
EXPORT_SYMBOL_GPL(devm_regulator_unregister_notifier);

static void regulator_irq_helper_drop(void *res)
{
	regulator_irq_helper_cancel(&res);
}

 
void *devm_regulator_irq_helper(struct device *dev,
				const struct regulator_irq_desc *d, int irq,
				int irq_flags, int common_errs,
				int *per_rdev_errs,
				struct regulator_dev **rdev, int rdev_amount)
{
	void *ptr;
	int ret;

	ptr = regulator_irq_helper(dev, d, irq, irq_flags, common_errs,
				    per_rdev_errs, rdev, rdev_amount);
	if (IS_ERR(ptr))
		return ptr;

	ret = devm_add_action_or_reset(dev, regulator_irq_helper_drop, ptr);
	if (ret)
		return ERR_PTR(ret);

	return ptr;
}
EXPORT_SYMBOL_GPL(devm_regulator_irq_helper);
