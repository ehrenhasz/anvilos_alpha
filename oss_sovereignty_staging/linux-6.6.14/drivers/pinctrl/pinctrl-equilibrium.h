 
 

#ifndef __PINCTRL_EQUILIBRIUM_H
#define __PINCTRL_EQUILIBRIUM_H

 
#define REG_PMX_BASE	0x0	 
#define REG_PUEN	0x80	 
#define REG_PDEN	0x84	 
#define REG_SRC		0x88	 
#define REG_DCC0	0x8C	 
#define REG_DCC1	0x90	 
#define REG_OD		0x94	 
#define REG_AVAIL	0x98	 
#define DRV_CUR_PINS	16	 
#define REG_DRCC(x)	(REG_DCC0 + (x) * 4)  

 
#define GPIO_OUT	0x0	 
#define GPIO_IN		0x4	 
#define GPIO_DIR	0x8	 
#define GPIO_EXINTCR0	0x18	 
#define GPIO_EXINTCR1	0x1C	 
#define GPIO_IRNCR	0x20	 
#define GPIO_IRNICR	0x24	 
#define GPIO_IRNEN	0x28	 
#define GPIO_IRNCFG	0x2C	 
#define GPIO_IRNRNSET	0x30	 
#define GPIO_IRNENCLR	0x34	 
#define GPIO_OUTSET	0x40	 
#define GPIO_OUTCLR	0x44	 
#define GPIO_DIRSET	0x48	 
#define GPIO_DIRCLR	0x4C	 

 
#define PARSE_DRV_CURRENT(val, pin) (((val) >> ((pin) * 2)) & 0x3)

#define GPIO_EDGE_TRIG		0
#define GPIO_LEVEL_TRIG		1
#define GPIO_SINGLE_EDGE	0
#define GPIO_BOTH_EDGE		1
#define GPIO_POSITIVE_TRIG	0
#define GPIO_NEGATIVE_TRIG	1

#define EQBR_GPIO_MODE		0

typedef enum {
	OP_COUNT_NR_FUNCS,
	OP_ADD_FUNCS,
	OP_COUNT_NR_FUNC_GRPS,
	OP_ADD_FUNC_GRPS,
	OP_NONE,
} funcs_util_ops;

 
struct gpio_irq_type {
	unsigned int trig_type;
	unsigned int edge_type;
	unsigned int logic_type;
};

 
struct eqbr_pmx_func {
	const char		*name;
	const char		**groups;
	unsigned int		nr_groups;
};

 
struct eqbr_pin_bank {
	void __iomem		*membase;
	unsigned int		id;
	unsigned int		pin_base;
	unsigned int		nr_pins;
	u32			aval_pinmap;
};

struct fwnode_handle;

 
struct eqbr_gpio_ctrl {
	struct gpio_chip	chip;
	struct fwnode_handle	*fwnode;
	struct eqbr_pin_bank	*bank;
	void __iomem		*membase;
	const char		*name;
	unsigned int		virq;
	raw_spinlock_t		lock;  
};

 
struct eqbr_pinctrl_drv_data {
	struct device			*dev;
	struct pinctrl_desc		pctl_desc;
	struct pinctrl_dev		*pctl_dev;
	void __iomem			*membase;
	struct eqbr_pin_bank		*pin_banks;
	unsigned int			nr_banks;
	struct eqbr_gpio_ctrl		*gpio_ctrls;
	unsigned int			nr_gpio_ctrls;
	raw_spinlock_t			lock;  
};

#endif  
