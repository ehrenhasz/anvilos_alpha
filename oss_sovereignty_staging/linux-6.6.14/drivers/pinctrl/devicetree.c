
 

#include <linux/device.h>
#include <linux/of.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/slab.h>

#include "core.h"
#include "devicetree.h"

 
struct pinctrl_dt_map {
	struct list_head node;
	struct pinctrl_dev *pctldev;
	struct pinctrl_map *map;
	unsigned num_maps;
};

static void dt_free_map(struct pinctrl_dev *pctldev,
		     struct pinctrl_map *map, unsigned num_maps)
{
	int i;

	for (i = 0; i < num_maps; ++i) {
		kfree_const(map[i].dev_name);
		map[i].dev_name = NULL;
	}

	if (pctldev) {
		const struct pinctrl_ops *ops = pctldev->desc->pctlops;
		if (ops->dt_free_map)
			ops->dt_free_map(pctldev, map, num_maps);
	} else {
		 
		kfree(map);
	}
}

void pinctrl_dt_free_maps(struct pinctrl *p)
{
	struct pinctrl_dt_map *dt_map, *n1;

	list_for_each_entry_safe(dt_map, n1, &p->dt_maps, node) {
		pinctrl_unregister_mappings(dt_map->map);
		list_del(&dt_map->node);
		dt_free_map(dt_map->pctldev, dt_map->map,
			    dt_map->num_maps);
		kfree(dt_map);
	}

	of_node_put(p->dev->of_node);
}

static int dt_remember_or_free_map(struct pinctrl *p, const char *statename,
				   struct pinctrl_dev *pctldev,
				   struct pinctrl_map *map, unsigned num_maps)
{
	int i;
	struct pinctrl_dt_map *dt_map;

	 
	for (i = 0; i < num_maps; i++) {
		const char *devname;

		devname = kstrdup_const(dev_name(p->dev), GFP_KERNEL);
		if (!devname)
			goto err_free_map;

		map[i].dev_name = devname;
		map[i].name = statename;
		if (pctldev)
			map[i].ctrl_dev_name = dev_name(pctldev->dev);
	}

	 
	dt_map = kzalloc(sizeof(*dt_map), GFP_KERNEL);
	if (!dt_map)
		goto err_free_map;

	dt_map->pctldev = pctldev;
	dt_map->map = map;
	dt_map->num_maps = num_maps;
	list_add_tail(&dt_map->node, &p->dt_maps);

	return pinctrl_register_mappings(map, num_maps);

err_free_map:
	dt_free_map(pctldev, map, num_maps);
	return -ENOMEM;
}

struct pinctrl_dev *of_pinctrl_get(struct device_node *np)
{
	return get_pinctrl_dev_from_of_node(np);
}
EXPORT_SYMBOL_GPL(of_pinctrl_get);

static int dt_to_map_one_config(struct pinctrl *p,
				struct pinctrl_dev *hog_pctldev,
				const char *statename,
				struct device_node *np_config)
{
	struct pinctrl_dev *pctldev = NULL;
	struct device_node *np_pctldev;
	const struct pinctrl_ops *ops;
	int ret;
	struct pinctrl_map *map;
	unsigned num_maps;
	bool allow_default = false;

	 
	np_pctldev = of_node_get(np_config);
	for (;;) {
		if (!allow_default)
			allow_default = of_property_read_bool(np_pctldev,
							      "pinctrl-use-default");

		np_pctldev = of_get_next_parent(np_pctldev);
		if (!np_pctldev || of_node_is_root(np_pctldev)) {
			of_node_put(np_pctldev);
			ret = -ENODEV;
			 
			if (IS_ENABLED(CONFIG_MODULES) && !allow_default && ret < 0)
				ret = -EPROBE_DEFER;
			return ret;
		}
		 
		if (hog_pctldev && (np_pctldev == p->dev->of_node)) {
			pctldev = hog_pctldev;
			break;
		}
		pctldev = get_pinctrl_dev_from_of_node(np_pctldev);
		if (pctldev)
			break;
		 
		if (np_pctldev == p->dev->of_node) {
			of_node_put(np_pctldev);
			return -ENODEV;
		}
	}
	of_node_put(np_pctldev);

	 
	ops = pctldev->desc->pctlops;
	if (!ops->dt_node_to_map) {
		dev_err(p->dev, "pctldev %s doesn't support DT\n",
			dev_name(pctldev->dev));
		return -ENODEV;
	}
	ret = ops->dt_node_to_map(pctldev, np_config, &map, &num_maps);
	if (ret < 0)
		return ret;
	else if (num_maps == 0) {
		 
		dev_info(p->dev,
			 "there is not valid maps for state %s\n", statename);
		return 0;
	}

	 
	return dt_remember_or_free_map(p, statename, pctldev, map, num_maps);
}

static int dt_remember_dummy_state(struct pinctrl *p, const char *statename)
{
	struct pinctrl_map *map;

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map)
		return -ENOMEM;

	 
	map->type = PIN_MAP_TYPE_DUMMY_STATE;

	return dt_remember_or_free_map(p, statename, NULL, map, 1);
}

int pinctrl_dt_to_map(struct pinctrl *p, struct pinctrl_dev *pctldev)
{
	struct device_node *np = p->dev->of_node;
	int state, ret;
	char *propname;
	struct property *prop;
	const char *statename;
	const __be32 *list;
	int size, config;
	phandle phandle;
	struct device_node *np_config;

	 
	if (!np) {
		if (of_have_populated_dt())
			dev_dbg(p->dev,
				"no of_node; not parsing pinctrl DT\n");
		return 0;
	}

	 
	of_node_get(np);

	 
	for (state = 0; ; state++) {
		 
		propname = kasprintf(GFP_KERNEL, "pinctrl-%d", state);
		if (!propname)
			return -ENOMEM;
		prop = of_find_property(np, propname, &size);
		kfree(propname);
		if (!prop) {
			if (state == 0) {
				of_node_put(np);
				return -ENODEV;
			}
			break;
		}
		list = prop->value;
		size /= sizeof(*list);

		 
		ret = of_property_read_string_index(np, "pinctrl-names",
						    state, &statename);
		 
		if (ret < 0)
			statename = prop->name + strlen("pinctrl-");

		 
		for (config = 0; config < size; config++) {
			phandle = be32_to_cpup(list++);

			 
			np_config = of_find_node_by_phandle(phandle);
			if (!np_config) {
				dev_err(p->dev,
					"prop %s index %i invalid phandle\n",
					prop->name, config);
				ret = -EINVAL;
				goto err;
			}

			 
			ret = dt_to_map_one_config(p, pctldev, statename,
						   np_config);
			of_node_put(np_config);
			if (ret < 0)
				goto err;
		}

		 
		if (!size) {
			ret = dt_remember_dummy_state(p, statename);
			if (ret < 0)
				goto err;
		}
	}

	return 0;

err:
	pinctrl_dt_free_maps(p);
	return ret;
}

 
static int pinctrl_find_cells_size(const struct device_node *np)
{
	const char *cells_name = "#pinctrl-cells";
	int cells_size, error;

	error = of_property_read_u32(np->parent, cells_name, &cells_size);
	if (error) {
		error = of_property_read_u32(np->parent->parent,
					     cells_name, &cells_size);
		if (error)
			return -ENOENT;
	}

	return cells_size;
}

 
static int pinctrl_get_list_and_count(const struct device_node *np,
				      const char *list_name,
				      const __be32 **list,
				      int *cells_size,
				      int *nr_elements)
{
	int size;

	*cells_size = 0;
	*nr_elements = 0;

	*list = of_get_property(np, list_name, &size);
	if (!*list)
		return -ENOENT;

	*cells_size = pinctrl_find_cells_size(np);
	if (*cells_size < 0)
		return -ENOENT;

	 
	*nr_elements = (size / sizeof(**list)) / (*cells_size + 1);

	return 0;
}

 
int pinctrl_count_index_with_args(const struct device_node *np,
				  const char *list_name)
{
	const __be32 *list;
	int size, nr_cells, error;

	error = pinctrl_get_list_and_count(np, list_name, &list,
					   &nr_cells, &size);
	if (error)
		return error;

	return size;
}
EXPORT_SYMBOL_GPL(pinctrl_count_index_with_args);

 
static int pinctrl_copy_args(const struct device_node *np,
			     const __be32 *list,
			     int index, int nr_cells, int nr_elem,
			     struct of_phandle_args *out_args)
{
	int i;

	memset(out_args, 0, sizeof(*out_args));
	out_args->np = (struct device_node *)np;
	out_args->args_count = nr_cells + 1;

	if (index >= nr_elem)
		return -EINVAL;

	list += index * (nr_cells + 1);

	for (i = 0; i < nr_cells + 1; i++)
		out_args->args[i] = be32_to_cpup(list++);

	return 0;
}

 
int pinctrl_parse_index_with_args(const struct device_node *np,
				  const char *list_name, int index,
				  struct of_phandle_args *out_args)
{
	const __be32 *list;
	int nr_elem, nr_cells, error;

	error = pinctrl_get_list_and_count(np, list_name, &list,
					   &nr_cells, &nr_elem);
	if (error || !nr_cells)
		return error;

	error = pinctrl_copy_args(np, list, index, nr_cells, nr_elem,
				  out_args);
	if (error)
		return error;

	return 0;
}
EXPORT_SYMBOL_GPL(pinctrl_parse_index_with_args);
