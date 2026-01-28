#ifndef __LINUX_ANYBUSS_CONTROLLER_H__
#define __LINUX_ANYBUSS_CONTROLLER_H__
#include <linux/device.h>
#include <linux/regmap.h>
struct anybuss_ops {
	void (*reset)(struct device *dev, bool assert);
	struct regmap *regmap;
	int irq;
	int host_idx;
};
struct anybuss_host;
struct anybuss_host * __must_check
anybuss_host_common_probe(struct device *dev,
			  const struct anybuss_ops *ops);
void anybuss_host_common_remove(struct anybuss_host *host);
struct anybuss_host * __must_check
devm_anybuss_host_common_probe(struct device *dev,
			       const struct anybuss_ops *ops);
#endif  
