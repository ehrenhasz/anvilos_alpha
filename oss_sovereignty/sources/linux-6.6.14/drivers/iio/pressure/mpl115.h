


#include <linux/pm_runtime.h>

#ifndef _MPL115_H_
#define _MPL115_H_

struct mpl115_ops {
	int (*init)(struct device *);
	int (*read)(struct device *, u8);
	int (*write)(struct device *, u8, u8);
};

int mpl115_probe(struct device *dev, const char *name,
			const struct mpl115_ops *ops);


extern const struct dev_pm_ops mpl115_dev_pm_ops;

#endif
