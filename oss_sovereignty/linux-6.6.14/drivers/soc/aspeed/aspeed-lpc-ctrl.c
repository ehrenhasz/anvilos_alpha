
 

#include <linux/clk.h>
#include <linux/log2.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/regmap.h>

#include <linux/aspeed-lpc-ctrl.h>

#define DEVICE_NAME	"aspeed-lpc-ctrl"

#define HICR5 0x80
#define HICR5_ENL2H	BIT(8)
#define HICR5_ENFWH	BIT(10)

#define HICR6 0x84
#define SW_FWH2AHB	BIT(17)

#define HICR7 0x88
#define HICR8 0x8c

struct aspeed_lpc_ctrl {
	struct miscdevice	miscdev;
	struct regmap		*regmap;
	struct clk		*clk;
	phys_addr_t		mem_base;
	resource_size_t		mem_size;
	u32			pnor_size;
	u32			pnor_base;
	bool			fwh2ahb;
	struct regmap		*scu;
};

static struct aspeed_lpc_ctrl *file_aspeed_lpc_ctrl(struct file *file)
{
	return container_of(file->private_data, struct aspeed_lpc_ctrl,
			miscdev);
}

static int aspeed_lpc_ctrl_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct aspeed_lpc_ctrl *lpc_ctrl = file_aspeed_lpc_ctrl(file);
	unsigned long vsize = vma->vm_end - vma->vm_start;
	pgprot_t prot = vma->vm_page_prot;

	if (vma->vm_pgoff + vma_pages(vma) > lpc_ctrl->mem_size >> PAGE_SHIFT)
		return -EINVAL;

	 
	prot = pgprot_noncached(prot);

	if (remap_pfn_range(vma, vma->vm_start,
		(lpc_ctrl->mem_base >> PAGE_SHIFT) + vma->vm_pgoff,
		vsize, prot))
		return -EAGAIN;

	return 0;
}

static long aspeed_lpc_ctrl_ioctl(struct file *file, unsigned int cmd,
		unsigned long param)
{
	struct aspeed_lpc_ctrl *lpc_ctrl = file_aspeed_lpc_ctrl(file);
	struct device *dev = file->private_data;
	void __user *p = (void __user *)param;
	struct aspeed_lpc_ctrl_mapping map;
	u32 addr;
	u32 size;
	long rc;

	if (copy_from_user(&map, p, sizeof(map)))
		return -EFAULT;

	if (map.flags != 0)
		return -EINVAL;

	switch (cmd) {
	case ASPEED_LPC_CTRL_IOCTL_GET_SIZE:
		 
		if (map.window_type != ASPEED_LPC_CTRL_WINDOW_MEMORY)
			return -EINVAL;

		 
		if (map.window_id != 0)
			return -EINVAL;

		 
		if (!lpc_ctrl->mem_size) {
			dev_dbg(dev, "Didn't find reserved memory\n");
			return -ENXIO;
		}

		map.size = lpc_ctrl->mem_size;

		return copy_to_user(p, &map, sizeof(map)) ? -EFAULT : 0;
	case ASPEED_LPC_CTRL_IOCTL_MAP:

		 

		 

		if ((map.size & 0x0000ffff) || (map.offset & 0x0000ffff))
			return -EINVAL;

		 
		if (map.offset & (map.size - 1))
			return -EINVAL;

		if (map.window_type == ASPEED_LPC_CTRL_WINDOW_FLASH) {
			if (!lpc_ctrl->pnor_size) {
				dev_dbg(dev, "Didn't find host pnor flash\n");
				return -ENXIO;
			}
			addr = lpc_ctrl->pnor_base;
			size = lpc_ctrl->pnor_size;
		} else if (map.window_type == ASPEED_LPC_CTRL_WINDOW_MEMORY) {
			 
			if (!lpc_ctrl->mem_size) {
				dev_dbg(dev, "Didn't find reserved memory\n");
				return -ENXIO;
			}
			addr = lpc_ctrl->mem_base;
			size = lpc_ctrl->mem_size;
		} else {
			return -EINVAL;
		}

		 
		if (map.offset + map.size < map.offset ||
			map.offset + map.size > size)
			return -EINVAL;

		if (map.size == 0 || map.size > size)
			return -EINVAL;

		addr += map.offset;

		 

		rc = regmap_write(lpc_ctrl->regmap, HICR7,
				(addr | (map.addr >> 16)));
		if (rc)
			return rc;

		rc = regmap_write(lpc_ctrl->regmap, HICR8,
				(~(map.size - 1)) | ((map.size >> 16) - 1));
		if (rc)
			return rc;

		 
		if (lpc_ctrl->fwh2ahb) {
			 
			regmap_update_bits(lpc_ctrl->scu, 0x0D8, BIT(2), 0);

			 
			regmap_write(lpc_ctrl->regmap, HICR6, SW_FWH2AHB);
		}

		 
		return regmap_update_bits(lpc_ctrl->regmap, HICR5,
				HICR5_ENFWH | HICR5_ENL2H,
				HICR5_ENFWH | HICR5_ENL2H);
	}

	return -EINVAL;
}

static const struct file_operations aspeed_lpc_ctrl_fops = {
	.owner		= THIS_MODULE,
	.mmap		= aspeed_lpc_ctrl_mmap,
	.unlocked_ioctl	= aspeed_lpc_ctrl_ioctl,
};

static int aspeed_lpc_ctrl_probe(struct platform_device *pdev)
{
	struct aspeed_lpc_ctrl *lpc_ctrl;
	struct device_node *node;
	struct resource resm;
	struct device *dev;
	struct device_node *np;
	int rc;

	dev = &pdev->dev;

	lpc_ctrl = devm_kzalloc(dev, sizeof(*lpc_ctrl), GFP_KERNEL);
	if (!lpc_ctrl)
		return -ENOMEM;

	 
	node = of_parse_phandle(dev->of_node, "flash", 0);
	if (!node) {
		dev_dbg(dev, "Didn't find host pnor flash node\n");
	} else {
		rc = of_address_to_resource(node, 1, &resm);
		of_node_put(node);
		if (rc) {
			dev_err(dev, "Couldn't address to resource for flash\n");
			return rc;
		}

		lpc_ctrl->pnor_size = resource_size(&resm);
		lpc_ctrl->pnor_base = resm.start;
	}


	dev_set_drvdata(&pdev->dev, lpc_ctrl);

	 
	node = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!node) {
		dev_dbg(dev, "Didn't find reserved memory\n");
	} else {
		rc = of_address_to_resource(node, 0, &resm);
		of_node_put(node);
		if (rc) {
			dev_err(dev, "Couldn't address to resource for reserved memory\n");
			return -ENXIO;
		}

		lpc_ctrl->mem_size = resource_size(&resm);
		lpc_ctrl->mem_base = resm.start;

		if (!is_power_of_2(lpc_ctrl->mem_size)) {
			dev_err(dev, "Reserved memory size must be a power of 2, got %u\n",
			       (unsigned int)lpc_ctrl->mem_size);
			return -EINVAL;
		}

		if (!IS_ALIGNED(lpc_ctrl->mem_base, lpc_ctrl->mem_size)) {
			dev_err(dev, "Reserved memory must be naturally aligned for size %u\n",
			       (unsigned int)lpc_ctrl->mem_size);
			return -EINVAL;
		}
	}

	np = pdev->dev.parent->of_node;
	if (!of_device_is_compatible(np, "aspeed,ast2400-lpc-v2") &&
	    !of_device_is_compatible(np, "aspeed,ast2500-lpc-v2") &&
	    !of_device_is_compatible(np, "aspeed,ast2600-lpc-v2")) {
		dev_err(dev, "unsupported LPC device binding\n");
		return -ENODEV;
	}

	lpc_ctrl->regmap = syscon_node_to_regmap(np);
	if (IS_ERR(lpc_ctrl->regmap)) {
		dev_err(dev, "Couldn't get regmap\n");
		return -ENODEV;
	}

	if (of_device_is_compatible(dev->of_node, "aspeed,ast2600-lpc-ctrl")) {
		lpc_ctrl->fwh2ahb = true;

		lpc_ctrl->scu = syscon_regmap_lookup_by_compatible("aspeed,ast2600-scu");
		if (IS_ERR(lpc_ctrl->scu)) {
			dev_err(dev, "couldn't find scu\n");
			return PTR_ERR(lpc_ctrl->scu);
		}
	}

	lpc_ctrl->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(lpc_ctrl->clk))
		return dev_err_probe(dev, PTR_ERR(lpc_ctrl->clk),
				     "couldn't get clock\n");
	rc = clk_prepare_enable(lpc_ctrl->clk);
	if (rc) {
		dev_err(dev, "couldn't enable clock\n");
		return rc;
	}

	lpc_ctrl->miscdev.minor = MISC_DYNAMIC_MINOR;
	lpc_ctrl->miscdev.name = DEVICE_NAME;
	lpc_ctrl->miscdev.fops = &aspeed_lpc_ctrl_fops;
	lpc_ctrl->miscdev.parent = dev;
	rc = misc_register(&lpc_ctrl->miscdev);
	if (rc) {
		dev_err(dev, "Unable to register device\n");
		goto err;
	}

	return 0;

err:
	clk_disable_unprepare(lpc_ctrl->clk);
	return rc;
}

static int aspeed_lpc_ctrl_remove(struct platform_device *pdev)
{
	struct aspeed_lpc_ctrl *lpc_ctrl = dev_get_drvdata(&pdev->dev);

	misc_deregister(&lpc_ctrl->miscdev);
	clk_disable_unprepare(lpc_ctrl->clk);

	return 0;
}

static const struct of_device_id aspeed_lpc_ctrl_match[] = {
	{ .compatible = "aspeed,ast2400-lpc-ctrl" },
	{ .compatible = "aspeed,ast2500-lpc-ctrl" },
	{ .compatible = "aspeed,ast2600-lpc-ctrl" },
	{ },
};

static struct platform_driver aspeed_lpc_ctrl_driver = {
	.driver = {
		.name		= DEVICE_NAME,
		.of_match_table = aspeed_lpc_ctrl_match,
	},
	.probe = aspeed_lpc_ctrl_probe,
	.remove = aspeed_lpc_ctrl_remove,
};

module_platform_driver(aspeed_lpc_ctrl_driver);

MODULE_DEVICE_TABLE(of, aspeed_lpc_ctrl_match);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cyril Bur <cyrilbur@gmail.com>");
MODULE_DESCRIPTION("Control for ASPEED LPC HOST to BMC mappings");
