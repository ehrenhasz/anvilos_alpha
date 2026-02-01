
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hwspinlock.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "hwspinlock_internal.h"

 
#define SYSSTATUS_OFFSET		0x0014
#define LOCK_BASE_OFFSET		0x0800

#define SPINLOCK_NUMLOCKS_BIT_OFFSET	(24)

 
#define SPINLOCK_NOTTAKEN		(0)	 
#define SPINLOCK_TAKEN			(1)	 

static int omap_hwspinlock_trylock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;

	 
	return (SPINLOCK_NOTTAKEN == readl(lock_addr));
}

static void omap_hwspinlock_unlock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;

	 
	writel(SPINLOCK_NOTTAKEN, lock_addr);
}

 
static void omap_hwspinlock_relax(struct hwspinlock *lock)
{
	ndelay(50);
}

static const struct hwspinlock_ops omap_hwspinlock_ops = {
	.trylock = omap_hwspinlock_trylock,
	.unlock = omap_hwspinlock_unlock,
	.relax = omap_hwspinlock_relax,
};

static int omap_hwspinlock_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct hwspinlock_device *bank;
	struct hwspinlock *hwlock;
	void __iomem *io_base;
	int num_locks, i, ret;
	 
	int base_id = 0;

	if (!node)
		return -ENODEV;

	io_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(io_base))
		return PTR_ERR(io_base);

	 
	pm_runtime_enable(&pdev->dev);
	ret = pm_runtime_resume_and_get(&pdev->dev);
	if (ret < 0)
		goto runtime_err;

	 
	i = readl(io_base + SYSSTATUS_OFFSET);
	i >>= SPINLOCK_NUMLOCKS_BIT_OFFSET;

	 
	ret = pm_runtime_put(&pdev->dev);
	if (ret < 0)
		goto runtime_err;

	 
	if (hweight_long(i & 0xf) != 1 || i > 8) {
		ret = -EINVAL;
		goto runtime_err;
	}

	num_locks = i * 32;  

	bank = devm_kzalloc(&pdev->dev, struct_size(bank, lock, num_locks),
			    GFP_KERNEL);
	if (!bank) {
		ret = -ENOMEM;
		goto runtime_err;
	}

	platform_set_drvdata(pdev, bank);

	for (i = 0, hwlock = &bank->lock[0]; i < num_locks; i++, hwlock++)
		hwlock->priv = io_base + LOCK_BASE_OFFSET + sizeof(u32) * i;

	ret = hwspin_lock_register(bank, &pdev->dev, &omap_hwspinlock_ops,
						base_id, num_locks);
	if (ret)
		goto runtime_err;

	dev_dbg(&pdev->dev, "Registered %d locks with HwSpinlock core\n",
		num_locks);

	return 0;

runtime_err:
	pm_runtime_disable(&pdev->dev);
	return ret;
}

static void omap_hwspinlock_remove(struct platform_device *pdev)
{
	struct hwspinlock_device *bank = platform_get_drvdata(pdev);
	int ret;

	ret = hwspin_lock_unregister(bank);
	if (ret) {
		dev_err(&pdev->dev, "%s failed: %d\n", __func__, ret);
		return;
	}

	pm_runtime_disable(&pdev->dev);
}

static const struct of_device_id omap_hwspinlock_of_match[] = {
	{ .compatible = "ti,omap4-hwspinlock", },
	{ .compatible = "ti,am64-hwspinlock", },
	{ .compatible = "ti,am654-hwspinlock", },
	{   },
};
MODULE_DEVICE_TABLE(of, omap_hwspinlock_of_match);

static struct platform_driver omap_hwspinlock_driver = {
	.probe		= omap_hwspinlock_probe,
	.remove_new	= omap_hwspinlock_remove,
	.driver		= {
		.name	= "omap_hwspinlock",
		.of_match_table = omap_hwspinlock_of_match,
	},
};

static int __init omap_hwspinlock_init(void)
{
	return platform_driver_register(&omap_hwspinlock_driver);
}
 
postcore_initcall(omap_hwspinlock_init);

static void __exit omap_hwspinlock_exit(void)
{
	platform_driver_unregister(&omap_hwspinlock_driver);
}
module_exit(omap_hwspinlock_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hardware spinlock driver for OMAP");
MODULE_AUTHOR("Simon Que <sque@ti.com>");
MODULE_AUTHOR("Hari Kanigeri <h-kanigeri2@ti.com>");
MODULE_AUTHOR("Ohad Ben-Cohen <ohad@wizery.com>");
