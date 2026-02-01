
 

#define pr_fmt(fmt) "mux-core: " fmt

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mux/consumer.h>
#include <linux/mux/driver.h>
#include <linux/of.h>
#include <linux/slab.h>

 
#define MUX_CACHE_UNKNOWN MUX_IDLE_AS_IS

 
struct mux_state {
	struct mux_control *mux;
	unsigned int state;
};

static struct class mux_class = {
	.name = "mux",
};

static DEFINE_IDA(mux_ida);

static int __init mux_init(void)
{
	ida_init(&mux_ida);
	return class_register(&mux_class);
}

static void __exit mux_exit(void)
{
	class_unregister(&mux_class);
	ida_destroy(&mux_ida);
}

static void mux_chip_release(struct device *dev)
{
	struct mux_chip *mux_chip = to_mux_chip(dev);

	ida_simple_remove(&mux_ida, mux_chip->id);
	kfree(mux_chip);
}

static const struct device_type mux_type = {
	.name = "mux-chip",
	.release = mux_chip_release,
};

 
struct mux_chip *mux_chip_alloc(struct device *dev,
				unsigned int controllers, size_t sizeof_priv)
{
	struct mux_chip *mux_chip;
	int i;

	if (WARN_ON(!dev || !controllers))
		return ERR_PTR(-EINVAL);

	mux_chip = kzalloc(sizeof(*mux_chip) +
			   controllers * sizeof(*mux_chip->mux) +
			   sizeof_priv, GFP_KERNEL);
	if (!mux_chip)
		return ERR_PTR(-ENOMEM);

	mux_chip->mux = (struct mux_control *)(mux_chip + 1);
	mux_chip->dev.class = &mux_class;
	mux_chip->dev.type = &mux_type;
	mux_chip->dev.parent = dev;
	mux_chip->dev.of_node = dev->of_node;
	dev_set_drvdata(&mux_chip->dev, mux_chip);

	mux_chip->id = ida_simple_get(&mux_ida, 0, 0, GFP_KERNEL);
	if (mux_chip->id < 0) {
		int err = mux_chip->id;

		pr_err("muxchipX failed to get a device id\n");
		kfree(mux_chip);
		return ERR_PTR(err);
	}
	dev_set_name(&mux_chip->dev, "muxchip%d", mux_chip->id);

	mux_chip->controllers = controllers;
	for (i = 0; i < controllers; ++i) {
		struct mux_control *mux = &mux_chip->mux[i];

		mux->chip = mux_chip;
		sema_init(&mux->lock, 1);
		mux->cached_state = MUX_CACHE_UNKNOWN;
		mux->idle_state = MUX_IDLE_AS_IS;
		mux->last_change = ktime_get();
	}

	device_initialize(&mux_chip->dev);

	return mux_chip;
}
EXPORT_SYMBOL_GPL(mux_chip_alloc);

static int mux_control_set(struct mux_control *mux, int state)
{
	int ret = mux->chip->ops->set(mux, state);

	mux->cached_state = ret < 0 ? MUX_CACHE_UNKNOWN : state;
	if (ret >= 0)
		mux->last_change = ktime_get();

	return ret;
}

 
int mux_chip_register(struct mux_chip *mux_chip)
{
	int i;
	int ret;

	for (i = 0; i < mux_chip->controllers; ++i) {
		struct mux_control *mux = &mux_chip->mux[i];

		if (mux->idle_state == mux->cached_state)
			continue;

		ret = mux_control_set(mux, mux->idle_state);
		if (ret < 0) {
			dev_err(&mux_chip->dev, "unable to set idle state\n");
			return ret;
		}
	}

	ret = device_add(&mux_chip->dev);
	if (ret < 0)
		dev_err(&mux_chip->dev,
			"device_add failed in %s: %d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL_GPL(mux_chip_register);

 
void mux_chip_unregister(struct mux_chip *mux_chip)
{
	device_del(&mux_chip->dev);
}
EXPORT_SYMBOL_GPL(mux_chip_unregister);

 
void mux_chip_free(struct mux_chip *mux_chip)
{
	if (!mux_chip)
		return;

	put_device(&mux_chip->dev);
}
EXPORT_SYMBOL_GPL(mux_chip_free);

static void devm_mux_chip_release(struct device *dev, void *res)
{
	struct mux_chip *mux_chip = *(struct mux_chip **)res;

	mux_chip_free(mux_chip);
}

 
struct mux_chip *devm_mux_chip_alloc(struct device *dev,
				     unsigned int controllers,
				     size_t sizeof_priv)
{
	struct mux_chip **ptr, *mux_chip;

	ptr = devres_alloc(devm_mux_chip_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	mux_chip = mux_chip_alloc(dev, controllers, sizeof_priv);
	if (IS_ERR(mux_chip)) {
		devres_free(ptr);
		return mux_chip;
	}

	*ptr = mux_chip;
	devres_add(dev, ptr);

	return mux_chip;
}
EXPORT_SYMBOL_GPL(devm_mux_chip_alloc);

static void devm_mux_chip_reg_release(struct device *dev, void *res)
{
	struct mux_chip *mux_chip = *(struct mux_chip **)res;

	mux_chip_unregister(mux_chip);
}

 
int devm_mux_chip_register(struct device *dev,
			   struct mux_chip *mux_chip)
{
	struct mux_chip **ptr;
	int res;

	ptr = devres_alloc(devm_mux_chip_reg_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	res = mux_chip_register(mux_chip);
	if (res) {
		devres_free(ptr);
		return res;
	}

	*ptr = mux_chip;
	devres_add(dev, ptr);

	return res;
}
EXPORT_SYMBOL_GPL(devm_mux_chip_register);

 
unsigned int mux_control_states(struct mux_control *mux)
{
	return mux->states;
}
EXPORT_SYMBOL_GPL(mux_control_states);

 
static int __mux_control_select(struct mux_control *mux, int state)
{
	int ret;

	if (WARN_ON(state < 0 || state >= mux->states))
		return -EINVAL;

	if (mux->cached_state == state)
		return 0;

	ret = mux_control_set(mux, state);
	if (ret >= 0)
		return 0;

	 
	if (mux->idle_state != MUX_IDLE_AS_IS)
		mux_control_set(mux, mux->idle_state);

	return ret;
}

static void mux_control_delay(struct mux_control *mux, unsigned int delay_us)
{
	ktime_t delayend;
	s64 remaining;

	if (!delay_us)
		return;

	delayend = ktime_add_us(mux->last_change, delay_us);
	remaining = ktime_us_delta(delayend, ktime_get());
	if (remaining > 0)
		fsleep(remaining);
}

 
int mux_control_select_delay(struct mux_control *mux, unsigned int state,
			     unsigned int delay_us)
{
	int ret;

	ret = down_killable(&mux->lock);
	if (ret < 0)
		return ret;

	ret = __mux_control_select(mux, state);
	if (ret >= 0)
		mux_control_delay(mux, delay_us);

	if (ret < 0)
		up(&mux->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mux_control_select_delay);

 
int mux_state_select_delay(struct mux_state *mstate, unsigned int delay_us)
{
	return mux_control_select_delay(mstate->mux, mstate->state, delay_us);
}
EXPORT_SYMBOL_GPL(mux_state_select_delay);

 
int mux_control_try_select_delay(struct mux_control *mux, unsigned int state,
				 unsigned int delay_us)
{
	int ret;

	if (down_trylock(&mux->lock))
		return -EBUSY;

	ret = __mux_control_select(mux, state);
	if (ret >= 0)
		mux_control_delay(mux, delay_us);

	if (ret < 0)
		up(&mux->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mux_control_try_select_delay);

 
int mux_state_try_select_delay(struct mux_state *mstate, unsigned int delay_us)
{
	return mux_control_try_select_delay(mstate->mux, mstate->state, delay_us);
}
EXPORT_SYMBOL_GPL(mux_state_try_select_delay);

 
int mux_control_deselect(struct mux_control *mux)
{
	int ret = 0;

	if (mux->idle_state != MUX_IDLE_AS_IS &&
	    mux->idle_state != mux->cached_state)
		ret = mux_control_set(mux, mux->idle_state);

	up(&mux->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mux_control_deselect);

 
int mux_state_deselect(struct mux_state *mstate)
{
	return mux_control_deselect(mstate->mux);
}
EXPORT_SYMBOL_GPL(mux_state_deselect);

 
static struct mux_chip *of_find_mux_chip_by_node(struct device_node *np)
{
	struct device *dev;

	dev = class_find_device_by_of_node(&mux_class, np);

	return dev ? to_mux_chip(dev) : NULL;
}

 
static struct mux_control *mux_get(struct device *dev, const char *mux_name,
				   unsigned int *state)
{
	struct device_node *np = dev->of_node;
	struct of_phandle_args args;
	struct mux_chip *mux_chip;
	unsigned int controller;
	int index = 0;
	int ret;

	if (mux_name) {
		if (state)
			index = of_property_match_string(np, "mux-state-names",
							 mux_name);
		else
			index = of_property_match_string(np, "mux-control-names",
							 mux_name);
		if (index < 0) {
			dev_err(dev, "mux controller '%s' not found\n",
				mux_name);
			return ERR_PTR(index);
		}
	}

	if (state)
		ret = of_parse_phandle_with_args(np,
						 "mux-states", "#mux-state-cells",
						 index, &args);
	else
		ret = of_parse_phandle_with_args(np,
						 "mux-controls", "#mux-control-cells",
						 index, &args);
	if (ret) {
		dev_err(dev, "%pOF: failed to get mux-%s %s(%i)\n",
			np, state ? "state" : "control", mux_name ?: "", index);
		return ERR_PTR(ret);
	}

	mux_chip = of_find_mux_chip_by_node(args.np);
	of_node_put(args.np);
	if (!mux_chip)
		return ERR_PTR(-EPROBE_DEFER);

	controller = 0;
	if (state) {
		if (args.args_count > 2 || args.args_count == 0 ||
		    (args.args_count < 2 && mux_chip->controllers > 1)) {
			dev_err(dev, "%pOF: wrong #mux-state-cells for %pOF\n",
				np, args.np);
			put_device(&mux_chip->dev);
			return ERR_PTR(-EINVAL);
		}

		if (args.args_count == 2) {
			controller = args.args[0];
			*state = args.args[1];
		} else {
			*state = args.args[0];
		}

	} else {
		if (args.args_count > 1 ||
		    (!args.args_count && mux_chip->controllers > 1)) {
			dev_err(dev, "%pOF: wrong #mux-control-cells for %pOF\n",
				np, args.np);
			put_device(&mux_chip->dev);
			return ERR_PTR(-EINVAL);
		}

		if (args.args_count)
			controller = args.args[0];
	}

	if (controller >= mux_chip->controllers) {
		dev_err(dev, "%pOF: bad mux controller %u specified in %pOF\n",
			np, controller, args.np);
		put_device(&mux_chip->dev);
		return ERR_PTR(-EINVAL);
	}

	return &mux_chip->mux[controller];
}

 
struct mux_control *mux_control_get(struct device *dev, const char *mux_name)
{
	return mux_get(dev, mux_name, NULL);
}
EXPORT_SYMBOL_GPL(mux_control_get);

 
void mux_control_put(struct mux_control *mux)
{
	put_device(&mux->chip->dev);
}
EXPORT_SYMBOL_GPL(mux_control_put);

static void devm_mux_control_release(struct device *dev, void *res)
{
	struct mux_control *mux = *(struct mux_control **)res;

	mux_control_put(mux);
}

 
struct mux_control *devm_mux_control_get(struct device *dev,
					 const char *mux_name)
{
	struct mux_control **ptr, *mux;

	ptr = devres_alloc(devm_mux_control_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	mux = mux_control_get(dev, mux_name);
	if (IS_ERR(mux)) {
		devres_free(ptr);
		return mux;
	}

	*ptr = mux;
	devres_add(dev, ptr);

	return mux;
}
EXPORT_SYMBOL_GPL(devm_mux_control_get);

 
static struct mux_state *mux_state_get(struct device *dev, const char *mux_name)
{
	struct mux_state *mstate;

	mstate = kzalloc(sizeof(*mstate), GFP_KERNEL);
	if (!mstate)
		return ERR_PTR(-ENOMEM);

	mstate->mux = mux_get(dev, mux_name, &mstate->state);
	if (IS_ERR(mstate->mux)) {
		int err = PTR_ERR(mstate->mux);

		kfree(mstate);
		return ERR_PTR(err);
	}

	return mstate;
}

 
static void mux_state_put(struct mux_state *mstate)
{
	mux_control_put(mstate->mux);
	kfree(mstate);
}

static void devm_mux_state_release(struct device *dev, void *res)
{
	struct mux_state *mstate = *(struct mux_state **)res;

	mux_state_put(mstate);
}

 
struct mux_state *devm_mux_state_get(struct device *dev,
				     const char *mux_name)
{
	struct mux_state **ptr, *mstate;

	ptr = devres_alloc(devm_mux_state_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	mstate = mux_state_get(dev, mux_name);
	if (IS_ERR(mstate)) {
		devres_free(ptr);
		return mstate;
	}

	*ptr = mstate;
	devres_add(dev, ptr);

	return mstate;
}
EXPORT_SYMBOL_GPL(devm_mux_state_get);

 
subsys_initcall(mux_init);
module_exit(mux_exit);

MODULE_DESCRIPTION("Multiplexer subsystem");
MODULE_AUTHOR("Peter Rosin <peda@axentia.se>");
MODULE_LICENSE("GPL v2");
