
 

#ifndef __PINCTRL_OWL_H__
#define __PINCTRL_OWL_H__

#define OWL_PINCONF_SLEW_SLOW 0
#define OWL_PINCONF_SLEW_FAST 1

#define MUX_PG(group_name, reg, shift, width)				\
	{								\
		.name = #group_name,					\
		.pads = group_name##_pads,				\
		.npads = ARRAY_SIZE(group_name##_pads),			\
		.funcs = group_name##_funcs,				\
		.nfuncs = ARRAY_SIZE(group_name##_funcs),		\
		.mfpctl_reg  = MFCTL##reg,				\
		.mfpctl_shift = shift,					\
		.mfpctl_width = width,					\
		.drv_reg = -1,						\
		.drv_shift = -1,					\
		.drv_width = -1,					\
		.sr_reg = -1,						\
		.sr_shift = -1,						\
		.sr_width = -1,						\
	}

#define DRV_PG(group_name, reg, shift, width)				\
	{								\
		.name = #group_name,					\
		.pads = group_name##_pads,				\
		.npads = ARRAY_SIZE(group_name##_pads),			\
		.mfpctl_reg  = -1,					\
		.mfpctl_shift = -1,					\
		.mfpctl_width = -1,					\
		.drv_reg = PAD_DRV##reg,				\
		.drv_shift = shift,					\
		.drv_width = width,					\
		.sr_reg = -1,						\
		.sr_shift = -1,						\
		.sr_width = -1,						\
	}

#define SR_PG(group_name, reg, shift, width)				\
	{								\
		.name = #group_name,					\
		.pads = group_name##_pads,				\
		.npads = ARRAY_SIZE(group_name##_pads),			\
		.mfpctl_reg  = -1,					\
		.mfpctl_shift = -1,					\
		.mfpctl_width = -1,					\
		.drv_reg = -1,						\
		.drv_shift = -1,					\
		.drv_width = -1,					\
		.sr_reg = PAD_SR##reg,					\
		.sr_shift = shift,					\
		.sr_width = width,					\
	}

#define FUNCTION(fname)					\
	{						\
		.name = #fname,				\
		.groups = fname##_groups,		\
		.ngroups = ARRAY_SIZE(fname##_groups),	\
	}

 
#define PULLCTL_CONF(pull_reg, pull_sft, pull_wdt)	\
	{						\
		.reg = PAD_PULLCTL##pull_reg,		\
		.shift = pull_sft,			\
		.width = pull_wdt,			\
	}

#define PAD_PULLCTL_CONF(pad_name, pull_reg, pull_sft, pull_wdt)	\
	struct owl_pullctl pad_name##_pullctl_conf			\
		= PULLCTL_CONF(pull_reg, pull_sft, pull_wdt)

#define ST_CONF(st_reg, st_sft, st_wdt)			\
	{						\
		.reg = PAD_ST##st_reg,			\
		.shift = st_sft,			\
		.width = st_wdt,			\
	}

#define PAD_ST_CONF(pad_name, st_reg, st_sft, st_wdt)	\
	struct owl_st pad_name##_st_conf		\
		= ST_CONF(st_reg, st_sft, st_wdt)

#define PAD_INFO(name)					\
	{						\
		.pad = name,				\
		.pullctl = NULL,			\
		.st = NULL,				\
	}

#define PAD_INFO_ST(name)				\
	{						\
		.pad = name,				\
		.pullctl = NULL,			\
		.st = &name##_st_conf,			\
	}

#define PAD_INFO_PULLCTL(name)				\
	{						\
		.pad = name,				\
		.pullctl = &name##_pullctl_conf,	\
		.st = NULL,				\
	}

#define PAD_INFO_PULLCTL_ST(name)			\
	{						\
		.pad = name,				\
		.pullctl = &name##_pullctl_conf,	\
		.st = &name##_st_conf,			\
	}

#define OWL_GPIO_PORT_A		0
#define OWL_GPIO_PORT_B		1
#define OWL_GPIO_PORT_C		2
#define OWL_GPIO_PORT_D		3
#define OWL_GPIO_PORT_E		4
#define OWL_GPIO_PORT_F		5

#define OWL_GPIO_PORT(port, base, count, _outen, _inen, _dat, _intc_ctl,\
			_intc_pd, _intc_msk, _intc_type, _share)	\
	[OWL_GPIO_PORT_##port] = {				\
		.offset = base,					\
		.pins = count,					\
		.outen = _outen,				\
		.inen = _inen,					\
		.dat = _dat,					\
		.intc_ctl = _intc_ctl,				\
		.intc_pd = _intc_pd,				\
		.intc_msk = _intc_msk,				\
		.intc_type = _intc_type,			\
		.shared_ctl_offset = _share,			\
	}

enum owl_pinconf_drv {
	OWL_PINCONF_DRV_2MA,
	OWL_PINCONF_DRV_4MA,
	OWL_PINCONF_DRV_8MA,
	OWL_PINCONF_DRV_12MA,
};

 
#define OWL_GPIO_CTLR_PENDING		0
#define OWL_GPIO_CTLR_ENABLE		1
#define OWL_GPIO_CTLR_SAMPLE_CLK_24M	2

 
#define OWL_GPIO_INT_LEVEL_HIGH		0
#define OWL_GPIO_INT_LEVEL_LOW		1
#define OWL_GPIO_INT_EDGE_RISING	2
#define OWL_GPIO_INT_EDGE_FALLING	3
#define OWL_GPIO_INT_MASK		3

 
struct owl_pullctl {
	int reg;
	unsigned int shift;
	unsigned int width;
};

 
struct owl_st {
	int reg;
	unsigned int shift;
	unsigned int width;
};

 
struct owl_pingroup {
	const char *name;
	unsigned int *pads;
	unsigned int npads;
	unsigned int *funcs;
	unsigned int nfuncs;

	int mfpctl_reg;
	unsigned int mfpctl_shift;
	unsigned int mfpctl_width;

	int drv_reg;
	unsigned int drv_shift;
	unsigned int drv_width;

	int sr_reg;
	unsigned int sr_shift;
	unsigned int sr_width;
};

 
struct owl_padinfo {
	int pad;
	struct owl_pullctl *pullctl;
	struct owl_st *st;
};

 
struct owl_pinmux_func {
	const char *name;
	const char * const *groups;
	unsigned int ngroups;
};

 
struct owl_gpio_port {
	unsigned int offset;
	unsigned int pins;
	unsigned int outen;
	unsigned int inen;
	unsigned int dat;
	unsigned int intc_ctl;
	unsigned int intc_pd;
	unsigned int intc_msk;
	unsigned int intc_type;
	u8 shared_ctl_offset;
};

 
struct owl_pinctrl_soc_data {
	const struct pinctrl_pin_desc *pins;
	unsigned int npins;
	const struct owl_pinmux_func *functions;
	unsigned int nfunctions;
	const struct owl_pingroup *groups;
	unsigned int ngroups;
	const struct owl_padinfo *padinfo;
	unsigned int ngpios;
	const struct owl_gpio_port *ports;
	unsigned int nports;
	int (*padctl_val2arg)(const struct owl_padinfo *padinfo,
				unsigned int param,
				u32 *arg);
	int (*padctl_arg2val)(const struct owl_padinfo *info,
				unsigned int param,
				u32 *arg);
};

int owl_pinctrl_probe(struct platform_device *pdev,
		struct owl_pinctrl_soc_data *soc_data);

#endif  
