



#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/rohm-bd718x7.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>

 
#define BD71847_BUCK1_STARTUP_TIME 144
#define BD71847_BUCK2_STARTUP_TIME 162
#define BD71847_BUCK3_STARTUP_TIME 162
#define BD71847_BUCK4_STARTUP_TIME 240
#define BD71847_BUCK5_STARTUP_TIME 270
#define BD71847_BUCK6_STARTUP_TIME 200
#define BD71847_LDO1_STARTUP_TIME  440
#define BD71847_LDO2_STARTUP_TIME  370
#define BD71847_LDO3_STARTUP_TIME  310
#define BD71847_LDO4_STARTUP_TIME  400
#define BD71847_LDO5_STARTUP_TIME  530
#define BD71847_LDO6_STARTUP_TIME  400

#define BD71837_BUCK1_STARTUP_TIME 160
#define BD71837_BUCK2_STARTUP_TIME 180
#define BD71837_BUCK3_STARTUP_TIME 180
#define BD71837_BUCK4_STARTUP_TIME 180
#define BD71837_BUCK5_STARTUP_TIME 160
#define BD71837_BUCK6_STARTUP_TIME 240
#define BD71837_BUCK7_STARTUP_TIME 220
#define BD71837_BUCK8_STARTUP_TIME 200
#define BD71837_LDO1_STARTUP_TIME  440
#define BD71837_LDO2_STARTUP_TIME  370
#define BD71837_LDO3_STARTUP_TIME  310
#define BD71837_LDO4_STARTUP_TIME  400
#define BD71837_LDO5_STARTUP_TIME  310
#define BD71837_LDO6_STARTUP_TIME  400
#define BD71837_LDO7_STARTUP_TIME  530

 
#define BD718XX_HWOPNAME(swopname) swopname##_hwcontrol

#define BD718XX_OPS(name, _list_voltage, _map_voltage, _set_voltage_sel, \
		   _get_voltage_sel, _set_voltage_time_sel, _set_ramp_delay, \
		   _set_uvp, _set_ovp)				\
static const struct regulator_ops name = {			\
	.enable = regulator_enable_regmap,			\
	.disable = regulator_disable_regmap,			\
	.is_enabled = regulator_is_enabled_regmap,		\
	.list_voltage = (_list_voltage),			\
	.map_voltage = (_map_voltage),				\
	.set_voltage_sel = (_set_voltage_sel),			\
	.get_voltage_sel = (_get_voltage_sel),			\
	.set_voltage_time_sel = (_set_voltage_time_sel),	\
	.set_ramp_delay = (_set_ramp_delay),			\
	.set_under_voltage_protection = (_set_uvp),		\
	.set_over_voltage_protection = (_set_ovp),		\
};								\
								\
static const struct regulator_ops BD718XX_HWOPNAME(name) = {	\
	.is_enabled = always_enabled_by_hwstate,		\
	.list_voltage = (_list_voltage),			\
	.map_voltage = (_map_voltage),				\
	.set_voltage_sel = (_set_voltage_sel),			\
	.get_voltage_sel = (_get_voltage_sel),			\
	.set_voltage_time_sel = (_set_voltage_time_sel),	\
	.set_ramp_delay = (_set_ramp_delay),			\
	.set_under_voltage_protection = (_set_uvp),		\
	.set_over_voltage_protection = (_set_ovp),		\
}								\

 
static const unsigned int bd718xx_ramp_delay[] = { 10000, 5000, 2500, 1250 };

 
static int always_enabled_by_hwstate(struct regulator_dev *rdev)
{
	return 1;
}

static int never_enabled_by_hwstate(struct regulator_dev *rdev)
{
	return 0;
}

static int bd71837_get_buck34_enable_hwctrl(struct regulator_dev *rdev)
{
	int ret;
	unsigned int val;

	ret = regmap_read(rdev->regmap, rdev->desc->enable_reg, &val);
	if (ret)
		return ret;

	return !!(BD718XX_BUCK_RUN_ON & val);
}

static void voltage_change_done(struct regulator_dev *rdev, unsigned int sel,
				unsigned int *mask)
{
	int ret;

	if (*mask) {
		 
		msleep(1);

		ret = regmap_clear_bits(rdev->regmap, BD718XX_REG_MVRFLTMASK2,
					 *mask);
		if (ret)
			dev_err(&rdev->dev,
				"Failed to re-enable voltage monitoring (%d)\n",
				ret);
	}
}

static int voltage_change_prepare(struct regulator_dev *rdev, unsigned int sel,
				  unsigned int *mask)
{
	int ret;

	*mask = 0;
	if (rdev->desc->ops->is_enabled(rdev)) {
		int now, new;

		now = rdev->desc->ops->get_voltage_sel(rdev);
		if (now < 0)
			return now;

		now = rdev->desc->ops->list_voltage(rdev, now);
		if (now < 0)
			return now;

		new = rdev->desc->ops->list_voltage(rdev, sel);
		if (new < 0)
			return new;

		 
		if (new > now) {
			int tmp;
			int prot_bit;
			int ldo_offset = rdev->desc->id - BD718XX_LDO1;

			prot_bit = BD718XX_LDO1_VRMON80 << ldo_offset;
			ret = regmap_read(rdev->regmap, BD718XX_REG_MVRFLTMASK2,
					  &tmp);
			if (ret) {
				dev_err(&rdev->dev,
					"Failed to read voltage monitoring state\n");
				return ret;
			}

			if (!(tmp & prot_bit)) {
				 
				ret = regmap_set_bits(rdev->regmap,
						      BD718XX_REG_MVRFLTMASK2,
						      prot_bit);
				 
				*mask = prot_bit;
			}
			if (ret) {
				dev_err(&rdev->dev,
					"Failed to stop voltage monitoring\n");
				return ret;
			}
		}
	}

	return 0;
}

static int bd718xx_set_voltage_sel_restricted(struct regulator_dev *rdev,
						    unsigned int sel)
{
	int ret;
	int mask;

	ret = voltage_change_prepare(rdev, sel, &mask);
	if (ret)
		return ret;

	ret = regulator_set_voltage_sel_regmap(rdev, sel);
	voltage_change_done(rdev, sel, &mask);

	return ret;
}

static int bd718xx_set_voltage_sel_pickable_restricted(
		struct regulator_dev *rdev, unsigned int sel)
{
	int ret;
	int mask;

	ret = voltage_change_prepare(rdev, sel, &mask);
	if (ret)
		return ret;

	ret = regulator_set_voltage_sel_pickable_regmap(rdev, sel);
	voltage_change_done(rdev, sel, &mask);

	return ret;
}

static int bd71837_set_voltage_sel_pickable_restricted(
		struct regulator_dev *rdev, unsigned int sel)
{
	if (rdev->desc->ops->is_enabled(rdev))
		return -EBUSY;

	return regulator_set_voltage_sel_pickable_regmap(rdev, sel);
}

 
static const struct linear_range bd718xx_dvs_buck_volts[] = {
	REGULATOR_LINEAR_RANGE(700000, 0x00, 0x3C, 10000),
	REGULATOR_LINEAR_RANGE(1300000, 0x3D, 0x3F, 0),
};

 
static const struct linear_range bd71837_buck5_volts[] = {
	 
	REGULATOR_LINEAR_RANGE(700000, 0x00, 0x03, 100000),
	REGULATOR_LINEAR_RANGE(1050000, 0x04, 0x05, 50000),
	REGULATOR_LINEAR_RANGE(1200000, 0x06, 0x07, 150000),
	 
	REGULATOR_LINEAR_RANGE(675000, 0x0, 0x3, 100000),
	REGULATOR_LINEAR_RANGE(1025000, 0x4, 0x5, 50000),
	REGULATOR_LINEAR_RANGE(1175000, 0x6, 0x7, 150000),
};

 
static const unsigned int bd71837_buck5_volt_range_sel[] = {
	0x0, 0x0, 0x0, 0x1, 0x1, 0x1
};

 
static const struct linear_range bd71847_buck3_volts[] = {
	 
	REGULATOR_LINEAR_RANGE(700000, 0x00, 0x03, 100000),
	REGULATOR_LINEAR_RANGE(1050000, 0x04, 0x05, 50000),
	REGULATOR_LINEAR_RANGE(1200000, 0x06, 0x07, 150000),
	 
	REGULATOR_LINEAR_RANGE(550000, 0x0, 0x7, 50000),
	 
	REGULATOR_LINEAR_RANGE(675000, 0x0, 0x3, 100000),
	REGULATOR_LINEAR_RANGE(1025000, 0x4, 0x5, 50000),
	REGULATOR_LINEAR_RANGE(1175000, 0x6, 0x7, 150000),
};

static const unsigned int bd71847_buck3_volt_range_sel[] = {
	0x0, 0x0, 0x0, 0x1, 0x2, 0x2, 0x2
};

static const struct linear_range bd71847_buck4_volts[] = {
	REGULATOR_LINEAR_RANGE(3000000, 0x00, 0x03, 100000),
	REGULATOR_LINEAR_RANGE(2600000, 0x00, 0x03, 100000),
};

static const unsigned int bd71847_buck4_volt_range_sel[] = { 0x0, 0x1 };

 
static const struct linear_range bd71837_buck6_volts[] = {
	REGULATOR_LINEAR_RANGE(3000000, 0x00, 0x03, 100000),
};

 
static const unsigned int bd718xx_3rd_nodvs_buck_volts[] = {
	1605000, 1695000, 1755000, 1800000, 1845000, 1905000, 1950000, 1995000
};

 
static const struct linear_range bd718xx_4th_nodvs_buck_volts[] = {
	REGULATOR_LINEAR_RANGE(800000, 0x00, 0x3C, 10000),
};

 
static const struct linear_range bd718xx_ldo1_volts[] = {
	REGULATOR_LINEAR_RANGE(3000000, 0x00, 0x03, 100000),
	REGULATOR_LINEAR_RANGE(1600000, 0x00, 0x03, 100000),
};

static const unsigned int bd718xx_ldo1_volt_range_sel[] = { 0x0, 0x1 };

 
static const unsigned int ldo_2_volts[] = {
	900000, 800000
};

 
static const struct linear_range bd718xx_ldo3_volts[] = {
	REGULATOR_LINEAR_RANGE(1800000, 0x00, 0x0F, 100000),
};

 
static const struct linear_range bd718xx_ldo4_volts[] = {
	REGULATOR_LINEAR_RANGE(900000, 0x00, 0x09, 100000),
};

 
static const struct linear_range bd71837_ldo5_volts[] = {
	REGULATOR_LINEAR_RANGE(1800000, 0x00, 0x0F, 100000),
};

 
static const struct linear_range bd71847_ldo5_volts[] = {
	REGULATOR_LINEAR_RANGE(1800000, 0x00, 0x0F, 100000),
	REGULATOR_LINEAR_RANGE(800000, 0x00, 0x0F, 100000),
};

static const unsigned int bd71847_ldo5_volt_range_sel[] = { 0x0, 0x1 };

 
static const struct linear_range bd718xx_ldo6_volts[] = {
	REGULATOR_LINEAR_RANGE(900000, 0x00, 0x09, 100000),
};

 
static const struct linear_range bd71837_ldo7_volts[] = {
	REGULATOR_LINEAR_RANGE(1800000, 0x00, 0x0F, 100000),
};

struct reg_init {
	unsigned int reg;
	unsigned int mask;
	unsigned int val;
};
struct bd718xx_regulator_data {
	struct regulator_desc desc;
	const struct rohm_dvs_config dvs;
	const struct reg_init init;
	const struct reg_init *additional_inits;
	int additional_init_amnt;
};

static int bd718x7_xvp_sanity_check(struct regulator_dev *rdev, int lim_uV,
				    int severity)
{
	 
	if (severity != REGULATOR_SEVERITY_PROT) {
		dev_err(&rdev->dev,
			"Unsupported Under Voltage protection level\n");
		return -EINVAL;
	}

	 
	if (lim_uV)
		return -EINVAL;

	return 0;
}

static int bd718x7_set_ldo_uvp(struct regulator_dev *rdev, int lim_uV,
			       int severity, bool enable)
{
	int ldo_offset = rdev->desc->id - BD718XX_LDO1;
	int prot_bit, ret;

	ret = bd718x7_xvp_sanity_check(rdev, lim_uV, severity);
	if (ret)
		return ret;

	prot_bit = BD718XX_LDO1_VRMON80 << ldo_offset;

	if (enable)
		return regmap_clear_bits(rdev->regmap, BD718XX_REG_MVRFLTMASK2,
					 prot_bit);

	return regmap_set_bits(rdev->regmap, BD718XX_REG_MVRFLTMASK2,
			       prot_bit);
}

static int bd718x7_get_buck_prot_reg(int id, int *reg)
{

	if (id > BD718XX_BUCK8) {
		WARN_ON(id > BD718XX_BUCK8);
		return -EINVAL;
	}

	if (id > BD718XX_BUCK4)
		*reg = BD718XX_REG_MVRFLTMASK0;
	else
		*reg = BD718XX_REG_MVRFLTMASK1;

	return 0;
}

static int bd718x7_get_buck_ovp_info(int id, int *reg, int *bit)
{
	int ret;

	ret = bd718x7_get_buck_prot_reg(id, reg);
	if (ret)
		return ret;

	*bit = BIT((id % 4) * 2 + 1);

	return 0;
}

static int bd718x7_get_buck_uvp_info(int id, int *reg, int *bit)
{
	int ret;

	ret = bd718x7_get_buck_prot_reg(id, reg);
	if (ret)
		return ret;

	*bit = BIT((id % 4) * 2);

	return 0;
}

static int bd718x7_set_buck_uvp(struct regulator_dev *rdev, int lim_uV,
				int severity, bool enable)
{
	int bit, reg, ret;

	ret = bd718x7_xvp_sanity_check(rdev, lim_uV, severity);
	if (ret)
		return ret;

	ret = bd718x7_get_buck_uvp_info(rdev->desc->id, &reg, &bit);
	if (ret)
		return ret;

	if (enable)
		return regmap_clear_bits(rdev->regmap, reg, bit);

	return regmap_set_bits(rdev->regmap, reg, bit);

}

static int bd718x7_set_buck_ovp(struct regulator_dev *rdev, int lim_uV,
				int severity,
				bool enable)
{
	int bit, reg, ret;

	ret = bd718x7_xvp_sanity_check(rdev, lim_uV, severity);
	if (ret)
		return ret;

	ret = bd718x7_get_buck_ovp_info(rdev->desc->id, &reg, &bit);
	if (ret)
		return ret;

	if (enable)
		return regmap_clear_bits(rdev->regmap, reg, bit);

	return regmap_set_bits(rdev->regmap, reg, bit);
}

 
BD718XX_OPS(bd718xx_pickable_range_ldo_ops,
	    regulator_list_voltage_pickable_linear_range, NULL,
	    bd718xx_set_voltage_sel_pickable_restricted,
	    regulator_get_voltage_sel_pickable_regmap, NULL, NULL,
	    bd718x7_set_ldo_uvp, NULL);

 
static const struct regulator_ops bd718xx_ldo5_ops_hwstate = {
	.is_enabled = never_enabled_by_hwstate,
	.list_voltage = regulator_list_voltage_pickable_linear_range,
	.set_voltage_sel = bd718xx_set_voltage_sel_pickable_restricted,
	.get_voltage_sel = regulator_get_voltage_sel_pickable_regmap,
	.set_under_voltage_protection = bd718x7_set_ldo_uvp,
};

BD718XX_OPS(bd718xx_pickable_range_buck_ops,
	    regulator_list_voltage_pickable_linear_range, NULL,
	    regulator_set_voltage_sel_pickable_regmap,
	    regulator_get_voltage_sel_pickable_regmap,
	    regulator_set_voltage_time_sel, NULL, bd718x7_set_buck_uvp,
	    bd718x7_set_buck_ovp);

BD718XX_OPS(bd718xx_ldo_regulator_ops, regulator_list_voltage_linear_range,
	    NULL, bd718xx_set_voltage_sel_restricted,
	    regulator_get_voltage_sel_regmap, NULL, NULL, bd718x7_set_ldo_uvp,
	    NULL);

BD718XX_OPS(bd718xx_ldo_regulator_nolinear_ops, regulator_list_voltage_table,
	    NULL, bd718xx_set_voltage_sel_restricted,
	    regulator_get_voltage_sel_regmap, NULL, NULL, bd718x7_set_ldo_uvp,
	    NULL);

BD718XX_OPS(bd718xx_buck_regulator_ops, regulator_list_voltage_linear_range,
	    NULL, regulator_set_voltage_sel_regmap,
	    regulator_get_voltage_sel_regmap, regulator_set_voltage_time_sel,
	    NULL, bd718x7_set_buck_uvp, bd718x7_set_buck_ovp);

BD718XX_OPS(bd718xx_buck_regulator_nolinear_ops, regulator_list_voltage_table,
	    regulator_map_voltage_ascend, regulator_set_voltage_sel_regmap,
	    regulator_get_voltage_sel_regmap, regulator_set_voltage_time_sel,
	    NULL, bd718x7_set_buck_uvp, bd718x7_set_buck_ovp);

 
BD718XX_OPS(bd71837_pickable_range_ldo_ops,
	    regulator_list_voltage_pickable_linear_range, NULL,
	    bd71837_set_voltage_sel_pickable_restricted,
	    regulator_get_voltage_sel_pickable_regmap, NULL, NULL,
	    bd718x7_set_ldo_uvp, NULL);

BD718XX_OPS(bd71837_pickable_range_buck_ops,
	    regulator_list_voltage_pickable_linear_range, NULL,
	    bd71837_set_voltage_sel_pickable_restricted,
	    regulator_get_voltage_sel_pickable_regmap,
	    regulator_set_voltage_time_sel, NULL, bd718x7_set_buck_uvp,
	    bd718x7_set_buck_ovp);

BD718XX_OPS(bd71837_ldo_regulator_ops, regulator_list_voltage_linear_range,
	    NULL, rohm_regulator_set_voltage_sel_restricted,
	    regulator_get_voltage_sel_regmap, NULL, NULL, bd718x7_set_ldo_uvp,
	    NULL);

BD718XX_OPS(bd71837_ldo_regulator_nolinear_ops, regulator_list_voltage_table,
	    NULL, rohm_regulator_set_voltage_sel_restricted,
	    regulator_get_voltage_sel_regmap, NULL, NULL, bd718x7_set_ldo_uvp,
	    NULL);

BD718XX_OPS(bd71837_buck_regulator_ops, regulator_list_voltage_linear_range,
	    NULL, rohm_regulator_set_voltage_sel_restricted,
	    regulator_get_voltage_sel_regmap, regulator_set_voltage_time_sel,
	    NULL, bd718x7_set_buck_uvp, bd718x7_set_buck_ovp);

BD718XX_OPS(bd71837_buck_regulator_nolinear_ops, regulator_list_voltage_table,
	    regulator_map_voltage_ascend, rohm_regulator_set_voltage_sel_restricted,
	    regulator_get_voltage_sel_regmap, regulator_set_voltage_time_sel,
	    NULL, bd718x7_set_buck_uvp, bd718x7_set_buck_ovp);
 
static const struct regulator_ops bd71837_buck34_ops_hwctrl = {
	.is_enabled = bd71837_get_buck34_enable_hwctrl,
	.list_voltage = regulator_list_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.set_ramp_delay = regulator_set_ramp_delay_regmap,
	.set_under_voltage_protection = bd718x7_set_buck_uvp,
	.set_over_voltage_protection = bd718x7_set_buck_ovp,
};

 
BD718XX_OPS(bd718xx_dvs_buck_regulator_ops, regulator_list_voltage_linear_range,
	    NULL, regulator_set_voltage_sel_regmap,
	    regulator_get_voltage_sel_regmap, regulator_set_voltage_time_sel,
	    regulator_set_ramp_delay_regmap, bd718x7_set_buck_uvp,
	    bd718x7_set_buck_ovp);



 
static const struct reg_init bd71837_ldo5_inits[] = {
	{
		.reg = BD718XX_REG_MVRFLTMASK2,
		.mask = BD718XX_LDO5_VRMON80,
		.val = BD718XX_LDO5_VRMON80,
	},
};

static const struct reg_init bd71837_ldo6_inits[] = {
	{
		.reg = BD718XX_REG_MVRFLTMASK2,
		.mask = BD718XX_LDO6_VRMON80,
		.val = BD718XX_LDO6_VRMON80,
	},
};

static int buck_set_hw_dvs_levels(struct device_node *np,
			    const struct regulator_desc *desc,
			    struct regulator_config *cfg)
{
	struct bd718xx_regulator_data *data;

	data = container_of(desc, struct bd718xx_regulator_data, desc);

	return rohm_regulator_set_dvs_levels(&data->dvs, np, desc, cfg->regmap);
}

static const struct regulator_ops *bd71847_swcontrol_ops[] = {
	&bd718xx_dvs_buck_regulator_ops, &bd718xx_dvs_buck_regulator_ops,
	&bd718xx_pickable_range_buck_ops, &bd718xx_pickable_range_buck_ops,
	&bd718xx_buck_regulator_nolinear_ops, &bd718xx_buck_regulator_ops,
	&bd718xx_pickable_range_ldo_ops, &bd718xx_ldo_regulator_nolinear_ops,
	&bd718xx_ldo_regulator_ops, &bd718xx_ldo_regulator_ops,
	&bd718xx_pickable_range_ldo_ops, &bd718xx_ldo_regulator_ops,
};

static const struct regulator_ops *bd71847_hwcontrol_ops[] = {
	&BD718XX_HWOPNAME(bd718xx_dvs_buck_regulator_ops),
	&BD718XX_HWOPNAME(bd718xx_dvs_buck_regulator_ops),
	&BD718XX_HWOPNAME(bd718xx_pickable_range_buck_ops),
	&BD718XX_HWOPNAME(bd718xx_pickable_range_buck_ops),
	&BD718XX_HWOPNAME(bd718xx_buck_regulator_nolinear_ops),
	&BD718XX_HWOPNAME(bd718xx_buck_regulator_ops),
	&BD718XX_HWOPNAME(bd718xx_pickable_range_ldo_ops),
	&BD718XX_HWOPNAME(bd718xx_ldo_regulator_nolinear_ops),
	&BD718XX_HWOPNAME(bd718xx_ldo_regulator_ops),
	&BD718XX_HWOPNAME(bd718xx_ldo_regulator_ops),
	&bd718xx_ldo5_ops_hwstate,
	&BD718XX_HWOPNAME(bd718xx_ldo_regulator_ops),
};

static struct bd718xx_regulator_data bd71847_regulators[] = {
	{
		.desc = {
			.name = "buck1",
			.of_match = of_match_ptr("BUCK1"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_BUCK1,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_DVS_BUCK_VOLTAGE_NUM,
			.linear_ranges = bd718xx_dvs_buck_volts,
			.n_linear_ranges =
				ARRAY_SIZE(bd718xx_dvs_buck_volts),
			.vsel_reg = BD718XX_REG_BUCK1_VOLT_RUN,
			.vsel_mask = DVS_BUCK_RUN_MASK,
			.enable_reg = BD718XX_REG_BUCK1_CTRL,
			.enable_mask = BD718XX_BUCK_EN,
			.enable_time = BD71847_BUCK1_STARTUP_TIME,
			.owner = THIS_MODULE,
			.ramp_delay_table = bd718xx_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd718xx_ramp_delay),
			.ramp_reg = BD718XX_REG_BUCK1_CTRL,
			.ramp_mask = BUCK_RAMPRATE_MASK,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND,
			.run_reg = BD718XX_REG_BUCK1_VOLT_RUN,
			.run_mask = DVS_BUCK_RUN_MASK,
			.idle_reg = BD718XX_REG_BUCK1_VOLT_IDLE,
			.idle_mask = DVS_BUCK_RUN_MASK,
			.suspend_reg = BD718XX_REG_BUCK1_VOLT_SUSP,
			.suspend_mask = DVS_BUCK_RUN_MASK,
		},
		.init = {
			.reg = BD718XX_REG_BUCK1_CTRL,
			.mask = BD718XX_BUCK_SEL,
			.val = BD718XX_BUCK_SEL,
		},
	},
	{
		.desc = {
			.name = "buck2",
			.of_match = of_match_ptr("BUCK2"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_BUCK2,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_DVS_BUCK_VOLTAGE_NUM,
			.linear_ranges = bd718xx_dvs_buck_volts,
			.n_linear_ranges = ARRAY_SIZE(bd718xx_dvs_buck_volts),
			.vsel_reg = BD718XX_REG_BUCK2_VOLT_RUN,
			.vsel_mask = DVS_BUCK_RUN_MASK,
			.enable_reg = BD718XX_REG_BUCK2_CTRL,
			.enable_mask = BD718XX_BUCK_EN,
			.enable_time = BD71847_BUCK2_STARTUP_TIME,
			.ramp_delay_table = bd718xx_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd718xx_ramp_delay),
			.ramp_reg = BD718XX_REG_BUCK2_CTRL,
			.ramp_mask = BUCK_RAMPRATE_MASK,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE,
			.run_reg = BD718XX_REG_BUCK2_VOLT_RUN,
			.run_mask = DVS_BUCK_RUN_MASK,
			.idle_reg = BD718XX_REG_BUCK2_VOLT_IDLE,
			.idle_mask = DVS_BUCK_RUN_MASK,
		},
		.init = {
			.reg = BD718XX_REG_BUCK2_CTRL,
			.mask = BD718XX_BUCK_SEL,
			.val = BD718XX_BUCK_SEL,
		},
	},
	{
		.desc = {
			.name = "buck3",
			.of_match = of_match_ptr("BUCK3"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_BUCK3,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD71847_BUCK3_VOLTAGE_NUM,
			.linear_ranges = bd71847_buck3_volts,
			.n_linear_ranges =
				ARRAY_SIZE(bd71847_buck3_volts),
			.vsel_reg = BD718XX_REG_1ST_NODVS_BUCK_VOLT,
			.vsel_mask = BD718XX_1ST_NODVS_BUCK_MASK,
			.vsel_range_reg = BD718XX_REG_1ST_NODVS_BUCK_VOLT,
			.vsel_range_mask = BD71847_BUCK3_RANGE_MASK,
			.linear_range_selectors_bitfield = bd71847_buck3_volt_range_sel,
			.enable_reg = BD718XX_REG_1ST_NODVS_BUCK_CTRL,
			.enable_mask = BD718XX_BUCK_EN,
			.enable_time = BD71847_BUCK3_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_1ST_NODVS_BUCK_CTRL,
			.mask = BD718XX_BUCK_SEL,
			.val = BD718XX_BUCK_SEL,
		},
	},
	{
		.desc = {
			.name = "buck4",
			.of_match = of_match_ptr("BUCK4"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_BUCK4,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD71847_BUCK4_VOLTAGE_NUM,
			.linear_ranges = bd71847_buck4_volts,
			.n_linear_ranges =
				ARRAY_SIZE(bd71847_buck4_volts),
			.enable_reg = BD718XX_REG_2ND_NODVS_BUCK_CTRL,
			.vsel_reg = BD718XX_REG_2ND_NODVS_BUCK_VOLT,
			.vsel_mask = BD71847_BUCK4_MASK,
			.vsel_range_reg = BD718XX_REG_2ND_NODVS_BUCK_VOLT,
			.vsel_range_mask = BD71847_BUCK4_RANGE_MASK,
			.linear_range_selectors_bitfield = bd71847_buck4_volt_range_sel,
			.enable_mask = BD718XX_BUCK_EN,
			.enable_time = BD71847_BUCK4_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_2ND_NODVS_BUCK_CTRL,
			.mask = BD718XX_BUCK_SEL,
			.val = BD718XX_BUCK_SEL,
		},
	},
	{
		.desc = {
			.name = "buck5",
			.of_match = of_match_ptr("BUCK5"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_BUCK5,
			.type = REGULATOR_VOLTAGE,
			.volt_table = &bd718xx_3rd_nodvs_buck_volts[0],
			.n_voltages = ARRAY_SIZE(bd718xx_3rd_nodvs_buck_volts),
			.vsel_reg = BD718XX_REG_3RD_NODVS_BUCK_VOLT,
			.vsel_mask = BD718XX_3RD_NODVS_BUCK_MASK,
			.enable_reg = BD718XX_REG_3RD_NODVS_BUCK_CTRL,
			.enable_mask = BD718XX_BUCK_EN,
			.enable_time = BD71847_BUCK5_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_3RD_NODVS_BUCK_CTRL,
			.mask = BD718XX_BUCK_SEL,
			.val = BD718XX_BUCK_SEL,
		},
	},
	{
		.desc = {
			.name = "buck6",
			.of_match = of_match_ptr("BUCK6"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_BUCK6,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_4TH_NODVS_BUCK_VOLTAGE_NUM,
			.linear_ranges = bd718xx_4th_nodvs_buck_volts,
			.n_linear_ranges =
				ARRAY_SIZE(bd718xx_4th_nodvs_buck_volts),
			.vsel_reg = BD718XX_REG_4TH_NODVS_BUCK_VOLT,
			.vsel_mask = BD718XX_4TH_NODVS_BUCK_MASK,
			.enable_reg = BD718XX_REG_4TH_NODVS_BUCK_CTRL,
			.enable_mask = BD718XX_BUCK_EN,
			.enable_time = BD71847_BUCK6_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_4TH_NODVS_BUCK_CTRL,
			.mask = BD718XX_BUCK_SEL,
			.val = BD718XX_BUCK_SEL,
		},
	},
	{
		.desc = {
			.name = "ldo1",
			.of_match = of_match_ptr("LDO1"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_LDO1,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_LDO1_VOLTAGE_NUM,
			.linear_ranges = bd718xx_ldo1_volts,
			.n_linear_ranges = ARRAY_SIZE(bd718xx_ldo1_volts),
			.vsel_reg = BD718XX_REG_LDO1_VOLT,
			.vsel_mask = BD718XX_LDO1_MASK,
			.vsel_range_reg = BD718XX_REG_LDO1_VOLT,
			.vsel_range_mask = BD718XX_LDO1_RANGE_MASK,
			.linear_range_selectors_bitfield = bd718xx_ldo1_volt_range_sel,
			.enable_reg = BD718XX_REG_LDO1_VOLT,
			.enable_mask = BD718XX_LDO_EN,
			.enable_time = BD71847_LDO1_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_LDO1_VOLT,
			.mask = BD718XX_LDO_SEL,
			.val = BD718XX_LDO_SEL,
		},
	},
	{
		.desc = {
			.name = "ldo2",
			.of_match = of_match_ptr("LDO2"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_LDO2,
			.type = REGULATOR_VOLTAGE,
			.volt_table = &ldo_2_volts[0],
			.vsel_reg = BD718XX_REG_LDO2_VOLT,
			.vsel_mask = BD718XX_LDO2_MASK,
			.n_voltages = ARRAY_SIZE(ldo_2_volts),
			.enable_reg = BD718XX_REG_LDO2_VOLT,
			.enable_mask = BD718XX_LDO_EN,
			.enable_time = BD71847_LDO2_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_LDO2_VOLT,
			.mask = BD718XX_LDO_SEL,
			.val = BD718XX_LDO_SEL,
		},
	},
	{
		.desc = {
			.name = "ldo3",
			.of_match = of_match_ptr("LDO3"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_LDO3,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_LDO3_VOLTAGE_NUM,
			.linear_ranges = bd718xx_ldo3_volts,
			.n_linear_ranges = ARRAY_SIZE(bd718xx_ldo3_volts),
			.vsel_reg = BD718XX_REG_LDO3_VOLT,
			.vsel_mask = BD718XX_LDO3_MASK,
			.enable_reg = BD718XX_REG_LDO3_VOLT,
			.enable_mask = BD718XX_LDO_EN,
			.enable_time = BD71847_LDO3_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_LDO3_VOLT,
			.mask = BD718XX_LDO_SEL,
			.val = BD718XX_LDO_SEL,
		},
	},
	{
		.desc = {
			.name = "ldo4",
			.of_match = of_match_ptr("LDO4"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_LDO4,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_LDO4_VOLTAGE_NUM,
			.linear_ranges = bd718xx_ldo4_volts,
			.n_linear_ranges = ARRAY_SIZE(bd718xx_ldo4_volts),
			.vsel_reg = BD718XX_REG_LDO4_VOLT,
			.vsel_mask = BD718XX_LDO4_MASK,
			.enable_reg = BD718XX_REG_LDO4_VOLT,
			.enable_mask = BD718XX_LDO_EN,
			.enable_time = BD71847_LDO4_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_LDO4_VOLT,
			.mask = BD718XX_LDO_SEL,
			.val = BD718XX_LDO_SEL,
		},
	},
	{
		.desc = {
			.name = "ldo5",
			.of_match = of_match_ptr("LDO5"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_LDO5,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD71847_LDO5_VOLTAGE_NUM,
			.linear_ranges = bd71847_ldo5_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71847_ldo5_volts),
			.vsel_reg = BD718XX_REG_LDO5_VOLT,
			.vsel_mask = BD71847_LDO5_MASK,
			.vsel_range_reg = BD718XX_REG_LDO5_VOLT,
			.vsel_range_mask = BD71847_LDO5_RANGE_MASK,
			.linear_range_selectors_bitfield = bd71847_ldo5_volt_range_sel,
			.enable_reg = BD718XX_REG_LDO5_VOLT,
			.enable_mask = BD718XX_LDO_EN,
			.enable_time = BD71847_LDO5_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_LDO5_VOLT,
			.mask = BD718XX_LDO_SEL,
			.val = BD718XX_LDO_SEL,
		},
	},
	{
		.desc = {
			.name = "ldo6",
			.of_match = of_match_ptr("LDO6"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_LDO6,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_LDO6_VOLTAGE_NUM,
			.linear_ranges = bd718xx_ldo6_volts,
			.n_linear_ranges = ARRAY_SIZE(bd718xx_ldo6_volts),
			 
			.supply_name = "buck5",
			.vsel_reg = BD718XX_REG_LDO6_VOLT,
			.vsel_mask = BD718XX_LDO6_MASK,
			.enable_reg = BD718XX_REG_LDO6_VOLT,
			.enable_mask = BD718XX_LDO_EN,
			.enable_time = BD71847_LDO6_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_LDO6_VOLT,
			.mask = BD718XX_LDO_SEL,
			.val = BD718XX_LDO_SEL,
		},
	},
};

static const struct regulator_ops *bd71837_swcontrol_ops[] = {
	&bd718xx_dvs_buck_regulator_ops, &bd718xx_dvs_buck_regulator_ops,
	&bd718xx_dvs_buck_regulator_ops, &bd718xx_dvs_buck_regulator_ops,
	&bd71837_pickable_range_buck_ops, &bd71837_buck_regulator_ops,
	&bd71837_buck_regulator_nolinear_ops, &bd71837_buck_regulator_ops,
	&bd71837_pickable_range_ldo_ops, &bd71837_ldo_regulator_nolinear_ops,
	&bd71837_ldo_regulator_ops, &bd71837_ldo_regulator_ops,
	&bd71837_ldo_regulator_ops, &bd71837_ldo_regulator_ops,
	&bd71837_ldo_regulator_ops,
};

static const struct regulator_ops *bd71837_hwcontrol_ops[] = {
	&BD718XX_HWOPNAME(bd718xx_dvs_buck_regulator_ops),
	&BD718XX_HWOPNAME(bd718xx_dvs_buck_regulator_ops),
	&bd71837_buck34_ops_hwctrl, &bd71837_buck34_ops_hwctrl,
	&BD718XX_HWOPNAME(bd71837_pickable_range_buck_ops),
	&BD718XX_HWOPNAME(bd71837_buck_regulator_ops),
	&BD718XX_HWOPNAME(bd71837_buck_regulator_nolinear_ops),
	&BD718XX_HWOPNAME(bd71837_buck_regulator_ops),
	&BD718XX_HWOPNAME(bd71837_pickable_range_ldo_ops),
	&BD718XX_HWOPNAME(bd71837_ldo_regulator_nolinear_ops),
	&BD718XX_HWOPNAME(bd71837_ldo_regulator_ops),
	&BD718XX_HWOPNAME(bd71837_ldo_regulator_ops),
	&BD718XX_HWOPNAME(bd71837_ldo_regulator_ops),
	&BD718XX_HWOPNAME(bd71837_ldo_regulator_ops),
	&BD718XX_HWOPNAME(bd71837_ldo_regulator_ops),
};

static struct bd718xx_regulator_data bd71837_regulators[] = {
	{
		.desc = {
			.name = "buck1",
			.of_match = of_match_ptr("BUCK1"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_BUCK1,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_DVS_BUCK_VOLTAGE_NUM,
			.linear_ranges = bd718xx_dvs_buck_volts,
			.n_linear_ranges = ARRAY_SIZE(bd718xx_dvs_buck_volts),
			.vsel_reg = BD718XX_REG_BUCK1_VOLT_RUN,
			.vsel_mask = DVS_BUCK_RUN_MASK,
			.enable_reg = BD718XX_REG_BUCK1_CTRL,
			.enable_mask = BD718XX_BUCK_EN,
			.enable_time = BD71837_BUCK1_STARTUP_TIME,
			.ramp_delay_table = bd718xx_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd718xx_ramp_delay),
			.ramp_reg = BD718XX_REG_BUCK1_CTRL,
			.ramp_mask = BUCK_RAMPRATE_MASK,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE |
				     ROHM_DVS_LEVEL_SUSPEND,
			.run_reg = BD718XX_REG_BUCK1_VOLT_RUN,
			.run_mask = DVS_BUCK_RUN_MASK,
			.idle_reg = BD718XX_REG_BUCK1_VOLT_IDLE,
			.idle_mask = DVS_BUCK_RUN_MASK,
			.suspend_reg = BD718XX_REG_BUCK1_VOLT_SUSP,
			.suspend_mask = DVS_BUCK_RUN_MASK,
		},
		.init = {
			.reg = BD718XX_REG_BUCK1_CTRL,
			.mask = BD718XX_BUCK_SEL,
			.val = BD718XX_BUCK_SEL,
		},
	},
	{
		.desc = {
			.name = "buck2",
			.of_match = of_match_ptr("BUCK2"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_BUCK2,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_DVS_BUCK_VOLTAGE_NUM,
			.linear_ranges = bd718xx_dvs_buck_volts,
			.n_linear_ranges = ARRAY_SIZE(bd718xx_dvs_buck_volts),
			.vsel_reg = BD718XX_REG_BUCK2_VOLT_RUN,
			.vsel_mask = DVS_BUCK_RUN_MASK,
			.enable_reg = BD718XX_REG_BUCK2_CTRL,
			.enable_mask = BD718XX_BUCK_EN,
			.enable_time = BD71837_BUCK2_STARTUP_TIME,
			.ramp_delay_table = bd718xx_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd718xx_ramp_delay),
			.ramp_reg = BD718XX_REG_BUCK2_CTRL,
			.ramp_mask = BUCK_RAMPRATE_MASK,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN | ROHM_DVS_LEVEL_IDLE,
			.run_reg = BD718XX_REG_BUCK2_VOLT_RUN,
			.run_mask = DVS_BUCK_RUN_MASK,
			.idle_reg = BD718XX_REG_BUCK2_VOLT_IDLE,
			.idle_mask = DVS_BUCK_RUN_MASK,
		},
		.init = {
			.reg = BD718XX_REG_BUCK2_CTRL,
			.mask = BD718XX_BUCK_SEL,
			.val = BD718XX_BUCK_SEL,
		},
	},
	{
		.desc = {
			.name = "buck3",
			.of_match = of_match_ptr("BUCK3"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_BUCK3,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_DVS_BUCK_VOLTAGE_NUM,
			.linear_ranges = bd718xx_dvs_buck_volts,
			.n_linear_ranges = ARRAY_SIZE(bd718xx_dvs_buck_volts),
			.vsel_reg = BD71837_REG_BUCK3_VOLT_RUN,
			.vsel_mask = DVS_BUCK_RUN_MASK,
			.enable_reg = BD71837_REG_BUCK3_CTRL,
			.enable_mask = BD718XX_BUCK_EN,
			.enable_time = BD71837_BUCK3_STARTUP_TIME,
			.ramp_delay_table = bd718xx_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd718xx_ramp_delay),
			.ramp_reg = BD71837_REG_BUCK3_CTRL,
			.ramp_mask = BUCK_RAMPRATE_MASK,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN,
			.run_reg = BD71837_REG_BUCK3_VOLT_RUN,
			.run_mask = DVS_BUCK_RUN_MASK,
		},
		.init = {
			.reg = BD71837_REG_BUCK3_CTRL,
			.mask = BD718XX_BUCK_SEL,
			.val = BD718XX_BUCK_SEL,
		},
	},
	{
		.desc = {
			.name = "buck4",
			.of_match = of_match_ptr("BUCK4"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_BUCK4,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_DVS_BUCK_VOLTAGE_NUM,
			.linear_ranges = bd718xx_dvs_buck_volts,
			.n_linear_ranges = ARRAY_SIZE(bd718xx_dvs_buck_volts),
			.vsel_reg = BD71837_REG_BUCK4_VOLT_RUN,
			.vsel_mask = DVS_BUCK_RUN_MASK,
			.enable_reg = BD71837_REG_BUCK4_CTRL,
			.enable_mask = BD718XX_BUCK_EN,
			.enable_time = BD71837_BUCK4_STARTUP_TIME,
			.ramp_delay_table = bd718xx_ramp_delay,
			.n_ramp_values = ARRAY_SIZE(bd718xx_ramp_delay),
			.ramp_reg = BD71837_REG_BUCK4_CTRL,
			.ramp_mask = BUCK_RAMPRATE_MASK,
			.owner = THIS_MODULE,
			.of_parse_cb = buck_set_hw_dvs_levels,
		},
		.dvs = {
			.level_map = ROHM_DVS_LEVEL_RUN,
			.run_reg = BD71837_REG_BUCK4_VOLT_RUN,
			.run_mask = DVS_BUCK_RUN_MASK,
		},
		.init = {
			.reg = BD71837_REG_BUCK4_CTRL,
			.mask = BD718XX_BUCK_SEL,
			.val = BD718XX_BUCK_SEL,
		},
	},
	{
		.desc = {
			.name = "buck5",
			.of_match = of_match_ptr("BUCK5"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_BUCK5,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD71837_BUCK5_VOLTAGE_NUM,
			.linear_ranges = bd71837_buck5_volts,
			.n_linear_ranges =
				ARRAY_SIZE(bd71837_buck5_volts),
			.vsel_reg = BD718XX_REG_1ST_NODVS_BUCK_VOLT,
			.vsel_mask = BD71837_BUCK5_MASK,
			.vsel_range_reg = BD718XX_REG_1ST_NODVS_BUCK_VOLT,
			.vsel_range_mask = BD71837_BUCK5_RANGE_MASK,
			.linear_range_selectors_bitfield = bd71837_buck5_volt_range_sel,
			.enable_reg = BD718XX_REG_1ST_NODVS_BUCK_CTRL,
			.enable_mask = BD718XX_BUCK_EN,
			.enable_time = BD71837_BUCK5_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_1ST_NODVS_BUCK_CTRL,
			.mask = BD718XX_BUCK_SEL,
			.val = BD718XX_BUCK_SEL,
		},
	},
	{
		.desc = {
			.name = "buck6",
			.of_match = of_match_ptr("BUCK6"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_BUCK6,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD71837_BUCK6_VOLTAGE_NUM,
			.linear_ranges = bd71837_buck6_volts,
			.n_linear_ranges =
				ARRAY_SIZE(bd71837_buck6_volts),
			.vsel_reg = BD718XX_REG_2ND_NODVS_BUCK_VOLT,
			.vsel_mask = BD71837_BUCK6_MASK,
			.enable_reg = BD718XX_REG_2ND_NODVS_BUCK_CTRL,
			.enable_mask = BD718XX_BUCK_EN,
			.enable_time = BD71837_BUCK6_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_2ND_NODVS_BUCK_CTRL,
			.mask = BD718XX_BUCK_SEL,
			.val = BD718XX_BUCK_SEL,
		},
	},
	{
		.desc = {
			.name = "buck7",
			.of_match = of_match_ptr("BUCK7"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_BUCK7,
			.type = REGULATOR_VOLTAGE,
			.volt_table = &bd718xx_3rd_nodvs_buck_volts[0],
			.n_voltages = ARRAY_SIZE(bd718xx_3rd_nodvs_buck_volts),
			.vsel_reg = BD718XX_REG_3RD_NODVS_BUCK_VOLT,
			.vsel_mask = BD718XX_3RD_NODVS_BUCK_MASK,
			.enable_reg = BD718XX_REG_3RD_NODVS_BUCK_CTRL,
			.enable_mask = BD718XX_BUCK_EN,
			.enable_time = BD71837_BUCK7_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_3RD_NODVS_BUCK_CTRL,
			.mask = BD718XX_BUCK_SEL,
			.val = BD718XX_BUCK_SEL,
		},
	},
	{
		.desc = {
			.name = "buck8",
			.of_match = of_match_ptr("BUCK8"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_BUCK8,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_4TH_NODVS_BUCK_VOLTAGE_NUM,
			.linear_ranges = bd718xx_4th_nodvs_buck_volts,
			.n_linear_ranges =
				ARRAY_SIZE(bd718xx_4th_nodvs_buck_volts),
			.vsel_reg = BD718XX_REG_4TH_NODVS_BUCK_VOLT,
			.vsel_mask = BD718XX_4TH_NODVS_BUCK_MASK,
			.enable_reg = BD718XX_REG_4TH_NODVS_BUCK_CTRL,
			.enable_mask = BD718XX_BUCK_EN,
			.enable_time = BD71837_BUCK8_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_4TH_NODVS_BUCK_CTRL,
			.mask = BD718XX_BUCK_SEL,
			.val = BD718XX_BUCK_SEL,
		},
	},
	{
		.desc = {
			.name = "ldo1",
			.of_match = of_match_ptr("LDO1"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_LDO1,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_LDO1_VOLTAGE_NUM,
			.linear_ranges = bd718xx_ldo1_volts,
			.n_linear_ranges = ARRAY_SIZE(bd718xx_ldo1_volts),
			.vsel_reg = BD718XX_REG_LDO1_VOLT,
			.vsel_mask = BD718XX_LDO1_MASK,
			.vsel_range_reg = BD718XX_REG_LDO1_VOLT,
			.vsel_range_mask = BD718XX_LDO1_RANGE_MASK,
			.linear_range_selectors_bitfield = bd718xx_ldo1_volt_range_sel,
			.enable_reg = BD718XX_REG_LDO1_VOLT,
			.enable_mask = BD718XX_LDO_EN,
			.enable_time = BD71837_LDO1_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_LDO1_VOLT,
			.mask = BD718XX_LDO_SEL,
			.val = BD718XX_LDO_SEL,
		},
	},
	{
		.desc = {
			.name = "ldo2",
			.of_match = of_match_ptr("LDO2"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_LDO2,
			.type = REGULATOR_VOLTAGE,
			.volt_table = &ldo_2_volts[0],
			.vsel_reg = BD718XX_REG_LDO2_VOLT,
			.vsel_mask = BD718XX_LDO2_MASK,
			.n_voltages = ARRAY_SIZE(ldo_2_volts),
			.enable_reg = BD718XX_REG_LDO2_VOLT,
			.enable_mask = BD718XX_LDO_EN,
			.enable_time = BD71837_LDO2_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_LDO2_VOLT,
			.mask = BD718XX_LDO_SEL,
			.val = BD718XX_LDO_SEL,
		},
	},
	{
		.desc = {
			.name = "ldo3",
			.of_match = of_match_ptr("LDO3"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_LDO3,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_LDO3_VOLTAGE_NUM,
			.linear_ranges = bd718xx_ldo3_volts,
			.n_linear_ranges = ARRAY_SIZE(bd718xx_ldo3_volts),
			.vsel_reg = BD718XX_REG_LDO3_VOLT,
			.vsel_mask = BD718XX_LDO3_MASK,
			.enable_reg = BD718XX_REG_LDO3_VOLT,
			.enable_mask = BD718XX_LDO_EN,
			.enable_time = BD71837_LDO3_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_LDO3_VOLT,
			.mask = BD718XX_LDO_SEL,
			.val = BD718XX_LDO_SEL,
		},
	},
	{
		.desc = {
			.name = "ldo4",
			.of_match = of_match_ptr("LDO4"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_LDO4,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_LDO4_VOLTAGE_NUM,
			.linear_ranges = bd718xx_ldo4_volts,
			.n_linear_ranges = ARRAY_SIZE(bd718xx_ldo4_volts),
			.vsel_reg = BD718XX_REG_LDO4_VOLT,
			.vsel_mask = BD718XX_LDO4_MASK,
			.enable_reg = BD718XX_REG_LDO4_VOLT,
			.enable_mask = BD718XX_LDO_EN,
			.enable_time = BD71837_LDO4_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_LDO4_VOLT,
			.mask = BD718XX_LDO_SEL,
			.val = BD718XX_LDO_SEL,
		},
	},
	{
		.desc = {
			.name = "ldo5",
			.of_match = of_match_ptr("LDO5"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_LDO5,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD71837_LDO5_VOLTAGE_NUM,
			.linear_ranges = bd71837_ldo5_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71837_ldo5_volts),
			 
			.supply_name = "buck6",
			.vsel_reg = BD718XX_REG_LDO5_VOLT,
			.vsel_mask = BD71837_LDO5_MASK,
			.enable_reg = BD718XX_REG_LDO5_VOLT,
			.enable_mask = BD718XX_LDO_EN,
			.enable_time = BD71837_LDO5_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_LDO5_VOLT,
			.mask = BD718XX_LDO_SEL,
			.val = BD718XX_LDO_SEL,
		},
		.additional_inits = bd71837_ldo5_inits,
		.additional_init_amnt = ARRAY_SIZE(bd71837_ldo5_inits),
	},
	{
		.desc = {
			.name = "ldo6",
			.of_match = of_match_ptr("LDO6"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_LDO6,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD718XX_LDO6_VOLTAGE_NUM,
			.linear_ranges = bd718xx_ldo6_volts,
			.n_linear_ranges = ARRAY_SIZE(bd718xx_ldo6_volts),
			 
			.supply_name = "buck7",
			.vsel_reg = BD718XX_REG_LDO6_VOLT,
			.vsel_mask = BD718XX_LDO6_MASK,
			.enable_reg = BD718XX_REG_LDO6_VOLT,
			.enable_mask = BD718XX_LDO_EN,
			.enable_time = BD71837_LDO6_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD718XX_REG_LDO6_VOLT,
			.mask = BD718XX_LDO_SEL,
			.val = BD718XX_LDO_SEL,
		},
		.additional_inits = bd71837_ldo6_inits,
		.additional_init_amnt = ARRAY_SIZE(bd71837_ldo6_inits),
	},
	{
		.desc = {
			.name = "ldo7",
			.of_match = of_match_ptr("LDO7"),
			.regulators_node = of_match_ptr("regulators"),
			.id = BD718XX_LDO7,
			.type = REGULATOR_VOLTAGE,
			.n_voltages = BD71837_LDO7_VOLTAGE_NUM,
			.linear_ranges = bd71837_ldo7_volts,
			.n_linear_ranges = ARRAY_SIZE(bd71837_ldo7_volts),
			.vsel_reg = BD71837_REG_LDO7_VOLT,
			.vsel_mask = BD71837_LDO7_MASK,
			.enable_reg = BD71837_REG_LDO7_VOLT,
			.enable_mask = BD718XX_LDO_EN,
			.enable_time = BD71837_LDO7_STARTUP_TIME,
			.owner = THIS_MODULE,
		},
		.init = {
			.reg = BD71837_REG_LDO7_VOLT,
			.mask = BD718XX_LDO_SEL,
			.val = BD718XX_LDO_SEL,
		},
	},
};

static void mark_hw_controlled(struct device *dev, struct device_node *np,
			       struct bd718xx_regulator_data *reg_data,
			       unsigned int num_reg_data, int *info)
{
	int i;

	for (i = 1; i <= num_reg_data; i++) {
		if (!of_node_name_eq(np, reg_data[i-1].desc.of_match))
			continue;

		*info |= 1 << (i - 1);
		dev_dbg(dev, "regulator %d runlevel controlled\n", i);
		return;
	}
	dev_warn(dev, "Bad regulator node\n");
}

 
static int setup_feedback_loop(struct device *dev, struct device_node *np,
			       struct bd718xx_regulator_data *reg_data,
			       unsigned int num_reg_data, int fb_uv)
{
	int i, r1, r2, ret;

	 

	for (i = 0; i < num_reg_data; i++) {
		struct regulator_desc *desc = &reg_data[i].desc;
		int j;

		if (!of_node_name_eq(np, desc->of_match))
			continue;

		 
		if (desc->id >= BD718XX_LDO1)
			return -EINVAL;

		ret = of_property_read_u32(np, "rohm,feedback-pull-up-r1-ohms",
					   &r1);
		if (ret)
			return ret;

		if (!r1)
			return -EINVAL;

		ret = of_property_read_u32(np, "rohm,feedback-pull-up-r2-ohms",
					   &r2);
		if (ret)
			return ret;

		if (desc->n_linear_ranges && desc->linear_ranges) {
			struct linear_range *new;

			new = devm_kzalloc(dev, desc->n_linear_ranges *
					   sizeof(struct linear_range),
					   GFP_KERNEL);
			if (!new)
				return -ENOMEM;

			for (j = 0; j < desc->n_linear_ranges; j++) {
				int min = desc->linear_ranges[j].min;
				int step = desc->linear_ranges[j].step;

				min -= (fb_uv - min)*r2/r1;
				step = step * (r1 + r2);
				step /= r1;

				new[j].min = min;
				new[j].step = step;

				dev_dbg(dev, "%s: old range min %d, step %d\n",
					desc->name, desc->linear_ranges[j].min,
					desc->linear_ranges[j].step);
				dev_dbg(dev, "new range min %d, step %d\n", min,
					step);
			}
			desc->linear_ranges = new;
		}
		dev_dbg(dev, "regulator '%s' has FB pull-up configured\n",
			desc->name);

		return 0;
	}

	return -ENODEV;
}

static int get_special_regulators(struct device *dev,
				  struct bd718xx_regulator_data *reg_data,
				  unsigned int num_reg_data, int *info)
{
	int ret;
	struct device_node *np;
	struct device_node *nproot = dev->of_node;
	int uv;

	*info = 0;

	nproot = of_get_child_by_name(nproot, "regulators");
	if (!nproot) {
		dev_err(dev, "failed to find regulators node\n");
		return -ENODEV;
	}
	for_each_child_of_node(nproot, np) {
		if (of_property_read_bool(np, "rohm,no-regulator-enable-control"))
			mark_hw_controlled(dev, np, reg_data, num_reg_data,
					   info);
		ret = of_property_read_u32(np, "rohm,fb-pull-up-microvolt",
					   &uv);
		if (ret) {
			if (ret == -EINVAL)
				continue;
			else
				goto err_out;
		}

		ret = setup_feedback_loop(dev, np, reg_data, num_reg_data, uv);
		if (ret)
			goto err_out;
	}

	of_node_put(nproot);
	return 0;

err_out:
	of_node_put(np);
	of_node_put(nproot);

	return ret;
}

static int bd718xx_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	struct regulator_config config = { 0 };
	int i, j, err, omit_enable;
	bool use_snvs;
	struct bd718xx_regulator_data *reg_data;
	unsigned int num_reg_data;
	enum rohm_chip_type chip = platform_get_device_id(pdev)->driver_data;
	const struct regulator_ops **swops, **hwops;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		dev_err(&pdev->dev, "No MFD driver data\n");
		return -EINVAL;
	}

	switch (chip) {
	case ROHM_CHIP_TYPE_BD71837:
		reg_data = bd71837_regulators;
		num_reg_data = ARRAY_SIZE(bd71837_regulators);
		swops = &bd71837_swcontrol_ops[0];
		hwops = &bd71837_hwcontrol_ops[0];
		break;
	case ROHM_CHIP_TYPE_BD71847:
		reg_data = bd71847_regulators;
		num_reg_data = ARRAY_SIZE(bd71847_regulators);
		swops = &bd71847_swcontrol_ops[0];
		hwops = &bd71847_hwcontrol_ops[0];
		break;
	default:
		dev_err(&pdev->dev, "Unsupported chip type\n");
		return -EINVAL;
	}

	 
	err = regmap_update_bits(regmap, BD718XX_REG_REGLOCK,
				 (REGLOCK_PWRSEQ | REGLOCK_VREG), 0);
	if (err)
		return dev_err_probe(&pdev->dev, err, "Failed to unlock PMIC\n");

	dev_dbg(&pdev->dev, "Unlocked lock register 0x%x\n",
		BD718XX_REG_REGLOCK);

	use_snvs = of_property_read_bool(pdev->dev.parent->of_node,
					 "rohm,reset-snvs-powered");

	 
	if (!use_snvs) {
		err = regmap_update_bits(regmap, BD718XX_REG_TRANS_COND1,
					 BD718XX_ON_REQ_POWEROFF_MASK |
					 BD718XX_SWRESET_POWEROFF_MASK |
					 BD718XX_WDOG_POWEROFF_MASK |
					 BD718XX_KEY_L_POWEROFF_MASK,
					 BD718XX_POWOFF_TO_RDY);
		if (err)
			return dev_err_probe(&pdev->dev, err,
					     "Failed to change reset target\n");

		dev_dbg(&pdev->dev, "Changed all resets from SVNS to READY\n");
	}

	config.dev = pdev->dev.parent;
	config.regmap = regmap;
	 
	err = get_special_regulators(pdev->dev.parent, reg_data, num_reg_data,
				     &omit_enable);
	if (err)
		return err;

	for (i = 0; i < num_reg_data; i++) {

		struct regulator_desc *desc;
		struct regulator_dev *rdev;
		struct bd718xx_regulator_data *r;
		int no_enable_control = omit_enable & (1 << i);

		r = &reg_data[i];
		desc = &r->desc;

		if (no_enable_control)
			desc->ops = hwops[i];
		else
			desc->ops = swops[i];

		rdev = devm_regulator_register(&pdev->dev, desc, &config);
		if (IS_ERR(rdev))
			return dev_err_probe(&pdev->dev, PTR_ERR(rdev),
					     "failed to register %s regulator\n",
					     desc->name);

		 
		if (!no_enable_control && (!use_snvs ||
		    !rdev->constraints->always_on ||
		    !rdev->constraints->boot_on)) {
			err = regmap_update_bits(regmap, r->init.reg,
						 r->init.mask, r->init.val);
			if (err)
				return dev_err_probe(&pdev->dev, err,
					"Failed to take control for (%s)\n",
					desc->name);
		}
		for (j = 0; j < r->additional_init_amnt; j++) {
			err = regmap_update_bits(regmap,
						 r->additional_inits[j].reg,
						 r->additional_inits[j].mask,
						 r->additional_inits[j].val);
			if (err)
				return dev_err_probe(&pdev->dev, err,
					"Buck (%s) initialization failed\n",
					desc->name);
		}
	}

	return err;
}

static const struct platform_device_id bd718x7_pmic_id[] = {
	{ "bd71837-pmic", ROHM_CHIP_TYPE_BD71837 },
	{ "bd71847-pmic", ROHM_CHIP_TYPE_BD71847 },
	{ },
};
MODULE_DEVICE_TABLE(platform, bd718x7_pmic_id);

static struct platform_driver bd718xx_regulator = {
	.driver = {
		.name = "bd718xx-pmic",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe = bd718xx_probe,
	.id_table = bd718x7_pmic_id,
};

module_platform_driver(bd718xx_regulator);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("BD71837/BD71847 voltage regulator driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:bd718xx-pmic");
