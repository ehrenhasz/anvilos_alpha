#ifndef ST_UVIS25_H
#define ST_UVIS25_H
#define ST_UVIS25_DEV_NAME		"uvis25"
#include <linux/iio/iio.h>
struct st_uvis25_hw {
	struct regmap *regmap;
	struct iio_trigger *trig;
	bool enabled;
	int irq;
	struct {
		u8 chan;
		s64 ts __aligned(8);
	} scan;
};
extern const struct dev_pm_ops st_uvis25_pm_ops;
int st_uvis25_probe(struct device *dev, int irq, struct regmap *regmap);
#endif  
