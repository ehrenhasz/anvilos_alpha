#ifndef __PLAT_GPIO_H
#define __PLAT_GPIO_H
#include <linux/init.h>
#include <linux/types.h>
#include <linux/irqdomain.h>
struct gpio_desc;
void orion_gpio_set_unused(unsigned pin);
void orion_gpio_set_blink(unsigned pin, int blink);
int orion_gpio_led_blink_set(struct gpio_desc *desc, int state,
	unsigned long *delay_on, unsigned long *delay_off);
#define GPIO_INPUT_OK		(1 << 0)
#define GPIO_OUTPUT_OK		(1 << 1)
void orion_gpio_set_valid(unsigned pin, int mode);
void __init orion_gpio_init(int gpio_base, int ngpio,
			    void __iomem *base, int mask_offset,
			    int secondary_irq_base,
			    int irq[4]);
#endif
