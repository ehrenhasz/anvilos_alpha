 
 
#ifndef __PINCTRL_MSM_H__
#define __PINCTRL_MSM_H__

#include <linux/pm.h>
#include <linux/types.h>

#include <linux/pinctrl/pinctrl.h>

struct platform_device;

struct pinctrl_pin_desc;

#define APQ_PIN_FUNCTION(fname)					\
	[APQ_MUX_##fname] = PINCTRL_PINFUNCTION(#fname,		\
					fname##_groups,		\
					ARRAY_SIZE(fname##_groups))

#define IPQ_PIN_FUNCTION(fname)					\
	[IPQ_MUX_##fname] = PINCTRL_PINFUNCTION(#fname,		\
					fname##_groups,		\
					ARRAY_SIZE(fname##_groups))

#define MSM_PIN_FUNCTION(fname) 				\
	[msm_mux_##fname] = PINCTRL_PINFUNCTION(#fname,		\
					fname##_groups,		\
					ARRAY_SIZE(fname##_groups))

#define QCA_PIN_FUNCTION(fname)					\
	[qca_mux_##fname] = PINCTRL_PINFUNCTION(#fname,		\
					fname##_groups,		\
					ARRAY_SIZE(fname##_groups))

 
struct msm_pingroup {
	struct pingroup grp;

	unsigned *funcs;
	unsigned nfuncs;

	u32 ctl_reg;
	u32 io_reg;
	u32 intr_cfg_reg;
	u32 intr_status_reg;
	u32 intr_target_reg;

	unsigned int tile:2;

	unsigned mux_bit:5;

	unsigned pull_bit:5;
	unsigned drv_bit:5;
	unsigned i2c_pull_bit:5;

	unsigned od_bit:5;
	unsigned egpio_enable:5;
	unsigned egpio_present:5;
	unsigned oe_bit:5;
	unsigned in_bit:5;
	unsigned out_bit:5;

	unsigned intr_enable_bit:5;
	unsigned intr_status_bit:5;
	unsigned intr_ack_high:1;

	unsigned intr_target_bit:5;
	unsigned intr_target_width:5;
	unsigned intr_target_kpss_val:5;
	unsigned intr_raw_status_bit:5;
	unsigned intr_polarity_bit:5;
	unsigned intr_detection_bit:5;
	unsigned intr_detection_width:5;
};

 
struct msm_gpio_wakeirq_map {
	unsigned int gpio;
	unsigned int wakeirq;
};

 
struct msm_pinctrl_soc_data {
	const struct pinctrl_pin_desc *pins;
	unsigned npins;
	const struct pinfunction *functions;
	unsigned nfunctions;
	const struct msm_pingroup *groups;
	unsigned ngroups;
	unsigned ngpios;
	bool pull_no_keeper;
	const char *const *tiles;
	unsigned int ntiles;
	const int *reserved_gpios;
	const struct msm_gpio_wakeirq_map *wakeirq_map;
	unsigned int nwakeirq_map;
	bool wakeirq_dual_edge_errata;
	unsigned int gpio_func;
	unsigned int egpio_func;
};

extern const struct dev_pm_ops msm_pinctrl_dev_pm_ops;

int msm_pinctrl_probe(struct platform_device *pdev,
		      const struct msm_pinctrl_soc_data *soc_data);
int msm_pinctrl_remove(struct platform_device *pdev);

#endif
