
 

#include <linux/platform_device.h>
#include <linux/uio_driver.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_data/uio_dmem_genirq.h>
#include <linux/stringify.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/irq.h>

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>

#define DRIVER_NAME "uio_dmem_genirq"
#define DMEM_MAP_ERROR (~0)

struct uio_dmem_genirq_platdata {
	struct uio_info *uioinfo;
	spinlock_t lock;
	unsigned long flags;
	struct platform_device *pdev;
	unsigned int dmem_region_start;
	unsigned int num_dmem_regions;
	void *dmem_region_vaddr[MAX_UIO_MAPS];
	struct mutex alloc_lock;
	unsigned int refcnt;
};

 
enum {
	UIO_IRQ_DISABLED = 0,
};

static int uio_dmem_genirq_open(struct uio_info *info, struct inode *inode)
{
	struct uio_dmem_genirq_platdata *priv = info->priv;
	struct uio_mem *uiomem;
	int dmem_region = priv->dmem_region_start;

	uiomem = &priv->uioinfo->mem[priv->dmem_region_start];

	mutex_lock(&priv->alloc_lock);
	while (!priv->refcnt && uiomem < &priv->uioinfo->mem[MAX_UIO_MAPS]) {
		void *addr;
		if (!uiomem->size)
			break;

		addr = dma_alloc_coherent(&priv->pdev->dev, uiomem->size,
				(dma_addr_t *)&uiomem->addr, GFP_KERNEL);
		if (!addr) {
			uiomem->addr = DMEM_MAP_ERROR;
		}
		priv->dmem_region_vaddr[dmem_region++] = addr;
		++uiomem;
	}
	priv->refcnt++;

	mutex_unlock(&priv->alloc_lock);
	 
	pm_runtime_get_sync(&priv->pdev->dev);
	return 0;
}

static int uio_dmem_genirq_release(struct uio_info *info, struct inode *inode)
{
	struct uio_dmem_genirq_platdata *priv = info->priv;
	struct uio_mem *uiomem;
	int dmem_region = priv->dmem_region_start;

	 
	pm_runtime_put_sync(&priv->pdev->dev);

	uiomem = &priv->uioinfo->mem[priv->dmem_region_start];

	mutex_lock(&priv->alloc_lock);

	priv->refcnt--;
	while (!priv->refcnt && uiomem < &priv->uioinfo->mem[MAX_UIO_MAPS]) {
		if (!uiomem->size)
			break;
		if (priv->dmem_region_vaddr[dmem_region]) {
			dma_free_coherent(&priv->pdev->dev, uiomem->size,
					priv->dmem_region_vaddr[dmem_region],
					uiomem->addr);
		}
		uiomem->addr = DMEM_MAP_ERROR;
		++dmem_region;
		++uiomem;
	}

	mutex_unlock(&priv->alloc_lock);
	return 0;
}

static irqreturn_t uio_dmem_genirq_handler(int irq, struct uio_info *dev_info)
{
	struct uio_dmem_genirq_platdata *priv = dev_info->priv;

	 

	spin_lock(&priv->lock);
	if (!__test_and_set_bit(UIO_IRQ_DISABLED, &priv->flags))
		disable_irq_nosync(irq);
	spin_unlock(&priv->lock);

	return IRQ_HANDLED;
}

static int uio_dmem_genirq_irqcontrol(struct uio_info *dev_info, s32 irq_on)
{
	struct uio_dmem_genirq_platdata *priv = dev_info->priv;
	unsigned long flags;

	 

	spin_lock_irqsave(&priv->lock, flags);
	if (irq_on) {
		if (__test_and_clear_bit(UIO_IRQ_DISABLED, &priv->flags))
			enable_irq(dev_info->irq);
	} else {
		if (!__test_and_set_bit(UIO_IRQ_DISABLED, &priv->flags))
			disable_irq_nosync(dev_info->irq);
	}
	spin_unlock_irqrestore(&priv->lock, flags);

	return 0;
}

static void uio_dmem_genirq_pm_disable(void *data)
{
	struct device *dev = data;

	pm_runtime_disable(dev);
}

static int uio_dmem_genirq_probe(struct platform_device *pdev)
{
	struct uio_dmem_genirq_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct uio_info *uioinfo = &pdata->uioinfo;
	struct uio_dmem_genirq_platdata *priv;
	struct uio_mem *uiomem;
	int ret = -EINVAL;
	int i;

	if (pdev->dev.of_node) {
		 
		uioinfo = devm_kzalloc(&pdev->dev, sizeof(*uioinfo), GFP_KERNEL);
		if (!uioinfo) {
			dev_err(&pdev->dev, "unable to kmalloc\n");
			return -ENOMEM;
		}
		uioinfo->name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%pOFn",
					       pdev->dev.of_node);
		uioinfo->version = "devicetree";
	}

	if (!uioinfo || !uioinfo->name || !uioinfo->version) {
		dev_err(&pdev->dev, "missing platform_data\n");
		return -EINVAL;
	}

	if (uioinfo->handler || uioinfo->irqcontrol ||
	    uioinfo->irq_flags & IRQF_SHARED) {
		dev_err(&pdev->dev, "interrupt configuration error\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "unable to kmalloc\n");
		return -ENOMEM;
	}

	ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(&pdev->dev, "DMA enable failed\n");
		return ret;
	}

	priv->uioinfo = uioinfo;
	spin_lock_init(&priv->lock);
	priv->flags = 0;  
	priv->pdev = pdev;
	mutex_init(&priv->alloc_lock);

	if (!uioinfo->irq) {
		 
		ret = platform_get_irq(pdev, 0);
		if (ret == -ENXIO && pdev->dev.of_node)
			ret = UIO_IRQ_NONE;
		else if (ret < 0)
			return ret;
		uioinfo->irq = ret;
	}

	if (uioinfo->irq) {
		struct irq_data *irq_data = irq_get_irq_data(uioinfo->irq);

		 
		if (irq_data &&
		    irqd_get_trigger_type(irq_data) & IRQ_TYPE_LEVEL_MASK) {
			dev_dbg(&pdev->dev, "disable lazy unmask\n");
			irq_set_status_flags(uioinfo->irq, IRQ_DISABLE_UNLAZY);
		}
	}

	uiomem = &uioinfo->mem[0];

	for (i = 0; i < pdev->num_resources; ++i) {
		struct resource *r = &pdev->resource[i];

		if (r->flags != IORESOURCE_MEM)
			continue;

		if (uiomem >= &uioinfo->mem[MAX_UIO_MAPS]) {
			dev_warn(&pdev->dev, "device has more than "
					__stringify(MAX_UIO_MAPS)
					" I/O memory resources.\n");
			break;
		}

		uiomem->memtype = UIO_MEM_PHYS;
		uiomem->addr = r->start;
		uiomem->size = resource_size(r);
		++uiomem;
	}

	priv->dmem_region_start = uiomem - &uioinfo->mem[0];
	priv->num_dmem_regions = pdata->num_dynamic_regions;

	for (i = 0; i < pdata->num_dynamic_regions; ++i) {
		if (uiomem >= &uioinfo->mem[MAX_UIO_MAPS]) {
			dev_warn(&pdev->dev, "device has more than "
					__stringify(MAX_UIO_MAPS)
					" dynamic and fixed memory regions.\n");
			break;
		}
		uiomem->memtype = UIO_MEM_PHYS;
		uiomem->addr = DMEM_MAP_ERROR;
		uiomem->size = pdata->dynamic_region_sizes[i];
		++uiomem;
	}

	while (uiomem < &uioinfo->mem[MAX_UIO_MAPS]) {
		uiomem->size = 0;
		++uiomem;
	}

	 

	uioinfo->handler = uio_dmem_genirq_handler;
	uioinfo->irqcontrol = uio_dmem_genirq_irqcontrol;
	uioinfo->open = uio_dmem_genirq_open;
	uioinfo->release = uio_dmem_genirq_release;
	uioinfo->priv = priv;

	 
	pm_runtime_enable(&pdev->dev);

	ret = devm_add_action_or_reset(&pdev->dev, uio_dmem_genirq_pm_disable, &pdev->dev);
	if (ret)
		return ret;

	return devm_uio_register_device(&pdev->dev, priv->uioinfo);
}

static int uio_dmem_genirq_runtime_nop(struct device *dev)
{
	 
	return 0;
}

static const struct dev_pm_ops uio_dmem_genirq_dev_pm_ops = {
	.runtime_suspend = uio_dmem_genirq_runtime_nop,
	.runtime_resume = uio_dmem_genirq_runtime_nop,
};

#ifdef CONFIG_OF
static const struct of_device_id uio_of_genirq_match[] = {
	{   },
};
MODULE_DEVICE_TABLE(of, uio_of_genirq_match);
#endif

static struct platform_driver uio_dmem_genirq = {
	.probe = uio_dmem_genirq_probe,
	.driver = {
		.name = DRIVER_NAME,
		.pm = &uio_dmem_genirq_dev_pm_ops,
		.of_match_table = of_match_ptr(uio_of_genirq_match),
	},
};

module_platform_driver(uio_dmem_genirq);

MODULE_AUTHOR("Damian Hobson-Garcia");
MODULE_DESCRIPTION("Userspace I/O platform driver with dynamic memory.");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRIVER_NAME);
