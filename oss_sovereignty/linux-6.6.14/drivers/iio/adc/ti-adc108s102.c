
 

#include <linux/acpi.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/types.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>

 
#define ADC108S102_VA_MV_ACPI_DEFAULT	5000

 
#define ADC108S102_BITS		12
#define ADC108S102_MAX_CHANNELS	8

 
#define ADC108S102_CMD(ch)		((u16)(ch) << 11)

 
#define ADC108S102_RES_DATA(res)	((u16)res & GENMASK(11, 0))

struct adc108s102_state {
	struct spi_device		*spi;
	struct regulator		*reg;
	u32				va_millivolt;
	 
	struct spi_transfer		ring_xfer;
	 
	struct spi_transfer		scan_single_xfer;
	 
	struct spi_message		ring_msg;
	 
	struct spi_message		scan_single_msg;

	 
	__be16				rx_buf[9] __aligned(IIO_DMA_MINALIGN);
	__be16				tx_buf[9] __aligned(IIO_DMA_MINALIGN);
};

#define ADC108S102_V_CHAN(index)					\
	{								\
		.type = IIO_VOLTAGE,					\
		.indexed = 1,						\
		.channel = index,					\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
			BIT(IIO_CHAN_INFO_SCALE),			\
		.address = index,					\
		.scan_index = index,					\
		.scan_type = {						\
			.sign = 'u',					\
			.realbits = ADC108S102_BITS,			\
			.storagebits = 16,				\
			.endianness = IIO_BE,				\
		},							\
	}

static const struct iio_chan_spec adc108s102_channels[] = {
	ADC108S102_V_CHAN(0),
	ADC108S102_V_CHAN(1),
	ADC108S102_V_CHAN(2),
	ADC108S102_V_CHAN(3),
	ADC108S102_V_CHAN(4),
	ADC108S102_V_CHAN(5),
	ADC108S102_V_CHAN(6),
	ADC108S102_V_CHAN(7),
	IIO_CHAN_SOFT_TIMESTAMP(8),
};

static int adc108s102_update_scan_mode(struct iio_dev *indio_dev,
		unsigned long const *active_scan_mask)
{
	struct adc108s102_state *st = iio_priv(indio_dev);
	unsigned int bit, cmds;

	 
	cmds = 0;
	for_each_set_bit(bit, active_scan_mask, ADC108S102_MAX_CHANNELS)
		st->tx_buf[cmds++] = cpu_to_be16(ADC108S102_CMD(bit));

	 
	st->tx_buf[cmds++] = 0x00;

	 
	st->ring_xfer.tx_buf = &st->tx_buf[0];
	st->ring_xfer.rx_buf = &st->rx_buf[0];
	st->ring_xfer.len = cmds * sizeof(st->tx_buf[0]);

	spi_message_init_with_transfers(&st->ring_msg, &st->ring_xfer, 1);

	return 0;
}

static irqreturn_t adc108s102_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct adc108s102_state *st = iio_priv(indio_dev);
	int ret;

	ret = spi_sync(st->spi, &st->ring_msg);
	if (ret < 0)
		goto out_notify;

	 
	iio_push_to_buffers_with_ts_unaligned(indio_dev,
					      &st->rx_buf[1],
					      st->ring_xfer.len - sizeof(st->rx_buf[1]),
					      iio_get_time_ns(indio_dev));

out_notify:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static int adc108s102_scan_direct(struct adc108s102_state *st, unsigned int ch)
{
	int ret;

	st->tx_buf[0] = cpu_to_be16(ADC108S102_CMD(ch));
	ret = spi_sync(st->spi, &st->scan_single_msg);
	if (ret)
		return ret;

	 
	return be16_to_cpu(st->rx_buf[1]);
}

static int adc108s102_read_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int *val, int *val2, long m)
{
	struct adc108s102_state *st = iio_priv(indio_dev);
	int ret;

	switch (m) {
	case IIO_CHAN_INFO_RAW:
		ret = iio_device_claim_direct_mode(indio_dev);
		if (ret)
			return ret;

		ret = adc108s102_scan_direct(st, chan->address);

		iio_device_release_direct_mode(indio_dev);

		if (ret < 0)
			return ret;

		*val = ADC108S102_RES_DATA(ret);

		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		if (chan->type != IIO_VOLTAGE)
			break;

		*val = st->va_millivolt;
		*val2 = chan->scan_type.realbits;

		return IIO_VAL_FRACTIONAL_LOG2;
	default:
		break;
	}

	return -EINVAL;
}

static const struct iio_info adc108s102_info = {
	.read_raw		= &adc108s102_read_raw,
	.update_scan_mode	= &adc108s102_update_scan_mode,
};

static void adc108s102_reg_disable(void *reg)
{
	regulator_disable(reg);
}

static int adc108s102_probe(struct spi_device *spi)
{
	struct adc108s102_state *st;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*st));
	if (!indio_dev)
		return -ENOMEM;

	st = iio_priv(indio_dev);

	if (ACPI_COMPANION(&spi->dev)) {
		st->va_millivolt = ADC108S102_VA_MV_ACPI_DEFAULT;
	} else {
		st->reg = devm_regulator_get(&spi->dev, "vref");
		if (IS_ERR(st->reg))
			return PTR_ERR(st->reg);

		ret = regulator_enable(st->reg);
		if (ret < 0) {
			dev_err(&spi->dev, "Cannot enable vref regulator\n");
			return ret;
		}
		ret = devm_add_action_or_reset(&spi->dev, adc108s102_reg_disable,
					       st->reg);
		if (ret)
			return ret;

		ret = regulator_get_voltage(st->reg);
		if (ret < 0) {
			dev_err(&spi->dev, "vref get voltage failed\n");
			return ret;
		}

		st->va_millivolt = ret / 1000;
	}

	st->spi = spi;

	indio_dev->name = spi->modalias;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = adc108s102_channels;
	indio_dev->num_channels = ARRAY_SIZE(adc108s102_channels);
	indio_dev->info = &adc108s102_info;

	 
	st->scan_single_xfer.tx_buf = st->tx_buf;
	st->scan_single_xfer.rx_buf = st->rx_buf;
	st->scan_single_xfer.len = 2 * sizeof(st->tx_buf[0]);

	spi_message_init_with_transfers(&st->scan_single_msg,
					&st->scan_single_xfer, 1);

	ret = devm_iio_triggered_buffer_setup(&spi->dev, indio_dev, NULL,
					      &adc108s102_trigger_handler,
					      NULL);
	if (ret)
		return ret;

	ret = devm_iio_device_register(&spi->dev, indio_dev);
	if (ret)
		dev_err(&spi->dev, "Failed to register IIO device\n");
	return ret;
}

static const struct of_device_id adc108s102_of_match[] = {
	{ .compatible = "ti,adc108s102" },
	{ }
};
MODULE_DEVICE_TABLE(of, adc108s102_of_match);

#ifdef CONFIG_ACPI
static const struct acpi_device_id adc108s102_acpi_ids[] = {
	{ "INT3495", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, adc108s102_acpi_ids);
#endif

static const struct spi_device_id adc108s102_id[] = {
	{ "adc108s102", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, adc108s102_id);

static struct spi_driver adc108s102_driver = {
	.driver = {
		.name   = "adc108s102",
		.of_match_table = adc108s102_of_match,
		.acpi_match_table = ACPI_PTR(adc108s102_acpi_ids),
	},
	.probe		= adc108s102_probe,
	.id_table	= adc108s102_id,
};
module_spi_driver(adc108s102_driver);

MODULE_AUTHOR("Bogdan Pricop <bogdan.pricop@emutex.com>");
MODULE_DESCRIPTION("Texas Instruments ADC108S102 and ADC128S102 driver");
MODULE_LICENSE("GPL v2");
