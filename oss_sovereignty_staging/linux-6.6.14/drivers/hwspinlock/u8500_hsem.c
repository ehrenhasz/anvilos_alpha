
 

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hwspinlock.h>
#include <linux/platform_device.h>

#include "hwspinlock_internal.h"

 

#define U8500_MAX_SEMAPHORE		32	 
#define RESET_SEMAPHORE			(0)	 

 
#define HSEM_MASTER_ID			0x01

#define HSEM_REGISTER_OFFSET		0x08

#define HSEM_CTRL_REG			0x00
#define HSEM_ICRALL			0x90
#define HSEM_PROTOCOL_1			0x01

static int u8500_hsem_trylock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;

	writel(HSEM_MASTER_ID, lock_addr);

	 
	return (HSEM_MASTER_ID == (0x0F & readl(lock_addr)));
}

static void u8500_hsem_unlock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;

	 
	writel(RESET_SEMAPHORE, lock_addr);
}

 
static void u8500_hsem_relax(struct hwspinlock *lock)
{
	ndelay(50);
}

static const struct hwspinlock_ops u8500_hwspinlock_ops = {
	.trylock	= u8500_hsem_trylock,
	.unlock		= u8500_hsem_unlock,
	.relax		= u8500_hsem_relax,
};

static int u8500_hsem_probe(struct platform_device *pdev)
{
	struct hwspinlock_pdata *pdata = pdev->dev.platform_data;
	struct hwspinlock_device *bank;
	struct hwspinlock *hwlock;
	void __iomem *io_base;
	int i, num_locks = U8500_MAX_SEMAPHORE;
	ulong val;

	if (!pdata)
		return -ENODEV;

	io_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(io_base))
		return PTR_ERR(io_base);

	 
	val = readl(io_base + HSEM_CTRL_REG);
	writel((val & ~HSEM_PROTOCOL_1), io_base + HSEM_CTRL_REG);

	 
	writel(0xFFFF, io_base + HSEM_ICRALL);

	bank = devm_kzalloc(&pdev->dev, struct_size(bank, lock, num_locks),
			    GFP_KERNEL);
	if (!bank)
		return -ENOMEM;

	platform_set_drvdata(pdev, bank);

	for (i = 0, hwlock = &bank->lock[0]; i < num_locks; i++, hwlock++)
		hwlock->priv = io_base + HSEM_REGISTER_OFFSET + sizeof(u32) * i;

	return devm_hwspin_lock_register(&pdev->dev, bank,
					 &u8500_hwspinlock_ops,
					 pdata->base_id, num_locks);
}

static void u8500_hsem_remove(struct platform_device *pdev)
{
	struct hwspinlock_device *bank = platform_get_drvdata(pdev);
	void __iomem *io_base = bank->lock[0].priv - HSEM_REGISTER_OFFSET;

	 
	writel(0xFFFF, io_base + HSEM_ICRALL);
}

static struct platform_driver u8500_hsem_driver = {
	.probe		= u8500_hsem_probe,
	.remove_new	= u8500_hsem_remove,
	.driver		= {
		.name	= "u8500_hsem",
	},
};

static int __init u8500_hsem_init(void)
{
	return platform_driver_register(&u8500_hsem_driver);
}
 
postcore_initcall(u8500_hsem_init);

static void __exit u8500_hsem_exit(void)
{
	platform_driver_unregister(&u8500_hsem_driver);
}
module_exit(u8500_hsem_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hardware Spinlock driver for u8500");
MODULE_AUTHOR("Mathieu Poirier <mathieu.poirier@linaro.org>");
