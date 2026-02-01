 
#ifndef PINCTRL_PINCTRL_ABx500_H
#define PINCTRL_PINCTRL_ABx500_H

#include <linux/types.h>

struct pinctrl_pin_desc;

 
#define PINCTRL_AB8500	0
#define PINCTRL_AB8505	1

 
enum abx500_pin_func {
	ABX500_DEFAULT,
	ABX500_ALT_A,
	ABX500_ALT_B,
	ABX500_ALT_C,
};

enum abx500_gpio_pull_updown {
	ABX500_GPIO_PULL_DOWN = 0x0,
	ABX500_GPIO_PULL_NONE = 0x1,
	ABX500_GPIO_PULL_UP = 0x3,
};

enum abx500_gpio_vinsel {
	ABX500_GPIO_VINSEL_VBAT = 0x0,
	ABX500_GPIO_VINSEL_VIN_1V8 = 0x1,
	ABX500_GPIO_VINSEL_VDD_BIF = 0x2,
};

 
struct abx500_function {
	const char *name;
	const char * const *groups;
	unsigned ngroups;
};

 
struct abx500_pingroup {
	const char *name;
	const unsigned int *pins;
	const unsigned npins;
	int altsetting;
};

#define ALTERNATE_FUNCTIONS(pin, sel_bit, alt1, alt2, alta, altb, altc)	\
{									\
	.pin_number = pin,						\
	.gpiosel_bit = sel_bit,						\
	.alt_bit1 = alt1,						\
	.alt_bit2 = alt2,						\
	.alta_val = alta,						\
	.altb_val = altb,						\
	.altc_val = altc,						\
}

#define UNUSED -1
 
struct alternate_functions {
	unsigned pin_number;
	s8 gpiosel_bit;
	s8 alt_bit1;
	s8 alt_bit2;
	u8 alta_val;
	u8 altb_val;
	u8 altc_val;
};

#define GPIO_IRQ_CLUSTER(a, b, c)	\
{					\
	.start = a,			\
	.end = b,			\
	.to_irq = c,			\
}

 

struct abx500_gpio_irq_cluster {
	int start;
	int end;
	int to_irq;
};

 
struct abx500_pinrange {
	unsigned int offset;
	unsigned int npins;
	int altfunc;
};

#define ABX500_PINRANGE(a, b, c) { .offset = a, .npins = b, .altfunc = c }

 

struct abx500_pinctrl_soc_data {
	const struct abx500_pinrange *gpio_ranges;
	unsigned gpio_num_ranges;
	const struct pinctrl_pin_desc *pins;
	unsigned npins;
	const struct abx500_function *functions;
	unsigned nfunctions;
	const struct abx500_pingroup *groups;
	unsigned ngroups;
	struct alternate_functions *alternate_functions;
	struct abx500_gpio_irq_cluster *gpio_irq_cluster;
	unsigned ngpio_irq_cluster;
	int irq_gpio_rising_offset;
	int irq_gpio_falling_offset;
	int irq_gpio_factor;
};

#ifdef CONFIG_PINCTRL_AB8500

void abx500_pinctrl_ab8500_init(struct abx500_pinctrl_soc_data **soc);

#else

static inline void
abx500_pinctrl_ab8500_init(struct abx500_pinctrl_soc_data **soc)
{
}

#endif

#ifdef CONFIG_PINCTRL_AB8505

void abx500_pinctrl_ab8505_init(struct abx500_pinctrl_soc_data **soc);

#else

static inline void
abx500_pinctrl_ab8505_init(struct abx500_pinctrl_soc_data **soc)
{
}

#endif

#endif  
