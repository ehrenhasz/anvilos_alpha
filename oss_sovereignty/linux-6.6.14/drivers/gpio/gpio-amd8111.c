
 
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio/driver.h>
#include <linux/pci.h>
#include <linux/spinlock.h>

#define PMBASE_OFFSET 0xb0
#define PMBASE_SIZE   0x30

#define AMD_REG_GPIO(i) (0x10 + (i))

#define AMD_GPIO_LTCH_STS	0x40  
#define AMD_GPIO_RTIN		0x20  
#define AMD_GPIO_DEBOUNCE	0x10  
#define AMD_GPIO_MODE_MASK	0x0c  
#define AMD_GPIO_MODE_IN	0x00
#define AMD_GPIO_MODE_OUT	0x04
 
#define AMD_GPIO_MODE_ALTFN	0x08  
#define AMD_GPIO_X_MASK		0x03  
#define AMD_GPIO_X_IN_ACTIVEHI	0x01  
#define AMD_GPIO_X_IN_LATCH	0x02  
#define AMD_GPIO_X_OUT_LOW	0x00
#define AMD_GPIO_X_OUT_HI	0x01
#define AMD_GPIO_X_OUT_CLK0	0x02
#define AMD_GPIO_X_OUT_CLK1	0x03

 
static const struct pci_device_id pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_8111_SMBUS), 0 },
	{ 0, },	 
};
MODULE_DEVICE_TABLE(pci, pci_tbl);

struct amd_gpio {
	struct gpio_chip	chip;
	u32			pmbase;
	void __iomem		*pm;
	struct pci_dev		*pdev;
	spinlock_t		lock;  
	u8			orig[32];
};

static int amd_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	struct amd_gpio *agp = gpiochip_get_data(chip);

	agp->orig[offset] = ioread8(agp->pm + AMD_REG_GPIO(offset)) &
		(AMD_GPIO_DEBOUNCE | AMD_GPIO_MODE_MASK | AMD_GPIO_X_MASK);

	dev_dbg(&agp->pdev->dev, "Requested gpio %d, data %x\n", offset, agp->orig[offset]);

	return 0;
}

static void amd_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	struct amd_gpio *agp = gpiochip_get_data(chip);

	dev_dbg(&agp->pdev->dev, "Freed gpio %d, data %x\n", offset, agp->orig[offset]);

	iowrite8(agp->orig[offset], agp->pm + AMD_REG_GPIO(offset));
}

static void amd_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct amd_gpio *agp = gpiochip_get_data(chip);
	u8 temp;
	unsigned long flags;

	spin_lock_irqsave(&agp->lock, flags);
	temp = ioread8(agp->pm + AMD_REG_GPIO(offset));
	temp = (temp & AMD_GPIO_DEBOUNCE) | AMD_GPIO_MODE_OUT | (value ? AMD_GPIO_X_OUT_HI : AMD_GPIO_X_OUT_LOW);
	iowrite8(temp, agp->pm + AMD_REG_GPIO(offset));
	spin_unlock_irqrestore(&agp->lock, flags);

	dev_dbg(&agp->pdev->dev, "Setting gpio %d, value %d, reg=%02x\n", offset, !!value, temp);
}

static int amd_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct amd_gpio *agp = gpiochip_get_data(chip);
	u8 temp;

	temp = ioread8(agp->pm + AMD_REG_GPIO(offset));

	dev_dbg(&agp->pdev->dev, "Getting gpio %d, reg=%02x\n", offset, temp);

	return (temp & AMD_GPIO_RTIN) ? 1 : 0;
}

static int amd_gpio_dirout(struct gpio_chip *chip, unsigned offset, int value)
{
	struct amd_gpio *agp = gpiochip_get_data(chip);
	u8 temp;
	unsigned long flags;

	spin_lock_irqsave(&agp->lock, flags);
	temp = ioread8(agp->pm + AMD_REG_GPIO(offset));
	temp = (temp & AMD_GPIO_DEBOUNCE) | AMD_GPIO_MODE_OUT | (value ? AMD_GPIO_X_OUT_HI : AMD_GPIO_X_OUT_LOW);
	iowrite8(temp, agp->pm + AMD_REG_GPIO(offset));
	spin_unlock_irqrestore(&agp->lock, flags);

	dev_dbg(&agp->pdev->dev, "Dirout gpio %d, value %d, reg=%02x\n", offset, !!value, temp);

	return 0;
}

static int amd_gpio_dirin(struct gpio_chip *chip, unsigned offset)
{
	struct amd_gpio *agp = gpiochip_get_data(chip);
	u8 temp;
	unsigned long flags;

	spin_lock_irqsave(&agp->lock, flags);
	temp = ioread8(agp->pm + AMD_REG_GPIO(offset));
	temp = (temp & AMD_GPIO_DEBOUNCE) | AMD_GPIO_MODE_IN;
	iowrite8(temp, agp->pm + AMD_REG_GPIO(offset));
	spin_unlock_irqrestore(&agp->lock, flags);

	dev_dbg(&agp->pdev->dev, "Dirin gpio %d, reg=%02x\n", offset, temp);

	return 0;
}

static struct amd_gpio gp = {
	.chip = {
		.label		= "AMD GPIO",
		.owner		= THIS_MODULE,
		.base		= -1,
		.ngpio		= 32,
		.request	= amd_gpio_request,
		.free		= amd_gpio_free,
		.set		= amd_gpio_set,
		.get		= amd_gpio_get,
		.direction_output = amd_gpio_dirout,
		.direction_input = amd_gpio_dirin,
	},
};

static int __init amd_gpio_init(void)
{
	int err = -ENODEV;
	struct pci_dev *pdev = NULL;
	const struct pci_device_id *ent;

	 
	for_each_pci_dev(pdev) {
		ent = pci_match_id(pci_tbl, pdev);
		if (ent)
			goto found;
	}
	 
	goto out;

found:
	err = pci_read_config_dword(pdev, 0x58, &gp.pmbase);
	if (err)
		goto out;
	err = -EIO;
	gp.pmbase &= 0x0000FF00;
	if (gp.pmbase == 0)
		goto out;
	if (!devm_request_region(&pdev->dev, gp.pmbase + PMBASE_OFFSET,
		PMBASE_SIZE, "AMD GPIO")) {
		dev_err(&pdev->dev, "AMD GPIO region 0x%x already in use!\n",
			gp.pmbase + PMBASE_OFFSET);
		err = -EBUSY;
		goto out;
	}
	gp.pm = ioport_map(gp.pmbase + PMBASE_OFFSET, PMBASE_SIZE);
	if (!gp.pm) {
		dev_err(&pdev->dev, "Couldn't map io port into io memory\n");
		err = -ENOMEM;
		goto out;
	}
	gp.pdev = pdev;
	gp.chip.parent = &pdev->dev;

	spin_lock_init(&gp.lock);

	dev_info(&pdev->dev, "AMD-8111 GPIO detected\n");
	err = gpiochip_add_data(&gp.chip, &gp);
	if (err) {
		dev_err(&pdev->dev, "GPIO registering failed (%d)\n", err);
		ioport_unmap(gp.pm);
		goto out;
	}
	return 0;

out:
	pci_dev_put(pdev);
	return err;
}

static void __exit amd_gpio_exit(void)
{
	gpiochip_remove(&gp.chip);
	ioport_unmap(gp.pm);
	pci_dev_put(gp.pdev);
}

module_init(amd_gpio_init);
module_exit(amd_gpio_exit);

MODULE_AUTHOR("The Linux Kernel team");
MODULE_DESCRIPTION("GPIO driver for AMD chipsets");
MODULE_LICENSE("GPL");
