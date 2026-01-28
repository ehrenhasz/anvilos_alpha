


#ifndef __DRIVERS_PINCTRL_IMX1_H
#define __DRIVERS_PINCTRL_IMX1_H

struct platform_device;


struct imx1_pin {
	unsigned int pin_id;
	unsigned int mux_id;
	unsigned long config;
};


struct imx1_pin_group {
	const char *name;
	unsigned int *pin_ids;
	struct imx1_pin *pins;
	unsigned npins;
};


struct imx1_pmx_func {
	const char *name;
	const char **groups;
	unsigned num_groups;
};

struct imx1_pinctrl_soc_info {
	struct device *dev;
	const struct pinctrl_pin_desc *pins;
	unsigned int npins;
	struct imx1_pin_group *groups;
	unsigned int ngroups;
	struct imx1_pmx_func *functions;
	unsigned int nfunctions;
};

#define IMX_PINCTRL_PIN(pin) PINCTRL_PIN(pin, #pin)

int imx1_pinctrl_core_probe(struct platform_device *pdev,
			struct imx1_pinctrl_soc_info *info);
#endif 
