
 

#include <linux/gpio/driver.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
 
#include <linux/mfd/rohm-bd71815.h>

struct bd71815_gpio {
	 
	struct gpio_chip chip;
	 
	struct device *dev;
	struct regmap *regmap;
};

static int bd71815gpo_get(struct gpio_chip *chip, unsigned int offset)
{
	struct bd71815_gpio *bd71815 = gpiochip_get_data(chip);
	int ret, val;

	ret = regmap_read(bd71815->regmap, BD71815_REG_GPO, &val);
	if (ret)
		return ret;

	return (val >> offset) & 1;
}

static void bd71815gpo_set(struct gpio_chip *chip, unsigned int offset,
			   int value)
{
	struct bd71815_gpio *bd71815 = gpiochip_get_data(chip);
	int ret, bit;

	bit = BIT(offset);

	if (value)
		ret = regmap_set_bits(bd71815->regmap, BD71815_REG_GPO, bit);
	else
		ret = regmap_clear_bits(bd71815->regmap, BD71815_REG_GPO, bit);

	if (ret)
		dev_warn(bd71815->dev, "failed to toggle GPO\n");
}

static int bd71815_gpio_set_config(struct gpio_chip *chip, unsigned int offset,
				   unsigned long config)
{
	struct bd71815_gpio *bdgpio = gpiochip_get_data(chip);

	switch (pinconf_to_config_param(config)) {
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		return regmap_update_bits(bdgpio->regmap,
					  BD71815_REG_GPO,
					  BD71815_GPIO_DRIVE_MASK << offset,
					  BD71815_GPIO_OPEN_DRAIN << offset);
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		return regmap_update_bits(bdgpio->regmap,
					  BD71815_REG_GPO,
					  BD71815_GPIO_DRIVE_MASK << offset,
					  BD71815_GPIO_CMOS << offset);
	default:
		break;
	}
	return -ENOTSUPP;
}

 
static int bd71815gpo_direction_get(struct gpio_chip *gc, unsigned int offset)
{
	return GPIO_LINE_DIRECTION_OUT;
}

 
static const struct gpio_chip bd71815gpo_chip = {
	.label			= "bd71815",
	.owner			= THIS_MODULE,
	.get			= bd71815gpo_get,
	.get_direction		= bd71815gpo_direction_get,
	.set			= bd71815gpo_set,
	.set_config		= bd71815_gpio_set_config,
	.can_sleep		= true,
};

#define BD71815_TWO_GPIOS	GENMASK(1, 0)
#define BD71815_ONE_GPIO	BIT(0)

 
static int bd71815_init_valid_mask(struct gpio_chip *gc,
				   unsigned long *valid_mask,
				   unsigned int ngpios)
{
	if (ngpios != 2)
		return 0;

	if (gc->parent && device_property_present(gc->parent,
						  "rohm,enable-hidden-gpo"))
		*valid_mask = BD71815_TWO_GPIOS;
	else
		*valid_mask = BD71815_ONE_GPIO;

	return 0;
}

static int gpo_bd71815_probe(struct platform_device *pdev)
{
	struct bd71815_gpio *g;
	struct device *parent, *dev;

	 
	dev = &pdev->dev;
	 
	parent = dev->parent;

	g = devm_kzalloc(dev, sizeof(*g), GFP_KERNEL);
	if (!g)
		return -ENOMEM;

	g->chip = bd71815gpo_chip;

	 
	if (device_property_present(parent, "rohm,enable-hidden-gpo"))
		g->chip.ngpio = 2;
	else
		g->chip.ngpio = 1;

	g->chip.init_valid_mask = bd71815_init_valid_mask;
	g->chip.base = -1;
	g->chip.parent = parent;
	g->regmap = dev_get_regmap(parent, NULL);
	g->dev = dev;

	return devm_gpiochip_add_data(dev, &g->chip, g);
}

static struct platform_driver gpo_bd71815_driver = {
	.driver = {
		.name	= "bd71815-gpo",
	},
	.probe		= gpo_bd71815_probe,
};
module_platform_driver(gpo_bd71815_driver);

MODULE_ALIAS("platform:bd71815-gpo");
MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_AUTHOR("Peter Yang <yanglsh@embest-tech.com>");
MODULE_DESCRIPTION("GPO interface for BD71815");
MODULE_LICENSE("GPL");
