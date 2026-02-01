
 

#include <linux/err.h>
#include <linux/module.h>
#include <linux/gpio/driver.h>
#include <linux/bitops.h>
#include <linux/platform_device.h>

#include <asm/coprocessor.h>  

#ifndef XCHAL_CP_ID_XTIOP
#error GPIO32 option is not enabled for your xtensa core variant
#endif

#if XCHAL_HAVE_CP

static inline unsigned long enable_cp(unsigned long *cpenable)
{
	unsigned long flags;

	local_irq_save(flags);
	*cpenable = xtensa_get_sr(cpenable);
	xtensa_set_sr(*cpenable | BIT(XCHAL_CP_ID_XTIOP), cpenable);
	return flags;
}

static inline void disable_cp(unsigned long flags, unsigned long cpenable)
{
	xtensa_set_sr(cpenable, cpenable);
	local_irq_restore(flags);
}

#else

static inline unsigned long enable_cp(unsigned long *cpenable)
{
	*cpenable = 0;  
	return 0;
}

static inline void disable_cp(unsigned long flags, unsigned long cpenable)
{
}

#endif  

static int xtensa_impwire_get_direction(struct gpio_chip *gc, unsigned offset)
{
	return GPIO_LINE_DIRECTION_IN;  
}

static int xtensa_impwire_get_value(struct gpio_chip *gc, unsigned offset)
{
	unsigned long flags, saved_cpenable;
	u32 impwire;

	flags = enable_cp(&saved_cpenable);
	__asm__ __volatile__("read_impwire %0" : "=a" (impwire));
	disable_cp(flags, saved_cpenable);

	return !!(impwire & BIT(offset));
}

static void xtensa_impwire_set_value(struct gpio_chip *gc, unsigned offset,
				    int value)
{
	BUG();  
}

static int xtensa_expstate_get_direction(struct gpio_chip *gc, unsigned offset)
{
	return GPIO_LINE_DIRECTION_OUT;  
}

static int xtensa_expstate_get_value(struct gpio_chip *gc, unsigned offset)
{
	unsigned long flags, saved_cpenable;
	u32 expstate;

	flags = enable_cp(&saved_cpenable);
	__asm__ __volatile__("rur.expstate %0" : "=a" (expstate));
	disable_cp(flags, saved_cpenable);

	return !!(expstate & BIT(offset));
}

static void xtensa_expstate_set_value(struct gpio_chip *gc, unsigned offset,
				     int value)
{
	unsigned long flags, saved_cpenable;
	u32 mask = BIT(offset);
	u32 val = value ? BIT(offset) : 0;

	flags = enable_cp(&saved_cpenable);
	__asm__ __volatile__("wrmsk_expstate %0, %1"
			     :: "a" (val), "a" (mask));
	disable_cp(flags, saved_cpenable);
}

static struct gpio_chip impwire_chip = {
	.label		= "impwire",
	.base		= -1,
	.ngpio		= 32,
	.get_direction	= xtensa_impwire_get_direction,
	.get		= xtensa_impwire_get_value,
	.set		= xtensa_impwire_set_value,
};

static struct gpio_chip expstate_chip = {
	.label		= "expstate",
	.base		= -1,
	.ngpio		= 32,
	.get_direction	= xtensa_expstate_get_direction,
	.get		= xtensa_expstate_get_value,
	.set		= xtensa_expstate_set_value,
};

static int xtensa_gpio_probe(struct platform_device *pdev)
{
	int ret;

	ret = gpiochip_add_data(&impwire_chip, NULL);
	if (ret)
		return ret;
	return gpiochip_add_data(&expstate_chip, NULL);
}

static struct platform_driver xtensa_gpio_driver = {
	.driver		= {
		.name		= "xtensa-gpio",
	},
	.probe		= xtensa_gpio_probe,
};

static int __init xtensa_gpio_init(void)
{
	struct platform_device *pdev;

	pdev = platform_device_register_simple("xtensa-gpio", 0, NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	return platform_driver_register(&xtensa_gpio_driver);
}
device_initcall(xtensa_gpio_init);

MODULE_AUTHOR("Baruch Siach <baruch@tkos.co.il>");
MODULE_DESCRIPTION("Xtensa LX4 GPIO32 driver");
MODULE_LICENSE("GPL");
