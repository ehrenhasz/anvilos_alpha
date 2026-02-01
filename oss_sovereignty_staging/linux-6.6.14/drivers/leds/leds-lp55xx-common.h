 
 

#ifndef _LEDS_LP55XX_COMMON_H
#define _LEDS_LP55XX_COMMON_H

#include <linux/led-class-multicolor.h>

enum lp55xx_engine_index {
	LP55XX_ENGINE_INVALID,
	LP55XX_ENGINE_1,
	LP55XX_ENGINE_2,
	LP55XX_ENGINE_3,
	LP55XX_ENGINE_MAX = LP55XX_ENGINE_3,
};

enum lp55xx_engine_mode {
	LP55XX_ENGINE_DISABLED,
	LP55XX_ENGINE_LOAD,
	LP55XX_ENGINE_RUN,
};

#define LP55XX_DEV_ATTR_RW(name, show, store)	\
	DEVICE_ATTR(name, S_IRUGO | S_IWUSR, show, store)
#define LP55XX_DEV_ATTR_RO(name, show)		\
	DEVICE_ATTR(name, S_IRUGO, show, NULL)
#define LP55XX_DEV_ATTR_WO(name, store)		\
	DEVICE_ATTR(name, S_IWUSR, NULL, store)

#define show_mode(nr)							\
static ssize_t show_engine##nr##_mode(struct device *dev,		\
				    struct device_attribute *attr,	\
				    char *buf)				\
{									\
	return show_engine_mode(dev, attr, buf, nr);			\
}

#define store_mode(nr)							\
static ssize_t store_engine##nr##_mode(struct device *dev,		\
				     struct device_attribute *attr,	\
				     const char *buf, size_t len)	\
{									\
	return store_engine_mode(dev, attr, buf, len, nr);		\
}

#define show_leds(nr)							\
static ssize_t show_engine##nr##_leds(struct device *dev,		\
			    struct device_attribute *attr,		\
			    char *buf)					\
{									\
	return show_engine_leds(dev, attr, buf, nr);			\
}

#define store_leds(nr)						\
static ssize_t store_engine##nr##_leds(struct device *dev,	\
			     struct device_attribute *attr,	\
			     const char *buf, size_t len)	\
{								\
	return store_engine_leds(dev, attr, buf, len, nr);	\
}

#define store_load(nr)							\
static ssize_t store_engine##nr##_load(struct device *dev,		\
				     struct device_attribute *attr,	\
				     const char *buf, size_t len)	\
{									\
	return store_engine_load(dev, attr, buf, len, nr);		\
}

struct lp55xx_led;
struct lp55xx_chip;

 
struct lp55xx_reg {
	u8 addr;
	u8 val;
};

 
struct lp55xx_device_config {
	const struct lp55xx_reg reset;
	const struct lp55xx_reg enable;
	const int max_channel;

	 
	int (*post_init_device) (struct lp55xx_chip *chip);

	 
	int (*brightness_fn)(struct lp55xx_led *led);

	 
	int (*multicolor_brightness_fn)(struct lp55xx_led *led);

	 
	void (*set_led_current) (struct lp55xx_led *led, u8 led_current);

	 
	void (*firmware_cb)(struct lp55xx_chip *chip);

	 
	void (*run_engine) (struct lp55xx_chip *chip, bool start);

	 
	const struct attribute_group *dev_attr_group;
};

 
struct lp55xx_engine {
	enum lp55xx_engine_mode mode;
	u16 led_mux;
};

 
struct lp55xx_chip {
	struct i2c_client *cl;
	struct clk *clk;
	struct lp55xx_platform_data *pdata;
	struct mutex lock;	 
	int num_leds;
	struct lp55xx_device_config *cfg;
	enum lp55xx_engine_index engine_idx;
	struct lp55xx_engine engines[LP55XX_ENGINE_MAX];
	const struct firmware *fw;
};

 
struct lp55xx_led {
	int chan_nr;
	struct led_classdev cdev;
	struct led_classdev_mc mc_cdev;
	u8 led_current;
	u8 max_current;
	u8 brightness;
	struct lp55xx_chip *chip;
};

 
extern int lp55xx_write(struct lp55xx_chip *chip, u8 reg, u8 val);
extern int lp55xx_read(struct lp55xx_chip *chip, u8 reg, u8 *val);
extern int lp55xx_update_bits(struct lp55xx_chip *chip, u8 reg,
			u8 mask, u8 val);

 
extern bool lp55xx_is_extclk_used(struct lp55xx_chip *chip);

 
extern int lp55xx_init_device(struct lp55xx_chip *chip);
extern void lp55xx_deinit_device(struct lp55xx_chip *chip);

 
extern int lp55xx_register_leds(struct lp55xx_led *led,
				struct lp55xx_chip *chip);

 
extern int lp55xx_register_sysfs(struct lp55xx_chip *chip);
extern void lp55xx_unregister_sysfs(struct lp55xx_chip *chip);

 
extern struct lp55xx_platform_data
*lp55xx_of_populate_pdata(struct device *dev, struct device_node *np,
			  struct lp55xx_chip *chip);

#endif  
