


 

#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/mutex.h>
#include <uapi/linux/uleds.h>

struct spi_byte_chipdef {
	 
	u8	off_value;
	 
	u8	max_value;
};

struct spi_byte_led {
	struct led_classdev		ldev;
	struct spi_device		*spi;
	char				name[LED_MAX_NAME_SIZE];
	struct mutex			mutex;
	const struct spi_byte_chipdef	*cdef;
};

static const struct spi_byte_chipdef ubnt_acb_spi_led_cdef = {
	.off_value = 0x0,
	.max_value = 0x3F,
};

static const struct of_device_id spi_byte_dt_ids[] = {
	{ .compatible = "ubnt,acb-spi-led", .data = &ubnt_acb_spi_led_cdef },
	{},
};

MODULE_DEVICE_TABLE(of, spi_byte_dt_ids);

static int spi_byte_brightness_set_blocking(struct led_classdev *dev,
					    enum led_brightness brightness)
{
	struct spi_byte_led *led = container_of(dev, struct spi_byte_led, ldev);
	u8 value;
	int ret;

	value = (u8) brightness + led->cdef->off_value;

	mutex_lock(&led->mutex);
	ret = spi_write(led->spi, &value, sizeof(value));
	mutex_unlock(&led->mutex);

	return ret;
}

static int spi_byte_probe(struct spi_device *spi)
{
	struct device_node *child;
	struct device *dev = &spi->dev;
	struct spi_byte_led *led;
	const char *name = "leds-spi-byte::";
	const char *state;
	int ret;

	if (of_get_available_child_count(dev_of_node(dev)) != 1) {
		dev_err(dev, "Device must have exactly one LED sub-node.");
		return -EINVAL;
	}
	child = of_get_next_available_child(dev_of_node(dev), NULL);

	led = devm_kzalloc(dev, sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	of_property_read_string(child, "label", &name);
	strscpy(led->name, name, sizeof(led->name));
	led->spi = spi;
	mutex_init(&led->mutex);
	led->cdef = device_get_match_data(dev);
	led->ldev.name = led->name;
	led->ldev.brightness = LED_OFF;
	led->ldev.max_brightness = led->cdef->max_value - led->cdef->off_value;
	led->ldev.brightness_set_blocking = spi_byte_brightness_set_blocking;

	state = of_get_property(child, "default-state", NULL);
	if (state) {
		if (!strcmp(state, "on")) {
			led->ldev.brightness = led->ldev.max_brightness;
		} else if (strcmp(state, "off")) {
			 
			dev_err(dev, "default-state can only be 'on' or 'off'");
			return -EINVAL;
		}
	}
	spi_byte_brightness_set_blocking(&led->ldev,
					 led->ldev.brightness);

	ret = devm_led_classdev_register(&spi->dev, &led->ldev);
	if (ret) {
		mutex_destroy(&led->mutex);
		return ret;
	}
	spi_set_drvdata(spi, led);

	return 0;
}

static void spi_byte_remove(struct spi_device *spi)
{
	struct spi_byte_led	*led = spi_get_drvdata(spi);

	mutex_destroy(&led->mutex);
}

static struct spi_driver spi_byte_driver = {
	.probe		= spi_byte_probe,
	.remove		= spi_byte_remove,
	.driver = {
		.name		= KBUILD_MODNAME,
		.of_match_table	= spi_byte_dt_ids,
	},
};

module_spi_driver(spi_byte_driver);

MODULE_AUTHOR("Christian Mauderer <oss@c-mauderer.de>");
MODULE_DESCRIPTION("single byte SPI LED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("spi:leds-spi-byte");
