









#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/suspend.h>
#include <linux/gpio/consumer.h>

#define VDD_LOW_SEL 0x0D
#define VDD_HIGH_SEL 0x3F

#define MCP16502_FLT		BIT(7)
#define MCP16502_DVSR		GENMASK(3, 2)
#define MCP16502_ENS		BIT(0)

 

 
#define MCP16502_REG_BASE(i, r) ((((i) + 1) << 4) + MCP16502_REG_##r)
#define MCP16502_STAT_BASE(i) ((i) + 5)

#define MCP16502_OPMODE_ACTIVE REGULATOR_MODE_NORMAL
#define MCP16502_OPMODE_LPM REGULATOR_MODE_IDLE
#define MCP16502_OPMODE_HIB REGULATOR_MODE_STANDBY

#define MCP16502_MODE_AUTO_PFM 0
#define MCP16502_MODE_FPWM BIT(6)

#define MCP16502_VSEL 0x3F
#define MCP16502_EN BIT(7)
#define MCP16502_MODE BIT(6)

#define MCP16502_MIN_REG 0x0
#define MCP16502_MAX_REG 0x65

 
enum mcp16502_reg {
	MCP16502_REG_A,
	MCP16502_REG_LPM,
	MCP16502_REG_HIB,
	MCP16502_REG_HPM,
	MCP16502_REG_SEQ,
	MCP16502_REG_CFG,
};

 
static const unsigned int mcp16502_ramp_b1l12[] = {
	6250, 3125, 2083, 1563
};

 
static const unsigned int mcp16502_ramp_b234[] = {
	3125, 1563, 1042, 781
};

static unsigned int mcp16502_of_map_mode(unsigned int mode)
{
	if (mode == REGULATOR_MODE_NORMAL || mode == REGULATOR_MODE_IDLE)
		return mode;

	return REGULATOR_MODE_INVALID;
}

#define MCP16502_REGULATOR(_name, _id, _ranges, _ops, _ramp_table)	\
	[_id] = {							\
		.name			= _name,			\
		.regulators_node	= "regulators",			\
		.id			= _id,				\
		.ops			= &(_ops),			\
		.type			= REGULATOR_VOLTAGE,		\
		.owner			= THIS_MODULE,			\
		.n_voltages		= MCP16502_VSEL + 1,		\
		.linear_ranges		= _ranges,			\
		.linear_min_sel		= VDD_LOW_SEL,			\
		.n_linear_ranges	= ARRAY_SIZE(_ranges),		\
		.of_match		= _name,			\
		.of_map_mode		= mcp16502_of_map_mode,		\
		.vsel_reg		= (((_id) + 1) << 4),		\
		.vsel_mask		= MCP16502_VSEL,		\
		.enable_reg		= (((_id) + 1) << 4),		\
		.enable_mask		= MCP16502_EN,			\
		.ramp_reg		= MCP16502_REG_BASE(_id, CFG),	\
		.ramp_mask		= MCP16502_DVSR,		\
		.ramp_delay_table	= _ramp_table,			\
		.n_ramp_values		= ARRAY_SIZE(_ramp_table),	\
	}

enum {
	BUCK1 = 0,
	BUCK2,
	BUCK3,
	BUCK4,
	LDO1,
	LDO2,
	NUM_REGULATORS
};

 
struct mcp16502 {
	struct gpio_desc *lpm;
};

 
static void mcp16502_gpio_set_mode(struct mcp16502 *mcp, int mode)
{
	switch (mode) {
	case MCP16502_OPMODE_ACTIVE:
		gpiod_set_value(mcp->lpm, 0);
		break;
	case MCP16502_OPMODE_LPM:
	case MCP16502_OPMODE_HIB:
		gpiod_set_value(mcp->lpm, 1);
		break;
	default:
		pr_err("%s: %d invalid\n", __func__, mode);
	}
}

 
static int mcp16502_get_state_reg(struct regulator_dev *rdev, int opmode)
{
	switch (opmode) {
	case MCP16502_OPMODE_ACTIVE:
		return MCP16502_REG_BASE(rdev_get_id(rdev), A);
	case MCP16502_OPMODE_LPM:
		return MCP16502_REG_BASE(rdev_get_id(rdev), LPM);
	case MCP16502_OPMODE_HIB:
		return MCP16502_REG_BASE(rdev_get_id(rdev), HIB);
	default:
		return -EINVAL;
	}
}

 
static unsigned int mcp16502_get_mode(struct regulator_dev *rdev)
{
	unsigned int val;
	int ret, reg;

	reg = mcp16502_get_state_reg(rdev, MCP16502_OPMODE_ACTIVE);
	if (reg < 0)
		return reg;

	ret = regmap_read(rdev->regmap, reg, &val);
	if (ret)
		return ret;

	switch (val & MCP16502_MODE) {
	case MCP16502_MODE_FPWM:
		return REGULATOR_MODE_NORMAL;
	case MCP16502_MODE_AUTO_PFM:
		return REGULATOR_MODE_IDLE;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

 
static int _mcp16502_set_mode(struct regulator_dev *rdev, unsigned int mode,
			      unsigned int op_mode)
{
	int val;
	int reg;

	reg = mcp16502_get_state_reg(rdev, op_mode);
	if (reg < 0)
		return reg;

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		val = MCP16502_MODE_FPWM;
		break;
	case REGULATOR_MODE_IDLE:
		val = MCP16502_MODE_AUTO_PFM;
		break;
	default:
		return -EINVAL;
	}

	reg = regmap_update_bits(rdev->regmap, reg, MCP16502_MODE, val);
	return reg;
}

 
static int mcp16502_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	return _mcp16502_set_mode(rdev, mode, MCP16502_OPMODE_ACTIVE);
}

 
static int mcp16502_get_status(struct regulator_dev *rdev)
{
	int ret;
	unsigned int val;

	ret = regmap_read(rdev->regmap, MCP16502_STAT_BASE(rdev_get_id(rdev)),
			  &val);
	if (ret)
		return ret;

	if (val & MCP16502_FLT)
		return REGULATOR_STATUS_ERROR;
	else if (val & MCP16502_ENS)
		return REGULATOR_STATUS_ON;
	else if (!(val & MCP16502_ENS))
		return REGULATOR_STATUS_OFF;

	return REGULATOR_STATUS_UNDEFINED;
}

static int mcp16502_set_voltage_time_sel(struct regulator_dev *rdev,
					 unsigned int old_sel,
					 unsigned int new_sel)
{
	static const u8 us_ramp[] = { 8, 16, 24, 32 };
	int id = rdev_get_id(rdev);
	unsigned int uV_delta, val;
	int ret;

	ret = regmap_read(rdev->regmap, MCP16502_REG_BASE(id, CFG), &val);
	if (ret)
		return ret;

	val = (val & MCP16502_DVSR) >> 2;
	uV_delta = abs(new_sel * rdev->desc->linear_ranges->step -
		       old_sel * rdev->desc->linear_ranges->step);
	switch (id) {
	case BUCK1:
	case LDO1:
	case LDO2:
		ret = DIV_ROUND_CLOSEST(uV_delta * us_ramp[val],
					mcp16502_ramp_b1l12[val]);
		break;

	case BUCK2:
	case BUCK3:
	case BUCK4:
		ret = DIV_ROUND_CLOSEST(uV_delta * us_ramp[val],
					mcp16502_ramp_b234[val]);
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_SUSPEND
 
static int mcp16502_suspend_get_target_reg(struct regulator_dev *rdev)
{
	switch (pm_suspend_target_state) {
	case PM_SUSPEND_STANDBY:
		return mcp16502_get_state_reg(rdev, MCP16502_OPMODE_LPM);
	case PM_SUSPEND_ON:
	case PM_SUSPEND_MEM:
		return mcp16502_get_state_reg(rdev, MCP16502_OPMODE_HIB);
	default:
		dev_err(&rdev->dev, "invalid suspend target: %d\n",
			pm_suspend_target_state);
	}

	return -EINVAL;
}

 
static int mcp16502_set_suspend_voltage(struct regulator_dev *rdev, int uV)
{
	int sel = regulator_map_voltage_linear_range(rdev, uV, uV);
	int reg = mcp16502_suspend_get_target_reg(rdev);

	if (sel < 0)
		return sel;

	if (reg < 0)
		return reg;

	return regmap_update_bits(rdev->regmap, reg, MCP16502_VSEL, sel);
}

 
static int mcp16502_set_suspend_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	switch (pm_suspend_target_state) {
	case PM_SUSPEND_STANDBY:
		return _mcp16502_set_mode(rdev, mode, MCP16502_OPMODE_LPM);
	case PM_SUSPEND_ON:
	case PM_SUSPEND_MEM:
		return _mcp16502_set_mode(rdev, mode, MCP16502_OPMODE_HIB);
	default:
		dev_err(&rdev->dev, "invalid suspend target: %d\n",
			pm_suspend_target_state);
	}

	return -EINVAL;
}

 
static int mcp16502_set_suspend_enable(struct regulator_dev *rdev)
{
	int reg = mcp16502_suspend_get_target_reg(rdev);

	if (reg < 0)
		return reg;

	return regmap_update_bits(rdev->regmap, reg, MCP16502_EN, MCP16502_EN);
}

 
static int mcp16502_set_suspend_disable(struct regulator_dev *rdev)
{
	int reg = mcp16502_suspend_get_target_reg(rdev);

	if (reg < 0)
		return reg;

	return regmap_update_bits(rdev->regmap, reg, MCP16502_EN, 0);
}
#endif  

static const struct regulator_ops mcp16502_buck_ops = {
	.list_voltage			= regulator_list_voltage_linear_range,
	.map_voltage			= regulator_map_voltage_linear_range,
	.get_voltage_sel		= regulator_get_voltage_sel_regmap,
	.set_voltage_sel		= regulator_set_voltage_sel_regmap,
	.enable				= regulator_enable_regmap,
	.disable			= regulator_disable_regmap,
	.is_enabled			= regulator_is_enabled_regmap,
	.get_status			= mcp16502_get_status,
	.set_voltage_time_sel		= mcp16502_set_voltage_time_sel,
	.set_ramp_delay			= regulator_set_ramp_delay_regmap,

	.set_mode			= mcp16502_set_mode,
	.get_mode			= mcp16502_get_mode,

#ifdef CONFIG_SUSPEND
	.set_suspend_voltage		= mcp16502_set_suspend_voltage,
	.set_suspend_mode		= mcp16502_set_suspend_mode,
	.set_suspend_enable		= mcp16502_set_suspend_enable,
	.set_suspend_disable		= mcp16502_set_suspend_disable,
#endif  
};

 
static const struct regulator_ops mcp16502_ldo_ops = {
	.list_voltage			= regulator_list_voltage_linear_range,
	.map_voltage			= regulator_map_voltage_linear_range,
	.get_voltage_sel		= regulator_get_voltage_sel_regmap,
	.set_voltage_sel		= regulator_set_voltage_sel_regmap,
	.enable				= regulator_enable_regmap,
	.disable			= regulator_disable_regmap,
	.is_enabled			= regulator_is_enabled_regmap,
	.get_status			= mcp16502_get_status,
	.set_voltage_time_sel		= mcp16502_set_voltage_time_sel,
	.set_ramp_delay			= regulator_set_ramp_delay_regmap,

#ifdef CONFIG_SUSPEND
	.set_suspend_voltage		= mcp16502_set_suspend_voltage,
	.set_suspend_enable		= mcp16502_set_suspend_enable,
	.set_suspend_disable		= mcp16502_set_suspend_disable,
#endif  
};

static const struct of_device_id mcp16502_ids[] = {
	{ .compatible = "microchip,mcp16502", },
	{}
};
MODULE_DEVICE_TABLE(of, mcp16502_ids);

static const struct linear_range b1l12_ranges[] = {
	REGULATOR_LINEAR_RANGE(1200000, VDD_LOW_SEL, VDD_HIGH_SEL, 50000),
};

static const struct linear_range b234_ranges[] = {
	REGULATOR_LINEAR_RANGE(600000, VDD_LOW_SEL, VDD_HIGH_SEL, 25000),
};

static const struct regulator_desc mcp16502_desc[] = {
	 
	MCP16502_REGULATOR("VDD_IO", BUCK1, b1l12_ranges, mcp16502_buck_ops,
			   mcp16502_ramp_b1l12),
	MCP16502_REGULATOR("VDD_DDR", BUCK2, b234_ranges, mcp16502_buck_ops,
			   mcp16502_ramp_b234),
	MCP16502_REGULATOR("VDD_CORE", BUCK3, b234_ranges, mcp16502_buck_ops,
			   mcp16502_ramp_b234),
	MCP16502_REGULATOR("VDD_OTHER", BUCK4, b234_ranges, mcp16502_buck_ops,
			   mcp16502_ramp_b234),
	MCP16502_REGULATOR("LDO1", LDO1, b1l12_ranges, mcp16502_ldo_ops,
			   mcp16502_ramp_b1l12),
	MCP16502_REGULATOR("LDO2", LDO2, b1l12_ranges, mcp16502_ldo_ops,
			   mcp16502_ramp_b1l12)
};

static const struct regmap_range mcp16502_ranges[] = {
	regmap_reg_range(MCP16502_MIN_REG, MCP16502_MAX_REG)
};

static const struct regmap_access_table mcp16502_yes_reg_table = {
	.yes_ranges = mcp16502_ranges,
	.n_yes_ranges = ARRAY_SIZE(mcp16502_ranges),
};

static const struct regmap_config mcp16502_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= MCP16502_MAX_REG,
	.cache_type	= REGCACHE_NONE,
	.rd_table	= &mcp16502_yes_reg_table,
	.wr_table	= &mcp16502_yes_reg_table,
};

static int mcp16502_probe(struct i2c_client *client)
{
	struct regulator_config config = { };
	struct regulator_dev *rdev;
	struct device *dev;
	struct mcp16502 *mcp;
	struct regmap *rmap;
	int i, ret;

	dev = &client->dev;
	config.dev = dev;

	mcp = devm_kzalloc(dev, sizeof(*mcp), GFP_KERNEL);
	if (!mcp)
		return -ENOMEM;

	rmap = devm_regmap_init_i2c(client, &mcp16502_regmap_config);
	if (IS_ERR(rmap)) {
		ret = PTR_ERR(rmap);
		dev_err(dev, "regmap init failed: %d\n", ret);
		return ret;
	}

	i2c_set_clientdata(client, mcp);
	config.regmap = rmap;
	config.driver_data = mcp;

	mcp->lpm = devm_gpiod_get_optional(dev, "lpm", GPIOD_OUT_LOW);
	if (IS_ERR(mcp->lpm)) {
		dev_err(dev, "failed to get lpm pin: %ld\n", PTR_ERR(mcp->lpm));
		return PTR_ERR(mcp->lpm);
	}

	for (i = 0; i < NUM_REGULATORS; i++) {
		rdev = devm_regulator_register(dev, &mcp16502_desc[i], &config);
		if (IS_ERR(rdev)) {
			dev_err(dev,
				"failed to register %s regulator %ld\n",
				mcp16502_desc[i].name, PTR_ERR(rdev));
			return PTR_ERR(rdev);
		}
	}

	mcp16502_gpio_set_mode(mcp, MCP16502_OPMODE_ACTIVE);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mcp16502_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mcp16502 *mcp = i2c_get_clientdata(client);

	mcp16502_gpio_set_mode(mcp, MCP16502_OPMODE_LPM);

	return 0;
}

static int mcp16502_resume_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mcp16502 *mcp = i2c_get_clientdata(client);

	mcp16502_gpio_set_mode(mcp, MCP16502_OPMODE_ACTIVE);

	return 0;
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops mcp16502_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mcp16502_suspend_noirq,
				      mcp16502_resume_noirq)
};
#endif
static const struct i2c_device_id mcp16502_i2c_id[] = {
	{ "mcp16502", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mcp16502_i2c_id);

static struct i2c_driver mcp16502_drv = {
	.probe		= mcp16502_probe,
	.driver		= {
		.name	= "mcp16502-regulator",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table	= mcp16502_ids,
#ifdef CONFIG_PM
		.pm = &mcp16502_pm_ops,
#endif
	},
	.id_table	= mcp16502_i2c_id,
};

module_i2c_driver(mcp16502_drv);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MCP16502 PMIC driver");
MODULE_AUTHOR("Andrei Stefanescu andrei.stefanescu@microchip.com");
