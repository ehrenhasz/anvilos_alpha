#ifndef _I8255_H_
#define _I8255_H_
struct device;
struct irq_domain;
struct regmap;
#define i8255_volatile_regmap_range(_base) regmap_reg_range(_base, _base + 0x2)
struct i8255_regmap_config {
	struct device *parent;
	struct regmap *map;
	int num_ppi;
	const char *const *names;
	struct irq_domain *domain;
};
int devm_i8255_regmap_register(struct device *dev,
			       const struct i8255_regmap_config *config);
#endif  
