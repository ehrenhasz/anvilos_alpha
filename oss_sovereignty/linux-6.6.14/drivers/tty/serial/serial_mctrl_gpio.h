 
 

#ifndef __SERIAL_MCTRL_GPIO__
#define __SERIAL_MCTRL_GPIO__

#include <linux/err.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>

struct uart_port;

enum mctrl_gpio_idx {
	UART_GPIO_CTS,
	UART_GPIO_DSR,
	UART_GPIO_DCD,
	UART_GPIO_RNG,
	UART_GPIO_RI = UART_GPIO_RNG,
	UART_GPIO_RTS,
	UART_GPIO_DTR,
	UART_GPIO_MAX,
};

 
struct mctrl_gpios;

#ifdef CONFIG_GPIOLIB

 
void mctrl_gpio_set(struct mctrl_gpios *gpios, unsigned int mctrl);

 
unsigned int mctrl_gpio_get(struct mctrl_gpios *gpios, unsigned int *mctrl);

 
unsigned int
mctrl_gpio_get_outputs(struct mctrl_gpios *gpios, unsigned int *mctrl);

 
struct gpio_desc *mctrl_gpio_to_gpiod(struct mctrl_gpios *gpios,
				      enum mctrl_gpio_idx gidx);

 
struct mctrl_gpios *mctrl_gpio_init(struct uart_port *port, unsigned int idx);

 
struct mctrl_gpios *mctrl_gpio_init_noauto(struct device *dev,
					   unsigned int idx);

 
void mctrl_gpio_free(struct device *dev, struct mctrl_gpios *gpios);

 
void mctrl_gpio_enable_ms(struct mctrl_gpios *gpios);

 
void mctrl_gpio_disable_ms(struct mctrl_gpios *gpios);

 
void mctrl_gpio_enable_irq_wake(struct mctrl_gpios *gpios);

 
void mctrl_gpio_disable_irq_wake(struct mctrl_gpios *gpios);

#else  

static inline
void mctrl_gpio_set(struct mctrl_gpios *gpios, unsigned int mctrl)
{
}

static inline
unsigned int mctrl_gpio_get(struct mctrl_gpios *gpios, unsigned int *mctrl)
{
	return *mctrl;
}

static inline unsigned int
mctrl_gpio_get_outputs(struct mctrl_gpios *gpios, unsigned int *mctrl)
{
	return *mctrl;
}

static inline
struct gpio_desc *mctrl_gpio_to_gpiod(struct mctrl_gpios *gpios,
				      enum mctrl_gpio_idx gidx)
{
	return NULL;
}

static inline
struct mctrl_gpios *mctrl_gpio_init(struct uart_port *port, unsigned int idx)
{
	return NULL;
}

static inline
struct mctrl_gpios *mctrl_gpio_init_noauto(struct device *dev, unsigned int idx)
{
	return NULL;
}

static inline
void mctrl_gpio_free(struct device *dev, struct mctrl_gpios *gpios)
{
}

static inline void mctrl_gpio_enable_ms(struct mctrl_gpios *gpios)
{
}

static inline void mctrl_gpio_disable_ms(struct mctrl_gpios *gpios)
{
}

static inline void mctrl_gpio_enable_irq_wake(struct mctrl_gpios *gpios)
{
}

static inline void mctrl_gpio_disable_irq_wake(struct mctrl_gpios *gpios)
{
}

#endif  

#endif
