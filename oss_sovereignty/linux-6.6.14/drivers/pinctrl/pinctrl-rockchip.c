
 

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/gpio/driver.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/string_helpers.h>

#include <dt-bindings/pinctrl/rockchip.h>

#include "core.h"
#include "pinconf.h"
#include "pinctrl-rockchip.h"

 
#define WRITE_MASK_VAL(h, l, v) \
	(GENMASK(((h) + 16), ((l) + 16)) | (((v) << (l)) & GENMASK((h), (l))))

 
#define IOMUX_GPIO_ONLY		BIT(0)
#define IOMUX_WIDTH_4BIT	BIT(1)
#define IOMUX_SOURCE_PMU	BIT(2)
#define IOMUX_UNROUTED		BIT(3)
#define IOMUX_WIDTH_3BIT	BIT(4)
#define IOMUX_WIDTH_2BIT	BIT(5)
#define IOMUX_L_SOURCE_PMU	BIT(6)

#define PIN_BANK(id, pins, label)			\
	{						\
		.bank_num	= id,			\
		.nr_pins	= pins,			\
		.name		= label,		\
		.iomux		= {			\
			{ .offset = -1 },		\
			{ .offset = -1 },		\
			{ .offset = -1 },		\
			{ .offset = -1 },		\
		},					\
	}

#define PIN_BANK_IOMUX_FLAGS(id, pins, label, iom0, iom1, iom2, iom3)	\
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .type = iom0, .offset = -1 },			\
			{ .type = iom1, .offset = -1 },			\
			{ .type = iom2, .offset = -1 },			\
			{ .type = iom3, .offset = -1 },			\
		},							\
	}

#define PIN_BANK_DRV_FLAGS(id, pins, label, type0, type1, type2, type3) \
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .offset = -1 },				\
			{ .offset = -1 },				\
			{ .offset = -1 },				\
			{ .offset = -1 },				\
		},							\
		.drv		= {					\
			{ .drv_type = type0, .offset = -1 },		\
			{ .drv_type = type1, .offset = -1 },		\
			{ .drv_type = type2, .offset = -1 },		\
			{ .drv_type = type3, .offset = -1 },		\
		},							\
	}

#define PIN_BANK_IOMUX_FLAGS_PULL_FLAGS(id, pins, label, iom0, iom1,	\
					iom2, iom3, pull0, pull1,	\
					pull2, pull3)			\
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .type = iom0, .offset = -1 },			\
			{ .type = iom1, .offset = -1 },			\
			{ .type = iom2, .offset = -1 },			\
			{ .type = iom3, .offset = -1 },			\
		},							\
		.pull_type[0] = pull0,					\
		.pull_type[1] = pull1,					\
		.pull_type[2] = pull2,					\
		.pull_type[3] = pull3,					\
	}

#define PIN_BANK_DRV_FLAGS_PULL_FLAGS(id, pins, label, drv0, drv1,	\
				      drv2, drv3, pull0, pull1,		\
				      pull2, pull3)			\
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .offset = -1 },				\
			{ .offset = -1 },				\
			{ .offset = -1 },				\
			{ .offset = -1 },				\
		},							\
		.drv		= {					\
			{ .drv_type = drv0, .offset = -1 },		\
			{ .drv_type = drv1, .offset = -1 },		\
			{ .drv_type = drv2, .offset = -1 },		\
			{ .drv_type = drv3, .offset = -1 },		\
		},							\
		.pull_type[0] = pull0,					\
		.pull_type[1] = pull1,					\
		.pull_type[2] = pull2,					\
		.pull_type[3] = pull3,					\
	}

#define PIN_BANK_IOMUX_FLAGS_OFFSET(id, pins, label, iom0, iom1, iom2,	\
				    iom3, offset0, offset1, offset2,	\
				    offset3)				\
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .type = iom0, .offset = offset0 },		\
			{ .type = iom1, .offset = offset1 },		\
			{ .type = iom2, .offset = offset2 },		\
			{ .type = iom3, .offset = offset3 },		\
		},							\
	}

#define PIN_BANK_IOMUX_DRV_FLAGS_OFFSET(id, pins, label, iom0, iom1,	\
					iom2, iom3, drv0, drv1, drv2,	\
					drv3, offset0, offset1,		\
					offset2, offset3)		\
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .type = iom0, .offset = -1 },			\
			{ .type = iom1, .offset = -1 },			\
			{ .type = iom2, .offset = -1 },			\
			{ .type = iom3, .offset = -1 },			\
		},							\
		.drv		= {					\
			{ .drv_type = drv0, .offset = offset0 },	\
			{ .drv_type = drv1, .offset = offset1 },	\
			{ .drv_type = drv2, .offset = offset2 },	\
			{ .drv_type = drv3, .offset = offset3 },	\
		},							\
	}

#define PIN_BANK_IOMUX_FLAGS_DRV_FLAGS_OFFSET_PULL_FLAGS(id, pins,	\
					      label, iom0, iom1, iom2,  \
					      iom3, drv0, drv1, drv2,   \
					      drv3, offset0, offset1,   \
					      offset2, offset3, pull0,  \
					      pull1, pull2, pull3)	\
	{								\
		.bank_num	= id,					\
		.nr_pins	= pins,					\
		.name		= label,				\
		.iomux		= {					\
			{ .type = iom0, .offset = -1 },			\
			{ .type = iom1, .offset = -1 },			\
			{ .type = iom2, .offset = -1 },			\
			{ .type = iom3, .offset = -1 },			\
		},							\
		.drv		= {					\
			{ .drv_type = drv0, .offset = offset0 },	\
			{ .drv_type = drv1, .offset = offset1 },	\
			{ .drv_type = drv2, .offset = offset2 },	\
			{ .drv_type = drv3, .offset = offset3 },	\
		},							\
		.pull_type[0] = pull0,					\
		.pull_type[1] = pull1,					\
		.pull_type[2] = pull2,					\
		.pull_type[3] = pull3,					\
	}

#define PIN_BANK_MUX_ROUTE_FLAGS(ID, PIN, FUNC, REG, VAL, FLAG)		\
	{								\
		.bank_num	= ID,					\
		.pin		= PIN,					\
		.func		= FUNC,					\
		.route_offset	= REG,					\
		.route_val	= VAL,					\
		.route_location	= FLAG,					\
	}

#define RK_MUXROUTE_SAME(ID, PIN, FUNC, REG, VAL)	\
	PIN_BANK_MUX_ROUTE_FLAGS(ID, PIN, FUNC, REG, VAL, ROCKCHIP_ROUTE_SAME)

#define RK_MUXROUTE_GRF(ID, PIN, FUNC, REG, VAL)	\
	PIN_BANK_MUX_ROUTE_FLAGS(ID, PIN, FUNC, REG, VAL, ROCKCHIP_ROUTE_GRF)

#define RK_MUXROUTE_PMU(ID, PIN, FUNC, REG, VAL)	\
	PIN_BANK_MUX_ROUTE_FLAGS(ID, PIN, FUNC, REG, VAL, ROCKCHIP_ROUTE_PMU)

#define RK3588_PIN_BANK_FLAGS(ID, PIN, LABEL, M, P)			\
	PIN_BANK_IOMUX_FLAGS_PULL_FLAGS(ID, PIN, LABEL, M, M, M, M, P, P, P, P)

static struct regmap_config rockchip_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static inline const struct rockchip_pin_group *pinctrl_name_to_group(
					const struct rockchip_pinctrl *info,
					const char *name)
{
	int i;

	for (i = 0; i < info->ngroups; i++) {
		if (!strcmp(info->groups[i].name, name))
			return &info->groups[i];
	}

	return NULL;
}

 
static struct rockchip_pin_bank *pin_to_bank(struct rockchip_pinctrl *info,
								unsigned pin)
{
	struct rockchip_pin_bank *b = info->ctrl->pin_banks;

	while (pin >= (b->pin_base + b->nr_pins))
		b++;

	return b;
}

static struct rockchip_pin_bank *bank_num_to_bank(
					struct rockchip_pinctrl *info,
					unsigned num)
{
	struct rockchip_pin_bank *b = info->ctrl->pin_banks;
	int i;

	for (i = 0; i < info->ctrl->nr_banks; i++, b++) {
		if (b->bank_num == num)
			return b;
	}

	return ERR_PTR(-EINVAL);
}

 

static int rockchip_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct rockchip_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	return info->ngroups;
}

static const char *rockchip_get_group_name(struct pinctrl_dev *pctldev,
							unsigned selector)
{
	struct rockchip_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	return info->groups[selector].name;
}

static int rockchip_get_group_pins(struct pinctrl_dev *pctldev,
				      unsigned selector, const unsigned **pins,
				      unsigned *npins)
{
	struct rockchip_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	if (selector >= info->ngroups)
		return -EINVAL;

	*pins = info->groups[selector].pins;
	*npins = info->groups[selector].npins;

	return 0;
}

static int rockchip_dt_node_to_map(struct pinctrl_dev *pctldev,
				 struct device_node *np,
				 struct pinctrl_map **map, unsigned *num_maps)
{
	struct rockchip_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);
	const struct rockchip_pin_group *grp;
	struct device *dev = info->dev;
	struct pinctrl_map *new_map;
	struct device_node *parent;
	int map_num = 1;
	int i;

	 
	grp = pinctrl_name_to_group(info, np->name);
	if (!grp) {
		dev_err(dev, "unable to find group for node %pOFn\n", np);
		return -EINVAL;
	}

	map_num += grp->npins;

	new_map = kcalloc(map_num, sizeof(*new_map), GFP_KERNEL);
	if (!new_map)
		return -ENOMEM;

	*map = new_map;
	*num_maps = map_num;

	 
	parent = of_get_parent(np);
	if (!parent) {
		kfree(new_map);
		return -EINVAL;
	}
	new_map[0].type = PIN_MAP_TYPE_MUX_GROUP;
	new_map[0].data.mux.function = parent->name;
	new_map[0].data.mux.group = np->name;
	of_node_put(parent);

	 
	new_map++;
	for (i = 0; i < grp->npins; i++) {
		new_map[i].type = PIN_MAP_TYPE_CONFIGS_PIN;
		new_map[i].data.configs.group_or_pin =
				pin_get_name(pctldev, grp->pins[i]);
		new_map[i].data.configs.configs = grp->data[i].configs;
		new_map[i].data.configs.num_configs = grp->data[i].nconfigs;
	}

	dev_dbg(dev, "maps: function %s group %s num %d\n",
		(*map)->data.mux.function, (*map)->data.mux.group, map_num);

	return 0;
}

static void rockchip_dt_free_map(struct pinctrl_dev *pctldev,
				    struct pinctrl_map *map, unsigned num_maps)
{
	kfree(map);
}

static const struct pinctrl_ops rockchip_pctrl_ops = {
	.get_groups_count	= rockchip_get_groups_count,
	.get_group_name		= rockchip_get_group_name,
	.get_group_pins		= rockchip_get_group_pins,
	.dt_node_to_map		= rockchip_dt_node_to_map,
	.dt_free_map		= rockchip_dt_free_map,
};

 

static struct rockchip_mux_recalced_data rv1108_mux_recalced_data[] = {
	{
		.num = 1,
		.pin = 0,
		.reg = 0x418,
		.bit = 0,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 1,
		.reg = 0x418,
		.bit = 2,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 2,
		.reg = 0x418,
		.bit = 4,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 3,
		.reg = 0x418,
		.bit = 6,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 4,
		.reg = 0x418,
		.bit = 8,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 5,
		.reg = 0x418,
		.bit = 10,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 6,
		.reg = 0x418,
		.bit = 12,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 7,
		.reg = 0x418,
		.bit = 14,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 8,
		.reg = 0x41c,
		.bit = 0,
		.mask = 0x3
	}, {
		.num = 1,
		.pin = 9,
		.reg = 0x41c,
		.bit = 2,
		.mask = 0x3
	},
};

static struct rockchip_mux_recalced_data rv1126_mux_recalced_data[] = {
	{
		.num = 0,
		.pin = 20,
		.reg = 0x10000,
		.bit = 0,
		.mask = 0xf
	},
	{
		.num = 0,
		.pin = 21,
		.reg = 0x10000,
		.bit = 4,
		.mask = 0xf
	},
	{
		.num = 0,
		.pin = 22,
		.reg = 0x10000,
		.bit = 8,
		.mask = 0xf
	},
	{
		.num = 0,
		.pin = 23,
		.reg = 0x10000,
		.bit = 12,
		.mask = 0xf
	},
};

static  struct rockchip_mux_recalced_data rk3128_mux_recalced_data[] = {
	{
		.num = 2,
		.pin = 20,
		.reg = 0xe8,
		.bit = 0,
		.mask = 0x7
	}, {
		.num = 2,
		.pin = 21,
		.reg = 0xe8,
		.bit = 4,
		.mask = 0x7
	}, {
		.num = 2,
		.pin = 22,
		.reg = 0xe8,
		.bit = 8,
		.mask = 0x7
	}, {
		.num = 2,
		.pin = 23,
		.reg = 0xe8,
		.bit = 12,
		.mask = 0x7
	}, {
		.num = 2,
		.pin = 24,
		.reg = 0xd4,
		.bit = 12,
		.mask = 0x7
	},
};

static struct rockchip_mux_recalced_data rk3308_mux_recalced_data[] = {
	{
		 
		.num = 1,
		.pin = 14,
		.reg = 0x28,
		.bit = 12,
		.mask = 0xf
	}, {
		 
		.num = 1,
		.pin = 15,
		.reg = 0x2c,
		.bit = 0,
		.mask = 0x3
	}, {
		 
		.num = 1,
		.pin = 18,
		.reg = 0x30,
		.bit = 4,
		.mask = 0xf
	}, {
		 
		.num = 1,
		.pin = 19,
		.reg = 0x30,
		.bit = 8,
		.mask = 0xf
	}, {
		 
		.num = 1,
		.pin = 20,
		.reg = 0x30,
		.bit = 12,
		.mask = 0xf
	}, {
		 
		.num = 1,
		.pin = 21,
		.reg = 0x34,
		.bit = 0,
		.mask = 0xf
	}, {
		 
		.num = 1,
		.pin = 22,
		.reg = 0x34,
		.bit = 4,
		.mask = 0xf
	}, {
		 
		.num = 1,
		.pin = 23,
		.reg = 0x34,
		.bit = 8,
		.mask = 0xf
	}, {
		 
		.num = 2,
		.pin = 2,
		.reg = 0x40,
		.bit = 4,
		.mask = 0x3
	}, {
		 
		.num = 2,
		.pin = 3,
		.reg = 0x40,
		.bit = 6,
		.mask = 0x3
	}, {
		 
		.num = 2,
		.pin = 16,
		.reg = 0x50,
		.bit = 0,
		.mask = 0x3
	}, {
		 
		.num = 3,
		.pin = 10,
		.reg = 0x68,
		.bit = 4,
		.mask = 0x3
	}, {
		 
		.num = 3,
		.pin = 11,
		.reg = 0x68,
		.bit = 6,
		.mask = 0x3
	}, {
		 
		.num = 3,
		.pin = 12,
		.reg = 0x68,
		.bit = 8,
		.mask = 0xf
	}, {
		 
		.num = 3,
		.pin = 13,
		.reg = 0x68,
		.bit = 12,
		.mask = 0xf
	},
};

static struct rockchip_mux_recalced_data rk3328_mux_recalced_data[] = {
	{
		.num = 2,
		.pin = 12,
		.reg = 0x24,
		.bit = 8,
		.mask = 0x3
	}, {
		.num = 2,
		.pin = 15,
		.reg = 0x28,
		.bit = 0,
		.mask = 0x7
	}, {
		.num = 2,
		.pin = 23,
		.reg = 0x30,
		.bit = 14,
		.mask = 0x3
	},
};

static void rockchip_get_recalced_mux(struct rockchip_pin_bank *bank, int pin,
				      int *reg, u8 *bit, int *mask)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	struct rockchip_mux_recalced_data *data;
	int i;

	for (i = 0; i < ctrl->niomux_recalced; i++) {
		data = &ctrl->iomux_recalced[i];
		if (data->num == bank->bank_num &&
		    data->pin == pin)
			break;
	}

	if (i >= ctrl->niomux_recalced)
		return;

	*reg = data->reg;
	*mask = data->mask;
	*bit = data->bit;
}

static struct rockchip_mux_route_data px30_mux_route_data[] = {
	RK_MUXROUTE_SAME(2, RK_PB4, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PA1, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PB6, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PA2, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PA0, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PA3, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PA1, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PA5, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PA2, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PA7, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PA3, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PB0, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PA4, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PB1, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PA5, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PB4, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PA6, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PB6, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PA7, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PB7, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PB7, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PC6, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PC0, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PC7, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PB0, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PD1, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PB1, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PD2, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PB2, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PD3, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PB3, 1, 0x184, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(3, RK_PD0, 3, 0x184, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(3, RK_PC6, 2, 0x184, BIT(16 + 8)),  
	RK_MUXROUTE_SAME(2, RK_PC6, 1, 0x184, BIT(16 + 8) | BIT(8)),  
	RK_MUXROUTE_SAME(3, RK_PD3, 2, 0x184, BIT(16 + 8)),  
	RK_MUXROUTE_SAME(2, RK_PC5, 2, 0x184, BIT(16 + 8) | BIT(8)),  
	RK_MUXROUTE_SAME(1, RK_PD3, 2, 0x184, BIT(16 + 10)),  
	RK_MUXROUTE_SAME(2, RK_PB6, 2, 0x184, BIT(16 + 10) | BIT(10)),  
	RK_MUXROUTE_SAME(1, RK_PD2, 2, 0x184, BIT(16 + 10)),  
	RK_MUXROUTE_SAME(2, RK_PB4, 2, 0x184, BIT(16 + 10) | BIT(10)),  
	RK_MUXROUTE_SAME(0, RK_PC1, 2, 0x184, BIT(16 + 9)),  
	RK_MUXROUTE_SAME(1, RK_PB7, 2, 0x184, BIT(16 + 9) | BIT(9)),  
	RK_MUXROUTE_SAME(0, RK_PC0, 2, 0x184, BIT(16 + 9)),  
	RK_MUXROUTE_SAME(1, RK_PB6, 2, 0x184, BIT(16 + 9) | BIT(9)),  
	RK_MUXROUTE_SAME(0, RK_PC2, 2, 0x184, BIT(16 + 9)),  
	RK_MUXROUTE_SAME(1, RK_PB4, 2, 0x184, BIT(16 + 9) | BIT(9)),  
	RK_MUXROUTE_SAME(0, RK_PC3, 2, 0x184, BIT(16 + 9)),  
	RK_MUXROUTE_SAME(1, RK_PB5, 2, 0x184, BIT(16 + 9) | BIT(9)),  
};

static struct rockchip_mux_route_data rv1126_mux_route_data[] = {
	RK_MUXROUTE_GRF(3, RK_PD2, 1, 0x10260, WRITE_MASK_VAL(0, 0, 0)),  
	RK_MUXROUTE_GRF(3, RK_PB0, 3, 0x10260, WRITE_MASK_VAL(0, 0, 1)),  

	RK_MUXROUTE_GRF(0, RK_PD4, 4, 0x10260, WRITE_MASK_VAL(3, 2, 0)),  
	RK_MUXROUTE_GRF(1, RK_PD5, 2, 0x10260, WRITE_MASK_VAL(3, 2, 1)),  
	RK_MUXROUTE_GRF(2, RK_PC7, 6, 0x10260, WRITE_MASK_VAL(3, 2, 2)),  

	RK_MUXROUTE_GRF(1, RK_PD0, 1, 0x10260, WRITE_MASK_VAL(4, 4, 0)),  
	RK_MUXROUTE_GRF(2, RK_PB3, 2, 0x10260, WRITE_MASK_VAL(4, 4, 1)),  

	RK_MUXROUTE_GRF(3, RK_PD4, 2, 0x10260, WRITE_MASK_VAL(12, 12, 0)),  
	RK_MUXROUTE_GRF(3, RK_PC0, 3, 0x10260, WRITE_MASK_VAL(12, 12, 1)),  

	RK_MUXROUTE_GRF(3, RK_PC6, 1, 0x10264, WRITE_MASK_VAL(0, 0, 0)),  
	RK_MUXROUTE_GRF(2, RK_PD1, 3, 0x10264, WRITE_MASK_VAL(0, 0, 1)),  

	RK_MUXROUTE_GRF(3, RK_PA4, 5, 0x10264, WRITE_MASK_VAL(5, 4, 0)),  
	RK_MUXROUTE_GRF(2, RK_PD4, 7, 0x10264, WRITE_MASK_VAL(5, 4, 1)),  
	RK_MUXROUTE_GRF(1, RK_PD6, 3, 0x10264, WRITE_MASK_VAL(5, 4, 2)),  

	RK_MUXROUTE_GRF(3, RK_PA0, 7, 0x10264, WRITE_MASK_VAL(6, 6, 0)),  
	RK_MUXROUTE_GRF(4, RK_PA0, 4, 0x10264, WRITE_MASK_VAL(6, 6, 1)),  

	RK_MUXROUTE_GRF(2, RK_PA5, 7, 0x10264, WRITE_MASK_VAL(9, 8, 0)),  
	RK_MUXROUTE_GRF(3, RK_PB0, 5, 0x10264, WRITE_MASK_VAL(9, 8, 1)),  
	RK_MUXROUTE_GRF(1, RK_PD0, 4, 0x10264, WRITE_MASK_VAL(9, 8, 2)),  

	RK_MUXROUTE_GRF(3, RK_PC0, 5, 0x10264, WRITE_MASK_VAL(11, 10, 0)),  
	RK_MUXROUTE_GRF(1, RK_PC6, 3, 0x10264, WRITE_MASK_VAL(11, 10, 1)),  
	RK_MUXROUTE_GRF(2, RK_PD5, 6, 0x10264, WRITE_MASK_VAL(11, 10, 2)),  

	RK_MUXROUTE_GRF(3, RK_PC0, 2, 0x10264, WRITE_MASK_VAL(12, 12, 0)),  
	RK_MUXROUTE_GRF(2, RK_PB7, 2, 0x10264, WRITE_MASK_VAL(12, 12, 1)),  

	RK_MUXROUTE_GRF(3, RK_PA1, 3, 0x10264, WRITE_MASK_VAL(13, 13, 0)),  
	RK_MUXROUTE_GRF(3, RK_PA7, 5, 0x10264, WRITE_MASK_VAL(13, 13, 1)),  

	RK_MUXROUTE_GRF(3, RK_PA4, 6, 0x10268, WRITE_MASK_VAL(0, 0, 0)),  
	RK_MUXROUTE_GRF(2, RK_PD7, 5, 0x10268, WRITE_MASK_VAL(0, 0, 1)),  

	RK_MUXROUTE_GRF(3, RK_PA5, 6, 0x10268, WRITE_MASK_VAL(2, 2, 0)),  
	RK_MUXROUTE_GRF(2, RK_PD6, 5, 0x10268, WRITE_MASK_VAL(2, 2, 1)),  

	RK_MUXROUTE_GRF(3, RK_PA6, 6, 0x10268, WRITE_MASK_VAL(4, 4, 0)),  
	RK_MUXROUTE_GRF(2, RK_PD5, 5, 0x10268, WRITE_MASK_VAL(4, 4, 1)),  

	RK_MUXROUTE_GRF(3, RK_PA7, 6, 0x10268, WRITE_MASK_VAL(6, 6, 0)),  
	RK_MUXROUTE_GRF(3, RK_PA1, 5, 0x10268, WRITE_MASK_VAL(6, 6, 1)),  

	RK_MUXROUTE_GRF(1, RK_PA5, 3, 0x10268, WRITE_MASK_VAL(8, 8, 0)),  
	RK_MUXROUTE_GRF(3, RK_PA2, 1, 0x10268, WRITE_MASK_VAL(8, 8, 1)),  

	RK_MUXROUTE_GRF(3, RK_PC6, 3, 0x10268, WRITE_MASK_VAL(11, 10, 0)),  
	RK_MUXROUTE_GRF(1, RK_PA7, 2, 0x10268, WRITE_MASK_VAL(11, 10, 1)),  
	RK_MUXROUTE_GRF(3, RK_PA0, 4, 0x10268, WRITE_MASK_VAL(11, 10, 2)),  

	RK_MUXROUTE_GRF(3, RK_PA4, 4, 0x10268, WRITE_MASK_VAL(13, 12, 0)),  
	RK_MUXROUTE_GRF(2, RK_PA6, 4, 0x10268, WRITE_MASK_VAL(13, 12, 1)),  
	RK_MUXROUTE_GRF(1, RK_PD5, 3, 0x10268, WRITE_MASK_VAL(13, 12, 2)),  

	RK_MUXROUTE_GRF(3, RK_PA6, 4, 0x10268, WRITE_MASK_VAL(15, 14, 0)),  
	RK_MUXROUTE_GRF(2, RK_PB0, 4, 0x10268, WRITE_MASK_VAL(15, 14, 1)),  
	RK_MUXROUTE_GRF(2, RK_PA0, 3, 0x10268, WRITE_MASK_VAL(15, 14, 2)),  

	RK_MUXROUTE_PMU(0, RK_PB6, 3, 0x0114, WRITE_MASK_VAL(0, 0, 0)),  
	RK_MUXROUTE_PMU(2, RK_PB3, 5, 0x0114, WRITE_MASK_VAL(0, 0, 1)),  

	RK_MUXROUTE_PMU(0, RK_PB7, 3, 0x0114, WRITE_MASK_VAL(2, 2, 0)),  
	RK_MUXROUTE_PMU(2, RK_PB2, 5, 0x0114, WRITE_MASK_VAL(2, 2, 1)),  

	RK_MUXROUTE_PMU(0, RK_PC0, 3, 0x0114, WRITE_MASK_VAL(4, 4, 0)),  
	RK_MUXROUTE_PMU(2, RK_PB1, 5, 0x0114, WRITE_MASK_VAL(4, 4, 1)),  

	RK_MUXROUTE_PMU(0, RK_PC1, 3, 0x0114, WRITE_MASK_VAL(6, 6, 0)),  
	RK_MUXROUTE_PMU(2, RK_PB0, 5, 0x0114, WRITE_MASK_VAL(6, 6, 1)),  

	RK_MUXROUTE_PMU(0, RK_PC2, 3, 0x0114, WRITE_MASK_VAL(8, 8, 0)),  
	RK_MUXROUTE_PMU(2, RK_PA7, 5, 0x0114, WRITE_MASK_VAL(8, 8, 1)),  

	RK_MUXROUTE_PMU(0, RK_PC3, 3, 0x0114, WRITE_MASK_VAL(10, 10, 0)),  
	RK_MUXROUTE_PMU(2, RK_PA6, 5, 0x0114, WRITE_MASK_VAL(10, 10, 1)),  

	RK_MUXROUTE_PMU(0, RK_PB2, 3, 0x0114, WRITE_MASK_VAL(12, 12, 0)),  
	RK_MUXROUTE_PMU(2, RK_PD4, 5, 0x0114, WRITE_MASK_VAL(12, 12, 1)),  

	RK_MUXROUTE_PMU(0, RK_PB1, 3, 0x0114, WRITE_MASK_VAL(14, 14, 0)),  
	RK_MUXROUTE_PMU(3, RK_PA0, 5, 0x0114, WRITE_MASK_VAL(14, 14, 1)),  

	RK_MUXROUTE_PMU(0, RK_PB0, 1, 0x0118, WRITE_MASK_VAL(1, 0, 0)),  
	RK_MUXROUTE_PMU(2, RK_PA1, 1, 0x0118, WRITE_MASK_VAL(1, 0, 1)),  
	RK_MUXROUTE_PMU(2, RK_PB2, 6, 0x0118, WRITE_MASK_VAL(1, 0, 2)),  

	RK_MUXROUTE_PMU(0, RK_PB6, 2, 0x0118, WRITE_MASK_VAL(2, 2, 0)),  
	RK_MUXROUTE_PMU(1, RK_PD0, 5, 0x0118, WRITE_MASK_VAL(2, 2, 1)),  
};

static struct rockchip_mux_route_data rk3128_mux_route_data[] = {
	RK_MUXROUTE_SAME(1, RK_PB2, 1, 0x144, BIT(16 + 3) | BIT(16 + 4)),  
	RK_MUXROUTE_SAME(1, RK_PD3, 3, 0x144, BIT(16 + 3) | BIT(16 + 4) | BIT(3)),  
	RK_MUXROUTE_SAME(0, RK_PB5, 2, 0x144, BIT(16 + 3) | BIT(16 + 4) | BIT(4)),  
	RK_MUXROUTE_SAME(1, RK_PA5, 1, 0x144, BIT(16 + 5)),  
	RK_MUXROUTE_SAME(0, RK_PB6, 1, 0x144, BIT(16 + 5) | BIT(5)),  
	RK_MUXROUTE_SAME(1, RK_PC6, 2, 0x144, BIT(16 + 6)),  
	RK_MUXROUTE_SAME(2, RK_PA4, 2, 0x144, BIT(16 + 6) | BIT(6)),  
};

static struct rockchip_mux_route_data rk3188_mux_route_data[] = {
	RK_MUXROUTE_SAME(0, RK_PD0, 1, 0xa0, BIT(16 + 11)),  
	RK_MUXROUTE_SAME(0, RK_PD0, 2, 0xa0, BIT(16 + 11) | BIT(11)),  
};

static struct rockchip_mux_route_data rk3228_mux_route_data[] = {
	RK_MUXROUTE_SAME(0, RK_PD2, 1, 0x50, BIT(16)),  
	RK_MUXROUTE_SAME(3, RK_PC5, 1, 0x50, BIT(16) | BIT(0)),  
	RK_MUXROUTE_SAME(0, RK_PD3, 1, 0x50, BIT(16 + 1)),  
	RK_MUXROUTE_SAME(0, RK_PD6, 2, 0x50, BIT(16 + 1) | BIT(1)),  
	RK_MUXROUTE_SAME(0, RK_PD4, 1, 0x50, BIT(16 + 2)),  
	RK_MUXROUTE_SAME(1, RK_PB4, 2, 0x50, BIT(16 + 2) | BIT(2)),  
	RK_MUXROUTE_SAME(3, RK_PD2, 1, 0x50, BIT(16 + 3)),  
	RK_MUXROUTE_SAME(1, RK_PB3, 2, 0x50, BIT(16 + 3) | BIT(3)),  
	RK_MUXROUTE_SAME(1, RK_PA1, 1, 0x50, BIT(16 + 4)),  
	RK_MUXROUTE_SAME(3, RK_PA2, 1, 0x50, BIT(16 + 4) | BIT(4)),  
	RK_MUXROUTE_SAME(0, RK_PB5, 2, 0x50, BIT(16 + 5)),  
	RK_MUXROUTE_SAME(2, RK_PA0, 2, 0x50, BIT(16 + 5) | BIT(5)),  
	RK_MUXROUTE_SAME(1, RK_PC6, 2, 0x50, BIT(16 + 7)),  
	RK_MUXROUTE_SAME(2, RK_PA4, 2, 0x50, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(1, RK_PC3, 2, 0x50, BIT(16 + 8)),  
	RK_MUXROUTE_SAME(1, RK_PB2, 2, 0x50, BIT(16 + 8) | BIT(8)),  
	RK_MUXROUTE_SAME(1, RK_PB2, 1, 0x50, BIT(16 + 11)),  
	RK_MUXROUTE_SAME(3, RK_PB5, 1, 0x50, BIT(16 + 11) | BIT(11)),  
};

static struct rockchip_mux_route_data rk3288_mux_route_data[] = {
	RK_MUXROUTE_SAME(7, RK_PC0, 2, 0x264, BIT(16 + 12) | BIT(12)),  
	RK_MUXROUTE_SAME(7, RK_PC7, 4, 0x264, BIT(16 + 12)),  
};

static struct rockchip_mux_route_data rk3308_mux_route_data[] = {
	RK_MUXROUTE_SAME(0, RK_PC3, 1, 0x314, BIT(16 + 0) | BIT(0)),  
	RK_MUXROUTE_SAME(1, RK_PC6, 2, 0x314, BIT(16 + 2) | BIT(16 + 3)),  
	RK_MUXROUTE_SAME(4, RK_PD2, 2, 0x314, BIT(16 + 2) | BIT(16 + 3) | BIT(2)),  
	RK_MUXROUTE_SAME(0, RK_PB7, 2, 0x608, BIT(16 + 8) | BIT(16 + 9)),  
	RK_MUXROUTE_SAME(3, RK_PB4, 2, 0x608, BIT(16 + 8) | BIT(16 + 9) | BIT(8)),  
	RK_MUXROUTE_SAME(2, RK_PA0, 3, 0x608, BIT(16 + 8) | BIT(16 + 9) | BIT(9)),  
	RK_MUXROUTE_SAME(1, RK_PA3, 2, 0x308, BIT(16 + 3)),  
	RK_MUXROUTE_SAME(1, RK_PA4, 2, 0x308, BIT(16 + 3)),  
	RK_MUXROUTE_SAME(1, RK_PB5, 2, 0x308, BIT(16 + 3) | BIT(3)),  
	RK_MUXROUTE_SAME(1, RK_PB6, 2, 0x308, BIT(16 + 3) | BIT(3)),  
	RK_MUXROUTE_SAME(1, RK_PA4, 3, 0x308, BIT(16 + 12) | BIT(16 + 13)),  
	RK_MUXROUTE_SAME(1, RK_PB6, 4, 0x308, BIT(16 + 12) | BIT(16 + 13) | BIT(12)),  
	RK_MUXROUTE_SAME(2, RK_PA6, 2, 0x308, BIT(16 + 12) | BIT(16 + 13) | BIT(13)),  
	RK_MUXROUTE_SAME(2, RK_PA4, 3, 0x600, BIT(16 + 2) | BIT(2)),  
	RK_MUXROUTE_SAME(3, RK_PB2, 3, 0x314, BIT(16 + 9)),  
	RK_MUXROUTE_SAME(2, RK_PA4, 2, 0x314, BIT(16 + 9) | BIT(9)),  
	RK_MUXROUTE_SAME(0, RK_PB3, 3, 0x314, BIT(16 + 10) | BIT(16 + 11)),  
	RK_MUXROUTE_SAME(1, RK_PC6, 7, 0x314, BIT(16 + 10) | BIT(16 + 11) | BIT(10)),  
	RK_MUXROUTE_SAME(2, RK_PA2, 5, 0x314, BIT(16 + 10) | BIT(16 + 11) | BIT(11)),  
	RK_MUXROUTE_SAME(0, RK_PB3, 2, 0x314, BIT(16 + 12) | BIT(16 + 13)),  
	RK_MUXROUTE_SAME(1, RK_PC6, 5, 0x314, BIT(16 + 12) | BIT(16 + 13) | BIT(12)),  
	RK_MUXROUTE_SAME(2, RK_PA2, 4, 0x314, BIT(16 + 12) | BIT(16 + 13) | BIT(13)),  
	RK_MUXROUTE_SAME(1, RK_PC4, 3, 0x314, BIT(16 + 14)),  
	RK_MUXROUTE_SAME(4, RK_PA2, 2, 0x314, BIT(16 + 14) | BIT(14)),  
	RK_MUXROUTE_SAME(3, RK_PB4, 4, 0x314, BIT(16 + 15)),  
	RK_MUXROUTE_SAME(0, RK_PC1, 3, 0x314, BIT(16 + 15) | BIT(15)),  
};

static struct rockchip_mux_route_data rk3328_mux_route_data[] = {
	RK_MUXROUTE_SAME(1, RK_PA1, 2, 0x50, BIT(16) | BIT(16 + 1)),  
	RK_MUXROUTE_SAME(2, RK_PA1, 1, 0x50, BIT(16) | BIT(16 + 1) | BIT(0)),  
	RK_MUXROUTE_SAME(1, RK_PB3, 2, 0x50, BIT(16 + 2) | BIT(2)),  
	RK_MUXROUTE_SAME(1, RK_PB6, 2, 0x50, BIT(16 + 10) | BIT(10)),  
	RK_MUXROUTE_SAME(2, RK_PC3, 2, 0x50, BIT(16 + 3)),  
	RK_MUXROUTE_SAME(1, RK_PC7, 3, 0x50, BIT(16 + 3) | BIT(3)),  
	RK_MUXROUTE_SAME(3, RK_PA2, 4, 0x50, BIT(16 + 4) | BIT(16 + 5) | BIT(5)),  
	RK_MUXROUTE_SAME(1, RK_PD0, 1, 0x50, BIT(16 + 6)),  
	RK_MUXROUTE_SAME(3, RK_PA2, 6, 0x50, BIT(16 + 6) | BIT(6)),  
	RK_MUXROUTE_SAME(2, RK_PC6, 3, 0x50, BIT(16 + 7) | BIT(7)),  
	RK_MUXROUTE_SAME(2, RK_PC0, 3, 0x50, BIT(16 + 8) | BIT(8)),  
	RK_MUXROUTE_SAME(2, RK_PC0, 4, 0x50, BIT(16 + 9) | BIT(9)),  
};

static struct rockchip_mux_route_data rk3399_mux_route_data[] = {
	RK_MUXROUTE_SAME(4, RK_PB0, 2, 0xe21c, BIT(16 + 10) | BIT(16 + 11)),  
	RK_MUXROUTE_SAME(4, RK_PC0, 2, 0xe21c, BIT(16 + 10) | BIT(16 + 11) | BIT(10)),  
	RK_MUXROUTE_SAME(4, RK_PC3, 1, 0xe21c, BIT(16 + 10) | BIT(16 + 11) | BIT(11)),  
	RK_MUXROUTE_SAME(2, RK_PD2, 2, 0xe21c, BIT(16 + 14)),  
	RK_MUXROUTE_SAME(4, RK_PD0, 1, 0xe21c, BIT(16 + 14) | BIT(14)),  
};

static struct rockchip_mux_route_data rk3568_mux_route_data[] = {
	RK_MUXROUTE_PMU(0, RK_PB7, 1, 0x0110, WRITE_MASK_VAL(1, 0, 0)),  
	RK_MUXROUTE_PMU(0, RK_PC7, 2, 0x0110, WRITE_MASK_VAL(1, 0, 1)),  
	RK_MUXROUTE_PMU(0, RK_PC0, 1, 0x0110, WRITE_MASK_VAL(3, 2, 0)),  
	RK_MUXROUTE_PMU(0, RK_PB5, 4, 0x0110, WRITE_MASK_VAL(3, 2, 1)),  
	RK_MUXROUTE_PMU(0, RK_PC1, 1, 0x0110, WRITE_MASK_VAL(5, 4, 0)),  
	RK_MUXROUTE_PMU(0, RK_PB6, 4, 0x0110, WRITE_MASK_VAL(5, 4, 1)),  
	RK_MUXROUTE_GRF(0, RK_PB3, 2, 0x0300, WRITE_MASK_VAL(0, 0, 0)),  
	RK_MUXROUTE_GRF(2, RK_PA1, 4, 0x0300, WRITE_MASK_VAL(0, 0, 1)),  
	RK_MUXROUTE_GRF(1, RK_PA1, 3, 0x0300, WRITE_MASK_VAL(2, 2, 0)),  
	RK_MUXROUTE_GRF(4, RK_PC3, 3, 0x0300, WRITE_MASK_VAL(2, 2, 1)),  
	RK_MUXROUTE_GRF(4, RK_PB5, 3, 0x0300, WRITE_MASK_VAL(4, 4, 0)),  
	RK_MUXROUTE_GRF(2, RK_PB2, 4, 0x0300, WRITE_MASK_VAL(4, 4, 1)),  
	RK_MUXROUTE_GRF(4, RK_PC4, 1, 0x0300, WRITE_MASK_VAL(6, 6, 0)),  
	RK_MUXROUTE_GRF(0, RK_PC2, 2, 0x0300, WRITE_MASK_VAL(6, 6, 1)),  
	RK_MUXROUTE_GRF(3, RK_PB1, 3, 0x0300, WRITE_MASK_VAL(8, 8, 0)),  
	RK_MUXROUTE_GRF(4, RK_PA7, 3, 0x0300, WRITE_MASK_VAL(8, 8, 1)),  
	RK_MUXROUTE_GRF(4, RK_PD1, 1, 0x0300, WRITE_MASK_VAL(10, 10, 0)),  
	RK_MUXROUTE_GRF(0, RK_PC7, 1, 0x0300, WRITE_MASK_VAL(10, 10, 1)),  
	RK_MUXROUTE_GRF(0, RK_PB6, 1, 0x0300, WRITE_MASK_VAL(14, 14, 0)),  
	RK_MUXROUTE_GRF(4, RK_PB4, 1, 0x0300, WRITE_MASK_VAL(14, 14, 1)),  
	RK_MUXROUTE_GRF(1, RK_PA0, 1, 0x0304, WRITE_MASK_VAL(0, 0, 0)),  
	RK_MUXROUTE_GRF(3, RK_PB6, 4, 0x0304, WRITE_MASK_VAL(0, 0, 1)),  
	RK_MUXROUTE_GRF(4, RK_PB2, 1, 0x0304, WRITE_MASK_VAL(2, 2, 0)),  
	RK_MUXROUTE_GRF(2, RK_PB1, 2, 0x0304, WRITE_MASK_VAL(2, 2, 1)),  
	RK_MUXROUTE_GRF(3, RK_PB4, 4, 0x0304, WRITE_MASK_VAL(4, 4, 0)),  
	RK_MUXROUTE_GRF(4, RK_PD0, 2, 0x0304, WRITE_MASK_VAL(4, 4, 1)),  
	RK_MUXROUTE_GRF(3, RK_PB1, 5, 0x0304, WRITE_MASK_VAL(14, 14, 0)),  
	RK_MUXROUTE_GRF(1, RK_PD5, 4, 0x0304, WRITE_MASK_VAL(14, 14, 1)),  
	RK_MUXROUTE_GRF(3, RK_PB2, 5, 0x0308, WRITE_MASK_VAL(0, 0, 0)),  
	RK_MUXROUTE_GRF(1, RK_PD6, 4, 0x0308, WRITE_MASK_VAL(0, 0, 1)),  
	RK_MUXROUTE_GRF(3, RK_PB5, 5, 0x0308, WRITE_MASK_VAL(2, 2, 0)),  
	RK_MUXROUTE_GRF(2, RK_PA1, 2, 0x0308, WRITE_MASK_VAL(2, 2, 1)),  
	RK_MUXROUTE_GRF(3, RK_PB6, 5, 0x0308, WRITE_MASK_VAL(4, 4, 0)),  
	RK_MUXROUTE_GRF(4, RK_PC0, 3, 0x0308, WRITE_MASK_VAL(4, 4, 1)),  
	RK_MUXROUTE_GRF(3, RK_PB7, 2, 0x0308, WRITE_MASK_VAL(6, 6, 0)),  
	RK_MUXROUTE_GRF(4, RK_PC5, 1, 0x0308, WRITE_MASK_VAL(6, 6, 1)),  
	RK_MUXROUTE_GRF(3, RK_PC0, 2, 0x0308, WRITE_MASK_VAL(8, 8, 0)),  
	RK_MUXROUTE_GRF(4, RK_PC6, 1, 0x0308, WRITE_MASK_VAL(8, 8, 1)),  
	RK_MUXROUTE_GRF(3, RK_PC4, 1, 0x0308, WRITE_MASK_VAL(10, 10, 0)),  
	RK_MUXROUTE_GRF(4, RK_PC2, 1, 0x0308, WRITE_MASK_VAL(10, 10, 1)),  
	RK_MUXROUTE_GRF(3, RK_PC5, 1, 0x0308, WRITE_MASK_VAL(12, 12, 0)),  
	RK_MUXROUTE_GRF(4, RK_PC3, 1, 0x0308, WRITE_MASK_VAL(12, 12, 1)),  
	RK_MUXROUTE_GRF(3, RK_PD2, 3, 0x0308, WRITE_MASK_VAL(14, 14, 0)),  
	RK_MUXROUTE_GRF(3, RK_PA5, 5, 0x0308, WRITE_MASK_VAL(14, 14, 1)),  
	RK_MUXROUTE_GRF(0, RK_PB5, 2, 0x030c, WRITE_MASK_VAL(0, 0, 0)),  
	RK_MUXROUTE_GRF(2, RK_PD3, 3, 0x030c, WRITE_MASK_VAL(0, 0, 1)),  
	RK_MUXROUTE_GRF(2, RK_PB5, 3, 0x030c, WRITE_MASK_VAL(2, 2, 0)),  
	RK_MUXROUTE_GRF(3, RK_PC3, 3, 0x030c, WRITE_MASK_VAL(2, 2, 1)),  
	RK_MUXROUTE_GRF(2, RK_PC1, 4, 0x030c, WRITE_MASK_VAL(4, 4, 0)),  
	RK_MUXROUTE_GRF(3, RK_PA0, 3, 0x030c, WRITE_MASK_VAL(4, 4, 1)),  
	RK_MUXROUTE_GRF(4, RK_PB3, 4, 0x030c, WRITE_MASK_VAL(6, 6, 0)),  
	RK_MUXROUTE_GRF(4, RK_PC2, 2, 0x030c, WRITE_MASK_VAL(6, 6, 1)),  
	RK_MUXROUTE_GRF(2, RK_PB4, 2, 0x030c, WRITE_MASK_VAL(8, 8, 0)),  
	RK_MUXROUTE_GRF(3, RK_PD6, 4, 0x030c, WRITE_MASK_VAL(8, 8, 1)),  
	RK_MUXROUTE_GRF(0, RK_PD1, 1, 0x030c, WRITE_MASK_VAL(10, 10, 0)),  
	RK_MUXROUTE_GRF(1, RK_PD5, 2, 0x030c, WRITE_MASK_VAL(10, 10, 1)),  
	RK_MUXROUTE_GRF(1, RK_PA1, 2, 0x030c, WRITE_MASK_VAL(12, 12, 0)),  
	RK_MUXROUTE_GRF(3, RK_PB7, 4, 0x030c, WRITE_MASK_VAL(12, 12, 1)),  
	RK_MUXROUTE_GRF(1, RK_PA6, 2, 0x030c, WRITE_MASK_VAL(14, 14, 0)),  
	RK_MUXROUTE_GRF(3, RK_PB2, 4, 0x030c, WRITE_MASK_VAL(14, 14, 1)),  
	RK_MUXROUTE_GRF(2, RK_PA2, 3, 0x0310, WRITE_MASK_VAL(0, 0, 0)),  
	RK_MUXROUTE_GRF(3, RK_PC2, 4, 0x0310, WRITE_MASK_VAL(0, 0, 1)),  
	RK_MUXROUTE_GRF(2, RK_PA4, 3, 0x0310, WRITE_MASK_VAL(2, 2, 0)),  
	RK_MUXROUTE_GRF(1, RK_PD5, 3, 0x0310, WRITE_MASK_VAL(2, 2, 1)),  
	RK_MUXROUTE_GRF(2, RK_PA6, 3, 0x0310, WRITE_MASK_VAL(5, 4, 0)),  
	RK_MUXROUTE_GRF(3, RK_PC4, 4, 0x0310, WRITE_MASK_VAL(5, 4, 1)),  
	RK_MUXROUTE_GRF(4, RK_PA2, 4, 0x0310, WRITE_MASK_VAL(5, 4, 2)),  
	RK_MUXROUTE_GRF(2, RK_PC5, 3, 0x0310, WRITE_MASK_VAL(6, 6, 0)),  
	RK_MUXROUTE_GRF(2, RK_PD7, 4, 0x0310, WRITE_MASK_VAL(6, 6, 1)),  
	RK_MUXROUTE_GRF(2, RK_PB0, 3, 0x0310, WRITE_MASK_VAL(9, 8, 0)),  
	RK_MUXROUTE_GRF(4, RK_PC5, 4, 0x0310, WRITE_MASK_VAL(9, 8, 1)),  
	RK_MUXROUTE_GRF(4, RK_PA4, 4, 0x0310, WRITE_MASK_VAL(9, 8, 2)),  
	RK_MUXROUTE_GRF(1, RK_PA2, 1, 0x0310, WRITE_MASK_VAL(11, 10, 0)),  
	RK_MUXROUTE_GRF(3, RK_PC6, 4, 0x0310, WRITE_MASK_VAL(11, 10, 1)),  
	RK_MUXROUTE_GRF(2, RK_PD0, 5, 0x0310, WRITE_MASK_VAL(11, 10, 2)),  
	RK_MUXROUTE_GRF(2, RK_PC1, 1, 0x0310, WRITE_MASK_VAL(12, 12, 0)),  
	RK_MUXROUTE_GRF(4, RK_PB6, 5, 0x0310, WRITE_MASK_VAL(12, 12, 1)),  
	RK_MUXROUTE_GRF(3, RK_PA2, 4, 0x0310, WRITE_MASK_VAL(14, 14, 0)),  
	RK_MUXROUTE_GRF(4, RK_PC2, 5, 0x0310, WRITE_MASK_VAL(14, 14, 1)),  
	RK_MUXROUTE_GRF(1, RK_PA4, 3, 0x0314, WRITE_MASK_VAL(1, 0, 0)),  
	RK_MUXROUTE_GRF(1, RK_PA6, 3, 0x0314, WRITE_MASK_VAL(1, 0, 0)),  
	RK_MUXROUTE_GRF(3, RK_PD6, 5, 0x0314, WRITE_MASK_VAL(1, 0, 1)),  
	RK_MUXROUTE_GRF(4, RK_PA0, 4, 0x0314, WRITE_MASK_VAL(1, 0, 1)),  
	RK_MUXROUTE_GRF(3, RK_PC4, 5, 0x0314, WRITE_MASK_VAL(1, 0, 2)),  
	RK_MUXROUTE_GRF(0, RK_PA5, 3, 0x0314, WRITE_MASK_VAL(3, 2, 0)),  
	RK_MUXROUTE_GRF(2, RK_PD0, 4, 0x0314, WRITE_MASK_VAL(3, 2, 1)),  
	RK_MUXROUTE_GRF(1, RK_PB0, 4, 0x0314, WRITE_MASK_VAL(3, 2, 2)),  
	RK_MUXROUTE_GRF(0, RK_PA4, 3, 0x0314, WRITE_MASK_VAL(5, 4, 0)),  
	RK_MUXROUTE_GRF(2, RK_PD2, 4, 0x0314, WRITE_MASK_VAL(5, 4, 1)),  
	RK_MUXROUTE_GRF(1, RK_PA5, 4, 0x0314, WRITE_MASK_VAL(5, 4, 2)),  
	RK_MUXROUTE_GRF(0, RK_PA6, 2, 0x0314, WRITE_MASK_VAL(7, 6, 0)),  
	RK_MUXROUTE_GRF(2, RK_PD4, 4, 0x0314, WRITE_MASK_VAL(7, 6, 1)),  
	RK_MUXROUTE_GRF(4, RK_PC2, 4, 0x0314, WRITE_MASK_VAL(7, 6, 2)),  
};

static bool rockchip_get_mux_route(struct rockchip_pin_bank *bank, int pin,
				   int mux, u32 *loc, u32 *reg, u32 *value)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	struct rockchip_mux_route_data *data;
	int i;

	for (i = 0; i < ctrl->niomux_routes; i++) {
		data = &ctrl->iomux_routes[i];
		if ((data->bank_num == bank->bank_num) &&
		    (data->pin == pin) && (data->func == mux))
			break;
	}

	if (i >= ctrl->niomux_routes)
		return false;

	*loc = data->route_location;
	*reg = data->route_offset;
	*value = data->route_val;

	return true;
}

static int rockchip_get_mux(struct rockchip_pin_bank *bank, int pin)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	int iomux_num = (pin / 8);
	struct regmap *regmap;
	unsigned int val;
	int reg, ret, mask, mux_type;
	u8 bit;

	if (iomux_num > 3)
		return -EINVAL;

	if (bank->iomux[iomux_num].type & IOMUX_UNROUTED) {
		dev_err(info->dev, "pin %d is unrouted\n", pin);
		return -EINVAL;
	}

	if (bank->iomux[iomux_num].type & IOMUX_GPIO_ONLY)
		return RK_FUNC_GPIO;

	if (bank->iomux[iomux_num].type & IOMUX_SOURCE_PMU)
		regmap = info->regmap_pmu;
	else if (bank->iomux[iomux_num].type & IOMUX_L_SOURCE_PMU)
		regmap = (pin % 8 < 4) ? info->regmap_pmu : info->regmap_base;
	else
		regmap = info->regmap_base;

	 
	mux_type = bank->iomux[iomux_num].type;
	reg = bank->iomux[iomux_num].offset;
	if (mux_type & IOMUX_WIDTH_4BIT) {
		if ((pin % 8) >= 4)
			reg += 0x4;
		bit = (pin % 4) * 4;
		mask = 0xf;
	} else if (mux_type & IOMUX_WIDTH_3BIT) {
		if ((pin % 8) >= 5)
			reg += 0x4;
		bit = (pin % 8 % 5) * 3;
		mask = 0x7;
	} else {
		bit = (pin % 8) * 2;
		mask = 0x3;
	}

	if (bank->recalced_mask & BIT(pin))
		rockchip_get_recalced_mux(bank, pin, &reg, &bit, &mask);

	if (ctrl->type == RK3588) {
		if (bank->bank_num == 0) {
			if ((pin >= RK_PB4) && (pin <= RK_PD7)) {
				u32 reg0 = 0;

				reg0 = reg + 0x4000 - 0xC;  
				ret = regmap_read(regmap, reg0, &val);
				if (ret)
					return ret;

				if (!(val & BIT(8)))
					return ((val >> bit) & mask);

				reg = reg + 0x8000;  
				regmap = info->regmap_base;
			}
		} else if (bank->bank_num > 0) {
			reg += 0x8000;  
		}
	}

	ret = regmap_read(regmap, reg, &val);
	if (ret)
		return ret;

	return ((val >> bit) & mask);
}

static int rockchip_verify_mux(struct rockchip_pin_bank *bank,
			       int pin, int mux)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct device *dev = info->dev;
	int iomux_num = (pin / 8);

	if (iomux_num > 3)
		return -EINVAL;

	if (bank->iomux[iomux_num].type & IOMUX_UNROUTED) {
		dev_err(dev, "pin %d is unrouted\n", pin);
		return -EINVAL;
	}

	if (bank->iomux[iomux_num].type & IOMUX_GPIO_ONLY) {
		if (mux != RK_FUNC_GPIO) {
			dev_err(dev, "pin %d only supports a gpio mux\n", pin);
			return -ENOTSUPP;
		}
	}

	return 0;
}

 
static int rockchip_set_mux(struct rockchip_pin_bank *bank, int pin, int mux)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	struct device *dev = info->dev;
	int iomux_num = (pin / 8);
	struct regmap *regmap;
	int reg, ret, mask, mux_type;
	u8 bit;
	u32 data, rmask, route_location, route_reg, route_val;

	ret = rockchip_verify_mux(bank, pin, mux);
	if (ret < 0)
		return ret;

	if (bank->iomux[iomux_num].type & IOMUX_GPIO_ONLY)
		return 0;

	dev_dbg(dev, "setting mux of GPIO%d-%d to %d\n", bank->bank_num, pin, mux);

	if (bank->iomux[iomux_num].type & IOMUX_SOURCE_PMU)
		regmap = info->regmap_pmu;
	else if (bank->iomux[iomux_num].type & IOMUX_L_SOURCE_PMU)
		regmap = (pin % 8 < 4) ? info->regmap_pmu : info->regmap_base;
	else
		regmap = info->regmap_base;

	 
	mux_type = bank->iomux[iomux_num].type;
	reg = bank->iomux[iomux_num].offset;
	if (mux_type & IOMUX_WIDTH_4BIT) {
		if ((pin % 8) >= 4)
			reg += 0x4;
		bit = (pin % 4) * 4;
		mask = 0xf;
	} else if (mux_type & IOMUX_WIDTH_3BIT) {
		if ((pin % 8) >= 5)
			reg += 0x4;
		bit = (pin % 8 % 5) * 3;
		mask = 0x7;
	} else {
		bit = (pin % 8) * 2;
		mask = 0x3;
	}

	if (bank->recalced_mask & BIT(pin))
		rockchip_get_recalced_mux(bank, pin, &reg, &bit, &mask);

	if (ctrl->type == RK3588) {
		if (bank->bank_num == 0) {
			if ((pin >= RK_PB4) && (pin <= RK_PD7)) {
				if (mux < 8) {
					reg += 0x4000 - 0xC;  
					data = (mask << (bit + 16));
					rmask = data | (data >> 16);
					data |= (mux & mask) << bit;
					ret = regmap_update_bits(regmap, reg, rmask, data);
				} else {
					u32 reg0 = 0;

					reg0 = reg + 0x4000 - 0xC;  
					data = (mask << (bit + 16));
					rmask = data | (data >> 16);
					data |= 8 << bit;
					ret = regmap_update_bits(regmap, reg0, rmask, data);

					reg0 = reg + 0x8000;  
					data = (mask << (bit + 16));
					rmask = data | (data >> 16);
					data |= mux << bit;
					regmap = info->regmap_base;
					ret |= regmap_update_bits(regmap, reg0, rmask, data);
				}
			} else {
				data = (mask << (bit + 16));
				rmask = data | (data >> 16);
				data |= (mux & mask) << bit;
				ret = regmap_update_bits(regmap, reg, rmask, data);
			}
			return ret;
		} else if (bank->bank_num > 0) {
			reg += 0x8000;  
		}
	}

	if (mux > mask)
		return -EINVAL;

	if (bank->route_mask & BIT(pin)) {
		if (rockchip_get_mux_route(bank, pin, mux, &route_location,
					   &route_reg, &route_val)) {
			struct regmap *route_regmap = regmap;

			 
			switch (route_location) {
			case ROCKCHIP_ROUTE_PMU:
				route_regmap = info->regmap_pmu;
				break;
			case ROCKCHIP_ROUTE_GRF:
				route_regmap = info->regmap_base;
				break;
			}

			ret = regmap_write(route_regmap, route_reg, route_val);
			if (ret)
				return ret;
		}
	}

	data = (mask << (bit + 16));
	rmask = data | (data >> 16);
	data |= (mux & mask) << bit;
	ret = regmap_update_bits(regmap, reg, rmask, data);

	return ret;
}

#define PX30_PULL_PMU_OFFSET		0x10
#define PX30_PULL_GRF_OFFSET		0x60
#define PX30_PULL_BITS_PER_PIN		2
#define PX30_PULL_PINS_PER_REG		8
#define PX30_PULL_BANK_STRIDE		16

static int px30_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
				      int pin_num, struct regmap **regmap,
				      int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	 
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = PX30_PULL_PMU_OFFSET;
	} else {
		*regmap = info->regmap_base;
		*reg = PX30_PULL_GRF_OFFSET;

		 
		*reg -= 0x10;
		*reg += bank->bank_num * PX30_PULL_BANK_STRIDE;
	}

	*reg += ((pin_num / PX30_PULL_PINS_PER_REG) * 4);
	*bit = (pin_num % PX30_PULL_PINS_PER_REG);
	*bit *= PX30_PULL_BITS_PER_PIN;

	return 0;
}

#define PX30_DRV_PMU_OFFSET		0x20
#define PX30_DRV_GRF_OFFSET		0xf0
#define PX30_DRV_BITS_PER_PIN		2
#define PX30_DRV_PINS_PER_REG		8
#define PX30_DRV_BANK_STRIDE		16

static int px30_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				     int pin_num, struct regmap **regmap,
				     int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	 
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = PX30_DRV_PMU_OFFSET;
	} else {
		*regmap = info->regmap_base;
		*reg = PX30_DRV_GRF_OFFSET;

		 
		*reg -= 0x10;
		*reg += bank->bank_num * PX30_DRV_BANK_STRIDE;
	}

	*reg += ((pin_num / PX30_DRV_PINS_PER_REG) * 4);
	*bit = (pin_num % PX30_DRV_PINS_PER_REG);
	*bit *= PX30_DRV_BITS_PER_PIN;

	return 0;
}

#define PX30_SCHMITT_PMU_OFFSET			0x38
#define PX30_SCHMITT_GRF_OFFSET			0xc0
#define PX30_SCHMITT_PINS_PER_PMU_REG		16
#define PX30_SCHMITT_BANK_STRIDE		16
#define PX30_SCHMITT_PINS_PER_GRF_REG		8

static int px30_calc_schmitt_reg_and_bit(struct rockchip_pin_bank *bank,
					 int pin_num,
					 struct regmap **regmap,
					 int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	int pins_per_reg;

	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = PX30_SCHMITT_PMU_OFFSET;
		pins_per_reg = PX30_SCHMITT_PINS_PER_PMU_REG;
	} else {
		*regmap = info->regmap_base;
		*reg = PX30_SCHMITT_GRF_OFFSET;
		pins_per_reg = PX30_SCHMITT_PINS_PER_GRF_REG;
		*reg += (bank->bank_num  - 1) * PX30_SCHMITT_BANK_STRIDE;
	}

	*reg += ((pin_num / pins_per_reg) * 4);
	*bit = pin_num % pins_per_reg;

	return 0;
}

#define RV1108_PULL_PMU_OFFSET		0x10
#define RV1108_PULL_OFFSET		0x110
#define RV1108_PULL_PINS_PER_REG	8
#define RV1108_PULL_BITS_PER_PIN	2
#define RV1108_PULL_BANK_STRIDE		16

static int rv1108_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	 
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RV1108_PULL_PMU_OFFSET;
	} else {
		*reg = RV1108_PULL_OFFSET;
		*regmap = info->regmap_base;
		 
		*reg -= 0x10;
		*reg += bank->bank_num * RV1108_PULL_BANK_STRIDE;
	}

	*reg += ((pin_num / RV1108_PULL_PINS_PER_REG) * 4);
	*bit = (pin_num % RV1108_PULL_PINS_PER_REG);
	*bit *= RV1108_PULL_BITS_PER_PIN;

	return 0;
}

#define RV1108_DRV_PMU_OFFSET		0x20
#define RV1108_DRV_GRF_OFFSET		0x210
#define RV1108_DRV_BITS_PER_PIN		2
#define RV1108_DRV_PINS_PER_REG		8
#define RV1108_DRV_BANK_STRIDE		16

static int rv1108_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	 
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RV1108_DRV_PMU_OFFSET;
	} else {
		*regmap = info->regmap_base;
		*reg = RV1108_DRV_GRF_OFFSET;

		 
		*reg -= 0x10;
		*reg += bank->bank_num * RV1108_DRV_BANK_STRIDE;
	}

	*reg += ((pin_num / RV1108_DRV_PINS_PER_REG) * 4);
	*bit = pin_num % RV1108_DRV_PINS_PER_REG;
	*bit *= RV1108_DRV_BITS_PER_PIN;

	return 0;
}

#define RV1108_SCHMITT_PMU_OFFSET		0x30
#define RV1108_SCHMITT_GRF_OFFSET		0x388
#define RV1108_SCHMITT_BANK_STRIDE		8
#define RV1108_SCHMITT_PINS_PER_GRF_REG		16
#define RV1108_SCHMITT_PINS_PER_PMU_REG		8

static int rv1108_calc_schmitt_reg_and_bit(struct rockchip_pin_bank *bank,
					   int pin_num,
					   struct regmap **regmap,
					   int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	int pins_per_reg;

	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RV1108_SCHMITT_PMU_OFFSET;
		pins_per_reg = RV1108_SCHMITT_PINS_PER_PMU_REG;
	} else {
		*regmap = info->regmap_base;
		*reg = RV1108_SCHMITT_GRF_OFFSET;
		pins_per_reg = RV1108_SCHMITT_PINS_PER_GRF_REG;
		*reg += (bank->bank_num  - 1) * RV1108_SCHMITT_BANK_STRIDE;
	}
	*reg += ((pin_num / pins_per_reg) * 4);
	*bit = pin_num % pins_per_reg;

	return 0;
}

#define RV1126_PULL_PMU_OFFSET		0x40
#define RV1126_PULL_GRF_GPIO1A0_OFFSET	0x10108
#define RV1126_PULL_PINS_PER_REG	8
#define RV1126_PULL_BITS_PER_PIN	2
#define RV1126_PULL_BANK_STRIDE		16
#define RV1126_GPIO_C4_D7(p)		(p >= 20 && p <= 31)  

static int rv1126_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	 
	if (bank->bank_num == 0) {
		if (RV1126_GPIO_C4_D7(pin_num)) {
			*regmap = info->regmap_base;
			*reg = RV1126_PULL_GRF_GPIO1A0_OFFSET;
			*reg -= (((31 - pin_num) / RV1126_PULL_PINS_PER_REG + 1) * 4);
			*bit = pin_num % RV1126_PULL_PINS_PER_REG;
			*bit *= RV1126_PULL_BITS_PER_PIN;
			return 0;
		}
		*regmap = info->regmap_pmu;
		*reg = RV1126_PULL_PMU_OFFSET;
	} else {
		*reg = RV1126_PULL_GRF_GPIO1A0_OFFSET;
		*regmap = info->regmap_base;
		*reg += (bank->bank_num - 1) * RV1126_PULL_BANK_STRIDE;
	}

	*reg += ((pin_num / RV1126_PULL_PINS_PER_REG) * 4);
	*bit = (pin_num % RV1126_PULL_PINS_PER_REG);
	*bit *= RV1126_PULL_BITS_PER_PIN;

	return 0;
}

#define RV1126_DRV_PMU_OFFSET		0x20
#define RV1126_DRV_GRF_GPIO1A0_OFFSET	0x10090
#define RV1126_DRV_BITS_PER_PIN		4
#define RV1126_DRV_PINS_PER_REG		4
#define RV1126_DRV_BANK_STRIDE		32

static int rv1126_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	 
	if (bank->bank_num == 0) {
		if (RV1126_GPIO_C4_D7(pin_num)) {
			*regmap = info->regmap_base;
			*reg = RV1126_DRV_GRF_GPIO1A0_OFFSET;
			*reg -= (((31 - pin_num) / RV1126_DRV_PINS_PER_REG + 1) * 4);
			*reg -= 0x4;
			*bit = pin_num % RV1126_DRV_PINS_PER_REG;
			*bit *= RV1126_DRV_BITS_PER_PIN;
			return 0;
		}
		*regmap = info->regmap_pmu;
		*reg = RV1126_DRV_PMU_OFFSET;
	} else {
		*regmap = info->regmap_base;
		*reg = RV1126_DRV_GRF_GPIO1A0_OFFSET;
		*reg += (bank->bank_num - 1) * RV1126_DRV_BANK_STRIDE;
	}

	*reg += ((pin_num / RV1126_DRV_PINS_PER_REG) * 4);
	*bit = pin_num % RV1126_DRV_PINS_PER_REG;
	*bit *= RV1126_DRV_BITS_PER_PIN;

	return 0;
}

#define RV1126_SCHMITT_PMU_OFFSET		0x60
#define RV1126_SCHMITT_GRF_GPIO1A0_OFFSET	0x10188
#define RV1126_SCHMITT_BANK_STRIDE		16
#define RV1126_SCHMITT_PINS_PER_GRF_REG		8
#define RV1126_SCHMITT_PINS_PER_PMU_REG		8

static int rv1126_calc_schmitt_reg_and_bit(struct rockchip_pin_bank *bank,
					   int pin_num,
					   struct regmap **regmap,
					   int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	int pins_per_reg;

	if (bank->bank_num == 0) {
		if (RV1126_GPIO_C4_D7(pin_num)) {
			*regmap = info->regmap_base;
			*reg = RV1126_SCHMITT_GRF_GPIO1A0_OFFSET;
			*reg -= (((31 - pin_num) / RV1126_SCHMITT_PINS_PER_GRF_REG + 1) * 4);
			*bit = pin_num % RV1126_SCHMITT_PINS_PER_GRF_REG;
			return 0;
		}
		*regmap = info->regmap_pmu;
		*reg = RV1126_SCHMITT_PMU_OFFSET;
		pins_per_reg = RV1126_SCHMITT_PINS_PER_PMU_REG;
	} else {
		*regmap = info->regmap_base;
		*reg = RV1126_SCHMITT_GRF_GPIO1A0_OFFSET;
		pins_per_reg = RV1126_SCHMITT_PINS_PER_GRF_REG;
		*reg += (bank->bank_num - 1) * RV1126_SCHMITT_BANK_STRIDE;
	}
	*reg += ((pin_num / pins_per_reg) * 4);
	*bit = pin_num % pins_per_reg;

	return 0;
}

#define RK3308_SCHMITT_PINS_PER_REG		8
#define RK3308_SCHMITT_BANK_STRIDE		16
#define RK3308_SCHMITT_GRF_OFFSET		0x1a0

static int rk3308_calc_schmitt_reg_and_bit(struct rockchip_pin_bank *bank,
				    int pin_num, struct regmap **regmap,
				    int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK3308_SCHMITT_GRF_OFFSET;

	*reg += bank->bank_num * RK3308_SCHMITT_BANK_STRIDE;
	*reg += ((pin_num / RK3308_SCHMITT_PINS_PER_REG) * 4);
	*bit = pin_num % RK3308_SCHMITT_PINS_PER_REG;

	return 0;
}

#define RK2928_PULL_OFFSET		0x118
#define RK2928_PULL_PINS_PER_REG	16
#define RK2928_PULL_BANK_STRIDE		8

static int rk2928_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK2928_PULL_OFFSET;
	*reg += bank->bank_num * RK2928_PULL_BANK_STRIDE;
	*reg += (pin_num / RK2928_PULL_PINS_PER_REG) * 4;

	*bit = pin_num % RK2928_PULL_PINS_PER_REG;

	return 0;
};

#define RK3128_PULL_OFFSET	0x118

static int rk3128_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK3128_PULL_OFFSET;
	*reg += bank->bank_num * RK2928_PULL_BANK_STRIDE;
	*reg += ((pin_num / RK2928_PULL_PINS_PER_REG) * 4);

	*bit = pin_num % RK2928_PULL_PINS_PER_REG;

	return 0;
}

#define RK3188_PULL_OFFSET		0x164
#define RK3188_PULL_BITS_PER_PIN	2
#define RK3188_PULL_PINS_PER_REG	8
#define RK3188_PULL_BANK_STRIDE		16
#define RK3188_PULL_PMU_OFFSET		0x64

static int rk3188_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	 
	if (bank->bank_num == 0 && pin_num < 12) {
		*regmap = info->regmap_pmu ? info->regmap_pmu
					   : bank->regmap_pull;
		*reg = info->regmap_pmu ? RK3188_PULL_PMU_OFFSET : 0;
		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);
		*bit = pin_num % RK3188_PULL_PINS_PER_REG;
		*bit *= RK3188_PULL_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_pull ? info->regmap_pull
					    : info->regmap_base;
		*reg = info->regmap_pull ? 0 : RK3188_PULL_OFFSET;

		 
		*reg -= 4;
		*reg += bank->bank_num * RK3188_PULL_BANK_STRIDE;
		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);

		 
		*bit = 7 - (pin_num % RK3188_PULL_PINS_PER_REG);
		*bit *= RK3188_PULL_BITS_PER_PIN;
	}

	return 0;
}

#define RK3288_PULL_OFFSET		0x140
static int rk3288_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	 
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RK3188_PULL_PMU_OFFSET;

		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);
		*bit = pin_num % RK3188_PULL_PINS_PER_REG;
		*bit *= RK3188_PULL_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3288_PULL_OFFSET;

		 
		*reg -= 0x10;
		*reg += bank->bank_num * RK3188_PULL_BANK_STRIDE;
		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);

		*bit = (pin_num % RK3188_PULL_PINS_PER_REG);
		*bit *= RK3188_PULL_BITS_PER_PIN;
	}

	return 0;
}

#define RK3288_DRV_PMU_OFFSET		0x70
#define RK3288_DRV_GRF_OFFSET		0x1c0
#define RK3288_DRV_BITS_PER_PIN		2
#define RK3288_DRV_PINS_PER_REG		8
#define RK3288_DRV_BANK_STRIDE		16

static int rk3288_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	 
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RK3288_DRV_PMU_OFFSET;

		*reg += ((pin_num / RK3288_DRV_PINS_PER_REG) * 4);
		*bit = pin_num % RK3288_DRV_PINS_PER_REG;
		*bit *= RK3288_DRV_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3288_DRV_GRF_OFFSET;

		 
		*reg -= 0x10;
		*reg += bank->bank_num * RK3288_DRV_BANK_STRIDE;
		*reg += ((pin_num / RK3288_DRV_PINS_PER_REG) * 4);

		*bit = (pin_num % RK3288_DRV_PINS_PER_REG);
		*bit *= RK3288_DRV_BITS_PER_PIN;
	}

	return 0;
}

#define RK3228_PULL_OFFSET		0x100

static int rk3228_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK3228_PULL_OFFSET;
	*reg += bank->bank_num * RK3188_PULL_BANK_STRIDE;
	*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);

	*bit = (pin_num % RK3188_PULL_PINS_PER_REG);
	*bit *= RK3188_PULL_BITS_PER_PIN;

	return 0;
}

#define RK3228_DRV_GRF_OFFSET		0x200

static int rk3228_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK3228_DRV_GRF_OFFSET;
	*reg += bank->bank_num * RK3288_DRV_BANK_STRIDE;
	*reg += ((pin_num / RK3288_DRV_PINS_PER_REG) * 4);

	*bit = (pin_num % RK3288_DRV_PINS_PER_REG);
	*bit *= RK3288_DRV_BITS_PER_PIN;

	return 0;
}

#define RK3308_PULL_OFFSET		0xa0

static int rk3308_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK3308_PULL_OFFSET;
	*reg += bank->bank_num * RK3188_PULL_BANK_STRIDE;
	*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);

	*bit = (pin_num % RK3188_PULL_PINS_PER_REG);
	*bit *= RK3188_PULL_BITS_PER_PIN;

	return 0;
}

#define RK3308_DRV_GRF_OFFSET		0x100

static int rk3308_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK3308_DRV_GRF_OFFSET;
	*reg += bank->bank_num * RK3288_DRV_BANK_STRIDE;
	*reg += ((pin_num / RK3288_DRV_PINS_PER_REG) * 4);

	*bit = (pin_num % RK3288_DRV_PINS_PER_REG);
	*bit *= RK3288_DRV_BITS_PER_PIN;

	return 0;
}

#define RK3368_PULL_GRF_OFFSET		0x100
#define RK3368_PULL_PMU_OFFSET		0x10

static int rk3368_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	 
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RK3368_PULL_PMU_OFFSET;

		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);
		*bit = pin_num % RK3188_PULL_PINS_PER_REG;
		*bit *= RK3188_PULL_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3368_PULL_GRF_OFFSET;

		 
		*reg -= 0x10;
		*reg += bank->bank_num * RK3188_PULL_BANK_STRIDE;
		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);

		*bit = (pin_num % RK3188_PULL_PINS_PER_REG);
		*bit *= RK3188_PULL_BITS_PER_PIN;
	}

	return 0;
}

#define RK3368_DRV_PMU_OFFSET		0x20
#define RK3368_DRV_GRF_OFFSET		0x200

static int rk3368_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	 
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RK3368_DRV_PMU_OFFSET;

		*reg += ((pin_num / RK3288_DRV_PINS_PER_REG) * 4);
		*bit = pin_num % RK3288_DRV_PINS_PER_REG;
		*bit *= RK3288_DRV_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3368_DRV_GRF_OFFSET;

		 
		*reg -= 0x10;
		*reg += bank->bank_num * RK3288_DRV_BANK_STRIDE;
		*reg += ((pin_num / RK3288_DRV_PINS_PER_REG) * 4);

		*bit = (pin_num % RK3288_DRV_PINS_PER_REG);
		*bit *= RK3288_DRV_BITS_PER_PIN;
	}

	return 0;
}

#define RK3399_PULL_GRF_OFFSET		0xe040
#define RK3399_PULL_PMU_OFFSET		0x40
#define RK3399_DRV_3BITS_PER_PIN	3

static int rk3399_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	 
	if ((bank->bank_num == 0) || (bank->bank_num == 1)) {
		*regmap = info->regmap_pmu;
		*reg = RK3399_PULL_PMU_OFFSET;

		*reg += bank->bank_num * RK3188_PULL_BANK_STRIDE;

		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);
		*bit = pin_num % RK3188_PULL_PINS_PER_REG;
		*bit *= RK3188_PULL_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3399_PULL_GRF_OFFSET;

		 
		*reg -= 0x20;
		*reg += bank->bank_num * RK3188_PULL_BANK_STRIDE;
		*reg += ((pin_num / RK3188_PULL_PINS_PER_REG) * 4);

		*bit = (pin_num % RK3188_PULL_PINS_PER_REG);
		*bit *= RK3188_PULL_BITS_PER_PIN;
	}

	return 0;
}

static int rk3399_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	int drv_num = (pin_num / 8);

	 
	if ((bank->bank_num == 0) || (bank->bank_num == 1))
		*regmap = info->regmap_pmu;
	else
		*regmap = info->regmap_base;

	*reg = bank->drv[drv_num].offset;
	if ((bank->drv[drv_num].drv_type == DRV_TYPE_IO_1V8_3V0_AUTO) ||
	    (bank->drv[drv_num].drv_type == DRV_TYPE_IO_3V3_ONLY))
		*bit = (pin_num % 8) * 3;
	else
		*bit = (pin_num % 8) * 2;

	return 0;
}

#define RK3568_PULL_PMU_OFFSET		0x20
#define RK3568_PULL_GRF_OFFSET		0x80
#define RK3568_PULL_BITS_PER_PIN	2
#define RK3568_PULL_PINS_PER_REG	8
#define RK3568_PULL_BANK_STRIDE		0x10

static int rk3568_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RK3568_PULL_PMU_OFFSET;
		*reg += bank->bank_num * RK3568_PULL_BANK_STRIDE;
		*reg += ((pin_num / RK3568_PULL_PINS_PER_REG) * 4);

		*bit = pin_num % RK3568_PULL_PINS_PER_REG;
		*bit *= RK3568_PULL_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3568_PULL_GRF_OFFSET;
		*reg += (bank->bank_num - 1) * RK3568_PULL_BANK_STRIDE;
		*reg += ((pin_num / RK3568_PULL_PINS_PER_REG) * 4);

		*bit = (pin_num % RK3568_PULL_PINS_PER_REG);
		*bit *= RK3568_PULL_BITS_PER_PIN;
	}

	return 0;
}

#define RK3568_DRV_PMU_OFFSET		0x70
#define RK3568_DRV_GRF_OFFSET		0x200
#define RK3568_DRV_BITS_PER_PIN		8
#define RK3568_DRV_PINS_PER_REG		2
#define RK3568_DRV_BANK_STRIDE		0x40

static int rk3568_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	 
	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RK3568_DRV_PMU_OFFSET;
		*reg += ((pin_num / RK3568_DRV_PINS_PER_REG) * 4);

		*bit = pin_num % RK3568_DRV_PINS_PER_REG;
		*bit *= RK3568_DRV_BITS_PER_PIN;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3568_DRV_GRF_OFFSET;
		*reg += (bank->bank_num - 1) * RK3568_DRV_BANK_STRIDE;
		*reg += ((pin_num / RK3568_DRV_PINS_PER_REG) * 4);

		*bit = (pin_num % RK3568_DRV_PINS_PER_REG);
		*bit *= RK3568_DRV_BITS_PER_PIN;
	}

	return 0;
}

#define RK3588_PMU1_IOC_REG		(0x0000)
#define RK3588_PMU2_IOC_REG		(0x4000)
#define RK3588_BUS_IOC_REG		(0x8000)
#define RK3588_VCCIO1_4_IOC_REG		(0x9000)
#define RK3588_VCCIO3_5_IOC_REG		(0xA000)
#define RK3588_VCCIO2_IOC_REG		(0xB000)
#define RK3588_VCCIO6_IOC_REG		(0xC000)
#define RK3588_EMMC_IOC_REG		(0xD000)

static const u32 rk3588_ds_regs[][2] = {
	{RK_GPIO0_A0, RK3588_PMU1_IOC_REG + 0x0010},
	{RK_GPIO0_A4, RK3588_PMU1_IOC_REG + 0x0014},
	{RK_GPIO0_B0, RK3588_PMU1_IOC_REG + 0x0018},
	{RK_GPIO0_B4, RK3588_PMU2_IOC_REG + 0x0014},
	{RK_GPIO0_C0, RK3588_PMU2_IOC_REG + 0x0018},
	{RK_GPIO0_C4, RK3588_PMU2_IOC_REG + 0x001C},
	{RK_GPIO0_D0, RK3588_PMU2_IOC_REG + 0x0020},
	{RK_GPIO0_D4, RK3588_PMU2_IOC_REG + 0x0024},
	{RK_GPIO1_A0, RK3588_VCCIO1_4_IOC_REG + 0x0020},
	{RK_GPIO1_A4, RK3588_VCCIO1_4_IOC_REG + 0x0024},
	{RK_GPIO1_B0, RK3588_VCCIO1_4_IOC_REG + 0x0028},
	{RK_GPIO1_B4, RK3588_VCCIO1_4_IOC_REG + 0x002C},
	{RK_GPIO1_C0, RK3588_VCCIO1_4_IOC_REG + 0x0030},
	{RK_GPIO1_C4, RK3588_VCCIO1_4_IOC_REG + 0x0034},
	{RK_GPIO1_D0, RK3588_VCCIO1_4_IOC_REG + 0x0038},
	{RK_GPIO1_D4, RK3588_VCCIO1_4_IOC_REG + 0x003C},
	{RK_GPIO2_A0, RK3588_EMMC_IOC_REG + 0x0040},
	{RK_GPIO2_A4, RK3588_VCCIO3_5_IOC_REG + 0x0044},
	{RK_GPIO2_B0, RK3588_VCCIO3_5_IOC_REG + 0x0048},
	{RK_GPIO2_B4, RK3588_VCCIO3_5_IOC_REG + 0x004C},
	{RK_GPIO2_C0, RK3588_VCCIO3_5_IOC_REG + 0x0050},
	{RK_GPIO2_C4, RK3588_VCCIO3_5_IOC_REG + 0x0054},
	{RK_GPIO2_D0, RK3588_EMMC_IOC_REG + 0x0058},
	{RK_GPIO2_D4, RK3588_EMMC_IOC_REG + 0x005C},
	{RK_GPIO3_A0, RK3588_VCCIO3_5_IOC_REG + 0x0060},
	{RK_GPIO3_A4, RK3588_VCCIO3_5_IOC_REG + 0x0064},
	{RK_GPIO3_B0, RK3588_VCCIO3_5_IOC_REG + 0x0068},
	{RK_GPIO3_B4, RK3588_VCCIO3_5_IOC_REG + 0x006C},
	{RK_GPIO3_C0, RK3588_VCCIO3_5_IOC_REG + 0x0070},
	{RK_GPIO3_C4, RK3588_VCCIO3_5_IOC_REG + 0x0074},
	{RK_GPIO3_D0, RK3588_VCCIO3_5_IOC_REG + 0x0078},
	{RK_GPIO3_D4, RK3588_VCCIO3_5_IOC_REG + 0x007C},
	{RK_GPIO4_A0, RK3588_VCCIO6_IOC_REG + 0x0080},
	{RK_GPIO4_A4, RK3588_VCCIO6_IOC_REG + 0x0084},
	{RK_GPIO4_B0, RK3588_VCCIO6_IOC_REG + 0x0088},
	{RK_GPIO4_B4, RK3588_VCCIO6_IOC_REG + 0x008C},
	{RK_GPIO4_C0, RK3588_VCCIO6_IOC_REG + 0x0090},
	{RK_GPIO4_C2, RK3588_VCCIO3_5_IOC_REG + 0x0090},
	{RK_GPIO4_C4, RK3588_VCCIO3_5_IOC_REG + 0x0094},
	{RK_GPIO4_D0, RK3588_VCCIO2_IOC_REG + 0x0098},
	{RK_GPIO4_D4, RK3588_VCCIO2_IOC_REG + 0x009C},
};

static const u32 rk3588_p_regs[][2] = {
	{RK_GPIO0_A0, RK3588_PMU1_IOC_REG + 0x0020},
	{RK_GPIO0_B0, RK3588_PMU1_IOC_REG + 0x0024},
	{RK_GPIO0_B5, RK3588_PMU2_IOC_REG + 0x0028},
	{RK_GPIO0_C0, RK3588_PMU2_IOC_REG + 0x002C},
	{RK_GPIO0_D0, RK3588_PMU2_IOC_REG + 0x0030},
	{RK_GPIO1_A0, RK3588_VCCIO1_4_IOC_REG + 0x0110},
	{RK_GPIO1_B0, RK3588_VCCIO1_4_IOC_REG + 0x0114},
	{RK_GPIO1_C0, RK3588_VCCIO1_4_IOC_REG + 0x0118},
	{RK_GPIO1_D0, RK3588_VCCIO1_4_IOC_REG + 0x011C},
	{RK_GPIO2_A0, RK3588_EMMC_IOC_REG + 0x0120},
	{RK_GPIO2_A6, RK3588_VCCIO3_5_IOC_REG + 0x0120},
	{RK_GPIO2_B0, RK3588_VCCIO3_5_IOC_REG + 0x0124},
	{RK_GPIO2_C0, RK3588_VCCIO3_5_IOC_REG + 0x0128},
	{RK_GPIO2_D0, RK3588_EMMC_IOC_REG + 0x012C},
	{RK_GPIO3_A0, RK3588_VCCIO3_5_IOC_REG + 0x0130},
	{RK_GPIO3_B0, RK3588_VCCIO3_5_IOC_REG + 0x0134},
	{RK_GPIO3_C0, RK3588_VCCIO3_5_IOC_REG + 0x0138},
	{RK_GPIO3_D0, RK3588_VCCIO3_5_IOC_REG + 0x013C},
	{RK_GPIO4_A0, RK3588_VCCIO6_IOC_REG + 0x0140},
	{RK_GPIO4_B0, RK3588_VCCIO6_IOC_REG + 0x0144},
	{RK_GPIO4_C0, RK3588_VCCIO6_IOC_REG + 0x0148},
	{RK_GPIO4_C2, RK3588_VCCIO3_5_IOC_REG + 0x0148},
	{RK_GPIO4_D0, RK3588_VCCIO2_IOC_REG + 0x014C},
};

static const u32 rk3588_smt_regs[][2] = {
	{RK_GPIO0_A0, RK3588_PMU1_IOC_REG + 0x0030},
	{RK_GPIO0_B0, RK3588_PMU1_IOC_REG + 0x0034},
	{RK_GPIO0_B5, RK3588_PMU2_IOC_REG + 0x0040},
	{RK_GPIO0_C0, RK3588_PMU2_IOC_REG + 0x0044},
	{RK_GPIO0_D0, RK3588_PMU2_IOC_REG + 0x0048},
	{RK_GPIO1_A0, RK3588_VCCIO1_4_IOC_REG + 0x0210},
	{RK_GPIO1_B0, RK3588_VCCIO1_4_IOC_REG + 0x0214},
	{RK_GPIO1_C0, RK3588_VCCIO1_4_IOC_REG + 0x0218},
	{RK_GPIO1_D0, RK3588_VCCIO1_4_IOC_REG + 0x021C},
	{RK_GPIO2_A0, RK3588_EMMC_IOC_REG + 0x0220},
	{RK_GPIO2_A6, RK3588_VCCIO3_5_IOC_REG + 0x0220},
	{RK_GPIO2_B0, RK3588_VCCIO3_5_IOC_REG + 0x0224},
	{RK_GPIO2_C0, RK3588_VCCIO3_5_IOC_REG + 0x0228},
	{RK_GPIO2_D0, RK3588_EMMC_IOC_REG + 0x022C},
	{RK_GPIO3_A0, RK3588_VCCIO3_5_IOC_REG + 0x0230},
	{RK_GPIO3_B0, RK3588_VCCIO3_5_IOC_REG + 0x0234},
	{RK_GPIO3_C0, RK3588_VCCIO3_5_IOC_REG + 0x0238},
	{RK_GPIO3_D0, RK3588_VCCIO3_5_IOC_REG + 0x023C},
	{RK_GPIO4_A0, RK3588_VCCIO6_IOC_REG + 0x0240},
	{RK_GPIO4_B0, RK3588_VCCIO6_IOC_REG + 0x0244},
	{RK_GPIO4_C0, RK3588_VCCIO6_IOC_REG + 0x0248},
	{RK_GPIO4_C2, RK3588_VCCIO3_5_IOC_REG + 0x0248},
	{RK_GPIO4_D0, RK3588_VCCIO2_IOC_REG + 0x024C},
};

#define RK3588_PULL_BITS_PER_PIN		2
#define RK3588_PULL_PINS_PER_REG		8

static int rk3588_calc_pull_reg_and_bit(struct rockchip_pin_bank *bank,
					int pin_num, struct regmap **regmap,
					int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	u8 bank_num = bank->bank_num;
	u32 pin = bank_num * 32 + pin_num;
	int i;

	for (i = ARRAY_SIZE(rk3588_p_regs) - 1; i >= 0; i--) {
		if (pin >= rk3588_p_regs[i][0]) {
			*reg = rk3588_p_regs[i][1];
			*regmap = info->regmap_base;
			*bit = pin_num % RK3588_PULL_PINS_PER_REG;
			*bit *= RK3588_PULL_BITS_PER_PIN;
			return 0;
		}
	}

	return -EINVAL;
}

#define RK3588_DRV_BITS_PER_PIN		4
#define RK3588_DRV_PINS_PER_REG		4

static int rk3588_calc_drv_reg_and_bit(struct rockchip_pin_bank *bank,
				       int pin_num, struct regmap **regmap,
				       int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	u8 bank_num = bank->bank_num;
	u32 pin = bank_num * 32 + pin_num;
	int i;

	for (i = ARRAY_SIZE(rk3588_ds_regs) - 1; i >= 0; i--) {
		if (pin >= rk3588_ds_regs[i][0]) {
			*reg = rk3588_ds_regs[i][1];
			*regmap = info->regmap_base;
			*bit = pin_num % RK3588_DRV_PINS_PER_REG;
			*bit *= RK3588_DRV_BITS_PER_PIN;
			return 0;
		}
	}

	return -EINVAL;
}

#define RK3588_SMT_BITS_PER_PIN		1
#define RK3588_SMT_PINS_PER_REG		8

static int rk3588_calc_schmitt_reg_and_bit(struct rockchip_pin_bank *bank,
					   int pin_num,
					   struct regmap **regmap,
					   int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	u8 bank_num = bank->bank_num;
	u32 pin = bank_num * 32 + pin_num;
	int i;

	for (i = ARRAY_SIZE(rk3588_smt_regs) - 1; i >= 0; i--) {
		if (pin >= rk3588_smt_regs[i][0]) {
			*reg = rk3588_smt_regs[i][1];
			*regmap = info->regmap_base;
			*bit = pin_num % RK3588_SMT_PINS_PER_REG;
			*bit *= RK3588_SMT_BITS_PER_PIN;
			return 0;
		}
	}

	return -EINVAL;
}

static int rockchip_perpin_drv_list[DRV_TYPE_MAX][8] = {
	{ 2, 4, 8, 12, -1, -1, -1, -1 },
	{ 3, 6, 9, 12, -1, -1, -1, -1 },
	{ 5, 10, 15, 20, -1, -1, -1, -1 },
	{ 4, 6, 8, 10, 12, 14, 16, 18 },
	{ 4, 7, 10, 13, 16, 19, 22, 26 }
};

static int rockchip_get_drive_perpin(struct rockchip_pin_bank *bank,
				     int pin_num)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	struct device *dev = info->dev;
	struct regmap *regmap;
	int reg, ret;
	u32 data, temp, rmask_bits;
	u8 bit;
	int drv_type = bank->drv[pin_num / 8].drv_type;

	ret = ctrl->drv_calc_reg(bank, pin_num, &regmap, &reg, &bit);
	if (ret)
		return ret;

	switch (drv_type) {
	case DRV_TYPE_IO_1V8_3V0_AUTO:
	case DRV_TYPE_IO_3V3_ONLY:
		rmask_bits = RK3399_DRV_3BITS_PER_PIN;
		switch (bit) {
		case 0 ... 12:
			 
			break;
		case 15:
			 
			ret = regmap_read(regmap, reg, &data);
			if (ret)
				return ret;

			ret = regmap_read(regmap, reg + 0x4, &temp);
			if (ret)
				return ret;

			 
			data >>= 15;
			temp &= 0x3;
			temp <<= 1;
			data |= temp;

			return rockchip_perpin_drv_list[drv_type][data];
		case 18 ... 21:
			 
			reg += 4;
			bit -= 16;
			break;
		default:
			dev_err(dev, "unsupported bit: %d for pinctrl drive type: %d\n",
				bit, drv_type);
			return -EINVAL;
		}

		break;
	case DRV_TYPE_IO_DEFAULT:
	case DRV_TYPE_IO_1V8_OR_3V0:
	case DRV_TYPE_IO_1V8_ONLY:
		rmask_bits = RK3288_DRV_BITS_PER_PIN;
		break;
	default:
		dev_err(dev, "unsupported pinctrl drive type: %d\n", drv_type);
		return -EINVAL;
	}

	ret = regmap_read(regmap, reg, &data);
	if (ret)
		return ret;

	data >>= bit;
	data &= (1 << rmask_bits) - 1;

	return rockchip_perpin_drv_list[drv_type][data];
}

static int rockchip_set_drive_perpin(struct rockchip_pin_bank *bank,
				     int pin_num, int strength)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	struct device *dev = info->dev;
	struct regmap *regmap;
	int reg, ret, i;
	u32 data, rmask, rmask_bits, temp;
	u8 bit;
	int drv_type = bank->drv[pin_num / 8].drv_type;

	dev_dbg(dev, "setting drive of GPIO%d-%d to %d\n",
		bank->bank_num, pin_num, strength);

	ret = ctrl->drv_calc_reg(bank, pin_num, &regmap, &reg, &bit);
	if (ret)
		return ret;
	if (ctrl->type == RK3588) {
		rmask_bits = RK3588_DRV_BITS_PER_PIN;
		ret = strength;
		goto config;
	} else if (ctrl->type == RK3568) {
		rmask_bits = RK3568_DRV_BITS_PER_PIN;
		ret = (1 << (strength + 1)) - 1;
		goto config;
	}

	if (ctrl->type == RV1126) {
		rmask_bits = RV1126_DRV_BITS_PER_PIN;
		ret = strength;
		goto config;
	}

	ret = -EINVAL;
	for (i = 0; i < ARRAY_SIZE(rockchip_perpin_drv_list[drv_type]); i++) {
		if (rockchip_perpin_drv_list[drv_type][i] == strength) {
			ret = i;
			break;
		} else if (rockchip_perpin_drv_list[drv_type][i] < 0) {
			ret = rockchip_perpin_drv_list[drv_type][i];
			break;
		}
	}

	if (ret < 0) {
		dev_err(dev, "unsupported driver strength %d\n", strength);
		return ret;
	}

	switch (drv_type) {
	case DRV_TYPE_IO_1V8_3V0_AUTO:
	case DRV_TYPE_IO_3V3_ONLY:
		rmask_bits = RK3399_DRV_3BITS_PER_PIN;
		switch (bit) {
		case 0 ... 12:
			 
			break;
		case 15:
			 
			data = (ret & 0x1) << 15;
			temp = (ret >> 0x1) & 0x3;

			rmask = BIT(15) | BIT(31);
			data |= BIT(31);
			ret = regmap_update_bits(regmap, reg, rmask, data);
			if (ret)
				return ret;

			rmask = 0x3 | (0x3 << 16);
			temp |= (0x3 << 16);
			reg += 0x4;
			ret = regmap_update_bits(regmap, reg, rmask, temp);

			return ret;
		case 18 ... 21:
			 
			reg += 4;
			bit -= 16;
			break;
		default:
			dev_err(dev, "unsupported bit: %d for pinctrl drive type: %d\n",
				bit, drv_type);
			return -EINVAL;
		}
		break;
	case DRV_TYPE_IO_DEFAULT:
	case DRV_TYPE_IO_1V8_OR_3V0:
	case DRV_TYPE_IO_1V8_ONLY:
		rmask_bits = RK3288_DRV_BITS_PER_PIN;
		break;
	default:
		dev_err(dev, "unsupported pinctrl drive type: %d\n", drv_type);
		return -EINVAL;
	}

config:
	 
	data = ((1 << rmask_bits) - 1) << (bit + 16);
	rmask = data | (data >> 16);
	data |= (ret << bit);

	ret = regmap_update_bits(regmap, reg, rmask, data);

	return ret;
}

static int rockchip_pull_list[PULL_TYPE_MAX][4] = {
	{
		PIN_CONFIG_BIAS_DISABLE,
		PIN_CONFIG_BIAS_PULL_UP,
		PIN_CONFIG_BIAS_PULL_DOWN,
		PIN_CONFIG_BIAS_BUS_HOLD
	},
	{
		PIN_CONFIG_BIAS_DISABLE,
		PIN_CONFIG_BIAS_PULL_DOWN,
		PIN_CONFIG_BIAS_DISABLE,
		PIN_CONFIG_BIAS_PULL_UP
	},
};

static int rockchip_get_pull(struct rockchip_pin_bank *bank, int pin_num)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	struct device *dev = info->dev;
	struct regmap *regmap;
	int reg, ret, pull_type;
	u8 bit;
	u32 data;

	 
	if (ctrl->type == RK3066B)
		return PIN_CONFIG_BIAS_DISABLE;

	ret = ctrl->pull_calc_reg(bank, pin_num, &regmap, &reg, &bit);
	if (ret)
		return ret;

	ret = regmap_read(regmap, reg, &data);
	if (ret)
		return ret;

	switch (ctrl->type) {
	case RK2928:
	case RK3128:
		return !(data & BIT(bit))
				? PIN_CONFIG_BIAS_PULL_PIN_DEFAULT
				: PIN_CONFIG_BIAS_DISABLE;
	case PX30:
	case RV1108:
	case RK3188:
	case RK3288:
	case RK3308:
	case RK3368:
	case RK3399:
	case RK3568:
	case RK3588:
		pull_type = bank->pull_type[pin_num / 8];
		data >>= bit;
		data &= (1 << RK3188_PULL_BITS_PER_PIN) - 1;
		 
		if (ctrl->type == RK3568 && bank->bank_num == 0 && pin_num >= 27 && pin_num <= 30) {
			if (data == 3)
				data = 1;
		}

		return rockchip_pull_list[pull_type][data];
	default:
		dev_err(dev, "unsupported pinctrl type\n");
		return -EINVAL;
	};
}

static int rockchip_set_pull(struct rockchip_pin_bank *bank,
					int pin_num, int pull)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	struct device *dev = info->dev;
	struct regmap *regmap;
	int reg, ret, i, pull_type;
	u8 bit;
	u32 data, rmask;

	dev_dbg(dev, "setting pull of GPIO%d-%d to %d\n", bank->bank_num, pin_num, pull);

	 
	if (ctrl->type == RK3066B)
		return pull ? -EINVAL : 0;

	ret = ctrl->pull_calc_reg(bank, pin_num, &regmap, &reg, &bit);
	if (ret)
		return ret;

	switch (ctrl->type) {
	case RK2928:
	case RK3128:
		data = BIT(bit + 16);
		if (pull == PIN_CONFIG_BIAS_DISABLE)
			data |= BIT(bit);
		ret = regmap_write(regmap, reg, data);
		break;
	case PX30:
	case RV1108:
	case RV1126:
	case RK3188:
	case RK3288:
	case RK3308:
	case RK3368:
	case RK3399:
	case RK3568:
	case RK3588:
		pull_type = bank->pull_type[pin_num / 8];
		ret = -EINVAL;
		for (i = 0; i < ARRAY_SIZE(rockchip_pull_list[pull_type]);
			i++) {
			if (rockchip_pull_list[pull_type][i] == pull) {
				ret = i;
				break;
			}
		}
		 
		if (ctrl->type == RK3568 && bank->bank_num == 0 && pin_num >= 27 && pin_num <= 30) {
			if (ret == 1)
				ret = 3;
		}

		if (ret < 0) {
			dev_err(dev, "unsupported pull setting %d\n", pull);
			return ret;
		}

		 
		data = ((1 << RK3188_PULL_BITS_PER_PIN) - 1) << (bit + 16);
		rmask = data | (data >> 16);
		data |= (ret << bit);

		ret = regmap_update_bits(regmap, reg, rmask, data);
		break;
	default:
		dev_err(dev, "unsupported pinctrl type\n");
		return -EINVAL;
	}

	return ret;
}

#define RK3328_SCHMITT_BITS_PER_PIN		1
#define RK3328_SCHMITT_PINS_PER_REG		16
#define RK3328_SCHMITT_BANK_STRIDE		8
#define RK3328_SCHMITT_GRF_OFFSET		0x380

static int rk3328_calc_schmitt_reg_and_bit(struct rockchip_pin_bank *bank,
					   int pin_num,
					   struct regmap **regmap,
					   int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	*regmap = info->regmap_base;
	*reg = RK3328_SCHMITT_GRF_OFFSET;

	*reg += bank->bank_num * RK3328_SCHMITT_BANK_STRIDE;
	*reg += ((pin_num / RK3328_SCHMITT_PINS_PER_REG) * 4);
	*bit = pin_num % RK3328_SCHMITT_PINS_PER_REG;

	return 0;
}

#define RK3568_SCHMITT_BITS_PER_PIN		2
#define RK3568_SCHMITT_PINS_PER_REG		8
#define RK3568_SCHMITT_BANK_STRIDE		0x10
#define RK3568_SCHMITT_GRF_OFFSET		0xc0
#define RK3568_SCHMITT_PMUGRF_OFFSET		0x30

static int rk3568_calc_schmitt_reg_and_bit(struct rockchip_pin_bank *bank,
					   int pin_num,
					   struct regmap **regmap,
					   int *reg, u8 *bit)
{
	struct rockchip_pinctrl *info = bank->drvdata;

	if (bank->bank_num == 0) {
		*regmap = info->regmap_pmu;
		*reg = RK3568_SCHMITT_PMUGRF_OFFSET;
	} else {
		*regmap = info->regmap_base;
		*reg = RK3568_SCHMITT_GRF_OFFSET;
		*reg += (bank->bank_num - 1) * RK3568_SCHMITT_BANK_STRIDE;
	}

	*reg += ((pin_num / RK3568_SCHMITT_PINS_PER_REG) * 4);
	*bit = pin_num % RK3568_SCHMITT_PINS_PER_REG;
	*bit *= RK3568_SCHMITT_BITS_PER_PIN;

	return 0;
}

static int rockchip_get_schmitt(struct rockchip_pin_bank *bank, int pin_num)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	struct regmap *regmap;
	int reg, ret;
	u8 bit;
	u32 data;

	ret = ctrl->schmitt_calc_reg(bank, pin_num, &regmap, &reg, &bit);
	if (ret)
		return ret;

	ret = regmap_read(regmap, reg, &data);
	if (ret)
		return ret;

	data >>= bit;
	switch (ctrl->type) {
	case RK3568:
		return data & ((1 << RK3568_SCHMITT_BITS_PER_PIN) - 1);
	default:
		break;
	}

	return data & 0x1;
}

static int rockchip_set_schmitt(struct rockchip_pin_bank *bank,
				int pin_num, int enable)
{
	struct rockchip_pinctrl *info = bank->drvdata;
	struct rockchip_pin_ctrl *ctrl = info->ctrl;
	struct device *dev = info->dev;
	struct regmap *regmap;
	int reg, ret;
	u8 bit;
	u32 data, rmask;

	dev_dbg(dev, "setting input schmitt of GPIO%d-%d to %d\n",
		bank->bank_num, pin_num, enable);

	ret = ctrl->schmitt_calc_reg(bank, pin_num, &regmap, &reg, &bit);
	if (ret)
		return ret;

	 
	switch (ctrl->type) {
	case RK3568:
		data = ((1 << RK3568_SCHMITT_BITS_PER_PIN) - 1) << (bit + 16);
		rmask = data | (data >> 16);
		data |= ((enable ? 0x2 : 0x1) << bit);
		break;
	default:
		data = BIT(bit + 16) | (enable << bit);
		rmask = BIT(bit + 16) | BIT(bit);
		break;
	}

	return regmap_update_bits(regmap, reg, rmask, data);
}

 

static int rockchip_pmx_get_funcs_count(struct pinctrl_dev *pctldev)
{
	struct rockchip_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	return info->nfunctions;
}

static const char *rockchip_pmx_get_func_name(struct pinctrl_dev *pctldev,
					  unsigned selector)
{
	struct rockchip_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	return info->functions[selector].name;
}

static int rockchip_pmx_get_groups(struct pinctrl_dev *pctldev,
				unsigned selector, const char * const **groups,
				unsigned * const num_groups)
{
	struct rockchip_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	*groups = info->functions[selector].groups;
	*num_groups = info->functions[selector].ngroups;

	return 0;
}

static int rockchip_pmx_set(struct pinctrl_dev *pctldev, unsigned selector,
			    unsigned group)
{
	struct rockchip_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);
	const unsigned int *pins = info->groups[group].pins;
	const struct rockchip_pin_config *data = info->groups[group].data;
	struct device *dev = info->dev;
	struct rockchip_pin_bank *bank;
	int cnt, ret = 0;

	dev_dbg(dev, "enable function %s group %s\n",
		info->functions[selector].name, info->groups[group].name);

	 
	for (cnt = 0; cnt < info->groups[group].npins; cnt++) {
		bank = pin_to_bank(info, pins[cnt]);
		ret = rockchip_set_mux(bank, pins[cnt] - bank->pin_base,
				       data[cnt].func);
		if (ret)
			break;
	}

	if (ret) {
		 
		for (cnt--; cnt >= 0; cnt--)
			rockchip_set_mux(bank, pins[cnt] - bank->pin_base, 0);

		return ret;
	}

	return 0;
}

static int rockchip_pmx_gpio_set_direction(struct pinctrl_dev *pctldev,
					   struct pinctrl_gpio_range *range,
					   unsigned offset,
					   bool input)
{
	struct rockchip_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);
	struct rockchip_pin_bank *bank;

	bank = pin_to_bank(info, offset);
	return rockchip_set_mux(bank, offset - bank->pin_base, RK_FUNC_GPIO);
}

static const struct pinmux_ops rockchip_pmx_ops = {
	.get_functions_count	= rockchip_pmx_get_funcs_count,
	.get_function_name	= rockchip_pmx_get_func_name,
	.get_function_groups	= rockchip_pmx_get_groups,
	.set_mux		= rockchip_pmx_set,
	.gpio_set_direction	= rockchip_pmx_gpio_set_direction,
};

 

static bool rockchip_pinconf_pull_valid(struct rockchip_pin_ctrl *ctrl,
					enum pin_config_param pull)
{
	switch (ctrl->type) {
	case RK2928:
	case RK3128:
		return (pull == PIN_CONFIG_BIAS_PULL_PIN_DEFAULT ||
					pull == PIN_CONFIG_BIAS_DISABLE);
	case RK3066B:
		return pull ? false : true;
	case PX30:
	case RV1108:
	case RV1126:
	case RK3188:
	case RK3288:
	case RK3308:
	case RK3368:
	case RK3399:
	case RK3568:
	case RK3588:
		return (pull != PIN_CONFIG_BIAS_PULL_PIN_DEFAULT);
	}

	return false;
}

static int rockchip_pinconf_defer_pin(struct rockchip_pin_bank *bank,
					 unsigned int pin, u32 param, u32 arg)
{
	struct rockchip_pin_deferred *cfg;

	cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;

	cfg->pin = pin;
	cfg->param = param;
	cfg->arg = arg;

	list_add_tail(&cfg->head, &bank->deferred_pins);

	return 0;
}

 
static int rockchip_pinconf_set(struct pinctrl_dev *pctldev, unsigned int pin,
				unsigned long *configs, unsigned num_configs)
{
	struct rockchip_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);
	struct rockchip_pin_bank *bank = pin_to_bank(info, pin);
	struct gpio_chip *gpio = &bank->gpio_chip;
	enum pin_config_param param;
	u32 arg;
	int i;
	int rc;

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);

		if (param == PIN_CONFIG_OUTPUT || param == PIN_CONFIG_INPUT_ENABLE) {
			 
			mutex_lock(&bank->deferred_lock);
			if (!gpio || !gpio->direction_output) {
				rc = rockchip_pinconf_defer_pin(bank, pin - bank->pin_base, param,
								arg);
				mutex_unlock(&bank->deferred_lock);
				if (rc)
					return rc;

				break;
			}
			mutex_unlock(&bank->deferred_lock);
		}

		switch (param) {
		case PIN_CONFIG_BIAS_DISABLE:
			rc =  rockchip_set_pull(bank, pin - bank->pin_base,
				param);
			if (rc)
				return rc;
			break;
		case PIN_CONFIG_BIAS_PULL_UP:
		case PIN_CONFIG_BIAS_PULL_DOWN:
		case PIN_CONFIG_BIAS_PULL_PIN_DEFAULT:
		case PIN_CONFIG_BIAS_BUS_HOLD:
			if (!rockchip_pinconf_pull_valid(info->ctrl, param))
				return -ENOTSUPP;

			if (!arg)
				return -EINVAL;

			rc = rockchip_set_pull(bank, pin - bank->pin_base,
				param);
			if (rc)
				return rc;
			break;
		case PIN_CONFIG_OUTPUT:
			rc = rockchip_set_mux(bank, pin - bank->pin_base,
					      RK_FUNC_GPIO);
			if (rc != RK_FUNC_GPIO)
				return -EINVAL;

			rc = gpio->direction_output(gpio, pin - bank->pin_base,
						    arg);
			if (rc)
				return rc;
			break;
		case PIN_CONFIG_INPUT_ENABLE:
			rc = rockchip_set_mux(bank, pin - bank->pin_base,
					      RK_FUNC_GPIO);
			if (rc != RK_FUNC_GPIO)
				return -EINVAL;

			rc = gpio->direction_input(gpio, pin - bank->pin_base);
			if (rc)
				return rc;
			break;
		case PIN_CONFIG_DRIVE_STRENGTH:
			 
			if (!info->ctrl->drv_calc_reg)
				return -ENOTSUPP;

			rc = rockchip_set_drive_perpin(bank,
						pin - bank->pin_base, arg);
			if (rc < 0)
				return rc;
			break;
		case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
			if (!info->ctrl->schmitt_calc_reg)
				return -ENOTSUPP;

			rc = rockchip_set_schmitt(bank,
						  pin - bank->pin_base, arg);
			if (rc < 0)
				return rc;
			break;
		default:
			return -ENOTSUPP;
			break;
		}
	}  

	return 0;
}

 
static int rockchip_pinconf_get(struct pinctrl_dev *pctldev, unsigned int pin,
							unsigned long *config)
{
	struct rockchip_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);
	struct rockchip_pin_bank *bank = pin_to_bank(info, pin);
	struct gpio_chip *gpio = &bank->gpio_chip;
	enum pin_config_param param = pinconf_to_config_param(*config);
	u16 arg;
	int rc;

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
		if (rockchip_get_pull(bank, pin - bank->pin_base) != param)
			return -EINVAL;

		arg = 0;
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_PIN_DEFAULT:
	case PIN_CONFIG_BIAS_BUS_HOLD:
		if (!rockchip_pinconf_pull_valid(info->ctrl, param))
			return -ENOTSUPP;

		if (rockchip_get_pull(bank, pin - bank->pin_base) != param)
			return -EINVAL;

		arg = 1;
		break;
	case PIN_CONFIG_OUTPUT:
		rc = rockchip_get_mux(bank, pin - bank->pin_base);
		if (rc != RK_FUNC_GPIO)
			return -EINVAL;

		if (!gpio || !gpio->get) {
			arg = 0;
			break;
		}

		rc = gpio->get(gpio, pin - bank->pin_base);
		if (rc < 0)
			return rc;

		arg = rc ? 1 : 0;
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		 
		if (!info->ctrl->drv_calc_reg)
			return -ENOTSUPP;

		rc = rockchip_get_drive_perpin(bank, pin - bank->pin_base);
		if (rc < 0)
			return rc;

		arg = rc;
		break;
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		if (!info->ctrl->schmitt_calc_reg)
			return -ENOTSUPP;

		rc = rockchip_get_schmitt(bank, pin - bank->pin_base);
		if (rc < 0)
			return rc;

		arg = rc;
		break;
	default:
		return -ENOTSUPP;
		break;
	}

	*config = pinconf_to_config_packed(param, arg);

	return 0;
}

static const struct pinconf_ops rockchip_pinconf_ops = {
	.pin_config_get			= rockchip_pinconf_get,
	.pin_config_set			= rockchip_pinconf_set,
	.is_generic			= true,
};

static const struct of_device_id rockchip_bank_match[] = {
	{ .compatible = "rockchip,gpio-bank" },
	{ .compatible = "rockchip,rk3188-gpio-bank0" },
	{},
};

static void rockchip_pinctrl_child_count(struct rockchip_pinctrl *info,
						struct device_node *np)
{
	struct device_node *child;

	for_each_child_of_node(np, child) {
		if (of_match_node(rockchip_bank_match, child))
			continue;

		info->nfunctions++;
		info->ngroups += of_get_child_count(child);
	}
}

static int rockchip_pinctrl_parse_groups(struct device_node *np,
					      struct rockchip_pin_group *grp,
					      struct rockchip_pinctrl *info,
					      u32 index)
{
	struct device *dev = info->dev;
	struct rockchip_pin_bank *bank;
	int size;
	const __be32 *list;
	int num;
	int i, j;
	int ret;

	dev_dbg(dev, "group(%d): %pOFn\n", index, np);

	 
	grp->name = np->name;

	 
	list = of_get_property(np, "rockchip,pins", &size);
	 
	size /= sizeof(*list);
	if (!size || size % 4)
		return dev_err_probe(dev, -EINVAL, "wrong pins number or pins and configs should be by 4\n");

	grp->npins = size / 4;

	grp->pins = devm_kcalloc(dev, grp->npins, sizeof(*grp->pins), GFP_KERNEL);
	grp->data = devm_kcalloc(dev, grp->npins, sizeof(*grp->data), GFP_KERNEL);
	if (!grp->pins || !grp->data)
		return -ENOMEM;

	for (i = 0, j = 0; i < size; i += 4, j++) {
		const __be32 *phandle;
		struct device_node *np_config;

		num = be32_to_cpu(*list++);
		bank = bank_num_to_bank(info, num);
		if (IS_ERR(bank))
			return PTR_ERR(bank);

		grp->pins[j] = bank->pin_base + be32_to_cpu(*list++);
		grp->data[j].func = be32_to_cpu(*list++);

		phandle = list++;
		if (!phandle)
			return -EINVAL;

		np_config = of_find_node_by_phandle(be32_to_cpup(phandle));
		ret = pinconf_generic_parse_dt_config(np_config, NULL,
				&grp->data[j].configs, &grp->data[j].nconfigs);
		of_node_put(np_config);
		if (ret)
			return ret;
	}

	return 0;
}

static int rockchip_pinctrl_parse_functions(struct device_node *np,
						struct rockchip_pinctrl *info,
						u32 index)
{
	struct device *dev = info->dev;
	struct device_node *child;
	struct rockchip_pmx_func *func;
	struct rockchip_pin_group *grp;
	int ret;
	static u32 grp_index;
	u32 i = 0;

	dev_dbg(dev, "parse function(%d): %pOFn\n", index, np);

	func = &info->functions[index];

	 
	func->name = np->name;
	func->ngroups = of_get_child_count(np);
	if (func->ngroups <= 0)
		return 0;

	func->groups = devm_kcalloc(dev, func->ngroups, sizeof(*func->groups), GFP_KERNEL);
	if (!func->groups)
		return -ENOMEM;

	for_each_child_of_node(np, child) {
		func->groups[i] = child->name;
		grp = &info->groups[grp_index++];
		ret = rockchip_pinctrl_parse_groups(child, grp, info, i++);
		if (ret) {
			of_node_put(child);
			return ret;
		}
	}

	return 0;
}

static int rockchip_pinctrl_parse_dt(struct platform_device *pdev,
					      struct rockchip_pinctrl *info)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child;
	int ret;
	int i;

	rockchip_pinctrl_child_count(info, np);

	dev_dbg(dev, "nfunctions = %d\n", info->nfunctions);
	dev_dbg(dev, "ngroups = %d\n", info->ngroups);

	info->functions = devm_kcalloc(dev, info->nfunctions, sizeof(*info->functions), GFP_KERNEL);
	if (!info->functions)
		return -ENOMEM;

	info->groups = devm_kcalloc(dev, info->ngroups, sizeof(*info->groups), GFP_KERNEL);
	if (!info->groups)
		return -ENOMEM;

	i = 0;

	for_each_child_of_node(np, child) {
		if (of_match_node(rockchip_bank_match, child))
			continue;

		ret = rockchip_pinctrl_parse_functions(child, info, i++);
		if (ret) {
			dev_err(dev, "failed to parse function\n");
			of_node_put(child);
			return ret;
		}
	}

	return 0;
}

static int rockchip_pinctrl_register(struct platform_device *pdev,
					struct rockchip_pinctrl *info)
{
	struct pinctrl_desc *ctrldesc = &info->pctl;
	struct pinctrl_pin_desc *pindesc, *pdesc;
	struct rockchip_pin_bank *pin_bank;
	struct device *dev = &pdev->dev;
	char **pin_names;
	int pin, bank, ret;
	int k;

	ctrldesc->name = "rockchip-pinctrl";
	ctrldesc->owner = THIS_MODULE;
	ctrldesc->pctlops = &rockchip_pctrl_ops;
	ctrldesc->pmxops = &rockchip_pmx_ops;
	ctrldesc->confops = &rockchip_pinconf_ops;

	pindesc = devm_kcalloc(dev, info->ctrl->nr_pins, sizeof(*pindesc), GFP_KERNEL);
	if (!pindesc)
		return -ENOMEM;

	ctrldesc->pins = pindesc;
	ctrldesc->npins = info->ctrl->nr_pins;

	pdesc = pindesc;
	for (bank = 0, k = 0; bank < info->ctrl->nr_banks; bank++) {
		pin_bank = &info->ctrl->pin_banks[bank];

		pin_names = devm_kasprintf_strarray(dev, pin_bank->name, pin_bank->nr_pins);
		if (IS_ERR(pin_names))
			return PTR_ERR(pin_names);

		for (pin = 0; pin < pin_bank->nr_pins; pin++, k++) {
			pdesc->number = k;
			pdesc->name = pin_names[pin];
			pdesc++;
		}

		INIT_LIST_HEAD(&pin_bank->deferred_pins);
		mutex_init(&pin_bank->deferred_lock);
	}

	ret = rockchip_pinctrl_parse_dt(pdev, info);
	if (ret)
		return ret;

	info->pctl_dev = devm_pinctrl_register(dev, ctrldesc, info);
	if (IS_ERR(info->pctl_dev))
		return dev_err_probe(dev, PTR_ERR(info->pctl_dev), "could not register pinctrl driver\n");

	return 0;
}

static const struct of_device_id rockchip_pinctrl_dt_match[];

 
static struct rockchip_pin_ctrl *rockchip_pinctrl_get_soc_data(
						struct rockchip_pinctrl *d,
						struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	const struct of_device_id *match;
	struct rockchip_pin_ctrl *ctrl;
	struct rockchip_pin_bank *bank;
	int grf_offs, pmu_offs, drv_grf_offs, drv_pmu_offs, i, j;

	match = of_match_node(rockchip_pinctrl_dt_match, node);
	ctrl = (struct rockchip_pin_ctrl *)match->data;

	grf_offs = ctrl->grf_mux_offset;
	pmu_offs = ctrl->pmu_mux_offset;
	drv_pmu_offs = ctrl->pmu_drv_offset;
	drv_grf_offs = ctrl->grf_drv_offset;
	bank = ctrl->pin_banks;
	for (i = 0; i < ctrl->nr_banks; ++i, ++bank) {
		int bank_pins = 0;

		raw_spin_lock_init(&bank->slock);
		bank->drvdata = d;
		bank->pin_base = ctrl->nr_pins;
		ctrl->nr_pins += bank->nr_pins;

		 
		for (j = 0; j < 4; j++) {
			struct rockchip_iomux *iom = &bank->iomux[j];
			struct rockchip_drv *drv = &bank->drv[j];
			int inc;

			if (bank_pins >= bank->nr_pins)
				break;

			 
			if (iom->offset >= 0) {
				if ((iom->type & IOMUX_SOURCE_PMU) ||
				    (iom->type & IOMUX_L_SOURCE_PMU))
					pmu_offs = iom->offset;
				else
					grf_offs = iom->offset;
			} else {  
				iom->offset = ((iom->type & IOMUX_SOURCE_PMU) ||
					       (iom->type & IOMUX_L_SOURCE_PMU)) ?
							pmu_offs : grf_offs;
			}

			 
			if (drv->offset >= 0) {
				if (iom->type & IOMUX_SOURCE_PMU)
					drv_pmu_offs = drv->offset;
				else
					drv_grf_offs = drv->offset;
			} else {  
				drv->offset = (iom->type & IOMUX_SOURCE_PMU) ?
						drv_pmu_offs : drv_grf_offs;
			}

			dev_dbg(dev, "bank %d, iomux %d has iom_offset 0x%x drv_offset 0x%x\n",
				i, j, iom->offset, drv->offset);

			 
			inc = (iom->type & (IOMUX_WIDTH_4BIT |
					    IOMUX_WIDTH_3BIT |
					    IOMUX_WIDTH_2BIT)) ? 8 : 4;
			if ((iom->type & IOMUX_SOURCE_PMU) || (iom->type & IOMUX_L_SOURCE_PMU))
				pmu_offs += inc;
			else
				grf_offs += inc;

			 
			if ((drv->drv_type == DRV_TYPE_IO_1V8_3V0_AUTO) ||
			    (drv->drv_type == DRV_TYPE_IO_3V3_ONLY))
				inc = 8;
			else
				inc = 4;

			if (iom->type & IOMUX_SOURCE_PMU)
				drv_pmu_offs += inc;
			else
				drv_grf_offs += inc;

			bank_pins += 8;
		}

		 
		for (j = 0; j < ctrl->niomux_recalced; j++) {
			int pin = 0;

			if (ctrl->iomux_recalced[j].num == bank->bank_num) {
				pin = ctrl->iomux_recalced[j].pin;
				bank->recalced_mask |= BIT(pin);
			}
		}

		 
		for (j = 0; j < ctrl->niomux_routes; j++) {
			int pin = 0;

			if (ctrl->iomux_routes[j].bank_num == bank->bank_num) {
				pin = ctrl->iomux_routes[j].pin;
				bank->route_mask |= BIT(pin);
			}
		}
	}

	return ctrl;
}

#define RK3288_GRF_GPIO6C_IOMUX		0x64
#define GPIO6C6_SEL_WRITE_ENABLE	BIT(28)

static u32 rk3288_grf_gpio6c_iomux;

static int __maybe_unused rockchip_pinctrl_suspend(struct device *dev)
{
	struct rockchip_pinctrl *info = dev_get_drvdata(dev);
	int ret = pinctrl_force_sleep(info->pctl_dev);

	if (ret)
		return ret;

	 
	if (info->ctrl->type == RK3288) {
		ret = regmap_read(info->regmap_base, RK3288_GRF_GPIO6C_IOMUX,
				  &rk3288_grf_gpio6c_iomux);
		if (ret) {
			pinctrl_force_default(info->pctl_dev);
			return ret;
		}
	}

	return 0;
}

static int __maybe_unused rockchip_pinctrl_resume(struct device *dev)
{
	struct rockchip_pinctrl *info = dev_get_drvdata(dev);
	int ret;

	if (info->ctrl->type == RK3288) {
		ret = regmap_write(info->regmap_base, RK3288_GRF_GPIO6C_IOMUX,
				   rk3288_grf_gpio6c_iomux |
				   GPIO6C6_SEL_WRITE_ENABLE);
		if (ret)
			return ret;
	}

	return pinctrl_force_default(info->pctl_dev);
}

static SIMPLE_DEV_PM_OPS(rockchip_pinctrl_dev_pm_ops, rockchip_pinctrl_suspend,
			 rockchip_pinctrl_resume);

static int rockchip_pinctrl_probe(struct platform_device *pdev)
{
	struct rockchip_pinctrl *info;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node, *node;
	struct rockchip_pin_ctrl *ctrl;
	struct resource *res;
	void __iomem *base;
	int ret;

	if (!dev->of_node)
		return dev_err_probe(dev, -ENODEV, "device tree node not found\n");

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;

	ctrl = rockchip_pinctrl_get_soc_data(info, pdev);
	if (!ctrl)
		return dev_err_probe(dev, -EINVAL, "driver data not available\n");
	info->ctrl = ctrl;

	node = of_parse_phandle(np, "rockchip,grf", 0);
	if (node) {
		info->regmap_base = syscon_node_to_regmap(node);
		of_node_put(node);
		if (IS_ERR(info->regmap_base))
			return PTR_ERR(info->regmap_base);
	} else {
		base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
		if (IS_ERR(base))
			return PTR_ERR(base);

		rockchip_regmap_config.max_register = resource_size(res) - 4;
		rockchip_regmap_config.name = "rockchip,pinctrl";
		info->regmap_base =
			devm_regmap_init_mmio(dev, base, &rockchip_regmap_config);

		 
		info->reg_size = resource_size(res);

		 
		if (ctrl->type == RK3188 && info->reg_size < 0x200) {
			base = devm_platform_get_and_ioremap_resource(pdev, 1, &res);
			if (IS_ERR(base))
				return PTR_ERR(base);

			rockchip_regmap_config.max_register = resource_size(res) - 4;
			rockchip_regmap_config.name = "rockchip,pinctrl-pull";
			info->regmap_pull =
				devm_regmap_init_mmio(dev, base, &rockchip_regmap_config);
		}
	}

	 
	node = of_parse_phandle(np, "rockchip,pmu", 0);
	if (node) {
		info->regmap_pmu = syscon_node_to_regmap(node);
		of_node_put(node);
		if (IS_ERR(info->regmap_pmu))
			return PTR_ERR(info->regmap_pmu);
	}

	ret = rockchip_pinctrl_register(pdev, info);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, info);

	ret = of_platform_populate(np, NULL, NULL, &pdev->dev);
	if (ret)
		return dev_err_probe(dev, ret, "failed to register gpio device\n");

	return 0;
}

static int rockchip_pinctrl_remove(struct platform_device *pdev)
{
	struct rockchip_pinctrl *info = platform_get_drvdata(pdev);
	struct rockchip_pin_bank *bank;
	struct rockchip_pin_deferred *cfg;
	int i;

	of_platform_depopulate(&pdev->dev);

	for (i = 0; i < info->ctrl->nr_banks; i++) {
		bank = &info->ctrl->pin_banks[i];

		mutex_lock(&bank->deferred_lock);
		while (!list_empty(&bank->deferred_pins)) {
			cfg = list_first_entry(&bank->deferred_pins,
					       struct rockchip_pin_deferred, head);
			list_del(&cfg->head);
			kfree(cfg);
		}
		mutex_unlock(&bank->deferred_lock);
	}

	return 0;
}

static struct rockchip_pin_bank px30_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU
			    ),
	PIN_BANK_IOMUX_FLAGS(1, 32, "gpio1", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT
			    ),
	PIN_BANK_IOMUX_FLAGS(2, 32, "gpio2", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT
			    ),
	PIN_BANK_IOMUX_FLAGS(3, 32, "gpio3", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT
			    ),
};

static struct rockchip_pin_ctrl px30_pin_ctrl = {
		.pin_banks		= px30_pin_banks,
		.nr_banks		= ARRAY_SIZE(px30_pin_banks),
		.label			= "PX30-GPIO",
		.type			= PX30,
		.grf_mux_offset		= 0x0,
		.pmu_mux_offset		= 0x0,
		.iomux_routes		= px30_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(px30_mux_route_data),
		.pull_calc_reg		= px30_calc_pull_reg_and_bit,
		.drv_calc_reg		= px30_calc_drv_reg_and_bit,
		.schmitt_calc_reg	= px30_calc_schmitt_reg_and_bit,
};

static struct rockchip_pin_bank rv1108_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU),
	PIN_BANK_IOMUX_FLAGS(1, 32, "gpio1", 0, 0, 0, 0),
	PIN_BANK_IOMUX_FLAGS(2, 32, "gpio2", 0, 0, 0, 0),
	PIN_BANK_IOMUX_FLAGS(3, 32, "gpio3", 0, 0, 0, 0),
};

static struct rockchip_pin_ctrl rv1108_pin_ctrl = {
	.pin_banks		= rv1108_pin_banks,
	.nr_banks		= ARRAY_SIZE(rv1108_pin_banks),
	.label			= "RV1108-GPIO",
	.type			= RV1108,
	.grf_mux_offset		= 0x10,
	.pmu_mux_offset		= 0x0,
	.iomux_recalced		= rv1108_mux_recalced_data,
	.niomux_recalced	= ARRAY_SIZE(rv1108_mux_recalced_data),
	.pull_calc_reg		= rv1108_calc_pull_reg_and_bit,
	.drv_calc_reg		= rv1108_calc_drv_reg_and_bit,
	.schmitt_calc_reg	= rv1108_calc_schmitt_reg_and_bit,
};

static struct rockchip_pin_bank rv1126_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0",
			     IOMUX_WIDTH_4BIT | IOMUX_SOURCE_PMU,
			     IOMUX_WIDTH_4BIT | IOMUX_SOURCE_PMU,
			     IOMUX_WIDTH_4BIT | IOMUX_L_SOURCE_PMU,
			     IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS_OFFSET(1, 32, "gpio1",
				    IOMUX_WIDTH_4BIT,
				    IOMUX_WIDTH_4BIT,
				    IOMUX_WIDTH_4BIT,
				    IOMUX_WIDTH_4BIT,
				    0x10010, 0x10018, 0x10020, 0x10028),
	PIN_BANK_IOMUX_FLAGS(2, 32, "gpio2",
			     IOMUX_WIDTH_4BIT,
			     IOMUX_WIDTH_4BIT,
			     IOMUX_WIDTH_4BIT,
			     IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS(3, 32, "gpio3",
			     IOMUX_WIDTH_4BIT,
			     IOMUX_WIDTH_4BIT,
			     IOMUX_WIDTH_4BIT,
			     IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS(4, 2, "gpio4",
			     IOMUX_WIDTH_4BIT, 0, 0, 0),
};

static struct rockchip_pin_ctrl rv1126_pin_ctrl = {
	.pin_banks		= rv1126_pin_banks,
	.nr_banks		= ARRAY_SIZE(rv1126_pin_banks),
	.label			= "RV1126-GPIO",
	.type			= RV1126,
	.grf_mux_offset		= 0x10004,  
	.pmu_mux_offset		= 0x0,
	.iomux_routes		= rv1126_mux_route_data,
	.niomux_routes		= ARRAY_SIZE(rv1126_mux_route_data),
	.iomux_recalced		= rv1126_mux_recalced_data,
	.niomux_recalced	= ARRAY_SIZE(rv1126_mux_recalced_data),
	.pull_calc_reg		= rv1126_calc_pull_reg_and_bit,
	.drv_calc_reg		= rv1126_calc_drv_reg_and_bit,
	.schmitt_calc_reg	= rv1126_calc_schmitt_reg_and_bit,
};

static struct rockchip_pin_bank rk2928_pin_banks[] = {
	PIN_BANK(0, 32, "gpio0"),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
	PIN_BANK(3, 32, "gpio3"),
};

static struct rockchip_pin_ctrl rk2928_pin_ctrl = {
		.pin_banks		= rk2928_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk2928_pin_banks),
		.label			= "RK2928-GPIO",
		.type			= RK2928,
		.grf_mux_offset		= 0xa8,
		.pull_calc_reg		= rk2928_calc_pull_reg_and_bit,
};

static struct rockchip_pin_bank rk3036_pin_banks[] = {
	PIN_BANK(0, 32, "gpio0"),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
};

static struct rockchip_pin_ctrl rk3036_pin_ctrl = {
		.pin_banks		= rk3036_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3036_pin_banks),
		.label			= "RK3036-GPIO",
		.type			= RK2928,
		.grf_mux_offset		= 0xa8,
		.pull_calc_reg		= rk2928_calc_pull_reg_and_bit,
};

static struct rockchip_pin_bank rk3066a_pin_banks[] = {
	PIN_BANK(0, 32, "gpio0"),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
	PIN_BANK(3, 32, "gpio3"),
	PIN_BANK(4, 32, "gpio4"),
	PIN_BANK(6, 16, "gpio6"),
};

static struct rockchip_pin_ctrl rk3066a_pin_ctrl = {
		.pin_banks		= rk3066a_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3066a_pin_banks),
		.label			= "RK3066a-GPIO",
		.type			= RK2928,
		.grf_mux_offset		= 0xa8,
		.pull_calc_reg		= rk2928_calc_pull_reg_and_bit,
};

static struct rockchip_pin_bank rk3066b_pin_banks[] = {
	PIN_BANK(0, 32, "gpio0"),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
	PIN_BANK(3, 32, "gpio3"),
};

static struct rockchip_pin_ctrl rk3066b_pin_ctrl = {
		.pin_banks	= rk3066b_pin_banks,
		.nr_banks	= ARRAY_SIZE(rk3066b_pin_banks),
		.label		= "RK3066b-GPIO",
		.type		= RK3066B,
		.grf_mux_offset	= 0x60,
};

static struct rockchip_pin_bank rk3128_pin_banks[] = {
	PIN_BANK(0, 32, "gpio0"),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
	PIN_BANK(3, 32, "gpio3"),
};

static struct rockchip_pin_ctrl rk3128_pin_ctrl = {
		.pin_banks		= rk3128_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3128_pin_banks),
		.label			= "RK3128-GPIO",
		.type			= RK3128,
		.grf_mux_offset		= 0xa8,
		.iomux_recalced		= rk3128_mux_recalced_data,
		.niomux_recalced	= ARRAY_SIZE(rk3128_mux_recalced_data),
		.iomux_routes		= rk3128_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(rk3128_mux_route_data),
		.pull_calc_reg		= rk3128_calc_pull_reg_and_bit,
};

static struct rockchip_pin_bank rk3188_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", IOMUX_GPIO_ONLY, 0, 0, 0),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
	PIN_BANK(3, 32, "gpio3"),
};

static struct rockchip_pin_ctrl rk3188_pin_ctrl = {
		.pin_banks		= rk3188_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3188_pin_banks),
		.label			= "RK3188-GPIO",
		.type			= RK3188,
		.grf_mux_offset		= 0x60,
		.iomux_routes		= rk3188_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(rk3188_mux_route_data),
		.pull_calc_reg		= rk3188_calc_pull_reg_and_bit,
};

static struct rockchip_pin_bank rk3228_pin_banks[] = {
	PIN_BANK(0, 32, "gpio0"),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
	PIN_BANK(3, 32, "gpio3"),
};

static struct rockchip_pin_ctrl rk3228_pin_ctrl = {
		.pin_banks		= rk3228_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3228_pin_banks),
		.label			= "RK3228-GPIO",
		.type			= RK3288,
		.grf_mux_offset		= 0x0,
		.iomux_routes		= rk3228_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(rk3228_mux_route_data),
		.pull_calc_reg		= rk3228_calc_pull_reg_and_bit,
		.drv_calc_reg		= rk3228_calc_drv_reg_and_bit,
};

static struct rockchip_pin_bank rk3288_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 24, "gpio0", IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_UNROUTED
			    ),
	PIN_BANK_IOMUX_FLAGS(1, 32, "gpio1", IOMUX_UNROUTED,
					     IOMUX_UNROUTED,
					     IOMUX_UNROUTED,
					     0
			    ),
	PIN_BANK_IOMUX_FLAGS(2, 32, "gpio2", 0, 0, 0, IOMUX_UNROUTED),
	PIN_BANK_IOMUX_FLAGS(3, 32, "gpio3", 0, 0, 0, IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS(4, 32, "gpio4", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     0,
					     0
			    ),
	PIN_BANK_IOMUX_FLAGS(5, 32, "gpio5", IOMUX_UNROUTED,
					     0,
					     0,
					     IOMUX_UNROUTED
			    ),
	PIN_BANK_IOMUX_FLAGS(6, 32, "gpio6", 0, 0, 0, IOMUX_UNROUTED),
	PIN_BANK_IOMUX_FLAGS(7, 32, "gpio7", 0,
					     0,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_UNROUTED
			    ),
	PIN_BANK(8, 16, "gpio8"),
};

static struct rockchip_pin_ctrl rk3288_pin_ctrl = {
		.pin_banks		= rk3288_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3288_pin_banks),
		.label			= "RK3288-GPIO",
		.type			= RK3288,
		.grf_mux_offset		= 0x0,
		.pmu_mux_offset		= 0x84,
		.iomux_routes		= rk3288_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(rk3288_mux_route_data),
		.pull_calc_reg		= rk3288_calc_pull_reg_and_bit,
		.drv_calc_reg		= rk3288_calc_drv_reg_and_bit,
};

static struct rockchip_pin_bank rk3308_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT),
	PIN_BANK_IOMUX_FLAGS(1, 32, "gpio1", IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT),
	PIN_BANK_IOMUX_FLAGS(2, 32, "gpio2", IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT),
	PIN_BANK_IOMUX_FLAGS(3, 32, "gpio3", IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT),
	PIN_BANK_IOMUX_FLAGS(4, 32, "gpio4", IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT,
					     IOMUX_WIDTH_2BIT),
};

static struct rockchip_pin_ctrl rk3308_pin_ctrl = {
		.pin_banks		= rk3308_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3308_pin_banks),
		.label			= "RK3308-GPIO",
		.type			= RK3308,
		.grf_mux_offset		= 0x0,
		.iomux_recalced		= rk3308_mux_recalced_data,
		.niomux_recalced	= ARRAY_SIZE(rk3308_mux_recalced_data),
		.iomux_routes		= rk3308_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(rk3308_mux_route_data),
		.pull_calc_reg		= rk3308_calc_pull_reg_and_bit,
		.drv_calc_reg		= rk3308_calc_drv_reg_and_bit,
		.schmitt_calc_reg	= rk3308_calc_schmitt_reg_and_bit,
};

static struct rockchip_pin_bank rk3328_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", 0, 0, 0, 0),
	PIN_BANK_IOMUX_FLAGS(1, 32, "gpio1", 0, 0, 0, 0),
	PIN_BANK_IOMUX_FLAGS(2, 32, "gpio2", 0,
			     IOMUX_WIDTH_3BIT,
			     IOMUX_WIDTH_3BIT,
			     0),
	PIN_BANK_IOMUX_FLAGS(3, 32, "gpio3",
			     IOMUX_WIDTH_3BIT,
			     IOMUX_WIDTH_3BIT,
			     0,
			     0),
};

static struct rockchip_pin_ctrl rk3328_pin_ctrl = {
		.pin_banks		= rk3328_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3328_pin_banks),
		.label			= "RK3328-GPIO",
		.type			= RK3288,
		.grf_mux_offset		= 0x0,
		.iomux_recalced		= rk3328_mux_recalced_data,
		.niomux_recalced	= ARRAY_SIZE(rk3328_mux_recalced_data),
		.iomux_routes		= rk3328_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(rk3328_mux_route_data),
		.pull_calc_reg		= rk3228_calc_pull_reg_and_bit,
		.drv_calc_reg		= rk3228_calc_drv_reg_and_bit,
		.schmitt_calc_reg	= rk3328_calc_schmitt_reg_and_bit,
};

static struct rockchip_pin_bank rk3368_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU,
					     IOMUX_SOURCE_PMU
			    ),
	PIN_BANK(1, 32, "gpio1"),
	PIN_BANK(2, 32, "gpio2"),
	PIN_BANK(3, 32, "gpio3"),
};

static struct rockchip_pin_ctrl rk3368_pin_ctrl = {
		.pin_banks		= rk3368_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3368_pin_banks),
		.label			= "RK3368-GPIO",
		.type			= RK3368,
		.grf_mux_offset		= 0x0,
		.pmu_mux_offset		= 0x0,
		.pull_calc_reg		= rk3368_calc_pull_reg_and_bit,
		.drv_calc_reg		= rk3368_calc_drv_reg_and_bit,
};

static struct rockchip_pin_bank rk3399_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS_DRV_FLAGS_OFFSET_PULL_FLAGS(0, 32, "gpio0",
							 IOMUX_SOURCE_PMU,
							 IOMUX_SOURCE_PMU,
							 IOMUX_SOURCE_PMU,
							 IOMUX_SOURCE_PMU,
							 DRV_TYPE_IO_1V8_ONLY,
							 DRV_TYPE_IO_1V8_ONLY,
							 DRV_TYPE_IO_DEFAULT,
							 DRV_TYPE_IO_DEFAULT,
							 0x80,
							 0x88,
							 -1,
							 -1,
							 PULL_TYPE_IO_1V8_ONLY,
							 PULL_TYPE_IO_1V8_ONLY,
							 PULL_TYPE_IO_DEFAULT,
							 PULL_TYPE_IO_DEFAULT
							),
	PIN_BANK_IOMUX_DRV_FLAGS_OFFSET(1, 32, "gpio1", IOMUX_SOURCE_PMU,
					IOMUX_SOURCE_PMU,
					IOMUX_SOURCE_PMU,
					IOMUX_SOURCE_PMU,
					DRV_TYPE_IO_1V8_OR_3V0,
					DRV_TYPE_IO_1V8_OR_3V0,
					DRV_TYPE_IO_1V8_OR_3V0,
					DRV_TYPE_IO_1V8_OR_3V0,
					0xa0,
					0xa8,
					0xb0,
					0xb8
					),
	PIN_BANK_DRV_FLAGS_PULL_FLAGS(2, 32, "gpio2", DRV_TYPE_IO_1V8_OR_3V0,
				      DRV_TYPE_IO_1V8_OR_3V0,
				      DRV_TYPE_IO_1V8_ONLY,
				      DRV_TYPE_IO_1V8_ONLY,
				      PULL_TYPE_IO_DEFAULT,
				      PULL_TYPE_IO_DEFAULT,
				      PULL_TYPE_IO_1V8_ONLY,
				      PULL_TYPE_IO_1V8_ONLY
				      ),
	PIN_BANK_DRV_FLAGS(3, 32, "gpio3", DRV_TYPE_IO_3V3_ONLY,
			   DRV_TYPE_IO_3V3_ONLY,
			   DRV_TYPE_IO_3V3_ONLY,
			   DRV_TYPE_IO_1V8_OR_3V0
			   ),
	PIN_BANK_DRV_FLAGS(4, 32, "gpio4", DRV_TYPE_IO_1V8_OR_3V0,
			   DRV_TYPE_IO_1V8_3V0_AUTO,
			   DRV_TYPE_IO_1V8_OR_3V0,
			   DRV_TYPE_IO_1V8_OR_3V0
			   ),
};

static struct rockchip_pin_ctrl rk3399_pin_ctrl = {
		.pin_banks		= rk3399_pin_banks,
		.nr_banks		= ARRAY_SIZE(rk3399_pin_banks),
		.label			= "RK3399-GPIO",
		.type			= RK3399,
		.grf_mux_offset		= 0xe000,
		.pmu_mux_offset		= 0x0,
		.grf_drv_offset		= 0xe100,
		.pmu_drv_offset		= 0x80,
		.iomux_routes		= rk3399_mux_route_data,
		.niomux_routes		= ARRAY_SIZE(rk3399_mux_route_data),
		.pull_calc_reg		= rk3399_calc_pull_reg_and_bit,
		.drv_calc_reg		= rk3399_calc_drv_reg_and_bit,
};

static struct rockchip_pin_bank rk3568_pin_banks[] = {
	PIN_BANK_IOMUX_FLAGS(0, 32, "gpio0", IOMUX_SOURCE_PMU | IOMUX_WIDTH_4BIT,
					     IOMUX_SOURCE_PMU | IOMUX_WIDTH_4BIT,
					     IOMUX_SOURCE_PMU | IOMUX_WIDTH_4BIT,
					     IOMUX_SOURCE_PMU | IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS(1, 32, "gpio1", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS(2, 32, "gpio2", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS(3, 32, "gpio3", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT),
	PIN_BANK_IOMUX_FLAGS(4, 32, "gpio4", IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT,
					     IOMUX_WIDTH_4BIT),
};

static struct rockchip_pin_ctrl rk3568_pin_ctrl = {
	.pin_banks		= rk3568_pin_banks,
	.nr_banks		= ARRAY_SIZE(rk3568_pin_banks),
	.label			= "RK3568-GPIO",
	.type			= RK3568,
	.grf_mux_offset		= 0x0,
	.pmu_mux_offset		= 0x0,
	.grf_drv_offset		= 0x0200,
	.pmu_drv_offset		= 0x0070,
	.iomux_routes		= rk3568_mux_route_data,
	.niomux_routes		= ARRAY_SIZE(rk3568_mux_route_data),
	.pull_calc_reg		= rk3568_calc_pull_reg_and_bit,
	.drv_calc_reg		= rk3568_calc_drv_reg_and_bit,
	.schmitt_calc_reg	= rk3568_calc_schmitt_reg_and_bit,
};

static struct rockchip_pin_bank rk3588_pin_banks[] = {
	RK3588_PIN_BANK_FLAGS(0, 32, "gpio0",
			      IOMUX_WIDTH_4BIT, PULL_TYPE_IO_1V8_ONLY),
	RK3588_PIN_BANK_FLAGS(1, 32, "gpio1",
			      IOMUX_WIDTH_4BIT, PULL_TYPE_IO_1V8_ONLY),
	RK3588_PIN_BANK_FLAGS(2, 32, "gpio2",
			      IOMUX_WIDTH_4BIT, PULL_TYPE_IO_1V8_ONLY),
	RK3588_PIN_BANK_FLAGS(3, 32, "gpio3",
			      IOMUX_WIDTH_4BIT, PULL_TYPE_IO_1V8_ONLY),
	RK3588_PIN_BANK_FLAGS(4, 32, "gpio4",
			      IOMUX_WIDTH_4BIT, PULL_TYPE_IO_1V8_ONLY),
};

static struct rockchip_pin_ctrl rk3588_pin_ctrl = {
	.pin_banks		= rk3588_pin_banks,
	.nr_banks		= ARRAY_SIZE(rk3588_pin_banks),
	.label			= "RK3588-GPIO",
	.type			= RK3588,
	.pull_calc_reg		= rk3588_calc_pull_reg_and_bit,
	.drv_calc_reg		= rk3588_calc_drv_reg_and_bit,
	.schmitt_calc_reg	= rk3588_calc_schmitt_reg_and_bit,
};

static const struct of_device_id rockchip_pinctrl_dt_match[] = {
	{ .compatible = "rockchip,px30-pinctrl",
		.data = &px30_pin_ctrl },
	{ .compatible = "rockchip,rv1108-pinctrl",
		.data = &rv1108_pin_ctrl },
	{ .compatible = "rockchip,rv1126-pinctrl",
		.data = &rv1126_pin_ctrl },
	{ .compatible = "rockchip,rk2928-pinctrl",
		.data = &rk2928_pin_ctrl },
	{ .compatible = "rockchip,rk3036-pinctrl",
		.data = &rk3036_pin_ctrl },
	{ .compatible = "rockchip,rk3066a-pinctrl",
		.data = &rk3066a_pin_ctrl },
	{ .compatible = "rockchip,rk3066b-pinctrl",
		.data = &rk3066b_pin_ctrl },
	{ .compatible = "rockchip,rk3128-pinctrl",
		.data = (void *)&rk3128_pin_ctrl },
	{ .compatible = "rockchip,rk3188-pinctrl",
		.data = &rk3188_pin_ctrl },
	{ .compatible = "rockchip,rk3228-pinctrl",
		.data = &rk3228_pin_ctrl },
	{ .compatible = "rockchip,rk3288-pinctrl",
		.data = &rk3288_pin_ctrl },
	{ .compatible = "rockchip,rk3308-pinctrl",
		.data = &rk3308_pin_ctrl },
	{ .compatible = "rockchip,rk3328-pinctrl",
		.data = &rk3328_pin_ctrl },
	{ .compatible = "rockchip,rk3368-pinctrl",
		.data = &rk3368_pin_ctrl },
	{ .compatible = "rockchip,rk3399-pinctrl",
		.data = &rk3399_pin_ctrl },
	{ .compatible = "rockchip,rk3568-pinctrl",
		.data = &rk3568_pin_ctrl },
	{ .compatible = "rockchip,rk3588-pinctrl",
		.data = &rk3588_pin_ctrl },
	{},
};

static struct platform_driver rockchip_pinctrl_driver = {
	.probe		= rockchip_pinctrl_probe,
	.remove		= rockchip_pinctrl_remove,
	.driver = {
		.name	= "rockchip-pinctrl",
		.pm = &rockchip_pinctrl_dev_pm_ops,
		.of_match_table = rockchip_pinctrl_dt_match,
	},
};

static int __init rockchip_pinctrl_drv_register(void)
{
	return platform_driver_register(&rockchip_pinctrl_driver);
}
postcore_initcall(rockchip_pinctrl_drv_register);

static void __exit rockchip_pinctrl_drv_unregister(void)
{
	platform_driver_unregister(&rockchip_pinctrl_driver);
}
module_exit(rockchip_pinctrl_drv_unregister);

MODULE_DESCRIPTION("ROCKCHIP Pin Controller Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pinctrl-rockchip");
MODULE_DEVICE_TABLE(of, rockchip_pinctrl_dt_match);
