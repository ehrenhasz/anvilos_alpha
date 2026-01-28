


#ifndef __MMA7455_H
#define __MMA7455_H

extern const struct regmap_config mma7455_core_regmap;

int mma7455_core_probe(struct device *dev, struct regmap *regmap,
		       const char *name);
void mma7455_core_remove(struct device *dev);

#endif
