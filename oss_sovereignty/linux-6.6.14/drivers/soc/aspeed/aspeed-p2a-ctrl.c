
 

#include <linux/fs.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/aspeed-p2a-ctrl.h>

#define DEVICE_NAME	"aspeed-p2a-ctrl"

 
#define SCU2C 0x2c
 
#define SCU180 0x180
 
#define SCU180_ENP2A BIT(1)

 
#define P2A_REGION_COUNT 6

struct region {
	u64 min;
	u64 max;
	u32 bit;
};

struct aspeed_p2a_model_data {
	 
	struct region regions[P2A_REGION_COUNT];
};

struct aspeed_p2a_ctrl {
	struct miscdevice miscdev;
	struct regmap *regmap;

	const struct aspeed_p2a_model_data *config;

	 
	struct mutex tracking;
	u32 readers;
	u32 readerwriters[P2A_REGION_COUNT];

	phys_addr_t mem_base;
	resource_size_t mem_size;
};

struct aspeed_p2a_user {
	struct file *file;
	struct aspeed_p2a_ctrl *parent;

	 
	u32 read;

	 
	u32 readwrite[P2A_REGION_COUNT];
};

static void aspeed_p2a_enable_bridge(struct aspeed_p2a_ctrl *p2a_ctrl)
{
	regmap_update_bits(p2a_ctrl->regmap,
		SCU180, SCU180_ENP2A, SCU180_ENP2A);
}

static void aspeed_p2a_disable_bridge(struct aspeed_p2a_ctrl *p2a_ctrl)
{
	regmap_update_bits(p2a_ctrl->regmap, SCU180, SCU180_ENP2A, 0);
}

static int aspeed_p2a_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long vsize;
	pgprot_t prot;
	struct aspeed_p2a_user *priv = file->private_data;
	struct aspeed_p2a_ctrl *ctrl = priv->parent;

	if (ctrl->mem_base == 0 && ctrl->mem_size == 0)
		return -EINVAL;

	vsize = vma->vm_end - vma->vm_start;
	prot = vma->vm_page_prot;

	if (vma->vm_pgoff + vma_pages(vma) > ctrl->mem_size >> PAGE_SHIFT)
		return -EINVAL;

	 
	prot = pgprot_noncached(prot);

	if (remap_pfn_range(vma, vma->vm_start,
		(ctrl->mem_base >> PAGE_SHIFT) + vma->vm_pgoff,
		vsize, prot))
		return -EAGAIN;

	return 0;
}

static bool aspeed_p2a_region_acquire(struct aspeed_p2a_user *priv,
		struct aspeed_p2a_ctrl *ctrl,
		struct aspeed_p2a_ctrl_mapping *map)
{
	int i;
	u64 base, end;
	bool matched = false;

	base = map->addr;
	end = map->addr + (map->length - 1);

	 
	for (i = 0; i < P2A_REGION_COUNT; i++) {
		const struct region *curr = &ctrl->config->regions[i];

		 
		if (curr->max < base)
			continue;

		 
		if (curr->min > end)
			break;

		 
		mutex_lock(&ctrl->tracking);
		ctrl->readerwriters[i] += 1;
		mutex_unlock(&ctrl->tracking);

		 
		priv->readwrite[i] += 1;

		 
		regmap_update_bits(ctrl->regmap, SCU2C, curr->bit, 0);
		matched = true;
	}

	return matched;
}

static long aspeed_p2a_ioctl(struct file *file, unsigned int cmd,
		unsigned long data)
{
	struct aspeed_p2a_user *priv = file->private_data;
	struct aspeed_p2a_ctrl *ctrl = priv->parent;
	void __user *arg = (void __user *)data;
	struct aspeed_p2a_ctrl_mapping map;

	if (copy_from_user(&map, arg, sizeof(map)))
		return -EFAULT;

	switch (cmd) {
	case ASPEED_P2A_CTRL_IOCTL_SET_WINDOW:
		 
		if (map.flags == ASPEED_P2A_CTRL_READ_ONLY) {
			mutex_lock(&ctrl->tracking);
			ctrl->readers += 1;
			mutex_unlock(&ctrl->tracking);

			 
			priv->read += 1;
		} else if (map.flags == ASPEED_P2A_CTRL_READWRITE) {
			 
			if (!aspeed_p2a_region_acquire(priv, ctrl, &map)) {
				return -EINVAL;
			}
		} else {
			 
			return -EINVAL;
		}

		aspeed_p2a_enable_bridge(ctrl);
		return 0;
	case ASPEED_P2A_CTRL_IOCTL_GET_MEMORY_CONFIG:
		 

		map.flags = 0;
		map.addr = ctrl->mem_base;
		map.length = ctrl->mem_size;

		return copy_to_user(arg, &map, sizeof(map)) ? -EFAULT : 0;
	}

	return -EINVAL;
}


 
static int aspeed_p2a_open(struct inode *inode, struct file *file)
{
	struct aspeed_p2a_user *priv;

	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->file = file;
	priv->read = 0;
	memset(priv->readwrite, 0, sizeof(priv->readwrite));

	 
	priv->parent = file->private_data;

	 
	file->private_data = priv;

	return 0;
}

 
static int aspeed_p2a_release(struct inode *inode, struct file *file)
{
	int i;
	u32 bits = 0;
	bool open_regions = false;
	struct aspeed_p2a_user *priv = file->private_data;

	 
	mutex_lock(&priv->parent->tracking);

	priv->parent->readers -= priv->read;

	for (i = 0; i < P2A_REGION_COUNT; i++) {
		priv->parent->readerwriters[i] -= priv->readwrite[i];

		if (priv->parent->readerwriters[i] > 0)
			open_regions = true;
		else
			bits |= priv->parent->config->regions[i].bit;
	}

	 

	 
	regmap_update_bits(priv->parent->regmap, SCU2C, bits, bits);

	 
	if (!open_regions && priv->parent->readers == 0)
		aspeed_p2a_disable_bridge(priv->parent);

	mutex_unlock(&priv->parent->tracking);

	kfree(priv);

	return 0;
}

static const struct file_operations aspeed_p2a_ctrl_fops = {
	.owner = THIS_MODULE,
	.mmap = aspeed_p2a_mmap,
	.unlocked_ioctl = aspeed_p2a_ioctl,
	.open = aspeed_p2a_open,
	.release = aspeed_p2a_release,
};

 
static void aspeed_p2a_disable_all(struct aspeed_p2a_ctrl *p2a_ctrl)
{
	int i;
	u32 value = 0;

	for (i = 0; i < P2A_REGION_COUNT; i++)
		value |= p2a_ctrl->config->regions[i].bit;

	regmap_update_bits(p2a_ctrl->regmap, SCU2C, value, value);

	 
	aspeed_p2a_disable_bridge(p2a_ctrl);
}

static int aspeed_p2a_ctrl_probe(struct platform_device *pdev)
{
	struct aspeed_p2a_ctrl *misc_ctrl;
	struct device *dev;
	struct resource resm;
	struct device_node *node;
	int rc = 0;

	dev = &pdev->dev;

	misc_ctrl = devm_kzalloc(dev, sizeof(*misc_ctrl), GFP_KERNEL);
	if (!misc_ctrl)
		return -ENOMEM;

	mutex_init(&misc_ctrl->tracking);

	 
	node = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (node) {
		rc = of_address_to_resource(node, 0, &resm);
		of_node_put(node);
		if (rc) {
			dev_err(dev, "Couldn't address to resource for reserved memory\n");
			return -ENODEV;
		}

		misc_ctrl->mem_size = resource_size(&resm);
		misc_ctrl->mem_base = resm.start;
	}

	misc_ctrl->regmap = syscon_node_to_regmap(pdev->dev.parent->of_node);
	if (IS_ERR(misc_ctrl->regmap)) {
		dev_err(dev, "Couldn't get regmap\n");
		return -ENODEV;
	}

	misc_ctrl->config = of_device_get_match_data(dev);

	dev_set_drvdata(&pdev->dev, misc_ctrl);

	aspeed_p2a_disable_all(misc_ctrl);

	misc_ctrl->miscdev.minor = MISC_DYNAMIC_MINOR;
	misc_ctrl->miscdev.name = DEVICE_NAME;
	misc_ctrl->miscdev.fops = &aspeed_p2a_ctrl_fops;
	misc_ctrl->miscdev.parent = dev;

	rc = misc_register(&misc_ctrl->miscdev);
	if (rc)
		dev_err(dev, "Unable to register device\n");

	return rc;
}

static int aspeed_p2a_ctrl_remove(struct platform_device *pdev)
{
	struct aspeed_p2a_ctrl *p2a_ctrl = dev_get_drvdata(&pdev->dev);

	misc_deregister(&p2a_ctrl->miscdev);

	return 0;
}

#define SCU2C_DRAM	BIT(25)
#define SCU2C_SPI	BIT(24)
#define SCU2C_SOC	BIT(23)
#define SCU2C_FLASH	BIT(22)

static const struct aspeed_p2a_model_data ast2400_model_data = {
	.regions = {
		{0x00000000, 0x17FFFFFF, SCU2C_FLASH},
		{0x18000000, 0x1FFFFFFF, SCU2C_SOC},
		{0x20000000, 0x2FFFFFFF, SCU2C_FLASH},
		{0x30000000, 0x3FFFFFFF, SCU2C_SPI},
		{0x40000000, 0x5FFFFFFF, SCU2C_DRAM},
		{0x60000000, 0xFFFFFFFF, SCU2C_SOC},
	}
};

static const struct aspeed_p2a_model_data ast2500_model_data = {
	.regions = {
		{0x00000000, 0x0FFFFFFF, SCU2C_FLASH},
		{0x10000000, 0x1FFFFFFF, SCU2C_SOC},
		{0x20000000, 0x3FFFFFFF, SCU2C_FLASH},
		{0x40000000, 0x5FFFFFFF, SCU2C_SOC},
		{0x60000000, 0x7FFFFFFF, SCU2C_SPI},
		{0x80000000, 0xFFFFFFFF, SCU2C_DRAM},
	}
};

static const struct of_device_id aspeed_p2a_ctrl_match[] = {
	{ .compatible = "aspeed,ast2400-p2a-ctrl",
	  .data = &ast2400_model_data },
	{ .compatible = "aspeed,ast2500-p2a-ctrl",
	  .data = &ast2500_model_data },
	{ },
};

static struct platform_driver aspeed_p2a_ctrl_driver = {
	.driver = {
		.name		= DEVICE_NAME,
		.of_match_table = aspeed_p2a_ctrl_match,
	},
	.probe = aspeed_p2a_ctrl_probe,
	.remove = aspeed_p2a_ctrl_remove,
};

module_platform_driver(aspeed_p2a_ctrl_driver);

MODULE_DEVICE_TABLE(of, aspeed_p2a_ctrl_match);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Patrick Venture <venture@google.com>");
MODULE_DESCRIPTION("Control for aspeed 2400/2500 P2A VGA HOST to BMC mappings");
