#ifndef __DRIVERS_PINCTRL_S32_H
#define __DRIVERS_PINCTRL_S32_H
struct platform_device;
struct s32_pin_group {
	struct pingroup data;
	unsigned int *pin_sss;
};
struct s32_pin_range {
	unsigned int start;
	unsigned int end;
};
struct s32_pinctrl_soc_data {
	const struct pinctrl_pin_desc *pins;
	unsigned int npins;
	const struct s32_pin_range *mem_pin_ranges;
	unsigned int mem_regions;
};
struct s32_pinctrl_soc_info {
	struct device *dev;
	const struct s32_pinctrl_soc_data *soc_data;
	struct s32_pin_group *groups;
	unsigned int ngroups;
	struct pinfunction *functions;
	unsigned int nfunctions;
	unsigned int grp_index;
};
#define S32_PINCTRL_PIN(pin)	PINCTRL_PIN(pin, #pin)
#define S32_PIN_RANGE(_start, _end) { .start = _start, .end = _end }
int s32_pinctrl_probe(struct platform_device *pdev,
		      const struct s32_pinctrl_soc_data *soc_data);
int s32_pinctrl_resume(struct device *dev);
int s32_pinctrl_suspend(struct device *dev);
#endif  
