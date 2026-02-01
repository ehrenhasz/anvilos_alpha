
 

#define dev_fmt(fmt)	"tegra-soc: " fmt

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>

#include <soc/tegra/common.h>
#include <soc/tegra/fuse.h>

static const struct of_device_id tegra_machine_match[] = {
	{ .compatible = "nvidia,tegra20", },
	{ .compatible = "nvidia,tegra30", },
	{ .compatible = "nvidia,tegra114", },
	{ .compatible = "nvidia,tegra124", },
	{ .compatible = "nvidia,tegra132", },
	{ .compatible = "nvidia,tegra210", },
	{ }
};

bool soc_is_tegra(void)
{
	const struct of_device_id *match;
	struct device_node *root;

	root = of_find_node_by_path("/");
	if (!root)
		return false;

	match = of_match_node(tegra_machine_match, root);
	of_node_put(root);

	return match != NULL;
}

static int tegra_core_dev_init_opp_state(struct device *dev)
{
	unsigned long rate;
	struct clk *clk;
	bool rpm_enabled;
	int err;

	clk = devm_clk_get(dev, NULL);
	if (IS_ERR(clk)) {
		dev_err(dev, "failed to get clk: %pe\n", clk);
		return PTR_ERR(clk);
	}

	rate = clk_get_rate(clk);
	if (!rate) {
		dev_err(dev, "failed to get clk rate\n");
		return -EINVAL;
	}

	 
	rpm_enabled = pm_runtime_enabled(dev);
	if (!rpm_enabled)
		pm_runtime_enable(dev);

	 
	if (!pm_runtime_enabled(dev)) {
		dev_WARN(dev, "failed to enable runtime PM\n");
		pm_runtime_disable(dev);
		return -EINVAL;
	}

	 
	err = dev_pm_opp_set_rate(dev, rate);

	if (!rpm_enabled)
		pm_runtime_disable(dev);

	if (err) {
		dev_err(dev, "failed to initialize OPP clock: %d\n", err);
		return err;
	}

	return 0;
}

 
int devm_tegra_core_dev_init_opp_table(struct device *dev,
				       struct tegra_core_opp_params *params)
{
	u32 hw_version;
	int err;
	 
	const char *clk_names[] = { NULL, NULL };
	struct dev_pm_opp_config config = {
		 
		.clk_names = clk_names,
	};

	if (of_machine_is_compatible("nvidia,tegra20")) {
		hw_version = BIT(tegra_sku_info.soc_process_id);
		config.supported_hw = &hw_version;
		config.supported_hw_count = 1;
	} else if (of_machine_is_compatible("nvidia,tegra30")) {
		hw_version = BIT(tegra_sku_info.soc_speedo_id);
		config.supported_hw = &hw_version;
		config.supported_hw_count = 1;
	}

	err = devm_pm_opp_set_config(dev, &config);
	if (err) {
		dev_err(dev, "failed to set OPP config: %d\n", err);
		return err;
	}

	 
	if (!config.supported_hw)
		return -ENODEV;

	 
	err = devm_pm_opp_of_add_table(dev);
	if (err) {
		if (err != -ENODEV)
			dev_err(dev, "failed to add OPP table: %d\n", err);

		return err;
	}

	if (params->init_state) {
		err = tegra_core_dev_init_opp_state(dev);
		if (err)
			return err;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(devm_tegra_core_dev_init_opp_table);
