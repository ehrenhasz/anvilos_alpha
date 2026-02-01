
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpu.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/pm_domain.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/energy_model.h>

#include "opp.h"

 
static LIST_HEAD(lazy_opp_tables);

 
static struct device_node *_opp_of_get_opp_desc_node(struct device_node *np,
						     int index)
{
	 
	return of_parse_phandle(np, "operating-points-v2", index);
}

 
struct device_node *dev_pm_opp_of_get_opp_desc_node(struct device *dev)
{
	return _opp_of_get_opp_desc_node(dev->of_node, 0);
}
EXPORT_SYMBOL_GPL(dev_pm_opp_of_get_opp_desc_node);

struct opp_table *_managed_opp(struct device *dev, int index)
{
	struct opp_table *opp_table, *managed_table = NULL;
	struct device_node *np;

	np = _opp_of_get_opp_desc_node(dev->of_node, index);
	if (!np)
		return NULL;

	list_for_each_entry(opp_table, &opp_tables, node) {
		if (opp_table->np == np) {
			 
			if (opp_table->shared_opp == OPP_TABLE_ACCESS_SHARED) {
				_get_opp_table_kref(opp_table);
				managed_table = opp_table;
			}

			break;
		}
	}

	of_node_put(np);

	return managed_table;
}

 
static struct dev_pm_opp *_find_opp_of_np(struct opp_table *opp_table,
					  struct device_node *opp_np)
{
	struct dev_pm_opp *opp;

	mutex_lock(&opp_table->lock);

	list_for_each_entry(opp, &opp_table->opp_list, node) {
		if (opp->np == opp_np) {
			dev_pm_opp_get(opp);
			mutex_unlock(&opp_table->lock);
			return opp;
		}
	}

	mutex_unlock(&opp_table->lock);

	return NULL;
}

static struct device_node *of_parse_required_opp(struct device_node *np,
						 int index)
{
	return of_parse_phandle(np, "required-opps", index);
}

 
static struct opp_table *_find_table_of_opp_np(struct device_node *opp_np)
{
	struct opp_table *opp_table;
	struct device_node *opp_table_np;

	opp_table_np = of_get_parent(opp_np);
	if (!opp_table_np)
		goto err;

	 
	of_node_put(opp_table_np);

	mutex_lock(&opp_table_lock);
	list_for_each_entry(opp_table, &opp_tables, node) {
		if (opp_table_np == opp_table->np) {
			_get_opp_table_kref(opp_table);
			mutex_unlock(&opp_table_lock);
			return opp_table;
		}
	}
	mutex_unlock(&opp_table_lock);

err:
	return ERR_PTR(-ENODEV);
}

 
static void _opp_table_free_required_tables(struct opp_table *opp_table)
{
	struct opp_table **required_opp_tables = opp_table->required_opp_tables;
	int i;

	if (!required_opp_tables)
		return;

	for (i = 0; i < opp_table->required_opp_count; i++) {
		if (IS_ERR_OR_NULL(required_opp_tables[i]))
			continue;

		dev_pm_opp_put_opp_table(required_opp_tables[i]);
	}

	kfree(required_opp_tables);

	opp_table->required_opp_count = 0;
	opp_table->required_opp_tables = NULL;

	mutex_lock(&opp_table_lock);
	list_del(&opp_table->lazy);
	mutex_unlock(&opp_table_lock);
}

 
static void _opp_table_alloc_required_tables(struct opp_table *opp_table,
					     struct device *dev,
					     struct device_node *opp_np)
{
	struct opp_table **required_opp_tables;
	struct device_node *required_np, *np;
	bool lazy = false;
	int count, i;

	 
	np = of_get_next_available_child(opp_np, NULL);
	if (!np) {
		dev_warn(dev, "Empty OPP table\n");

		return;
	}

	count = of_count_phandle_with_args(np, "required-opps", NULL);
	if (count <= 0)
		goto put_np;

	required_opp_tables = kcalloc(count, sizeof(*required_opp_tables),
				      GFP_KERNEL);
	if (!required_opp_tables)
		goto put_np;

	opp_table->required_opp_tables = required_opp_tables;
	opp_table->required_opp_count = count;

	for (i = 0; i < count; i++) {
		required_np = of_parse_required_opp(np, i);
		if (!required_np)
			goto free_required_tables;

		required_opp_tables[i] = _find_table_of_opp_np(required_np);
		of_node_put(required_np);

		if (IS_ERR(required_opp_tables[i]))
			lazy = true;
	}

	 
	if (lazy) {
		 
		mutex_lock(&opp_table_lock);
		list_add(&opp_table->lazy, &lazy_opp_tables);
		mutex_unlock(&opp_table_lock);
	}
	else
		_update_set_required_opps(opp_table);

	goto put_np;

free_required_tables:
	_opp_table_free_required_tables(opp_table);
put_np:
	of_node_put(np);
}

void _of_init_opp_table(struct opp_table *opp_table, struct device *dev,
			int index)
{
	struct device_node *np, *opp_np;
	u32 val;

	 
	np = of_node_get(dev->of_node);
	if (!np)
		return;

	if (!of_property_read_u32(np, "clock-latency", &val))
		opp_table->clock_latency_ns_max = val;
	of_property_read_u32(np, "voltage-tolerance",
			     &opp_table->voltage_tolerance_v1);

	if (of_property_present(np, "#power-domain-cells"))
		opp_table->is_genpd = true;

	 
	opp_np = _opp_of_get_opp_desc_node(np, index);
	of_node_put(np);

	if (!opp_np)
		return;

	if (of_property_read_bool(opp_np, "opp-shared"))
		opp_table->shared_opp = OPP_TABLE_ACCESS_SHARED;
	else
		opp_table->shared_opp = OPP_TABLE_ACCESS_EXCLUSIVE;

	opp_table->np = opp_np;

	_opp_table_alloc_required_tables(opp_table, dev, opp_np);
}

void _of_clear_opp_table(struct opp_table *opp_table)
{
	_opp_table_free_required_tables(opp_table);
	of_node_put(opp_table->np);
}

 
static void _of_opp_free_required_opps(struct opp_table *opp_table,
				       struct dev_pm_opp *opp)
{
	struct dev_pm_opp **required_opps = opp->required_opps;
	int i;

	if (!required_opps)
		return;

	for (i = 0; i < opp_table->required_opp_count; i++) {
		if (!required_opps[i])
			continue;

		 
		dev_pm_opp_put(required_opps[i]);
	}

	opp->required_opps = NULL;
	kfree(required_opps);
}

void _of_clear_opp(struct opp_table *opp_table, struct dev_pm_opp *opp)
{
	_of_opp_free_required_opps(opp_table, opp);
	of_node_put(opp->np);
}

 
static int _of_opp_alloc_required_opps(struct opp_table *opp_table,
				       struct dev_pm_opp *opp)
{
	struct dev_pm_opp **required_opps;
	struct opp_table *required_table;
	struct device_node *np;
	int i, ret, count = opp_table->required_opp_count;

	if (!count)
		return 0;

	required_opps = kcalloc(count, sizeof(*required_opps), GFP_KERNEL);
	if (!required_opps)
		return -ENOMEM;

	opp->required_opps = required_opps;

	for (i = 0; i < count; i++) {
		required_table = opp_table->required_opp_tables[i];

		 
		if (IS_ERR_OR_NULL(required_table))
			continue;

		np = of_parse_required_opp(opp->np, i);
		if (unlikely(!np)) {
			ret = -ENODEV;
			goto free_required_opps;
		}

		required_opps[i] = _find_opp_of_np(required_table, np);
		of_node_put(np);

		if (!required_opps[i]) {
			pr_err("%s: Unable to find required OPP node: %pOF (%d)\n",
			       __func__, opp->np, i);
			ret = -ENODEV;
			goto free_required_opps;
		}
	}

	return 0;

free_required_opps:
	_of_opp_free_required_opps(opp_table, opp);

	return ret;
}

 
static int lazy_link_required_opps(struct opp_table *opp_table,
				   struct opp_table *new_table, int index)
{
	struct device_node *required_np;
	struct dev_pm_opp *opp;

	list_for_each_entry(opp, &opp_table->opp_list, node) {
		required_np = of_parse_required_opp(opp->np, index);
		if (unlikely(!required_np))
			return -ENODEV;

		opp->required_opps[index] = _find_opp_of_np(new_table, required_np);
		of_node_put(required_np);

		if (!opp->required_opps[index]) {
			pr_err("%s: Unable to find required OPP node: %pOF (%d)\n",
			       __func__, opp->np, index);
			return -ENODEV;
		}
	}

	return 0;
}

 
static void lazy_link_required_opp_table(struct opp_table *new_table)
{
	struct opp_table *opp_table, *temp, **required_opp_tables;
	struct device_node *required_np, *opp_np, *required_table_np;
	struct dev_pm_opp *opp;
	int i, ret;

	mutex_lock(&opp_table_lock);

	list_for_each_entry_safe(opp_table, temp, &lazy_opp_tables, lazy) {
		bool lazy = false;

		 
		opp_np = of_get_next_available_child(opp_table->np, NULL);

		for (i = 0; i < opp_table->required_opp_count; i++) {
			required_opp_tables = opp_table->required_opp_tables;

			 
			if (!IS_ERR(required_opp_tables[i]))
				continue;

			 
			required_np = of_parse_required_opp(opp_np, i);
			required_table_np = of_get_parent(required_np);

			of_node_put(required_table_np);
			of_node_put(required_np);

			 
			if (required_table_np != new_table->np) {
				lazy = true;
				continue;
			}

			required_opp_tables[i] = new_table;
			_get_opp_table_kref(new_table);

			 
			ret = lazy_link_required_opps(opp_table, new_table, i);
			if (ret) {
				 
				lazy = false;
				break;
			}
		}

		of_node_put(opp_np);

		 
		if (!lazy) {
			_update_set_required_opps(opp_table);
			list_del_init(&opp_table->lazy);

			list_for_each_entry(opp, &opp_table->opp_list, node)
				_required_opps_available(opp, opp_table->required_opp_count);
		}
	}

	mutex_unlock(&opp_table_lock);
}

static int _bandwidth_supported(struct device *dev, struct opp_table *opp_table)
{
	struct device_node *np, *opp_np;
	struct property *prop;

	if (!opp_table) {
		np = of_node_get(dev->of_node);
		if (!np)
			return -ENODEV;

		opp_np = _opp_of_get_opp_desc_node(np, 0);
		of_node_put(np);
	} else {
		opp_np = of_node_get(opp_table->np);
	}

	 
	if (!opp_np)
		return 0;

	 
	np = of_get_next_available_child(opp_np, NULL);
	of_node_put(opp_np);
	if (!np) {
		dev_err(dev, "OPP table empty\n");
		return -EINVAL;
	}

	prop = of_find_property(np, "opp-peak-kBps", NULL);
	of_node_put(np);

	if (!prop || !prop->length)
		return 0;

	return 1;
}

int dev_pm_opp_of_find_icc_paths(struct device *dev,
				 struct opp_table *opp_table)
{
	struct device_node *np;
	int ret, i, count, num_paths;
	struct icc_path **paths;

	ret = _bandwidth_supported(dev, opp_table);
	if (ret == -EINVAL)
		return 0;  
	else if (ret <= 0)
		return ret;

	ret = 0;

	np = of_node_get(dev->of_node);
	if (!np)
		return 0;

	count = of_count_phandle_with_args(np, "interconnects",
					   "#interconnect-cells");
	of_node_put(np);
	if (count < 0)
		return 0;

	 
	if (count % 2) {
		dev_err(dev, "%s: Invalid interconnects values\n", __func__);
		return -EINVAL;
	}

	num_paths = count / 2;
	paths = kcalloc(num_paths, sizeof(*paths), GFP_KERNEL);
	if (!paths)
		return -ENOMEM;

	for (i = 0; i < num_paths; i++) {
		paths[i] = of_icc_get_by_index(dev, i);
		if (IS_ERR(paths[i])) {
			ret = dev_err_probe(dev, PTR_ERR(paths[i]), "%s: Unable to get path%d\n", __func__, i);
			goto err;
		}
	}

	if (opp_table) {
		opp_table->paths = paths;
		opp_table->path_count = num_paths;
		return 0;
	}

err:
	while (i--)
		icc_put(paths[i]);

	kfree(paths);

	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_opp_of_find_icc_paths);

static bool _opp_is_supported(struct device *dev, struct opp_table *opp_table,
			      struct device_node *np)
{
	unsigned int levels = opp_table->supported_hw_count;
	int count, versions, ret, i, j;
	u32 val;

	if (!opp_table->supported_hw) {
		 
		if (of_property_present(np, "opp-supported-hw"))
			return false;
		else
			return true;
	}

	count = of_property_count_u32_elems(np, "opp-supported-hw");
	if (count <= 0 || count % levels) {
		dev_err(dev, "%s: Invalid opp-supported-hw property (%d)\n",
			__func__, count);
		return false;
	}

	versions = count / levels;

	 
	for (i = 0; i < versions; i++) {
		bool supported = true;

		for (j = 0; j < levels; j++) {
			ret = of_property_read_u32_index(np, "opp-supported-hw",
							 i * levels + j, &val);
			if (ret) {
				dev_warn(dev, "%s: failed to read opp-supported-hw property at index %d: %d\n",
					 __func__, i * levels + j, ret);
				return false;
			}

			 
			if (!(val & opp_table->supported_hw[j])) {
				supported = false;
				break;
			}
		}

		if (supported)
			return true;
	}

	return false;
}

static u32 *_parse_named_prop(struct dev_pm_opp *opp, struct device *dev,
			      struct opp_table *opp_table,
			      const char *prop_type, bool *triplet)
{
	struct property *prop = NULL;
	char name[NAME_MAX];
	int count, ret;
	u32 *out;

	 
	if (opp_table->prop_name) {
		snprintf(name, sizeof(name), "opp-%s-%s", prop_type,
			 opp_table->prop_name);
		prop = of_find_property(opp->np, name, NULL);
	}

	if (!prop) {
		 
		snprintf(name, sizeof(name), "opp-%s", prop_type);
		prop = of_find_property(opp->np, name, NULL);
		if (!prop)
			return NULL;
	}

	count = of_property_count_u32_elems(opp->np, name);
	if (count < 0) {
		dev_err(dev, "%s: Invalid %s property (%d)\n", __func__, name,
			count);
		return ERR_PTR(count);
	}

	 
	if (unlikely(opp_table->regulator_count == -1))
		opp_table->regulator_count = 1;

	if (count != opp_table->regulator_count &&
	    (!triplet || count != opp_table->regulator_count * 3)) {
		dev_err(dev, "%s: Invalid number of elements in %s property (%u) with supplies (%d)\n",
			__func__, prop_type, count, opp_table->regulator_count);
		return ERR_PTR(-EINVAL);
	}

	out = kmalloc_array(count, sizeof(*out), GFP_KERNEL);
	if (!out)
		return ERR_PTR(-EINVAL);

	ret = of_property_read_u32_array(opp->np, name, out, count);
	if (ret) {
		dev_err(dev, "%s: error parsing %s: %d\n", __func__, name, ret);
		kfree(out);
		return ERR_PTR(-EINVAL);
	}

	if (triplet)
		*triplet = count != opp_table->regulator_count;

	return out;
}

static u32 *opp_parse_microvolt(struct dev_pm_opp *opp, struct device *dev,
				struct opp_table *opp_table, bool *triplet)
{
	u32 *microvolt;

	microvolt = _parse_named_prop(opp, dev, opp_table, "microvolt", triplet);
	if (IS_ERR(microvolt))
		return microvolt;

	if (!microvolt) {
		 
		if (list_empty(&opp_table->opp_list) &&
		    opp_table->regulator_count > 0) {
			dev_err(dev, "%s: opp-microvolt missing although OPP managing regulators\n",
				__func__);
			return ERR_PTR(-EINVAL);
		}
	}

	return microvolt;
}

static int opp_parse_supplies(struct dev_pm_opp *opp, struct device *dev,
			      struct opp_table *opp_table)
{
	u32 *microvolt, *microamp, *microwatt;
	int ret = 0, i, j;
	bool triplet;

	microvolt = opp_parse_microvolt(opp, dev, opp_table, &triplet);
	if (IS_ERR(microvolt))
		return PTR_ERR(microvolt);

	microamp = _parse_named_prop(opp, dev, opp_table, "microamp", NULL);
	if (IS_ERR(microamp)) {
		ret = PTR_ERR(microamp);
		goto free_microvolt;
	}

	microwatt = _parse_named_prop(opp, dev, opp_table, "microwatt", NULL);
	if (IS_ERR(microwatt)) {
		ret = PTR_ERR(microwatt);
		goto free_microamp;
	}

	 
	if (unlikely(opp_table->regulator_count == -1)) {
		opp_table->regulator_count = 0;
		return 0;
	}

	for (i = 0, j = 0; i < opp_table->regulator_count; i++) {
		if (microvolt) {
			opp->supplies[i].u_volt = microvolt[j++];

			if (triplet) {
				opp->supplies[i].u_volt_min = microvolt[j++];
				opp->supplies[i].u_volt_max = microvolt[j++];
			} else {
				opp->supplies[i].u_volt_min = opp->supplies[i].u_volt;
				opp->supplies[i].u_volt_max = opp->supplies[i].u_volt;
			}
		}

		if (microamp)
			opp->supplies[i].u_amp = microamp[i];

		if (microwatt)
			opp->supplies[i].u_watt = microwatt[i];
	}

	kfree(microwatt);
free_microamp:
	kfree(microamp);
free_microvolt:
	kfree(microvolt);

	return ret;
}

 
void dev_pm_opp_of_remove_table(struct device *dev)
{
	dev_pm_opp_remove_table(dev);
}
EXPORT_SYMBOL_GPL(dev_pm_opp_of_remove_table);

static int _read_rate(struct dev_pm_opp *new_opp, struct opp_table *opp_table,
		      struct device_node *np)
{
	struct property *prop;
	int i, count, ret;
	u64 *rates;

	prop = of_find_property(np, "opp-hz", NULL);
	if (!prop)
		return -ENODEV;

	count = prop->length / sizeof(u64);
	if (opp_table->clk_count != count) {
		pr_err("%s: Count mismatch between opp-hz and clk_count (%d %d)\n",
		       __func__, count, opp_table->clk_count);
		return -EINVAL;
	}

	rates = kmalloc_array(count, sizeof(*rates), GFP_KERNEL);
	if (!rates)
		return -ENOMEM;

	ret = of_property_read_u64_array(np, "opp-hz", rates, count);
	if (ret) {
		pr_err("%s: Error parsing opp-hz: %d\n", __func__, ret);
	} else {
		 
		for (i = 0; i < count; i++) {
			new_opp->rates[i] = (unsigned long)rates[i];

			 
			WARN_ON(new_opp->rates[i] != rates[i]);
		}
	}

	kfree(rates);

	return ret;
}

static int _read_bw(struct dev_pm_opp *new_opp, struct opp_table *opp_table,
		    struct device_node *np, bool peak)
{
	const char *name = peak ? "opp-peak-kBps" : "opp-avg-kBps";
	struct property *prop;
	int i, count, ret;
	u32 *bw;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -ENODEV;

	count = prop->length / sizeof(u32);
	if (opp_table->path_count != count) {
		pr_err("%s: Mismatch between %s and paths (%d %d)\n",
				__func__, name, count, opp_table->path_count);
		return -EINVAL;
	}

	bw = kmalloc_array(count, sizeof(*bw), GFP_KERNEL);
	if (!bw)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, name, bw, count);
	if (ret) {
		pr_err("%s: Error parsing %s: %d\n", __func__, name, ret);
		goto out;
	}

	for (i = 0; i < count; i++) {
		if (peak)
			new_opp->bandwidth[i].peak = kBps_to_icc(bw[i]);
		else
			new_opp->bandwidth[i].avg = kBps_to_icc(bw[i]);
	}

out:
	kfree(bw);
	return ret;
}

static int _read_opp_key(struct dev_pm_opp *new_opp,
			 struct opp_table *opp_table, struct device_node *np)
{
	bool found = false;
	int ret;

	ret = _read_rate(new_opp, opp_table, np);
	if (!ret)
		found = true;
	else if (ret != -ENODEV)
		return ret;

	 
	ret = _read_bw(new_opp, opp_table, np, true);
	if (!ret) {
		found = true;
		ret = _read_bw(new_opp, opp_table, np, false);
	}

	 
	if (ret && ret != -ENODEV)
		return ret;

	if (!of_property_read_u32(np, "opp-level", &new_opp->level))
		found = true;

	if (found)
		return 0;

	return ret;
}

 
static struct dev_pm_opp *_opp_add_static_v2(struct opp_table *opp_table,
		struct device *dev, struct device_node *np)
{
	struct dev_pm_opp *new_opp;
	u32 val;
	int ret;

	new_opp = _opp_allocate(opp_table);
	if (!new_opp)
		return ERR_PTR(-ENOMEM);

	ret = _read_opp_key(new_opp, opp_table, np);
	if (ret < 0) {
		dev_err(dev, "%s: opp key field not found\n", __func__);
		goto free_opp;
	}

	 
	if (!_opp_is_supported(dev, opp_table, np)) {
		dev_dbg(dev, "OPP not supported by hardware: %s\n",
			of_node_full_name(np));
		goto free_opp;
	}

	new_opp->turbo = of_property_read_bool(np, "turbo-mode");

	new_opp->np = of_node_get(np);
	new_opp->dynamic = false;
	new_opp->available = true;

	ret = _of_opp_alloc_required_opps(opp_table, new_opp);
	if (ret)
		goto free_opp;

	if (!of_property_read_u32(np, "clock-latency-ns", &val))
		new_opp->clock_latency_ns = val;

	ret = opp_parse_supplies(new_opp, dev, opp_table);
	if (ret)
		goto free_required_opps;

	ret = _opp_add(dev, new_opp, opp_table);
	if (ret) {
		 
		if (ret == -EBUSY)
			ret = 0;
		goto free_required_opps;
	}

	 
	if (of_property_read_bool(np, "opp-suspend")) {
		if (opp_table->suspend_opp) {
			 
			if (_opp_compare_key(opp_table, new_opp, opp_table->suspend_opp) == 1) {
				opp_table->suspend_opp->suspend = false;
				new_opp->suspend = true;
				opp_table->suspend_opp = new_opp;
			}
		} else {
			new_opp->suspend = true;
			opp_table->suspend_opp = new_opp;
		}
	}

	if (new_opp->clock_latency_ns > opp_table->clock_latency_ns_max)
		opp_table->clock_latency_ns_max = new_opp->clock_latency_ns;

	pr_debug("%s: turbo:%d rate:%lu uv:%lu uvmin:%lu uvmax:%lu latency:%lu level:%u\n",
		 __func__, new_opp->turbo, new_opp->rates[0],
		 new_opp->supplies[0].u_volt, new_opp->supplies[0].u_volt_min,
		 new_opp->supplies[0].u_volt_max, new_opp->clock_latency_ns,
		 new_opp->level);

	 
	blocking_notifier_call_chain(&opp_table->head, OPP_EVENT_ADD, new_opp);
	return new_opp;

free_required_opps:
	_of_opp_free_required_opps(opp_table, new_opp);
free_opp:
	_opp_free(new_opp);

	return ret ? ERR_PTR(ret) : NULL;
}

 
static int _of_add_opp_table_v2(struct device *dev, struct opp_table *opp_table)
{
	struct device_node *np;
	int ret, count = 0;
	struct dev_pm_opp *opp;

	 
	mutex_lock(&opp_table->lock);
	if (opp_table->parsed_static_opps) {
		opp_table->parsed_static_opps++;
		mutex_unlock(&opp_table->lock);
		return 0;
	}

	opp_table->parsed_static_opps = 1;
	mutex_unlock(&opp_table->lock);

	 
	for_each_available_child_of_node(opp_table->np, np) {
		opp = _opp_add_static_v2(opp_table, dev, np);
		if (IS_ERR(opp)) {
			ret = PTR_ERR(opp);
			dev_err(dev, "%s: Failed to add OPP, %d\n", __func__,
				ret);
			of_node_put(np);
			goto remove_static_opp;
		} else if (opp) {
			count++;
		}
	}

	 
	if (!count) {
		dev_err(dev, "%s: no supported OPPs", __func__);
		ret = -ENOENT;
		goto remove_static_opp;
	}

	lazy_link_required_opp_table(opp_table);

	return 0;

remove_static_opp:
	_opp_remove_all_static(opp_table);

	return ret;
}

 
static int _of_add_opp_table_v1(struct device *dev, struct opp_table *opp_table)
{
	const struct property *prop;
	const __be32 *val;
	int nr, ret = 0;

	mutex_lock(&opp_table->lock);
	if (opp_table->parsed_static_opps) {
		opp_table->parsed_static_opps++;
		mutex_unlock(&opp_table->lock);
		return 0;
	}

	opp_table->parsed_static_opps = 1;
	mutex_unlock(&opp_table->lock);

	prop = of_find_property(dev->of_node, "operating-points", NULL);
	if (!prop) {
		ret = -ENODEV;
		goto remove_static_opp;
	}
	if (!prop->value) {
		ret = -ENODATA;
		goto remove_static_opp;
	}

	 
	nr = prop->length / sizeof(u32);
	if (nr % 2) {
		dev_err(dev, "%s: Invalid OPP table\n", __func__);
		ret = -EINVAL;
		goto remove_static_opp;
	}

	val = prop->value;
	while (nr) {
		unsigned long freq = be32_to_cpup(val++) * 1000;
		unsigned long volt = be32_to_cpup(val++);

		ret = _opp_add_v1(opp_table, dev, freq, volt, false);
		if (ret) {
			dev_err(dev, "%s: Failed to add OPP %ld (%d)\n",
				__func__, freq, ret);
			goto remove_static_opp;
		}
		nr -= 2;
	}

	return 0;

remove_static_opp:
	_opp_remove_all_static(opp_table);

	return ret;
}

static int _of_add_table_indexed(struct device *dev, int index)
{
	struct opp_table *opp_table;
	int ret, count;

	if (index) {
		 
		count = of_count_phandle_with_args(dev->of_node,
						   "operating-points-v2", NULL);
		if (count == 1)
			index = 0;
	}

	opp_table = _add_opp_table_indexed(dev, index, true);
	if (IS_ERR(opp_table))
		return PTR_ERR(opp_table);

	 
	if (opp_table->np)
		ret = _of_add_opp_table_v2(dev, opp_table);
	else
		ret = _of_add_opp_table_v1(dev, opp_table);

	if (ret)
		dev_pm_opp_put_opp_table(opp_table);

	return ret;
}

static void devm_pm_opp_of_table_release(void *data)
{
	dev_pm_opp_of_remove_table(data);
}

static int _devm_of_add_table_indexed(struct device *dev, int index)
{
	int ret;

	ret = _of_add_table_indexed(dev, index);
	if (ret)
		return ret;

	return devm_add_action_or_reset(dev, devm_pm_opp_of_table_release, dev);
}

 
int devm_pm_opp_of_add_table(struct device *dev)
{
	return _devm_of_add_table_indexed(dev, 0);
}
EXPORT_SYMBOL_GPL(devm_pm_opp_of_add_table);

 
int dev_pm_opp_of_add_table(struct device *dev)
{
	return _of_add_table_indexed(dev, 0);
}
EXPORT_SYMBOL_GPL(dev_pm_opp_of_add_table);

 
int dev_pm_opp_of_add_table_indexed(struct device *dev, int index)
{
	return _of_add_table_indexed(dev, index);
}
EXPORT_SYMBOL_GPL(dev_pm_opp_of_add_table_indexed);

 
int devm_pm_opp_of_add_table_indexed(struct device *dev, int index)
{
	return _devm_of_add_table_indexed(dev, index);
}
EXPORT_SYMBOL_GPL(devm_pm_opp_of_add_table_indexed);

 

 
void dev_pm_opp_of_cpumask_remove_table(const struct cpumask *cpumask)
{
	_dev_pm_opp_cpumask_remove_table(cpumask, -1);
}
EXPORT_SYMBOL_GPL(dev_pm_opp_of_cpumask_remove_table);

 
int dev_pm_opp_of_cpumask_add_table(const struct cpumask *cpumask)
{
	struct device *cpu_dev;
	int cpu, ret;

	if (WARN_ON(cpumask_empty(cpumask)))
		return -ENODEV;

	for_each_cpu(cpu, cpumask) {
		cpu_dev = get_cpu_device(cpu);
		if (!cpu_dev) {
			pr_err("%s: failed to get cpu%d device\n", __func__,
			       cpu);
			ret = -ENODEV;
			goto remove_table;
		}

		ret = dev_pm_opp_of_add_table(cpu_dev);
		if (ret) {
			 
			pr_debug("%s: couldn't find opp table for cpu:%d, %d\n",
				 __func__, cpu, ret);

			goto remove_table;
		}
	}

	return 0;

remove_table:
	 
	_dev_pm_opp_cpumask_remove_table(cpumask, cpu);

	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_opp_of_cpumask_add_table);

 
 
int dev_pm_opp_of_get_sharing_cpus(struct device *cpu_dev,
				   struct cpumask *cpumask)
{
	struct device_node *np, *tmp_np, *cpu_np;
	int cpu, ret = 0;

	 
	np = dev_pm_opp_of_get_opp_desc_node(cpu_dev);
	if (!np) {
		dev_dbg(cpu_dev, "%s: Couldn't find opp node.\n", __func__);
		return -ENOENT;
	}

	cpumask_set_cpu(cpu_dev->id, cpumask);

	 
	if (!of_property_read_bool(np, "opp-shared"))
		goto put_cpu_node;

	for_each_possible_cpu(cpu) {
		if (cpu == cpu_dev->id)
			continue;

		cpu_np = of_cpu_device_node_get(cpu);
		if (!cpu_np) {
			dev_err(cpu_dev, "%s: failed to get cpu%d node\n",
				__func__, cpu);
			ret = -ENOENT;
			goto put_cpu_node;
		}

		 
		tmp_np = _opp_of_get_opp_desc_node(cpu_np, 0);
		of_node_put(cpu_np);
		if (!tmp_np) {
			pr_err("%pOF: Couldn't find opp node\n", cpu_np);
			ret = -ENOENT;
			goto put_cpu_node;
		}

		 
		if (np == tmp_np)
			cpumask_set_cpu(cpu, cpumask);

		of_node_put(tmp_np);
	}

put_cpu_node:
	of_node_put(np);
	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_opp_of_get_sharing_cpus);

 
int of_get_required_opp_performance_state(struct device_node *np, int index)
{
	struct dev_pm_opp *opp;
	struct device_node *required_np;
	struct opp_table *opp_table;
	int pstate = -EINVAL;

	required_np = of_parse_required_opp(np, index);
	if (!required_np)
		return -ENODEV;

	opp_table = _find_table_of_opp_np(required_np);
	if (IS_ERR(opp_table)) {
		pr_err("%s: Failed to find required OPP table %pOF: %ld\n",
		       __func__, np, PTR_ERR(opp_table));
		goto put_required_np;
	}

	 
	if (unlikely(!opp_table->is_genpd)) {
		pr_err("%s: Performance state is only valid for genpds.\n", __func__);
		goto put_required_np;
	}

	opp = _find_opp_of_np(opp_table, required_np);
	if (opp) {
		pstate = opp->level;
		dev_pm_opp_put(opp);
	}

	dev_pm_opp_put_opp_table(opp_table);

put_required_np:
	of_node_put(required_np);

	return pstate;
}
EXPORT_SYMBOL_GPL(of_get_required_opp_performance_state);

 
struct device_node *dev_pm_opp_get_of_node(struct dev_pm_opp *opp)
{
	if (IS_ERR_OR_NULL(opp)) {
		pr_err("%s: Invalid parameters\n", __func__);
		return NULL;
	}

	return of_node_get(opp->np);
}
EXPORT_SYMBOL_GPL(dev_pm_opp_get_of_node);

 
static int __maybe_unused
_get_dt_power(struct device *dev, unsigned long *uW, unsigned long *kHz)
{
	struct dev_pm_opp *opp;
	unsigned long opp_freq, opp_power;

	 
	opp_freq = *kHz * 1000;
	opp = dev_pm_opp_find_freq_ceil(dev, &opp_freq);
	if (IS_ERR(opp))
		return -EINVAL;

	opp_power = dev_pm_opp_get_power(opp);
	dev_pm_opp_put(opp);
	if (!opp_power)
		return -EINVAL;

	*kHz = opp_freq / 1000;
	*uW = opp_power;

	return 0;
}

 
static int __maybe_unused _get_power(struct device *dev, unsigned long *uW,
				     unsigned long *kHz)
{
	struct dev_pm_opp *opp;
	struct device_node *np;
	unsigned long mV, Hz;
	u32 cap;
	u64 tmp;
	int ret;

	np = of_node_get(dev->of_node);
	if (!np)
		return -EINVAL;

	ret = of_property_read_u32(np, "dynamic-power-coefficient", &cap);
	of_node_put(np);
	if (ret)
		return -EINVAL;

	Hz = *kHz * 1000;
	opp = dev_pm_opp_find_freq_ceil(dev, &Hz);
	if (IS_ERR(opp))
		return -EINVAL;

	mV = dev_pm_opp_get_voltage(opp) / 1000;
	dev_pm_opp_put(opp);
	if (!mV)
		return -EINVAL;

	tmp = (u64)cap * mV * mV * (Hz / 1000000);
	 
	do_div(tmp, 1000000);

	*uW = (unsigned long)tmp;
	*kHz = Hz / 1000;

	return 0;
}

static bool _of_has_opp_microwatt_property(struct device *dev)
{
	unsigned long power, freq = 0;
	struct dev_pm_opp *opp;

	 
	opp = dev_pm_opp_find_freq_ceil(dev, &freq);
	if (IS_ERR(opp))
		return false;

	power = dev_pm_opp_get_power(opp);
	dev_pm_opp_put(opp);
	if (!power)
		return false;

	return true;
}

 
int dev_pm_opp_of_register_em(struct device *dev, struct cpumask *cpus)
{
	struct em_data_callback em_cb;
	struct device_node *np;
	int ret, nr_opp;
	u32 cap;

	if (IS_ERR_OR_NULL(dev)) {
		ret = -EINVAL;
		goto failed;
	}

	nr_opp = dev_pm_opp_get_opp_count(dev);
	if (nr_opp <= 0) {
		ret = -EINVAL;
		goto failed;
	}

	 
	if (_of_has_opp_microwatt_property(dev)) {
		EM_SET_ACTIVE_POWER_CB(em_cb, _get_dt_power);
		goto register_em;
	}

	np = of_node_get(dev->of_node);
	if (!np) {
		ret = -EINVAL;
		goto failed;
	}

	 
	ret = of_property_read_u32(np, "dynamic-power-coefficient", &cap);
	of_node_put(np);
	if (ret || !cap) {
		dev_dbg(dev, "Couldn't find proper 'dynamic-power-coefficient' in DT\n");
		ret = -EINVAL;
		goto failed;
	}

	EM_SET_ACTIVE_POWER_CB(em_cb, _get_power);

register_em:
	ret = em_dev_register_perf_domain(dev, nr_opp, &em_cb, cpus, true);
	if (ret)
		goto failed;

	return 0;

failed:
	dev_dbg(dev, "Couldn't register Energy Model %d\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_opp_of_register_em);
