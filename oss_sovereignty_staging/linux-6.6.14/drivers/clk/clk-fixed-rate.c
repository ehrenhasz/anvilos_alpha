
 

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/platform_device.h>

 

#define to_clk_fixed_rate(_hw) container_of(_hw, struct clk_fixed_rate, hw)

static unsigned long clk_fixed_rate_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	return to_clk_fixed_rate(hw)->fixed_rate;
}

static unsigned long clk_fixed_rate_recalc_accuracy(struct clk_hw *hw,
		unsigned long parent_accuracy)
{
	struct clk_fixed_rate *fixed = to_clk_fixed_rate(hw);

	if (fixed->flags & CLK_FIXED_RATE_PARENT_ACCURACY)
		return parent_accuracy;

	return fixed->fixed_accuracy;
}

const struct clk_ops clk_fixed_rate_ops = {
	.recalc_rate = clk_fixed_rate_recalc_rate,
	.recalc_accuracy = clk_fixed_rate_recalc_accuracy,
};
EXPORT_SYMBOL_GPL(clk_fixed_rate_ops);

static void devm_clk_hw_register_fixed_rate_release(struct device *dev, void *res)
{
	struct clk_fixed_rate *fix = res;

	 
	clk_hw_unregister(&fix->hw);
}

struct clk_hw *__clk_hw_register_fixed_rate(struct device *dev,
		struct device_node *np, const char *name,
		const char *parent_name, const struct clk_hw *parent_hw,
		const struct clk_parent_data *parent_data, unsigned long flags,
		unsigned long fixed_rate, unsigned long fixed_accuracy,
		unsigned long clk_fixed_flags, bool devm)
{
	struct clk_fixed_rate *fixed;
	struct clk_hw *hw;
	struct clk_init_data init = {};
	int ret = -EINVAL;

	 
	if (devm)
		fixed = devres_alloc(devm_clk_hw_register_fixed_rate_release,
				     sizeof(*fixed), GFP_KERNEL);
	else
		fixed = kzalloc(sizeof(*fixed), GFP_KERNEL);
	if (!fixed)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &clk_fixed_rate_ops;
	init.flags = flags;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.parent_hws = parent_hw ? &parent_hw : NULL;
	init.parent_data = parent_data;
	if (parent_name || parent_hw || parent_data)
		init.num_parents = 1;
	else
		init.num_parents = 0;

	 
	fixed->flags = clk_fixed_flags;
	fixed->fixed_rate = fixed_rate;
	fixed->fixed_accuracy = fixed_accuracy;
	fixed->hw.init = &init;

	 
	hw = &fixed->hw;
	if (dev || !np)
		ret = clk_hw_register(dev, hw);
	else
		ret = of_clk_hw_register(np, hw);
	if (ret) {
		if (devm)
			devres_free(fixed);
		else
			kfree(fixed);
		hw = ERR_PTR(ret);
	} else if (devm)
		devres_add(dev, fixed);

	return hw;
}
EXPORT_SYMBOL_GPL(__clk_hw_register_fixed_rate);

struct clk *clk_register_fixed_rate(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		unsigned long fixed_rate)
{
	struct clk_hw *hw;

	hw = clk_hw_register_fixed_rate_with_accuracy(dev, name, parent_name,
						      flags, fixed_rate, 0);
	if (IS_ERR(hw))
		return ERR_CAST(hw);
	return hw->clk;
}
EXPORT_SYMBOL_GPL(clk_register_fixed_rate);

void clk_unregister_fixed_rate(struct clk *clk)
{
	struct clk_hw *hw;

	hw = __clk_get_hw(clk);
	if (!hw)
		return;

	clk_unregister(clk);
	kfree(to_clk_fixed_rate(hw));
}
EXPORT_SYMBOL_GPL(clk_unregister_fixed_rate);

void clk_hw_unregister_fixed_rate(struct clk_hw *hw)
{
	struct clk_fixed_rate *fixed;

	fixed = to_clk_fixed_rate(hw);

	clk_hw_unregister(hw);
	kfree(fixed);
}
EXPORT_SYMBOL_GPL(clk_hw_unregister_fixed_rate);

#ifdef CONFIG_OF
static struct clk_hw *_of_fixed_clk_setup(struct device_node *node)
{
	struct clk_hw *hw;
	const char *clk_name = node->name;
	u32 rate;
	u32 accuracy = 0;
	int ret;

	if (of_property_read_u32(node, "clock-frequency", &rate))
		return ERR_PTR(-EIO);

	of_property_read_u32(node, "clock-accuracy", &accuracy);

	of_property_read_string(node, "clock-output-names", &clk_name);

	hw = clk_hw_register_fixed_rate_with_accuracy(NULL, clk_name, NULL,
						    0, rate, accuracy);
	if (IS_ERR(hw))
		return hw;

	ret = of_clk_add_hw_provider(node, of_clk_hw_simple_get, hw);
	if (ret) {
		clk_hw_unregister_fixed_rate(hw);
		return ERR_PTR(ret);
	}

	return hw;
}

 
void __init of_fixed_clk_setup(struct device_node *node)
{
	_of_fixed_clk_setup(node);
}
CLK_OF_DECLARE(fixed_clk, "fixed-clock", of_fixed_clk_setup);

static void of_fixed_clk_remove(struct platform_device *pdev)
{
	struct clk_hw *hw = platform_get_drvdata(pdev);

	of_clk_del_provider(pdev->dev.of_node);
	clk_hw_unregister_fixed_rate(hw);
}

static int of_fixed_clk_probe(struct platform_device *pdev)
{
	struct clk_hw *hw;

	 
	hw = _of_fixed_clk_setup(pdev->dev.of_node);
	if (IS_ERR(hw))
		return PTR_ERR(hw);

	platform_set_drvdata(pdev, hw);

	return 0;
}

static const struct of_device_id of_fixed_clk_ids[] = {
	{ .compatible = "fixed-clock" },
	{ }
};

static struct platform_driver of_fixed_clk_driver = {
	.driver = {
		.name = "of_fixed_clk",
		.of_match_table = of_fixed_clk_ids,
	},
	.probe = of_fixed_clk_probe,
	.remove_new = of_fixed_clk_remove,
};
builtin_platform_driver(of_fixed_clk_driver);
#endif
