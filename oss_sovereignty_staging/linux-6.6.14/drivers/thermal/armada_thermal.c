
 
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/thermal.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>

 
#define PMU_TDC0_SW_RST_MASK		(0x1 << 1)
#define PMU_TM_DISABLE_OFFS		0
#define PMU_TM_DISABLE_MASK		(0x1 << PMU_TM_DISABLE_OFFS)
#define PMU_TDC0_REF_CAL_CNT_OFFS	11
#define PMU_TDC0_REF_CAL_CNT_MASK	(0x1ff << PMU_TDC0_REF_CAL_CNT_OFFS)
#define PMU_TDC0_OTF_CAL_MASK		(0x1 << 30)
#define PMU_TDC0_START_CAL_MASK		(0x1 << 25)

#define A375_UNIT_CONTROL_SHIFT		27
#define A375_UNIT_CONTROL_MASK		0x7
#define A375_READOUT_INVERT		BIT(15)
#define A375_HW_RESETn			BIT(8)

 
#define CONTROL0_TSEN_TC_TRIM_MASK	0x7
#define CONTROL0_TSEN_TC_TRIM_VAL	0x3

#define CONTROL0_TSEN_START		BIT(0)
#define CONTROL0_TSEN_RESET		BIT(1)
#define CONTROL0_TSEN_ENABLE		BIT(2)
#define CONTROL0_TSEN_AVG_BYPASS	BIT(6)
#define CONTROL0_TSEN_CHAN_SHIFT	13
#define CONTROL0_TSEN_CHAN_MASK		0xF
#define CONTROL0_TSEN_OSR_SHIFT		24
#define CONTROL0_TSEN_OSR_MAX		0x3
#define CONTROL0_TSEN_MODE_SHIFT	30
#define CONTROL0_TSEN_MODE_EXTERNAL	0x2
#define CONTROL0_TSEN_MODE_MASK		0x3

#define CONTROL1_TSEN_AVG_MASK		0x7
#define CONTROL1_EXT_TSEN_SW_RESET	BIT(7)
#define CONTROL1_EXT_TSEN_HW_RESETn	BIT(8)
#define CONTROL1_TSEN_INT_EN		BIT(25)
#define CONTROL1_TSEN_SELECT_OFF	21
#define CONTROL1_TSEN_SELECT_MASK	0x3

#define STATUS_POLL_PERIOD_US		1000
#define STATUS_POLL_TIMEOUT_US		100000
#define OVERHEAT_INT_POLL_DELAY_MS	1000

struct armada_thermal_data;

 
struct armada_thermal_priv {
	struct device *dev;
	struct regmap *syscon;
	char zone_name[THERMAL_NAME_LENGTH];
	 
	struct mutex update_lock;
	struct armada_thermal_data *data;
	struct thermal_zone_device *overheat_sensor;
	int interrupt_source;
	int current_channel;
	long current_threshold;
	long current_hysteresis;
};

struct armada_thermal_data {
	 
	void (*init)(struct platform_device *pdev,
		     struct armada_thermal_priv *priv);

	 
	s64 coef_b;
	s64 coef_m;
	u32 coef_div;
	bool inverted;
	bool signed_sample;

	 
	unsigned int temp_shift;
	unsigned int temp_mask;
	unsigned int thresh_shift;
	unsigned int hyst_shift;
	unsigned int hyst_mask;
	u32 is_valid_bit;

	 
	unsigned int syscon_control0_off;
	unsigned int syscon_control1_off;
	unsigned int syscon_status_off;
	unsigned int dfx_irq_cause_off;
	unsigned int dfx_irq_mask_off;
	unsigned int dfx_overheat_irq;
	unsigned int dfx_server_irq_mask_off;
	unsigned int dfx_server_irq_en;

	 
	unsigned int cpu_nr;
};

struct armada_drvdata {
	enum drvtype {
		LEGACY,
		SYSCON
	} type;
	union {
		struct armada_thermal_priv *priv;
		struct thermal_zone_device *tz;
	} data;
};

 
struct armada_thermal_sensor {
	struct armada_thermal_priv *priv;
	int id;
};

static void armadaxp_init(struct platform_device *pdev,
			  struct armada_thermal_priv *priv)
{
	struct armada_thermal_data *data = priv->data;
	u32 reg;

	regmap_read(priv->syscon, data->syscon_control1_off, &reg);
	reg |= PMU_TDC0_OTF_CAL_MASK;

	 
	reg &= ~PMU_TDC0_REF_CAL_CNT_MASK;
	reg |= (0xf1 << PMU_TDC0_REF_CAL_CNT_OFFS);

	 
	reg |= PMU_TDC0_SW_RST_MASK;

	regmap_write(priv->syscon, data->syscon_control1_off, reg);

	reg &= ~PMU_TDC0_SW_RST_MASK;
	regmap_write(priv->syscon, data->syscon_control1_off, reg);

	 
	regmap_read(priv->syscon, data->syscon_status_off, &reg);
	reg &= ~PMU_TM_DISABLE_MASK;
	regmap_write(priv->syscon, data->syscon_status_off, reg);
}

static void armada370_init(struct platform_device *pdev,
			   struct armada_thermal_priv *priv)
{
	struct armada_thermal_data *data = priv->data;
	u32 reg;

	regmap_read(priv->syscon, data->syscon_control1_off, &reg);
	reg |= PMU_TDC0_OTF_CAL_MASK;

	 
	reg &= ~PMU_TDC0_REF_CAL_CNT_MASK;
	reg |= (0xf1 << PMU_TDC0_REF_CAL_CNT_OFFS);

	 
	reg &= ~PMU_TDC0_START_CAL_MASK;

	regmap_write(priv->syscon, data->syscon_control1_off, reg);

	msleep(10);
}

static void armada375_init(struct platform_device *pdev,
			   struct armada_thermal_priv *priv)
{
	struct armada_thermal_data *data = priv->data;
	u32 reg;

	regmap_read(priv->syscon, data->syscon_control1_off, &reg);
	reg &= ~(A375_UNIT_CONTROL_MASK << A375_UNIT_CONTROL_SHIFT);
	reg &= ~A375_READOUT_INVERT;
	reg &= ~A375_HW_RESETn;
	regmap_write(priv->syscon, data->syscon_control1_off, reg);

	msleep(20);

	reg |= A375_HW_RESETn;
	regmap_write(priv->syscon, data->syscon_control1_off, reg);

	msleep(50);
}

static int armada_wait_sensor_validity(struct armada_thermal_priv *priv)
{
	u32 reg;

	return regmap_read_poll_timeout(priv->syscon,
					priv->data->syscon_status_off, reg,
					reg & priv->data->is_valid_bit,
					STATUS_POLL_PERIOD_US,
					STATUS_POLL_TIMEOUT_US);
}

static void armada380_init(struct platform_device *pdev,
			   struct armada_thermal_priv *priv)
{
	struct armada_thermal_data *data = priv->data;
	u32 reg;

	 
	regmap_read(priv->syscon, data->syscon_control1_off, &reg);
	reg |= CONTROL1_EXT_TSEN_HW_RESETn;
	reg &= ~CONTROL1_EXT_TSEN_SW_RESET;
	regmap_write(priv->syscon, data->syscon_control1_off, reg);

	 
	regmap_read(priv->syscon, data->syscon_control0_off, &reg);
	reg &= ~CONTROL0_TSEN_TC_TRIM_MASK;
	reg |= CONTROL0_TSEN_TC_TRIM_VAL;
	regmap_write(priv->syscon, data->syscon_control0_off, reg);
}

static void armada_ap80x_init(struct platform_device *pdev,
			      struct armada_thermal_priv *priv)
{
	struct armada_thermal_data *data = priv->data;
	u32 reg;

	regmap_read(priv->syscon, data->syscon_control0_off, &reg);
	reg &= ~CONTROL0_TSEN_RESET;
	reg |= CONTROL0_TSEN_START | CONTROL0_TSEN_ENABLE;

	 
	reg |= CONTROL0_TSEN_OSR_MAX << CONTROL0_TSEN_OSR_SHIFT;

	 
	reg &= ~CONTROL0_TSEN_AVG_BYPASS;

	regmap_write(priv->syscon, data->syscon_control0_off, reg);
}

static void armada_cp110_init(struct platform_device *pdev,
			      struct armada_thermal_priv *priv)
{
	struct armada_thermal_data *data = priv->data;
	u32 reg;

	armada380_init(pdev, priv);

	 
	regmap_read(priv->syscon, data->syscon_control0_off, &reg);
	reg |= CONTROL0_TSEN_OSR_MAX << CONTROL0_TSEN_OSR_SHIFT;
	regmap_write(priv->syscon, data->syscon_control0_off, reg);

	 
	regmap_read(priv->syscon, data->syscon_control1_off, &reg);
	reg &= ~CONTROL1_TSEN_AVG_MASK;
	reg |= 1;
	regmap_write(priv->syscon, data->syscon_control1_off, reg);
}

static bool armada_is_valid(struct armada_thermal_priv *priv)
{
	u32 reg;

	if (!priv->data->is_valid_bit)
		return true;

	regmap_read(priv->syscon, priv->data->syscon_status_off, &reg);

	return reg & priv->data->is_valid_bit;
}

static void armada_enable_overheat_interrupt(struct armada_thermal_priv *priv)
{
	struct armada_thermal_data *data = priv->data;
	u32 reg;

	 
	regmap_read(priv->syscon, data->dfx_irq_cause_off, &reg);

	 
	regmap_read(priv->syscon, data->dfx_irq_mask_off, &reg);
	reg |= data->dfx_overheat_irq;
	regmap_write(priv->syscon, data->dfx_irq_mask_off, reg);

	 
	regmap_read(priv->syscon, data->dfx_server_irq_mask_off, &reg);
	reg |= data->dfx_server_irq_en;
	regmap_write(priv->syscon, data->dfx_server_irq_mask_off, reg);

	 
	regmap_read(priv->syscon, data->syscon_control1_off, &reg);
	reg |= CONTROL1_TSEN_INT_EN;
	regmap_write(priv->syscon, data->syscon_control1_off, reg);
}

static void __maybe_unused
armada_disable_overheat_interrupt(struct armada_thermal_priv *priv)
{
	struct armada_thermal_data *data = priv->data;
	u32 reg;

	regmap_read(priv->syscon, data->syscon_control1_off, &reg);
	reg &= ~CONTROL1_TSEN_INT_EN;
	regmap_write(priv->syscon, data->syscon_control1_off, reg);
}

 
static int armada_select_channel(struct armada_thermal_priv *priv, int channel)
{
	struct armada_thermal_data *data = priv->data;
	u32 ctrl0;

	if (channel < 0 || channel > priv->data->cpu_nr)
		return -EINVAL;

	if (priv->current_channel == channel)
		return 0;

	 
	regmap_read(priv->syscon, data->syscon_control0_off, &ctrl0);
	ctrl0 &= ~CONTROL0_TSEN_START;
	regmap_write(priv->syscon, data->syscon_control0_off, ctrl0);

	 
	ctrl0 &= ~(CONTROL0_TSEN_MODE_MASK << CONTROL0_TSEN_MODE_SHIFT);

	 
	if (channel) {
		 
		ctrl0 |= CONTROL0_TSEN_MODE_EXTERNAL <<
			 CONTROL0_TSEN_MODE_SHIFT;
		 
		ctrl0 &= ~(CONTROL0_TSEN_CHAN_MASK << CONTROL0_TSEN_CHAN_SHIFT);
		ctrl0 |= (channel - 1) << CONTROL0_TSEN_CHAN_SHIFT;
	}

	 
	regmap_write(priv->syscon, data->syscon_control0_off, ctrl0);
	priv->current_channel = channel;

	 
	ctrl0 |= CONTROL0_TSEN_START;
	regmap_write(priv->syscon, data->syscon_control0_off, ctrl0);

	 
	if (armada_wait_sensor_validity(priv))
		return -EIO;

	return 0;
}

static int armada_read_sensor(struct armada_thermal_priv *priv, int *temp)
{
	u32 reg, div;
	s64 sample, b, m;

	regmap_read(priv->syscon, priv->data->syscon_status_off, &reg);
	reg = (reg >> priv->data->temp_shift) & priv->data->temp_mask;
	if (priv->data->signed_sample)
		 
		sample = sign_extend32(reg, fls(priv->data->temp_mask) - 1);
	else
		sample = reg;

	 
	b = priv->data->coef_b;
	m = priv->data->coef_m;
	div = priv->data->coef_div;

	if (priv->data->inverted)
		*temp = div_s64((m * sample) - b, div);
	else
		*temp = div_s64(b - (m * sample), div);

	return 0;
}

static int armada_get_temp_legacy(struct thermal_zone_device *thermal,
				  int *temp)
{
	struct armada_thermal_priv *priv = thermal_zone_device_priv(thermal);
	int ret;

	 
	if (!armada_is_valid(priv))
		return -EIO;

	 
	ret = armada_read_sensor(priv, temp);

	return ret;
}

static struct thermal_zone_device_ops legacy_ops = {
	.get_temp = armada_get_temp_legacy,
};

static int armada_get_temp(struct thermal_zone_device *tz, int *temp)
{
	struct armada_thermal_sensor *sensor = thermal_zone_device_priv(tz);
	struct armada_thermal_priv *priv = sensor->priv;
	int ret;

	mutex_lock(&priv->update_lock);

	 
	ret = armada_select_channel(priv, sensor->id);
	if (ret)
		goto unlock_mutex;

	 
	ret = armada_read_sensor(priv, temp);
	if (ret)
		goto unlock_mutex;

	 
	ret = armada_select_channel(priv, priv->interrupt_source);

unlock_mutex:
	mutex_unlock(&priv->update_lock);

	return ret;
}

static const struct thermal_zone_device_ops of_ops = {
	.get_temp = armada_get_temp,
};

static unsigned int armada_mc_to_reg_temp(struct armada_thermal_data *data,
					  unsigned int temp_mc)
{
	s64 b = data->coef_b;
	s64 m = data->coef_m;
	s64 div = data->coef_div;
	unsigned int sample;

	if (data->inverted)
		sample = div_s64(((temp_mc * div) + b), m);
	else
		sample = div_s64((b - (temp_mc * div)), m);

	return sample & data->temp_mask;
}

 
static unsigned int hyst_levels_mc[] = {1900, 3800, 7600, 15200};

static unsigned int armada_mc_to_reg_hyst(struct armada_thermal_data *data,
					  unsigned int hyst_mc)
{
	int i;

	 
	for (i = ARRAY_SIZE(hyst_levels_mc) - 1; i > 0; i--)
		if (hyst_mc >= hyst_levels_mc[i])
			break;

	return i & data->hyst_mask;
}

static void armada_set_overheat_thresholds(struct armada_thermal_priv *priv,
					   int thresh_mc, int hyst_mc)
{
	struct armada_thermal_data *data = priv->data;
	unsigned int threshold = armada_mc_to_reg_temp(data, thresh_mc);
	unsigned int hysteresis = armada_mc_to_reg_hyst(data, hyst_mc);
	u32 ctrl1;

	regmap_read(priv->syscon, data->syscon_control1_off, &ctrl1);

	 
	if (thresh_mc >= 0) {
		ctrl1 &= ~(data->temp_mask << data->thresh_shift);
		ctrl1 |= threshold << data->thresh_shift;
		priv->current_threshold = thresh_mc;
	}

	 
	if (hyst_mc >= 0) {
		ctrl1 &= ~(data->hyst_mask << data->hyst_shift);
		ctrl1 |= hysteresis << data->hyst_shift;
		priv->current_hysteresis = hyst_mc;
	}

	regmap_write(priv->syscon, data->syscon_control1_off, ctrl1);
}

static irqreturn_t armada_overheat_isr(int irq, void *blob)
{
	 
	disable_irq_nosync(irq);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t armada_overheat_isr_thread(int irq, void *blob)
{
	struct armada_thermal_priv *priv = blob;
	int low_threshold = priv->current_threshold - priv->current_hysteresis;
	int temperature;
	u32 dummy;
	int ret;

	 
	thermal_zone_device_update(priv->overheat_sensor,
				   THERMAL_EVENT_UNSPECIFIED);

	 
	do {
		msleep(OVERHEAT_INT_POLL_DELAY_MS);
		mutex_lock(&priv->update_lock);
		ret = armada_read_sensor(priv, &temperature);
		mutex_unlock(&priv->update_lock);
		if (ret)
			goto enable_irq;
	} while (temperature >= low_threshold);

	regmap_read(priv->syscon, priv->data->dfx_irq_cause_off, &dummy);

	 
	thermal_zone_device_update(priv->overheat_sensor,
				   THERMAL_EVENT_UNSPECIFIED);

enable_irq:
	enable_irq(irq);

	return IRQ_HANDLED;
}

static const struct armada_thermal_data armadaxp_data = {
	.init = armadaxp_init,
	.temp_shift = 10,
	.temp_mask = 0x1ff,
	.coef_b = 3153000000ULL,
	.coef_m = 10000000ULL,
	.coef_div = 13825,
	.syscon_status_off = 0xb0,
	.syscon_control1_off = 0x2d0,
};

static const struct armada_thermal_data armada370_data = {
	.init = armada370_init,
	.is_valid_bit = BIT(9),
	.temp_shift = 10,
	.temp_mask = 0x1ff,
	.coef_b = 3153000000ULL,
	.coef_m = 10000000ULL,
	.coef_div = 13825,
	.syscon_status_off = 0x0,
	.syscon_control1_off = 0x4,
};

static const struct armada_thermal_data armada375_data = {
	.init = armada375_init,
	.is_valid_bit = BIT(10),
	.temp_shift = 0,
	.temp_mask = 0x1ff,
	.coef_b = 3171900000ULL,
	.coef_m = 10000000ULL,
	.coef_div = 13616,
	.syscon_status_off = 0x78,
	.syscon_control0_off = 0x7c,
	.syscon_control1_off = 0x80,
};

static const struct armada_thermal_data armada380_data = {
	.init = armada380_init,
	.is_valid_bit = BIT(10),
	.temp_shift = 0,
	.temp_mask = 0x3ff,
	.coef_b = 1172499100ULL,
	.coef_m = 2000096ULL,
	.coef_div = 4201,
	.inverted = true,
	.syscon_control0_off = 0x70,
	.syscon_control1_off = 0x74,
	.syscon_status_off = 0x78,
};

static const struct armada_thermal_data armada_ap806_data = {
	.init = armada_ap80x_init,
	.is_valid_bit = BIT(16),
	.temp_shift = 0,
	.temp_mask = 0x3ff,
	.thresh_shift = 3,
	.hyst_shift = 19,
	.hyst_mask = 0x3,
	.coef_b = -150000LL,
	.coef_m = 423ULL,
	.coef_div = 1,
	.inverted = true,
	.signed_sample = true,
	.syscon_control0_off = 0x84,
	.syscon_control1_off = 0x88,
	.syscon_status_off = 0x8C,
	.dfx_irq_cause_off = 0x108,
	.dfx_irq_mask_off = 0x10C,
	.dfx_overheat_irq = BIT(22),
	.dfx_server_irq_mask_off = 0x104,
	.dfx_server_irq_en = BIT(1),
	.cpu_nr = 4,
};

static const struct armada_thermal_data armada_ap807_data = {
	.init = armada_ap80x_init,
	.is_valid_bit = BIT(16),
	.temp_shift = 0,
	.temp_mask = 0x3ff,
	.thresh_shift = 3,
	.hyst_shift = 19,
	.hyst_mask = 0x3,
	.coef_b = -128900LL,
	.coef_m = 394ULL,
	.coef_div = 1,
	.inverted = true,
	.signed_sample = true,
	.syscon_control0_off = 0x84,
	.syscon_control1_off = 0x88,
	.syscon_status_off = 0x8C,
	.dfx_irq_cause_off = 0x108,
	.dfx_irq_mask_off = 0x10C,
	.dfx_overheat_irq = BIT(22),
	.dfx_server_irq_mask_off = 0x104,
	.dfx_server_irq_en = BIT(1),
	.cpu_nr = 4,
};

static const struct armada_thermal_data armada_cp110_data = {
	.init = armada_cp110_init,
	.is_valid_bit = BIT(10),
	.temp_shift = 0,
	.temp_mask = 0x3ff,
	.thresh_shift = 16,
	.hyst_shift = 26,
	.hyst_mask = 0x3,
	.coef_b = 1172499100ULL,
	.coef_m = 2000096ULL,
	.coef_div = 4201,
	.inverted = true,
	.syscon_control0_off = 0x70,
	.syscon_control1_off = 0x74,
	.syscon_status_off = 0x78,
	.dfx_irq_cause_off = 0x108,
	.dfx_irq_mask_off = 0x10C,
	.dfx_overheat_irq = BIT(20),
	.dfx_server_irq_mask_off = 0x104,
	.dfx_server_irq_en = BIT(1),
};

static const struct of_device_id armada_thermal_id_table[] = {
	{
		.compatible = "marvell,armadaxp-thermal",
		.data       = &armadaxp_data,
	},
	{
		.compatible = "marvell,armada370-thermal",
		.data       = &armada370_data,
	},
	{
		.compatible = "marvell,armada375-thermal",
		.data       = &armada375_data,
	},
	{
		.compatible = "marvell,armada380-thermal",
		.data       = &armada380_data,
	},
	{
		.compatible = "marvell,armada-ap806-thermal",
		.data       = &armada_ap806_data,
	},
	{
		.compatible = "marvell,armada-ap807-thermal",
		.data       = &armada_ap807_data,
	},
	{
		.compatible = "marvell,armada-cp110-thermal",
		.data       = &armada_cp110_data,
	},
	{
		 
	},
};
MODULE_DEVICE_TABLE(of, armada_thermal_id_table);

static const struct regmap_config armada_thermal_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.fast_io = true,
};

static int armada_thermal_probe_legacy(struct platform_device *pdev,
				       struct armada_thermal_priv *priv)
{
	struct armada_thermal_data *data = priv->data;
	void __iomem *base;

	 
	base = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(base))
		return PTR_ERR(base);

	 
	if (((unsigned long)base & ~PAGE_MASK) < data->syscon_status_off)
		return -EINVAL;
	base -= data->syscon_status_off;

	priv->syscon = devm_regmap_init_mmio(&pdev->dev, base,
					     &armada_thermal_regmap_config);
	return PTR_ERR_OR_ZERO(priv->syscon);
}

static int armada_thermal_probe_syscon(struct platform_device *pdev,
				       struct armada_thermal_priv *priv)
{
	priv->syscon = syscon_node_to_regmap(pdev->dev.parent->of_node);
	return PTR_ERR_OR_ZERO(priv->syscon);
}

static void armada_set_sane_name(struct platform_device *pdev,
				 struct armada_thermal_priv *priv)
{
	const char *name = dev_name(&pdev->dev);
	char *insane_char;

	if (strlen(name) > THERMAL_NAME_LENGTH) {
		 
		name = strrchr(name, ':');
		if (!name)
			name = "armada_thermal";
		else
			name++;
	}

	 
	strscpy(priv->zone_name, name, THERMAL_NAME_LENGTH);

	 
	do {
		insane_char = strpbrk(priv->zone_name, "-");
		if (insane_char)
			*insane_char = '_';
	} while (insane_char);
}

 
static int armada_configure_overheat_int(struct armada_thermal_priv *priv,
					 struct thermal_zone_device *tz,
					 int sensor_id)
{
	 
	int temperature;
	int ret;

	ret = thermal_zone_get_crit_temp(tz, &temperature);
	if (ret)
		return ret;

	ret = armada_select_channel(priv, sensor_id);
	if (ret)
		return ret;

	 
	armada_set_overheat_thresholds(priv, temperature, 0);
	priv->overheat_sensor = tz;
	priv->interrupt_source = sensor_id;
	armada_enable_overheat_interrupt(priv);

	return 0;
}

static int armada_thermal_probe(struct platform_device *pdev)
{
	struct thermal_zone_device *tz;
	struct armada_thermal_sensor *sensor;
	struct armada_drvdata *drvdata;
	const struct of_device_id *match;
	struct armada_thermal_priv *priv;
	int sensor_id, irq;
	int ret;

	match = of_match_device(armada_thermal_id_table, &pdev->dev);
	if (!match)
		return -ENODEV;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	priv->dev = &pdev->dev;
	priv->data = (struct armada_thermal_data *)match->data;

	mutex_init(&priv->update_lock);

	 
	if (IS_ERR(syscon_node_to_regmap(pdev->dev.parent->of_node))) {
		 
		armada_set_sane_name(pdev, priv);

		ret = armada_thermal_probe_legacy(pdev, priv);
		if (ret)
			return ret;

		priv->data->init(pdev, priv);

		 
		armada_wait_sensor_validity(priv);

		tz = thermal_tripless_zone_device_register(priv->zone_name,
							   priv, &legacy_ops,
							   NULL);
		if (IS_ERR(tz)) {
			dev_err(&pdev->dev,
				"Failed to register thermal zone device\n");
			return PTR_ERR(tz);
		}

		ret = thermal_zone_device_enable(tz);
		if (ret) {
			thermal_zone_device_unregister(tz);
			return ret;
		}

		drvdata->type = LEGACY;
		drvdata->data.tz = tz;
		platform_set_drvdata(pdev, drvdata);

		return 0;
	}

	ret = armada_thermal_probe_syscon(pdev, priv);
	if (ret)
		return ret;

	priv->current_channel = -1;
	priv->data->init(pdev, priv);
	drvdata->type = SYSCON;
	drvdata->data.priv = priv;
	platform_set_drvdata(pdev, drvdata);

	irq = platform_get_irq(pdev, 0);
	if (irq == -EPROBE_DEFER)
		return irq;

	 
	if (irq > 0) {
		ret = devm_request_threaded_irq(&pdev->dev, irq,
						armada_overheat_isr,
						armada_overheat_isr_thread,
						0, NULL, priv);
		if (ret) {
			dev_err(&pdev->dev, "Cannot request threaded IRQ %d\n",
				irq);
			return ret;
		}
	}

	 
	for (sensor_id = 0; sensor_id <= priv->data->cpu_nr; sensor_id++) {
		sensor = devm_kzalloc(&pdev->dev,
				      sizeof(struct armada_thermal_sensor),
				      GFP_KERNEL);
		if (!sensor)
			return -ENOMEM;

		 
		sensor->priv = priv;
		sensor->id = sensor_id;
		tz = devm_thermal_of_zone_register(&pdev->dev,
						   sensor->id, sensor,
						   &of_ops);
		if (IS_ERR(tz)) {
			dev_info(&pdev->dev, "Thermal sensor %d unavailable\n",
				 sensor_id);
			devm_kfree(&pdev->dev, sensor);
			continue;
		}

		 
		if (irq > 0 && !priv->overheat_sensor)
			armada_configure_overheat_int(priv, tz, sensor->id);
	}

	 
	if (!priv->overheat_sensor)
		dev_warn(&pdev->dev, "Overheat interrupt not available\n");

	return 0;
}

static int armada_thermal_exit(struct platform_device *pdev)
{
	struct armada_drvdata *drvdata = platform_get_drvdata(pdev);

	if (drvdata->type == LEGACY)
		thermal_zone_device_unregister(drvdata->data.tz);

	return 0;
}

static struct platform_driver armada_thermal_driver = {
	.probe = armada_thermal_probe,
	.remove = armada_thermal_exit,
	.driver = {
		.name = "armada_thermal",
		.of_match_table = armada_thermal_id_table,
	},
};

module_platform_driver(armada_thermal_driver);

MODULE_AUTHOR("Ezequiel Garcia <ezequiel.garcia@free-electrons.com>");
MODULE_DESCRIPTION("Marvell EBU Armada SoCs thermal driver");
MODULE_LICENSE("GPL v2");
