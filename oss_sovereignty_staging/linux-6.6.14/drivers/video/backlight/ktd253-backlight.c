
 
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>

 
#define KTD253_MIN_RATIO 1
#define KTD253_MAX_RATIO 32
#define KTD253_DEFAULT_RATIO 13

#define KTD253_T_LOW_NS (200 + 10)  
#define KTD253_T_HIGH_NS (200 + 10)  
#define KTD253_T_OFF_CRIT_NS 100000  
#define KTD253_T_OFF_MS 3

struct ktd253_backlight {
	struct device *dev;
	struct backlight_device *bl;
	struct gpio_desc *gpiod;
	u16 ratio;
};

static void ktd253_backlight_set_max_ratio(struct ktd253_backlight *ktd253)
{
	gpiod_set_value_cansleep(ktd253->gpiod, 1);
	ndelay(KTD253_T_HIGH_NS);
	 
}

static int ktd253_backlight_stepdown(struct ktd253_backlight *ktd253)
{
	 
	u64 ns;

	ns = ktime_get_ns();
	gpiod_set_value(ktd253->gpiod, 0);
	ndelay(KTD253_T_LOW_NS);
	gpiod_set_value(ktd253->gpiod, 1);
	ns = ktime_get_ns() - ns;
	if (ns >= KTD253_T_OFF_CRIT_NS) {
		dev_err(ktd253->dev, "PCM on backlight took too long (%llu ns)\n", ns);
		return -EAGAIN;
	}
	ndelay(KTD253_T_HIGH_NS);
	return 0;
}

static int ktd253_backlight_update_status(struct backlight_device *bl)
{
	struct ktd253_backlight *ktd253 = bl_get_data(bl);
	int brightness = backlight_get_brightness(bl);
	u16 target_ratio;
	u16 current_ratio = ktd253->ratio;
	int ret;

	dev_dbg(ktd253->dev, "new brightness/ratio: %d/32\n", brightness);

	target_ratio = brightness;

	if (target_ratio == current_ratio)
		 
		return 0;

	if (target_ratio == 0) {
		gpiod_set_value_cansleep(ktd253->gpiod, 0);
		 
		msleep(KTD253_T_OFF_MS);
		ktd253->ratio = 0;
		return 0;
	}

	if (current_ratio == 0) {
		ktd253_backlight_set_max_ratio(ktd253);
		current_ratio = KTD253_MAX_RATIO;
	}

	while (current_ratio != target_ratio) {
		 
		ret = ktd253_backlight_stepdown(ktd253);
		if (ret == -EAGAIN) {
			 
			gpiod_set_value_cansleep(ktd253->gpiod, 0);
			msleep(KTD253_T_OFF_MS);
			ktd253_backlight_set_max_ratio(ktd253);
			current_ratio = KTD253_MAX_RATIO;
		} else if (current_ratio == KTD253_MIN_RATIO) {
			 
			current_ratio = KTD253_MAX_RATIO;
		} else {
			current_ratio--;
		}
	}
	ktd253->ratio = current_ratio;

	dev_dbg(ktd253->dev, "new ratio set to %d/32\n", target_ratio);

	return 0;
}

static const struct backlight_ops ktd253_backlight_ops = {
	.options	= BL_CORE_SUSPENDRESUME,
	.update_status	= ktd253_backlight_update_status,
};

static int ktd253_backlight_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct backlight_device *bl;
	struct ktd253_backlight *ktd253;
	u32 max_brightness;
	u32 brightness;
	int ret;

	ktd253 = devm_kzalloc(dev, sizeof(*ktd253), GFP_KERNEL);
	if (!ktd253)
		return -ENOMEM;
	ktd253->dev = dev;

	ret = device_property_read_u32(dev, "max-brightness", &max_brightness);
	if (ret)
		max_brightness = KTD253_MAX_RATIO;
	if (max_brightness > KTD253_MAX_RATIO) {
		 
		dev_err(dev, "illegal max brightness specified\n");
		max_brightness = KTD253_MAX_RATIO;
	}

	ret = device_property_read_u32(dev, "default-brightness", &brightness);
	if (ret)
		brightness = KTD253_DEFAULT_RATIO;
	if (brightness > max_brightness) {
		 
		dev_err(dev, "default brightness exceeds max brightness\n");
		brightness = max_brightness;
	}

	ktd253->gpiod = devm_gpiod_get(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(ktd253->gpiod))
		return dev_err_probe(dev, PTR_ERR(ktd253->gpiod),
				     "gpio line missing or invalid.\n");
	gpiod_set_consumer_name(ktd253->gpiod, dev_name(dev));
	 
	msleep(KTD253_T_OFF_MS);

	bl = devm_backlight_device_register(dev, dev_name(dev), dev, ktd253,
					    &ktd253_backlight_ops, NULL);
	if (IS_ERR(bl)) {
		dev_err(dev, "failed to register backlight\n");
		return PTR_ERR(bl);
	}
	bl->props.max_brightness = max_brightness;
	 
	if (brightness) {
		bl->props.brightness = brightness;
		bl->props.power = FB_BLANK_UNBLANK;
	} else {
		bl->props.brightness = 0;
		bl->props.power = FB_BLANK_POWERDOWN;
	}

	ktd253->bl = bl;
	platform_set_drvdata(pdev, bl);
	backlight_update_status(bl);

	return 0;
}

static const struct of_device_id ktd253_backlight_of_match[] = {
	{ .compatible = "kinetic,ktd253" },
	{ .compatible = "kinetic,ktd259" },
	{   }
};
MODULE_DEVICE_TABLE(of, ktd253_backlight_of_match);

static struct platform_driver ktd253_backlight_driver = {
	.driver = {
		.name = "ktd253-backlight",
		.of_match_table = ktd253_backlight_of_match,
	},
	.probe		= ktd253_backlight_probe,
};
module_platform_driver(ktd253_backlight_driver);

MODULE_AUTHOR("Linus Walleij <linus.walleij@linaro.org>");
MODULE_DESCRIPTION("Kinetic KTD253 Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ktd253-backlight");
