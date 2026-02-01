
 

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <linux/gpio/driver.h>

 

#define ZEVIO_GPIO_SECTION_SIZE			0x40

 
#define ZEVIO_GPIO_INT_MASKED_STATUS	0x00
#define ZEVIO_GPIO_INT_STATUS		0x04
#define ZEVIO_GPIO_INT_UNMASK		0x08
#define ZEVIO_GPIO_INT_MASK		0x0C
#define ZEVIO_GPIO_DIRECTION		0x10
#define ZEVIO_GPIO_OUTPUT		0x14
#define ZEVIO_GPIO_INPUT			0x18
#define ZEVIO_GPIO_INT_STICKY		0x20

 
#define ZEVIO_GPIO_BIT(gpio) (gpio&7)

struct zevio_gpio {
	struct gpio_chip        chip;
	spinlock_t		lock;
	void __iomem		*regs;
};

static inline u32 zevio_gpio_port_get(struct zevio_gpio *c, unsigned pin,
					unsigned port_offset)
{
	unsigned section_offset = ((pin >> 3) & 3)*ZEVIO_GPIO_SECTION_SIZE;
	return readl(IOMEM(c->regs + section_offset + port_offset));
}

static inline void zevio_gpio_port_set(struct zevio_gpio *c, unsigned pin,
					unsigned port_offset, u32 val)
{
	unsigned section_offset = ((pin >> 3) & 3)*ZEVIO_GPIO_SECTION_SIZE;
	writel(val, IOMEM(c->regs + section_offset + port_offset));
}

 
static int zevio_gpio_get(struct gpio_chip *chip, unsigned pin)
{
	struct zevio_gpio *controller = gpiochip_get_data(chip);
	u32 val, dir;

	spin_lock(&controller->lock);
	dir = zevio_gpio_port_get(controller, pin, ZEVIO_GPIO_DIRECTION);
	if (dir & BIT(ZEVIO_GPIO_BIT(pin)))
		val = zevio_gpio_port_get(controller, pin, ZEVIO_GPIO_INPUT);
	else
		val = zevio_gpio_port_get(controller, pin, ZEVIO_GPIO_OUTPUT);
	spin_unlock(&controller->lock);

	return (val >> ZEVIO_GPIO_BIT(pin)) & 0x1;
}

static void zevio_gpio_set(struct gpio_chip *chip, unsigned pin, int value)
{
	struct zevio_gpio *controller = gpiochip_get_data(chip);
	u32 val;

	spin_lock(&controller->lock);
	val = zevio_gpio_port_get(controller, pin, ZEVIO_GPIO_OUTPUT);
	if (value)
		val |= BIT(ZEVIO_GPIO_BIT(pin));
	else
		val &= ~BIT(ZEVIO_GPIO_BIT(pin));

	zevio_gpio_port_set(controller, pin, ZEVIO_GPIO_OUTPUT, val);
	spin_unlock(&controller->lock);
}

static int zevio_gpio_direction_input(struct gpio_chip *chip, unsigned pin)
{
	struct zevio_gpio *controller = gpiochip_get_data(chip);
	u32 val;

	spin_lock(&controller->lock);

	val = zevio_gpio_port_get(controller, pin, ZEVIO_GPIO_DIRECTION);
	val |= BIT(ZEVIO_GPIO_BIT(pin));
	zevio_gpio_port_set(controller, pin, ZEVIO_GPIO_DIRECTION, val);

	spin_unlock(&controller->lock);

	return 0;
}

static int zevio_gpio_direction_output(struct gpio_chip *chip,
				       unsigned pin, int value)
{
	struct zevio_gpio *controller = gpiochip_get_data(chip);
	u32 val;

	spin_lock(&controller->lock);
	val = zevio_gpio_port_get(controller, pin, ZEVIO_GPIO_OUTPUT);
	if (value)
		val |= BIT(ZEVIO_GPIO_BIT(pin));
	else
		val &= ~BIT(ZEVIO_GPIO_BIT(pin));

	zevio_gpio_port_set(controller, pin, ZEVIO_GPIO_OUTPUT, val);
	val = zevio_gpio_port_get(controller, pin, ZEVIO_GPIO_DIRECTION);
	val &= ~BIT(ZEVIO_GPIO_BIT(pin));
	zevio_gpio_port_set(controller, pin, ZEVIO_GPIO_DIRECTION, val);

	spin_unlock(&controller->lock);

	return 0;
}

static int zevio_gpio_to_irq(struct gpio_chip *chip, unsigned pin)
{
	 

	return -ENXIO;
}

static const struct gpio_chip zevio_gpio_chip = {
	.direction_input	= zevio_gpio_direction_input,
	.direction_output	= zevio_gpio_direction_output,
	.set			= zevio_gpio_set,
	.get			= zevio_gpio_get,
	.to_irq			= zevio_gpio_to_irq,
	.base			= 0,
	.owner			= THIS_MODULE,
	.ngpio			= 32,
};

 
static int zevio_gpio_probe(struct platform_device *pdev)
{
	struct zevio_gpio *controller;
	int status, i;

	controller = devm_kzalloc(&pdev->dev, sizeof(*controller), GFP_KERNEL);
	if (!controller)
		return -ENOMEM;

	 
	controller->chip = zevio_gpio_chip;
	controller->chip.parent = &pdev->dev;

	controller->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(controller->regs))
		return dev_err_probe(&pdev->dev, PTR_ERR(controller->regs),
				     "failed to ioremap memory resource\n");

	status = devm_gpiochip_add_data(&pdev->dev, &controller->chip, controller);
	if (status) {
		dev_err(&pdev->dev, "failed to add gpiochip: %d\n", status);
		return status;
	}

	spin_lock_init(&controller->lock);

	 
	for (i = 0; i < controller->chip.ngpio; i += 8)
		zevio_gpio_port_set(controller, i, ZEVIO_GPIO_INT_MASK, 0xFF);

	dev_dbg(controller->chip.parent, "ZEVIO GPIO controller set up!\n");

	return 0;
}

static const struct of_device_id zevio_gpio_of_match[] = {
	{ .compatible = "lsi,zevio-gpio", },
	{ },
};

static struct platform_driver zevio_gpio_driver = {
	.driver		= {
		.name	= "gpio-zevio",
		.of_match_table = zevio_gpio_of_match,
		.suppress_bind_attrs = true,
	},
	.probe		= zevio_gpio_probe,
};
builtin_platform_driver(zevio_gpio_driver);
