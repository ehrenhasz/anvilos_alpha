#ifndef RM3100_CORE_H
#define RM3100_CORE_H
#include <linux/regmap.h>
extern const struct regmap_access_table rm3100_readable_table;
extern const struct regmap_access_table rm3100_writable_table;
extern const struct regmap_access_table rm3100_volatile_table;
int rm3100_common_probe(struct device *dev, struct regmap *regmap, int irq);
#endif  
