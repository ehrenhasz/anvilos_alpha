
 
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio/regmap.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/types.h>

 
#define PLX_PEX8311_PCI_LCS_INTCSR  0x68
#define INTCSR_INTERNAL_PCI_WIRE    BIT(8)
#define INTCSR_LOCAL_INPUT          BIT(11)
#define IDIO_24_ENABLE_IRQ          (INTCSR_INTERNAL_PCI_WIRE | INTCSR_LOCAL_INPUT)

#define IDIO_24_OUT_BASE 0x0
#define IDIO_24_TTLCMOS_OUT_REG 0x3
#define IDIO_24_IN_BASE 0x4
#define IDIO_24_TTLCMOS_IN_REG 0x7
#define IDIO_24_COS_STATUS_BASE 0x8
#define IDIO_24_CONTROL_REG 0xC
#define IDIO_24_COS_ENABLE 0xE
#define IDIO_24_SOFT_RESET 0xF

#define CONTROL_REG_OUT_MODE BIT(1)

#define COS_ENABLE_RISING BIT(1)
#define COS_ENABLE_FALLING BIT(4)
#define COS_ENABLE_BOTH (COS_ENABLE_RISING | COS_ENABLE_FALLING)

static const struct regmap_config pex8311_intcsr_regmap_config = {
	.name = "pex8311_intcsr",
	.reg_bits = 32,
	.reg_stride = 1,
	.reg_base = PLX_PEX8311_PCI_LCS_INTCSR,
	.val_bits = 32,
	.io_port = true,
};

static const struct regmap_range idio_24_wr_ranges[] = {
	regmap_reg_range(0x0, 0x3), regmap_reg_range(0x8, 0xC),
	regmap_reg_range(0xE, 0xF),
};
static const struct regmap_range idio_24_rd_ranges[] = {
	regmap_reg_range(0x0, 0xC), regmap_reg_range(0xE, 0xF),
};
static const struct regmap_range idio_24_volatile_ranges[] = {
	regmap_reg_range(0x4, 0xB), regmap_reg_range(0xF, 0xF),
};
static const struct regmap_access_table idio_24_wr_table = {
	.yes_ranges = idio_24_wr_ranges,
	.n_yes_ranges = ARRAY_SIZE(idio_24_wr_ranges),
};
static const struct regmap_access_table idio_24_rd_table = {
	.yes_ranges = idio_24_rd_ranges,
	.n_yes_ranges = ARRAY_SIZE(idio_24_rd_ranges),
};
static const struct regmap_access_table idio_24_volatile_table = {
	.yes_ranges = idio_24_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(idio_24_volatile_ranges),
};

static const struct regmap_config idio_24_regmap_config = {
	.reg_bits = 8,
	.reg_stride = 1,
	.val_bits = 8,
	.io_port = true,
	.wr_table = &idio_24_wr_table,
	.rd_table = &idio_24_rd_table,
	.volatile_table = &idio_24_volatile_table,
	.cache_type = REGCACHE_FLAT,
	.use_raw_spinlock = true,
};

#define IDIO_24_NGPIO_PER_REG 8
#define IDIO_24_REGMAP_IRQ(_id)						\
	[24 + _id] = {							\
		.reg_offset = (_id) / IDIO_24_NGPIO_PER_REG,		\
		.mask = BIT((_id) % IDIO_24_NGPIO_PER_REG),		\
		.type = { .types_supported = IRQ_TYPE_EDGE_BOTH },	\
	}
#define IDIO_24_IIN_IRQ(_id) IDIO_24_REGMAP_IRQ(_id)
#define IDIO_24_TTL_IRQ(_id) IDIO_24_REGMAP_IRQ(24 + _id)

static const struct regmap_irq idio_24_regmap_irqs[] = {
	IDIO_24_IIN_IRQ(0), IDIO_24_IIN_IRQ(1), IDIO_24_IIN_IRQ(2),  
	IDIO_24_IIN_IRQ(3), IDIO_24_IIN_IRQ(4), IDIO_24_IIN_IRQ(5),  
	IDIO_24_IIN_IRQ(6), IDIO_24_IIN_IRQ(7), IDIO_24_IIN_IRQ(8),  
	IDIO_24_IIN_IRQ(9), IDIO_24_IIN_IRQ(10), IDIO_24_IIN_IRQ(11),  
	IDIO_24_IIN_IRQ(12), IDIO_24_IIN_IRQ(13), IDIO_24_IIN_IRQ(14),  
	IDIO_24_IIN_IRQ(15), IDIO_24_IIN_IRQ(16), IDIO_24_IIN_IRQ(17),  
	IDIO_24_IIN_IRQ(18), IDIO_24_IIN_IRQ(19), IDIO_24_IIN_IRQ(20),  
	IDIO_24_IIN_IRQ(21), IDIO_24_IIN_IRQ(22), IDIO_24_IIN_IRQ(23),  
	IDIO_24_TTL_IRQ(0), IDIO_24_TTL_IRQ(1), IDIO_24_TTL_IRQ(2),  
	IDIO_24_TTL_IRQ(3), IDIO_24_TTL_IRQ(4), IDIO_24_TTL_IRQ(5),  
	IDIO_24_TTL_IRQ(6), IDIO_24_TTL_IRQ(7),  
};

 
struct idio_24_gpio {
	struct regmap *map;
	raw_spinlock_t lock;
	u8 irq_type;
};

static int idio_24_handle_mask_sync(const int index, const unsigned int mask_buf_def,
				    const unsigned int mask_buf, void *const irq_drv_data)
{
	const unsigned int type_mask = COS_ENABLE_BOTH << index;
	struct idio_24_gpio *const idio24gpio = irq_drv_data;
	u8 type;
	int ret;

	raw_spin_lock(&idio24gpio->lock);

	 
	type = (mask_buf == mask_buf_def) ? ~type_mask : idio24gpio->irq_type;

	ret = regmap_update_bits(idio24gpio->map, IDIO_24_COS_ENABLE, type_mask, type);

	raw_spin_unlock(&idio24gpio->lock);

	return ret;
}

static int idio_24_set_type_config(unsigned int **const buf, const unsigned int type,
				   const struct regmap_irq *const irq_data, const int idx,
				   void *const irq_drv_data)
{
	const unsigned int offset = irq_data->reg_offset;
	const unsigned int rising = COS_ENABLE_RISING << offset;
	const unsigned int falling = COS_ENABLE_FALLING << offset;
	const unsigned int mask = COS_ENABLE_BOTH << offset;
	struct idio_24_gpio *const idio24gpio = irq_drv_data;
	unsigned int new;
	unsigned int cos_enable;
	int ret;

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		new = rising;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		new = falling;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		new = mask;
		break;
	default:
		return -EINVAL;
	}

	raw_spin_lock(&idio24gpio->lock);

	 
	idio24gpio->irq_type = (idio24gpio->irq_type & ~mask) | (new & mask);

	ret = regmap_read(idio24gpio->map, IDIO_24_COS_ENABLE, &cos_enable);
	if (ret)
		goto exit_unlock;

	 
	if (cos_enable & mask) {
		ret = regmap_update_bits(idio24gpio->map, IDIO_24_COS_ENABLE, mask,
					 idio24gpio->irq_type);
		if (ret)
			goto exit_unlock;
	}

exit_unlock:
	raw_spin_unlock(&idio24gpio->lock);

	return ret;
}

static int idio_24_reg_mask_xlate(struct gpio_regmap *const gpio, const unsigned int base,
				  const unsigned int offset, unsigned int *const reg,
				  unsigned int *const mask)
{
	const unsigned int out_stride = offset / IDIO_24_NGPIO_PER_REG;
	const unsigned int in_stride = (offset - 24) / IDIO_24_NGPIO_PER_REG;
	struct regmap *const map = gpio_regmap_get_drvdata(gpio);
	int err;
	unsigned int ctrl_reg;

	switch (base) {
	case IDIO_24_OUT_BASE:
		*mask = BIT(offset % IDIO_24_NGPIO_PER_REG);

		 
		if (offset < 24) {
			*reg = IDIO_24_OUT_BASE + out_stride;
			return 0;
		}

		 
		if (offset < 48) {
			*reg = IDIO_24_IN_BASE + in_stride;
			return 0;
		}

		err = regmap_read(map, IDIO_24_CONTROL_REG, &ctrl_reg);
		if (err)
			return err;

		 
		if (ctrl_reg & CONTROL_REG_OUT_MODE) {
			*reg = IDIO_24_TTLCMOS_OUT_REG;
			return 0;
		}

		 
		*reg = IDIO_24_TTLCMOS_IN_REG;
		return 0;
	case IDIO_24_CONTROL_REG:
		 
		if (offset < 48)
			return -EOPNOTSUPP;

		*reg = IDIO_24_CONTROL_REG;
		*mask = CONTROL_REG_OUT_MODE;
		return 0;
	default:
		 
		return -EINVAL;
	}
}

#define IDIO_24_NGPIO 56
static const char *idio_24_names[IDIO_24_NGPIO] = {
	"OUT0", "OUT1", "OUT2", "OUT3", "OUT4", "OUT5", "OUT6", "OUT7",
	"OUT8", "OUT9", "OUT10", "OUT11", "OUT12", "OUT13", "OUT14", "OUT15",
	"OUT16", "OUT17", "OUT18", "OUT19", "OUT20", "OUT21", "OUT22", "OUT23",
	"IIN0", "IIN1", "IIN2", "IIN3", "IIN4", "IIN5", "IIN6", "IIN7",
	"IIN8", "IIN9", "IIN10", "IIN11", "IIN12", "IIN13", "IIN14", "IIN15",
	"IIN16", "IIN17", "IIN18", "IIN19", "IIN20", "IIN21", "IIN22", "IIN23",
	"TTL0", "TTL1", "TTL2", "TTL3", "TTL4", "TTL5", "TTL6", "TTL7"
};

static int idio_24_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct device *const dev = &pdev->dev;
	struct idio_24_gpio *idio24gpio;
	int err;
	const size_t pci_plx_bar_index = 1;
	const size_t pci_bar_index = 2;
	const char *const name = pci_name(pdev);
	struct gpio_regmap_config gpio_config = {};
	void __iomem *pex8311_regs;
	void __iomem *idio_24_regs;
	struct regmap *intcsr_map;
	struct regmap_irq_chip *chip;
	struct regmap_irq_chip_data *chip_data;

	err = pcim_enable_device(pdev);
	if (err) {
		dev_err(dev, "Failed to enable PCI device (%d)\n", err);
		return err;
	}

	err = pcim_iomap_regions(pdev, BIT(pci_plx_bar_index) | BIT(pci_bar_index), name);
	if (err) {
		dev_err(dev, "Unable to map PCI I/O addresses (%d)\n", err);
		return err;
	}

	pex8311_regs = pcim_iomap_table(pdev)[pci_plx_bar_index];
	idio_24_regs = pcim_iomap_table(pdev)[pci_bar_index];

	intcsr_map = devm_regmap_init_mmio(dev, pex8311_regs, &pex8311_intcsr_regmap_config);
	if (IS_ERR(intcsr_map))
		return dev_err_probe(dev, PTR_ERR(intcsr_map),
				     "Unable to initialize PEX8311 register map\n");

	idio24gpio = devm_kzalloc(dev, sizeof(*idio24gpio), GFP_KERNEL);
	if (!idio24gpio)
		return -ENOMEM;

	idio24gpio->map = devm_regmap_init_mmio(dev, idio_24_regs, &idio_24_regmap_config);
	if (IS_ERR(idio24gpio->map))
		return dev_err_probe(dev, PTR_ERR(idio24gpio->map),
				     "Unable to initialize register map\n");

	raw_spin_lock_init(&idio24gpio->lock);

	 
	idio24gpio->irq_type = GENMASK(7, 0);

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->name = name;
	chip->status_base = IDIO_24_COS_STATUS_BASE;
	chip->mask_base = IDIO_24_COS_ENABLE;
	chip->ack_base = IDIO_24_COS_STATUS_BASE;
	chip->num_regs = 4;
	chip->irqs = idio_24_regmap_irqs;
	chip->num_irqs = ARRAY_SIZE(idio_24_regmap_irqs);
	chip->handle_mask_sync = idio_24_handle_mask_sync;
	chip->set_type_config = idio_24_set_type_config;
	chip->irq_drv_data = idio24gpio;

	 
	err = regmap_write(idio24gpio->map, IDIO_24_SOFT_RESET, 0);
	if (err)
		return err;
	 
	err = regmap_update_bits(intcsr_map, 0x0, IDIO_24_ENABLE_IRQ, IDIO_24_ENABLE_IRQ);
	if (err)
		return err;

	err = devm_regmap_add_irq_chip(dev, idio24gpio->map, pdev->irq, 0, 0, chip, &chip_data);
	if (err)
		return dev_err_probe(dev, err, "IRQ registration failed\n");

	gpio_config.parent = dev;
	gpio_config.regmap = idio24gpio->map;
	gpio_config.ngpio = IDIO_24_NGPIO;
	gpio_config.names = idio_24_names;
	gpio_config.reg_dat_base = GPIO_REGMAP_ADDR(IDIO_24_OUT_BASE);
	gpio_config.reg_set_base = GPIO_REGMAP_ADDR(IDIO_24_OUT_BASE);
	gpio_config.reg_dir_out_base = GPIO_REGMAP_ADDR(IDIO_24_CONTROL_REG);
	gpio_config.ngpio_per_reg = IDIO_24_NGPIO_PER_REG;
	gpio_config.irq_domain = regmap_irq_get_domain(chip_data);
	gpio_config.reg_mask_xlate = idio_24_reg_mask_xlate;
	gpio_config.drvdata = idio24gpio->map;

	return PTR_ERR_OR_ZERO(devm_gpio_regmap_register(dev, &gpio_config));
}

static const struct pci_device_id idio_24_pci_dev_id[] = {
	{ PCI_DEVICE(0x494F, 0x0FD0) }, { PCI_DEVICE(0x494F, 0x0BD0) },
	{ PCI_DEVICE(0x494F, 0x07D0) }, { PCI_DEVICE(0x494F, 0x0FC0) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, idio_24_pci_dev_id);

static struct pci_driver idio_24_driver = {
	.name = "pcie-idio-24",
	.id_table = idio_24_pci_dev_id,
	.probe = idio_24_probe
};

module_pci_driver(idio_24_driver);

MODULE_AUTHOR("William Breathitt Gray <vilhelm.gray@gmail.com>");
MODULE_DESCRIPTION("ACCES PCIe-IDIO-24 GPIO driver");
MODULE_LICENSE("GPL v2");
