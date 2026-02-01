
 
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>

#include "clk-exynos-arm64.h"

 
#define GATE_MANUAL		BIT(20)
#define GATE_ENABLE_HWACG	BIT(28)

 
#define GATE_OFF_START		0x2000
#define GATE_OFF_END		0x2fff

struct exynos_arm64_cmu_data {
	struct samsung_clk_reg_dump *clk_save;
	unsigned int nr_clk_save;
	const struct samsung_clk_reg_dump *clk_suspend;
	unsigned int nr_clk_suspend;

	struct clk *clk;
	struct clk **pclks;
	int nr_pclks;

	struct samsung_clk_provider *ctx;
};

 
static void __init exynos_arm64_init_clocks(struct device_node *np,
		const unsigned long *reg_offs, size_t reg_offs_len)
{
	void __iomem *reg_base;
	size_t i;

	reg_base = of_iomap(np, 0);
	if (!reg_base)
		panic("%s: failed to map registers\n", __func__);

	for (i = 0; i < reg_offs_len; ++i) {
		void __iomem *reg = reg_base + reg_offs[i];
		u32 val;

		 
		if (reg_offs[i] < GATE_OFF_START || reg_offs[i] > GATE_OFF_END)
			continue;

		val = readl(reg);
		val |= GATE_MANUAL;
		val &= ~GATE_ENABLE_HWACG;
		writel(val, reg);
	}

	iounmap(reg_base);
}

 
static int __init exynos_arm64_enable_bus_clk(struct device *dev,
		struct device_node *np, const struct samsung_cmu_info *cmu)
{
	struct clk *parent_clk;

	if (!cmu->clk_name)
		return 0;

	if (dev) {
		struct exynos_arm64_cmu_data *data;

		parent_clk = clk_get(dev, cmu->clk_name);
		data = dev_get_drvdata(dev);
		if (data)
			data->clk = parent_clk;
	} else {
		parent_clk = of_clk_get_by_name(np, cmu->clk_name);
	}

	if (IS_ERR(parent_clk))
		return PTR_ERR(parent_clk);

	return clk_prepare_enable(parent_clk);
}

static int __init exynos_arm64_cmu_prepare_pm(struct device *dev,
		const struct samsung_cmu_info *cmu)
{
	struct exynos_arm64_cmu_data *data = dev_get_drvdata(dev);
	int i;

	data->clk_save = samsung_clk_alloc_reg_dump(cmu->clk_regs,
						    cmu->nr_clk_regs);
	if (!data->clk_save)
		return -ENOMEM;

	data->nr_clk_save = cmu->nr_clk_regs;
	data->clk_suspend = cmu->suspend_regs;
	data->nr_clk_suspend = cmu->nr_suspend_regs;
	data->nr_pclks = of_clk_get_parent_count(dev->of_node);
	if (!data->nr_pclks)
		return 0;

	data->pclks = devm_kcalloc(dev, sizeof(struct clk *), data->nr_pclks,
				   GFP_KERNEL);
	if (!data->pclks) {
		kfree(data->clk_save);
		return -ENOMEM;
	}

	for (i = 0; i < data->nr_pclks; i++) {
		struct clk *clk = of_clk_get(dev->of_node, i);

		if (IS_ERR(clk)) {
			kfree(data->clk_save);
			while (--i >= 0)
				clk_put(data->pclks[i]);
			return PTR_ERR(clk);
		}
		data->pclks[i] = clk;
	}

	return 0;
}

 
void __init exynos_arm64_register_cmu(struct device *dev,
		struct device_node *np, const struct samsung_cmu_info *cmu)
{
	int err;

	 
	err = exynos_arm64_enable_bus_clk(dev, np, cmu);
	if (err)
		pr_err("%s: could not enable bus clock %s; err = %d\n",
		       __func__, cmu->clk_name, err);

	exynos_arm64_init_clocks(np, cmu->clk_regs, cmu->nr_clk_regs);
	samsung_cmu_register_one(np, cmu);
}

 
int __init exynos_arm64_register_cmu_pm(struct platform_device *pdev,
					bool set_manual)
{
	const struct samsung_cmu_info *cmu;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct exynos_arm64_cmu_data *data;
	void __iomem *reg_base;
	int ret;

	cmu = of_device_get_match_data(dev);

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);

	ret = exynos_arm64_cmu_prepare_pm(dev, cmu);
	if (ret)
		return ret;

	 
	ret = exynos_arm64_enable_bus_clk(dev, NULL, cmu);
	if (ret)
		dev_err(dev, "%s: could not enable bus clock %s; err = %d\n",
		       __func__, cmu->clk_name, ret);

	if (set_manual)
		exynos_arm64_init_clocks(np, cmu->clk_regs, cmu->nr_clk_regs);

	reg_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(reg_base))
		return PTR_ERR(reg_base);

	data->ctx = samsung_clk_init(dev, reg_base, cmu->nr_clk_ids);

	 
	pm_runtime_get_noresume(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	samsung_cmu_register_clocks(data->ctx, cmu);
	samsung_clk_of_add_provider(dev->of_node, data->ctx);
	pm_runtime_put_sync(dev);

	return 0;
}

int exynos_arm64_cmu_suspend(struct device *dev)
{
	struct exynos_arm64_cmu_data *data = dev_get_drvdata(dev);
	int i;

	samsung_clk_save(data->ctx->reg_base, data->clk_save,
			 data->nr_clk_save);

	for (i = 0; i < data->nr_pclks; i++)
		clk_prepare_enable(data->pclks[i]);

	 
	samsung_clk_restore(data->ctx->reg_base, data->clk_suspend,
			    data->nr_clk_suspend);

	for (i = 0; i < data->nr_pclks; i++)
		clk_disable_unprepare(data->pclks[i]);

	clk_disable_unprepare(data->clk);

	return 0;
}

int exynos_arm64_cmu_resume(struct device *dev)
{
	struct exynos_arm64_cmu_data *data = dev_get_drvdata(dev);
	int i;

	clk_prepare_enable(data->clk);

	for (i = 0; i < data->nr_pclks; i++)
		clk_prepare_enable(data->pclks[i]);

	samsung_clk_restore(data->ctx->reg_base, data->clk_save,
			    data->nr_clk_save);

	for (i = 0; i < data->nr_pclks; i++)
		clk_disable_unprepare(data->pclks[i]);

	return 0;
}
