
 

#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>
#include <sound/soc.h>

#include "adau1372.h"

static void adau1372_spi_switch_mode(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);

	 
	spi_w8r8(spi, 0x00);
	spi_w8r8(spi, 0x00);
	spi_w8r8(spi, 0x00);
}

static int adau1372_spi_probe(struct spi_device *spi)
{
	struct regmap_config config;

	config = adau1372_regmap_config;
	config.read_flag_mask = 0x1;

	return adau1372_probe(&spi->dev,
		devm_regmap_init_spi(spi, &config), adau1372_spi_switch_mode);
}

static const struct spi_device_id adau1372_spi_id[] = {
	{ "adau1372", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, adau1372_spi_id);

static struct spi_driver adau1372_spi_driver = {
	.driver = {
		.name = "adau1372",
	},
	.probe = adau1372_spi_probe,
	.id_table = adau1372_spi_id,
};
module_spi_driver(adau1372_spi_driver);

MODULE_DESCRIPTION("ASoC ADAU1372 CODEC SPI driver");
MODULE_AUTHOR("Lars-Peter Clausen <lars@metafoo.de>");
MODULE_LICENSE("GPL v2");
