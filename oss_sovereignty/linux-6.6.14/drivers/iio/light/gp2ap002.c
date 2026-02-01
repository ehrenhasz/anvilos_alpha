
 
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/consumer.h>  
#include <linux/iio/types.h>  
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/bits.h>
#include <linux/math64.h>
#include <linux/pm.h>

#define GP2AP002_PROX_CHANNEL 0
#define GP2AP002_ALS_CHANNEL 1

 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 

#define GP2AP002_PROX				0x00
#define GP2AP002_GAIN				0x01
#define GP2AP002_HYS				0x02
#define GP2AP002_CYCLE				0x03
#define GP2AP002_OPMOD				0x04
#define GP2AP002_CON				0x06

#define GP2AP002_PROX_VO_DETECT			BIT(0)

 
#define GP2AP002_GAIN_LED_NORMAL		BIT(3)

 
#define GP2AP002_HYS_HYSD_SHIFT		7
#define GP2AP002_HYS_HYSD_MASK		BIT(7)
#define GP2AP002_HYS_HYSC_SHIFT		5
#define GP2AP002_HYS_HYSC_MASK		GENMASK(6, 5)
#define GP2AP002_HYS_HYSF_SHIFT		0
#define GP2AP002_HYS_HYSF_MASK		GENMASK(3, 0)
#define GP2AP002_HYS_MASK		(GP2AP002_HYS_HYSD_MASK | \
					 GP2AP002_HYS_HYSC_MASK | \
					 GP2AP002_HYS_HYSF_MASK)

 
#define GP2AP002_CYCLE_CYCL_SHIFT	3
#define GP2AP002_CYCLE_CYCL_MASK	GENMASK(5, 3)

 
#define GP2AP002_CYCLE_OSC_EFFECTIVE	0
#define GP2AP002_CYCLE_OSC_INEFFECTIVE	BIT(2)
#define GP2AP002_CYCLE_OSC_MASK		BIT(2)

 
#define GP2AP002_OPMOD_ASD		BIT(4)
 
#define GP2AP002_OPMOD_SSD_OPERATING	BIT(0)
 
#define GP2AP002_OPMOD_VCON_IRQ		BIT(1)
#define GP2AP002_OPMOD_MASK		(BIT(0) | BIT(1) | BIT(4))

 
#define GP2AP002_CON_OCON_SHIFT		3
#define GP2AP002_CON_OCON_ENABLE	(0x0 << GP2AP002_CON_OCON_SHIFT)
#define GP2AP002_CON_OCON_LOW		(0x2 << GP2AP002_CON_OCON_SHIFT)
#define GP2AP002_CON_OCON_HIGH		(0x3 << GP2AP002_CON_OCON_SHIFT)
#define GP2AP002_CON_OCON_MASK		(0x3 << GP2AP002_CON_OCON_SHIFT)

 
struct gp2ap002 {
	struct regmap *map;
	struct device *dev;
	struct regulator *vdd;
	struct regulator *vio;
	struct iio_channel *alsout;
	u8 hys_far;
	u8 hys_close;
	bool is_gp2ap002s00f;
	int irq;
	bool enabled;
};

static irqreturn_t gp2ap002_prox_irq(int irq, void *d)
{
	struct iio_dev *indio_dev = d;
	struct gp2ap002 *gp2ap002 = iio_priv(indio_dev);
	u64 ev;
	int val;
	int ret;

	if (!gp2ap002->enabled)
		goto err_retrig;

	ret = regmap_read(gp2ap002->map, GP2AP002_PROX, &val);
	if (ret) {
		dev_err(gp2ap002->dev, "error reading proximity\n");
		goto err_retrig;
	}

	if (val & GP2AP002_PROX_VO_DETECT) {
		 
		dev_dbg(gp2ap002->dev, "close\n");
		ret = regmap_write(gp2ap002->map, GP2AP002_HYS,
				   gp2ap002->hys_far);
		if (ret)
			dev_err(gp2ap002->dev,
				"error setting up proximity hysteresis\n");
		ev = IIO_UNMOD_EVENT_CODE(IIO_PROXIMITY, GP2AP002_PROX_CHANNEL,
					IIO_EV_TYPE_THRESH, IIO_EV_DIR_RISING);
	} else {
		 
		dev_dbg(gp2ap002->dev, "far\n");
		ret = regmap_write(gp2ap002->map, GP2AP002_HYS,
				   gp2ap002->hys_close);
		if (ret)
			dev_err(gp2ap002->dev,
				"error setting up proximity hysteresis\n");
		ev = IIO_UNMOD_EVENT_CODE(IIO_PROXIMITY, GP2AP002_PROX_CHANNEL,
					IIO_EV_TYPE_THRESH, IIO_EV_DIR_FALLING);
	}
	iio_push_event(indio_dev, ev, iio_get_time_ns(indio_dev));

	 
	usleep_range(20000, 30000);

err_retrig:
	ret = regmap_write(gp2ap002->map, GP2AP002_CON,
			   GP2AP002_CON_OCON_ENABLE);
	if (ret)
		dev_err(gp2ap002->dev, "error setting up VOUT control\n");

	return IRQ_HANDLED;
}

 
static const u16 gp2ap002_illuminance_table[] = {
	0, 1, 1, 2, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 25, 32, 40, 50, 63, 79,
	100, 126, 158, 200, 251, 316, 398, 501, 631, 794, 1000, 1259, 1585,
	1995, 2512, 3162, 3981, 5012, 6310, 7943, 10000, 12589, 15849, 19953,
	25119, 31623, 39811, 50119,
};

static int gp2ap002_get_lux(struct gp2ap002 *gp2ap002)
{
	int ret, res;
	u16 lux;

	ret = iio_read_channel_processed(gp2ap002->alsout, &res);
	if (ret < 0)
		return ret;

	dev_dbg(gp2ap002->dev, "read %d mA from ADC\n", res);

	 
	res = clamp(res, 0, (int)ARRAY_SIZE(gp2ap002_illuminance_table) - 1);
	lux = gp2ap002_illuminance_table[res];

	return (int)lux;
}

static int gp2ap002_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2, long mask)
{
	struct gp2ap002 *gp2ap002 = iio_priv(indio_dev);
	int ret;

	pm_runtime_get_sync(gp2ap002->dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		switch (chan->type) {
		case IIO_LIGHT:
			ret = gp2ap002_get_lux(gp2ap002);
			if (ret < 0)
				return ret;
			*val = ret;
			ret = IIO_VAL_INT;
			goto out;
		default:
			ret = -EINVAL;
			goto out;
		}
	default:
		ret = -EINVAL;
	}

out:
	pm_runtime_mark_last_busy(gp2ap002->dev);
	pm_runtime_put_autosuspend(gp2ap002->dev);

	return ret;
}

static int gp2ap002_init(struct gp2ap002 *gp2ap002)
{
	int ret;

	 
	ret = regmap_write(gp2ap002->map, GP2AP002_GAIN,
			   GP2AP002_GAIN_LED_NORMAL);
	if (ret) {
		dev_err(gp2ap002->dev, "error setting up LED gain\n");
		return ret;
	}
	ret = regmap_write(gp2ap002->map, GP2AP002_HYS, gp2ap002->hys_far);
	if (ret) {
		dev_err(gp2ap002->dev,
			"error setting up proximity hysteresis\n");
		return ret;
	}

	 
	ret = regmap_write(gp2ap002->map, GP2AP002_CYCLE,
			   GP2AP002_CYCLE_OSC_INEFFECTIVE);
	if (ret) {
		dev_err(gp2ap002->dev,
			"error setting up internal frequency hopping\n");
		return ret;
	}

	 
	ret = regmap_write(gp2ap002->map, GP2AP002_OPMOD,
			   GP2AP002_OPMOD_SSD_OPERATING |
			   GP2AP002_OPMOD_VCON_IRQ);
	if (ret) {
		dev_err(gp2ap002->dev, "error setting up operation mode\n");
		return ret;
	}

	 
	ret = regmap_write(gp2ap002->map, GP2AP002_CON,
			   GP2AP002_CON_OCON_ENABLE);
	if (ret)
		dev_err(gp2ap002->dev, "error setting up VOUT control\n");

	return ret;
}

static int gp2ap002_read_event_config(struct iio_dev *indio_dev,
				      const struct iio_chan_spec *chan,
				      enum iio_event_type type,
				      enum iio_event_direction dir)
{
	struct gp2ap002 *gp2ap002 = iio_priv(indio_dev);

	 
	return gp2ap002->enabled;
}

static int gp2ap002_write_event_config(struct iio_dev *indio_dev,
				       const struct iio_chan_spec *chan,
				       enum iio_event_type type,
				       enum iio_event_direction dir,
				       int state)
{
	struct gp2ap002 *gp2ap002 = iio_priv(indio_dev);

	if (state) {
		 
		pm_runtime_get_sync(gp2ap002->dev);
		gp2ap002->enabled = true;
	} else {
		pm_runtime_mark_last_busy(gp2ap002->dev);
		pm_runtime_put_autosuspend(gp2ap002->dev);
		gp2ap002->enabled = false;
	}

	return 0;
}

static const struct iio_info gp2ap002_info = {
	.read_raw = gp2ap002_read_raw,
	.read_event_config = gp2ap002_read_event_config,
	.write_event_config = gp2ap002_write_event_config,
};

static const struct iio_event_spec gp2ap002_events[] = {
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_EITHER,
		.mask_separate = BIT(IIO_EV_INFO_ENABLE),
	},
};

static const struct iio_chan_spec gp2ap002_channels[] = {
	{
		.type = IIO_PROXIMITY,
		.event_spec = gp2ap002_events,
		.num_event_specs = ARRAY_SIZE(gp2ap002_events),
	},
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.channel = GP2AP002_ALS_CHANNEL,
	},
};

 
static int gp2ap002_regmap_i2c_read(void *context, unsigned int reg,
				    unsigned int *val)
{
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);
	int ret;

	ret = i2c_smbus_read_word_data(i2c, reg);
	if (ret < 0)
		return ret;

	*val = (ret >> 8) & 0xFF;

	return 0;
}

static int gp2ap002_regmap_i2c_write(void *context, unsigned int reg,
				     unsigned int val)
{
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);

	return i2c_smbus_write_byte_data(i2c, reg, val);
}

static struct regmap_bus gp2ap002_regmap_bus = {
	.reg_read = gp2ap002_regmap_i2c_read,
	.reg_write = gp2ap002_regmap_i2c_write,
};

static int gp2ap002_probe(struct i2c_client *client)
{
	struct gp2ap002 *gp2ap002;
	struct iio_dev *indio_dev;
	struct device *dev = &client->dev;
	enum iio_chan_type ch_type;
	static const struct regmap_config config = {
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = GP2AP002_CON,
	};
	struct regmap *regmap;
	int num_chan;
	const char *compat;
	u8 val;
	int ret;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*gp2ap002));
	if (!indio_dev)
		return -ENOMEM;
	i2c_set_clientdata(client, indio_dev);

	gp2ap002 = iio_priv(indio_dev);
	gp2ap002->dev = dev;

	 
	ret = device_property_read_string(dev, "compatible", &compat);
	if (ret) {
		dev_err(dev, "cannot check compatible\n");
		return ret;
	}
	gp2ap002->is_gp2ap002s00f = !strcmp(compat, "sharp,gp2ap002s00f");

	regmap = devm_regmap_init(dev, &gp2ap002_regmap_bus, dev, &config);
	if (IS_ERR(regmap)) {
		dev_err(dev, "Failed to register i2c regmap %ld\n", PTR_ERR(regmap));
		return PTR_ERR(regmap);
	}
	gp2ap002->map = regmap;

	 

	 
	ret = device_property_read_u8(dev, "sharp,proximity-far-hysteresis",
				      &val);
	if (ret) {
		dev_err(dev, "failed to obtain proximity far setting\n");
		return ret;
	}
	dev_dbg(dev, "proximity far setting %02x\n", val);
	gp2ap002->hys_far = val;

	ret = device_property_read_u8(dev, "sharp,proximity-close-hysteresis",
				      &val);
	if (ret) {
		dev_err(dev, "failed to obtain proximity close setting\n");
		return ret;
	}
	dev_dbg(dev, "proximity close setting %02x\n", val);
	gp2ap002->hys_close = val;

	 
	if (!gp2ap002->is_gp2ap002s00f) {
		gp2ap002->alsout = devm_iio_channel_get(dev, "alsout");
		if (IS_ERR(gp2ap002->alsout)) {
			ret = PTR_ERR(gp2ap002->alsout);
			ret = (ret == -ENODEV) ? -EPROBE_DEFER : ret;
			return dev_err_probe(dev, ret, "failed to get ALSOUT ADC channel\n");
		}
		ret = iio_get_channel_type(gp2ap002->alsout, &ch_type);
		if (ret < 0)
			return ret;
		if (ch_type != IIO_CURRENT) {
			dev_err(dev,
				"wrong type of IIO channel specified for ALSOUT\n");
			return -EINVAL;
		}
	}

	gp2ap002->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(gp2ap002->vdd))
		return dev_err_probe(dev, PTR_ERR(gp2ap002->vdd),
				     "failed to get VDD regulator\n");

	gp2ap002->vio = devm_regulator_get(dev, "vio");
	if (IS_ERR(gp2ap002->vio))
		return dev_err_probe(dev, PTR_ERR(gp2ap002->vio),
				     "failed to get VIO regulator\n");

	 
	ret = regulator_set_voltage(gp2ap002->vdd, 2400000, 3600000);
	if (ret) {
		dev_err(dev, "failed to sett VDD voltage\n");
		return ret;
	}

	 
	ret = regulator_get_voltage(gp2ap002->vdd);
	if (ret < 0) {
		dev_err(dev, "failed to get VDD voltage\n");
		return ret;
	}
	ret = regulator_set_voltage(gp2ap002->vio, 1650000, ret);
	if (ret) {
		dev_err(dev, "failed to set VIO voltage\n");
		return ret;
	}

	ret = regulator_enable(gp2ap002->vdd);
	if (ret) {
		dev_err(dev, "failed to enable VDD regulator\n");
		return ret;
	}
	ret = regulator_enable(gp2ap002->vio);
	if (ret) {
		dev_err(dev, "failed to enable VIO regulator\n");
		goto out_disable_vdd;
	}

	msleep(20);

	 
	ret = gp2ap002_init(gp2ap002);
	if (ret) {
		dev_err(dev, "initialization failed\n");
		goto out_disable_vio;
	}
	pm_runtime_get_noresume(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	gp2ap002->enabled = false;

	ret = devm_request_threaded_irq(dev, client->irq, NULL,
					gp2ap002_prox_irq, IRQF_ONESHOT,
					"gp2ap002", indio_dev);
	if (ret) {
		dev_err(dev, "unable to request IRQ\n");
		goto out_put_pm;
	}
	gp2ap002->irq = client->irq;

	 
	pm_runtime_set_autosuspend_delay(dev, 1000);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_put(dev);

	indio_dev->info = &gp2ap002_info;
	indio_dev->name = "gp2ap002";
	indio_dev->channels = gp2ap002_channels;
	 
	num_chan = ARRAY_SIZE(gp2ap002_channels);
	if (gp2ap002->is_gp2ap002s00f)
		num_chan--;
	indio_dev->num_channels = num_chan;
	indio_dev->modes = INDIO_DIRECT_MODE;

	ret = iio_device_register(indio_dev);
	if (ret)
		goto out_disable_pm;
	dev_dbg(dev, "Sharp GP2AP002 probed successfully\n");

	return 0;

out_put_pm:
	pm_runtime_put_noidle(dev);
out_disable_pm:
	pm_runtime_disable(dev);
out_disable_vio:
	regulator_disable(gp2ap002->vio);
out_disable_vdd:
	regulator_disable(gp2ap002->vdd);
	return ret;
}

static void gp2ap002_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct gp2ap002 *gp2ap002 = iio_priv(indio_dev);
	struct device *dev = &client->dev;

	pm_runtime_get_sync(dev);
	pm_runtime_put_noidle(dev);
	pm_runtime_disable(dev);
	iio_device_unregister(indio_dev);
	regulator_disable(gp2ap002->vio);
	regulator_disable(gp2ap002->vdd);
}

static int gp2ap002_runtime_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct gp2ap002 *gp2ap002 = iio_priv(indio_dev);
	int ret;

	 
	disable_irq(gp2ap002->irq);

	 
	ret = regmap_write(gp2ap002->map, GP2AP002_OPMOD, 0x00);
	if (ret) {
		dev_err(gp2ap002->dev, "error setting up operation mode\n");
		return ret;
	}
	 
	regulator_disable(gp2ap002->vio);
	regulator_disable(gp2ap002->vdd);

	return 0;
}

static int gp2ap002_runtime_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct gp2ap002 *gp2ap002 = iio_priv(indio_dev);
	int ret;

	ret = regulator_enable(gp2ap002->vdd);
	if (ret) {
		dev_err(dev, "failed to enable VDD regulator in resume path\n");
		return ret;
	}
	ret = regulator_enable(gp2ap002->vio);
	if (ret) {
		dev_err(dev, "failed to enable VIO regulator in resume path\n");
		return ret;
	}

	msleep(20);

	ret = gp2ap002_init(gp2ap002);
	if (ret) {
		dev_err(dev, "re-initialization failed\n");
		return ret;
	}

	 
	enable_irq(gp2ap002->irq);

	return 0;
}

static DEFINE_RUNTIME_DEV_PM_OPS(gp2ap002_dev_pm_ops, gp2ap002_runtime_suspend,
				 gp2ap002_runtime_resume, NULL);

static const struct i2c_device_id gp2ap002_id_table[] = {
	{ "gp2ap002", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, gp2ap002_id_table);

static const struct of_device_id gp2ap002_of_match[] = {
	{ .compatible = "sharp,gp2ap002a00f" },
	{ .compatible = "sharp,gp2ap002s00f" },
	{ },
};
MODULE_DEVICE_TABLE(of, gp2ap002_of_match);

static struct i2c_driver gp2ap002_driver = {
	.driver = {
		.name = "gp2ap002",
		.of_match_table = gp2ap002_of_match,
		.pm = pm_ptr(&gp2ap002_dev_pm_ops),
	},
	.probe = gp2ap002_probe,
	.remove = gp2ap002_remove,
	.id_table = gp2ap002_id_table,
};
module_i2c_driver(gp2ap002_driver);

MODULE_AUTHOR("Linus Walleij <linus.walleij@linaro.org>");
MODULE_DESCRIPTION("GP2AP002 ambient light and proximity sensor driver");
MODULE_LICENSE("GPL v2");
