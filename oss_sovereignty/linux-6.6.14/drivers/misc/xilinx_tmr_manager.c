
 

#include <asm/xilinx_mb_manager.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

 
#define XTMR_MANAGER_CR_OFFSET		0x0
#define XTMR_MANAGER_FFR_OFFSET		0x4
#define XTMR_MANAGER_CMR0_OFFSET	0x8
#define XTMR_MANAGER_CMR1_OFFSET	0xC
#define XTMR_MANAGER_BDIR_OFFSET	0x10
#define XTMR_MANAGER_SEMIMR_OFFSET	0x1C

 
#define XTMR_MANAGER_CR_MAGIC1_MASK	GENMASK(7, 0)
#define XTMR_MANAGER_CR_MAGIC2_MASK	GENMASK(15, 8)
#define XTMR_MANAGER_CR_RIR_MASK	BIT(16)
#define XTMR_MANAGER_FFR_LM12_MASK	BIT(0)
#define XTMR_MANAGER_FFR_LM13_MASK	BIT(1)
#define XTMR_MANAGER_FFR_LM23_MASK	BIT(2)

#define XTMR_MANAGER_CR_MAGIC2_SHIFT	4
#define XTMR_MANAGER_CR_RIR_SHIFT	16
#define XTMR_MANAGER_CR_BB_SHIFT	18

#define XTMR_MANAGER_MAGIC1_MAX_VAL	255

 
struct xtmr_manager_dev {
	void __iomem *regs;
	u32 cr_val;
	u32 magic1;
	u32 err_cnt;
	resource_size_t phys_baseaddr;
};

 
static inline void xtmr_manager_write(struct xtmr_manager_dev *xtmr_manager,
				      u32 addr, u32 value)
{
	iowrite32(value, xtmr_manager->regs + addr);
}

static inline u32 xtmr_manager_read(struct xtmr_manager_dev *xtmr_manager,
				    u32 addr)
{
	return ioread32(xtmr_manager->regs + addr);
}

static void xmb_manager_reset_handler(struct xtmr_manager_dev *xtmr_manager)
{
	 
	xtmr_manager_write(xtmr_manager, XTMR_MANAGER_FFR_OFFSET, 0);
}

static void xmb_manager_update_errcnt(struct xtmr_manager_dev *xtmr_manager)
{
	xtmr_manager->err_cnt++;
}

static ssize_t errcnt_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct xtmr_manager_dev *xtmr_manager = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%x\n", xtmr_manager->err_cnt);
}
static DEVICE_ATTR_RO(errcnt);

static ssize_t dis_block_break_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct xtmr_manager_dev *xtmr_manager = dev_get_drvdata(dev);
	int ret;
	long value;

	ret = kstrtoul(buf, 16, &value);
	if (ret)
		return ret;

	 
	xtmr_manager->cr_val &= ~(1 << XTMR_MANAGER_CR_BB_SHIFT);
	xtmr_manager_write(xtmr_manager, XTMR_MANAGER_CR_OFFSET,
			   xtmr_manager->cr_val);
	return size;
}
static DEVICE_ATTR_WO(dis_block_break);

static struct attribute *xtmr_manager_dev_attrs[] = {
	&dev_attr_dis_block_break.attr,
	&dev_attr_errcnt.attr,
	NULL,
};
ATTRIBUTE_GROUPS(xtmr_manager_dev);

static void xtmr_manager_init(struct xtmr_manager_dev *xtmr_manager)
{
	 
	xtmr_manager_write(xtmr_manager, XTMR_MANAGER_SEMIMR_OFFSET, 0);

	 
	xtmr_manager->cr_val = (1 << XTMR_MANAGER_CR_RIR_SHIFT) |
				xtmr_manager->magic1;
	xtmr_manager_write(xtmr_manager, XTMR_MANAGER_CR_OFFSET,
			   xtmr_manager->cr_val);
	 
	xtmr_manager_write(xtmr_manager, XTMR_MANAGER_BDIR_OFFSET, 0);

	 
	xtmr_manager->cr_val |= (1 << XTMR_MANAGER_CR_BB_SHIFT);

	 
	xmb_manager_register(xtmr_manager->phys_baseaddr, xtmr_manager->cr_val,
			     (void *)xmb_manager_update_errcnt,
			     xtmr_manager, (void *)xmb_manager_reset_handler);
}

 
static int xtmr_manager_probe(struct platform_device *pdev)
{
	struct xtmr_manager_dev *xtmr_manager;
	struct resource *res;
	int err;

	xtmr_manager = devm_kzalloc(&pdev->dev, sizeof(*xtmr_manager),
				    GFP_KERNEL);
	if (!xtmr_manager)
		return -ENOMEM;

	xtmr_manager->regs = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(xtmr_manager->regs))
		return PTR_ERR(xtmr_manager->regs);

	xtmr_manager->phys_baseaddr = res->start;

	err = of_property_read_u32(pdev->dev.of_node, "xlnx,magic1",
				   &xtmr_manager->magic1);
	if (err < 0) {
		dev_err(&pdev->dev, "unable to read xlnx,magic1 property");
		return err;
	}

	if (xtmr_manager->magic1 > XTMR_MANAGER_MAGIC1_MAX_VAL) {
		dev_err(&pdev->dev, "invalid xlnx,magic1 property value");
		return -EINVAL;
	}

	 
	xtmr_manager_init(xtmr_manager);

	platform_set_drvdata(pdev, xtmr_manager);

	return 0;
}

static const struct of_device_id xtmr_manager_of_match[] = {
	{
		.compatible = "xlnx,tmr-manager-1.0",
	},
	{   }
};
MODULE_DEVICE_TABLE(of, xtmr_manager_of_match);

static struct platform_driver xtmr_manager_driver = {
	.driver = {
		.name = "xilinx-tmr_manager",
		.of_match_table = xtmr_manager_of_match,
		.dev_groups = xtmr_manager_dev_groups,
	},
	.probe = xtmr_manager_probe,
};
module_platform_driver(xtmr_manager_driver);

MODULE_AUTHOR("Advanced Micro Devices, Inc");
MODULE_DESCRIPTION("Xilinx TMR Manager Driver");
MODULE_LICENSE("GPL");
