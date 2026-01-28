


#ifndef GPIOLIB_H
#define GPIOLIB_H

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h> 
#include <linux/gpio/driver.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/rwsem.h>

#define GPIOCHIP_NAME	"gpiochip"


struct gpio_device {
	struct device		dev;
	struct cdev		chrdev;
	int			id;
	struct device		*mockdev;
	struct module		*owner;
	struct gpio_chip	*chip;
	struct gpio_desc	*descs;
	int			base;
	u16			ngpio;
	const char		*label;
	void			*data;
	struct list_head        list;
	struct blocking_notifier_head line_state_notifier;
	struct blocking_notifier_head device_notifier;
	struct rw_semaphore	sem;

#ifdef CONFIG_PINCTRL
	
	struct list_head pin_ranges;
#endif
};

static inline struct gpio_device *to_gpio_device(struct device *dev)
{
	return container_of(dev, struct gpio_device, dev);
}


static __maybe_unused const char * const gpio_suffixes[] = { "gpios", "gpio" };


struct gpio_array {
	struct gpio_desc	**desc;
	unsigned int		size;
	struct gpio_chip	*chip;
	unsigned long		*get_mask;
	unsigned long		*set_mask;
	unsigned long		invert_mask[];
};

struct gpio_desc *gpiochip_get_desc(struct gpio_chip *gc, unsigned int hwnum);

#define for_each_gpio_desc(gc, desc)					\
	for (unsigned int __i = 0;					\
	     __i < gc->ngpio && (desc = gpiochip_get_desc(gc, __i));	\
	     __i++)							\

#define for_each_gpio_desc_with_flag(gc, desc, flag)			\
	for_each_gpio_desc(gc, desc)					\
		if (!test_bit(flag, &desc->flags)) {} else

int gpiod_get_array_value_complex(bool raw, bool can_sleep,
				  unsigned int array_size,
				  struct gpio_desc **desc_array,
				  struct gpio_array *array_info,
				  unsigned long *value_bitmap);
int gpiod_set_array_value_complex(bool raw, bool can_sleep,
				  unsigned int array_size,
				  struct gpio_desc **desc_array,
				  struct gpio_array *array_info,
				  unsigned long *value_bitmap);

extern spinlock_t gpio_lock;
extern struct list_head gpio_devices;

void gpiod_line_state_notify(struct gpio_desc *desc, unsigned long action);


struct gpio_desc {
	struct gpio_device	*gdev;
	unsigned long		flags;

#define FLAG_REQUESTED	0
#define FLAG_IS_OUT	1
#define FLAG_EXPORT	2	
#define FLAG_SYSFS	3	
#define FLAG_ACTIVE_LOW	6	
#define FLAG_OPEN_DRAIN	7	
#define FLAG_OPEN_SOURCE 8	
#define FLAG_USED_AS_IRQ 9	
#define FLAG_IRQ_IS_ENABLED 10	
#define FLAG_IS_HOGGED	11	
#define FLAG_TRANSITORY 12	
#define FLAG_PULL_UP    13	
#define FLAG_PULL_DOWN  14	
#define FLAG_BIAS_DISABLE    15	
#define FLAG_EDGE_RISING     16	
#define FLAG_EDGE_FALLING    17	
#define FLAG_EVENT_CLOCK_REALTIME	18 
#define FLAG_EVENT_CLOCK_HTE		19 

	
	const char		*label;
	
	const char		*name;
#ifdef CONFIG_OF_DYNAMIC
	struct device_node	*hog;
#endif
#ifdef CONFIG_GPIO_CDEV
	
	unsigned int		debounce_period_us;
#endif
};

#define gpiod_not_found(desc)		(IS_ERR(desc) && PTR_ERR(desc) == -ENOENT)

int gpiod_request(struct gpio_desc *desc, const char *label);
void gpiod_free(struct gpio_desc *desc);

static inline int gpiod_request_user(struct gpio_desc *desc, const char *label)
{
	int ret;

	ret = gpiod_request(desc, label);
	if (ret == -EPROBE_DEFER)
		ret = -ENODEV;

	return ret;
}

int gpiod_configure_flags(struct gpio_desc *desc, const char *con_id,
		unsigned long lflags, enum gpiod_flags dflags);
int gpio_set_debounce_timeout(struct gpio_desc *desc, unsigned int debounce);
int gpiod_hog(struct gpio_desc *desc, const char *name,
		unsigned long lflags, enum gpiod_flags dflags);
int gpiochip_get_ngpios(struct gpio_chip *gc, struct device *dev);


static inline int gpio_chip_hwgpio(const struct gpio_desc *desc)
{
	return desc - &desc->gdev->descs[0];
}



#define gpiod_emerg(desc, fmt, ...)					       \
	pr_emerg("gpio-%d (%s): " fmt, desc_to_gpio(desc), desc->label ? : "?",\
		 ##__VA_ARGS__)
#define gpiod_crit(desc, fmt, ...)					       \
	pr_crit("gpio-%d (%s): " fmt, desc_to_gpio(desc), desc->label ? : "?", \
		 ##__VA_ARGS__)
#define gpiod_err(desc, fmt, ...)					       \
	pr_err("gpio-%d (%s): " fmt, desc_to_gpio(desc), desc->label ? : "?",  \
		 ##__VA_ARGS__)
#define gpiod_warn(desc, fmt, ...)					       \
	pr_warn("gpio-%d (%s): " fmt, desc_to_gpio(desc), desc->label ? : "?", \
		 ##__VA_ARGS__)
#define gpiod_info(desc, fmt, ...)					       \
	pr_info("gpio-%d (%s): " fmt, desc_to_gpio(desc), desc->label ? : "?", \
		 ##__VA_ARGS__)
#define gpiod_dbg(desc, fmt, ...)					       \
	pr_debug("gpio-%d (%s): " fmt, desc_to_gpio(desc), desc->label ? : "?",\
		 ##__VA_ARGS__)



#define chip_emerg(gc, fmt, ...)					\
	dev_emerg(&gc->gpiodev->dev, "(%s): " fmt, gc->label, ##__VA_ARGS__)
#define chip_crit(gc, fmt, ...)					\
	dev_crit(&gc->gpiodev->dev, "(%s): " fmt, gc->label, ##__VA_ARGS__)
#define chip_err(gc, fmt, ...)					\
	dev_err(&gc->gpiodev->dev, "(%s): " fmt, gc->label, ##__VA_ARGS__)
#define chip_warn(gc, fmt, ...)					\
	dev_warn(&gc->gpiodev->dev, "(%s): " fmt, gc->label, ##__VA_ARGS__)
#define chip_info(gc, fmt, ...)					\
	dev_info(&gc->gpiodev->dev, "(%s): " fmt, gc->label, ##__VA_ARGS__)
#define chip_dbg(gc, fmt, ...)					\
	dev_dbg(&gc->gpiodev->dev, "(%s): " fmt, gc->label, ##__VA_ARGS__)

#endif 
