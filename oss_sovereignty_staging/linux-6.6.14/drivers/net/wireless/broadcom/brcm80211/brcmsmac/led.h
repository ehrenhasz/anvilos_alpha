 

#ifndef _BRCM_LED_H_
#define _BRCM_LED_H_

struct gpio_desc;

struct brcms_led {
	char name[32];
	struct gpio_desc *gpiod;
};

#ifdef CONFIG_BRCMSMAC_LEDS
void brcms_led_unregister(struct brcms_info *wl);
int brcms_led_register(struct brcms_info *wl);
#else
static inline void brcms_led_unregister(struct brcms_info *wl) {};
static inline int brcms_led_register(struct brcms_info *wl)
{
	return -ENOTSUPP;
};
#endif

#endif  
