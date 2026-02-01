
 

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/touchscreen.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>

 
#define BU21029_HWID_REG	(0x0E << 3)
#define SUPPORTED_HWID		0x0229

 
#define BU21029_CFR0_REG	(0x00 << 3)
#define CFR0_VALUE		0x00

 
#define BU21029_CFR1_REG	(0x01 << 3)
#define CFR1_VALUE		0xA6

 
#define BU21029_CFR2_REG	(0x02 << 3)
#define CFR2_VALUE		0xC9

 
#define BU21029_CFR3_REG	(0x0B << 3)
#define CFR3_VALUE		0x42

 
#define BU21029_LDO_REG		(0x0C << 3)
#define LDO_VALUE		0x77

 
#define BU21029_AUTOSCAN	0x80

 
#define PEN_UP_TIMEOUT_MS	50

#define STOP_DELAY_MIN_US	50
#define STOP_DELAY_MAX_US	1000
#define START_DELAY_MS		2
#define BUF_LEN			8
#define SCALE_12BIT		(1 << 12)
#define MAX_12BIT		((1 << 12) - 1)
#define DRIVER_NAME		"bu21029"

struct bu21029_ts_data {
	struct i2c_client		*client;
	struct input_dev		*in_dev;
	struct timer_list		timer;
	struct regulator		*vdd;
	struct gpio_desc		*reset_gpios;
	u32				x_plate_ohms;
	struct touchscreen_properties	prop;
};

static void bu21029_touch_report(struct bu21029_ts_data *bu21029, const u8 *buf)
{
	u16 x, y, z1, z2;
	u32 rz;
	s32 max_pressure = input_abs_get_max(bu21029->in_dev, ABS_PRESSURE);

	 
	x  = (buf[0] << 4) | (buf[1] >> 4);
	y  = (buf[2] << 4) | (buf[3] >> 4);
	z1 = (buf[4] << 4) | (buf[5] >> 4);
	z2 = (buf[6] << 4) | (buf[7] >> 4);

	if (z1 && z2) {
		 
		rz  = z2 - z1;
		rz *= x;
		rz *= bu21029->x_plate_ohms;
		rz /= z1;
		rz  = DIV_ROUND_CLOSEST(rz, SCALE_12BIT);
		if (rz <= max_pressure) {
			touchscreen_report_pos(bu21029->in_dev, &bu21029->prop,
					       x, y, false);
			input_report_abs(bu21029->in_dev, ABS_PRESSURE,
					 max_pressure - rz);
			input_report_key(bu21029->in_dev, BTN_TOUCH, 1);
			input_sync(bu21029->in_dev);
		}
	}
}

static void bu21029_touch_release(struct timer_list *t)
{
	struct bu21029_ts_data *bu21029 = from_timer(bu21029, t, timer);

	input_report_abs(bu21029->in_dev, ABS_PRESSURE, 0);
	input_report_key(bu21029->in_dev, BTN_TOUCH, 0);
	input_sync(bu21029->in_dev);
}

static irqreturn_t bu21029_touch_soft_irq(int irq, void *data)
{
	struct bu21029_ts_data *bu21029 = data;
	u8 buf[BUF_LEN];
	int error;

	 
	error = i2c_smbus_read_i2c_block_data(bu21029->client, BU21029_AUTOSCAN,
					      sizeof(buf), buf);
	if (error < 0)
		goto out;

	bu21029_touch_report(bu21029, buf);

	 
	mod_timer(&bu21029->timer,
		  jiffies + msecs_to_jiffies(PEN_UP_TIMEOUT_MS));

out:
	return IRQ_HANDLED;
}

static void bu21029_put_chip_in_reset(struct bu21029_ts_data *bu21029)
{
	if (bu21029->reset_gpios) {
		gpiod_set_value_cansleep(bu21029->reset_gpios, 1);
		usleep_range(STOP_DELAY_MIN_US, STOP_DELAY_MAX_US);
	}
}

static int bu21029_start_chip(struct input_dev *dev)
{
	struct bu21029_ts_data *bu21029 = input_get_drvdata(dev);
	struct i2c_client *i2c = bu21029->client;
	struct {
		u8 reg;
		u8 value;
	} init_table[] = {
		{BU21029_CFR0_REG, CFR0_VALUE},
		{BU21029_CFR1_REG, CFR1_VALUE},
		{BU21029_CFR2_REG, CFR2_VALUE},
		{BU21029_CFR3_REG, CFR3_VALUE},
		{BU21029_LDO_REG,  LDO_VALUE}
	};
	int error, i;
	__be16 hwid;

	error = regulator_enable(bu21029->vdd);
	if (error) {
		dev_err(&i2c->dev, "failed to power up chip: %d", error);
		return error;
	}

	 
	if (bu21029->reset_gpios) {
		gpiod_set_value_cansleep(bu21029->reset_gpios, 0);
		msleep(START_DELAY_MS);
	}

	error = i2c_smbus_read_i2c_block_data(i2c, BU21029_HWID_REG,
					      sizeof(hwid), (u8 *)&hwid);
	if (error < 0) {
		dev_err(&i2c->dev, "failed to read HW ID\n");
		goto err_out;
	}

	if (be16_to_cpu(hwid) != SUPPORTED_HWID) {
		dev_err(&i2c->dev,
			"unsupported HW ID 0x%x\n", be16_to_cpu(hwid));
		error = -ENODEV;
		goto err_out;
	}

	for (i = 0; i < ARRAY_SIZE(init_table); ++i) {
		error = i2c_smbus_write_byte_data(i2c,
						  init_table[i].reg,
						  init_table[i].value);
		if (error < 0) {
			dev_err(&i2c->dev,
				"failed to write %#02x to register %#02x: %d\n",
				init_table[i].value, init_table[i].reg,
				error);
			goto err_out;
		}
	}

	error = i2c_smbus_write_byte(i2c, BU21029_AUTOSCAN);
	if (error < 0) {
		dev_err(&i2c->dev, "failed to start autoscan\n");
		goto err_out;
	}

	enable_irq(bu21029->client->irq);
	return 0;

err_out:
	bu21029_put_chip_in_reset(bu21029);
	regulator_disable(bu21029->vdd);
	return error;
}

static void bu21029_stop_chip(struct input_dev *dev)
{
	struct bu21029_ts_data *bu21029 = input_get_drvdata(dev);

	disable_irq(bu21029->client->irq);
	del_timer_sync(&bu21029->timer);

	bu21029_put_chip_in_reset(bu21029);
	regulator_disable(bu21029->vdd);
}

static int bu21029_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct bu21029_ts_data *bu21029;
	struct input_dev *in_dev;
	int error;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_WRITE_BYTE |
				     I2C_FUNC_SMBUS_WRITE_BYTE_DATA |
				     I2C_FUNC_SMBUS_READ_I2C_BLOCK)) {
		dev_err(dev, "i2c functionality support is not sufficient\n");
		return -EIO;
	}

	bu21029 = devm_kzalloc(dev, sizeof(*bu21029), GFP_KERNEL);
	if (!bu21029)
		return -ENOMEM;

	error = device_property_read_u32(dev, "rohm,x-plate-ohms", &bu21029->x_plate_ohms);
	if (error) {
		dev_err(dev, "invalid 'x-plate-ohms' supplied: %d\n", error);
		return error;
	}

	bu21029->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(bu21029->vdd))
		return dev_err_probe(dev, PTR_ERR(bu21029->vdd),
				     "failed to acquire 'vdd' supply\n");

	bu21029->reset_gpios = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(bu21029->reset_gpios))
		return dev_err_probe(dev, PTR_ERR(bu21029->reset_gpios),
				     "failed to acquire 'reset' gpio\n");

	in_dev = devm_input_allocate_device(dev);
	if (!in_dev) {
		dev_err(dev, "unable to allocate input device\n");
		return -ENOMEM;
	}

	bu21029->client = client;
	bu21029->in_dev = in_dev;
	timer_setup(&bu21029->timer, bu21029_touch_release, 0);

	in_dev->name		= DRIVER_NAME;
	in_dev->id.bustype	= BUS_I2C;
	in_dev->open		= bu21029_start_chip;
	in_dev->close		= bu21029_stop_chip;

	input_set_capability(in_dev, EV_KEY, BTN_TOUCH);
	input_set_abs_params(in_dev, ABS_X, 0, MAX_12BIT, 0, 0);
	input_set_abs_params(in_dev, ABS_Y, 0, MAX_12BIT, 0, 0);
	input_set_abs_params(in_dev, ABS_PRESSURE, 0, MAX_12BIT, 0, 0);
	touchscreen_parse_properties(in_dev, false, &bu21029->prop);

	input_set_drvdata(in_dev, bu21029);

	error = devm_request_threaded_irq(dev, client->irq, NULL,
					  bu21029_touch_soft_irq,
					  IRQF_ONESHOT | IRQF_NO_AUTOEN,
					  DRIVER_NAME, bu21029);
	if (error) {
		dev_err(dev, "unable to request touch irq: %d\n", error);
		return error;
	}

	error = input_register_device(in_dev);
	if (error) {
		dev_err(dev, "unable to register input device: %d\n", error);
		return error;
	}

	i2c_set_clientdata(client, bu21029);

	return 0;
}

static int bu21029_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct bu21029_ts_data *bu21029 = i2c_get_clientdata(i2c);

	if (!device_may_wakeup(dev)) {
		mutex_lock(&bu21029->in_dev->mutex);
		if (input_device_enabled(bu21029->in_dev))
			bu21029_stop_chip(bu21029->in_dev);
		mutex_unlock(&bu21029->in_dev->mutex);
	}

	return 0;
}

static int bu21029_resume(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct bu21029_ts_data *bu21029 = i2c_get_clientdata(i2c);

	if (!device_may_wakeup(dev)) {
		mutex_lock(&bu21029->in_dev->mutex);
		if (input_device_enabled(bu21029->in_dev))
			bu21029_start_chip(bu21029->in_dev);
		mutex_unlock(&bu21029->in_dev->mutex);
	}

	return 0;
}
static DEFINE_SIMPLE_DEV_PM_OPS(bu21029_pm_ops, bu21029_suspend, bu21029_resume);

static const struct i2c_device_id bu21029_ids[] = {
	{ DRIVER_NAME, 0 },
	{   }
};
MODULE_DEVICE_TABLE(i2c, bu21029_ids);

#ifdef CONFIG_OF
static const struct of_device_id bu21029_of_ids[] = {
	{ .compatible = "rohm,bu21029" },
	{   }
};
MODULE_DEVICE_TABLE(of, bu21029_of_ids);
#endif

static struct i2c_driver bu21029_driver = {
	.driver	= {
		.name		= DRIVER_NAME,
		.of_match_table	= of_match_ptr(bu21029_of_ids),
		.pm		= pm_sleep_ptr(&bu21029_pm_ops),
	},
	.id_table	= bu21029_ids,
	.probe		= bu21029_probe,
};
module_i2c_driver(bu21029_driver);

MODULE_AUTHOR("Zhu Yi <yi.zhu5@cn.bosch.com>");
MODULE_DESCRIPTION("Rohm BU21029 touchscreen controller driver");
MODULE_LICENSE("GPL v2");
