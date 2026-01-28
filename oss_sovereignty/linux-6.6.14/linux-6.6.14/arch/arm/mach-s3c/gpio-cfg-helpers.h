#ifndef __PLAT_GPIO_CFG_HELPERS_H
#define __PLAT_GPIO_CFG_HELPERS_H __FILE__
static inline int samsung_gpio_do_setcfg(struct samsung_gpio_chip *chip,
					 unsigned int off, unsigned int config)
{
	return (chip->config->set_config)(chip, off, config);
}
static inline int samsung_gpio_do_setpull(struct samsung_gpio_chip *chip,
					  unsigned int off, samsung_gpio_pull_t pull)
{
	return (chip->config->set_pull)(chip, off, pull);
}
#endif  
