
 

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>

#include "max9271.h"

static int max9271_read(struct max9271_device *dev, u8 reg)
{
	int ret;

	dev_dbg(&dev->client->dev, "%s(0x%02x)\n", __func__, reg);

	ret = i2c_smbus_read_byte_data(dev->client, reg);
	if (ret < 0)
		dev_dbg(&dev->client->dev,
			"%s: register 0x%02x read failed (%d)\n",
			__func__, reg, ret);

	return ret;
}

static int max9271_write(struct max9271_device *dev, u8 reg, u8 val)
{
	int ret;

	dev_dbg(&dev->client->dev, "%s(0x%02x, 0x%02x)\n", __func__, reg, val);

	ret = i2c_smbus_write_byte_data(dev->client, reg, val);
	if (ret < 0)
		dev_err(&dev->client->dev,
			"%s: register 0x%02x write failed (%d)\n",
			__func__, reg, ret);

	return ret;
}

 
static int max9271_pclk_detect(struct max9271_device *dev)
{
	unsigned int i;
	int ret;

	for (i = 0; i < 100; i++) {
		ret = max9271_read(dev, 0x15);
		if (ret < 0)
			return ret;

		if (ret & MAX9271_PCLKDET)
			return 0;

		usleep_range(50, 100);
	}

	dev_err(&dev->client->dev, "Unable to detect valid pixel clock\n");

	return -EIO;
}

void max9271_wake_up(struct max9271_device *dev)
{
	 
	dev->client->addr = MAX9271_DEFAULT_ADDR;
	i2c_smbus_read_byte(dev->client);
	usleep_range(5000, 8000);
}
EXPORT_SYMBOL_GPL(max9271_wake_up);

int max9271_set_serial_link(struct max9271_device *dev, bool enable)
{
	int ret;
	u8 val = MAX9271_REVCCEN | MAX9271_FWDCCEN;

	if (enable) {
		ret = max9271_pclk_detect(dev);
		if (ret)
			return ret;

		val |= MAX9271_SEREN;
	} else {
		val |= MAX9271_CLINKEN;
	}

	 
	ret = max9271_write(dev, 0x04, val);
	if (ret < 0)
		return ret;

	usleep_range(5000, 8000);

	return 0;
}
EXPORT_SYMBOL_GPL(max9271_set_serial_link);

int max9271_configure_i2c(struct max9271_device *dev, u8 i2c_config)
{
	int ret;

	ret = max9271_write(dev, 0x0d, i2c_config);
	if (ret < 0)
		return ret;

	 
	usleep_range(3500, 5000);

	return 0;
}
EXPORT_SYMBOL_GPL(max9271_configure_i2c);

int max9271_set_high_threshold(struct max9271_device *dev, bool enable)
{
	int ret;

	ret = max9271_read(dev, 0x08);
	if (ret < 0)
		return ret;

	 
	ret = max9271_write(dev, 0x08, enable ? ret | BIT(0) : ret & ~BIT(0));
	if (ret < 0)
		return ret;

	usleep_range(2000, 2500);

	return 0;
}
EXPORT_SYMBOL_GPL(max9271_set_high_threshold);

int max9271_configure_gmsl_link(struct max9271_device *dev)
{
	int ret;

	 
	ret = max9271_write(dev, 0x07, MAX9271_DBL | MAX9271_HVEN |
			    MAX9271_EDC_1BIT_PARITY);
	if (ret < 0)
		return ret;

	usleep_range(5000, 8000);

	 
	ret = max9271_write(dev, 0x02,
			    MAX9271_SPREAD_SPECT_4 | MAX9271_R02_RES |
			    MAX9271_PCLK_AUTODETECT |
			    MAX9271_SERIAL_AUTODETECT);
	if (ret < 0)
		return ret;

	usleep_range(5000, 8000);

	return 0;
}
EXPORT_SYMBOL_GPL(max9271_configure_gmsl_link);

int max9271_set_gpios(struct max9271_device *dev, u8 gpio_mask)
{
	int ret;

	ret = max9271_read(dev, 0x0f);
	if (ret < 0)
		return 0;

	ret |= gpio_mask;
	ret = max9271_write(dev, 0x0f, ret);
	if (ret < 0) {
		dev_err(&dev->client->dev, "Failed to set gpio (%d)\n", ret);
		return ret;
	}

	usleep_range(3500, 5000);

	return 0;
}
EXPORT_SYMBOL_GPL(max9271_set_gpios);

int max9271_clear_gpios(struct max9271_device *dev, u8 gpio_mask)
{
	int ret;

	ret = max9271_read(dev, 0x0f);
	if (ret < 0)
		return 0;

	ret &= ~gpio_mask;
	ret = max9271_write(dev, 0x0f, ret);
	if (ret < 0) {
		dev_err(&dev->client->dev, "Failed to clear gpio (%d)\n", ret);
		return ret;
	}

	usleep_range(3500, 5000);

	return 0;
}
EXPORT_SYMBOL_GPL(max9271_clear_gpios);

int max9271_enable_gpios(struct max9271_device *dev, u8 gpio_mask)
{
	int ret;

	ret = max9271_read(dev, 0x0e);
	if (ret < 0)
		return 0;

	 
	ret |= (gpio_mask & ~BIT(0));
	ret = max9271_write(dev, 0x0e, ret);
	if (ret < 0) {
		dev_err(&dev->client->dev, "Failed to enable gpio (%d)\n", ret);
		return ret;
	}

	usleep_range(3500, 5000);

	return 0;
}
EXPORT_SYMBOL_GPL(max9271_enable_gpios);

int max9271_disable_gpios(struct max9271_device *dev, u8 gpio_mask)
{
	int ret;

	ret = max9271_read(dev, 0x0e);
	if (ret < 0)
		return 0;

	 
	ret &= ~(gpio_mask | BIT(0));
	ret = max9271_write(dev, 0x0e, ret);
	if (ret < 0) {
		dev_err(&dev->client->dev, "Failed to disable gpio (%d)\n", ret);
		return ret;
	}

	usleep_range(3500, 5000);

	return 0;
}
EXPORT_SYMBOL_GPL(max9271_disable_gpios);

int max9271_verify_id(struct max9271_device *dev)
{
	int ret;

	ret = max9271_read(dev, 0x1e);
	if (ret < 0) {
		dev_err(&dev->client->dev, "MAX9271 ID read failed (%d)\n",
			ret);
		return ret;
	}

	if (ret != MAX9271_ID) {
		dev_err(&dev->client->dev, "MAX9271 ID mismatch (0x%02x)\n",
			ret);
		return -ENXIO;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(max9271_verify_id);

int max9271_set_address(struct max9271_device *dev, u8 addr)
{
	int ret;

	ret = max9271_write(dev, 0x00, addr << 1);
	if (ret < 0) {
		dev_err(&dev->client->dev,
			"MAX9271 I2C address change failed (%d)\n", ret);
		return ret;
	}
	usleep_range(3500, 5000);

	return 0;
}
EXPORT_SYMBOL_GPL(max9271_set_address);

int max9271_set_deserializer_address(struct max9271_device *dev, u8 addr)
{
	int ret;

	ret = max9271_write(dev, 0x01, addr << 1);
	if (ret < 0) {
		dev_err(&dev->client->dev,
			"MAX9271 deserializer address set failed (%d)\n", ret);
		return ret;
	}
	usleep_range(3500, 5000);

	return 0;
}
EXPORT_SYMBOL_GPL(max9271_set_deserializer_address);

int max9271_set_translation(struct max9271_device *dev, u8 source, u8 dest)
{
	int ret;

	ret = max9271_write(dev, 0x09, source << 1);
	if (ret < 0) {
		dev_err(&dev->client->dev,
			"MAX9271 I2C translation setup failed (%d)\n", ret);
		return ret;
	}
	usleep_range(3500, 5000);

	ret = max9271_write(dev, 0x0a, dest << 1);
	if (ret < 0) {
		dev_err(&dev->client->dev,
			"MAX9271 I2C translation setup failed (%d)\n", ret);
		return ret;
	}
	usleep_range(3500, 5000);

	return 0;
}
EXPORT_SYMBOL_GPL(max9271_set_translation);

MODULE_DESCRIPTION("Maxim MAX9271 GMSL Serializer");
MODULE_AUTHOR("Jacopo Mondi");
MODULE_LICENSE("GPL v2");
