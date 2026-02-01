 
 

#ifndef SOUND_SOC_CODECS_ADAU1372_H
#define SOUND_SOC_CODECS_ADAU1372_H

#include <linux/regmap.h>

struct device;

int adau1372_probe(struct device *dev, struct regmap *regmap,
		   void (*switch_mode)(struct device *dev));

extern const struct regmap_config adau1372_regmap_config;

#endif
