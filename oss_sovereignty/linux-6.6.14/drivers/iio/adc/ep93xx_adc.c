
 

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/iio/iio.h>
#include <linux/io.h>
#include <linux/irqflags.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/of.h>

 
#ifdef CONFIG_HIGH_RES_TIMERS
#define ep93xx_adc_delay(usmin, usmax) usleep_range(usmin, usmax)
#else
#define ep93xx_adc_delay(usmin, usmax) udelay(usmin)
#endif

#define EP93XX_ADC_RESULT	0x08
#define   EP93XX_ADC_SDR	BIT(31)
#define EP93XX_ADC_SWITCH	0x18
#define EP93XX_ADC_SW_LOCK	0x20

struct ep93xx_adc_priv {
	struct clk *clk;
	void __iomem *base;
	int lastch;
	struct mutex lock;
};

#define EP93XX_ADC_CH(index, dname, swcfg) {			\
	.type = IIO_VOLTAGE,					\
	.indexed = 1,						\
	.channel = index,					\
	.address = swcfg,					\
	.datasheet_name = dname,				\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SCALE) |	\
				   BIT(IIO_CHAN_INFO_OFFSET),	\
}

 
static const struct iio_chan_spec ep93xx_adc_channels[8] = {
	EP93XX_ADC_CH(0, "YM",	0x608),
	EP93XX_ADC_CH(1, "SXP",	0x680),
	EP93XX_ADC_CH(2, "SXM",	0x640),
	EP93XX_ADC_CH(3, "SYP",	0x620),
	EP93XX_ADC_CH(4, "SYM",	0x610),
	EP93XX_ADC_CH(5, "XP",	0x601),
	EP93XX_ADC_CH(6, "XM",	0x602),
	EP93XX_ADC_CH(7, "YP",	0x604),
};

static int ep93xx_read_raw(struct iio_dev *iiodev,
			   struct iio_chan_spec const *channel, int *value,
			   int *shift, long mask)
{
	struct ep93xx_adc_priv *priv = iio_priv(iiodev);
	unsigned long timeout;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&priv->lock);
		if (priv->lastch != channel->channel) {
			priv->lastch = channel->channel;
			 
			local_irq_disable();
			writel_relaxed(0xAA, priv->base + EP93XX_ADC_SW_LOCK);
			writel_relaxed(channel->address,
				       priv->base + EP93XX_ADC_SWITCH);
			local_irq_enable();
			 
			ep93xx_adc_delay(2000, 2000);
		}
		 
		readl_relaxed(priv->base + EP93XX_ADC_RESULT);
		 
		ep93xx_adc_delay(DIV_ROUND_UP(1000000, 925),
				 DIV_ROUND_UP(1000000, 925));
		 
		ret = IIO_VAL_INT;
		timeout = jiffies + msecs_to_jiffies(1) + 1;
		while (1) {
			u32 t;

			t = readl_relaxed(priv->base + EP93XX_ADC_RESULT);
			if (t & EP93XX_ADC_SDR) {
				*value = sign_extend32(t, 15);
				break;
			}

			if (time_after(jiffies, timeout)) {
				dev_err(&iiodev->dev, "Conversion timeout\n");
				ret = -ETIMEDOUT;
				break;
			}

			cpu_relax();
		}
		mutex_unlock(&priv->lock);
		return ret;

	case IIO_CHAN_INFO_OFFSET:
		 
		*value = 25000;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		 
		*value = (1ULL << 32) * 3300 / 50000;
		*shift = 32;
		return IIO_VAL_FRACTIONAL_LOG2;
	}

	return -EINVAL;
}

static const struct iio_info ep93xx_adc_info = {
	.read_raw = ep93xx_read_raw,
};

static int ep93xx_adc_probe(struct platform_device *pdev)
{
	int ret;
	struct iio_dev *iiodev;
	struct ep93xx_adc_priv *priv;
	struct clk *pclk;

	iiodev = devm_iio_device_alloc(&pdev->dev, sizeof(*priv));
	if (!iiodev)
		return -ENOMEM;
	priv = iio_priv(iiodev);

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	iiodev->name = dev_name(&pdev->dev);
	iiodev->modes = INDIO_DIRECT_MODE;
	iiodev->info = &ep93xx_adc_info;
	iiodev->num_channels = ARRAY_SIZE(ep93xx_adc_channels);
	iiodev->channels = ep93xx_adc_channels;

	priv->lastch = -1;
	mutex_init(&priv->lock);

	platform_set_drvdata(pdev, iiodev);

	priv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clk)) {
		dev_err(&pdev->dev, "Cannot obtain clock\n");
		return PTR_ERR(priv->clk);
	}

	pclk = clk_get_parent(priv->clk);
	if (!pclk) {
		dev_warn(&pdev->dev, "Cannot obtain parent clock\n");
	} else {
		 
		ret = clk_set_rate(priv->clk, clk_get_rate(pclk) / 16);
		if (ret)
			dev_warn(&pdev->dev, "Cannot set clock rate\n");
		 
	}

	ret = clk_prepare_enable(priv->clk);
	if (ret) {
		dev_err(&pdev->dev, "Cannot enable clock\n");
		return ret;
	}

	ret = iio_device_register(iiodev);
	if (ret)
		clk_disable_unprepare(priv->clk);

	return ret;
}

static int ep93xx_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *iiodev = platform_get_drvdata(pdev);
	struct ep93xx_adc_priv *priv = iio_priv(iiodev);

	iio_device_unregister(iiodev);
	clk_disable_unprepare(priv->clk);

	return 0;
}

static const struct of_device_id ep93xx_adc_of_ids[] = {
	{ .compatible = "cirrus,ep9301-adc" },
	{}
};
MODULE_DEVICE_TABLE(of, ep93xx_adc_of_ids);

static struct platform_driver ep93xx_adc_driver = {
	.driver = {
		.name = "ep93xx-adc",
		.of_match_table = ep93xx_adc_of_ids,
	},
	.probe = ep93xx_adc_probe,
	.remove = ep93xx_adc_remove,
};
module_platform_driver(ep93xx_adc_driver);

MODULE_AUTHOR("Alexander Sverdlin <alexander.sverdlin@gmail.com>");
MODULE_DESCRIPTION("Cirrus Logic EP93XX ADC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ep93xx-adc");
