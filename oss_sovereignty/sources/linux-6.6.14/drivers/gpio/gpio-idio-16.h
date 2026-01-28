

#ifndef _IDIO_16_H_
#define _IDIO_16_H_

struct device;
struct regmap;
struct regmap_irq;


struct idio_16_regmap_config {
	struct device *parent;
	struct regmap *map;
	const struct regmap_irq *regmap_irqs;
	int num_regmap_irqs;
	unsigned int irq;
	bool no_status;
	bool filters;
};

int devm_idio_16_regmap_register(struct device *dev, const struct idio_16_regmap_config *config);

#endif 
