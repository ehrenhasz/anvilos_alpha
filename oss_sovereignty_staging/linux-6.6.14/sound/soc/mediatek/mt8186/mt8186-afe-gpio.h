 

#ifndef _MT8186_AFE_GPIO_H_
#define _MT8186_AFE_GPIO_H_

struct mtk_base_afe;

int mt8186_afe_gpio_init(struct device *dev);

int mt8186_afe_gpio_request(struct device *dev, bool enable,
			    int dai, int uplink);

#endif
