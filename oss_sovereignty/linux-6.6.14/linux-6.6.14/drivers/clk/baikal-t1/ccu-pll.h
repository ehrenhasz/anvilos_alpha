#ifndef __CLK_BT1_CCU_PLL_H__
#define __CLK_BT1_CCU_PLL_H__
#include <linux/clk-provider.h>
#include <linux/spinlock.h>
#include <linux/regmap.h>
#include <linux/bits.h>
#include <linux/of.h>
#define CCU_PLL_BASIC		BIT(0)
struct ccu_pll_init_data {
	unsigned int id;
	const char *name;
	const char *parent_name;
	unsigned int base;
	struct regmap *sys_regs;
	struct device_node *np;
	unsigned long flags;
	unsigned long features;
};
struct ccu_pll {
	struct clk_hw hw;
	unsigned int id;
	unsigned int reg_ctl;
	unsigned int reg_ctl1;
	struct regmap *sys_regs;
	spinlock_t lock;
};
#define to_ccu_pll(_hw) container_of(_hw, struct ccu_pll, hw)
static inline struct clk_hw *ccu_pll_get_clk_hw(struct ccu_pll *pll)
{
	return pll ? &pll->hw : NULL;
}
struct ccu_pll *ccu_pll_hw_register(const struct ccu_pll_init_data *init);
void ccu_pll_hw_unregister(struct ccu_pll *pll);
#endif  
