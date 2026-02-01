




#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/platform_data/mlxreg.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

 
#define MLXREG_LED_OFFSET_BLINK_3HZ	0x01  
#define MLXREG_LED_OFFSET_BLINK_6HZ	0x02  
#define MLXREG_LED_IS_OFF		0x00  
#define MLXREG_LED_RED_SOLID		0x05  
#define MLXREG_LED_GREEN_SOLID		0x0D  
#define MLXREG_LED_AMBER_SOLID		0x09  
#define MLXREG_LED_BLINK_3HZ		167  
#define MLXREG_LED_BLINK_6HZ		83  
#define MLXREG_LED_CAPABILITY_CLEAR	GENMASK(31, 8)  

 
struct mlxreg_led_data {
	struct mlxreg_core_data *data;
	struct led_classdev led_cdev;
	u8 base_color;
	void *data_parent;
	char led_cdev_name[MLXREG_CORE_LABEL_MAX_SIZE];
};

#define cdev_to_priv(c) container_of(c, struct mlxreg_led_data, led_cdev)

 
struct mlxreg_led_priv_data {
	struct platform_device *pdev;
	struct mlxreg_core_platform_data *pdata;
	struct mutex access_lock;  
};

static int
mlxreg_led_store_hw(struct mlxreg_led_data *led_data, u8 vset)
{
	struct mlxreg_led_priv_data *priv = led_data->data_parent;
	struct mlxreg_core_platform_data *led_pdata = priv->pdata;
	struct mlxreg_core_data *data = led_data->data;
	u32 regval;
	u32 nib;
	int ret;

	 
	mutex_lock(&priv->access_lock);

	ret = regmap_read(led_pdata->regmap, data->reg, &regval);
	if (ret)
		goto access_error;

	nib = (ror32(data->mask, data->bit) == 0xf0) ? rol32(vset, data->bit) :
	      rol32(vset, data->bit + 4);
	regval = (regval & data->mask) | nib;

	ret = regmap_write(led_pdata->regmap, data->reg, regval);

access_error:
	mutex_unlock(&priv->access_lock);

	return ret;
}

static enum led_brightness
mlxreg_led_get_hw(struct mlxreg_led_data *led_data)
{
	struct mlxreg_led_priv_data *priv = led_data->data_parent;
	struct mlxreg_core_platform_data *led_pdata = priv->pdata;
	struct mlxreg_core_data *data = led_data->data;
	u32 regval;
	int err;

	 
	err = regmap_read(led_pdata->regmap, data->reg, &regval);
	if (err < 0) {
		dev_warn(led_data->led_cdev.dev, "Failed to get current brightness, error: %d\n",
			 err);
		 
		return LED_OFF;
	}

	regval = regval & ~data->mask;
	regval = (ror32(data->mask, data->bit) == 0xf0) ? ror32(regval,
		 data->bit) : ror32(regval, data->bit + 4);
	if (regval >= led_data->base_color &&
	    regval <= (led_data->base_color + MLXREG_LED_OFFSET_BLINK_6HZ))
		return LED_FULL;

	return LED_OFF;
}

static int
mlxreg_led_brightness_set(struct led_classdev *cled, enum led_brightness value)
{
	struct mlxreg_led_data *led_data = cdev_to_priv(cled);

	if (value)
		return mlxreg_led_store_hw(led_data, led_data->base_color);
	else
		return mlxreg_led_store_hw(led_data, MLXREG_LED_IS_OFF);
}

static enum led_brightness
mlxreg_led_brightness_get(struct led_classdev *cled)
{
	struct mlxreg_led_data *led_data = cdev_to_priv(cled);

	return mlxreg_led_get_hw(led_data);
}

static int
mlxreg_led_blink_set(struct led_classdev *cled, unsigned long *delay_on,
		     unsigned long *delay_off)
{
	struct mlxreg_led_data *led_data = cdev_to_priv(cled);
	int err;

	 
	if (!(*delay_on == 0 && *delay_off == 0) &&
	    !(*delay_on == MLXREG_LED_BLINK_3HZ &&
	      *delay_off == MLXREG_LED_BLINK_3HZ) &&
	    !(*delay_on == MLXREG_LED_BLINK_6HZ &&
	      *delay_off == MLXREG_LED_BLINK_6HZ))
		return -EINVAL;

	if (*delay_on == MLXREG_LED_BLINK_6HZ)
		err = mlxreg_led_store_hw(led_data, led_data->base_color +
					  MLXREG_LED_OFFSET_BLINK_6HZ);
	else if (*delay_on == MLXREG_LED_BLINK_3HZ)
		err = mlxreg_led_store_hw(led_data, led_data->base_color +
					  MLXREG_LED_OFFSET_BLINK_3HZ);
	else
		err = mlxreg_led_store_hw(led_data, led_data->base_color);

	return err;
}

static int mlxreg_led_config(struct mlxreg_led_priv_data *priv)
{
	struct mlxreg_core_platform_data *led_pdata = priv->pdata;
	struct mlxreg_core_data *data = led_pdata->data;
	struct mlxreg_led_data *led_data;
	struct led_classdev *led_cdev;
	enum led_brightness brightness;
	u32 regval;
	int i;
	int err;

	for (i = 0; i < led_pdata->counter; i++, data++) {
		led_data = devm_kzalloc(&priv->pdev->dev, sizeof(*led_data),
					GFP_KERNEL);
		if (!led_data)
			return -ENOMEM;

		if (data->capability) {
			err = regmap_read(led_pdata->regmap, data->capability,
					  &regval);
			if (err) {
				dev_err(&priv->pdev->dev, "Failed to query capability register\n");
				return err;
			}
			if (!(regval & data->bit))
				continue;
			 
			data->bit &= MLXREG_LED_CAPABILITY_CLEAR;
		}

		led_cdev = &led_data->led_cdev;
		led_data->data_parent = priv;
		if (strstr(data->label, "red") ||
		    strstr(data->label, "orange")) {
			brightness = LED_OFF;
			led_data->base_color = MLXREG_LED_RED_SOLID;
		} else if (strstr(data->label, "amber")) {
			brightness = LED_OFF;
			led_data->base_color = MLXREG_LED_AMBER_SOLID;
		} else {
			brightness = LED_OFF;
			led_data->base_color = MLXREG_LED_GREEN_SOLID;
		}
		snprintf(led_data->led_cdev_name, sizeof(led_data->led_cdev_name),
			 "mlxreg:%s", data->label);
		led_cdev->name = led_data->led_cdev_name;
		led_cdev->brightness = brightness;
		led_cdev->max_brightness = LED_ON;
		led_cdev->brightness_set_blocking =
						mlxreg_led_brightness_set;
		led_cdev->brightness_get = mlxreg_led_brightness_get;
		led_cdev->blink_set = mlxreg_led_blink_set;
		led_cdev->flags = LED_CORE_SUSPENDRESUME;
		led_data->data = data;
		err = devm_led_classdev_register(&priv->pdev->dev, led_cdev);
		if (err)
			return err;

		if (led_cdev->brightness)
			mlxreg_led_brightness_set(led_cdev,
						  led_cdev->brightness);
		dev_info(led_cdev->dev, "label: %s, mask: 0x%02x, offset:0x%02x\n",
			 data->label, data->mask, data->reg);
	}

	return 0;
}

static int mlxreg_led_probe(struct platform_device *pdev)
{
	struct mlxreg_core_platform_data *led_pdata;
	struct mlxreg_led_priv_data *priv;

	led_pdata = dev_get_platdata(&pdev->dev);
	if (!led_pdata) {
		dev_err(&pdev->dev, "Failed to get platform data.\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	mutex_init(&priv->access_lock);
	priv->pdev = pdev;
	priv->pdata = led_pdata;

	return mlxreg_led_config(priv);
}

static int mlxreg_led_remove(struct platform_device *pdev)
{
	struct mlxreg_led_priv_data *priv = dev_get_drvdata(&pdev->dev);

	mutex_destroy(&priv->access_lock);

	return 0;
}

static struct platform_driver mlxreg_led_driver = {
	.driver = {
	    .name = "leds-mlxreg",
	},
	.probe = mlxreg_led_probe,
	.remove = mlxreg_led_remove,
};

module_platform_driver(mlxreg_led_driver);

MODULE_AUTHOR("Vadim Pasternak <vadimp@mellanox.com>");
MODULE_DESCRIPTION("Mellanox LED regmap driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("platform:leds-mlxreg");
