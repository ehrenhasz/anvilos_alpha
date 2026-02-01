
 

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>

 
#define IS31FL32XX_REG_NONE 0xFF

 
#define IS31FL32XX_SHUTDOWN_SSD_ENABLE  0
#define IS31FL32XX_SHUTDOWN_SSD_DISABLE BIT(0)

 
#define IS31FL3216_CONFIG_REG 0x00
#define IS31FL3216_LIGHTING_EFFECT_REG 0x03
#define IS31FL3216_CHANNEL_CONFIG_REG 0x04

 
#define IS31FL3216_CONFIG_SSD_ENABLE  BIT(7)
#define IS31FL3216_CONFIG_SSD_DISABLE 0

struct is31fl32xx_priv;
struct is31fl32xx_led_data {
	struct led_classdev cdev;
	u8 channel;  
	struct is31fl32xx_priv *priv;
};

struct is31fl32xx_priv {
	const struct is31fl32xx_chipdef *cdef;
	struct i2c_client *client;
	unsigned int num_leds;
	struct is31fl32xx_led_data leds[];
};

 
struct is31fl32xx_chipdef {
	u8	channels;
	u8	shutdown_reg;
	u8	pwm_update_reg;
	u8	global_control_reg;
	u8	reset_reg;
	u8	pwm_register_base;
	bool	pwm_registers_reversed;
	u8	led_control_register_base;
	u8	enable_bits_per_led_control_register;
	int (*reset_func)(struct is31fl32xx_priv *priv);
	int (*sw_shutdown_func)(struct is31fl32xx_priv *priv, bool enable);
};

static const struct is31fl32xx_chipdef is31fl3236_cdef = {
	.channels				= 36,
	.shutdown_reg				= 0x00,
	.pwm_update_reg				= 0x25,
	.global_control_reg			= 0x4a,
	.reset_reg				= 0x4f,
	.pwm_register_base			= 0x01,
	.led_control_register_base		= 0x26,
	.enable_bits_per_led_control_register	= 1,
};

static const struct is31fl32xx_chipdef is31fl3235_cdef = {
	.channels				= 28,
	.shutdown_reg				= 0x00,
	.pwm_update_reg				= 0x25,
	.global_control_reg			= 0x4a,
	.reset_reg				= 0x4f,
	.pwm_register_base			= 0x05,
	.led_control_register_base		= 0x2a,
	.enable_bits_per_led_control_register	= 1,
};

static const struct is31fl32xx_chipdef is31fl3218_cdef = {
	.channels				= 18,
	.shutdown_reg				= 0x00,
	.pwm_update_reg				= 0x16,
	.global_control_reg			= IS31FL32XX_REG_NONE,
	.reset_reg				= 0x17,
	.pwm_register_base			= 0x01,
	.led_control_register_base		= 0x13,
	.enable_bits_per_led_control_register	= 6,
};

static int is31fl3216_reset(struct is31fl32xx_priv *priv);
static int is31fl3216_software_shutdown(struct is31fl32xx_priv *priv,
					bool enable);
static const struct is31fl32xx_chipdef is31fl3216_cdef = {
	.channels				= 16,
	.shutdown_reg				= IS31FL32XX_REG_NONE,
	.pwm_update_reg				= 0xB0,
	.global_control_reg			= IS31FL32XX_REG_NONE,
	.reset_reg				= IS31FL32XX_REG_NONE,
	.pwm_register_base			= 0x10,
	.pwm_registers_reversed			= true,
	.led_control_register_base		= 0x01,
	.enable_bits_per_led_control_register	= 8,
	.reset_func				= is31fl3216_reset,
	.sw_shutdown_func			= is31fl3216_software_shutdown,
};

static int is31fl32xx_write(struct is31fl32xx_priv *priv, u8 reg, u8 val)
{
	int ret;

	dev_dbg(&priv->client->dev, "writing register 0x%02X=0x%02X", reg, val);

	ret =  i2c_smbus_write_byte_data(priv->client, reg, val);
	if (ret) {
		dev_err(&priv->client->dev,
			"register write to 0x%02X failed (error %d)",
			reg, ret);
	}
	return ret;
}

 
static int is31fl3216_reset(struct is31fl32xx_priv *priv)
{
	unsigned int i;
	int ret;

	ret = is31fl32xx_write(priv, IS31FL3216_CONFIG_REG,
			       IS31FL3216_CONFIG_SSD_ENABLE);
	if (ret)
		return ret;
	for (i = 0; i < priv->cdef->channels; i++) {
		ret = is31fl32xx_write(priv, priv->cdef->pwm_register_base+i,
				       0x00);
		if (ret)
			return ret;
	}
	ret = is31fl32xx_write(priv, priv->cdef->pwm_update_reg, 0);
	if (ret)
		return ret;
	ret = is31fl32xx_write(priv, IS31FL3216_LIGHTING_EFFECT_REG, 0x00);
	if (ret)
		return ret;
	ret = is31fl32xx_write(priv, IS31FL3216_CHANNEL_CONFIG_REG, 0x00);
	if (ret)
		return ret;

	return 0;
}

 
static int is31fl3216_software_shutdown(struct is31fl32xx_priv *priv,
					bool enable)
{
	u8 value = enable ? IS31FL3216_CONFIG_SSD_ENABLE :
			    IS31FL3216_CONFIG_SSD_DISABLE;

	return is31fl32xx_write(priv, IS31FL3216_CONFIG_REG, value);
}

 
static int is31fl32xx_brightness_set(struct led_classdev *led_cdev,
				     enum led_brightness brightness)
{
	const struct is31fl32xx_led_data *led_data =
		container_of(led_cdev, struct is31fl32xx_led_data, cdev);
	const struct is31fl32xx_chipdef *cdef = led_data->priv->cdef;
	u8 pwm_register_offset;
	int ret;

	dev_dbg(led_cdev->dev, "%s: %d\n", __func__, brightness);

	 
	if (cdef->pwm_registers_reversed)
		pwm_register_offset = cdef->channels - led_data->channel;
	else
		pwm_register_offset = led_data->channel - 1;

	ret = is31fl32xx_write(led_data->priv,
			       cdef->pwm_register_base + pwm_register_offset,
			       brightness);
	if (ret)
		return ret;

	return is31fl32xx_write(led_data->priv, cdef->pwm_update_reg, 0);
}

static int is31fl32xx_reset_regs(struct is31fl32xx_priv *priv)
{
	const struct is31fl32xx_chipdef *cdef = priv->cdef;
	int ret;

	if (cdef->reset_reg != IS31FL32XX_REG_NONE) {
		ret = is31fl32xx_write(priv, cdef->reset_reg, 0);
		if (ret)
			return ret;
	}

	if (cdef->reset_func)
		return cdef->reset_func(priv);

	return 0;
}

static int is31fl32xx_software_shutdown(struct is31fl32xx_priv *priv,
					bool enable)
{
	const struct is31fl32xx_chipdef *cdef = priv->cdef;
	int ret;

	if (cdef->shutdown_reg != IS31FL32XX_REG_NONE) {
		u8 value = enable ? IS31FL32XX_SHUTDOWN_SSD_ENABLE :
				    IS31FL32XX_SHUTDOWN_SSD_DISABLE;
		ret = is31fl32xx_write(priv, cdef->shutdown_reg, value);
		if (ret)
			return ret;
	}

	if (cdef->sw_shutdown_func)
		return cdef->sw_shutdown_func(priv, enable);

	return 0;
}

static int is31fl32xx_init_regs(struct is31fl32xx_priv *priv)
{
	const struct is31fl32xx_chipdef *cdef = priv->cdef;
	int ret;

	ret = is31fl32xx_reset_regs(priv);
	if (ret)
		return ret;

	 
	if (cdef->led_control_register_base != IS31FL32XX_REG_NONE) {
		u8 value =
		    GENMASK(cdef->enable_bits_per_led_control_register-1, 0);
		u8 num_regs = cdef->channels /
				cdef->enable_bits_per_led_control_register;
		int i;

		for (i = 0; i < num_regs; i++) {
			ret = is31fl32xx_write(priv,
					       cdef->led_control_register_base+i,
					       value);
			if (ret)
				return ret;
		}
	}

	ret = is31fl32xx_software_shutdown(priv, false);
	if (ret)
		return ret;

	if (cdef->global_control_reg != IS31FL32XX_REG_NONE) {
		ret = is31fl32xx_write(priv, cdef->global_control_reg, 0x00);
		if (ret)
			return ret;
	}

	return 0;
}

static int is31fl32xx_parse_child_dt(const struct device *dev,
				     const struct device_node *child,
				     struct is31fl32xx_led_data *led_data)
{
	struct led_classdev *cdev = &led_data->cdev;
	int ret = 0;
	u32 reg;

	ret = of_property_read_u32(child, "reg", &reg);
	if (ret || reg < 1 || reg > led_data->priv->cdef->channels) {
		dev_err(dev,
			"Child node %pOF does not have a valid reg property\n",
			child);
		return -EINVAL;
	}
	led_data->channel = reg;

	cdev->brightness_set_blocking = is31fl32xx_brightness_set;

	return 0;
}

static struct is31fl32xx_led_data *is31fl32xx_find_led_data(
					struct is31fl32xx_priv *priv,
					u8 channel)
{
	size_t i;

	for (i = 0; i < priv->num_leds; i++) {
		if (priv->leds[i].channel == channel)
			return &priv->leds[i];
	}

	return NULL;
}

static int is31fl32xx_parse_dt(struct device *dev,
			       struct is31fl32xx_priv *priv)
{
	struct device_node *child;
	int ret = 0;

	for_each_available_child_of_node(dev_of_node(dev), child) {
		struct led_init_data init_data = {};
		struct is31fl32xx_led_data *led_data =
			&priv->leds[priv->num_leds];
		const struct is31fl32xx_led_data *other_led_data;

		led_data->priv = priv;

		ret = is31fl32xx_parse_child_dt(dev, child, led_data);
		if (ret)
			goto err;

		 
		other_led_data = is31fl32xx_find_led_data(priv,
							  led_data->channel);
		if (other_led_data) {
			dev_err(dev,
				"Node %pOF 'reg' conflicts with another LED\n",
				child);
			ret = -EINVAL;
			goto err;
		}

		init_data.fwnode = of_fwnode_handle(child);

		ret = devm_led_classdev_register_ext(dev, &led_data->cdev,
						     &init_data);
		if (ret) {
			dev_err(dev, "Failed to register LED for %pOF: %d\n",
				child, ret);
			goto err;
		}

		priv->num_leds++;
	}

	return 0;

err:
	of_node_put(child);
	return ret;
}

static const struct of_device_id of_is31fl32xx_match[] = {
	{ .compatible = "issi,is31fl3236", .data = &is31fl3236_cdef, },
	{ .compatible = "issi,is31fl3235", .data = &is31fl3235_cdef, },
	{ .compatible = "issi,is31fl3218", .data = &is31fl3218_cdef, },
	{ .compatible = "si-en,sn3218",    .data = &is31fl3218_cdef, },
	{ .compatible = "issi,is31fl3216", .data = &is31fl3216_cdef, },
	{ .compatible = "si-en,sn3216",    .data = &is31fl3216_cdef, },
	{},
};

MODULE_DEVICE_TABLE(of, of_is31fl32xx_match);

static int is31fl32xx_probe(struct i2c_client *client)
{
	const struct is31fl32xx_chipdef *cdef;
	struct device *dev = &client->dev;
	struct is31fl32xx_priv *priv;
	int count;
	int ret = 0;

	cdef = device_get_match_data(dev);

	count = of_get_available_child_count(dev_of_node(dev));
	if (!count)
		return -EINVAL;

	priv = devm_kzalloc(dev, struct_size(priv, leds, count),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->client = client;
	priv->cdef = cdef;
	i2c_set_clientdata(client, priv);

	ret = is31fl32xx_init_regs(priv);
	if (ret)
		return ret;

	ret = is31fl32xx_parse_dt(dev, priv);
	if (ret)
		return ret;

	return 0;
}

static void is31fl32xx_remove(struct i2c_client *client)
{
	struct is31fl32xx_priv *priv = i2c_get_clientdata(client);
	int ret;

	ret = is31fl32xx_reset_regs(priv);
	if (ret)
		dev_err(&client->dev, "Failed to reset registers on removal (%pe)\n",
			ERR_PTR(ret));
}

 
static const struct i2c_device_id is31fl32xx_id[] = {
	{ "is31fl3236" },
	{ "is31fl3235" },
	{ "is31fl3218" },
	{ "sn3218" },
	{ "is31fl3216" },
	{ "sn3216" },
	{},
};

MODULE_DEVICE_TABLE(i2c, is31fl32xx_id);

static struct i2c_driver is31fl32xx_driver = {
	.driver = {
		.name	= "is31fl32xx",
		.of_match_table = of_is31fl32xx_match,
	},
	.probe		= is31fl32xx_probe,
	.remove		= is31fl32xx_remove,
	.id_table	= is31fl32xx_id,
};

module_i2c_driver(is31fl32xx_driver);

MODULE_AUTHOR("David Rivshin <drivshin@allworx.com>");
MODULE_DESCRIPTION("ISSI IS31FL32xx LED driver");
MODULE_LICENSE("GPL v2");
