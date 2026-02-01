
 

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>

#include "../thermal_hwmon.h"

 
#define DTS_CFGR1_OFFSET	0x0
#define DTS_T0VALR1_OFFSET	0x8
#define DTS_RAMPVALR_OFFSET	0X10
#define DTS_ITR1_OFFSET		0x14
#define DTS_DR_OFFSET		0x1C
#define DTS_SR_OFFSET		0x20
#define DTS_ITENR_OFFSET	0x24
#define DTS_ICIFR_OFFSET	0x28

 
#define HSREF_CLK_DIV_MASK	GENMASK(30, 24)
#define TS1_SMP_TIME_MASK	GENMASK(19, 16)
#define TS1_INTRIG_SEL_MASK	GENMASK(11, 8)

 
#define TS1_T0_MASK		GENMASK(17, 16)
#define TS1_FMT0_MASK		GENMASK(15, 0)

 
#define TS1_RAMP_COEFF_MASK	GENMASK(15, 0)

 
#define TS1_HITTHD_MASK		GENMASK(31, 16)
#define TS1_LITTHD_MASK		GENMASK(15, 0)

 
#define TS1_MFREQ_MASK		GENMASK(15, 0)

 
#define ITENR_MASK		(GENMASK(2, 0) | GENMASK(6, 4))

 
#define ICIFR_MASK		(GENMASK(2, 0) | GENMASK(6, 4))

 
#define TS1_T0_POS		16
#define TS1_HITTHD_POS		16
#define TS1_LITTHD_POS		0
#define HSREF_CLK_DIV_POS	24

 
#define TS1_EN			BIT(0)
#define TS1_START		BIT(4)
#define REFCLK_SEL		BIT(20)
#define REFCLK_LSE		REFCLK_SEL
#define Q_MEAS_OPT		BIT(21)
#define CALIBRATION_CONTROL	Q_MEAS_OPT

 
#define TS_RDY			BIT(15)
 
#define HIGH_THRESHOLD		BIT(2)
#define LOW_THRESHOLD		BIT(1)

 
#define ADJUST			100
#define ONE_MHZ			1000000
#define POLL_TIMEOUT		5000
#define STARTUP_TIME		40
#define TS1_T0_VAL0		30000   
#define TS1_T0_VAL1		130000  
#define NO_HW_TRIG		0
#define SAMPLING_TIME		15

struct stm_thermal_sensor {
	struct device *dev;
	struct thermal_zone_device *th_dev;
	enum thermal_device_mode mode;
	struct clk *clk;
	unsigned int low_temp_enabled;
	unsigned int high_temp_enabled;
	int irq;
	void __iomem *base;
	int t0, fmt0, ramp_coeff;
};

static int stm_enable_irq(struct stm_thermal_sensor *sensor)
{
	u32 value;

	dev_dbg(sensor->dev, "low:%d high:%d\n", sensor->low_temp_enabled,
		sensor->high_temp_enabled);

	 
	value = readl_relaxed(sensor->base + DTS_ITENR_OFFSET);
	value &= ~(LOW_THRESHOLD | HIGH_THRESHOLD);

	if (sensor->low_temp_enabled)
		value |= HIGH_THRESHOLD;

	if (sensor->high_temp_enabled)
		value |= LOW_THRESHOLD;

	 
	writel_relaxed(value, sensor->base + DTS_ITENR_OFFSET);

	return 0;
}

static irqreturn_t stm_thermal_irq_handler(int irq, void *sdata)
{
	struct stm_thermal_sensor *sensor = sdata;

	dev_dbg(sensor->dev, "sr:%d\n",
		readl_relaxed(sensor->base + DTS_SR_OFFSET));

	thermal_zone_device_update(sensor->th_dev, THERMAL_EVENT_UNSPECIFIED);

	stm_enable_irq(sensor);

	 
	writel_relaxed(ICIFR_MASK, sensor->base + DTS_ICIFR_OFFSET);

	return IRQ_HANDLED;
}

static int stm_sensor_power_on(struct stm_thermal_sensor *sensor)
{
	int ret;
	u32 value;

	 
	value = readl_relaxed(sensor->base + DTS_CFGR1_OFFSET);
	value |= TS1_EN;
	writel_relaxed(value, sensor->base + DTS_CFGR1_OFFSET);

	 
	ret = readl_poll_timeout(sensor->base + DTS_SR_OFFSET,
				 value, (value & TS_RDY),
				 STARTUP_TIME, POLL_TIMEOUT);
	if (ret)
		return ret;

	 
	value = readl_relaxed(sensor->base +
			      DTS_CFGR1_OFFSET);
	value |= TS1_START;
	writel_relaxed(value, sensor->base +
		       DTS_CFGR1_OFFSET);

	sensor->mode = THERMAL_DEVICE_ENABLED;

	return 0;
}

static int stm_sensor_power_off(struct stm_thermal_sensor *sensor)
{
	u32 value;

	sensor->mode = THERMAL_DEVICE_DISABLED;

	 
	value = readl_relaxed(sensor->base + DTS_CFGR1_OFFSET);
	value &= ~TS1_START;
	writel_relaxed(value, sensor->base + DTS_CFGR1_OFFSET);

	 
	usleep_range(STARTUP_TIME, POLL_TIMEOUT);

	 
	value = readl_relaxed(sensor->base + DTS_CFGR1_OFFSET);
	value &= ~TS1_EN;
	writel_relaxed(value, sensor->base + DTS_CFGR1_OFFSET);

	 
	return readl_poll_timeout(sensor->base + DTS_SR_OFFSET, value,
				  !(value & TS_RDY),
				  STARTUP_TIME, POLL_TIMEOUT);
}

static int stm_thermal_calibration(struct stm_thermal_sensor *sensor)
{
	u32 value, clk_freq;
	u32 prescaler;

	 
	clk_freq = clk_get_rate(sensor->clk);
	if (!clk_freq)
		return -EINVAL;

	prescaler = 0;
	clk_freq /= ONE_MHZ;
	if (clk_freq) {
		while (prescaler <= clk_freq)
			prescaler++;
	}

	value = readl_relaxed(sensor->base + DTS_CFGR1_OFFSET);

	 
	value &= ~HSREF_CLK_DIV_MASK;

	 
	value |= (prescaler << HSREF_CLK_DIV_POS);

	 
	value &= ~REFCLK_SEL;

	 
	value |= TS1_SMP_TIME_MASK;

	 
	value &= ~CALIBRATION_CONTROL;

	 
	value &= ~TS1_INTRIG_SEL_MASK;
	value |= NO_HW_TRIG;

	writel_relaxed(value, sensor->base + DTS_CFGR1_OFFSET);

	return 0;
}

 
static int stm_thermal_read_factory_settings(struct stm_thermal_sensor *sensor)
{
	 
	sensor->t0 = readl_relaxed(sensor->base + DTS_T0VALR1_OFFSET) &
					TS1_T0_MASK;
	if (!sensor->t0)
		sensor->t0 = TS1_T0_VAL0;
	else
		sensor->t0 = TS1_T0_VAL1;

	 
	sensor->fmt0 = ADJUST * (readl_relaxed(sensor->base +
				 DTS_T0VALR1_OFFSET) & TS1_FMT0_MASK);

	 
	sensor->ramp_coeff = readl_relaxed(sensor->base + DTS_RAMPVALR_OFFSET) &
					   TS1_RAMP_COEFF_MASK;

	if (!sensor->fmt0 || !sensor->ramp_coeff) {
		dev_err(sensor->dev, "%s: wrong setting\n", __func__);
		return -EINVAL;
	}

	dev_dbg(sensor->dev, "%s: T0 = %doC, FMT0 = %dHz, RAMP_COEFF = %dHz/oC",
		__func__, sensor->t0, sensor->fmt0, sensor->ramp_coeff);

	return 0;
}

static int stm_thermal_calculate_threshold(struct stm_thermal_sensor *sensor,
					   int temp, u32 *th)
{
	int freqM;

	 
	freqM = ((temp - sensor->t0) * sensor->ramp_coeff) / 1000 +
		sensor->fmt0;

	 
	*th = clk_get_rate(sensor->clk) * SAMPLING_TIME / freqM;
	if (!*th)
		return -EINVAL;

	dev_dbg(sensor->dev, "freqM=%d Hz, threshold=0x%x", freqM, *th);

	return 0;
}

 
static int stm_disable_irq(struct stm_thermal_sensor *sensor)
{
	u32 value;

	 
	value = readl_relaxed(sensor->base + DTS_ITENR_OFFSET);
	value &= ~ITENR_MASK;
	writel_relaxed(value, sensor->base + DTS_ITENR_OFFSET);

	return 0;
}

static int stm_thermal_set_trips(struct thermal_zone_device *tz, int low, int high)
{
	struct stm_thermal_sensor *sensor = thermal_zone_device_priv(tz);
	u32 itr1, th;
	int ret;

	dev_dbg(sensor->dev, "set trips %d <--> %d\n", low, high);

	 
	itr1 = readl_relaxed(sensor->base + DTS_ITR1_OFFSET);
	itr1 &= ~(TS1_LITTHD_MASK | TS1_HITTHD_MASK);

	 

	if (low > -INT_MAX) {
		sensor->low_temp_enabled = 1;
		 
		ret = stm_thermal_calculate_threshold(sensor, low - 500, &th);
		if (ret)
			return ret;

		itr1 |= (TS1_HITTHD_MASK  & (th << TS1_HITTHD_POS));
	} else {
		sensor->low_temp_enabled = 0;
	}

	 
	if (high < INT_MAX) {
		sensor->high_temp_enabled = 1;
		ret = stm_thermal_calculate_threshold(sensor, high, &th);
		if (ret)
			return ret;

		itr1 |= (TS1_LITTHD_MASK  & (th << TS1_LITTHD_POS));
	} else {
		sensor->high_temp_enabled = 0;
	}

	 
	writel_relaxed(itr1, sensor->base + DTS_ITR1_OFFSET);

	return 0;
}

 
static int stm_thermal_get_temp(struct thermal_zone_device *tz, int *temp)
{
	struct stm_thermal_sensor *sensor = thermal_zone_device_priv(tz);
	u32 periods;
	int freqM, ret;

	if (sensor->mode != THERMAL_DEVICE_ENABLED)
		return -EAGAIN;

	 
	ret = readl_relaxed_poll_timeout(sensor->base + DTS_DR_OFFSET, periods,
					 (periods & TS1_MFREQ_MASK),
					 STARTUP_TIME, POLL_TIMEOUT);
	if (ret)
		return ret;

	 
	freqM = (clk_get_rate(sensor->clk) * SAMPLING_TIME) / periods;
	if (!freqM)
		return -EINVAL;

	 
	*temp = (freqM - sensor->fmt0) * 1000 / sensor->ramp_coeff + sensor->t0;

	return 0;
}

 
static int stm_register_irq(struct stm_thermal_sensor *sensor)
{
	struct device *dev = sensor->dev;
	struct platform_device *pdev = to_platform_device(dev);
	int ret;

	sensor->irq = platform_get_irq(pdev, 0);
	if (sensor->irq < 0)
		return sensor->irq;

	ret = devm_request_threaded_irq(dev, sensor->irq,
					NULL,
					stm_thermal_irq_handler,
					IRQF_ONESHOT,
					dev->driver->name, sensor);
	if (ret) {
		dev_err(dev, "%s: Failed to register IRQ %d\n", __func__,
			sensor->irq);
		return ret;
	}

	dev_dbg(dev, "%s: thermal IRQ registered", __func__);

	return 0;
}

static int stm_thermal_sensor_off(struct stm_thermal_sensor *sensor)
{
	int ret;

	stm_disable_irq(sensor);

	ret = stm_sensor_power_off(sensor);
	if (ret)
		return ret;

	clk_disable_unprepare(sensor->clk);

	return 0;
}

static int stm_thermal_prepare(struct stm_thermal_sensor *sensor)
{
	int ret;

	ret = clk_prepare_enable(sensor->clk);
	if (ret)
		return ret;

	ret = stm_thermal_read_factory_settings(sensor);
	if (ret)
		goto thermal_unprepare;

	ret = stm_thermal_calibration(sensor);
	if (ret)
		goto thermal_unprepare;

	return 0;

thermal_unprepare:
	clk_disable_unprepare(sensor->clk);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int stm_thermal_suspend(struct device *dev)
{
	struct stm_thermal_sensor *sensor = dev_get_drvdata(dev);

	return stm_thermal_sensor_off(sensor);
}

static int stm_thermal_resume(struct device *dev)
{
	int ret;
	struct stm_thermal_sensor *sensor = dev_get_drvdata(dev);

	ret = stm_thermal_prepare(sensor);
	if (ret)
		return ret;

	ret = stm_sensor_power_on(sensor);
	if (ret)
		return ret;

	thermal_zone_device_update(sensor->th_dev, THERMAL_EVENT_UNSPECIFIED);
	stm_enable_irq(sensor);

	return 0;
}
#endif  

static SIMPLE_DEV_PM_OPS(stm_thermal_pm_ops,
			 stm_thermal_suspend, stm_thermal_resume);

static const struct thermal_zone_device_ops stm_tz_ops = {
	.get_temp	= stm_thermal_get_temp,
	.set_trips	= stm_thermal_set_trips,
};

static const struct of_device_id stm_thermal_of_match[] = {
		{ .compatible = "st,stm32-thermal"},
	{   }
};
MODULE_DEVICE_TABLE(of, stm_thermal_of_match);

static int stm_thermal_probe(struct platform_device *pdev)
{
	struct stm_thermal_sensor *sensor;
	void __iomem *base;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "%s: device tree node not found\n",
			__func__);
		return -EINVAL;
	}

	sensor = devm_kzalloc(&pdev->dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	platform_set_drvdata(pdev, sensor);

	sensor->dev = &pdev->dev;

	base = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(base))
		return PTR_ERR(base);

	 
	sensor->base = base;

	sensor->clk = devm_clk_get(&pdev->dev, "pclk");
	if (IS_ERR(sensor->clk)) {
		dev_err(&pdev->dev, "%s: failed to fetch PCLK clock\n",
			__func__);
		return PTR_ERR(sensor->clk);
	}

	stm_disable_irq(sensor);

	 
	writel_relaxed(ICIFR_MASK, sensor->base + DTS_ICIFR_OFFSET);

	 
	ret = stm_thermal_prepare(sensor);
	if (ret) {
		dev_err(&pdev->dev, "Error prepare sensor: %d\n", ret);
		return ret;
	}

	ret = stm_sensor_power_on(sensor);
	if (ret) {
		dev_err(&pdev->dev, "Error power on sensor: %d\n", ret);
		return ret;
	}

	sensor->th_dev = devm_thermal_of_zone_register(&pdev->dev, 0,
						       sensor,
						       &stm_tz_ops);

	if (IS_ERR(sensor->th_dev)) {
		dev_err(&pdev->dev, "%s: thermal zone sensor registering KO\n",
			__func__);
		ret = PTR_ERR(sensor->th_dev);
		return ret;
	}

	 
	ret = stm_register_irq(sensor);
	if (ret)
		goto err_tz;

	stm_enable_irq(sensor);

	 
	ret = thermal_add_hwmon_sysfs(sensor->th_dev);
	if (ret)
		goto err_tz;

	dev_info(&pdev->dev, "%s: Driver initialized successfully\n",
		 __func__);

	return 0;

err_tz:
	return ret;
}

static int stm_thermal_remove(struct platform_device *pdev)
{
	struct stm_thermal_sensor *sensor = platform_get_drvdata(pdev);

	stm_thermal_sensor_off(sensor);
	thermal_remove_hwmon_sysfs(sensor->th_dev);

	return 0;
}

static struct platform_driver stm_thermal_driver = {
	.driver = {
		.name	= "stm_thermal",
		.pm     = &stm_thermal_pm_ops,
		.of_match_table = stm_thermal_of_match,
	},
	.probe		= stm_thermal_probe,
	.remove		= stm_thermal_remove,
};
module_platform_driver(stm_thermal_driver);

MODULE_DESCRIPTION("STMicroelectronics STM32 Thermal Sensor Driver");
MODULE_AUTHOR("David Hernandez Sanchez <david.hernandezsanchez@st.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:stm_thermal");
