#ifndef __PINMUX_TEGRA_H__
#define __PINMUX_TEGRA_H__
struct tegra_pmx {
	struct device *dev;
	struct pinctrl_dev *pctl;
	const struct tegra_pinctrl_soc_data *soc;
	struct tegra_function *functions;
	const char **group_pins;
	struct pinctrl_gpio_range gpio_range;
	struct pinctrl_desc desc;
	int nbanks;
	void __iomem **regs;
	u32 *backup_regs;
};
enum tegra_pinconf_param {
	TEGRA_PINCONF_PARAM_PULL,
	TEGRA_PINCONF_PARAM_TRISTATE,
	TEGRA_PINCONF_PARAM_ENABLE_INPUT,
	TEGRA_PINCONF_PARAM_OPEN_DRAIN,
	TEGRA_PINCONF_PARAM_LOCK,
	TEGRA_PINCONF_PARAM_IORESET,
	TEGRA_PINCONF_PARAM_RCV_SEL,
	TEGRA_PINCONF_PARAM_HIGH_SPEED_MODE,
	TEGRA_PINCONF_PARAM_SCHMITT,
	TEGRA_PINCONF_PARAM_LOW_POWER_MODE,
	TEGRA_PINCONF_PARAM_DRIVE_DOWN_STRENGTH,
	TEGRA_PINCONF_PARAM_DRIVE_UP_STRENGTH,
	TEGRA_PINCONF_PARAM_SLEW_RATE_FALLING,
	TEGRA_PINCONF_PARAM_SLEW_RATE_RISING,
	TEGRA_PINCONF_PARAM_DRIVE_TYPE,
};
enum tegra_pinconf_pull {
	TEGRA_PINCONFIG_PULL_NONE,
	TEGRA_PINCONFIG_PULL_DOWN,
	TEGRA_PINCONFIG_PULL_UP,
};
enum tegra_pinconf_tristate {
	TEGRA_PINCONFIG_DRIVEN,
	TEGRA_PINCONFIG_TRISTATE,
};
#define TEGRA_PINCONF_PACK(_param_, _arg_) ((_param_) << 16 | (_arg_))
#define TEGRA_PINCONF_UNPACK_PARAM(_conf_) ((_conf_) >> 16)
#define TEGRA_PINCONF_UNPACK_ARG(_conf_) ((_conf_) & 0xffff)
struct tegra_function {
	const char *name;
	const char **groups;
	unsigned ngroups;
};
struct tegra_pingroup {
	const char *name;
	const unsigned *pins;
	u8 npins;
	u8 funcs[4];
	s32 mux_reg;
	s32 pupd_reg;
	s32 tri_reg;
	s32 drv_reg;
	u32 mux_bank:2;
	u32 pupd_bank:2;
	u32 tri_bank:2;
	u32 drv_bank:2;
	s32 mux_bit:6;
	s32 pupd_bit:6;
	s32 tri_bit:6;
	s32 einput_bit:6;
	s32 odrain_bit:6;
	s32 lock_bit:6;
	s32 ioreset_bit:6;
	s32 rcv_sel_bit:6;
	s32 hsm_bit:6;
	s32 sfsel_bit:6;
	s32 schmitt_bit:6;
	s32 lpmd_bit:6;
	s32 drvdn_bit:6;
	s32 drvup_bit:6;
	s32 slwr_bit:6;
	s32 slwf_bit:6;
	s32 lpdr_bit:6;
	s32 drvtype_bit:6;
	s32 drvdn_width:6;
	s32 drvup_width:6;
	s32 slwr_width:6;
	s32 slwf_width:6;
	u32 parked_bitmask;
};
struct tegra_pinctrl_soc_data {
	unsigned ngpios;
	const char *gpio_compatible;
	const struct pinctrl_pin_desc *pins;
	unsigned npins;
	const char * const *functions;
	unsigned nfunctions;
	const struct tegra_pingroup *groups;
	unsigned ngroups;
	bool hsm_in_mux;
	bool schmitt_in_mux;
	bool drvtype_in_mux;
	bool sfsel_in_mux;
};
extern const struct dev_pm_ops tegra_pinctrl_pm;
int tegra_pinctrl_probe(struct platform_device *pdev,
			const struct tegra_pinctrl_soc_data *soc_data);
#endif
