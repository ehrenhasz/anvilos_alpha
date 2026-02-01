
 
#include <linux/component.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/debugfs.h>

 

struct component;

struct component_match_array {
	void *data;
	int (*compare)(struct device *, void *);
	int (*compare_typed)(struct device *, int, void *);
	void (*release)(struct device *, void *);
	struct component *component;
	bool duplicate;
};

struct component_match {
	size_t alloc;
	size_t num;
	struct component_match_array *compare;
};

struct aggregate_device {
	struct list_head node;
	bool bound;

	const struct component_master_ops *ops;
	struct device *parent;
	struct component_match *match;
};

struct component {
	struct list_head node;
	struct aggregate_device *adev;
	bool bound;

	const struct component_ops *ops;
	int subcomponent;
	struct device *dev;
};

static DEFINE_MUTEX(component_mutex);
static LIST_HEAD(component_list);
static LIST_HEAD(aggregate_devices);

#ifdef CONFIG_DEBUG_FS

static struct dentry *component_debugfs_dir;

static int component_devices_show(struct seq_file *s, void *data)
{
	struct aggregate_device *m = s->private;
	struct component_match *match = m->match;
	size_t i;

	mutex_lock(&component_mutex);
	seq_printf(s, "%-40s %20s\n", "aggregate_device name", "status");
	seq_puts(s, "-------------------------------------------------------------\n");
	seq_printf(s, "%-40s %20s\n\n",
		   dev_name(m->parent), m->bound ? "bound" : "not bound");

	seq_printf(s, "%-40s %20s\n", "device name", "status");
	seq_puts(s, "-------------------------------------------------------------\n");
	for (i = 0; i < match->num; i++) {
		struct component *component = match->compare[i].component;

		seq_printf(s, "%-40s %20s\n",
			   component ? dev_name(component->dev) : "(unknown)",
			   component ? (component->bound ? "bound" : "not bound") : "not registered");
	}
	mutex_unlock(&component_mutex);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(component_devices);

static int __init component_debug_init(void)
{
	component_debugfs_dir = debugfs_create_dir("device_component", NULL);

	return 0;
}

core_initcall(component_debug_init);

static void component_debugfs_add(struct aggregate_device *m)
{
	debugfs_create_file(dev_name(m->parent), 0444, component_debugfs_dir, m,
			    &component_devices_fops);
}

static void component_debugfs_del(struct aggregate_device *m)
{
	debugfs_lookup_and_remove(dev_name(m->parent), component_debugfs_dir);
}

#else

static void component_debugfs_add(struct aggregate_device *m)
{ }

static void component_debugfs_del(struct aggregate_device *m)
{ }

#endif

static struct aggregate_device *__aggregate_find(struct device *parent,
	const struct component_master_ops *ops)
{
	struct aggregate_device *m;

	list_for_each_entry(m, &aggregate_devices, node)
		if (m->parent == parent && (!ops || m->ops == ops))
			return m;

	return NULL;
}

static struct component *find_component(struct aggregate_device *adev,
	struct component_match_array *mc)
{
	struct component *c;

	list_for_each_entry(c, &component_list, node) {
		if (c->adev && c->adev != adev)
			continue;

		if (mc->compare && mc->compare(c->dev, mc->data))
			return c;

		if (mc->compare_typed &&
		    mc->compare_typed(c->dev, c->subcomponent, mc->data))
			return c;
	}

	return NULL;
}

static int find_components(struct aggregate_device *adev)
{
	struct component_match *match = adev->match;
	size_t i;
	int ret = 0;

	 
	for (i = 0; i < match->num; i++) {
		struct component_match_array *mc = &match->compare[i];
		struct component *c;

		dev_dbg(adev->parent, "Looking for component %zu\n", i);

		if (match->compare[i].component)
			continue;

		c = find_component(adev, mc);
		if (!c) {
			ret = -ENXIO;
			break;
		}

		dev_dbg(adev->parent, "found component %s, duplicate %u\n",
			dev_name(c->dev), !!c->adev);

		 
		match->compare[i].duplicate = !!c->adev;
		match->compare[i].component = c;
		c->adev = adev;
	}
	return ret;
}

 
static void remove_component(struct aggregate_device *adev, struct component *c)
{
	size_t i;

	 
	for (i = 0; i < adev->match->num; i++)
		if (adev->match->compare[i].component == c)
			adev->match->compare[i].component = NULL;
}

 
static int try_to_bring_up_aggregate_device(struct aggregate_device *adev,
	struct component *component)
{
	int ret;

	dev_dbg(adev->parent, "trying to bring up adev\n");

	if (find_components(adev)) {
		dev_dbg(adev->parent, "master has incomplete components\n");
		return 0;
	}

	if (component && component->adev != adev) {
		dev_dbg(adev->parent, "master is not for this component (%s)\n",
			dev_name(component->dev));
		return 0;
	}

	if (!devres_open_group(adev->parent, adev, GFP_KERNEL))
		return -ENOMEM;

	 
	ret = adev->ops->bind(adev->parent);
	if (ret < 0) {
		devres_release_group(adev->parent, NULL);
		if (ret != -EPROBE_DEFER)
			dev_info(adev->parent, "adev bind failed: %d\n", ret);
		return ret;
	}

	devres_close_group(adev->parent, NULL);
	adev->bound = true;
	return 1;
}

static int try_to_bring_up_masters(struct component *component)
{
	struct aggregate_device *adev;
	int ret = 0;

	list_for_each_entry(adev, &aggregate_devices, node) {
		if (!adev->bound) {
			ret = try_to_bring_up_aggregate_device(adev, component);
			if (ret != 0)
				break;
		}
	}

	return ret;
}

static void take_down_aggregate_device(struct aggregate_device *adev)
{
	if (adev->bound) {
		adev->ops->unbind(adev->parent);
		devres_release_group(adev->parent, adev);
		adev->bound = false;
	}
}

 
int component_compare_of(struct device *dev, void *data)
{
	return device_match_of_node(dev, data);
}
EXPORT_SYMBOL_GPL(component_compare_of);

 
void component_release_of(struct device *dev, void *data)
{
	of_node_put(data);
}
EXPORT_SYMBOL_GPL(component_release_of);

 
int component_compare_dev(struct device *dev, void *data)
{
	return dev == data;
}
EXPORT_SYMBOL_GPL(component_compare_dev);

 
int component_compare_dev_name(struct device *dev, void *data)
{
	return device_match_name(dev, data);
}
EXPORT_SYMBOL_GPL(component_compare_dev_name);

static void devm_component_match_release(struct device *parent, void *res)
{
	struct component_match *match = res;
	unsigned int i;

	for (i = 0; i < match->num; i++) {
		struct component_match_array *mc = &match->compare[i];

		if (mc->release)
			mc->release(parent, mc->data);
	}

	kfree(match->compare);
}

static int component_match_realloc(struct component_match *match, size_t num)
{
	struct component_match_array *new;

	if (match->alloc == num)
		return 0;

	new = kmalloc_array(num, sizeof(*new), GFP_KERNEL);
	if (!new)
		return -ENOMEM;

	if (match->compare) {
		memcpy(new, match->compare, sizeof(*new) *
					    min(match->num, num));
		kfree(match->compare);
	}
	match->compare = new;
	match->alloc = num;

	return 0;
}

static void __component_match_add(struct device *parent,
	struct component_match **matchptr,
	void (*release)(struct device *, void *),
	int (*compare)(struct device *, void *),
	int (*compare_typed)(struct device *, int, void *),
	void *compare_data)
{
	struct component_match *match = *matchptr;

	if (IS_ERR(match))
		return;

	if (!match) {
		match = devres_alloc(devm_component_match_release,
				     sizeof(*match), GFP_KERNEL);
		if (!match) {
			*matchptr = ERR_PTR(-ENOMEM);
			return;
		}

		devres_add(parent, match);

		*matchptr = match;
	}

	if (match->num == match->alloc) {
		size_t new_size = match->alloc + 16;
		int ret;

		ret = component_match_realloc(match, new_size);
		if (ret) {
			*matchptr = ERR_PTR(ret);
			return;
		}
	}

	match->compare[match->num].compare = compare;
	match->compare[match->num].compare_typed = compare_typed;
	match->compare[match->num].release = release;
	match->compare[match->num].data = compare_data;
	match->compare[match->num].component = NULL;
	match->num++;
}

 
void component_match_add_release(struct device *parent,
	struct component_match **matchptr,
	void (*release)(struct device *, void *),
	int (*compare)(struct device *, void *), void *compare_data)
{
	__component_match_add(parent, matchptr, release, compare, NULL,
			      compare_data);
}
EXPORT_SYMBOL(component_match_add_release);

 
void component_match_add_typed(struct device *parent,
	struct component_match **matchptr,
	int (*compare_typed)(struct device *, int, void *), void *compare_data)
{
	__component_match_add(parent, matchptr, NULL, NULL, compare_typed,
			      compare_data);
}
EXPORT_SYMBOL(component_match_add_typed);

static void free_aggregate_device(struct aggregate_device *adev)
{
	struct component_match *match = adev->match;
	int i;

	component_debugfs_del(adev);
	list_del(&adev->node);

	if (match) {
		for (i = 0; i < match->num; i++) {
			struct component *c = match->compare[i].component;
			if (c)
				c->adev = NULL;
		}
	}

	kfree(adev);
}

 
int component_master_add_with_match(struct device *parent,
	const struct component_master_ops *ops,
	struct component_match *match)
{
	struct aggregate_device *adev;
	int ret;

	 
	ret = component_match_realloc(match, match->num);
	if (ret)
		return ret;

	adev = kzalloc(sizeof(*adev), GFP_KERNEL);
	if (!adev)
		return -ENOMEM;

	adev->parent = parent;
	adev->ops = ops;
	adev->match = match;

	component_debugfs_add(adev);
	 
	mutex_lock(&component_mutex);
	list_add(&adev->node, &aggregate_devices);

	ret = try_to_bring_up_aggregate_device(adev, NULL);

	if (ret < 0)
		free_aggregate_device(adev);

	mutex_unlock(&component_mutex);

	return ret < 0 ? ret : 0;
}
EXPORT_SYMBOL_GPL(component_master_add_with_match);

 
void component_master_del(struct device *parent,
	const struct component_master_ops *ops)
{
	struct aggregate_device *adev;

	mutex_lock(&component_mutex);
	adev = __aggregate_find(parent, ops);
	if (adev) {
		take_down_aggregate_device(adev);
		free_aggregate_device(adev);
	}
	mutex_unlock(&component_mutex);
}
EXPORT_SYMBOL_GPL(component_master_del);

static void component_unbind(struct component *component,
	struct aggregate_device *adev, void *data)
{
	WARN_ON(!component->bound);

	if (component->ops && component->ops->unbind)
		component->ops->unbind(component->dev, adev->parent, data);
	component->bound = false;

	 
	devres_release_group(component->dev, component);
}

 
void component_unbind_all(struct device *parent, void *data)
{
	struct aggregate_device *adev;
	struct component *c;
	size_t i;

	WARN_ON(!mutex_is_locked(&component_mutex));

	adev = __aggregate_find(parent, NULL);
	if (!adev)
		return;

	 
	for (i = adev->match->num; i--; )
		if (!adev->match->compare[i].duplicate) {
			c = adev->match->compare[i].component;
			component_unbind(c, adev, data);
		}
}
EXPORT_SYMBOL_GPL(component_unbind_all);

static int component_bind(struct component *component, struct aggregate_device *adev,
	void *data)
{
	int ret;

	 
	if (!devres_open_group(adev->parent, NULL, GFP_KERNEL))
		return -ENOMEM;

	 
	if (!devres_open_group(component->dev, component, GFP_KERNEL)) {
		devres_release_group(adev->parent, NULL);
		return -ENOMEM;
	}

	dev_dbg(adev->parent, "binding %s (ops %ps)\n",
		dev_name(component->dev), component->ops);

	ret = component->ops->bind(component->dev, adev->parent, data);
	if (!ret) {
		component->bound = true;

		 
		devres_close_group(component->dev, NULL);
		devres_remove_group(adev->parent, NULL);

		dev_info(adev->parent, "bound %s (ops %ps)\n",
			 dev_name(component->dev), component->ops);
	} else {
		devres_release_group(component->dev, NULL);
		devres_release_group(adev->parent, NULL);

		if (ret != -EPROBE_DEFER)
			dev_err(adev->parent, "failed to bind %s (ops %ps): %d\n",
				dev_name(component->dev), component->ops, ret);
	}

	return ret;
}

 
int component_bind_all(struct device *parent, void *data)
{
	struct aggregate_device *adev;
	struct component *c;
	size_t i;
	int ret = 0;

	WARN_ON(!mutex_is_locked(&component_mutex));

	adev = __aggregate_find(parent, NULL);
	if (!adev)
		return -EINVAL;

	 
	for (i = 0; i < adev->match->num; i++)
		if (!adev->match->compare[i].duplicate) {
			c = adev->match->compare[i].component;
			ret = component_bind(c, adev, data);
			if (ret)
				break;
		}

	if (ret != 0) {
		for (; i > 0; i--)
			if (!adev->match->compare[i - 1].duplicate) {
				c = adev->match->compare[i - 1].component;
				component_unbind(c, adev, data);
			}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(component_bind_all);

static int __component_add(struct device *dev, const struct component_ops *ops,
	int subcomponent)
{
	struct component *component;
	int ret;

	component = kzalloc(sizeof(*component), GFP_KERNEL);
	if (!component)
		return -ENOMEM;

	component->ops = ops;
	component->dev = dev;
	component->subcomponent = subcomponent;

	dev_dbg(dev, "adding component (ops %ps)\n", ops);

	mutex_lock(&component_mutex);
	list_add_tail(&component->node, &component_list);

	ret = try_to_bring_up_masters(component);
	if (ret < 0) {
		if (component->adev)
			remove_component(component->adev, component);
		list_del(&component->node);

		kfree(component);
	}
	mutex_unlock(&component_mutex);

	return ret < 0 ? ret : 0;
}

 
int component_add_typed(struct device *dev, const struct component_ops *ops,
	int subcomponent)
{
	if (WARN_ON(subcomponent == 0))
		return -EINVAL;

	return __component_add(dev, ops, subcomponent);
}
EXPORT_SYMBOL_GPL(component_add_typed);

 
int component_add(struct device *dev, const struct component_ops *ops)
{
	return __component_add(dev, ops, 0);
}
EXPORT_SYMBOL_GPL(component_add);

 
void component_del(struct device *dev, const struct component_ops *ops)
{
	struct component *c, *component = NULL;

	mutex_lock(&component_mutex);
	list_for_each_entry(c, &component_list, node)
		if (c->dev == dev && c->ops == ops) {
			list_del(&c->node);
			component = c;
			break;
		}

	if (component && component->adev) {
		take_down_aggregate_device(component->adev);
		remove_component(component->adev, component);
	}

	mutex_unlock(&component_mutex);

	WARN_ON(!component);
	kfree(component);
}
EXPORT_SYMBOL_GPL(component_del);
