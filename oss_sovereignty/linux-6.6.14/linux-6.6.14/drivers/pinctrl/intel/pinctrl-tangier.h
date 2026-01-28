#ifndef PINCTRL_TANGIER_H
#define PINCTRL_TANGIER_H
#include <linux/spinlock_types.h>
#include <linux/types.h>
#include <linux/pinctrl/pinctrl.h>
#include "pinctrl-intel.h"
struct device;
struct platform_device;
#define TNG_FAMILY_NR			64
#define TNG_FAMILY_LEN			0x400
struct tng_family {
	unsigned int barno;
	unsigned int pin_base;
	size_t npins;
	bool protected;
	void __iomem *regs;
};
#define TNG_FAMILY(b, s, e)				\
	{						\
		.barno = (b),				\
		.pin_base = (s),			\
		.npins = (e) - (s) + 1,			\
	}
#define TNG_FAMILY_PROTECTED(b, s, e)			\
	{						\
		.barno = (b),				\
		.pin_base = (s),			\
		.npins = (e) - (s) + 1,			\
		.protected = true,			\
	}
struct tng_pinctrl {
	struct device *dev;
	raw_spinlock_t lock;
	struct pinctrl_desc pctldesc;
	struct pinctrl_dev *pctldev;
	const struct tng_family *families;
	size_t nfamilies;
	const struct intel_function *functions;
	size_t nfunctions;
	const struct intel_pingroup *groups;
	size_t ngroups;
	const struct pinctrl_pin_desc *pins;
	size_t npins;
};
int devm_tng_pinctrl_probe(struct platform_device *pdev);
#endif  
