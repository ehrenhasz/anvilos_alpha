
 
#define pr_fmt(fmt) "pinctrl core: " fmt

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/devinfo.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinctrl.h>

#ifdef CONFIG_GPIOLIB
#include "../gpio/gpiolib.h"
#endif

#include "core.h"
#include "devicetree.h"
#include "pinconf.h"
#include "pinmux.h"

static bool pinctrl_dummy_state;

 
static DEFINE_MUTEX(pinctrl_list_mutex);

 
DEFINE_MUTEX(pinctrl_maps_mutex);

 
static DEFINE_MUTEX(pinctrldev_list_mutex);

 
static LIST_HEAD(pinctrldev_list);

 
static LIST_HEAD(pinctrl_list);

 
LIST_HEAD(pinctrl_maps);


 
void pinctrl_provide_dummies(void)
{
	pinctrl_dummy_state = true;
}

const char *pinctrl_dev_get_name(struct pinctrl_dev *pctldev)
{
	 
	return pctldev->desc->name;
}
EXPORT_SYMBOL_GPL(pinctrl_dev_get_name);

const char *pinctrl_dev_get_devname(struct pinctrl_dev *pctldev)
{
	return dev_name(pctldev->dev);
}
EXPORT_SYMBOL_GPL(pinctrl_dev_get_devname);

void *pinctrl_dev_get_drvdata(struct pinctrl_dev *pctldev)
{
	return pctldev->driver_data;
}
EXPORT_SYMBOL_GPL(pinctrl_dev_get_drvdata);

 
struct pinctrl_dev *get_pinctrl_dev_from_devname(const char *devname)
{
	struct pinctrl_dev *pctldev;

	if (!devname)
		return NULL;

	mutex_lock(&pinctrldev_list_mutex);

	list_for_each_entry(pctldev, &pinctrldev_list, node) {
		if (!strcmp(dev_name(pctldev->dev), devname)) {
			 
			mutex_unlock(&pinctrldev_list_mutex);
			return pctldev;
		}
	}

	mutex_unlock(&pinctrldev_list_mutex);

	return NULL;
}

struct pinctrl_dev *get_pinctrl_dev_from_of_node(struct device_node *np)
{
	struct pinctrl_dev *pctldev;

	mutex_lock(&pinctrldev_list_mutex);

	list_for_each_entry(pctldev, &pinctrldev_list, node)
		if (device_match_of_node(pctldev->dev, np)) {
			mutex_unlock(&pinctrldev_list_mutex);
			return pctldev;
		}

	mutex_unlock(&pinctrldev_list_mutex);

	return NULL;
}

 
int pin_get_from_name(struct pinctrl_dev *pctldev, const char *name)
{
	unsigned i, pin;

	 
	for (i = 0; i < pctldev->desc->npins; i++) {
		struct pin_desc *desc;

		pin = pctldev->desc->pins[i].number;
		desc = pin_desc_get(pctldev, pin);
		 
		if (desc && !strcmp(name, desc->name))
			return pin;
	}

	return -EINVAL;
}

 
const char *pin_get_name(struct pinctrl_dev *pctldev, const unsigned pin)
{
	const struct pin_desc *desc;

	desc = pin_desc_get(pctldev, pin);
	if (!desc) {
		dev_err(pctldev->dev, "failed to get pin(%d) name\n",
			pin);
		return NULL;
	}

	return desc->name;
}
EXPORT_SYMBOL_GPL(pin_get_name);

 
static void pinctrl_free_pindescs(struct pinctrl_dev *pctldev,
				  const struct pinctrl_pin_desc *pins,
				  unsigned num_pins)
{
	int i;

	for (i = 0; i < num_pins; i++) {
		struct pin_desc *pindesc;

		pindesc = radix_tree_lookup(&pctldev->pin_desc_tree,
					    pins[i].number);
		if (pindesc) {
			radix_tree_delete(&pctldev->pin_desc_tree,
					  pins[i].number);
			if (pindesc->dynamic_name)
				kfree(pindesc->name);
		}
		kfree(pindesc);
	}
}

static int pinctrl_register_one_pin(struct pinctrl_dev *pctldev,
				    const struct pinctrl_pin_desc *pin)
{
	struct pin_desc *pindesc;
	int error;

	pindesc = pin_desc_get(pctldev, pin->number);
	if (pindesc) {
		dev_err(pctldev->dev, "pin %d already registered\n",
			pin->number);
		return -EINVAL;
	}

	pindesc = kzalloc(sizeof(*pindesc), GFP_KERNEL);
	if (!pindesc)
		return -ENOMEM;

	 
	pindesc->pctldev = pctldev;

	 
	if (pin->name) {
		pindesc->name = pin->name;
	} else {
		pindesc->name = kasprintf(GFP_KERNEL, "PIN%u", pin->number);
		if (!pindesc->name) {
			error = -ENOMEM;
			goto failed;
		}
		pindesc->dynamic_name = true;
	}

	pindesc->drv_data = pin->drv_data;

	error = radix_tree_insert(&pctldev->pin_desc_tree, pin->number, pindesc);
	if (error)
		goto failed;

	pr_debug("registered pin %d (%s) on %s\n",
		 pin->number, pindesc->name, pctldev->desc->name);
	return 0;

failed:
	kfree(pindesc);
	return error;
}

static int pinctrl_register_pins(struct pinctrl_dev *pctldev,
				 const struct pinctrl_pin_desc *pins,
				 unsigned num_descs)
{
	unsigned i;
	int ret = 0;

	for (i = 0; i < num_descs; i++) {
		ret = pinctrl_register_one_pin(pctldev, &pins[i]);
		if (ret)
			return ret;
	}

	return 0;
}

 
static inline int gpio_to_pin(struct pinctrl_gpio_range *range,
				unsigned int gpio)
{
	unsigned int offset = gpio - range->base;
	if (range->pins)
		return range->pins[offset];
	else
		return range->pin_base + offset;
}

 
static struct pinctrl_gpio_range *
pinctrl_match_gpio_range(struct pinctrl_dev *pctldev, unsigned gpio)
{
	struct pinctrl_gpio_range *range;

	mutex_lock(&pctldev->mutex);
	 
	list_for_each_entry(range, &pctldev->gpio_ranges, node) {
		 
		if (gpio >= range->base &&
		    gpio < range->base + range->npins) {
			mutex_unlock(&pctldev->mutex);
			return range;
		}
	}
	mutex_unlock(&pctldev->mutex);
	return NULL;
}

 
#ifdef CONFIG_GPIOLIB
static bool pinctrl_ready_for_gpio_range(unsigned gpio)
{
	struct pinctrl_dev *pctldev;
	struct pinctrl_gpio_range *range = NULL;
	 
	struct gpio_chip *chip = gpiod_to_chip(gpio_to_desc(gpio));

	if (WARN(!chip, "no gpio_chip for gpio%i?", gpio))
		return false;

	mutex_lock(&pinctrldev_list_mutex);

	 
	list_for_each_entry(pctldev, &pinctrldev_list, node) {
		 
		mutex_lock(&pctldev->mutex);
		list_for_each_entry(range, &pctldev->gpio_ranges, node) {
			 
			if (range->base + range->npins - 1 < chip->base ||
			    range->base > chip->base + chip->ngpio - 1)
				continue;
			mutex_unlock(&pctldev->mutex);
			mutex_unlock(&pinctrldev_list_mutex);
			return true;
		}
		mutex_unlock(&pctldev->mutex);
	}

	mutex_unlock(&pinctrldev_list_mutex);

	return false;
}
#else
static bool pinctrl_ready_for_gpio_range(unsigned gpio) { return true; }
#endif

 
static int pinctrl_get_device_gpio_range(unsigned gpio,
					 struct pinctrl_dev **outdev,
					 struct pinctrl_gpio_range **outrange)
{
	struct pinctrl_dev *pctldev;

	mutex_lock(&pinctrldev_list_mutex);

	 
	list_for_each_entry(pctldev, &pinctrldev_list, node) {
		struct pinctrl_gpio_range *range;

		range = pinctrl_match_gpio_range(pctldev, gpio);
		if (range) {
			*outdev = pctldev;
			*outrange = range;
			mutex_unlock(&pinctrldev_list_mutex);
			return 0;
		}
	}

	mutex_unlock(&pinctrldev_list_mutex);

	return -EPROBE_DEFER;
}

 
void pinctrl_add_gpio_range(struct pinctrl_dev *pctldev,
			    struct pinctrl_gpio_range *range)
{
	mutex_lock(&pctldev->mutex);
	list_add_tail(&range->node, &pctldev->gpio_ranges);
	mutex_unlock(&pctldev->mutex);
}
EXPORT_SYMBOL_GPL(pinctrl_add_gpio_range);

void pinctrl_add_gpio_ranges(struct pinctrl_dev *pctldev,
			     struct pinctrl_gpio_range *ranges,
			     unsigned nranges)
{
	int i;

	for (i = 0; i < nranges; i++)
		pinctrl_add_gpio_range(pctldev, &ranges[i]);
}
EXPORT_SYMBOL_GPL(pinctrl_add_gpio_ranges);

struct pinctrl_dev *pinctrl_find_and_add_gpio_range(const char *devname,
		struct pinctrl_gpio_range *range)
{
	struct pinctrl_dev *pctldev;

	pctldev = get_pinctrl_dev_from_devname(devname);

	 
	if (!pctldev) {
		return ERR_PTR(-EPROBE_DEFER);
	}
	pinctrl_add_gpio_range(pctldev, range);

	return pctldev;
}
EXPORT_SYMBOL_GPL(pinctrl_find_and_add_gpio_range);

int pinctrl_get_group_pins(struct pinctrl_dev *pctldev, const char *pin_group,
				const unsigned **pins, unsigned *num_pins)
{
	const struct pinctrl_ops *pctlops = pctldev->desc->pctlops;
	int gs;

	if (!pctlops->get_group_pins)
		return -EINVAL;

	gs = pinctrl_get_group_selector(pctldev, pin_group);
	if (gs < 0)
		return gs;

	return pctlops->get_group_pins(pctldev, gs, pins, num_pins);
}
EXPORT_SYMBOL_GPL(pinctrl_get_group_pins);

struct pinctrl_gpio_range *
pinctrl_find_gpio_range_from_pin_nolock(struct pinctrl_dev *pctldev,
					unsigned int pin)
{
	struct pinctrl_gpio_range *range;

	 
	list_for_each_entry(range, &pctldev->gpio_ranges, node) {
		 
		if (range->pins) {
			int a;
			for (a = 0; a < range->npins; a++) {
				if (range->pins[a] == pin)
					return range;
			}
		} else if (pin >= range->pin_base &&
			   pin < range->pin_base + range->npins)
			return range;
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(pinctrl_find_gpio_range_from_pin_nolock);

 
struct pinctrl_gpio_range *
pinctrl_find_gpio_range_from_pin(struct pinctrl_dev *pctldev,
				 unsigned int pin)
{
	struct pinctrl_gpio_range *range;

	mutex_lock(&pctldev->mutex);
	range = pinctrl_find_gpio_range_from_pin_nolock(pctldev, pin);
	mutex_unlock(&pctldev->mutex);

	return range;
}
EXPORT_SYMBOL_GPL(pinctrl_find_gpio_range_from_pin);

 
void pinctrl_remove_gpio_range(struct pinctrl_dev *pctldev,
			       struct pinctrl_gpio_range *range)
{
	mutex_lock(&pctldev->mutex);
	list_del(&range->node);
	mutex_unlock(&pctldev->mutex);
}
EXPORT_SYMBOL_GPL(pinctrl_remove_gpio_range);

#ifdef CONFIG_GENERIC_PINCTRL_GROUPS

 
int pinctrl_generic_get_group_count(struct pinctrl_dev *pctldev)
{
	return pctldev->num_groups;
}
EXPORT_SYMBOL_GPL(pinctrl_generic_get_group_count);

 
const char *pinctrl_generic_get_group_name(struct pinctrl_dev *pctldev,
					   unsigned int selector)
{
	struct group_desc *group;

	group = radix_tree_lookup(&pctldev->pin_group_tree,
				  selector);
	if (!group)
		return NULL;

	return group->name;
}
EXPORT_SYMBOL_GPL(pinctrl_generic_get_group_name);

 
int pinctrl_generic_get_group_pins(struct pinctrl_dev *pctldev,
				   unsigned int selector,
				   const unsigned int **pins,
				   unsigned int *num_pins)
{
	struct group_desc *group;

	group = radix_tree_lookup(&pctldev->pin_group_tree,
				  selector);
	if (!group) {
		dev_err(pctldev->dev, "%s could not find pingroup%i\n",
			__func__, selector);
		return -EINVAL;
	}

	*pins = group->pins;
	*num_pins = group->num_pins;

	return 0;
}
EXPORT_SYMBOL_GPL(pinctrl_generic_get_group_pins);

 
struct group_desc *pinctrl_generic_get_group(struct pinctrl_dev *pctldev,
					     unsigned int selector)
{
	struct group_desc *group;

	group = radix_tree_lookup(&pctldev->pin_group_tree,
				  selector);
	if (!group)
		return NULL;

	return group;
}
EXPORT_SYMBOL_GPL(pinctrl_generic_get_group);

static int pinctrl_generic_group_name_to_selector(struct pinctrl_dev *pctldev,
						  const char *function)
{
	const struct pinctrl_ops *ops = pctldev->desc->pctlops;
	int ngroups = ops->get_groups_count(pctldev);
	int selector = 0;

	 
	while (selector < ngroups) {
		const char *gname = ops->get_group_name(pctldev, selector);

		if (gname && !strcmp(function, gname))
			return selector;

		selector++;
	}

	return -EINVAL;
}

 
int pinctrl_generic_add_group(struct pinctrl_dev *pctldev, const char *name,
			      int *pins, int num_pins, void *data)
{
	struct group_desc *group;
	int selector, error;

	if (!name)
		return -EINVAL;

	selector = pinctrl_generic_group_name_to_selector(pctldev, name);
	if (selector >= 0)
		return selector;

	selector = pctldev->num_groups;

	group = devm_kzalloc(pctldev->dev, sizeof(*group), GFP_KERNEL);
	if (!group)
		return -ENOMEM;

	group->name = name;
	group->pins = pins;
	group->num_pins = num_pins;
	group->data = data;

	error = radix_tree_insert(&pctldev->pin_group_tree, selector, group);
	if (error)
		return error;

	pctldev->num_groups++;

	return selector;
}
EXPORT_SYMBOL_GPL(pinctrl_generic_add_group);

 
int pinctrl_generic_remove_group(struct pinctrl_dev *pctldev,
				 unsigned int selector)
{
	struct group_desc *group;

	group = radix_tree_lookup(&pctldev->pin_group_tree,
				  selector);
	if (!group)
		return -ENOENT;

	radix_tree_delete(&pctldev->pin_group_tree, selector);
	devm_kfree(pctldev->dev, group);

	pctldev->num_groups--;

	return 0;
}
EXPORT_SYMBOL_GPL(pinctrl_generic_remove_group);

 
static void pinctrl_generic_free_groups(struct pinctrl_dev *pctldev)
{
	struct radix_tree_iter iter;
	void __rcu **slot;

	radix_tree_for_each_slot(slot, &pctldev->pin_group_tree, &iter, 0)
		radix_tree_delete(&pctldev->pin_group_tree, iter.index);

	pctldev->num_groups = 0;
}

#else
static inline void pinctrl_generic_free_groups(struct pinctrl_dev *pctldev)
{
}
#endif  

 
int pinctrl_get_group_selector(struct pinctrl_dev *pctldev,
			       const char *pin_group)
{
	const struct pinctrl_ops *pctlops = pctldev->desc->pctlops;
	unsigned ngroups = pctlops->get_groups_count(pctldev);
	unsigned group_selector = 0;

	while (group_selector < ngroups) {
		const char *gname = pctlops->get_group_name(pctldev,
							    group_selector);
		if (gname && !strcmp(gname, pin_group)) {
			dev_dbg(pctldev->dev,
				"found group selector %u for %s\n",
				group_selector,
				pin_group);
			return group_selector;
		}

		group_selector++;
	}

	dev_err(pctldev->dev, "does not have pin group %s\n",
		pin_group);

	return -EINVAL;
}

bool pinctrl_gpio_can_use_line(unsigned gpio)
{
	struct pinctrl_dev *pctldev;
	struct pinctrl_gpio_range *range;
	bool result;
	int pin;

	 
	if (pinctrl_get_device_gpio_range(gpio, &pctldev, &range))
		return true;

	mutex_lock(&pctldev->mutex);

	 
	pin = gpio_to_pin(range, gpio);

	result = pinmux_can_be_used_for_gpio(pctldev, pin);

	mutex_unlock(&pctldev->mutex);

	return result;
}
EXPORT_SYMBOL_GPL(pinctrl_gpio_can_use_line);

 
int pinctrl_gpio_request(unsigned gpio)
{
	struct pinctrl_dev *pctldev;
	struct pinctrl_gpio_range *range;
	int ret;
	int pin;

	ret = pinctrl_get_device_gpio_range(gpio, &pctldev, &range);
	if (ret) {
		if (pinctrl_ready_for_gpio_range(gpio))
			ret = 0;
		return ret;
	}

	mutex_lock(&pctldev->mutex);

	 
	pin = gpio_to_pin(range, gpio);

	ret = pinmux_request_gpio(pctldev, range, pin, gpio);

	mutex_unlock(&pctldev->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(pinctrl_gpio_request);

 
void pinctrl_gpio_free(unsigned gpio)
{
	struct pinctrl_dev *pctldev;
	struct pinctrl_gpio_range *range;
	int ret;
	int pin;

	ret = pinctrl_get_device_gpio_range(gpio, &pctldev, &range);
	if (ret) {
		return;
	}
	mutex_lock(&pctldev->mutex);

	 
	pin = gpio_to_pin(range, gpio);

	pinmux_free_gpio(pctldev, pin, range);

	mutex_unlock(&pctldev->mutex);
}
EXPORT_SYMBOL_GPL(pinctrl_gpio_free);

static int pinctrl_gpio_direction(unsigned gpio, bool input)
{
	struct pinctrl_dev *pctldev;
	struct pinctrl_gpio_range *range;
	int ret;
	int pin;

	ret = pinctrl_get_device_gpio_range(gpio, &pctldev, &range);
	if (ret) {
		return ret;
	}

	mutex_lock(&pctldev->mutex);

	 
	pin = gpio_to_pin(range, gpio);
	ret = pinmux_gpio_direction(pctldev, range, pin, input);

	mutex_unlock(&pctldev->mutex);

	return ret;
}

 
int pinctrl_gpio_direction_input(unsigned gpio)
{
	return pinctrl_gpio_direction(gpio, true);
}
EXPORT_SYMBOL_GPL(pinctrl_gpio_direction_input);

 
int pinctrl_gpio_direction_output(unsigned gpio)
{
	return pinctrl_gpio_direction(gpio, false);
}
EXPORT_SYMBOL_GPL(pinctrl_gpio_direction_output);

 
int pinctrl_gpio_set_config(unsigned gpio, unsigned long config)
{
	unsigned long configs[] = { config };
	struct pinctrl_gpio_range *range;
	struct pinctrl_dev *pctldev;
	int ret, pin;

	ret = pinctrl_get_device_gpio_range(gpio, &pctldev, &range);
	if (ret)
		return ret;

	mutex_lock(&pctldev->mutex);
	pin = gpio_to_pin(range, gpio);
	ret = pinconf_set_config(pctldev, pin, configs, ARRAY_SIZE(configs));
	mutex_unlock(&pctldev->mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(pinctrl_gpio_set_config);

static struct pinctrl_state *find_state(struct pinctrl *p,
					const char *name)
{
	struct pinctrl_state *state;

	list_for_each_entry(state, &p->states, node)
		if (!strcmp(state->name, name))
			return state;

	return NULL;
}

static struct pinctrl_state *create_state(struct pinctrl *p,
					  const char *name)
{
	struct pinctrl_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return ERR_PTR(-ENOMEM);

	state->name = name;
	INIT_LIST_HEAD(&state->settings);

	list_add_tail(&state->node, &p->states);

	return state;
}

static int add_setting(struct pinctrl *p, struct pinctrl_dev *pctldev,
		       const struct pinctrl_map *map)
{
	struct pinctrl_state *state;
	struct pinctrl_setting *setting;
	int ret;

	state = find_state(p, map->name);
	if (!state)
		state = create_state(p, map->name);
	if (IS_ERR(state))
		return PTR_ERR(state);

	if (map->type == PIN_MAP_TYPE_DUMMY_STATE)
		return 0;

	setting = kzalloc(sizeof(*setting), GFP_KERNEL);
	if (!setting)
		return -ENOMEM;

	setting->type = map->type;

	if (pctldev)
		setting->pctldev = pctldev;
	else
		setting->pctldev =
			get_pinctrl_dev_from_devname(map->ctrl_dev_name);
	if (!setting->pctldev) {
		kfree(setting);
		 
		if (!strcmp(map->ctrl_dev_name, map->dev_name))
			return -ENODEV;
		 
		dev_info(p->dev, "unknown pinctrl device %s in map entry, deferring probe",
			map->ctrl_dev_name);
		return -EPROBE_DEFER;
	}

	setting->dev_name = map->dev_name;

	switch (map->type) {
	case PIN_MAP_TYPE_MUX_GROUP:
		ret = pinmux_map_to_setting(map, setting);
		break;
	case PIN_MAP_TYPE_CONFIGS_PIN:
	case PIN_MAP_TYPE_CONFIGS_GROUP:
		ret = pinconf_map_to_setting(map, setting);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	if (ret < 0) {
		kfree(setting);
		return ret;
	}

	list_add_tail(&setting->node, &state->settings);

	return 0;
}

static struct pinctrl *find_pinctrl(struct device *dev)
{
	struct pinctrl *p;

	mutex_lock(&pinctrl_list_mutex);
	list_for_each_entry(p, &pinctrl_list, node)
		if (p->dev == dev) {
			mutex_unlock(&pinctrl_list_mutex);
			return p;
		}

	mutex_unlock(&pinctrl_list_mutex);
	return NULL;
}

static void pinctrl_free(struct pinctrl *p, bool inlist);

static struct pinctrl *create_pinctrl(struct device *dev,
				      struct pinctrl_dev *pctldev)
{
	struct pinctrl *p;
	const char *devname;
	struct pinctrl_maps *maps_node;
	const struct pinctrl_map *map;
	int ret;

	 
	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return ERR_PTR(-ENOMEM);
	p->dev = dev;
	INIT_LIST_HEAD(&p->states);
	INIT_LIST_HEAD(&p->dt_maps);

	ret = pinctrl_dt_to_map(p, pctldev);
	if (ret < 0) {
		kfree(p);
		return ERR_PTR(ret);
	}

	devname = dev_name(dev);

	mutex_lock(&pinctrl_maps_mutex);
	 
	for_each_pin_map(maps_node, map) {
		 
		if (strcmp(map->dev_name, devname))
			continue;
		 
		if (pctldev &&
		    strcmp(dev_name(pctldev->dev), map->ctrl_dev_name))
			continue;

		ret = add_setting(p, pctldev, map);
		 
		if (ret == -EPROBE_DEFER) {
			pinctrl_free(p, false);
			mutex_unlock(&pinctrl_maps_mutex);
			return ERR_PTR(ret);
		}
	}
	mutex_unlock(&pinctrl_maps_mutex);

	if (ret < 0) {
		 
		pinctrl_free(p, false);
		return ERR_PTR(ret);
	}

	kref_init(&p->users);

	 
	mutex_lock(&pinctrl_list_mutex);
	list_add_tail(&p->node, &pinctrl_list);
	mutex_unlock(&pinctrl_list_mutex);

	return p;
}

 
struct pinctrl *pinctrl_get(struct device *dev)
{
	struct pinctrl *p;

	if (WARN_ON(!dev))
		return ERR_PTR(-EINVAL);

	 
	p = find_pinctrl(dev);
	if (p) {
		dev_dbg(dev, "obtain a copy of previously claimed pinctrl\n");
		kref_get(&p->users);
		return p;
	}

	return create_pinctrl(dev, NULL);
}
EXPORT_SYMBOL_GPL(pinctrl_get);

static void pinctrl_free_setting(bool disable_setting,
				 struct pinctrl_setting *setting)
{
	switch (setting->type) {
	case PIN_MAP_TYPE_MUX_GROUP:
		if (disable_setting)
			pinmux_disable_setting(setting);
		pinmux_free_setting(setting);
		break;
	case PIN_MAP_TYPE_CONFIGS_PIN:
	case PIN_MAP_TYPE_CONFIGS_GROUP:
		pinconf_free_setting(setting);
		break;
	default:
		break;
	}
}

static void pinctrl_free(struct pinctrl *p, bool inlist)
{
	struct pinctrl_state *state, *n1;
	struct pinctrl_setting *setting, *n2;

	mutex_lock(&pinctrl_list_mutex);
	list_for_each_entry_safe(state, n1, &p->states, node) {
		list_for_each_entry_safe(setting, n2, &state->settings, node) {
			pinctrl_free_setting(state == p->state, setting);
			list_del(&setting->node);
			kfree(setting);
		}
		list_del(&state->node);
		kfree(state);
	}

	pinctrl_dt_free_maps(p);

	if (inlist)
		list_del(&p->node);
	kfree(p);
	mutex_unlock(&pinctrl_list_mutex);
}

 
static void pinctrl_release(struct kref *kref)
{
	struct pinctrl *p = container_of(kref, struct pinctrl, users);

	pinctrl_free(p, true);
}

 
void pinctrl_put(struct pinctrl *p)
{
	kref_put(&p->users, pinctrl_release);
}
EXPORT_SYMBOL_GPL(pinctrl_put);

 
struct pinctrl_state *pinctrl_lookup_state(struct pinctrl *p,
						 const char *name)
{
	struct pinctrl_state *state;

	state = find_state(p, name);
	if (!state) {
		if (pinctrl_dummy_state) {
			 
			dev_dbg(p->dev, "using pinctrl dummy state (%s)\n",
				name);
			state = create_state(p, name);
		} else
			state = ERR_PTR(-ENODEV);
	}

	return state;
}
EXPORT_SYMBOL_GPL(pinctrl_lookup_state);

static void pinctrl_link_add(struct pinctrl_dev *pctldev,
			     struct device *consumer)
{
	if (pctldev->desc->link_consumers)
		device_link_add(consumer, pctldev->dev,
				DL_FLAG_PM_RUNTIME |
				DL_FLAG_AUTOREMOVE_CONSUMER);
}

 
static int pinctrl_commit_state(struct pinctrl *p, struct pinctrl_state *state)
{
	struct pinctrl_setting *setting, *setting2;
	struct pinctrl_state *old_state = READ_ONCE(p->state);
	int ret;

	if (old_state) {
		 
		list_for_each_entry(setting, &old_state->settings, node) {
			if (setting->type != PIN_MAP_TYPE_MUX_GROUP)
				continue;
			pinmux_disable_setting(setting);
		}
	}

	p->state = NULL;

	 
	list_for_each_entry(setting, &state->settings, node) {
		switch (setting->type) {
		case PIN_MAP_TYPE_MUX_GROUP:
			ret = pinmux_enable_setting(setting);
			break;
		case PIN_MAP_TYPE_CONFIGS_PIN:
		case PIN_MAP_TYPE_CONFIGS_GROUP:
			ret = 0;
			break;
		default:
			ret = -EINVAL;
			break;
		}

		if (ret < 0)
			goto unapply_new_state;

		 
		if (p != setting->pctldev->p)
			pinctrl_link_add(setting->pctldev, p->dev);
	}

	 
	list_for_each_entry(setting, &state->settings, node) {
		switch (setting->type) {
		case PIN_MAP_TYPE_MUX_GROUP:
			ret = 0;
			break;
		case PIN_MAP_TYPE_CONFIGS_PIN:
		case PIN_MAP_TYPE_CONFIGS_GROUP:
			ret = pinconf_apply_setting(setting);
			break;
		default:
			ret = -EINVAL;
			break;
		}

		if (ret < 0) {
			goto unapply_new_state;
		}

		 
		if (p != setting->pctldev->p)
			pinctrl_link_add(setting->pctldev, p->dev);
	}

	p->state = state;

	return 0;

unapply_new_state:
	dev_err(p->dev, "Error applying setting, reverse things back\n");

	list_for_each_entry(setting2, &state->settings, node) {
		if (&setting2->node == &setting->node)
			break;
		 
		if (setting2->type == PIN_MAP_TYPE_MUX_GROUP)
			pinmux_disable_setting(setting2);
	}

	 
	if (old_state)
		pinctrl_select_state(p, old_state);

	return ret;
}

 
int pinctrl_select_state(struct pinctrl *p, struct pinctrl_state *state)
{
	if (p->state == state)
		return 0;

	return pinctrl_commit_state(p, state);
}
EXPORT_SYMBOL_GPL(pinctrl_select_state);

static void devm_pinctrl_release(struct device *dev, void *res)
{
	pinctrl_put(*(struct pinctrl **)res);
}

 
struct pinctrl *devm_pinctrl_get(struct device *dev)
{
	struct pinctrl **ptr, *p;

	ptr = devres_alloc(devm_pinctrl_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	p = pinctrl_get(dev);
	if (!IS_ERR(p)) {
		*ptr = p;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return p;
}
EXPORT_SYMBOL_GPL(devm_pinctrl_get);

static int devm_pinctrl_match(struct device *dev, void *res, void *data)
{
	struct pinctrl **p = res;

	return *p == data;
}

 
void devm_pinctrl_put(struct pinctrl *p)
{
	WARN_ON(devres_release(p->dev, devm_pinctrl_release,
			       devm_pinctrl_match, p));
}
EXPORT_SYMBOL_GPL(devm_pinctrl_put);

 
int pinctrl_register_mappings(const struct pinctrl_map *maps,
			      unsigned num_maps)
{
	int i, ret;
	struct pinctrl_maps *maps_node;

	pr_debug("add %u pinctrl maps\n", num_maps);

	 
	for (i = 0; i < num_maps; i++) {
		if (!maps[i].dev_name) {
			pr_err("failed to register map %s (%d): no device given\n",
			       maps[i].name, i);
			return -EINVAL;
		}

		if (!maps[i].name) {
			pr_err("failed to register map %d: no map name given\n",
			       i);
			return -EINVAL;
		}

		if (maps[i].type != PIN_MAP_TYPE_DUMMY_STATE &&
				!maps[i].ctrl_dev_name) {
			pr_err("failed to register map %s (%d): no pin control device given\n",
			       maps[i].name, i);
			return -EINVAL;
		}

		switch (maps[i].type) {
		case PIN_MAP_TYPE_DUMMY_STATE:
			break;
		case PIN_MAP_TYPE_MUX_GROUP:
			ret = pinmux_validate_map(&maps[i], i);
			if (ret < 0)
				return ret;
			break;
		case PIN_MAP_TYPE_CONFIGS_PIN:
		case PIN_MAP_TYPE_CONFIGS_GROUP:
			ret = pinconf_validate_map(&maps[i], i);
			if (ret < 0)
				return ret;
			break;
		default:
			pr_err("failed to register map %s (%d): invalid type given\n",
			       maps[i].name, i);
			return -EINVAL;
		}
	}

	maps_node = kzalloc(sizeof(*maps_node), GFP_KERNEL);
	if (!maps_node)
		return -ENOMEM;

	maps_node->maps = maps;
	maps_node->num_maps = num_maps;

	mutex_lock(&pinctrl_maps_mutex);
	list_add_tail(&maps_node->node, &pinctrl_maps);
	mutex_unlock(&pinctrl_maps_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(pinctrl_register_mappings);

 
void pinctrl_unregister_mappings(const struct pinctrl_map *map)
{
	struct pinctrl_maps *maps_node;

	mutex_lock(&pinctrl_maps_mutex);
	list_for_each_entry(maps_node, &pinctrl_maps, node) {
		if (maps_node->maps == map) {
			list_del(&maps_node->node);
			kfree(maps_node);
			mutex_unlock(&pinctrl_maps_mutex);
			return;
		}
	}
	mutex_unlock(&pinctrl_maps_mutex);
}
EXPORT_SYMBOL_GPL(pinctrl_unregister_mappings);

 
int pinctrl_force_sleep(struct pinctrl_dev *pctldev)
{
	if (!IS_ERR(pctldev->p) && !IS_ERR(pctldev->hog_sleep))
		return pinctrl_commit_state(pctldev->p, pctldev->hog_sleep);
	return 0;
}
EXPORT_SYMBOL_GPL(pinctrl_force_sleep);

 
int pinctrl_force_default(struct pinctrl_dev *pctldev)
{
	if (!IS_ERR(pctldev->p) && !IS_ERR(pctldev->hog_default))
		return pinctrl_commit_state(pctldev->p, pctldev->hog_default);
	return 0;
}
EXPORT_SYMBOL_GPL(pinctrl_force_default);

 
int pinctrl_init_done(struct device *dev)
{
	struct dev_pin_info *pins = dev->pins;
	int ret;

	if (!pins)
		return 0;

	if (IS_ERR(pins->init_state))
		return 0;  

	if (pins->p->state != pins->init_state)
		return 0;  

	if (IS_ERR(pins->default_state))
		return 0;  

	ret = pinctrl_select_state(pins->p, pins->default_state);
	if (ret)
		dev_err(dev, "failed to activate default pinctrl state\n");

	return ret;
}

static int pinctrl_select_bound_state(struct device *dev,
				      struct pinctrl_state *state)
{
	struct dev_pin_info *pins = dev->pins;
	int ret;

	if (IS_ERR(state))
		return 0;  
	ret = pinctrl_select_state(pins->p, state);
	if (ret)
		dev_err(dev, "failed to activate pinctrl state %s\n",
			state->name);
	return ret;
}

 
int pinctrl_select_default_state(struct device *dev)
{
	if (!dev->pins)
		return 0;

	return pinctrl_select_bound_state(dev, dev->pins->default_state);
}
EXPORT_SYMBOL_GPL(pinctrl_select_default_state);

#ifdef CONFIG_PM

 
int pinctrl_pm_select_default_state(struct device *dev)
{
	return pinctrl_select_default_state(dev);
}
EXPORT_SYMBOL_GPL(pinctrl_pm_select_default_state);

 
int pinctrl_pm_select_sleep_state(struct device *dev)
{
	if (!dev->pins)
		return 0;

	return pinctrl_select_bound_state(dev, dev->pins->sleep_state);
}
EXPORT_SYMBOL_GPL(pinctrl_pm_select_sleep_state);

 
int pinctrl_pm_select_idle_state(struct device *dev)
{
	if (!dev->pins)
		return 0;

	return pinctrl_select_bound_state(dev, dev->pins->idle_state);
}
EXPORT_SYMBOL_GPL(pinctrl_pm_select_idle_state);
#endif

#ifdef CONFIG_DEBUG_FS

static int pinctrl_pins_show(struct seq_file *s, void *what)
{
	struct pinctrl_dev *pctldev = s->private;
	const struct pinctrl_ops *ops = pctldev->desc->pctlops;
	unsigned i, pin;
#ifdef CONFIG_GPIOLIB
	struct pinctrl_gpio_range *range;
	struct gpio_chip *chip;
	int gpio_num;
#endif

	seq_printf(s, "registered pins: %d\n", pctldev->desc->npins);

	mutex_lock(&pctldev->mutex);

	 
	for (i = 0; i < pctldev->desc->npins; i++) {
		struct pin_desc *desc;

		pin = pctldev->desc->pins[i].number;
		desc = pin_desc_get(pctldev, pin);
		 
		if (!desc)
			continue;

		seq_printf(s, "pin %d (%s) ", pin, desc->name);

#ifdef CONFIG_GPIOLIB
		gpio_num = -1;
		list_for_each_entry(range, &pctldev->gpio_ranges, node) {
			if ((pin >= range->pin_base) &&
			    (pin < (range->pin_base + range->npins))) {
				gpio_num = range->base + (pin - range->pin_base);
				break;
			}
		}
		if (gpio_num >= 0)
			 
			chip = gpiod_to_chip(gpio_to_desc(gpio_num));
		else
			chip = NULL;
		if (chip)
			seq_printf(s, "%u:%s ", gpio_num - chip->gpiodev->base, chip->label);
		else
			seq_puts(s, "0:? ");
#endif

		 
		if (ops->pin_dbg_show)
			ops->pin_dbg_show(pctldev, s, pin);

		seq_puts(s, "\n");
	}

	mutex_unlock(&pctldev->mutex);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(pinctrl_pins);

static int pinctrl_groups_show(struct seq_file *s, void *what)
{
	struct pinctrl_dev *pctldev = s->private;
	const struct pinctrl_ops *ops = pctldev->desc->pctlops;
	unsigned ngroups, selector = 0;

	mutex_lock(&pctldev->mutex);

	ngroups = ops->get_groups_count(pctldev);

	seq_puts(s, "registered pin groups:\n");
	while (selector < ngroups) {
		const unsigned *pins = NULL;
		unsigned num_pins = 0;
		const char *gname = ops->get_group_name(pctldev, selector);
		const char *pname;
		int ret = 0;
		int i;

		if (ops->get_group_pins)
			ret = ops->get_group_pins(pctldev, selector,
						  &pins, &num_pins);
		if (ret)
			seq_printf(s, "%s [ERROR GETTING PINS]\n",
				   gname);
		else {
			seq_printf(s, "group: %s\n", gname);
			for (i = 0; i < num_pins; i++) {
				pname = pin_get_name(pctldev, pins[i]);
				if (WARN_ON(!pname)) {
					mutex_unlock(&pctldev->mutex);
					return -EINVAL;
				}
				seq_printf(s, "pin %d (%s)\n", pins[i], pname);
			}
			seq_puts(s, "\n");
		}
		selector++;
	}

	mutex_unlock(&pctldev->mutex);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(pinctrl_groups);

static int pinctrl_gpioranges_show(struct seq_file *s, void *what)
{
	struct pinctrl_dev *pctldev = s->private;
	struct pinctrl_gpio_range *range;

	seq_puts(s, "GPIO ranges handled:\n");

	mutex_lock(&pctldev->mutex);

	 
	list_for_each_entry(range, &pctldev->gpio_ranges, node) {
		if (range->pins) {
			int a;
			seq_printf(s, "%u: %s GPIOS [%u - %u] PINS {",
				range->id, range->name,
				range->base, (range->base + range->npins - 1));
			for (a = 0; a < range->npins - 1; a++)
				seq_printf(s, "%u, ", range->pins[a]);
			seq_printf(s, "%u}\n", range->pins[a]);
		}
		else
			seq_printf(s, "%u: %s GPIOS [%u - %u] PINS [%u - %u]\n",
				range->id, range->name,
				range->base, (range->base + range->npins - 1),
				range->pin_base,
				(range->pin_base + range->npins - 1));
	}

	mutex_unlock(&pctldev->mutex);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(pinctrl_gpioranges);

static int pinctrl_devices_show(struct seq_file *s, void *what)
{
	struct pinctrl_dev *pctldev;

	seq_puts(s, "name [pinmux] [pinconf]\n");

	mutex_lock(&pinctrldev_list_mutex);

	list_for_each_entry(pctldev, &pinctrldev_list, node) {
		seq_printf(s, "%s ", pctldev->desc->name);
		if (pctldev->desc->pmxops)
			seq_puts(s, "yes ");
		else
			seq_puts(s, "no ");
		if (pctldev->desc->confops)
			seq_puts(s, "yes");
		else
			seq_puts(s, "no");
		seq_puts(s, "\n");
	}

	mutex_unlock(&pinctrldev_list_mutex);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(pinctrl_devices);

static inline const char *map_type(enum pinctrl_map_type type)
{
	static const char * const names[] = {
		"INVALID",
		"DUMMY_STATE",
		"MUX_GROUP",
		"CONFIGS_PIN",
		"CONFIGS_GROUP",
	};

	if (type >= ARRAY_SIZE(names))
		return "UNKNOWN";

	return names[type];
}

static int pinctrl_maps_show(struct seq_file *s, void *what)
{
	struct pinctrl_maps *maps_node;
	const struct pinctrl_map *map;

	seq_puts(s, "Pinctrl maps:\n");

	mutex_lock(&pinctrl_maps_mutex);
	for_each_pin_map(maps_node, map) {
		seq_printf(s, "device %s\nstate %s\ntype %s (%d)\n",
			   map->dev_name, map->name, map_type(map->type),
			   map->type);

		if (map->type != PIN_MAP_TYPE_DUMMY_STATE)
			seq_printf(s, "controlling device %s\n",
				   map->ctrl_dev_name);

		switch (map->type) {
		case PIN_MAP_TYPE_MUX_GROUP:
			pinmux_show_map(s, map);
			break;
		case PIN_MAP_TYPE_CONFIGS_PIN:
		case PIN_MAP_TYPE_CONFIGS_GROUP:
			pinconf_show_map(s, map);
			break;
		default:
			break;
		}

		seq_putc(s, '\n');
	}
	mutex_unlock(&pinctrl_maps_mutex);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(pinctrl_maps);

static int pinctrl_show(struct seq_file *s, void *what)
{
	struct pinctrl *p;
	struct pinctrl_state *state;
	struct pinctrl_setting *setting;

	seq_puts(s, "Requested pin control handlers their pinmux maps:\n");

	mutex_lock(&pinctrl_list_mutex);

	list_for_each_entry(p, &pinctrl_list, node) {
		seq_printf(s, "device: %s current state: %s\n",
			   dev_name(p->dev),
			   p->state ? p->state->name : "none");

		list_for_each_entry(state, &p->states, node) {
			seq_printf(s, "  state: %s\n", state->name);

			list_for_each_entry(setting, &state->settings, node) {
				struct pinctrl_dev *pctldev = setting->pctldev;

				seq_printf(s, "    type: %s controller %s ",
					   map_type(setting->type),
					   pinctrl_dev_get_name(pctldev));

				switch (setting->type) {
				case PIN_MAP_TYPE_MUX_GROUP:
					pinmux_show_setting(s, setting);
					break;
				case PIN_MAP_TYPE_CONFIGS_PIN:
				case PIN_MAP_TYPE_CONFIGS_GROUP:
					pinconf_show_setting(s, setting);
					break;
				default:
					break;
				}
			}
		}
	}

	mutex_unlock(&pinctrl_list_mutex);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(pinctrl);

static struct dentry *debugfs_root;

static void pinctrl_init_device_debugfs(struct pinctrl_dev *pctldev)
{
	struct dentry *device_root;
	const char *debugfs_name;

	if (pctldev->desc->name &&
			strcmp(dev_name(pctldev->dev), pctldev->desc->name)) {
		debugfs_name = devm_kasprintf(pctldev->dev, GFP_KERNEL,
				"%s-%s", dev_name(pctldev->dev),
				pctldev->desc->name);
		if (!debugfs_name) {
			pr_warn("failed to determine debugfs dir name for %s\n",
				dev_name(pctldev->dev));
			return;
		}
	} else {
		debugfs_name = dev_name(pctldev->dev);
	}

	device_root = debugfs_create_dir(debugfs_name, debugfs_root);
	pctldev->device_root = device_root;

	if (IS_ERR(device_root) || !device_root) {
		pr_warn("failed to create debugfs directory for %s\n",
			dev_name(pctldev->dev));
		return;
	}
	debugfs_create_file("pins", 0444,
			    device_root, pctldev, &pinctrl_pins_fops);
	debugfs_create_file("pingroups", 0444,
			    device_root, pctldev, &pinctrl_groups_fops);
	debugfs_create_file("gpio-ranges", 0444,
			    device_root, pctldev, &pinctrl_gpioranges_fops);
	if (pctldev->desc->pmxops)
		pinmux_init_device_debugfs(device_root, pctldev);
	if (pctldev->desc->confops)
		pinconf_init_device_debugfs(device_root, pctldev);
}

static void pinctrl_remove_device_debugfs(struct pinctrl_dev *pctldev)
{
	debugfs_remove_recursive(pctldev->device_root);
}

static void pinctrl_init_debugfs(void)
{
	debugfs_root = debugfs_create_dir("pinctrl", NULL);
	if (IS_ERR(debugfs_root) || !debugfs_root) {
		pr_warn("failed to create debugfs directory\n");
		debugfs_root = NULL;
		return;
	}

	debugfs_create_file("pinctrl-devices", 0444,
			    debugfs_root, NULL, &pinctrl_devices_fops);
	debugfs_create_file("pinctrl-maps", 0444,
			    debugfs_root, NULL, &pinctrl_maps_fops);
	debugfs_create_file("pinctrl-handles", 0444,
			    debugfs_root, NULL, &pinctrl_fops);
}

#else  

static void pinctrl_init_device_debugfs(struct pinctrl_dev *pctldev)
{
}

static void pinctrl_init_debugfs(void)
{
}

static void pinctrl_remove_device_debugfs(struct pinctrl_dev *pctldev)
{
}

#endif

static int pinctrl_check_ops(struct pinctrl_dev *pctldev)
{
	const struct pinctrl_ops *ops = pctldev->desc->pctlops;

	if (!ops ||
	    !ops->get_groups_count ||
	    !ops->get_group_name)
		return -EINVAL;

	return 0;
}

 
static struct pinctrl_dev *
pinctrl_init_controller(struct pinctrl_desc *pctldesc, struct device *dev,
			void *driver_data)
{
	struct pinctrl_dev *pctldev;
	int ret;

	if (!pctldesc)
		return ERR_PTR(-EINVAL);
	if (!pctldesc->name)
		return ERR_PTR(-EINVAL);

	pctldev = kzalloc(sizeof(*pctldev), GFP_KERNEL);
	if (!pctldev)
		return ERR_PTR(-ENOMEM);

	 
	pctldev->owner = pctldesc->owner;
	pctldev->desc = pctldesc;
	pctldev->driver_data = driver_data;
	INIT_RADIX_TREE(&pctldev->pin_desc_tree, GFP_KERNEL);
#ifdef CONFIG_GENERIC_PINCTRL_GROUPS
	INIT_RADIX_TREE(&pctldev->pin_group_tree, GFP_KERNEL);
#endif
#ifdef CONFIG_GENERIC_PINMUX_FUNCTIONS
	INIT_RADIX_TREE(&pctldev->pin_function_tree, GFP_KERNEL);
#endif
	INIT_LIST_HEAD(&pctldev->gpio_ranges);
	INIT_LIST_HEAD(&pctldev->node);
	pctldev->dev = dev;
	mutex_init(&pctldev->mutex);

	 
	ret = pinctrl_check_ops(pctldev);
	if (ret) {
		dev_err(dev, "pinctrl ops lacks necessary functions\n");
		goto out_err;
	}

	 
	if (pctldesc->pmxops) {
		ret = pinmux_check_ops(pctldev);
		if (ret)
			goto out_err;
	}

	 
	if (pctldesc->confops) {
		ret = pinconf_check_ops(pctldev);
		if (ret)
			goto out_err;
	}

	 
	dev_dbg(dev, "try to register %d pins ...\n",  pctldesc->npins);
	ret = pinctrl_register_pins(pctldev, pctldesc->pins, pctldesc->npins);
	if (ret) {
		dev_err(dev, "error during pin registration\n");
		pinctrl_free_pindescs(pctldev, pctldesc->pins,
				      pctldesc->npins);
		goto out_err;
	}

	return pctldev;

out_err:
	mutex_destroy(&pctldev->mutex);
	kfree(pctldev);
	return ERR_PTR(ret);
}

static int pinctrl_claim_hogs(struct pinctrl_dev *pctldev)
{
	pctldev->p = create_pinctrl(pctldev->dev, pctldev);
	if (PTR_ERR(pctldev->p) == -ENODEV) {
		dev_dbg(pctldev->dev, "no hogs found\n");

		return 0;
	}

	if (IS_ERR(pctldev->p)) {
		dev_err(pctldev->dev, "error claiming hogs: %li\n",
			PTR_ERR(pctldev->p));

		return PTR_ERR(pctldev->p);
	}

	pctldev->hog_default =
		pinctrl_lookup_state(pctldev->p, PINCTRL_STATE_DEFAULT);
	if (IS_ERR(pctldev->hog_default)) {
		dev_dbg(pctldev->dev,
			"failed to lookup the default state\n");
	} else {
		if (pinctrl_select_state(pctldev->p,
					 pctldev->hog_default))
			dev_err(pctldev->dev,
				"failed to select default state\n");
	}

	pctldev->hog_sleep =
		pinctrl_lookup_state(pctldev->p,
				     PINCTRL_STATE_SLEEP);
	if (IS_ERR(pctldev->hog_sleep))
		dev_dbg(pctldev->dev,
			"failed to lookup the sleep state\n");

	return 0;
}

int pinctrl_enable(struct pinctrl_dev *pctldev)
{
	int error;

	error = pinctrl_claim_hogs(pctldev);
	if (error) {
		dev_err(pctldev->dev, "could not claim hogs: %i\n",
			error);
		pinctrl_free_pindescs(pctldev, pctldev->desc->pins,
				      pctldev->desc->npins);
		mutex_destroy(&pctldev->mutex);
		kfree(pctldev);

		return error;
	}

	mutex_lock(&pinctrldev_list_mutex);
	list_add_tail(&pctldev->node, &pinctrldev_list);
	mutex_unlock(&pinctrldev_list_mutex);

	pinctrl_init_device_debugfs(pctldev);

	return 0;
}
EXPORT_SYMBOL_GPL(pinctrl_enable);

 
struct pinctrl_dev *pinctrl_register(struct pinctrl_desc *pctldesc,
				    struct device *dev, void *driver_data)
{
	struct pinctrl_dev *pctldev;
	int error;

	pctldev = pinctrl_init_controller(pctldesc, dev, driver_data);
	if (IS_ERR(pctldev))
		return pctldev;

	error = pinctrl_enable(pctldev);
	if (error)
		return ERR_PTR(error);

	return pctldev;
}
EXPORT_SYMBOL_GPL(pinctrl_register);

 
int pinctrl_register_and_init(struct pinctrl_desc *pctldesc,
			      struct device *dev, void *driver_data,
			      struct pinctrl_dev **pctldev)
{
	struct pinctrl_dev *p;

	p = pinctrl_init_controller(pctldesc, dev, driver_data);
	if (IS_ERR(p))
		return PTR_ERR(p);

	 
	*pctldev = p;

	return 0;
}
EXPORT_SYMBOL_GPL(pinctrl_register_and_init);

 
void pinctrl_unregister(struct pinctrl_dev *pctldev)
{
	struct pinctrl_gpio_range *range, *n;

	if (!pctldev)
		return;

	mutex_lock(&pctldev->mutex);
	pinctrl_remove_device_debugfs(pctldev);
	mutex_unlock(&pctldev->mutex);

	if (!IS_ERR_OR_NULL(pctldev->p))
		pinctrl_put(pctldev->p);

	mutex_lock(&pinctrldev_list_mutex);
	mutex_lock(&pctldev->mutex);
	 
	list_del(&pctldev->node);
	pinmux_generic_free_functions(pctldev);
	pinctrl_generic_free_groups(pctldev);
	 
	pinctrl_free_pindescs(pctldev, pctldev->desc->pins,
			      pctldev->desc->npins);
	 
	list_for_each_entry_safe(range, n, &pctldev->gpio_ranges, node)
		list_del(&range->node);

	mutex_unlock(&pctldev->mutex);
	mutex_destroy(&pctldev->mutex);
	kfree(pctldev);
	mutex_unlock(&pinctrldev_list_mutex);
}
EXPORT_SYMBOL_GPL(pinctrl_unregister);

static void devm_pinctrl_dev_release(struct device *dev, void *res)
{
	struct pinctrl_dev *pctldev = *(struct pinctrl_dev **)res;

	pinctrl_unregister(pctldev);
}

static int devm_pinctrl_dev_match(struct device *dev, void *res, void *data)
{
	struct pctldev **r = res;

	if (WARN_ON(!r || !*r))
		return 0;

	return *r == data;
}

 
struct pinctrl_dev *devm_pinctrl_register(struct device *dev,
					  struct pinctrl_desc *pctldesc,
					  void *driver_data)
{
	struct pinctrl_dev **ptr, *pctldev;

	ptr = devres_alloc(devm_pinctrl_dev_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	pctldev = pinctrl_register(pctldesc, dev, driver_data);
	if (IS_ERR(pctldev)) {
		devres_free(ptr);
		return pctldev;
	}

	*ptr = pctldev;
	devres_add(dev, ptr);

	return pctldev;
}
EXPORT_SYMBOL_GPL(devm_pinctrl_register);

 
int devm_pinctrl_register_and_init(struct device *dev,
				   struct pinctrl_desc *pctldesc,
				   void *driver_data,
				   struct pinctrl_dev **pctldev)
{
	struct pinctrl_dev **ptr;
	int error;

	ptr = devres_alloc(devm_pinctrl_dev_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	error = pinctrl_register_and_init(pctldesc, dev, driver_data, pctldev);
	if (error) {
		devres_free(ptr);
		return error;
	}

	*ptr = *pctldev;
	devres_add(dev, ptr);

	return 0;
}
EXPORT_SYMBOL_GPL(devm_pinctrl_register_and_init);

 
void devm_pinctrl_unregister(struct device *dev, struct pinctrl_dev *pctldev)
{
	WARN_ON(devres_release(dev, devm_pinctrl_dev_release,
			       devm_pinctrl_dev_match, pctldev));
}
EXPORT_SYMBOL_GPL(devm_pinctrl_unregister);

static int __init pinctrl_init(void)
{
	pr_info("initialized pinctrl subsystem\n");
	pinctrl_init_debugfs();
	return 0;
}

 
core_initcall(pinctrl_init);
