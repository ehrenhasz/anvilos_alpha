
 

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/tps65090.h>

#define MAX_CTRL_READ_TRIES	5
#define MAX_FET_ENABLE_TRIES	1000

#define CTRL_EN_BIT		0  
#define CTRL_WT_BIT		2  
#define CTRL_PG_BIT		4  
#define CTRL_TO_BIT		7  

#define MAX_OVERCURRENT_WAIT	3  

 

struct tps65090_regulator {
	struct device		*dev;
	struct regulator_desc	*desc;
	struct regulator_dev	*rdev;
	bool			overcurrent_wait_valid;
	int			overcurrent_wait;
};

static const struct regulator_ops tps65090_ext_control_ops = {
};

 
static int tps65090_reg_set_overcurrent_wait(struct tps65090_regulator *ri,
					     struct regulator_dev *rdev)
{
	int ret;

	ret = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
				 MAX_OVERCURRENT_WAIT << CTRL_WT_BIT,
				 ri->overcurrent_wait << CTRL_WT_BIT);
	if (ret) {
		dev_err(&rdev->dev, "Error updating overcurrent wait %#x\n",
			rdev->desc->enable_reg);
	}

	return ret;
}

 
static int tps65090_try_enable_fet(struct regulator_dev *rdev)
{
	unsigned int control;
	int ret, i;

	ret = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
				 rdev->desc->enable_mask,
				 rdev->desc->enable_mask);
	if (ret < 0) {
		dev_err(&rdev->dev, "Error in updating reg %#x\n",
			rdev->desc->enable_reg);
		return ret;
	}

	for (i = 0; i < MAX_CTRL_READ_TRIES; i++) {
		ret = regmap_read(rdev->regmap, rdev->desc->enable_reg,
				  &control);
		if (ret < 0)
			return ret;

		if (!(control & BIT(CTRL_TO_BIT)))
			break;

		usleep_range(1000, 1500);
	}
	if (!(control & BIT(CTRL_PG_BIT)))
		return -ENOTRECOVERABLE;

	return 0;
}

 
static int tps65090_fet_enable(struct regulator_dev *rdev)
{
	int ret, tries;

	 
	tries = 0;
	while (true) {
		ret = tps65090_try_enable_fet(rdev);
		if (!ret)
			break;
		if (ret != -ENOTRECOVERABLE || tries == MAX_FET_ENABLE_TRIES)
			goto err;

		 
		ret = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
					 rdev->desc->enable_mask, 0);
		if (ret)
			goto err;

		tries++;
	}

	if (tries)
		dev_warn(&rdev->dev, "reg %#x enable ok after %d tries\n",
			 rdev->desc->enable_reg, tries);

	return 0;
err:
	dev_warn(&rdev->dev, "reg %#x enable failed\n", rdev->desc->enable_reg);
	WARN_ON(1);

	return ret;
}

static const struct regulator_ops tps65090_reg_control_ops = {
	.enable		= regulator_enable_regmap,
	.disable	= regulator_disable_regmap,
	.is_enabled	= regulator_is_enabled_regmap,
};

static const struct regulator_ops tps65090_fet_control_ops = {
	.enable		= tps65090_fet_enable,
	.disable	= regulator_disable_regmap,
	.is_enabled	= regulator_is_enabled_regmap,
};

static const struct regulator_ops tps65090_ldo_ops = {
};

#define tps65090_REG_DESC(_id, _sname, _en_reg, _en_bits, _nvolt, _volt, _ops) \
{							\
	.name = "TPS65090_RAILS"#_id,			\
	.supply_name = _sname,				\
	.id = TPS65090_REGULATOR_##_id,			\
	.n_voltages = _nvolt,				\
	.ops = &_ops,					\
	.fixed_uV = _volt,				\
	.enable_reg = _en_reg,				\
	.enable_val = _en_bits,				\
	.enable_mask = _en_bits,			\
	.type = REGULATOR_VOLTAGE,			\
	.owner = THIS_MODULE,				\
}

#define tps65090_REG_FIXEDV(_id, _sname, en_reg, _en_bits, _volt, _ops) \
	tps65090_REG_DESC(_id, _sname, en_reg, _en_bits, 1, _volt, _ops)

#define tps65090_REG_SWITCH(_id, _sname, en_reg, _en_bits, _ops) \
	tps65090_REG_DESC(_id, _sname, en_reg, _en_bits, 0, 0, _ops)

static struct regulator_desc tps65090_regulator_desc[] = {
	tps65090_REG_FIXEDV(DCDC1, "vsys1",   0x0C, BIT(CTRL_EN_BIT), 5000000,
			    tps65090_reg_control_ops),
	tps65090_REG_FIXEDV(DCDC2, "vsys2",   0x0D, BIT(CTRL_EN_BIT), 3300000,
			    tps65090_reg_control_ops),
	tps65090_REG_SWITCH(DCDC3, "vsys3",   0x0E, BIT(CTRL_EN_BIT),
			    tps65090_reg_control_ops),

	tps65090_REG_SWITCH(FET1,  "infet1",  0x0F,
			    BIT(CTRL_EN_BIT) | BIT(CTRL_PG_BIT),
			    tps65090_fet_control_ops),
	tps65090_REG_SWITCH(FET2,  "infet2",  0x10,
			    BIT(CTRL_EN_BIT) | BIT(CTRL_PG_BIT),
			    tps65090_fet_control_ops),
	tps65090_REG_SWITCH(FET3,  "infet3",  0x11,
			    BIT(CTRL_EN_BIT) | BIT(CTRL_PG_BIT),
			    tps65090_fet_control_ops),
	tps65090_REG_SWITCH(FET4,  "infet4",  0x12,
			    BIT(CTRL_EN_BIT) | BIT(CTRL_PG_BIT),
			    tps65090_fet_control_ops),
	tps65090_REG_SWITCH(FET5,  "infet5",  0x13,
			    BIT(CTRL_EN_BIT) | BIT(CTRL_PG_BIT),
			    tps65090_fet_control_ops),
	tps65090_REG_SWITCH(FET6,  "infet6",  0x14,
			    BIT(CTRL_EN_BIT) | BIT(CTRL_PG_BIT),
			    tps65090_fet_control_ops),
	tps65090_REG_SWITCH(FET7,  "infet7",  0x15,
			    BIT(CTRL_EN_BIT) | BIT(CTRL_PG_BIT),
			    tps65090_fet_control_ops),

	tps65090_REG_FIXEDV(LDO1,  "vsys-l1", 0, 0, 5000000,
			    tps65090_ldo_ops),
	tps65090_REG_FIXEDV(LDO2,  "vsys-l2", 0, 0, 3300000,
			    tps65090_ldo_ops),
};

static inline bool is_dcdc(int id)
{
	switch (id) {
	case TPS65090_REGULATOR_DCDC1:
	case TPS65090_REGULATOR_DCDC2:
	case TPS65090_REGULATOR_DCDC3:
		return true;
	default:
		return false;
	}
}

static int tps65090_config_ext_control(
	struct tps65090_regulator *ri, bool enable)
{
	int ret;
	struct device *parent = ri->dev->parent;
	unsigned int reg_en_reg = ri->desc->enable_reg;

	if (enable)
		ret = tps65090_set_bits(parent, reg_en_reg, 1);
	else
		ret =  tps65090_clr_bits(parent, reg_en_reg, 1);
	if (ret < 0)
		dev_err(ri->dev, "Error in updating reg 0x%x\n", reg_en_reg);
	return ret;
}

static int tps65090_regulator_disable_ext_control(
		struct tps65090_regulator *ri,
		struct tps65090_regulator_plat_data *tps_pdata)
{
	int ret = 0;
	struct device *parent = ri->dev->parent;
	unsigned int reg_en_reg = ri->desc->enable_reg;

	 
	if (tps_pdata->reg_init_data->constraints.always_on ||
			tps_pdata->reg_init_data->constraints.boot_on) {
		ret =  tps65090_set_bits(parent, reg_en_reg, 0);
		if (ret < 0) {
			dev_err(ri->dev, "Error in set reg 0x%x\n", reg_en_reg);
			return ret;
		}
	}
	return tps65090_config_ext_control(ri, false);
}

#ifdef CONFIG_OF
static struct of_regulator_match tps65090_matches[] = {
	{ .name = "dcdc1", },
	{ .name = "dcdc2", },
	{ .name = "dcdc3", },
	{ .name = "fet1",  },
	{ .name = "fet2",  },
	{ .name = "fet3",  },
	{ .name = "fet4",  },
	{ .name = "fet5",  },
	{ .name = "fet6",  },
	{ .name = "fet7",  },
	{ .name = "ldo1",  },
	{ .name = "ldo2",  },
};

static struct tps65090_platform_data *tps65090_parse_dt_reg_data(
		struct platform_device *pdev,
		struct of_regulator_match **tps65090_reg_matches)
{
	struct tps65090_platform_data *tps65090_pdata;
	struct device_node *np = pdev->dev.parent->of_node;
	struct device_node *regulators;
	int idx = 0, ret;
	struct tps65090_regulator_plat_data *reg_pdata;

	tps65090_pdata = devm_kzalloc(&pdev->dev, sizeof(*tps65090_pdata),
				GFP_KERNEL);
	if (!tps65090_pdata)
		return ERR_PTR(-ENOMEM);

	reg_pdata = devm_kcalloc(&pdev->dev,
				 TPS65090_REGULATOR_MAX, sizeof(*reg_pdata),
				 GFP_KERNEL);
	if (!reg_pdata)
		return ERR_PTR(-ENOMEM);

	regulators = of_get_child_by_name(np, "regulators");
	if (!regulators) {
		dev_err(&pdev->dev, "regulator node not found\n");
		return ERR_PTR(-ENODEV);
	}

	ret = of_regulator_match(&pdev->dev, regulators, tps65090_matches,
			ARRAY_SIZE(tps65090_matches));
	of_node_put(regulators);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Error parsing regulator init data: %d\n", ret);
		return ERR_PTR(ret);
	}

	*tps65090_reg_matches = tps65090_matches;
	for (idx = 0; idx < ARRAY_SIZE(tps65090_matches); idx++) {
		struct regulator_init_data *ri_data;
		struct tps65090_regulator_plat_data *rpdata;
		struct device_node *np;

		rpdata = &reg_pdata[idx];
		ri_data = tps65090_matches[idx].init_data;
		if (!ri_data)
			continue;

		np = tps65090_matches[idx].of_node;
		if (!np)
			continue;

		rpdata->reg_init_data = ri_data;
		rpdata->enable_ext_control = of_property_read_bool(np,
						"ti,enable-ext-control");
		if (rpdata->enable_ext_control) {
			enum gpiod_flags gflags;

			if (ri_data->constraints.always_on ||
			    ri_data->constraints.boot_on)
				gflags = GPIOD_OUT_HIGH;
			else
				gflags = GPIOD_OUT_LOW;
			gflags |= GPIOD_FLAGS_BIT_NONEXCLUSIVE;

			rpdata->gpiod = devm_fwnode_gpiod_get(
							&pdev->dev,
							of_fwnode_handle(np),
							"dcdc-ext-control",
							gflags,
							"tps65090");
			if (PTR_ERR(rpdata->gpiod) == -ENOENT) {
				dev_err(&pdev->dev,
					"could not find DCDC external control GPIO\n");
				rpdata->gpiod = NULL;
			} else if (IS_ERR(rpdata->gpiod))
				return ERR_CAST(rpdata->gpiod);
		}

		if (of_property_read_u32(np, "ti,overcurrent-wait",
					 &rpdata->overcurrent_wait) == 0)
			rpdata->overcurrent_wait_valid = true;

		tps65090_pdata->reg_pdata[idx] = rpdata;
	}
	return tps65090_pdata;
}
#else
static inline struct tps65090_platform_data *tps65090_parse_dt_reg_data(
			struct platform_device *pdev,
			struct of_regulator_match **tps65090_reg_matches)
{
	*tps65090_reg_matches = NULL;
	return NULL;
}
#endif

static int tps65090_regulator_probe(struct platform_device *pdev)
{
	struct tps65090 *tps65090_mfd = dev_get_drvdata(pdev->dev.parent);
	struct tps65090_regulator *ri = NULL;
	struct regulator_config config = { };
	struct regulator_dev *rdev;
	struct tps65090_regulator_plat_data *tps_pdata;
	struct tps65090_regulator *pmic;
	struct tps65090_platform_data *tps65090_pdata;
	struct of_regulator_match *tps65090_reg_matches = NULL;
	int num;
	int ret;

	dev_dbg(&pdev->dev, "Probing regulator\n");

	tps65090_pdata = dev_get_platdata(pdev->dev.parent);
	if (!tps65090_pdata && tps65090_mfd->dev->of_node)
		tps65090_pdata = tps65090_parse_dt_reg_data(pdev,
					&tps65090_reg_matches);
	if (IS_ERR_OR_NULL(tps65090_pdata)) {
		dev_err(&pdev->dev, "Platform data missing\n");
		return tps65090_pdata ? PTR_ERR(tps65090_pdata) : -EINVAL;
	}

	pmic = devm_kcalloc(&pdev->dev,
			    TPS65090_REGULATOR_MAX, sizeof(*pmic),
			    GFP_KERNEL);
	if (!pmic)
		return -ENOMEM;

	for (num = 0; num < TPS65090_REGULATOR_MAX; num++) {
		tps_pdata = tps65090_pdata->reg_pdata[num];

		ri = &pmic[num];
		ri->dev = &pdev->dev;
		ri->desc = &tps65090_regulator_desc[num];
		if (tps_pdata) {
			ri->overcurrent_wait_valid =
				tps_pdata->overcurrent_wait_valid;
			ri->overcurrent_wait = tps_pdata->overcurrent_wait;
		}

		 
		if (tps_pdata && is_dcdc(num) && tps_pdata->reg_init_data) {
			if (tps_pdata->enable_ext_control) {
				config.ena_gpiod = tps_pdata->gpiod;
				ri->desc->ops = &tps65090_ext_control_ops;
			} else {
				ret = tps65090_regulator_disable_ext_control(
						ri, tps_pdata);
				if (ret < 0) {
					dev_err(&pdev->dev,
						"failed disable ext control\n");
					return ret;
				}
			}
		}

		config.dev = pdev->dev.parent;
		config.driver_data = ri;
		config.regmap = tps65090_mfd->rmap;
		if (tps_pdata)
			config.init_data = tps_pdata->reg_init_data;
		else
			config.init_data = NULL;
		if (tps65090_reg_matches)
			config.of_node = tps65090_reg_matches[num].of_node;
		else
			config.of_node = NULL;

		 
		if (config.ena_gpiod)
			devm_gpiod_unhinge(&pdev->dev, config.ena_gpiod);
		rdev = devm_regulator_register(&pdev->dev, ri->desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev, "failed to register regulator %s\n",
				ri->desc->name);
			return PTR_ERR(rdev);
		}
		ri->rdev = rdev;

		if (ri->overcurrent_wait_valid) {
			ret = tps65090_reg_set_overcurrent_wait(ri, rdev);
			if (ret < 0)
				return ret;
		}

		 
		if (tps_pdata && is_dcdc(num) && tps_pdata->reg_init_data &&
				tps_pdata->enable_ext_control) {
			ret = tps65090_config_ext_control(ri, true);
			if (ret < 0)
				return ret;
		}
	}

	platform_set_drvdata(pdev, pmic);
	return 0;
}

static struct platform_driver tps65090_regulator_driver = {
	.driver	= {
		.name	= "tps65090-pmic",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe		= tps65090_regulator_probe,
};

static int __init tps65090_regulator_init(void)
{
	return platform_driver_register(&tps65090_regulator_driver);
}
subsys_initcall(tps65090_regulator_init);

static void __exit tps65090_regulator_exit(void)
{
	platform_driver_unregister(&tps65090_regulator_driver);
}
module_exit(tps65090_regulator_exit);

MODULE_DESCRIPTION("tps65090 regulator driver");
MODULE_AUTHOR("Venu Byravarasu <vbyravarasu@nvidia.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:tps65090-pmic");
