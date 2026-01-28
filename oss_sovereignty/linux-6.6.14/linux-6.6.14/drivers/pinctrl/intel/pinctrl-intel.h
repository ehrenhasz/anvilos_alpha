#ifndef PINCTRL_INTEL_H
#define PINCTRL_INTEL_H
#include <linux/bits.h>
#include <linux/compiler_types.h>
#include <linux/gpio/driver.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/pm.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/spinlock_types.h>
struct platform_device;
struct device;
struct intel_pingroup {
	struct pingroup grp;
	unsigned short mode;
	const unsigned int *modes;
};
struct intel_function {
	struct pinfunction func;
};
#define INTEL_PINCTRL_MAX_GPP_SIZE	32
struct intel_padgroup {
	unsigned int reg_num;
	unsigned int base;
	unsigned int size;
	int gpio_base;
	unsigned int padown_num;
};
enum {
	INTEL_GPIO_BASE_ZERO	= -2,
	INTEL_GPIO_BASE_NOMAP	= -1,
	INTEL_GPIO_BASE_MATCH	= 0,
};
struct intel_community {
	unsigned int barno;
	unsigned int padown_offset;
	unsigned int padcfglock_offset;
	unsigned int hostown_offset;
	unsigned int is_offset;
	unsigned int ie_offset;
	unsigned int features;
	unsigned int pin_base;
	size_t npins;
	unsigned int gpp_size;
	unsigned int gpp_num_padown_regs;
	const struct intel_padgroup *gpps;
	size_t ngpps;
	const unsigned int *pad_map;
	unsigned short nirqs;
	unsigned short acpi_space_id;
	void __iomem *regs;
	void __iomem *pad_regs;
};
#define PINCTRL_FEATURE_DEBOUNCE	BIT(0)
#define PINCTRL_FEATURE_1K_PD		BIT(1)
#define PINCTRL_FEATURE_GPIO_HW_INFO	BIT(2)
#define PINCTRL_FEATURE_PWM		BIT(3)
#define PINCTRL_FEATURE_BLINK		BIT(4)
#define PINCTRL_FEATURE_EXP		BIT(5)
#define __INTEL_COMMUNITY(b, s, e, g, n, gs, gn, soc)		\
	{							\
		.barno = (b),					\
		.padown_offset = soc ## _PAD_OWN,		\
		.padcfglock_offset = soc ## _PADCFGLOCK,	\
		.hostown_offset = soc ## _HOSTSW_OWN,		\
		.is_offset = soc ## _GPI_IS,			\
		.ie_offset = soc ## _GPI_IE,			\
		.gpp_size = (gs),				\
		.gpp_num_padown_regs = (gn),			\
		.pin_base = (s),				\
		.npins = ((e) - (s) + 1),			\
		.gpps = (g),					\
		.ngpps = (n),					\
	}
#define INTEL_COMMUNITY_GPPS(b, s, e, g, soc)			\
	__INTEL_COMMUNITY(b, s, e, g, ARRAY_SIZE(g), 0, 0, soc)
#define INTEL_COMMUNITY_SIZE(b, s, e, gs, gn, soc)		\
	__INTEL_COMMUNITY(b, s, e, NULL, 0, gs, gn, soc)
#define PIN_GROUP(n, p, m)								\
	{										\
		.grp = PINCTRL_PINGROUP((n), (p), ARRAY_SIZE((p))),			\
		.mode = __builtin_choose_expr(__builtin_constant_p((m)), (m), 0),	\
		.modes = __builtin_choose_expr(__builtin_constant_p((m)), NULL, (m)),	\
	}
#define FUNCTION(n, g)							\
	{								\
		.func = PINCTRL_PINFUNCTION((n), (g), ARRAY_SIZE(g)),	\
	}
struct intel_pinctrl_soc_data {
	const char *uid;
	const struct pinctrl_pin_desc *pins;
	size_t npins;
	const struct intel_pingroup *groups;
	size_t ngroups;
	const struct intel_function *functions;
	size_t nfunctions;
	const struct intel_community *communities;
	size_t ncommunities;
};
const struct intel_pinctrl_soc_data *intel_pinctrl_get_soc_data(struct platform_device *pdev);
struct intel_pad_context;
struct intel_community_context;
struct intel_pinctrl_context {
	struct intel_pad_context *pads;
	struct intel_community_context *communities;
};
struct intel_pinctrl {
	struct device *dev;
	raw_spinlock_t lock;
	struct pinctrl_desc pctldesc;
	struct pinctrl_dev *pctldev;
	struct gpio_chip chip;
	const struct intel_pinctrl_soc_data *soc;
	struct intel_community *communities;
	size_t ncommunities;
	struct intel_pinctrl_context context;
	int irq;
};
int intel_pinctrl_probe_by_hid(struct platform_device *pdev);
int intel_pinctrl_probe_by_uid(struct platform_device *pdev);
#ifdef CONFIG_PM_SLEEP
int intel_pinctrl_suspend_noirq(struct device *dev);
int intel_pinctrl_resume_noirq(struct device *dev);
#endif
#define INTEL_PINCTRL_PM_OPS(_name)					\
const struct dev_pm_ops _name = {					\
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(intel_pinctrl_suspend_noirq,	\
				      intel_pinctrl_resume_noirq)	\
}
struct intel_community *intel_get_community(struct intel_pinctrl *pctrl, unsigned int pin);
int intel_get_groups_count(struct pinctrl_dev *pctldev);
const char *intel_get_group_name(struct pinctrl_dev *pctldev, unsigned int group);
int intel_get_group_pins(struct pinctrl_dev *pctldev, unsigned int group,
			 const unsigned int **pins, unsigned int *npins);
int intel_get_functions_count(struct pinctrl_dev *pctldev);
const char *intel_get_function_name(struct pinctrl_dev *pctldev, unsigned int function);
int intel_get_function_groups(struct pinctrl_dev *pctldev, unsigned int function,
			      const char * const **groups, unsigned int * const ngroups);
#endif  
