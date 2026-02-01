 
 

#ifndef __DRIVERS_PINCTRL_IMX_H
#define __DRIVERS_PINCTRL_IMX_H

#include <linux/pinctrl/pinmux.h>

struct platform_device;

extern struct pinmux_ops imx_pmx_ops;
extern const struct dev_pm_ops imx_pinctrl_pm_ops;

 
struct imx_pin_mmio {
	unsigned int mux_mode;
	u16 input_reg;
	unsigned int input_val;
	unsigned long config;
};

 
struct imx_pin_scu {
	unsigned int mux_mode;
	unsigned long config;
};

 
struct imx_pin {
	unsigned int pin;
	union {
		struct imx_pin_mmio mmio;
		struct imx_pin_scu scu;
	} conf;
};

 
struct imx_pin_reg {
	s16 mux_reg;
	s16 conf_reg;
};

 
struct imx_pinctrl {
	struct device *dev;
	struct pinctrl_dev *pctl;
	void __iomem *base;
	void __iomem *input_sel_base;
	const struct imx_pinctrl_soc_info *info;
	struct imx_pin_reg *pin_regs;
	unsigned int group_index;
	struct mutex mutex;
};

struct imx_pinctrl_soc_info {
	const struct pinctrl_pin_desc *pins;
	unsigned int npins;
	unsigned int flags;
	const char *gpr_compatible;

	 
	unsigned int mux_mask;
	u8 mux_shift;

	int (*gpio_set_direction)(struct pinctrl_dev *pctldev,
				  struct pinctrl_gpio_range *range,
				  unsigned offset,
				  bool input);
	int (*imx_pinconf_get)(struct pinctrl_dev *pctldev, unsigned int pin_id,
			       unsigned long *config);
	int (*imx_pinconf_set)(struct pinctrl_dev *pctldev, unsigned int pin_id,
			       unsigned long *configs, unsigned int num_configs);
	void (*imx_pinctrl_parse_pin)(struct imx_pinctrl *ipctl,
				      unsigned int *pin_id, struct imx_pin *pin,
				      const __be32 **list_p);
};

#define SHARE_MUX_CONF_REG	BIT(0)
#define ZERO_OFFSET_VALID	BIT(1)
#define IMX_USE_SCU		BIT(2)

#define NO_MUX		0x0
#define NO_PAD		0x0

#define IMX_PINCTRL_PIN(pin) PINCTRL_PIN(pin, #pin)

#define PAD_CTL_MASK(len)	((1 << len) - 1)
#define IMX_MUX_MASK	0x7
#define IOMUXC_CONFIG_SION	(0x1 << 4)

int imx_pinctrl_probe(struct platform_device *pdev,
			const struct imx_pinctrl_soc_info *info);

#define BM_PAD_CTL_GP_ENABLE		BIT(30)
#define BM_PAD_CTL_IFMUX_ENABLE		BIT(31)
#define BP_PAD_CTL_IFMUX		27

int imx_pinctrl_sc_ipc_init(struct platform_device *pdev);
int imx_pinconf_get_scu(struct pinctrl_dev *pctldev, unsigned pin_id,
			unsigned long *config);
int imx_pinconf_set_scu(struct pinctrl_dev *pctldev, unsigned pin_id,
			unsigned long *configs, unsigned num_configs);
void imx_pinctrl_parse_pin_scu(struct imx_pinctrl *ipctl,
			       unsigned int *pin_id, struct imx_pin *pin,
			       const __be32 **list_p);
#endif  
