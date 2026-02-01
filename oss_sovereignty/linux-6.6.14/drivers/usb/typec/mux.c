
 

#include <linux/device.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/property.h>
#include <linux/slab.h>

#include "class.h"
#include "mux.h"

#define TYPEC_MUX_MAX_DEVS	3

struct typec_switch {
	struct typec_switch_dev *sw_devs[TYPEC_MUX_MAX_DEVS];
	unsigned int num_sw_devs;
};

static int switch_fwnode_match(struct device *dev, const void *fwnode)
{
	if (!is_typec_switch_dev(dev))
		return 0;

	return device_match_fwnode(dev, fwnode);
}

static void *typec_switch_match(const struct fwnode_handle *fwnode,
				const char *id, void *data)
{
	struct device *dev;

	 
	if (id && !fwnode_property_present(fwnode, id))
		return NULL;

	 
	dev = class_find_device(&typec_mux_class, NULL, fwnode,
				switch_fwnode_match);

	return dev ? to_typec_switch_dev(dev) : ERR_PTR(-EPROBE_DEFER);
}

 
struct typec_switch *fwnode_typec_switch_get(struct fwnode_handle *fwnode)
{
	struct typec_switch_dev *sw_devs[TYPEC_MUX_MAX_DEVS];
	struct typec_switch *sw;
	int count;
	int err;
	int i;

	sw = kzalloc(sizeof(*sw), GFP_KERNEL);
	if (!sw)
		return ERR_PTR(-ENOMEM);

	count = fwnode_connection_find_matches(fwnode, "orientation-switch", NULL,
					       typec_switch_match,
					       (void **)sw_devs,
					       ARRAY_SIZE(sw_devs));
	if (count <= 0) {
		kfree(sw);
		return NULL;
	}

	for (i = 0; i < count; i++) {
		if (IS_ERR(sw_devs[i])) {
			err = PTR_ERR(sw_devs[i]);
			goto put_sw_devs;
		}
	}

	for (i = 0; i < count; i++) {
		WARN_ON(!try_module_get(sw_devs[i]->dev.parent->driver->owner));
		sw->sw_devs[i] = sw_devs[i];
	}

	sw->num_sw_devs = count;

	return sw;

put_sw_devs:
	for (i = 0; i < count; i++) {
		if (!IS_ERR(sw_devs[i]))
			put_device(&sw_devs[i]->dev);
	}

	kfree(sw);

	return ERR_PTR(err);
}
EXPORT_SYMBOL_GPL(fwnode_typec_switch_get);

 
void typec_switch_put(struct typec_switch *sw)
{
	struct typec_switch_dev *sw_dev;
	unsigned int i;

	if (IS_ERR_OR_NULL(sw))
		return;

	for (i = 0; i < sw->num_sw_devs; i++) {
		sw_dev = sw->sw_devs[i];

		module_put(sw_dev->dev.parent->driver->owner);
		put_device(&sw_dev->dev);
	}
	kfree(sw);
}
EXPORT_SYMBOL_GPL(typec_switch_put);

static void typec_switch_release(struct device *dev)
{
	kfree(to_typec_switch_dev(dev));
}

const struct device_type typec_switch_dev_type = {
	.name = "orientation_switch",
	.release = typec_switch_release,
};

 
struct typec_switch_dev *
typec_switch_register(struct device *parent,
		      const struct typec_switch_desc *desc)
{
	struct typec_switch_dev *sw_dev;
	int ret;

	if (!desc || !desc->set)
		return ERR_PTR(-EINVAL);

	sw_dev = kzalloc(sizeof(*sw_dev), GFP_KERNEL);
	if (!sw_dev)
		return ERR_PTR(-ENOMEM);

	sw_dev->set = desc->set;

	device_initialize(&sw_dev->dev);
	sw_dev->dev.parent = parent;
	sw_dev->dev.fwnode = desc->fwnode;
	sw_dev->dev.class = &typec_mux_class;
	sw_dev->dev.type = &typec_switch_dev_type;
	sw_dev->dev.driver_data = desc->drvdata;
	ret = dev_set_name(&sw_dev->dev, "%s-switch", desc->name ? desc->name : dev_name(parent));
	if (ret) {
		put_device(&sw_dev->dev);
		return ERR_PTR(ret);
	}

	ret = device_add(&sw_dev->dev);
	if (ret) {
		dev_err(parent, "failed to register switch (%d)\n", ret);
		put_device(&sw_dev->dev);
		return ERR_PTR(ret);
	}

	return sw_dev;
}
EXPORT_SYMBOL_GPL(typec_switch_register);

int typec_switch_set(struct typec_switch *sw,
		     enum typec_orientation orientation)
{
	struct typec_switch_dev *sw_dev;
	unsigned int i;
	int ret;

	if (IS_ERR_OR_NULL(sw))
		return 0;

	for (i = 0; i < sw->num_sw_devs; i++) {
		sw_dev = sw->sw_devs[i];

		ret = sw_dev->set(sw_dev, orientation);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(typec_switch_set);

 
void typec_switch_unregister(struct typec_switch_dev *sw_dev)
{
	if (!IS_ERR_OR_NULL(sw_dev))
		device_unregister(&sw_dev->dev);
}
EXPORT_SYMBOL_GPL(typec_switch_unregister);

void typec_switch_set_drvdata(struct typec_switch_dev *sw_dev, void *data)
{
	dev_set_drvdata(&sw_dev->dev, data);
}
EXPORT_SYMBOL_GPL(typec_switch_set_drvdata);

void *typec_switch_get_drvdata(struct typec_switch_dev *sw_dev)
{
	return dev_get_drvdata(&sw_dev->dev);
}
EXPORT_SYMBOL_GPL(typec_switch_get_drvdata);

 

struct typec_mux {
	struct typec_mux_dev *mux_devs[TYPEC_MUX_MAX_DEVS];
	unsigned int num_mux_devs;
};

static int mux_fwnode_match(struct device *dev, const void *fwnode)
{
	if (!is_typec_mux_dev(dev))
		return 0;

	return device_match_fwnode(dev, fwnode);
}

static void *typec_mux_match(const struct fwnode_handle *fwnode,
			     const char *id, void *data)
{
	struct device *dev;

	 
	if (id && !fwnode_property_present(fwnode, id))
		return NULL;

	dev = class_find_device(&typec_mux_class, NULL, fwnode,
				mux_fwnode_match);

	return dev ? to_typec_mux_dev(dev) : ERR_PTR(-EPROBE_DEFER);
}

 
struct typec_mux *fwnode_typec_mux_get(struct fwnode_handle *fwnode)
{
	struct typec_mux_dev *mux_devs[TYPEC_MUX_MAX_DEVS];
	struct typec_mux *mux;
	int count;
	int err;
	int i;

	mux = kzalloc(sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	count = fwnode_connection_find_matches(fwnode, "mode-switch",
					       NULL, typec_mux_match,
					       (void **)mux_devs,
					       ARRAY_SIZE(mux_devs));
	if (count <= 0) {
		kfree(mux);
		return NULL;
	}

	for (i = 0; i < count; i++) {
		if (IS_ERR(mux_devs[i])) {
			err = PTR_ERR(mux_devs[i]);
			goto put_mux_devs;
		}
	}

	for (i = 0; i < count; i++) {
		WARN_ON(!try_module_get(mux_devs[i]->dev.parent->driver->owner));
		mux->mux_devs[i] = mux_devs[i];
	}

	mux->num_mux_devs = count;

	return mux;

put_mux_devs:
	for (i = 0; i < count; i++) {
		if (!IS_ERR(mux_devs[i]))
			put_device(&mux_devs[i]->dev);
	}

	kfree(mux);

	return ERR_PTR(err);
}
EXPORT_SYMBOL_GPL(fwnode_typec_mux_get);

 
void typec_mux_put(struct typec_mux *mux)
{
	struct typec_mux_dev *mux_dev;
	unsigned int i;

	if (IS_ERR_OR_NULL(mux))
		return;

	for (i = 0; i < mux->num_mux_devs; i++) {
		mux_dev = mux->mux_devs[i];
		module_put(mux_dev->dev.parent->driver->owner);
		put_device(&mux_dev->dev);
	}
	kfree(mux);
}
EXPORT_SYMBOL_GPL(typec_mux_put);

int typec_mux_set(struct typec_mux *mux, struct typec_mux_state *state)
{
	struct typec_mux_dev *mux_dev;
	unsigned int i;
	int ret;

	if (IS_ERR_OR_NULL(mux))
		return 0;

	for (i = 0; i < mux->num_mux_devs; i++) {
		mux_dev = mux->mux_devs[i];

		ret = mux_dev->set(mux_dev, state);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(typec_mux_set);

static void typec_mux_release(struct device *dev)
{
	kfree(to_typec_mux_dev(dev));
}

const struct device_type typec_mux_dev_type = {
	.name = "mode_switch",
	.release = typec_mux_release,
};

 
struct typec_mux_dev *
typec_mux_register(struct device *parent, const struct typec_mux_desc *desc)
{
	struct typec_mux_dev *mux_dev;
	int ret;

	if (!desc || !desc->set)
		return ERR_PTR(-EINVAL);

	mux_dev = kzalloc(sizeof(*mux_dev), GFP_KERNEL);
	if (!mux_dev)
		return ERR_PTR(-ENOMEM);

	mux_dev->set = desc->set;

	device_initialize(&mux_dev->dev);
	mux_dev->dev.parent = parent;
	mux_dev->dev.fwnode = desc->fwnode;
	mux_dev->dev.class = &typec_mux_class;
	mux_dev->dev.type = &typec_mux_dev_type;
	mux_dev->dev.driver_data = desc->drvdata;
	ret = dev_set_name(&mux_dev->dev, "%s-mux", desc->name ? desc->name : dev_name(parent));
	if (ret) {
		put_device(&mux_dev->dev);
		return ERR_PTR(ret);
	}

	ret = device_add(&mux_dev->dev);
	if (ret) {
		dev_err(parent, "failed to register mux (%d)\n", ret);
		put_device(&mux_dev->dev);
		return ERR_PTR(ret);
	}

	return mux_dev;
}
EXPORT_SYMBOL_GPL(typec_mux_register);

 
void typec_mux_unregister(struct typec_mux_dev *mux_dev)
{
	if (!IS_ERR_OR_NULL(mux_dev))
		device_unregister(&mux_dev->dev);
}
EXPORT_SYMBOL_GPL(typec_mux_unregister);

void typec_mux_set_drvdata(struct typec_mux_dev *mux_dev, void *data)
{
	dev_set_drvdata(&mux_dev->dev, data);
}
EXPORT_SYMBOL_GPL(typec_mux_set_drvdata);

void *typec_mux_get_drvdata(struct typec_mux_dev *mux_dev)
{
	return dev_get_drvdata(&mux_dev->dev);
}
EXPORT_SYMBOL_GPL(typec_mux_get_drvdata);

struct class typec_mux_class = {
	.name = "typec_mux",
};
