 
 

#include <linux/module.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/device.h>
#include <linux/gfp.h>

#include "gpiolib.h"

static void devm_gpiod_release(struct device *dev, void *res)
{
	struct gpio_desc **desc = res;

	gpiod_put(*desc);
}

static int devm_gpiod_match(struct device *dev, void *res, void *data)
{
	struct gpio_desc **this = res, **gpio = data;

	return *this == *gpio;
}

static void devm_gpiod_release_array(struct device *dev, void *res)
{
	struct gpio_descs **descs = res;

	gpiod_put_array(*descs);
}

static int devm_gpiod_match_array(struct device *dev, void *res, void *data)
{
	struct gpio_descs **this = res, **gpios = data;

	return *this == *gpios;
}

 
struct gpio_desc *__must_check devm_gpiod_get(struct device *dev,
					      const char *con_id,
					      enum gpiod_flags flags)
{
	return devm_gpiod_get_index(dev, con_id, 0, flags);
}
EXPORT_SYMBOL_GPL(devm_gpiod_get);

 
struct gpio_desc *__must_check devm_gpiod_get_optional(struct device *dev,
						       const char *con_id,
						       enum gpiod_flags flags)
{
	return devm_gpiod_get_index_optional(dev, con_id, 0, flags);
}
EXPORT_SYMBOL_GPL(devm_gpiod_get_optional);

 
struct gpio_desc *__must_check devm_gpiod_get_index(struct device *dev,
						    const char *con_id,
						    unsigned int idx,
						    enum gpiod_flags flags)
{
	struct gpio_desc **dr;
	struct gpio_desc *desc;

	desc = gpiod_get_index(dev, con_id, idx, flags);
	if (IS_ERR(desc))
		return desc;

	 
	if (flags & GPIOD_FLAGS_BIT_NONEXCLUSIVE) {
		struct devres *dres;

		dres = devres_find(dev, devm_gpiod_release,
				   devm_gpiod_match, &desc);
		if (dres)
			return desc;
	}

	dr = devres_alloc(devm_gpiod_release, sizeof(struct gpio_desc *),
			  GFP_KERNEL);
	if (!dr) {
		gpiod_put(desc);
		return ERR_PTR(-ENOMEM);
	}

	*dr = desc;
	devres_add(dev, dr);

	return desc;
}
EXPORT_SYMBOL_GPL(devm_gpiod_get_index);

 
struct gpio_desc *devm_fwnode_gpiod_get_index(struct device *dev,
					      struct fwnode_handle *fwnode,
					      const char *con_id, int index,
					      enum gpiod_flags flags,
					      const char *label)
{
	struct gpio_desc **dr;
	struct gpio_desc *desc;

	dr = devres_alloc(devm_gpiod_release, sizeof(struct gpio_desc *),
			  GFP_KERNEL);
	if (!dr)
		return ERR_PTR(-ENOMEM);

	desc = fwnode_gpiod_get_index(fwnode, con_id, index, flags, label);
	if (IS_ERR(desc)) {
		devres_free(dr);
		return desc;
	}

	*dr = desc;
	devres_add(dev, dr);

	return desc;
}
EXPORT_SYMBOL_GPL(devm_fwnode_gpiod_get_index);

 
struct gpio_desc *__must_check devm_gpiod_get_index_optional(struct device *dev,
							     const char *con_id,
							     unsigned int index,
							     enum gpiod_flags flags)
{
	struct gpio_desc *desc;

	desc = devm_gpiod_get_index(dev, con_id, index, flags);
	if (gpiod_not_found(desc))
		return NULL;

	return desc;
}
EXPORT_SYMBOL_GPL(devm_gpiod_get_index_optional);

 
struct gpio_descs *__must_check devm_gpiod_get_array(struct device *dev,
						     const char *con_id,
						     enum gpiod_flags flags)
{
	struct gpio_descs **dr;
	struct gpio_descs *descs;

	dr = devres_alloc(devm_gpiod_release_array,
			  sizeof(struct gpio_descs *), GFP_KERNEL);
	if (!dr)
		return ERR_PTR(-ENOMEM);

	descs = gpiod_get_array(dev, con_id, flags);
	if (IS_ERR(descs)) {
		devres_free(dr);
		return descs;
	}

	*dr = descs;
	devres_add(dev, dr);

	return descs;
}
EXPORT_SYMBOL_GPL(devm_gpiod_get_array);

 
struct gpio_descs *__must_check
devm_gpiod_get_array_optional(struct device *dev, const char *con_id,
			      enum gpiod_flags flags)
{
	struct gpio_descs *descs;

	descs = devm_gpiod_get_array(dev, con_id, flags);
	if (gpiod_not_found(descs))
		return NULL;

	return descs;
}
EXPORT_SYMBOL_GPL(devm_gpiod_get_array_optional);

 
void devm_gpiod_put(struct device *dev, struct gpio_desc *desc)
{
	WARN_ON(devres_release(dev, devm_gpiod_release, devm_gpiod_match,
		&desc));
}
EXPORT_SYMBOL_GPL(devm_gpiod_put);

 

void devm_gpiod_unhinge(struct device *dev, struct gpio_desc *desc)
{
	int ret;

	if (IS_ERR_OR_NULL(desc))
		return;
	ret = devres_destroy(dev, devm_gpiod_release,
			     devm_gpiod_match, &desc);
	 
	if (ret == -ENOENT)
		return;
	 
	WARN_ON(ret);
}
EXPORT_SYMBOL_GPL(devm_gpiod_unhinge);

 
void devm_gpiod_put_array(struct device *dev, struct gpio_descs *descs)
{
	WARN_ON(devres_release(dev, devm_gpiod_release_array,
			       devm_gpiod_match_array, &descs));
}
EXPORT_SYMBOL_GPL(devm_gpiod_put_array);

static void devm_gpio_release(struct device *dev, void *res)
{
	unsigned *gpio = res;

	gpio_free(*gpio);
}

 
int devm_gpio_request(struct device *dev, unsigned gpio, const char *label)
{
	unsigned *dr;
	int rc;

	dr = devres_alloc(devm_gpio_release, sizeof(unsigned), GFP_KERNEL);
	if (!dr)
		return -ENOMEM;

	rc = gpio_request(gpio, label);
	if (rc) {
		devres_free(dr);
		return rc;
	}

	*dr = gpio;
	devres_add(dev, dr);

	return 0;
}
EXPORT_SYMBOL_GPL(devm_gpio_request);

 
int devm_gpio_request_one(struct device *dev, unsigned gpio,
			  unsigned long flags, const char *label)
{
	unsigned *dr;
	int rc;

	dr = devres_alloc(devm_gpio_release, sizeof(unsigned), GFP_KERNEL);
	if (!dr)
		return -ENOMEM;

	rc = gpio_request_one(gpio, flags, label);
	if (rc) {
		devres_free(dr);
		return rc;
	}

	*dr = gpio;
	devres_add(dev, dr);

	return 0;
}
EXPORT_SYMBOL_GPL(devm_gpio_request_one);

static void devm_gpio_chip_release(void *data)
{
	struct gpio_chip *gc = data;

	gpiochip_remove(gc);
}

 
int devm_gpiochip_add_data_with_key(struct device *dev, struct gpio_chip *gc, void *data,
				    struct lock_class_key *lock_key,
				    struct lock_class_key *request_key)
{
	int ret;

	ret = gpiochip_add_data_with_key(gc, data, lock_key, request_key);
	if (ret < 0)
		return ret;

	return devm_add_action_or_reset(dev, devm_gpio_chip_release, gc);
}
EXPORT_SYMBOL_GPL(devm_gpiochip_add_data_with_key);
