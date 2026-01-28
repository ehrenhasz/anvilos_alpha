#ifndef __PLAT_GPIO_CFG_H
#define __PLAT_GPIO_CFG_H __FILE__
#include <linux/types.h>
typedef unsigned int __bitwise samsung_gpio_pull_t;
struct samsung_gpio_chip;
struct samsung_gpio_cfg {
	unsigned int	cfg_eint;
	samsung_gpio_pull_t	(*get_pull)(struct samsung_gpio_chip *chip, unsigned offs);
	int		(*set_pull)(struct samsung_gpio_chip *chip, unsigned offs,
				    samsung_gpio_pull_t pull);
	unsigned (*get_config)(struct samsung_gpio_chip *chip, unsigned offs);
	int	 (*set_config)(struct samsung_gpio_chip *chip, unsigned offs,
			       unsigned config);
};
#define S3C_GPIO_SPECIAL_MARK	(0xfffffff0)
#define S3C_GPIO_SPECIAL(x)	(S3C_GPIO_SPECIAL_MARK | (x))
#define S3C_GPIO_INPUT	(S3C_GPIO_SPECIAL(0))
#define S3C_GPIO_OUTPUT	(S3C_GPIO_SPECIAL(1))
#define S3C_GPIO_SFN(x)	(S3C_GPIO_SPECIAL(x))
#define samsung_gpio_is_cfg_special(_cfg) \
	(((_cfg) & S3C_GPIO_SPECIAL_MARK) == S3C_GPIO_SPECIAL_MARK)
extern int s3c_gpio_cfgpin(unsigned int pin, unsigned int to);
extern int s3c_gpio_cfgpin_range(unsigned int start, unsigned int nr,
				 unsigned int cfg);
#define S3C_GPIO_PULL_NONE	((__force samsung_gpio_pull_t)0x00)
#define S3C_GPIO_PULL_DOWN	((__force samsung_gpio_pull_t)0x01)
#define S3C_GPIO_PULL_UP	((__force samsung_gpio_pull_t)0x02)
extern int s3c_gpio_setpull(unsigned int pin, samsung_gpio_pull_t pull);
extern int s3c_gpio_cfgall_range(unsigned int start, unsigned int nr,
				 unsigned int cfg, samsung_gpio_pull_t pull);
static inline int s3c_gpio_cfgrange_nopull(unsigned int pin, unsigned int size,
					   unsigned int cfg)
{
	return s3c_gpio_cfgall_range(pin, size, cfg, S3C_GPIO_PULL_NONE);
}
#endif  
