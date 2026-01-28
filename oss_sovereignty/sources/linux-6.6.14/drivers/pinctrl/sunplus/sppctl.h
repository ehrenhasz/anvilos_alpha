


#ifndef __SPPCTL_H__
#define __SPPCTL_H__

#include <linux/bits.h>
#include <linux/gpio/driver.h>
#include <linux/kernel.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#define SPPCTL_MODULE_NAME		"sppctl_sp7021"

#define SPPCTL_GPIO_OFF_FIRST		0x00
#define SPPCTL_GPIO_OFF_MASTER		0x00
#define SPPCTL_GPIO_OFF_OE		0x20
#define SPPCTL_GPIO_OFF_OUT		0x40
#define SPPCTL_GPIO_OFF_IN		0x60
#define SPPCTL_GPIO_OFF_IINV		0x80
#define SPPCTL_GPIO_OFF_OINV		0xa0
#define SPPCTL_GPIO_OFF_OD		0xc0

#define SPPCTL_FULLY_PINMUX_MASK_MASK	GENMASK(22, 16)
#define SPPCTL_FULLY_PINMUX_SEL_MASK	GENMASK(6, 0)
#define SPPCTL_FULLY_PINMUX_UPPER_SHIFT	8


#define SPPCTL_MOON_REG_MASK_SHIFT	16
#define SPPCTL_SET_MOON_REG_BIT(bit)	(BIT((bit) + SPPCTL_MOON_REG_MASK_SHIFT) | BIT(bit))
#define SPPCTL_CLR_MOON_REG_BIT(bit)	BIT((bit) + SPPCTL_MOON_REG_MASK_SHIFT)

#define SPPCTL_IOP_CONFIGS		0xff

#define FNCE(n, r, o, bo, bl, g) { \
	.name = n, \
	.type = r, \
	.roff = o, \
	.boff = bo, \
	.blen = bl, \
	.grps = (g), \
	.gnum = ARRAY_SIZE(g), \
}

#define FNCN(n, r, o, bo, bl) { \
	.name = n, \
	.type = r, \
	.roff = o, \
	.boff = bo, \
	.blen = bl, \
	.grps = NULL, \
	.gnum = 0, \
}

#define EGRP(n, v, p) { \
	.name = n, \
	.gval = (v), \
	.pins = (p), \
	.pnum = ARRAY_SIZE(p), \
}


enum mux_first_reg {
	mux_f_mux = 0,
	mux_f_gpio = 1,
	mux_f_keep = 2,
};


enum mux_master_reg {
	mux_m_iop = 0,
	mux_m_gpio = 1,
	mux_m_keep = 2,
};


enum pinmux_type {
	pinmux_type_fpmx,
	pinmux_type_grp,
};


struct grp2fp_map {
	u16 f_idx;
	u16 g_idx;
};

struct sppctl_gpio_chip;

struct sppctl_pdata {
	void __iomem *moon2_base;	
	void __iomem *gpioxt_base;	
	void __iomem *first_base;	
	void __iomem *moon1_base;	

	struct pinctrl_desc pctl_desc;
	struct pinctrl_dev *pctl_dev;
	struct pinctrl_gpio_range pctl_grange;
	struct sppctl_gpio_chip *spp_gchip;

	char const **unq_grps;
	size_t unq_grps_sz;
	struct grp2fp_map *g2fp_maps;
};

struct sppctl_grp {
	const char * const name;
	const u8 gval;                  
	const unsigned * const pins;    
	const unsigned int pnum;        
};

struct sppctl_func {
	const char * const name;
	const enum pinmux_type type;    
	const u8 roff;                  
	const u8 boff;                  
	const u8 blen;                  
	const struct sppctl_grp * const grps; 
	const unsigned int gnum;        
};

extern const struct sppctl_func sppctl_list_funcs[];
extern const char * const sppctl_pmux_list_s[];
extern const char * const sppctl_gpio_list_s[];
extern const struct pinctrl_pin_desc sppctl_pins_all[];
extern const unsigned int sppctl_pins_gpio[];

extern const size_t sppctl_list_funcs_sz;
extern const size_t sppctl_pmux_list_sz;
extern const size_t sppctl_gpio_list_sz;
extern const size_t sppctl_pins_all_sz;

#endif
