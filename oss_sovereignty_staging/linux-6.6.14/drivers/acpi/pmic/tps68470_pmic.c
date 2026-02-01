
 

#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/mfd/tps68470.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

struct tps68470_pmic_table {
	u32 address;		 
	u32 reg;		 
	u32 bitmask;		 
};

#define TI_PMIC_POWER_OPREGION_ID		0xB0
#define TI_PMIC_VR_VAL_OPREGION_ID		0xB1
#define TI_PMIC_CLOCK_OPREGION_ID		0xB2
#define TI_PMIC_CLKFREQ_OPREGION_ID		0xB3

struct tps68470_pmic_opregion {
	struct mutex lock;
	struct regmap *regmap;
};

#define S_IO_I2C_EN	(BIT(0) | BIT(1))

static const struct tps68470_pmic_table power_table[] = {
	{
		.address = 0x00,
		.reg = TPS68470_REG_S_I2C_CTL,
		.bitmask = S_IO_I2C_EN,
		 
	},
	{
		.address = 0x04,
		.reg = TPS68470_REG_VCMCTL,
		.bitmask = BIT(0),
		 
	},
	{
		.address = 0x08,
		.reg = TPS68470_REG_VAUX1CTL,
		.bitmask = BIT(0),
		 
	},
	{
		.address = 0x0C,
		.reg = TPS68470_REG_VAUX2CTL,
		.bitmask = BIT(0),
		 
	},
	{
		.address = 0x10,
		.reg = TPS68470_REG_VACTL,
		.bitmask = BIT(0),
		 
	},
	{
		.address = 0x14,
		.reg = TPS68470_REG_VDCTL,
		.bitmask = BIT(0),
		 
	},
};

 
static const struct tps68470_pmic_table vr_val_table[] = {
	{
		.address = 0x00,
		.reg = TPS68470_REG_VSIOVAL,
		.bitmask = TPS68470_VSIOVAL_IOVOLT_MASK,
		 
	},
	{
		.address = 0x04,
		.reg = TPS68470_REG_VIOVAL,
		.bitmask = TPS68470_VIOVAL_IOVOLT_MASK,
		 
	},
	{
		.address = 0x08,
		.reg = TPS68470_REG_VCMVAL,
		.bitmask = TPS68470_VCMVAL_VCVOLT_MASK,
		 
	},
	{
		.address = 0x0C,
		.reg = TPS68470_REG_VAUX1VAL,
		.bitmask = TPS68470_VAUX1VAL_AUX1VOLT_MASK,
		 
	},
	{
		.address = 0x10,
		.reg = TPS68470_REG_VAUX2VAL,
		.bitmask = TPS68470_VAUX2VAL_AUX2VOLT_MASK,
		 
	},
	{
		.address = 0x14,
		.reg = TPS68470_REG_VAVAL,
		.bitmask = TPS68470_VAVAL_AVOLT_MASK,
		 
	},
	{
		.address = 0x18,
		.reg = TPS68470_REG_VDVAL,
		.bitmask = TPS68470_VDVAL_DVOLT_MASK,
		 
	},
};

 
static const struct tps68470_pmic_table clk_freq_table[] = {
	{
		.address = 0x00,
		.reg = TPS68470_REG_POSTDIV2,
		.bitmask = BIT(0) | BIT(1),
		 
	},
	{
		.address = 0x04,
		.reg = TPS68470_REG_BOOSTDIV,
		.bitmask = 0x1F,
		 
	},
	{
		.address = 0x08,
		.reg = TPS68470_REG_BUCKDIV,
		.bitmask = 0x0F,
		 
	},
	{
		.address = 0x0C,
		.reg = TPS68470_REG_PLLSWR,
		.bitmask = 0x13,
		 
	},
	{
		.address = 0x10,
		.reg = TPS68470_REG_XTALDIV,
		.bitmask = 0xFF,
		 
	},
	{
		.address = 0x14,
		.reg = TPS68470_REG_PLLDIV,
		.bitmask = 0xFF,
		 
	},
	{
		.address = 0x18,
		.reg = TPS68470_REG_POSTDIV,
		.bitmask = 0x83,
		 
	},
};

 
static const struct tps68470_pmic_table clk_table[] = {
	{
		.address = 0x00,
		.reg = TPS68470_REG_PLLCTL,
		.bitmask = 0xF5,
		 
	},
	{
		.address = 0x04,
		.reg = TPS68470_REG_PLLCTL2,
		.bitmask = BIT(0),
		 
	},
	{
		.address = 0x08,
		.reg = TPS68470_REG_CLKCFG1,
		.bitmask = TPS68470_CLKCFG1_MODE_A_MASK |
			TPS68470_CLKCFG1_MODE_B_MASK,
		 
	},
	{
		.address = 0x0C,
		.reg = TPS68470_REG_CLKCFG2,
		.bitmask = TPS68470_CLKCFG1_MODE_A_MASK |
			TPS68470_CLKCFG1_MODE_B_MASK,
		 
	},
};

static int pmic_get_reg_bit(u64 address,
			    const struct tps68470_pmic_table *table,
			    const unsigned int table_size, int *reg,
			    int *bitmask)
{
	u64 i;

	i = address / 4;
	if (i >= table_size)
		return -ENOENT;

	if (!reg || !bitmask)
		return -EINVAL;

	*reg = table[i].reg;
	*bitmask = table[i].bitmask;

	return 0;
}

static int tps68470_pmic_get_power(struct regmap *regmap, int reg,
				       int bitmask, u64 *value)
{
	unsigned int data;

	if (regmap_read(regmap, reg, &data))
		return -EIO;

	*value = (data & bitmask) ? 1 : 0;
	return 0;
}

static int tps68470_pmic_get_vr_val(struct regmap *regmap, int reg,
				       int bitmask, u64 *value)
{
	unsigned int data;

	if (regmap_read(regmap, reg, &data))
		return -EIO;

	*value = data & bitmask;
	return 0;
}

static int tps68470_pmic_get_clk(struct regmap *regmap, int reg,
				       int bitmask, u64 *value)
{
	unsigned int data;

	if (regmap_read(regmap, reg, &data))
		return -EIO;

	*value = (data & bitmask) ? 1 : 0;
	return 0;
}

static int tps68470_pmic_get_clk_freq(struct regmap *regmap, int reg,
				       int bitmask, u64 *value)
{
	unsigned int data;

	if (regmap_read(regmap, reg, &data))
		return -EIO;

	*value = data & bitmask;
	return 0;
}

static int ti_tps68470_regmap_update_bits(struct regmap *regmap, int reg,
					int bitmask, u64 value)
{
	return regmap_update_bits(regmap, reg, bitmask, value);
}

static acpi_status tps68470_pmic_common_handler(u32 function,
					  acpi_physical_address address,
					  u32 bits, u64 *value,
					  void *region_context,
					  int (*get)(struct regmap *,
						     int, int, u64 *),
					  int (*update)(struct regmap *,
							int, int, u64),
					  const struct tps68470_pmic_table *tbl,
					  unsigned int tbl_size)
{
	struct tps68470_pmic_opregion *opregion = region_context;
	struct regmap *regmap = opregion->regmap;
	int reg, ret, bitmask;

	if (bits != 32)
		return AE_BAD_PARAMETER;

	ret = pmic_get_reg_bit(address, tbl, tbl_size, &reg, &bitmask);
	if (ret < 0)
		return AE_BAD_PARAMETER;

	if (function == ACPI_WRITE && *value > bitmask)
		return AE_BAD_PARAMETER;

	mutex_lock(&opregion->lock);

	ret = (function == ACPI_READ) ?
		get(regmap, reg, bitmask, value) :
		update(regmap, reg, bitmask, *value);

	mutex_unlock(&opregion->lock);

	return ret ? AE_ERROR : AE_OK;
}

static acpi_status tps68470_pmic_cfreq_handler(u32 function,
					    acpi_physical_address address,
					    u32 bits, u64 *value,
					    void *handler_context,
					    void *region_context)
{
	return tps68470_pmic_common_handler(function, address, bits, value,
				region_context,
				tps68470_pmic_get_clk_freq,
				ti_tps68470_regmap_update_bits,
				clk_freq_table,
				ARRAY_SIZE(clk_freq_table));
}

static acpi_status tps68470_pmic_clk_handler(u32 function,
				       acpi_physical_address address, u32 bits,
				       u64 *value, void *handler_context,
				       void *region_context)
{
	return tps68470_pmic_common_handler(function, address, bits, value,
				region_context,
				tps68470_pmic_get_clk,
				ti_tps68470_regmap_update_bits,
				clk_table,
				ARRAY_SIZE(clk_table));
}

static acpi_status tps68470_pmic_vrval_handler(u32 function,
					  acpi_physical_address address,
					  u32 bits, u64 *value,
					  void *handler_context,
					  void *region_context)
{
	return tps68470_pmic_common_handler(function, address, bits, value,
				region_context,
				tps68470_pmic_get_vr_val,
				ti_tps68470_regmap_update_bits,
				vr_val_table,
				ARRAY_SIZE(vr_val_table));
}

static acpi_status tps68470_pmic_pwr_handler(u32 function,
					 acpi_physical_address address,
					 u32 bits, u64 *value,
					 void *handler_context,
					 void *region_context)
{
	if (bits != 32)
		return AE_BAD_PARAMETER;

	 
	if (function == ACPI_WRITE &&
	    !(*value == 0 || *value == 1 || *value == 3)) {
		return AE_BAD_PARAMETER;
	}

	return tps68470_pmic_common_handler(function, address, bits, value,
				region_context,
				tps68470_pmic_get_power,
				ti_tps68470_regmap_update_bits,
				power_table,
				ARRAY_SIZE(power_table));
}

static int tps68470_pmic_opregion_probe(struct platform_device *pdev)
{
	struct regmap *tps68470_regmap = dev_get_drvdata(pdev->dev.parent);
	acpi_handle handle = ACPI_HANDLE(pdev->dev.parent);
	struct device *dev = &pdev->dev;
	struct tps68470_pmic_opregion *opregion;
	acpi_status status;

	if (!dev || !tps68470_regmap) {
		dev_warn(dev, "dev or regmap is NULL\n");
		return -EINVAL;
	}

	if (!handle) {
		dev_warn(dev, "acpi handle is NULL\n");
		return -ENODEV;
	}

	opregion = devm_kzalloc(dev, sizeof(*opregion), GFP_KERNEL);
	if (!opregion)
		return -ENOMEM;

	mutex_init(&opregion->lock);
	opregion->regmap = tps68470_regmap;

	status = acpi_install_address_space_handler(handle,
						    TI_PMIC_POWER_OPREGION_ID,
						    tps68470_pmic_pwr_handler,
						    NULL, opregion);
	if (ACPI_FAILURE(status))
		goto out_mutex_destroy;

	status = acpi_install_address_space_handler(handle,
						    TI_PMIC_VR_VAL_OPREGION_ID,
						    tps68470_pmic_vrval_handler,
						    NULL, opregion);
	if (ACPI_FAILURE(status))
		goto out_remove_power_handler;

	status = acpi_install_address_space_handler(handle,
						    TI_PMIC_CLOCK_OPREGION_ID,
						    tps68470_pmic_clk_handler,
						    NULL, opregion);
	if (ACPI_FAILURE(status))
		goto out_remove_vr_val_handler;

	status = acpi_install_address_space_handler(handle,
						    TI_PMIC_CLKFREQ_OPREGION_ID,
						    tps68470_pmic_cfreq_handler,
						    NULL, opregion);
	if (ACPI_FAILURE(status))
		goto out_remove_clk_handler;

	return 0;

out_remove_clk_handler:
	acpi_remove_address_space_handler(handle, TI_PMIC_CLOCK_OPREGION_ID,
					  tps68470_pmic_clk_handler);
out_remove_vr_val_handler:
	acpi_remove_address_space_handler(handle, TI_PMIC_VR_VAL_OPREGION_ID,
					  tps68470_pmic_vrval_handler);
out_remove_power_handler:
	acpi_remove_address_space_handler(handle, TI_PMIC_POWER_OPREGION_ID,
					  tps68470_pmic_pwr_handler);
out_mutex_destroy:
	mutex_destroy(&opregion->lock);
	return -ENODEV;
}

static struct platform_driver tps68470_pmic_opregion_driver = {
	.probe = tps68470_pmic_opregion_probe,
	.driver = {
		.name = "tps68470_pmic_opregion",
	},
};

builtin_platform_driver(tps68470_pmic_opregion_driver)
