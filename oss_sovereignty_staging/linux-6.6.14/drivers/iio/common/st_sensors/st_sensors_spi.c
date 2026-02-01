
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/iio/iio.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>

#include <linux/iio/common/st_sensors_spi.h>

#define ST_SENSORS_SPI_MULTIREAD	0xc0

static const struct regmap_config st_sensors_spi_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static const struct regmap_config st_sensors_spi_regmap_multiread_bit_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.read_flag_mask = ST_SENSORS_SPI_MULTIREAD,
};

 
static bool st_sensors_is_spi_3_wire(struct spi_device *spi)
{
	struct st_sensors_platform_data *pdata;
	struct device *dev = &spi->dev;

	if (device_property_read_bool(dev, "spi-3wire"))
		return true;

	pdata = dev_get_platdata(dev);
	if (pdata && pdata->spi_3wire)
		return true;

	return false;
}

 
static int st_sensors_configure_spi_3_wire(struct spi_device *spi,
					   struct st_sensor_settings *settings)
{
	if (settings->sim.addr) {
		u8 buffer[] = {
			settings->sim.addr,
			settings->sim.value
		};

		return spi_write(spi, buffer, 2);
	}

	return 0;
}

 
int st_sensors_spi_configure(struct iio_dev *indio_dev,
			     struct spi_device *spi)
{
	struct st_sensor_data *sdata = iio_priv(indio_dev);
	const struct regmap_config *config;
	int err;

	if (st_sensors_is_spi_3_wire(spi)) {
		err = st_sensors_configure_spi_3_wire(spi,
						      sdata->sensor_settings);
		if (err < 0)
			return err;
	}

	if (sdata->sensor_settings->multi_read_bit)
		config = &st_sensors_spi_regmap_multiread_bit_config;
	else
		config = &st_sensors_spi_regmap_config;

	sdata->regmap = devm_regmap_init_spi(spi, config);
	if (IS_ERR(sdata->regmap)) {
		dev_err(&spi->dev, "Failed to register spi regmap (%ld)\n",
			PTR_ERR(sdata->regmap));
		return PTR_ERR(sdata->regmap);
	}

	spi_set_drvdata(spi, indio_dev);

	indio_dev->name = spi->modalias;

	sdata->irq = spi->irq;

	return 0;
}
EXPORT_SYMBOL_NS(st_sensors_spi_configure, IIO_ST_SENSORS);

MODULE_AUTHOR("Denis Ciocca <denis.ciocca@st.com>");
MODULE_DESCRIPTION("STMicroelectronics ST-sensors spi driver");
MODULE_LICENSE("GPL v2");
