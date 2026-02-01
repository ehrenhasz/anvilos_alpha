
 
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/soc/ti/ti_sci_protocol.h>
#include <linux/bsearch.h>
#include <linux/list_sort.h>

#define SCI_CLK_SSC_ENABLE		BIT(0)
#define SCI_CLK_ALLOW_FREQ_CHANGE	BIT(1)
#define SCI_CLK_INPUT_TERMINATION	BIT(2)

 
struct sci_clk_provider {
	const struct ti_sci_handle *sci;
	const struct ti_sci_clk_ops *ops;
	struct device *dev;
	struct sci_clk **clocks;
	int num_clocks;
};

 
struct sci_clk {
	struct clk_hw hw;
	u16 dev_id;
	u32 clk_id;
	u32 num_parents;
	struct sci_clk_provider *provider;
	u8 flags;
	struct list_head node;
	unsigned long cached_req;
	unsigned long cached_res;
};

#define to_sci_clk(_hw) container_of(_hw, struct sci_clk, hw)

 
static int sci_clk_prepare(struct clk_hw *hw)
{
	struct sci_clk *clk = to_sci_clk(hw);
	bool enable_ssc = clk->flags & SCI_CLK_SSC_ENABLE;
	bool allow_freq_change = clk->flags & SCI_CLK_ALLOW_FREQ_CHANGE;
	bool input_termination = clk->flags & SCI_CLK_INPUT_TERMINATION;

	return clk->provider->ops->get_clock(clk->provider->sci, clk->dev_id,
					     clk->clk_id, enable_ssc,
					     allow_freq_change,
					     input_termination);
}

 
static void sci_clk_unprepare(struct clk_hw *hw)
{
	struct sci_clk *clk = to_sci_clk(hw);
	int ret;

	ret = clk->provider->ops->put_clock(clk->provider->sci, clk->dev_id,
					    clk->clk_id);
	if (ret)
		dev_err(clk->provider->dev,
			"unprepare failed for dev=%d, clk=%d, ret=%d\n",
			clk->dev_id, clk->clk_id, ret);
}

 
static int sci_clk_is_prepared(struct clk_hw *hw)
{
	struct sci_clk *clk = to_sci_clk(hw);
	bool req_state, current_state;
	int ret;

	ret = clk->provider->ops->is_on(clk->provider->sci, clk->dev_id,
					clk->clk_id, &req_state,
					&current_state);
	if (ret) {
		dev_err(clk->provider->dev,
			"is_prepared failed for dev=%d, clk=%d, ret=%d\n",
			clk->dev_id, clk->clk_id, ret);
		return 0;
	}

	return req_state;
}

 
static unsigned long sci_clk_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	struct sci_clk *clk = to_sci_clk(hw);
	u64 freq;
	int ret;

	ret = clk->provider->ops->get_freq(clk->provider->sci, clk->dev_id,
					   clk->clk_id, &freq);
	if (ret) {
		dev_err(clk->provider->dev,
			"recalc-rate failed for dev=%d, clk=%d, ret=%d\n",
			clk->dev_id, clk->clk_id, ret);
		return 0;
	}

	return freq;
}

 
static int sci_clk_determine_rate(struct clk_hw *hw,
				  struct clk_rate_request *req)
{
	struct sci_clk *clk = to_sci_clk(hw);
	int ret;
	u64 new_rate;

	if (clk->cached_req && clk->cached_req == req->rate) {
		req->rate = clk->cached_res;
		return 0;
	}

	ret = clk->provider->ops->get_best_match_freq(clk->provider->sci,
						      clk->dev_id,
						      clk->clk_id,
						      req->min_rate,
						      req->rate,
						      req->max_rate,
						      &new_rate);
	if (ret) {
		dev_err(clk->provider->dev,
			"determine-rate failed for dev=%d, clk=%d, ret=%d\n",
			clk->dev_id, clk->clk_id, ret);
		return ret;
	}

	clk->cached_req = req->rate;
	clk->cached_res = new_rate;

	req->rate = new_rate;

	return 0;
}

 
static int sci_clk_set_rate(struct clk_hw *hw, unsigned long rate,
			    unsigned long parent_rate)
{
	struct sci_clk *clk = to_sci_clk(hw);

	return clk->provider->ops->set_freq(clk->provider->sci, clk->dev_id,
					    clk->clk_id, rate / 10 * 9, rate,
					    rate / 10 * 11);
}

 
static u8 sci_clk_get_parent(struct clk_hw *hw)
{
	struct sci_clk *clk = to_sci_clk(hw);
	u32 parent_id = 0;
	int ret;

	ret = clk->provider->ops->get_parent(clk->provider->sci, clk->dev_id,
					     clk->clk_id, (void *)&parent_id);
	if (ret) {
		dev_err(clk->provider->dev,
			"get-parent failed for dev=%d, clk=%d, ret=%d\n",
			clk->dev_id, clk->clk_id, ret);
		return 0;
	}

	parent_id = parent_id - clk->clk_id - 1;

	return (u8)parent_id;
}

 
static int sci_clk_set_parent(struct clk_hw *hw, u8 index)
{
	struct sci_clk *clk = to_sci_clk(hw);

	clk->cached_req = 0;

	return clk->provider->ops->set_parent(clk->provider->sci, clk->dev_id,
					      clk->clk_id,
					      index + 1 + clk->clk_id);
}

static const struct clk_ops sci_clk_ops = {
	.prepare = sci_clk_prepare,
	.unprepare = sci_clk_unprepare,
	.is_prepared = sci_clk_is_prepared,
	.recalc_rate = sci_clk_recalc_rate,
	.determine_rate = sci_clk_determine_rate,
	.set_rate = sci_clk_set_rate,
	.get_parent = sci_clk_get_parent,
	.set_parent = sci_clk_set_parent,
};

 
static int _sci_clk_build(struct sci_clk_provider *provider,
			  struct sci_clk *sci_clk)
{
	struct clk_init_data init = { NULL };
	char *name = NULL;
	char **parent_names = NULL;
	int i;
	int ret = 0;

	name = kasprintf(GFP_KERNEL, "clk:%d:%d", sci_clk->dev_id,
			 sci_clk->clk_id);
	if (!name)
		return -ENOMEM;

	init.name = name;

	 
	if (sci_clk->num_parents < 2)
		sci_clk->num_parents = 0;

	if (sci_clk->num_parents) {
		parent_names = kcalloc(sci_clk->num_parents, sizeof(char *),
				       GFP_KERNEL);

		if (!parent_names) {
			ret = -ENOMEM;
			goto err;
		}

		for (i = 0; i < sci_clk->num_parents; i++) {
			char *parent_name;

			parent_name = kasprintf(GFP_KERNEL, "clk:%d:%d",
						sci_clk->dev_id,
						sci_clk->clk_id + 1 + i);
			if (!parent_name) {
				ret = -ENOMEM;
				goto err;
			}
			parent_names[i] = parent_name;
		}
		init.parent_names = (void *)parent_names;
	}

	init.ops = &sci_clk_ops;
	init.num_parents = sci_clk->num_parents;
	sci_clk->hw.init = &init;

	ret = devm_clk_hw_register(provider->dev, &sci_clk->hw);
	if (ret)
		dev_err(provider->dev, "failed clk register with %d\n", ret);

err:
	if (parent_names) {
		for (i = 0; i < sci_clk->num_parents; i++)
			kfree(parent_names[i]);

		kfree(parent_names);
	}

	kfree(name);

	return ret;
}

static int _cmp_sci_clk(const void *a, const void *b)
{
	const struct sci_clk *ca = a;
	const struct sci_clk *cb = *(struct sci_clk **)b;

	if (ca->dev_id == cb->dev_id && ca->clk_id == cb->clk_id)
		return 0;
	if (ca->dev_id > cb->dev_id ||
	    (ca->dev_id == cb->dev_id && ca->clk_id > cb->clk_id))
		return 1;
	return -1;
}

 
static struct clk_hw *sci_clk_get(struct of_phandle_args *clkspec, void *data)
{
	struct sci_clk_provider *provider = data;
	struct sci_clk **clk;
	struct sci_clk key;

	if (clkspec->args_count != 2)
		return ERR_PTR(-EINVAL);

	key.dev_id = clkspec->args[0];
	key.clk_id = clkspec->args[1];

	clk = bsearch(&key, provider->clocks, provider->num_clocks,
		      sizeof(clk), _cmp_sci_clk);

	if (!clk)
		return ERR_PTR(-ENODEV);

	return &(*clk)->hw;
}

static int ti_sci_init_clocks(struct sci_clk_provider *p)
{
	int i;
	int ret;

	for (i = 0; i < p->num_clocks; i++) {
		ret = _sci_clk_build(p, p->clocks[i]);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct of_device_id ti_sci_clk_of_match[] = {
	{ .compatible = "ti,k2g-sci-clk" },
	{   },
};
MODULE_DEVICE_TABLE(of, ti_sci_clk_of_match);

#ifdef CONFIG_TI_SCI_CLK_PROBE_FROM_FW
static int ti_sci_scan_clocks_from_fw(struct sci_clk_provider *provider)
{
	int ret;
	int num_clks = 0;
	struct sci_clk **clks = NULL;
	struct sci_clk **tmp_clks;
	struct sci_clk *sci_clk;
	int max_clks = 0;
	int clk_id = 0;
	int dev_id = 0;
	u32 num_parents = 0;
	int gap_size = 0;
	struct device *dev = provider->dev;

	while (1) {
		ret = provider->ops->get_num_parents(provider->sci, dev_id,
						     clk_id,
						     (void *)&num_parents);
		if (ret) {
			gap_size++;
			if (!clk_id) {
				if (gap_size >= 5)
					break;
				dev_id++;
			} else {
				if (gap_size >= 2) {
					dev_id++;
					clk_id = 0;
					gap_size = 0;
				} else {
					clk_id++;
				}
			}
			continue;
		}

		gap_size = 0;

		if (num_clks == max_clks) {
			tmp_clks = devm_kmalloc_array(dev, max_clks + 64,
						      sizeof(sci_clk),
						      GFP_KERNEL);
			memcpy(tmp_clks, clks, max_clks * sizeof(sci_clk));
			if (max_clks)
				devm_kfree(dev, clks);
			max_clks += 64;
			clks = tmp_clks;
		}

		sci_clk = devm_kzalloc(dev, sizeof(*sci_clk), GFP_KERNEL);
		if (!sci_clk)
			return -ENOMEM;
		sci_clk->dev_id = dev_id;
		sci_clk->clk_id = clk_id;
		sci_clk->provider = provider;
		sci_clk->num_parents = num_parents;

		clks[num_clks] = sci_clk;

		clk_id++;
		num_clks++;
	}

	provider->clocks = devm_kmalloc_array(dev, num_clks, sizeof(sci_clk),
					      GFP_KERNEL);
	if (!provider->clocks)
		return -ENOMEM;

	memcpy(provider->clocks, clks, num_clks * sizeof(sci_clk));

	provider->num_clocks = num_clks;

	devm_kfree(dev, clks);

	return 0;
}

#else

static int _cmp_sci_clk_list(void *priv, const struct list_head *a,
			     const struct list_head *b)
{
	struct sci_clk *ca = container_of(a, struct sci_clk, node);
	struct sci_clk *cb = container_of(b, struct sci_clk, node);

	return _cmp_sci_clk(ca, &cb);
}

static int ti_sci_scan_clocks_from_dt(struct sci_clk_provider *provider)
{
	struct device *dev = provider->dev;
	struct device_node *np = NULL;
	int ret;
	int index;
	struct of_phandle_args args;
	struct list_head clks;
	struct sci_clk *sci_clk, *prev;
	int num_clks = 0;
	int num_parents;
	int clk_id;
	const char * const clk_names[] = {
		"clocks", "assigned-clocks", "assigned-clock-parents", NULL
	};
	const char * const *clk_name;

	INIT_LIST_HEAD(&clks);

	clk_name = clk_names;

	while (*clk_name) {
		np = of_find_node_with_property(np, *clk_name);
		if (!np) {
			clk_name++;
			continue;
		}

		if (!of_device_is_available(np))
			continue;

		index = 0;

		do {
			ret = of_parse_phandle_with_args(np, *clk_name,
							 "#clock-cells", index,
							 &args);
			if (ret)
				break;

			if (args.args_count == 2 && args.np == dev->of_node) {
				sci_clk = devm_kzalloc(dev, sizeof(*sci_clk),
						       GFP_KERNEL);
				if (!sci_clk)
					return -ENOMEM;

				sci_clk->dev_id = args.args[0];
				sci_clk->clk_id = args.args[1];
				sci_clk->provider = provider;
				provider->ops->get_num_parents(provider->sci,
							       sci_clk->dev_id,
							       sci_clk->clk_id,
							       (void *)&sci_clk->num_parents);
				list_add_tail(&sci_clk->node, &clks);

				num_clks++;

				num_parents = sci_clk->num_parents;
				if (num_parents == 1)
					num_parents = 0;

				 
				if (num_parents >= 255) {
					dev_warn(dev, "too many parents for dev=%d, clk=%d (%d), cropping to 255.\n",
						 sci_clk->dev_id,
						 sci_clk->clk_id, num_parents);
					num_parents = 255;
				}

				clk_id = args.args[1] + 1;

				while (num_parents--) {
					sci_clk = devm_kzalloc(dev,
							       sizeof(*sci_clk),
							       GFP_KERNEL);
					if (!sci_clk)
						return -ENOMEM;
					sci_clk->dev_id = args.args[0];
					sci_clk->clk_id = clk_id++;
					sci_clk->provider = provider;
					list_add_tail(&sci_clk->node, &clks);

					num_clks++;
				}
			}

			index++;
		} while (args.np);
	}

	list_sort(NULL, &clks, _cmp_sci_clk_list);

	provider->clocks = devm_kmalloc_array(dev, num_clks, sizeof(sci_clk),
					      GFP_KERNEL);
	if (!provider->clocks)
		return -ENOMEM;

	num_clks = 0;
	prev = NULL;

	list_for_each_entry(sci_clk, &clks, node) {
		if (prev && prev->dev_id == sci_clk->dev_id &&
		    prev->clk_id == sci_clk->clk_id)
			continue;

		provider->clocks[num_clks++] = sci_clk;
		prev = sci_clk;
	}

	provider->num_clocks = num_clks;

	return 0;
}
#endif

 
static int ti_sci_clk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct sci_clk_provider *provider;
	const struct ti_sci_handle *handle;
	int ret;

	handle = devm_ti_sci_get_handle(dev);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	provider = devm_kzalloc(dev, sizeof(*provider), GFP_KERNEL);
	if (!provider)
		return -ENOMEM;

	provider->sci = handle;
	provider->ops = &handle->ops.clk_ops;
	provider->dev = dev;

#ifdef CONFIG_TI_SCI_CLK_PROBE_FROM_FW
	ret = ti_sci_scan_clocks_from_fw(provider);
	if (ret) {
		dev_err(dev, "scan clocks from FW failed: %d\n", ret);
		return ret;
	}
#else
	ret = ti_sci_scan_clocks_from_dt(provider);
	if (ret) {
		dev_err(dev, "scan clocks from DT failed: %d\n", ret);
		return ret;
	}
#endif

	ret = ti_sci_init_clocks(provider);
	if (ret) {
		pr_err("ti-sci-init-clocks failed.\n");
		return ret;
	}

	return of_clk_add_hw_provider(np, sci_clk_get, provider);
}

 
static void ti_sci_clk_remove(struct platform_device *pdev)
{
	of_clk_del_provider(pdev->dev.of_node);
}

static struct platform_driver ti_sci_clk_driver = {
	.probe = ti_sci_clk_probe,
	.remove_new = ti_sci_clk_remove,
	.driver = {
		.name = "ti-sci-clk",
		.of_match_table = of_match_ptr(ti_sci_clk_of_match),
	},
};
module_platform_driver(ti_sci_clk_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TI System Control Interface(SCI) Clock driver");
MODULE_AUTHOR("Tero Kristo");
MODULE_ALIAS("platform:ti-sci-clk");
