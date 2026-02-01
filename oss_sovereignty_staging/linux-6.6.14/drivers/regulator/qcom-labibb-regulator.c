


#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>

#define REG_PERPH_TYPE                  0x04

#define QCOM_LAB_TYPE			0x24
#define QCOM_IBB_TYPE			0x20

#define PMI8998_LAB_REG_BASE		0xde00
#define PMI8998_IBB_REG_BASE		0xdc00
#define PMI8998_IBB_LAB_REG_OFFSET	0x200

#define REG_LABIBB_STATUS1		0x08
 #define LABIBB_STATUS1_SC_BIT		BIT(6)
 #define LABIBB_STATUS1_VREG_OK_BIT	BIT(7)

#define REG_LABIBB_INT_SET_TYPE		0x11
#define REG_LABIBB_INT_POLARITY_HIGH	0x12
#define REG_LABIBB_INT_POLARITY_LOW	0x13
#define REG_LABIBB_INT_LATCHED_CLR	0x14
#define REG_LABIBB_INT_EN_SET		0x15
#define REG_LABIBB_INT_EN_CLR		0x16
 #define LABIBB_INT_VREG_OK		BIT(0)
 #define LABIBB_INT_VREG_TYPE_LEVEL	0

#define REG_LABIBB_VOLTAGE		0x41
 #define LABIBB_VOLTAGE_OVERRIDE_EN	BIT(7)
 #define LAB_VOLTAGE_SET_MASK		GENMASK(3, 0)
 #define IBB_VOLTAGE_SET_MASK		GENMASK(5, 0)

#define REG_LABIBB_ENABLE_CTL		0x46
 #define LABIBB_CONTROL_ENABLE		BIT(7)

#define REG_LABIBB_PD_CTL		0x47
 #define LAB_PD_CTL_MASK		GENMASK(1, 0)
 #define IBB_PD_CTL_MASK		(BIT(0) | BIT(7))
 #define LAB_PD_CTL_STRONG_PULL		BIT(0)
 #define IBB_PD_CTL_HALF_STRENGTH	BIT(0)
 #define IBB_PD_CTL_EN			BIT(7)

#define REG_LABIBB_CURRENT_LIMIT	0x4b
 #define LAB_CURRENT_LIMIT_MASK		GENMASK(2, 0)
 #define IBB_CURRENT_LIMIT_MASK		GENMASK(4, 0)
 #define LAB_CURRENT_LIMIT_OVERRIDE_EN	BIT(3)
 #define LABIBB_CURRENT_LIMIT_EN	BIT(7)

#define REG_IBB_PWRUP_PWRDN_CTL_1	0x58
 #define IBB_CTL_1_DISCHARGE_EN		BIT(2)

#define REG_LABIBB_SOFT_START_CTL	0x5f
#define REG_LABIBB_SEC_ACCESS		0xd0
 #define LABIBB_SEC_UNLOCK_CODE		0xa5

#define LAB_ENABLE_CTL_MASK		BIT(7)
#define IBB_ENABLE_CTL_MASK		(BIT(7) | BIT(6))

#define LABIBB_OFF_ON_DELAY		1000
#define LAB_ENABLE_TIME			(LABIBB_OFF_ON_DELAY * 2)
#define IBB_ENABLE_TIME			(LABIBB_OFF_ON_DELAY * 10)
#define LABIBB_POLL_ENABLED_TIME	1000
#define OCP_RECOVERY_INTERVAL_MS	500
#define SC_RECOVERY_INTERVAL_MS		250
#define LABIBB_MAX_OCP_COUNT		4
#define LABIBB_MAX_SC_COUNT		3
#define LABIBB_MAX_FATAL_COUNT		2

struct labibb_current_limits {
	u32				uA_min;
	u32				uA_step;
	u8				ovr_val;
};

struct labibb_regulator {
	struct regulator_desc		desc;
	struct device			*dev;
	struct regmap			*regmap;
	struct regulator_dev		*rdev;
	struct labibb_current_limits	uA_limits;
	struct delayed_work		ocp_recovery_work;
	struct delayed_work		sc_recovery_work;
	u16				base;
	u8				type;
	u8				dischg_sel;
	u8				soft_start_sel;
	int				sc_irq;
	int				sc_count;
	int				ocp_irq;
	int				ocp_irq_count;
	int				fatal_count;
};

struct labibb_regulator_data {
	const char			*name;
	u8				type;
	u16				base;
	const struct regulator_desc	*desc;
};

static int qcom_labibb_ocp_hw_enable(struct regulator_dev *rdev)
{
	struct labibb_regulator *vreg = rdev_get_drvdata(rdev);
	int ret;

	 
	ret = regmap_update_bits(rdev->regmap,
				 vreg->base + REG_LABIBB_INT_LATCHED_CLR,
				 LABIBB_INT_VREG_OK, 1);
	if (ret)
		return ret;

	 
	return regmap_update_bits(rdev->regmap,
				  vreg->base + REG_LABIBB_INT_EN_SET,
				  LABIBB_INT_VREG_OK, 1);
}

static int qcom_labibb_ocp_hw_disable(struct regulator_dev *rdev)
{
	struct labibb_regulator *vreg = rdev_get_drvdata(rdev);

	return regmap_update_bits(rdev->regmap,
				  vreg->base + REG_LABIBB_INT_EN_CLR,
				  LABIBB_INT_VREG_OK, 1);
}

 
static int qcom_labibb_check_ocp_status(struct labibb_regulator *vreg)
{
	u32 cur_status;
	int ret;

	ret = regmap_read(vreg->rdev->regmap, vreg->base + REG_LABIBB_STATUS1,
			  &cur_status);
	if (ret)
		return ret;

	return !(cur_status & LABIBB_STATUS1_VREG_OK_BIT);
}

 
static void qcom_labibb_ocp_recovery_worker(struct work_struct *work)
{
	struct labibb_regulator *vreg;
	const struct regulator_ops *ops;
	int ret;

	vreg = container_of(work, struct labibb_regulator,
			    ocp_recovery_work.work);
	ops = vreg->rdev->desc->ops;

	if (vreg->ocp_irq_count >= LABIBB_MAX_OCP_COUNT) {
		 
		BUG_ON(vreg->fatal_count > LABIBB_MAX_FATAL_COUNT);
		dev_err(&vreg->rdev->dev, "LABIBB: CRITICAL: Disabling regulator\n");

		 
		ret = ops->disable(vreg->rdev);
		if (ret) {
			vreg->fatal_count++;
			goto reschedule;
		}
		enable_irq(vreg->ocp_irq);
		vreg->fatal_count = 0;
		return;
	}

	ret = qcom_labibb_check_ocp_status(vreg);
	if (ret != 0) {
		vreg->ocp_irq_count++;
		goto reschedule;
	}

	ret = qcom_labibb_ocp_hw_enable(vreg->rdev);
	if (ret) {
		 
		dev_err(vreg->dev, "Cannot enable OCP IRQ\n");
		vreg->ocp_irq_count++;
		goto reschedule;
	}

	enable_irq(vreg->ocp_irq);
	 
	vreg->ocp_irq_count = 0;
	return;

reschedule:
	mod_delayed_work(system_wq, &vreg->ocp_recovery_work,
			 msecs_to_jiffies(OCP_RECOVERY_INTERVAL_MS));
}

 
static irqreturn_t qcom_labibb_ocp_isr(int irq, void *chip)
{
	struct labibb_regulator *vreg = chip;
	const struct regulator_ops *ops = vreg->rdev->desc->ops;
	int ret;

	 
	if (!ops->is_enabled(vreg->rdev))
		return IRQ_HANDLED;

	 
	if (vreg->ocp_irq_count > LABIBB_MAX_OCP_COUNT)
		return IRQ_NONE;

	 
	ret = qcom_labibb_check_ocp_status(vreg);
	if (ret == 0) {
		vreg->ocp_irq_count = 0;
		goto end;
	}
	vreg->ocp_irq_count++;

	 
	disable_irq_nosync(irq);

	 
	dev_warn(vreg->dev, "Over-Current interrupt fired!\n");

	 
	ret = qcom_labibb_ocp_hw_disable(vreg->rdev);
	if (ret)
		goto end;

	 
	regulator_notifier_call_chain(vreg->rdev,
				      REGULATOR_EVENT_OVER_CURRENT, NULL);

end:
	 
	schedule_delayed_work(&vreg->ocp_recovery_work,
			      msecs_to_jiffies(OCP_RECOVERY_INTERVAL_MS));
	if (ret)
		return IRQ_NONE;

	return IRQ_HANDLED;
}

static int qcom_labibb_set_ocp(struct regulator_dev *rdev, int lim,
			       int severity, bool enable)
{
	struct labibb_regulator *vreg = rdev_get_drvdata(rdev);
	char *ocp_irq_name;
	u32 irq_flags = IRQF_ONESHOT;
	int irq_trig_low, ret;

	 
	if (lim || severity != REGULATOR_SEVERITY_PROT || !enable)
		return -EINVAL;

	 
	if (vreg->ocp_irq <= 0)
		return -EINVAL;

	ocp_irq_name = devm_kasprintf(vreg->dev, GFP_KERNEL, "%s-over-current",
				      vreg->desc.name);
	if (!ocp_irq_name)
		return -ENOMEM;

	 
	switch (vreg->type) {
	case QCOM_LAB_TYPE:
		irq_flags |= IRQF_TRIGGER_LOW;
		irq_trig_low = 1;
		break;
	case QCOM_IBB_TYPE:
		irq_flags |= IRQF_TRIGGER_HIGH;
		irq_trig_low = 0;
		break;
	default:
		return -EINVAL;
	}

	 
	ret = regmap_update_bits(rdev->regmap,
				 vreg->base + REG_LABIBB_INT_SET_TYPE,
				 LABIBB_INT_VREG_OK,
				 LABIBB_INT_VREG_TYPE_LEVEL);
	if (ret)
		return ret;

	 
	ret = regmap_update_bits(rdev->regmap,
				 vreg->base + REG_LABIBB_INT_POLARITY_HIGH,
				 LABIBB_INT_VREG_OK, !irq_trig_low);
	if (ret)
		return ret;
	ret = regmap_update_bits(rdev->regmap,
				 vreg->base + REG_LABIBB_INT_POLARITY_LOW,
				 LABIBB_INT_VREG_OK, irq_trig_low);
	if (ret)
		return ret;

	ret = qcom_labibb_ocp_hw_enable(rdev);
	if (ret)
		return ret;

	return devm_request_threaded_irq(vreg->dev, vreg->ocp_irq, NULL,
					 qcom_labibb_ocp_isr, irq_flags,
					 ocp_irq_name, vreg);
}

 
static int qcom_labibb_check_sc_status(struct labibb_regulator *vreg)
{
	u32 ibb_status, ibb_reg, lab_status, lab_reg;
	int ret;

	 
	lab_reg = ibb_reg = vreg->base + REG_LABIBB_STATUS1;
	if (vreg->type == QCOM_LAB_TYPE)
		ibb_reg -= PMI8998_IBB_LAB_REG_OFFSET;
	else
		lab_reg += PMI8998_IBB_LAB_REG_OFFSET;

	ret = regmap_read(vreg->rdev->regmap, lab_reg, &lab_status);
	if (ret)
		return ret;
	ret = regmap_read(vreg->rdev->regmap, ibb_reg, &ibb_status);
	if (ret)
		return ret;

	return !!(lab_status & LABIBB_STATUS1_SC_BIT) ||
	       !!(ibb_status & LABIBB_STATUS1_SC_BIT);
}

 
static void qcom_labibb_sc_recovery_worker(struct work_struct *work)
{
	struct labibb_regulator *vreg;
	const struct regulator_ops *ops;
	u32 lab_reg, ibb_reg, lab_val, ibb_val, val;
	bool pbs_cut = false;
	int i, sc, ret;

	vreg = container_of(work, struct labibb_regulator,
			    sc_recovery_work.work);
	ops = vreg->rdev->desc->ops;

	 
	if (vreg->fatal_count > LABIBB_MAX_FATAL_COUNT)
		return;

	 
	if (vreg->sc_count > LABIBB_MAX_SC_COUNT)
		return;

	 
	lab_reg = ibb_reg = vreg->base + REG_LABIBB_ENABLE_CTL;
	if (vreg->type == QCOM_LAB_TYPE)
		ibb_reg -= PMI8998_IBB_LAB_REG_OFFSET;
	else
		lab_reg += PMI8998_IBB_LAB_REG_OFFSET;

	sc = qcom_labibb_check_sc_status(vreg);
	if (sc)
		goto reschedule;

	for (i = 0; i < LABIBB_MAX_SC_COUNT; i++) {
		ret = regmap_read(vreg->regmap, lab_reg, &lab_val);
		if (ret) {
			vreg->fatal_count++;
			goto reschedule;
		}

		ret = regmap_read(vreg->regmap, ibb_reg, &ibb_val);
		if (ret) {
			vreg->fatal_count++;
			goto reschedule;
		}
		val = lab_val & ibb_val;

		if (!(val & LABIBB_CONTROL_ENABLE)) {
			pbs_cut = true;
			break;
		}
		usleep_range(5000, 6000);
	}
	if (pbs_cut)
		goto reschedule;


	 
	ret = ops->enable(vreg->rdev);
	if (ret)
		goto reschedule;

	 
	vreg->sc_count = 0;
	enable_irq(vreg->sc_irq);
	return;

reschedule:
	 
	vreg->sc_count++;
	mod_delayed_work(system_wq, &vreg->sc_recovery_work,
			 msecs_to_jiffies(SC_RECOVERY_INTERVAL_MS));
}

 
static irqreturn_t qcom_labibb_sc_isr(int irq, void *chip)
{
	struct labibb_regulator *vreg = chip;

	if (vreg->sc_count > LABIBB_MAX_SC_COUNT)
		return IRQ_NONE;

	 
	dev_warn(vreg->dev, "Short-Circuit interrupt fired!\n");

	 
	disable_irq_nosync(irq);

	 
	regulator_notifier_call_chain(vreg->rdev,
				      REGULATOR_EVENT_REGULATION_OUT, NULL);

	 
	mod_delayed_work(system_highpri_wq, &vreg->sc_recovery_work,
			 msecs_to_jiffies(SC_RECOVERY_INTERVAL_MS));
	return IRQ_HANDLED;
}


static int qcom_labibb_set_current_limit(struct regulator_dev *rdev,
					 int min_uA, int max_uA)
{
	struct labibb_regulator *vreg = rdev_get_drvdata(rdev);
	struct regulator_desc *desc = &vreg->desc;
	struct labibb_current_limits *lim = &vreg->uA_limits;
	u32 mask, val;
	int i, ret, sel = -1;

	if (min_uA < lim->uA_min || max_uA < lim->uA_min)
		return -EINVAL;

	for (i = 0; i < desc->n_current_limits; i++) {
		int uA_limit = (lim->uA_step * i) + lim->uA_min;

		if (max_uA >= uA_limit && min_uA <= uA_limit)
			sel = i;
	}
	if (sel < 0)
		return -EINVAL;

	 
	ret = regmap_write(vreg->regmap, vreg->base + REG_LABIBB_SEC_ACCESS,
			   LABIBB_SEC_UNLOCK_CODE);
	if (ret)
		return ret;

	mask = desc->csel_mask | lim->ovr_val;
	mask |= LABIBB_CURRENT_LIMIT_EN;
	val = (u32)sel | lim->ovr_val;
	val |= LABIBB_CURRENT_LIMIT_EN;

	return regmap_update_bits(vreg->regmap, desc->csel_reg, mask, val);
}

static int qcom_labibb_get_current_limit(struct regulator_dev *rdev)
{
	struct labibb_regulator *vreg = rdev_get_drvdata(rdev);
	struct regulator_desc *desc = &vreg->desc;
	struct labibb_current_limits *lim = &vreg->uA_limits;
	unsigned int cur_step;
	int ret;

	ret = regmap_read(vreg->regmap, desc->csel_reg, &cur_step);
	if (ret)
		return ret;
	cur_step &= desc->csel_mask;

	return (cur_step * lim->uA_step) + lim->uA_min;
}

static int qcom_labibb_set_soft_start(struct regulator_dev *rdev)
{
	struct labibb_regulator *vreg = rdev_get_drvdata(rdev);
	u32 val = 0;

	if (vreg->type == QCOM_IBB_TYPE)
		val = vreg->dischg_sel;
	else
		val = vreg->soft_start_sel;

	return regmap_write(rdev->regmap, rdev->desc->soft_start_reg, val);
}

static int qcom_labibb_get_table_sel(const int *table, int sz, u32 value)
{
	int i;

	for (i = 0; i < sz; i++)
		if (table[i] == value)
			return i;
	return -EINVAL;
}

 
static const int dischg_resistor_values[] = { 300, 64, 32, 16 };

 
static const int soft_start_values[] = { 200, 400, 600, 800 };

static int qcom_labibb_of_parse_cb(struct device_node *np,
				   const struct regulator_desc *desc,
				   struct regulator_config *config)
{
	struct labibb_regulator *vreg = config->driver_data;
	u32 dischg_kohms, soft_start_time;
	int ret;

	ret = of_property_read_u32(np, "qcom,discharge-resistor-kohms",
				       &dischg_kohms);
	if (ret)
		dischg_kohms = 300;

	ret = qcom_labibb_get_table_sel(dischg_resistor_values,
					ARRAY_SIZE(dischg_resistor_values),
					dischg_kohms);
	if (ret < 0)
		return ret;
	vreg->dischg_sel = (u8)ret;

	ret = of_property_read_u32(np, "qcom,soft-start-us",
				   &soft_start_time);
	if (ret)
		soft_start_time = 200;

	ret = qcom_labibb_get_table_sel(soft_start_values,
					ARRAY_SIZE(soft_start_values),
					soft_start_time);
	if (ret < 0)
		return ret;
	vreg->soft_start_sel = (u8)ret;

	return 0;
}

static const struct regulator_ops qcom_labibb_ops = {
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.set_active_discharge	= regulator_set_active_discharge_regmap,
	.set_pull_down		= regulator_set_pull_down_regmap,
	.set_current_limit	= qcom_labibb_set_current_limit,
	.get_current_limit	= qcom_labibb_get_current_limit,
	.set_soft_start		= qcom_labibb_set_soft_start,
	.set_over_current_protection = qcom_labibb_set_ocp,
};

static const struct regulator_desc pmi8998_lab_desc = {
	.enable_mask		= LAB_ENABLE_CTL_MASK,
	.enable_reg		= (PMI8998_LAB_REG_BASE + REG_LABIBB_ENABLE_CTL),
	.enable_val		= LABIBB_CONTROL_ENABLE,
	.enable_time		= LAB_ENABLE_TIME,
	.poll_enabled_time	= LABIBB_POLL_ENABLED_TIME,
	.soft_start_reg		= (PMI8998_LAB_REG_BASE + REG_LABIBB_SOFT_START_CTL),
	.pull_down_reg		= (PMI8998_LAB_REG_BASE + REG_LABIBB_PD_CTL),
	.pull_down_mask		= LAB_PD_CTL_MASK,
	.pull_down_val_on	= LAB_PD_CTL_STRONG_PULL,
	.vsel_reg		= (PMI8998_LAB_REG_BASE + REG_LABIBB_VOLTAGE),
	.vsel_mask		= LAB_VOLTAGE_SET_MASK,
	.apply_reg		= (PMI8998_LAB_REG_BASE + REG_LABIBB_VOLTAGE),
	.apply_bit		= LABIBB_VOLTAGE_OVERRIDE_EN,
	.csel_reg		= (PMI8998_LAB_REG_BASE + REG_LABIBB_CURRENT_LIMIT),
	.csel_mask		= LAB_CURRENT_LIMIT_MASK,
	.n_current_limits	= 8,
	.off_on_delay		= LABIBB_OFF_ON_DELAY,
	.owner			= THIS_MODULE,
	.type			= REGULATOR_VOLTAGE,
	.min_uV			= 4600000,
	.uV_step		= 100000,
	.n_voltages		= 16,
	.ops			= &qcom_labibb_ops,
	.of_parse_cb		= qcom_labibb_of_parse_cb,
};

static const struct regulator_desc pmi8998_ibb_desc = {
	.enable_mask		= IBB_ENABLE_CTL_MASK,
	.enable_reg		= (PMI8998_IBB_REG_BASE + REG_LABIBB_ENABLE_CTL),
	.enable_val		= LABIBB_CONTROL_ENABLE,
	.enable_time		= IBB_ENABLE_TIME,
	.poll_enabled_time	= LABIBB_POLL_ENABLED_TIME,
	.soft_start_reg		= (PMI8998_IBB_REG_BASE + REG_LABIBB_SOFT_START_CTL),
	.active_discharge_off	= 0,
	.active_discharge_on	= IBB_CTL_1_DISCHARGE_EN,
	.active_discharge_mask	= IBB_CTL_1_DISCHARGE_EN,
	.active_discharge_reg	= (PMI8998_IBB_REG_BASE + REG_IBB_PWRUP_PWRDN_CTL_1),
	.pull_down_reg		= (PMI8998_IBB_REG_BASE + REG_LABIBB_PD_CTL),
	.pull_down_mask		= IBB_PD_CTL_MASK,
	.pull_down_val_on	= IBB_PD_CTL_HALF_STRENGTH | IBB_PD_CTL_EN,
	.vsel_reg		= (PMI8998_IBB_REG_BASE + REG_LABIBB_VOLTAGE),
	.vsel_mask		= IBB_VOLTAGE_SET_MASK,
	.apply_reg		= (PMI8998_IBB_REG_BASE + REG_LABIBB_VOLTAGE),
	.apply_bit		= LABIBB_VOLTAGE_OVERRIDE_EN,
	.csel_reg		= (PMI8998_IBB_REG_BASE + REG_LABIBB_CURRENT_LIMIT),
	.csel_mask		= IBB_CURRENT_LIMIT_MASK,
	.n_current_limits	= 32,
	.off_on_delay		= LABIBB_OFF_ON_DELAY,
	.owner			= THIS_MODULE,
	.type			= REGULATOR_VOLTAGE,
	.min_uV			= 1400000,
	.uV_step		= 100000,
	.n_voltages		= 64,
	.ops			= &qcom_labibb_ops,
	.of_parse_cb		= qcom_labibb_of_parse_cb,
};

static const struct labibb_regulator_data pmi8998_labibb_data[] = {
	{"lab", QCOM_LAB_TYPE, PMI8998_LAB_REG_BASE, &pmi8998_lab_desc},
	{"ibb", QCOM_IBB_TYPE, PMI8998_IBB_REG_BASE, &pmi8998_ibb_desc},
	{ },
};

static const struct of_device_id qcom_labibb_match[] = {
	{ .compatible = "qcom,pmi8998-lab-ibb", .data = &pmi8998_labibb_data},
	{ },
};
MODULE_DEVICE_TABLE(of, qcom_labibb_match);

static int qcom_labibb_regulator_probe(struct platform_device *pdev)
{
	struct labibb_regulator *vreg;
	struct device *dev = &pdev->dev;
	struct regulator_config cfg = {};
	struct device_node *reg_node;
	const struct of_device_id *match;
	const struct labibb_regulator_data *reg_data;
	struct regmap *reg_regmap;
	unsigned int type;
	int ret;

	reg_regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!reg_regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -ENODEV;
	}

	match = of_match_device(qcom_labibb_match, &pdev->dev);
	if (!match)
		return -ENODEV;

	for (reg_data = match->data; reg_data->name; reg_data++) {
		char *sc_irq_name;
		int irq = 0;

		 
		ret = regmap_read(reg_regmap, reg_data->base + REG_PERPH_TYPE,
				  &type);
		if (ret < 0) {
			dev_err(dev,
				"Peripheral type read failed ret=%d\n",
				ret);
			return -EINVAL;
		}

		if (WARN_ON((type != QCOM_LAB_TYPE) && (type != QCOM_IBB_TYPE)) ||
		    WARN_ON(type != reg_data->type))
			return -EINVAL;

		vreg  = devm_kzalloc(&pdev->dev, sizeof(*vreg),
					   GFP_KERNEL);
		if (!vreg)
			return -ENOMEM;

		sc_irq_name = devm_kasprintf(dev, GFP_KERNEL,
					     "%s-short-circuit",
					     reg_data->name);
		if (!sc_irq_name)
			return -ENOMEM;

		reg_node = of_get_child_by_name(pdev->dev.of_node,
						reg_data->name);
		if (!reg_node)
			return -EINVAL;

		 
		irq = of_irq_get_byname(reg_node, "sc-err");
		if (irq <= 0) {
			if (irq == 0)
				irq = -EINVAL;

			of_node_put(reg_node);
			return dev_err_probe(vreg->dev, irq,
					     "Short-circuit irq not found.\n");
		}
		vreg->sc_irq = irq;

		 
		irq = of_irq_get_byname(reg_node, "ocp");
		vreg->ocp_irq = irq;
		vreg->ocp_irq_count = 0;
		of_node_put(reg_node);

		vreg->regmap = reg_regmap;
		vreg->dev = dev;
		vreg->base = reg_data->base;
		vreg->type = reg_data->type;
		INIT_DELAYED_WORK(&vreg->sc_recovery_work,
				  qcom_labibb_sc_recovery_worker);

		if (vreg->ocp_irq > 0)
			INIT_DELAYED_WORK(&vreg->ocp_recovery_work,
					  qcom_labibb_ocp_recovery_worker);

		switch (vreg->type) {
		case QCOM_LAB_TYPE:
			 
			vreg->uA_limits.uA_min  = 200000;
			vreg->uA_limits.uA_step = 200000;
			vreg->uA_limits.ovr_val = LAB_CURRENT_LIMIT_OVERRIDE_EN;
			break;
		case QCOM_IBB_TYPE:
			 
			vreg->uA_limits.uA_min  = 0;
			vreg->uA_limits.uA_step = 50000;
			vreg->uA_limits.ovr_val = 0;  
			break;
		default:
			return -EINVAL;
		}

		memcpy(&vreg->desc, reg_data->desc, sizeof(vreg->desc));
		vreg->desc.of_match = reg_data->name;
		vreg->desc.name = reg_data->name;

		cfg.dev = vreg->dev;
		cfg.driver_data = vreg;
		cfg.regmap = vreg->regmap;

		vreg->rdev = devm_regulator_register(vreg->dev, &vreg->desc,
							&cfg);

		if (IS_ERR(vreg->rdev)) {
			dev_err(dev, "qcom_labibb: error registering %s : %d\n",
					reg_data->name, ret);
			return PTR_ERR(vreg->rdev);
		}

		ret = devm_request_threaded_irq(vreg->dev, vreg->sc_irq, NULL,
						qcom_labibb_sc_isr,
						IRQF_ONESHOT |
						IRQF_TRIGGER_RISING,
						sc_irq_name, vreg);
		if (ret)
			return ret;
	}

	return 0;
}

static struct platform_driver qcom_labibb_regulator_driver = {
	.driver	= {
		.name = "qcom-lab-ibb-regulator",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table	= qcom_labibb_match,
	},
	.probe = qcom_labibb_regulator_probe,
};
module_platform_driver(qcom_labibb_regulator_driver);

MODULE_DESCRIPTION("Qualcomm labibb driver");
MODULE_AUTHOR("Nisha Kumari <nishakumari@codeaurora.org>");
MODULE_AUTHOR("Sumit Semwal <sumit.semwal@linaro.org>");
MODULE_LICENSE("GPL v2");
