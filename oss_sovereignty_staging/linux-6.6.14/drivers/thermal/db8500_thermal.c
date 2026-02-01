
 

#include <linux/cpu_cooling.h>
#include <linux/interrupt.h>
#include <linux/mfd/dbx500-prcmu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>

#define PRCMU_DEFAULT_MEASURE_TIME	0xFFF
#define PRCMU_DEFAULT_LOW_TEMP		0

 
static const unsigned long db8500_thermal_points[] = {
	15000,
	20000,
	25000,
	30000,
	35000,
	40000,
	45000,
	50000,
	55000,
	60000,
	65000,
	70000,
	75000,
	80000,
	 
	85000,
	90000,
	95000,
	100000,
};

struct db8500_thermal_zone {
	struct thermal_zone_device *tz;
	struct device *dev;
	unsigned long interpolated_temp;
	unsigned int cur_index;
};

 
static int db8500_thermal_get_temp(struct thermal_zone_device *tz, int *temp)
{
	struct db8500_thermal_zone *th = thermal_zone_device_priv(tz);

	 
	*temp = th->interpolated_temp;

	return 0;
}

static const struct thermal_zone_device_ops thdev_ops = {
	.get_temp = db8500_thermal_get_temp,
};

static void db8500_thermal_update_config(struct db8500_thermal_zone *th,
					 unsigned int idx,
					 unsigned long next_low,
					 unsigned long next_high)
{
	prcmu_stop_temp_sense();

	th->cur_index = idx;
	th->interpolated_temp = (next_low + next_high)/2;

	 
	prcmu_config_hotmon((u8)(next_low/1000), (u8)(next_high/1000));
	prcmu_start_temp_sense(PRCMU_DEFAULT_MEASURE_TIME);
}

static irqreturn_t prcmu_low_irq_handler(int irq, void *irq_data)
{
	struct db8500_thermal_zone *th = irq_data;
	unsigned int idx = th->cur_index;
	unsigned long next_low, next_high;

	if (idx == 0)
		 
		return IRQ_HANDLED;

	if (idx == 1) {
		next_high = db8500_thermal_points[0];
		next_low = PRCMU_DEFAULT_LOW_TEMP;
	} else {
		next_high = db8500_thermal_points[idx - 1];
		next_low = db8500_thermal_points[idx - 2];
	}
	idx -= 1;

	db8500_thermal_update_config(th, idx, next_low, next_high);
	dev_dbg(th->dev,
		"PRCMU set max %ld, min %ld\n", next_high, next_low);

	thermal_zone_device_update(th->tz, THERMAL_EVENT_UNSPECIFIED);

	return IRQ_HANDLED;
}

static irqreturn_t prcmu_high_irq_handler(int irq, void *irq_data)
{
	struct db8500_thermal_zone *th = irq_data;
	unsigned int idx = th->cur_index;
	unsigned long next_low, next_high;
	int num_points = ARRAY_SIZE(db8500_thermal_points);

	if (idx < num_points - 1) {
		next_high = db8500_thermal_points[idx+1];
		next_low = db8500_thermal_points[idx];
		idx += 1;

		db8500_thermal_update_config(th, idx, next_low, next_high);

		dev_dbg(th->dev,
			"PRCMU set max %ld, min %ld\n", next_high, next_low);
	} else if (idx == num_points - 1)
		 
		th->interpolated_temp = db8500_thermal_points[idx] + 1;

	thermal_zone_device_update(th->tz, THERMAL_EVENT_UNSPECIFIED);

	return IRQ_HANDLED;
}

static int db8500_thermal_probe(struct platform_device *pdev)
{
	struct db8500_thermal_zone *th = NULL;
	struct device *dev = &pdev->dev;
	int low_irq, high_irq, ret = 0;

	th = devm_kzalloc(dev, sizeof(*th), GFP_KERNEL);
	if (!th)
		return -ENOMEM;

	th->dev = dev;

	low_irq = platform_get_irq_byname(pdev, "IRQ_HOTMON_LOW");
	if (low_irq < 0)
		return low_irq;

	ret = devm_request_threaded_irq(dev, low_irq, NULL,
		prcmu_low_irq_handler, IRQF_NO_SUSPEND | IRQF_ONESHOT,
		"dbx500_temp_low", th);
	if (ret < 0) {
		dev_err(dev, "failed to allocate temp low irq\n");
		return ret;
	}

	high_irq = platform_get_irq_byname(pdev, "IRQ_HOTMON_HIGH");
	if (high_irq < 0)
		return high_irq;

	ret = devm_request_threaded_irq(dev, high_irq, NULL,
		prcmu_high_irq_handler, IRQF_NO_SUSPEND | IRQF_ONESHOT,
		"dbx500_temp_high", th);
	if (ret < 0) {
		dev_err(dev, "failed to allocate temp high irq\n");
		return ret;
	}

	 
	th->tz = devm_thermal_of_zone_register(dev, 0, th, &thdev_ops);
	if (IS_ERR(th->tz)) {
		dev_err(dev, "register thermal zone sensor failed\n");
		return PTR_ERR(th->tz);
	}
	dev_info(dev, "thermal zone sensor registered\n");

	 
	db8500_thermal_update_config(th, 0, PRCMU_DEFAULT_LOW_TEMP,
				     db8500_thermal_points[0]);

	platform_set_drvdata(pdev, th);

	return 0;
}

static int db8500_thermal_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	prcmu_stop_temp_sense();

	return 0;
}

static int db8500_thermal_resume(struct platform_device *pdev)
{
	struct db8500_thermal_zone *th = platform_get_drvdata(pdev);

	 
	db8500_thermal_update_config(th, 0, PRCMU_DEFAULT_LOW_TEMP,
				     db8500_thermal_points[0]);

	return 0;
}

static const struct of_device_id db8500_thermal_match[] = {
	{ .compatible = "stericsson,db8500-thermal" },
	{},
};
MODULE_DEVICE_TABLE(of, db8500_thermal_match);

static struct platform_driver db8500_thermal_driver = {
	.driver = {
		.name = "db8500-thermal",
		.of_match_table = db8500_thermal_match,
	},
	.probe = db8500_thermal_probe,
	.suspend = db8500_thermal_suspend,
	.resume = db8500_thermal_resume,
};

module_platform_driver(db8500_thermal_driver);

MODULE_AUTHOR("Hongbo Zhang <hongbo.zhang@stericsson.com>");
MODULE_DESCRIPTION("DB8500 thermal driver");
MODULE_LICENSE("GPL");
