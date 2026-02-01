
 

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware/xlnx-zynqmp.h>

 
#define MODE_PINS			4

 
static int modepin_gpio_get_value(struct gpio_chip *chip, unsigned int pin)
{
	u32 regval = 0;
	int ret;

	ret = zynqmp_pm_bootmode_read(&regval);
	if (ret)
		return ret;

	 
	if (regval & BIT(pin))
		return !!(regval & BIT(pin + 8));
	else
		return !!(regval & BIT(pin + 4));
}

 
static void modepin_gpio_set_value(struct gpio_chip *chip, unsigned int pin,
				   int state)
{
	u32 bootpin_val = 0;
	int ret;

	zynqmp_pm_bootmode_read(&bootpin_val);

	 
	bootpin_val |= BIT(pin);

	if (state)
		bootpin_val |= BIT(pin + 8);
	else
		bootpin_val &= ~BIT(pin + 8);

	 
	ret = zynqmp_pm_bootmode_write(bootpin_val);
	if (ret)
		pr_err("modepin: set value error %d for pin %d\n", ret, pin);
}

 
static int modepin_gpio_dir_in(struct gpio_chip *chip, unsigned int pin)
{
	return 0;
}

 
static int modepin_gpio_dir_out(struct gpio_chip *chip, unsigned int pin,
				int state)
{
	return 0;
}

 
static int modepin_gpio_probe(struct platform_device *pdev)
{
	struct gpio_chip *chip;
	int status;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	platform_set_drvdata(pdev, chip);

	 
	chip->base = -1;
	chip->ngpio = MODE_PINS;
	chip->owner = THIS_MODULE;
	chip->parent = &pdev->dev;
	chip->get = modepin_gpio_get_value;
	chip->set = modepin_gpio_set_value;
	chip->direction_input = modepin_gpio_dir_in;
	chip->direction_output = modepin_gpio_dir_out;
	chip->label = dev_name(&pdev->dev);

	 
	status = devm_gpiochip_add_data(&pdev->dev, chip, chip);
	if (status)
		return dev_err_probe(&pdev->dev, status,
			      "Failed to add GPIO chip\n");

	return status;
}

static const struct of_device_id modepin_platform_id[] = {
	{ .compatible = "xlnx,zynqmp-gpio-modepin", },
	{ }
};

static struct platform_driver modepin_platform_driver = {
	.driver = {
		.name = "modepin-gpio",
		.of_match_table = modepin_platform_id,
	},
	.probe = modepin_gpio_probe,
};

module_platform_driver(modepin_platform_driver);

MODULE_AUTHOR("Piyush Mehta <piyush.mehta@xilinx.com>");
MODULE_DESCRIPTION("ZynqMP Boot PS_MODE Configuration");
MODULE_LICENSE("GPL v2");
