


#ifndef _GPIO_TANGIER_H_
#define _GPIO_TANGIER_H_

#include <linux/gpio/driver.h>
#include <linux/spinlock_types.h>
#include <linux/types.h>

struct device;

struct tng_gpio_context;


#define GWMR_EHL	0x100	
#define GWSR_EHL	0x118	
#define GSIR_EHL	0x130	


#define GWMR_MRFLD	0x400	
#define GWSR_MRFLD	0x418	
#define GSIR_MRFLD	0xc00	


struct tng_wake_regs {
	u32 gwmr;
	u32 gwsr;
	u32 gsir;
};


struct tng_gpio_pinrange {
	unsigned int gpio_base;
	unsigned int pin_base;
	unsigned int npins;
};

#define GPIO_PINRANGE(gstart, gend, pstart)		\
(struct tng_gpio_pinrange) {				\
		.gpio_base = (gstart),			\
		.pin_base = (pstart),			\
		.npins = (gend) - (gstart) + 1,		\
	}


struct tng_gpio_pin_info {
	const struct tng_gpio_pinrange *pin_ranges;
	unsigned int nranges;
	const char *name;
};


struct tng_gpio_info {
	int base;
	u16 ngpio;
	unsigned int first;
};


struct tng_gpio {
	struct gpio_chip chip;
	void __iomem *reg_base;
	int irq;
	raw_spinlock_t lock;
	struct device *dev;
	struct tng_gpio_context *ctx;
	struct tng_wake_regs wake_regs;
	struct tng_gpio_pin_info pin_info;
	struct tng_gpio_info info;
};

int devm_tng_gpio_probe(struct device *dev, struct tng_gpio *gpio);

int tng_gpio_suspend(struct device *dev);
int tng_gpio_resume(struct device *dev);

#endif 
