
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>

#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/spi/spi_gpio.h>


 

struct spi_gpio {
	struct spi_bitbang		bitbang;
	struct gpio_desc		*sck;
	struct gpio_desc		*miso;
	struct gpio_desc		*mosi;
	struct gpio_desc		**cs_gpios;
};

 

 

#ifndef DRIVER_NAME
#define DRIVER_NAME	"spi_gpio"

#define GENERIC_BITBANG	 

#endif

 

static inline struct spi_gpio *__pure
spi_to_spi_gpio(const struct spi_device *spi)
{
	const struct spi_bitbang	*bang;
	struct spi_gpio			*spi_gpio;

	bang = spi_controller_get_devdata(spi->controller);
	spi_gpio = container_of(bang, struct spi_gpio, bitbang);
	return spi_gpio;
}

 
static inline void setsck(const struct spi_device *spi, int is_on)
{
	struct spi_gpio *spi_gpio = spi_to_spi_gpio(spi);

	gpiod_set_value_cansleep(spi_gpio->sck, is_on);
}

static inline void setmosi(const struct spi_device *spi, int is_on)
{
	struct spi_gpio *spi_gpio = spi_to_spi_gpio(spi);

	gpiod_set_value_cansleep(spi_gpio->mosi, is_on);
}

static inline int getmiso(const struct spi_device *spi)
{
	struct spi_gpio *spi_gpio = spi_to_spi_gpio(spi);

	if (spi->mode & SPI_3WIRE)
		return !!gpiod_get_value_cansleep(spi_gpio->mosi);
	else
		return !!gpiod_get_value_cansleep(spi_gpio->miso);
}

 
#define spidelay(nsecs)	do {} while (0)

#include "spi-bitbang-txrx.h"

 

static u32 spi_gpio_txrx_word_mode0(struct spi_device *spi,
		unsigned nsecs, u32 word, u8 bits, unsigned flags)
{
	if (unlikely(spi->mode & SPI_LSB_FIRST))
		return bitbang_txrx_le_cpha0(spi, nsecs, 0, flags, word, bits);
	else
		return bitbang_txrx_be_cpha0(spi, nsecs, 0, flags, word, bits);
}

static u32 spi_gpio_txrx_word_mode1(struct spi_device *spi,
		unsigned nsecs, u32 word, u8 bits, unsigned flags)
{
	if (unlikely(spi->mode & SPI_LSB_FIRST))
		return bitbang_txrx_le_cpha1(spi, nsecs, 0, flags, word, bits);
	else
		return bitbang_txrx_be_cpha1(spi, nsecs, 0, flags, word, bits);
}

static u32 spi_gpio_txrx_word_mode2(struct spi_device *spi,
		unsigned nsecs, u32 word, u8 bits, unsigned flags)
{
	if (unlikely(spi->mode & SPI_LSB_FIRST))
		return bitbang_txrx_le_cpha0(spi, nsecs, 1, flags, word, bits);
	else
		return bitbang_txrx_be_cpha0(spi, nsecs, 1, flags, word, bits);
}

static u32 spi_gpio_txrx_word_mode3(struct spi_device *spi,
		unsigned nsecs, u32 word, u8 bits, unsigned flags)
{
	if (unlikely(spi->mode & SPI_LSB_FIRST))
		return bitbang_txrx_le_cpha1(spi, nsecs, 1, flags, word, bits);
	else
		return bitbang_txrx_be_cpha1(spi, nsecs, 1, flags, word, bits);
}

 

static u32 spi_gpio_spec_txrx_word_mode0(struct spi_device *spi,
		unsigned nsecs, u32 word, u8 bits, unsigned flags)
{
	flags = spi->controller->flags;
	if (unlikely(spi->mode & SPI_LSB_FIRST))
		return bitbang_txrx_le_cpha0(spi, nsecs, 0, flags, word, bits);
	else
		return bitbang_txrx_be_cpha0(spi, nsecs, 0, flags, word, bits);
}

static u32 spi_gpio_spec_txrx_word_mode1(struct spi_device *spi,
		unsigned nsecs, u32 word, u8 bits, unsigned flags)
{
	flags = spi->controller->flags;
	if (unlikely(spi->mode & SPI_LSB_FIRST))
		return bitbang_txrx_le_cpha1(spi, nsecs, 0, flags, word, bits);
	else
		return bitbang_txrx_be_cpha1(spi, nsecs, 0, flags, word, bits);
}

static u32 spi_gpio_spec_txrx_word_mode2(struct spi_device *spi,
		unsigned nsecs, u32 word, u8 bits, unsigned flags)
{
	flags = spi->controller->flags;
	if (unlikely(spi->mode & SPI_LSB_FIRST))
		return bitbang_txrx_le_cpha0(spi, nsecs, 1, flags, word, bits);
	else
		return bitbang_txrx_be_cpha0(spi, nsecs, 1, flags, word, bits);
}

static u32 spi_gpio_spec_txrx_word_mode3(struct spi_device *spi,
		unsigned nsecs, u32 word, u8 bits, unsigned flags)
{
	flags = spi->controller->flags;
	if (unlikely(spi->mode & SPI_LSB_FIRST))
		return bitbang_txrx_le_cpha1(spi, nsecs, 1, flags, word, bits);
	else
		return bitbang_txrx_be_cpha1(spi, nsecs, 1, flags, word, bits);
}

 

static void spi_gpio_chipselect(struct spi_device *spi, int is_active)
{
	struct spi_gpio *spi_gpio = spi_to_spi_gpio(spi);

	 
	if (is_active)
		gpiod_set_value_cansleep(spi_gpio->sck, spi->mode & SPI_CPOL);

	 
	if (spi_gpio->cs_gpios) {
		struct gpio_desc *cs = spi_gpio->cs_gpios[spi_get_chipselect(spi, 0)];

		 
		gpiod_set_value_cansleep(cs, (spi->mode & SPI_CS_HIGH) ? is_active : !is_active);
	}
}

static int spi_gpio_setup(struct spi_device *spi)
{
	struct gpio_desc	*cs;
	int			status = 0;
	struct spi_gpio		*spi_gpio = spi_to_spi_gpio(spi);

	 
	if (spi_gpio->cs_gpios) {
		cs = spi_gpio->cs_gpios[spi_get_chipselect(spi, 0)];
		if (!spi->controller_state && cs)
			status = gpiod_direction_output(cs,
						  !(spi->mode & SPI_CS_HIGH));
	}

	if (!status)
		status = spi_bitbang_setup(spi);

	return status;
}

static int spi_gpio_set_direction(struct spi_device *spi, bool output)
{
	struct spi_gpio *spi_gpio = spi_to_spi_gpio(spi);
	int ret;

	if (output)
		return gpiod_direction_output(spi_gpio->mosi, 1);

	 
	if (spi->mode & SPI_3WIRE) {
		ret = gpiod_direction_input(spi_gpio->mosi);
		if (ret)
			return ret;
	}
	 
	if (spi->mode & SPI_3WIRE_HIZ) {
		gpiod_set_value_cansleep(spi_gpio->sck,
					 !(spi->mode & SPI_CPOL));
		gpiod_set_value_cansleep(spi_gpio->sck,
					 !!(spi->mode & SPI_CPOL));
	}
	return 0;
}

static void spi_gpio_cleanup(struct spi_device *spi)
{
	spi_bitbang_cleanup(spi);
}

 
static int spi_gpio_request(struct device *dev, struct spi_gpio *spi_gpio)
{
	spi_gpio->mosi = devm_gpiod_get_optional(dev, "mosi", GPIOD_OUT_LOW);
	if (IS_ERR(spi_gpio->mosi))
		return PTR_ERR(spi_gpio->mosi);

	spi_gpio->miso = devm_gpiod_get_optional(dev, "miso", GPIOD_IN);
	if (IS_ERR(spi_gpio->miso))
		return PTR_ERR(spi_gpio->miso);

	spi_gpio->sck = devm_gpiod_get(dev, "sck", GPIOD_OUT_LOW);
	return PTR_ERR_OR_ZERO(spi_gpio->sck);
}

#ifdef CONFIG_OF
static const struct of_device_id spi_gpio_dt_ids[] = {
	{ .compatible = "spi-gpio" },
	{}
};
MODULE_DEVICE_TABLE(of, spi_gpio_dt_ids);

static int spi_gpio_probe_dt(struct platform_device *pdev,
			     struct spi_controller *host)
{
	host->dev.of_node = pdev->dev.of_node;
	host->use_gpio_descriptors = true;

	return 0;
}
#else
static inline int spi_gpio_probe_dt(struct platform_device *pdev,
				    struct spi_controller *host)
{
	return 0;
}
#endif

static int spi_gpio_probe_pdata(struct platform_device *pdev,
				struct spi_controller *host)
{
	struct device *dev = &pdev->dev;
	struct spi_gpio_platform_data *pdata = dev_get_platdata(dev);
	struct spi_gpio *spi_gpio = spi_controller_get_devdata(host);
	int i;

#ifdef GENERIC_BITBANG
	if (!pdata || !pdata->num_chipselect)
		return -ENODEV;
#endif
	 
	host->num_chipselect = pdata->num_chipselect ?: 1;

	spi_gpio->cs_gpios = devm_kcalloc(dev, host->num_chipselect,
					  sizeof(*spi_gpio->cs_gpios),
					  GFP_KERNEL);
	if (!spi_gpio->cs_gpios)
		return -ENOMEM;

	for (i = 0; i < host->num_chipselect; i++) {
		spi_gpio->cs_gpios[i] = devm_gpiod_get_index(dev, "cs", i,
							     GPIOD_OUT_HIGH);
		if (IS_ERR(spi_gpio->cs_gpios[i]))
			return PTR_ERR(spi_gpio->cs_gpios[i]);
	}

	return 0;
}

static int spi_gpio_probe(struct platform_device *pdev)
{
	int				status;
	struct spi_controller		*host;
	struct spi_gpio			*spi_gpio;
	struct device			*dev = &pdev->dev;
	struct spi_bitbang		*bb;

	host = devm_spi_alloc_host(dev, sizeof(*spi_gpio));
	if (!host)
		return -ENOMEM;

	if (pdev->dev.of_node)
		status = spi_gpio_probe_dt(pdev, host);
	else
		status = spi_gpio_probe_pdata(pdev, host);

	if (status)
		return status;

	spi_gpio = spi_controller_get_devdata(host);

	status = spi_gpio_request(dev, spi_gpio);
	if (status)
		return status;

	host->bits_per_word_mask = SPI_BPW_RANGE_MASK(1, 32);
	host->mode_bits = SPI_3WIRE | SPI_3WIRE_HIZ | SPI_CPHA | SPI_CPOL |
			    SPI_CS_HIGH | SPI_LSB_FIRST;
	if (!spi_gpio->mosi) {
		 
		host->flags = SPI_CONTROLLER_NO_TX;
	}

	host->bus_num = pdev->id;
	host->setup = spi_gpio_setup;
	host->cleanup = spi_gpio_cleanup;

	bb = &spi_gpio->bitbang;
	bb->master = host;
	 
	host->flags |= SPI_CONTROLLER_GPIO_SS;
	bb->chipselect = spi_gpio_chipselect;
	bb->set_line_direction = spi_gpio_set_direction;

	if (host->flags & SPI_CONTROLLER_NO_TX) {
		bb->txrx_word[SPI_MODE_0] = spi_gpio_spec_txrx_word_mode0;
		bb->txrx_word[SPI_MODE_1] = spi_gpio_spec_txrx_word_mode1;
		bb->txrx_word[SPI_MODE_2] = spi_gpio_spec_txrx_word_mode2;
		bb->txrx_word[SPI_MODE_3] = spi_gpio_spec_txrx_word_mode3;
	} else {
		bb->txrx_word[SPI_MODE_0] = spi_gpio_txrx_word_mode0;
		bb->txrx_word[SPI_MODE_1] = spi_gpio_txrx_word_mode1;
		bb->txrx_word[SPI_MODE_2] = spi_gpio_txrx_word_mode2;
		bb->txrx_word[SPI_MODE_3] = spi_gpio_txrx_word_mode3;
	}
	bb->setup_transfer = spi_bitbang_setup_transfer;

	status = spi_bitbang_init(&spi_gpio->bitbang);
	if (status)
		return status;

	return devm_spi_register_controller(&pdev->dev, host);
}

MODULE_ALIAS("platform:" DRIVER_NAME);

static struct platform_driver spi_gpio_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table = of_match_ptr(spi_gpio_dt_ids),
	},
	.probe		= spi_gpio_probe,
};
module_platform_driver(spi_gpio_driver);

MODULE_DESCRIPTION("SPI host driver using generic bitbanged GPIO ");
MODULE_AUTHOR("David Brownell");
MODULE_LICENSE("GPL");
