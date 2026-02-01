 
 

#ifndef __PINCTRL_MVEBU_H__
#define __PINCTRL_MVEBU_H__

 
struct mvebu_mpp_ctrl_data {
	union {
		void __iomem *base;
		struct {
			struct regmap *map;
			u32 offset;
		} regmap;
	};
};

 
struct mvebu_mpp_ctrl {
	const char *name;
	u8 pid;
	u8 npins;
	unsigned *pins;
	int (*mpp_get)(struct mvebu_mpp_ctrl_data *data, unsigned pid,
		       unsigned long *config);
	int (*mpp_set)(struct mvebu_mpp_ctrl_data *data, unsigned pid,
		       unsigned long config);
	int (*mpp_gpio_req)(struct mvebu_mpp_ctrl_data *data, unsigned pid);
	int (*mpp_gpio_dir)(struct mvebu_mpp_ctrl_data *data, unsigned pid,
			    bool input);
};

 
struct mvebu_mpp_ctrl_setting {
	u8 val;
	const char *name;
	const char *subname;
	u8 variant;
	u8 flags;
#define  MVEBU_SETTING_GPO	(1 << 0)
#define  MVEBU_SETTING_GPI	(1 << 1)
};

 
struct mvebu_mpp_mode {
	u8 pid;
	struct mvebu_mpp_ctrl_setting *settings;
};

 
struct mvebu_pinctrl_soc_info {
	u8 variant;
	const struct mvebu_mpp_ctrl *controls;
	struct mvebu_mpp_ctrl_data *control_data;
	int ncontrols;
	struct mvebu_mpp_mode *modes;
	int nmodes;
	struct pinctrl_gpio_range *gpioranges;
	int ngpioranges;
};

#define MPP_FUNC_CTRL(_idl, _idh, _name, _func)			\
	{							\
		.name = _name,					\
		.pid = _idl,					\
		.npins = _idh - _idl + 1,			\
		.pins = (unsigned[_idh - _idl + 1]) { },	\
		.mpp_get = _func ## _get,			\
		.mpp_set = _func ## _set,			\
		.mpp_gpio_req = NULL,				\
		.mpp_gpio_dir = NULL,				\
	}

#define MPP_FUNC_GPIO_CTRL(_idl, _idh, _name, _func)		\
	{							\
		.name = _name,					\
		.pid = _idl,					\
		.npins = _idh - _idl + 1,			\
		.pins = (unsigned[_idh - _idl + 1]) { },	\
		.mpp_get = _func ## _get,			\
		.mpp_set = _func ## _set,			\
		.mpp_gpio_req = _func ## _gpio_req,		\
		.mpp_gpio_dir = _func ## _gpio_dir,		\
	}

#define _MPP_VAR_FUNCTION(_val, _name, _subname, _mask)		\
	{							\
		.val = _val,					\
		.name = _name,					\
		.subname = _subname,				\
		.variant = _mask,				\
		.flags = 0,					\
	}

#if defined(CONFIG_DEBUG_FS)
#define MPP_VAR_FUNCTION(_val, _name, _subname, _mask)		\
	_MPP_VAR_FUNCTION(_val, _name, _subname, _mask)
#else
#define MPP_VAR_FUNCTION(_val, _name, _subname, _mask)		\
	_MPP_VAR_FUNCTION(_val, _name, NULL, _mask)
#endif

#define MPP_FUNCTION(_val, _name, _subname)			\
	MPP_VAR_FUNCTION(_val, _name, _subname, (u8)-1)

#define MPP_MODE(_id, ...)					\
	{							\
		.pid = _id,					\
		.settings = (struct mvebu_mpp_ctrl_setting[]){	\
			__VA_ARGS__, { } },			\
	}

#define MPP_GPIO_RANGE(_id, _pinbase, _gpiobase, _npins)	\
	{							\
		.name = "mvebu-gpio",				\
		.id = _id,					\
		.pin_base = _pinbase,				\
		.base = _gpiobase,				\
		.npins = _npins,				\
	}

#define MVEBU_MPPS_PER_REG	8
#define MVEBU_MPP_BITS		4
#define MVEBU_MPP_MASK		0xf

int mvebu_mmio_mpp_ctrl_get(struct mvebu_mpp_ctrl_data *data, unsigned pid,
			       unsigned long *config);
int mvebu_mmio_mpp_ctrl_set(struct mvebu_mpp_ctrl_data *data, unsigned pid,
			       unsigned long config);
int mvebu_regmap_mpp_ctrl_get(struct mvebu_mpp_ctrl_data *data, unsigned pid,
			      unsigned long *config);
int mvebu_regmap_mpp_ctrl_set(struct mvebu_mpp_ctrl_data *data, unsigned pid,
			      unsigned long config);

int mvebu_pinctrl_probe(struct platform_device *pdev);
int mvebu_pinctrl_simple_mmio_probe(struct platform_device *pdev);
int mvebu_pinctrl_simple_regmap_probe(struct platform_device *pdev,
				      struct device *syscon_dev, u32 offset);

#endif
