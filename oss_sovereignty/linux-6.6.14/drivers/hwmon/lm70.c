
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/hwmon.h>
#include <linux/mutex.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/property.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>

#define DRVNAME		"lm70"

#define LM70_CHIP_LM70		0	 
#define LM70_CHIP_TMP121	1	 
#define LM70_CHIP_LM71		2	 
#define LM70_CHIP_LM74		3	 
#define LM70_CHIP_TMP122	4	 
#define LM70_CHIP_TMP125	5	 

struct lm70 {
	struct spi_device *spi;
	struct mutex lock;
	unsigned int chip;
};

 
static ssize_t temp1_input_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lm70 *p_lm70 = dev_get_drvdata(dev);
	struct spi_device *spi = p_lm70->spi;
	int status, val = 0;
	u8 rxbuf[2];
	s16 raw = 0;

	if (mutex_lock_interruptible(&p_lm70->lock))
		return -ERESTARTSYS;

	 
	status = spi_write_then_read(spi, NULL, 0, &rxbuf[0], 2);
	if (status < 0) {
		dev_warn(dev, "spi_write_then_read failed with status %d\n",
			 status);
		goto out;
	}
	raw = (rxbuf[0] << 8) + rxbuf[1];
	dev_dbg(dev, "rxbuf[0] : 0x%02x rxbuf[1] : 0x%02x raw=0x%04x\n",
		rxbuf[0], rxbuf[1], raw);

	 
	switch (p_lm70->chip) {
	case LM70_CHIP_LM70:
		val = ((int)raw / 32) * 250;
		break;

	case LM70_CHIP_TMP121:
	case LM70_CHIP_TMP122:
	case LM70_CHIP_LM74:
		val = ((int)raw / 8) * 625 / 10;
		break;

	case LM70_CHIP_LM71:
		val = ((int)raw / 4) * 3125 / 100;
		break;

	case LM70_CHIP_TMP125:
		val = (sign_extend32(raw, 14) / 32) * 250;
		break;
	}

	status = sprintf(buf, "%d\n", val);  
out:
	mutex_unlock(&p_lm70->lock);
	return status;
}

static DEVICE_ATTR_RO(temp1_input);

static struct attribute *lm70_attrs[] = {
	&dev_attr_temp1_input.attr,
	NULL
};

ATTRIBUTE_GROUPS(lm70);

 

#ifdef CONFIG_OF
static const struct of_device_id lm70_of_ids[] = {
	{
		.compatible = "ti,lm70",
		.data = (void *) LM70_CHIP_LM70,
	},
	{
		.compatible = "ti,tmp121",
		.data = (void *) LM70_CHIP_TMP121,
	},
	{
		.compatible = "ti,tmp122",
		.data = (void *) LM70_CHIP_TMP122,
	},
	{
		.compatible = "ti,tmp125",
		.data = (void *) LM70_CHIP_TMP125,
	},
	{
		.compatible = "ti,lm71",
		.data = (void *) LM70_CHIP_LM71,
	},
	{
		.compatible = "ti,lm74",
		.data = (void *) LM70_CHIP_LM74,
	},
	{},
};
MODULE_DEVICE_TABLE(of, lm70_of_ids);
#endif

static int lm70_probe(struct spi_device *spi)
{
	struct device *hwmon_dev;
	struct lm70 *p_lm70;
	int chip;

	if (dev_fwnode(&spi->dev))
		chip = (int)(uintptr_t)device_get_match_data(&spi->dev);
	else
		chip = spi_get_device_id(spi)->driver_data;


	 
	if ((spi->mode & SPI_MODE_X_MASK) != SPI_MODE_0)
		return -EINVAL;

	 

	p_lm70 = devm_kzalloc(&spi->dev, sizeof(*p_lm70), GFP_KERNEL);
	if (!p_lm70)
		return -ENOMEM;

	mutex_init(&p_lm70->lock);
	p_lm70->chip = chip;
	p_lm70->spi = spi;

	hwmon_dev = devm_hwmon_device_register_with_groups(&spi->dev,
							   spi->modalias,
							   p_lm70, lm70_groups);
	return PTR_ERR_OR_ZERO(hwmon_dev);
}

static const struct spi_device_id lm70_ids[] = {
	{ "lm70",   LM70_CHIP_LM70 },
	{ "tmp121", LM70_CHIP_TMP121 },
	{ "tmp122", LM70_CHIP_TMP122 },
	{ "tmp125", LM70_CHIP_TMP125 },
	{ "lm71",   LM70_CHIP_LM71 },
	{ "lm74",   LM70_CHIP_LM74 },
	{ },
};
MODULE_DEVICE_TABLE(spi, lm70_ids);

static struct spi_driver lm70_driver = {
	.driver = {
		.name	= "lm70",
		.of_match_table	= of_match_ptr(lm70_of_ids),
	},
	.id_table = lm70_ids,
	.probe	= lm70_probe,
};

module_spi_driver(lm70_driver);

MODULE_AUTHOR("Kaiwan N Billimoria");
MODULE_DESCRIPTION("NS LM70 and compatibles Linux driver");
MODULE_LICENSE("GPL");
