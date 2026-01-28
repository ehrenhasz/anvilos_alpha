#ifndef _MT8192_AFE_GPIO_H_
#define _MT8192_AFE_GPIO_H_
struct device;
int mt8192_afe_gpio_init(struct device *dev);
int mt8192_afe_gpio_request(struct device *dev, bool enable,
			    int dai, int uplink);
#endif
