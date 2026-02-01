 
 
#ifndef __CLK_BT1_CCU_DIV_H__
#define __CLK_BT1_CCU_DIV_H__

#include <linux/clk-provider.h>
#include <linux/spinlock.h>
#include <linux/regmap.h>
#include <linux/bits.h>
#include <linux/of.h>

 
#define CCU_SYS_SATA_CLK		-1
#define CCU_SYS_XGMAC_CLK		-2

 
#define CCU_DIV_BASIC			BIT(0)
#define CCU_DIV_SKIP_ONE		BIT(1)
#define CCU_DIV_SKIP_ONE_TO_THREE	BIT(2)
#define CCU_DIV_LOCK_SHIFTED		BIT(3)
#define CCU_DIV_RESET_DOMAIN		BIT(4)

 
enum ccu_div_type {
	CCU_DIV_VAR,
	CCU_DIV_GATE,
	CCU_DIV_BUF,
	CCU_DIV_FIXED
};

 
struct ccu_div_init_data {
	unsigned int id;
	const char *name;
	const char *parent_name;
	unsigned int base;
	struct regmap *sys_regs;
	struct device_node *np;
	enum ccu_div_type type;
	union {
		unsigned int width;
		unsigned int divider;
	};
	unsigned long flags;
	unsigned long features;
};

 
struct ccu_div {
	struct clk_hw hw;
	unsigned int id;
	unsigned int reg_ctl;
	struct regmap *sys_regs;
	spinlock_t lock;
	union {
		u32 mask;
		unsigned int divider;
	};
	unsigned long flags;
	unsigned long features;
};
#define to_ccu_div(_hw) container_of(_hw, struct ccu_div, hw)

static inline struct clk_hw *ccu_div_get_clk_hw(struct ccu_div *div)
{
	return div ? &div->hw : NULL;
}

struct ccu_div *ccu_div_hw_register(const struct ccu_div_init_data *init);

void ccu_div_hw_unregister(struct ccu_div *div);

#endif  
