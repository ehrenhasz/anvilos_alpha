
 

#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/twl.h>
#include <linux/delay.h>

 

struct twlreg_info {
	 
	u8			base;

	 
	u8			id;

	 
	u8			table_len;
	const u16		*table;

	 
	u8			remap;

	 
	struct regulator_desc	desc;

	 
	unsigned long		features;

	 
	void			*data;
};


 
 
#define VREG_GRP		0
 
#define VREG_TYPE		1
#define VREG_REMAP		2
#define VREG_DEDICATED		3	 
#define VREG_VOLTAGE_SMPS_4030	9
 
#define VREG_TRANS		1
#define VREG_STATE		2
#define VREG_VOLTAGE		3
#define VREG_VOLTAGE_SMPS	4

static inline int
twlreg_read(struct twlreg_info *info, unsigned slave_subgp, unsigned offset)
{
	u8 value;
	int status;

	status = twl_i2c_read_u8(slave_subgp,
			&value, info->base + offset);
	return (status < 0) ? status : value;
}

static inline int
twlreg_write(struct twlreg_info *info, unsigned slave_subgp, unsigned offset,
						 u8 value)
{
	return twl_i2c_write_u8(slave_subgp,
			value, info->base + offset);
}

 

 

static int twlreg_grp(struct regulator_dev *rdev)
{
	return twlreg_read(rdev_get_drvdata(rdev), TWL_MODULE_PM_RECEIVER,
								 VREG_GRP);
}

 
 
#define P3_GRP_4030	BIT(7)		 
#define P2_GRP_4030	BIT(6)		 
#define P1_GRP_4030	BIT(5)		 
 
#define P3_GRP_6030	BIT(2)		 
#define P2_GRP_6030	BIT(1)		 
#define P1_GRP_6030	BIT(0)		 

static int twl4030reg_is_enabled(struct regulator_dev *rdev)
{
	int	state = twlreg_grp(rdev);

	if (state < 0)
		return state;

	return state & P1_GRP_4030;
}

#define PB_I2C_BUSY	BIT(0)
#define PB_I2C_BWEN	BIT(1)

 
static int twl4030_wait_pb_ready(void)
{

	int	ret;
	int	timeout = 10;
	u8	val;

	do {
		ret = twl_i2c_read_u8(TWL_MODULE_PM_MASTER, &val,
				      TWL4030_PM_MASTER_PB_CFG);
		if (ret < 0)
			return ret;

		if (!(val & PB_I2C_BUSY))
			return 0;

		mdelay(1);
		timeout--;
	} while (timeout);

	return -ETIMEDOUT;
}

 
static int twl4030_send_pb_msg(unsigned msg)
{
	u8	val;
	int	ret;

	 
	ret = twl_i2c_read_u8(TWL_MODULE_PM_MASTER, &val,
			      TWL4030_PM_MASTER_PB_CFG);
	if (ret < 0)
		return ret;

	 
	ret = twl_i2c_write_u8(TWL_MODULE_PM_MASTER, val | PB_I2C_BWEN,
			       TWL4030_PM_MASTER_PB_CFG);
	if (ret < 0)
		return ret;

	ret = twl4030_wait_pb_ready();
	if (ret < 0)
		return ret;

	ret = twl_i2c_write_u8(TWL_MODULE_PM_MASTER, msg >> 8,
			       TWL4030_PM_MASTER_PB_WORD_MSB);
	if (ret < 0)
		return ret;

	ret = twl_i2c_write_u8(TWL_MODULE_PM_MASTER, msg & 0xff,
			       TWL4030_PM_MASTER_PB_WORD_LSB);
	if (ret < 0)
		return ret;

	ret = twl4030_wait_pb_ready();
	if (ret < 0)
		return ret;

	 
	return twl_i2c_write_u8(TWL_MODULE_PM_MASTER, val,
				TWL4030_PM_MASTER_PB_CFG);
}

static int twl4030reg_enable(struct regulator_dev *rdev)
{
	struct twlreg_info	*info = rdev_get_drvdata(rdev);
	int			grp;

	grp = twlreg_grp(rdev);
	if (grp < 0)
		return grp;

	grp |= P1_GRP_4030;

	return twlreg_write(info, TWL_MODULE_PM_RECEIVER, VREG_GRP, grp);
}

static int twl4030reg_disable(struct regulator_dev *rdev)
{
	struct twlreg_info	*info = rdev_get_drvdata(rdev);
	int			grp;

	grp = twlreg_grp(rdev);
	if (grp < 0)
		return grp;

	grp &= ~(P1_GRP_4030 | P2_GRP_4030 | P3_GRP_4030);

	return twlreg_write(info, TWL_MODULE_PM_RECEIVER, VREG_GRP, grp);
}

static int twl4030reg_get_status(struct regulator_dev *rdev)
{
	int	state = twlreg_grp(rdev);

	if (state < 0)
		return state;
	state &= 0x0f;

	 
	if (!state)
		return REGULATOR_STATUS_OFF;
	return (state & BIT(3))
		? REGULATOR_STATUS_NORMAL
		: REGULATOR_STATUS_STANDBY;
}

static int twl4030reg_set_mode(struct regulator_dev *rdev, unsigned mode)
{
	struct twlreg_info	*info = rdev_get_drvdata(rdev);
	unsigned		message;

	 
	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		message = MSG_SINGULAR(DEV_GRP_P1, info->id, RES_STATE_ACTIVE);
		break;
	case REGULATOR_MODE_STANDBY:
		message = MSG_SINGULAR(DEV_GRP_P1, info->id, RES_STATE_SLEEP);
		break;
	default:
		return -EINVAL;
	}

	return twl4030_send_pb_msg(message);
}

static inline unsigned int twl4030reg_map_mode(unsigned int mode)
{
	switch (mode) {
	case RES_STATE_ACTIVE:
		return REGULATOR_MODE_NORMAL;
	case RES_STATE_SLEEP:
		return REGULATOR_MODE_STANDBY;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

 

 
#define UNSUP_MASK	0x8000

#define UNSUP(x)	(UNSUP_MASK | (x))
#define IS_UNSUP(info, x)			\
	((UNSUP_MASK & (x)) &&			\
	 !((info)->features & TWL4030_ALLOW_UNSUPPORTED))
#define LDO_MV(x)	(~UNSUP_MASK & (x))


static const u16 VAUX1_VSEL_table[] = {
	UNSUP(1500), UNSUP(1800), 2500, 2800,
	3000, 3000, 3000, 3000,
};
static const u16 VAUX2_4030_VSEL_table[] = {
	UNSUP(1000), UNSUP(1000), UNSUP(1200), 1300,
	1500, 1800, UNSUP(1850), 2500,
	UNSUP(2600), 2800, UNSUP(2850), UNSUP(3000),
	UNSUP(3150), UNSUP(3150), UNSUP(3150), UNSUP(3150),
};
static const u16 VAUX2_VSEL_table[] = {
	1700, 1700, 1900, 1300,
	1500, 1800, 2000, 2500,
	2100, 2800, 2200, 2300,
	2400, 2400, 2400, 2400,
};
static const u16 VAUX3_VSEL_table[] = {
	1500, 1800, 2500, 2800,
	3000, 3000, 3000, 3000,
};
static const u16 VAUX4_VSEL_table[] = {
	700, 1000, 1200, UNSUP(1300),
	1500, 1800, UNSUP(1850), 2500,
	UNSUP(2600), 2800, UNSUP(2850), UNSUP(3000),
	UNSUP(3150), UNSUP(3150), UNSUP(3150), UNSUP(3150),
};
static const u16 VMMC1_VSEL_table[] = {
	1850, 2850, 3000, 3150,
};
static const u16 VMMC2_VSEL_table[] = {
	UNSUP(1000), UNSUP(1000), UNSUP(1200), UNSUP(1300),
	UNSUP(1500), UNSUP(1800), 1850, UNSUP(2500),
	2600, 2800, 2850, 3000,
	3150, 3150, 3150, 3150,
};
static const u16 VPLL1_VSEL_table[] = {
	1000, 1200, 1300, 1800,
	UNSUP(2800), UNSUP(3000), UNSUP(3000), UNSUP(3000),
};
static const u16 VPLL2_VSEL_table[] = {
	700, 1000, 1200, 1300,
	UNSUP(1500), 1800, UNSUP(1850), UNSUP(2500),
	UNSUP(2600), UNSUP(2800), UNSUP(2850), UNSUP(3000),
	UNSUP(3150), UNSUP(3150), UNSUP(3150), UNSUP(3150),
};
static const u16 VSIM_VSEL_table[] = {
	UNSUP(1000), UNSUP(1200), UNSUP(1300), 1800,
	2800, 3000, 3000, 3000,
};
static const u16 VDAC_VSEL_table[] = {
	1200, 1300, 1800, 1800,
};
static const u16 VIO_VSEL_table[] = {
	1800, 1850,
};
static const u16 VINTANA2_VSEL_table[] = {
	2500, 2750,
};

 
static const struct linear_range VDD1_ranges[] = {
	REGULATOR_LINEAR_RANGE(600000, 0, 68, 12500)
};

 
static const struct linear_range VDD2_ranges[] = {
	REGULATOR_LINEAR_RANGE(600000, 0, 68, 12500),
	REGULATOR_LINEAR_RANGE(1500000, 69, 69, 12500)
};

static int twl4030ldo_list_voltage(struct regulator_dev *rdev, unsigned index)
{
	struct twlreg_info	*info = rdev_get_drvdata(rdev);
	int			mV = info->table[index];

	return IS_UNSUP(info, mV) ? 0 : (LDO_MV(mV) * 1000);
}

static int
twl4030ldo_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	struct twlreg_info	*info = rdev_get_drvdata(rdev);

	return twlreg_write(info, TWL_MODULE_PM_RECEIVER, VREG_VOLTAGE,
			    selector);
}

static int twl4030ldo_get_voltage_sel(struct regulator_dev *rdev)
{
	struct twlreg_info	*info = rdev_get_drvdata(rdev);
	int vsel = twlreg_read(info, TWL_MODULE_PM_RECEIVER, VREG_VOLTAGE);

	if (vsel < 0)
		return vsel;

	vsel &= info->table_len - 1;
	return vsel;
}

static const struct regulator_ops twl4030ldo_ops = {
	.list_voltage	= twl4030ldo_list_voltage,

	.set_voltage_sel = twl4030ldo_set_voltage_sel,
	.get_voltage_sel = twl4030ldo_get_voltage_sel,

	.enable		= twl4030reg_enable,
	.disable	= twl4030reg_disable,
	.is_enabled	= twl4030reg_is_enabled,

	.set_mode	= twl4030reg_set_mode,

	.get_status	= twl4030reg_get_status,
};

static int
twl4030smps_set_voltage(struct regulator_dev *rdev, int min_uV, int max_uV,
			unsigned *selector)
{
	struct twlreg_info *info = rdev_get_drvdata(rdev);
	int vsel = DIV_ROUND_UP(min_uV - 600000, 12500);

	twlreg_write(info, TWL_MODULE_PM_RECEIVER, VREG_VOLTAGE_SMPS_4030, vsel);

	return 0;
}

static int twl4030smps_get_voltage(struct regulator_dev *rdev)
{
	struct twlreg_info *info = rdev_get_drvdata(rdev);
	int vsel;

	vsel = twlreg_read(info, TWL_MODULE_PM_RECEIVER,
		VREG_VOLTAGE_SMPS_4030);

	return vsel * 12500 + 600000;
}

static const struct regulator_ops twl4030smps_ops = {
	.list_voltage   = regulator_list_voltage_linear_range,

	.set_voltage	= twl4030smps_set_voltage,
	.get_voltage	= twl4030smps_get_voltage,
};

 

static const struct regulator_ops twl4030fixed_ops = {
	.list_voltage	= regulator_list_voltage_linear,

	.enable		= twl4030reg_enable,
	.disable	= twl4030reg_disable,
	.is_enabled	= twl4030reg_is_enabled,

	.set_mode	= twl4030reg_set_mode,

	.get_status	= twl4030reg_get_status,
};

 

#define TWL4030_ADJUSTABLE_LDO(label, offset, num, turnon_delay, remap_conf) \
static const struct twlreg_info TWL4030_INFO_##label = { \
	.base = offset, \
	.id = num, \
	.table_len = ARRAY_SIZE(label##_VSEL_table), \
	.table = label##_VSEL_table, \
	.remap = remap_conf, \
	.desc = { \
		.name = #label, \
		.id = TWL4030_REG_##label, \
		.n_voltages = ARRAY_SIZE(label##_VSEL_table), \
		.ops = &twl4030ldo_ops, \
		.type = REGULATOR_VOLTAGE, \
		.owner = THIS_MODULE, \
		.enable_time = turnon_delay, \
		.of_map_mode = twl4030reg_map_mode, \
		}, \
	}

#define TWL4030_ADJUSTABLE_SMPS(label, offset, num, turnon_delay, remap_conf, \
		n_volt) \
static const struct twlreg_info TWL4030_INFO_##label = { \
	.base = offset, \
	.id = num, \
	.remap = remap_conf, \
	.desc = { \
		.name = #label, \
		.id = TWL4030_REG_##label, \
		.ops = &twl4030smps_ops, \
		.type = REGULATOR_VOLTAGE, \
		.owner = THIS_MODULE, \
		.enable_time = turnon_delay, \
		.of_map_mode = twl4030reg_map_mode, \
		.n_voltages = n_volt, \
		.n_linear_ranges = ARRAY_SIZE(label ## _ranges), \
		.linear_ranges = label ## _ranges, \
		}, \
	}

#define TWL4030_FIXED_LDO(label, offset, mVolts, num, turnon_delay, \
			remap_conf) \
static const struct twlreg_info TWLFIXED_INFO_##label = { \
	.base = offset, \
	.id = num, \
	.remap = remap_conf, \
	.desc = { \
		.name = #label, \
		.id = TWL4030##_REG_##label, \
		.n_voltages = 1, \
		.ops = &twl4030fixed_ops, \
		.type = REGULATOR_VOLTAGE, \
		.owner = THIS_MODULE, \
		.min_uV = mVolts * 1000, \
		.enable_time = turnon_delay, \
		.of_map_mode = twl4030reg_map_mode, \
		}, \
	}

 
TWL4030_ADJUSTABLE_LDO(VAUX1, 0x17, 1, 100, 0x08);
TWL4030_ADJUSTABLE_LDO(VAUX2_4030, 0x1b, 2, 100, 0x08);
TWL4030_ADJUSTABLE_LDO(VAUX2, 0x1b, 2, 100, 0x08);
TWL4030_ADJUSTABLE_LDO(VAUX3, 0x1f, 3, 100, 0x08);
TWL4030_ADJUSTABLE_LDO(VAUX4, 0x23, 4, 100, 0x08);
TWL4030_ADJUSTABLE_LDO(VMMC1, 0x27, 5, 100, 0x08);
TWL4030_ADJUSTABLE_LDO(VMMC2, 0x2b, 6, 100, 0x08);
TWL4030_ADJUSTABLE_LDO(VPLL1, 0x2f, 7, 100, 0x00);
TWL4030_ADJUSTABLE_LDO(VPLL2, 0x33, 8, 100, 0x08);
TWL4030_ADJUSTABLE_LDO(VSIM, 0x37, 9, 100, 0x00);
TWL4030_ADJUSTABLE_LDO(VDAC, 0x3b, 10, 100, 0x08);
TWL4030_ADJUSTABLE_LDO(VINTANA2, 0x43, 12, 100, 0x08);
TWL4030_ADJUSTABLE_LDO(VIO, 0x4b, 14, 1000, 0x08);
TWL4030_ADJUSTABLE_SMPS(VDD1, 0x55, 15, 1000, 0x08, 68);
TWL4030_ADJUSTABLE_SMPS(VDD2, 0x63, 16, 1000, 0x08, 69);
 
TWL4030_FIXED_LDO(VINTANA1, 0x3f, 1500, 11, 100, 0x08);
TWL4030_FIXED_LDO(VINTDIG, 0x47, 1500, 13, 100, 0x08);
TWL4030_FIXED_LDO(VUSB1V5, 0x71, 1500, 17, 100, 0x08);
TWL4030_FIXED_LDO(VUSB1V8, 0x74, 1800, 18, 100, 0x08);
TWL4030_FIXED_LDO(VUSB3V1, 0x77, 3100, 19, 150, 0x08);

#define TWL_OF_MATCH(comp, family, label) \
	{ \
		.compatible = comp, \
		.data = &family##_INFO_##label, \
	}

#define TWL4030_OF_MATCH(comp, label) TWL_OF_MATCH(comp, TWL4030, label)
#define TWL6030_OF_MATCH(comp, label) TWL_OF_MATCH(comp, TWL6030, label)
#define TWL6032_OF_MATCH(comp, label) TWL_OF_MATCH(comp, TWL6032, label)
#define TWLFIXED_OF_MATCH(comp, label) TWL_OF_MATCH(comp, TWLFIXED, label)
#define TWLSMPS_OF_MATCH(comp, label) TWL_OF_MATCH(comp, TWLSMPS, label)

static const struct of_device_id twl_of_match[] = {
	TWL4030_OF_MATCH("ti,twl4030-vaux1", VAUX1),
	TWL4030_OF_MATCH("ti,twl4030-vaux2", VAUX2_4030),
	TWL4030_OF_MATCH("ti,twl5030-vaux2", VAUX2),
	TWL4030_OF_MATCH("ti,twl4030-vaux3", VAUX3),
	TWL4030_OF_MATCH("ti,twl4030-vaux4", VAUX4),
	TWL4030_OF_MATCH("ti,twl4030-vmmc1", VMMC1),
	TWL4030_OF_MATCH("ti,twl4030-vmmc2", VMMC2),
	TWL4030_OF_MATCH("ti,twl4030-vpll1", VPLL1),
	TWL4030_OF_MATCH("ti,twl4030-vpll2", VPLL2),
	TWL4030_OF_MATCH("ti,twl4030-vsim", VSIM),
	TWL4030_OF_MATCH("ti,twl4030-vdac", VDAC),
	TWL4030_OF_MATCH("ti,twl4030-vintana2", VINTANA2),
	TWL4030_OF_MATCH("ti,twl4030-vio", VIO),
	TWL4030_OF_MATCH("ti,twl4030-vdd1", VDD1),
	TWL4030_OF_MATCH("ti,twl4030-vdd2", VDD2),
	TWLFIXED_OF_MATCH("ti,twl4030-vintana1", VINTANA1),
	TWLFIXED_OF_MATCH("ti,twl4030-vintdig", VINTDIG),
	TWLFIXED_OF_MATCH("ti,twl4030-vusb1v5", VUSB1V5),
	TWLFIXED_OF_MATCH("ti,twl4030-vusb1v8", VUSB1V8),
	TWLFIXED_OF_MATCH("ti,twl4030-vusb3v1", VUSB3V1),
	{},
};
MODULE_DEVICE_TABLE(of, twl_of_match);

static int twlreg_probe(struct platform_device *pdev)
{
	int id;
	struct twlreg_info		*info;
	const struct twlreg_info	*template;
	struct regulator_init_data	*initdata;
	struct regulation_constraints	*c;
	struct regulator_dev		*rdev;
	struct regulator_config		config = { };

	template = of_device_get_match_data(&pdev->dev);
	if (!template)
		return -ENODEV;

	id = template->desc.id;
	initdata = of_get_regulator_init_data(&pdev->dev, pdev->dev.of_node,
						&template->desc);
	if (!initdata)
		return -EINVAL;

	info = devm_kmemdup(&pdev->dev, template, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	 
	c = &initdata->constraints;
	c->valid_modes_mask &= REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY;
	c->valid_ops_mask &= REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_MODE
				| REGULATOR_CHANGE_STATUS;
	switch (id) {
	case TWL4030_REG_VIO:
	case TWL4030_REG_VDD1:
	case TWL4030_REG_VDD2:
	case TWL4030_REG_VPLL1:
	case TWL4030_REG_VINTANA1:
	case TWL4030_REG_VINTANA2:
	case TWL4030_REG_VINTDIG:
		c->always_on = true;
		break;
	default:
		break;
	}

	config.dev = &pdev->dev;
	config.init_data = initdata;
	config.driver_data = info;
	config.of_node = pdev->dev.of_node;

	rdev = devm_regulator_register(&pdev->dev, &info->desc, &config);
	if (IS_ERR(rdev)) {
		dev_err(&pdev->dev, "can't register %s, %ld\n",
				info->desc.name, PTR_ERR(rdev));
		return PTR_ERR(rdev);
	}
	platform_set_drvdata(pdev, rdev);

	twlreg_write(info, TWL_MODULE_PM_RECEIVER, VREG_REMAP, info->remap);

	 

	return 0;
}

MODULE_ALIAS("platform:twl4030_reg");

static struct platform_driver twlreg_driver = {
	.probe		= twlreg_probe,
	 
	.driver  = {
		.name  = "twl4030_reg",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = of_match_ptr(twl_of_match),
	},
};

static int __init twlreg_init(void)
{
	return platform_driver_register(&twlreg_driver);
}
subsys_initcall(twlreg_init);

static void __exit twlreg_exit(void)
{
	platform_driver_unregister(&twlreg_driver);
}
module_exit(twlreg_exit)

MODULE_DESCRIPTION("TWL4030 regulator driver");
MODULE_LICENSE("GPL");
