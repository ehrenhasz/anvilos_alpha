


#ifndef __PINCTRL_SAMSUNG_H
#define __PINCTRL_SAMSUNG_H

#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/machine.h>

#include <linux/gpio/driver.h>


enum pincfg_type {
	PINCFG_TYPE_FUNC,
	PINCFG_TYPE_DAT,
	PINCFG_TYPE_PUD,
	PINCFG_TYPE_DRV,
	PINCFG_TYPE_CON_PDN,
	PINCFG_TYPE_PUD_PDN,

	PINCFG_TYPE_NUM
};


#define PINCFG_TYPE_MASK		0xFF
#define PINCFG_VALUE_SHIFT		8
#define PINCFG_VALUE_MASK		(0xFF << PINCFG_VALUE_SHIFT)
#define PINCFG_PACK(type, value)	(((value) << PINCFG_VALUE_SHIFT) | type)
#define PINCFG_UNPACK_TYPE(cfg)		((cfg) & PINCFG_TYPE_MASK)
#define PINCFG_UNPACK_VALUE(cfg)	(((cfg) & PINCFG_VALUE_MASK) >> \
						PINCFG_VALUE_SHIFT)

#define PIN_CON_FUNC_INPUT		0x0
#define PIN_CON_FUNC_OUTPUT		0x1


enum eint_type {
	EINT_TYPE_NONE,
	EINT_TYPE_GPIO,
	EINT_TYPE_WKUP,
	EINT_TYPE_WKUP_MUX,
};


#define PIN_NAME_LENGTH	10

#define PIN_GROUP(n, p, f)				\
	{						\
		.name		= n,			\
		.pins		= p,			\
		.num_pins	= ARRAY_SIZE(p),	\
		.func		= f			\
	}

#define PMX_FUNC(n, g)					\
	{						\
		.name		= n,			\
		.groups		= g,			\
		.num_groups	= ARRAY_SIZE(g),	\
	}

struct samsung_pinctrl_drv_data;


struct samsung_pin_bank_type {
	u8 fld_width[PINCFG_TYPE_NUM];
	u8 reg_offset[PINCFG_TYPE_NUM];
};


struct samsung_pin_bank_data {
	const struct samsung_pin_bank_type *type;
	u32		pctl_offset;
	u8		pctl_res_idx;
	u8		nr_pins;
	u8		eint_func;
	enum eint_type	eint_type;
	u32		eint_mask;
	u32		eint_offset;
	const char	*name;
};


struct samsung_pin_bank {
	const struct samsung_pin_bank_type *type;
	void __iomem	*pctl_base;
	u32		pctl_offset;
	u8		nr_pins;
	void __iomem	*eint_base;
	u8		eint_func;
	enum eint_type	eint_type;
	u32		eint_mask;
	u32		eint_offset;
	const char	*name;

	u32		pin_base;
	void		*soc_priv;
	struct fwnode_handle *fwnode;
	struct samsung_pinctrl_drv_data *drvdata;
	struct irq_domain *irq_domain;
	struct gpio_chip gpio_chip;
	struct pinctrl_gpio_range grange;
	struct exynos_irq_chip *irq_chip;
	raw_spinlock_t slock;

	u32 pm_save[PINCFG_TYPE_NUM + 1]; 
};


struct samsung_retention_ctrl {
	const u32	*regs;
	int		nr_regs;
	u32		value;
	atomic_t	*refcnt;
	void		*priv;
	void		(*enable)(struct samsung_pinctrl_drv_data *);
	void		(*disable)(struct samsung_pinctrl_drv_data *);
};


struct samsung_retention_data {
	const u32	*regs;
	int		nr_regs;
	u32		value;
	atomic_t	*refcnt;
	struct samsung_retention_ctrl *(*init)(struct samsung_pinctrl_drv_data *,
					const struct samsung_retention_data *);
};


struct samsung_pin_ctrl {
	const struct samsung_pin_bank_data *pin_banks;
	unsigned int	nr_banks;
	unsigned int	nr_ext_resources;
	const struct samsung_retention_data *retention_data;

	int		(*eint_gpio_init)(struct samsung_pinctrl_drv_data *);
	int		(*eint_wkup_init)(struct samsung_pinctrl_drv_data *);
	void		(*suspend)(struct samsung_pinctrl_drv_data *);
	void		(*resume)(struct samsung_pinctrl_drv_data *);
};


struct samsung_pinctrl_drv_data {
	struct list_head		node;
	void __iomem			*virt_base;
	struct device			*dev;
	int				irq;

	struct pinctrl_desc		pctl;
	struct pinctrl_dev		*pctl_dev;

	const struct samsung_pin_group	*pin_groups;
	unsigned int			nr_groups;
	const struct samsung_pmx_func	*pmx_functions;
	unsigned int			nr_functions;

	struct samsung_pin_bank		*pin_banks;
	unsigned int			nr_banks;
	unsigned int			pin_base;
	unsigned int			nr_pins;

	struct samsung_retention_ctrl	*retention_ctrl;

	void (*suspend)(struct samsung_pinctrl_drv_data *);
	void (*resume)(struct samsung_pinctrl_drv_data *);
};


struct samsung_pinctrl_of_match_data {
	const struct samsung_pin_ctrl	*ctrl;
	unsigned int			num_ctrl;
};


struct samsung_pin_group {
	const char		*name;
	const unsigned int	*pins;
	u8			num_pins;
	u8			func;
};


struct samsung_pmx_func {
	const char		*name;
	const char		**groups;
	u8			num_groups;
	u32			val;
};


extern const struct samsung_pinctrl_of_match_data exynos3250_of_data;
extern const struct samsung_pinctrl_of_match_data exynos4210_of_data;
extern const struct samsung_pinctrl_of_match_data exynos4x12_of_data;
extern const struct samsung_pinctrl_of_match_data exynos5250_of_data;
extern const struct samsung_pinctrl_of_match_data exynos5260_of_data;
extern const struct samsung_pinctrl_of_match_data exynos5410_of_data;
extern const struct samsung_pinctrl_of_match_data exynos5420_of_data;
extern const struct samsung_pinctrl_of_match_data exynos5433_of_data;
extern const struct samsung_pinctrl_of_match_data exynos7_of_data;
extern const struct samsung_pinctrl_of_match_data exynos7885_of_data;
extern const struct samsung_pinctrl_of_match_data exynos850_of_data;
extern const struct samsung_pinctrl_of_match_data exynosautov9_of_data;
extern const struct samsung_pinctrl_of_match_data fsd_of_data;
extern const struct samsung_pinctrl_of_match_data s3c64xx_of_data;
extern const struct samsung_pinctrl_of_match_data s3c2412_of_data;
extern const struct samsung_pinctrl_of_match_data s3c2416_of_data;
extern const struct samsung_pinctrl_of_match_data s3c2440_of_data;
extern const struct samsung_pinctrl_of_match_data s3c2450_of_data;
extern const struct samsung_pinctrl_of_match_data s5pv210_of_data;

#endif 
