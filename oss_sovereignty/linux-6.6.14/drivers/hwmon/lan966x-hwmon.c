

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/hwmon.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/polynomial.h>
#include <linux/regmap.h>

 

static const struct polynomial poly_N_to_temp = {
	.terms = {
		{4,  -34627, 1000, 1},
		{3,  110230, 1000, 1},
		{2, -191650, 1000, 1},
		{1,  306040, 1000, 1},
		{0,  -56197,    1, 1}
	}
};

#define PVT_SENSOR_CTRL		0x0  
#define PVT_SENSOR_CFG		0x4
#define   SENSOR_CFG_CLK_CFG		GENMASK(27, 20)
#define   SENSOR_CFG_TRIM_VAL		GENMASK(13, 9)
#define   SENSOR_CFG_SAMPLE_ENA		BIT(8)
#define   SENSOR_CFG_START_CAPTURE	BIT(7)
#define   SENSOR_CFG_CONTINIOUS_MODE	BIT(6)
#define   SENSOR_CFG_PSAMPLE_ENA	GENMASK(1, 0)
#define PVT_SENSOR_STAT		0x8
#define   SENSOR_STAT_DATA_VALID	BIT(10)
#define   SENSOR_STAT_DATA		GENMASK(9, 0)

#define FAN_CFG			0x0
#define   FAN_CFG_DUTY_CYCLE		GENMASK(23, 16)
#define   INV_POL			BIT(3)
#define   GATE_ENA			BIT(2)
#define   PWM_OPEN_COL_ENA		BIT(1)
#define   FAN_STAT_CFG			BIT(0)
#define FAN_PWM_FREQ		0x4
#define   FAN_PWM_CYC_10US		GENMASK(25, 15)
#define   FAN_PWM_FREQ_FREQ		GENMASK(14, 0)
#define FAN_CNT			0xc
#define   FAN_CNT_DATA			GENMASK(15, 0)

#define LAN966X_PVT_CLK		1200000  

struct lan966x_hwmon {
	struct regmap *regmap_pvt;
	struct regmap *regmap_fan;
	struct clk *clk;
	unsigned long clk_rate;
};

static int lan966x_hwmon_read_temp(struct device *dev, long *val)
{
	struct lan966x_hwmon *hwmon = dev_get_drvdata(dev);
	unsigned int data;
	int ret;

	ret = regmap_read(hwmon->regmap_pvt, PVT_SENSOR_STAT, &data);
	if (ret < 0)
		return ret;

	if (!(data & SENSOR_STAT_DATA_VALID))
		return -ENODATA;

	*val = polynomial_calc(&poly_N_to_temp,
			       FIELD_GET(SENSOR_STAT_DATA, data));

	return 0;
}

static int lan966x_hwmon_read_fan(struct device *dev, long *val)
{
	struct lan966x_hwmon *hwmon = dev_get_drvdata(dev);
	unsigned int data;
	int ret;

	ret = regmap_read(hwmon->regmap_fan, FAN_CNT, &data);
	if (ret < 0)
		return ret;

	 
	*val = FIELD_GET(FAN_CNT_DATA, data) * 60 / 2;

	return 0;
}

static int lan966x_hwmon_read_pwm(struct device *dev, long *val)
{
	struct lan966x_hwmon *hwmon = dev_get_drvdata(dev);
	unsigned int data;
	int ret;

	ret = regmap_read(hwmon->regmap_fan, FAN_CFG, &data);
	if (ret < 0)
		return ret;

	*val = FIELD_GET(FAN_CFG_DUTY_CYCLE, data);

	return 0;
}

static int lan966x_hwmon_read_pwm_freq(struct device *dev, long *val)
{
	struct lan966x_hwmon *hwmon = dev_get_drvdata(dev);
	unsigned long tmp;
	unsigned int data;
	int ret;

	ret = regmap_read(hwmon->regmap_fan, FAN_PWM_FREQ, &data);
	if (ret < 0)
		return ret;

	 
	data = FIELD_GET(FAN_PWM_FREQ_FREQ, data) + 1;
	tmp = DIV_ROUND_CLOSEST(hwmon->clk_rate, 256);
	*val = DIV_ROUND_CLOSEST(tmp, data);

	return 0;
}

static int lan966x_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
			      u32 attr, int channel, long *val)
{
	switch (type) {
	case hwmon_temp:
		return lan966x_hwmon_read_temp(dev, val);
	case hwmon_fan:
		return lan966x_hwmon_read_fan(dev, val);
	case hwmon_pwm:
		switch (attr) {
		case hwmon_pwm_input:
			return lan966x_hwmon_read_pwm(dev, val);
		case hwmon_pwm_freq:
			return lan966x_hwmon_read_pwm_freq(dev, val);
		default:
			return -EOPNOTSUPP;
		}
	default:
		return -EOPNOTSUPP;
	}
}

static int lan966x_hwmon_write_pwm(struct device *dev, long val)
{
	struct lan966x_hwmon *hwmon = dev_get_drvdata(dev);

	if (val < 0 || val > 255)
		return -EINVAL;

	return regmap_update_bits(hwmon->regmap_fan, FAN_CFG,
				  FAN_CFG_DUTY_CYCLE,
				  FIELD_PREP(FAN_CFG_DUTY_CYCLE, val));
}

static int lan966x_hwmon_write_pwm_freq(struct device *dev, long val)
{
	struct lan966x_hwmon *hwmon = dev_get_drvdata(dev);

	if (val <= 0)
		return -EINVAL;

	val = DIV_ROUND_CLOSEST(hwmon->clk_rate, val);
	val = DIV_ROUND_CLOSEST(val, 256) - 1;
	val = clamp_val(val, 0, FAN_PWM_FREQ_FREQ);

	return regmap_update_bits(hwmon->regmap_fan, FAN_PWM_FREQ,
				  FAN_PWM_FREQ_FREQ,
				  FIELD_PREP(FAN_PWM_FREQ_FREQ, val));
}

static int lan966x_hwmon_write(struct device *dev, enum hwmon_sensor_types type,
			       u32 attr, int channel, long val)
{
	switch (type) {
	case hwmon_pwm:
		switch (attr) {
		case hwmon_pwm_input:
			return lan966x_hwmon_write_pwm(dev, val);
		case hwmon_pwm_freq:
			return lan966x_hwmon_write_pwm_freq(dev, val);
		default:
			return -EOPNOTSUPP;
		}
	default:
		return -EOPNOTSUPP;
	}
}

static umode_t lan966x_hwmon_is_visible(const void *data,
					enum hwmon_sensor_types type,
					u32 attr, int channel)
{
	umode_t mode = 0;

	switch (type) {
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
			mode = 0444;
			break;
		default:
			break;
		}
		break;
	case hwmon_fan:
		switch (attr) {
		case hwmon_fan_input:
			mode = 0444;
			break;
		default:
			break;
		}
		break;
	case hwmon_pwm:
		switch (attr) {
		case hwmon_pwm_input:
		case hwmon_pwm_freq:
			mode = 0644;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return mode;
}

static const struct hwmon_channel_info * const lan966x_hwmon_info[] = {
	HWMON_CHANNEL_INFO(chip, HWMON_C_REGISTER_TZ),
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT),
	HWMON_CHANNEL_INFO(fan, HWMON_F_INPUT),
	HWMON_CHANNEL_INFO(pwm, HWMON_PWM_INPUT | HWMON_PWM_FREQ),
	NULL
};

static const struct hwmon_ops lan966x_hwmon_ops = {
	.is_visible = lan966x_hwmon_is_visible,
	.read = lan966x_hwmon_read,
	.write = lan966x_hwmon_write,
};

static const struct hwmon_chip_info lan966x_hwmon_chip_info = {
	.ops = &lan966x_hwmon_ops,
	.info = lan966x_hwmon_info,
};

static void lan966x_hwmon_disable(void *data)
{
	struct lan966x_hwmon *hwmon = data;

	regmap_update_bits(hwmon->regmap_pvt, PVT_SENSOR_CFG,
			   SENSOR_CFG_SAMPLE_ENA | SENSOR_CFG_CONTINIOUS_MODE,
			   0);
}

static int lan966x_hwmon_enable(struct device *dev,
				struct lan966x_hwmon *hwmon)
{
	unsigned int mask = SENSOR_CFG_CLK_CFG |
			    SENSOR_CFG_SAMPLE_ENA |
			    SENSOR_CFG_START_CAPTURE |
			    SENSOR_CFG_CONTINIOUS_MODE |
			    SENSOR_CFG_PSAMPLE_ENA;
	unsigned int val;
	unsigned int div;
	int ret;

	 
	val = SENSOR_CFG_SAMPLE_ENA | SENSOR_CFG_CONTINIOUS_MODE;

	 
	div = DIV_ROUND_CLOSEST(hwmon->clk_rate, LAN966X_PVT_CLK);
	val |= FIELD_PREP(SENSOR_CFG_CLK_CFG, div);

	ret = regmap_update_bits(hwmon->regmap_pvt, PVT_SENSOR_CFG,
				 mask, val);
	if (ret)
		return ret;

	return devm_add_action_or_reset(dev, lan966x_hwmon_disable, hwmon);
}

static struct regmap *lan966x_init_regmap(struct platform_device *pdev,
					  const char *name)
{
	struct regmap_config regmap_config = {
		.reg_bits = 32,
		.reg_stride = 4,
		.val_bits = 32,
	};
	void __iomem *base;

	base = devm_platform_ioremap_resource_byname(pdev, name);
	if (IS_ERR(base))
		return ERR_CAST(base);

	regmap_config.name = name;

	return devm_regmap_init_mmio(&pdev->dev, base, &regmap_config);
}

static int lan966x_hwmon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lan966x_hwmon *hwmon;
	struct device *hwmon_dev;
	int ret;

	hwmon = devm_kzalloc(dev, sizeof(*hwmon), GFP_KERNEL);
	if (!hwmon)
		return -ENOMEM;

	hwmon->clk = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(hwmon->clk))
		return dev_err_probe(dev, PTR_ERR(hwmon->clk),
				     "failed to get clock\n");

	hwmon->clk_rate = clk_get_rate(hwmon->clk);

	hwmon->regmap_pvt = lan966x_init_regmap(pdev, "pvt");
	if (IS_ERR(hwmon->regmap_pvt))
		return dev_err_probe(dev, PTR_ERR(hwmon->regmap_pvt),
				     "failed to get regmap for PVT registers\n");

	hwmon->regmap_fan = lan966x_init_regmap(pdev, "fan");
	if (IS_ERR(hwmon->regmap_fan))
		return dev_err_probe(dev, PTR_ERR(hwmon->regmap_fan),
				     "failed to get regmap for fan registers\n");

	ret = lan966x_hwmon_enable(dev, hwmon);
	if (ret)
		return dev_err_probe(dev, ret, "failed to enable sensor\n");

	hwmon_dev = devm_hwmon_device_register_with_info(&pdev->dev,
				"lan966x_hwmon", hwmon,
				&lan966x_hwmon_chip_info, NULL);
	if (IS_ERR(hwmon_dev))
		return dev_err_probe(dev, PTR_ERR(hwmon_dev),
				     "failed to register hwmon device\n");

	return 0;
}

static const struct of_device_id lan966x_hwmon_of_match[] = {
	{ .compatible = "microchip,lan9668-hwmon" },
	{}
};
MODULE_DEVICE_TABLE(of, lan966x_hwmon_of_match);

static struct platform_driver lan966x_hwmon_driver = {
	.probe = lan966x_hwmon_probe,
	.driver = {
		.name = "lan966x-hwmon",
		.of_match_table = lan966x_hwmon_of_match,
	},
};
module_platform_driver(lan966x_hwmon_driver);

MODULE_DESCRIPTION("LAN966x Hardware Monitoring Driver");
MODULE_AUTHOR("Michael Walle <michael@walle.cc>");
MODULE_LICENSE("GPL");
