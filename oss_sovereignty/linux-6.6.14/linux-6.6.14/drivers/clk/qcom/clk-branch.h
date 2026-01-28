#ifndef __QCOM_CLK_BRANCH_H__
#define __QCOM_CLK_BRANCH_H__
#include <linux/bitfield.h>
#include <linux/clk-provider.h>
#include "clk-regmap.h"
struct clk_branch {
	u32	hwcg_reg;
	u32	halt_reg;
	u8	hwcg_bit;
	u8	halt_bit;
	u8	halt_check;
#define BRANCH_VOTED			BIT(7)  
#define BRANCH_HALT			0  
#define BRANCH_HALT_VOTED		(BRANCH_HALT | BRANCH_VOTED)
#define BRANCH_HALT_ENABLE		1  
#define BRANCH_HALT_ENABLE_VOTED	(BRANCH_HALT_ENABLE | BRANCH_VOTED)
#define BRANCH_HALT_DELAY		2  
#define BRANCH_HALT_SKIP		3  
	struct clk_regmap clkr;
};
#define CBCR_CLK_OFF			BIT(31)
#define CBCR_NOC_FSM_STATUS		GENMASK(30, 28)
 #define FSM_STATUS_ON			BIT(1)
#define CBCR_FORCE_MEM_CORE_ON		BIT(14)
#define CBCR_FORCE_MEM_PERIPH_ON	BIT(13)
#define CBCR_FORCE_MEM_PERIPH_OFF	BIT(12)
#define CBCR_WAKEUP			GENMASK(11, 8)
#define CBCR_SLEEP			GENMASK(7, 4)
static inline void qcom_branch_set_force_mem_core(struct regmap *regmap,
						  struct clk_branch clk, bool on)
{
	regmap_update_bits(regmap, clk.halt_reg, CBCR_FORCE_MEM_CORE_ON,
			   on ? CBCR_FORCE_MEM_CORE_ON : 0);
}
static inline void qcom_branch_set_force_periph_on(struct regmap *regmap,
						   struct clk_branch clk, bool on)
{
	regmap_update_bits(regmap, clk.halt_reg, CBCR_FORCE_MEM_PERIPH_ON,
			   on ? CBCR_FORCE_MEM_PERIPH_ON : 0);
}
static inline void qcom_branch_set_force_periph_off(struct regmap *regmap,
						    struct clk_branch clk, bool on)
{
	regmap_update_bits(regmap, clk.halt_reg, CBCR_FORCE_MEM_PERIPH_OFF,
			   on ? CBCR_FORCE_MEM_PERIPH_OFF : 0);
}
static inline void qcom_branch_set_wakeup(struct regmap *regmap, struct clk_branch clk, u32 val)
{
	regmap_update_bits(regmap, clk.halt_reg, CBCR_WAKEUP,
			   FIELD_PREP(CBCR_WAKEUP, val));
}
static inline void qcom_branch_set_sleep(struct regmap *regmap, struct clk_branch clk, u32 val)
{
	regmap_update_bits(regmap, clk.halt_reg, CBCR_SLEEP,
			   FIELD_PREP(CBCR_SLEEP, val));
}
extern const struct clk_ops clk_branch_ops;
extern const struct clk_ops clk_branch2_ops;
extern const struct clk_ops clk_branch_simple_ops;
extern const struct clk_ops clk_branch2_aon_ops;
#define to_clk_branch(_hw) \
	container_of(to_clk_regmap(_hw), struct clk_branch, clkr)
#endif
