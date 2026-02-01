
 

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/spinlock.h>

#include "ccu_sdm.h"

bool ccu_sdm_helper_is_enabled(struct ccu_common *common,
			       struct ccu_sdm_internal *sdm)
{
	if (!(common->features & CCU_FEATURE_SIGMA_DELTA_MOD))
		return false;

	if (sdm->enable && !(readl(common->base + common->reg) & sdm->enable))
		return false;

	return !!(readl(common->base + sdm->tuning_reg) & sdm->tuning_enable);
}
EXPORT_SYMBOL_NS_GPL(ccu_sdm_helper_is_enabled, SUNXI_CCU);

void ccu_sdm_helper_enable(struct ccu_common *common,
			   struct ccu_sdm_internal *sdm,
			   unsigned long rate)
{
	unsigned long flags;
	unsigned int i;
	u32 reg;

	if (!(common->features & CCU_FEATURE_SIGMA_DELTA_MOD))
		return;

	 
	for (i = 0; i < sdm->table_size; i++)
		if (sdm->table[i].rate == rate)
			writel(sdm->table[i].pattern,
			       common->base + sdm->tuning_reg);

	 
	spin_lock_irqsave(common->lock, flags);
	reg = readl(common->base + sdm->tuning_reg);
	writel(reg | sdm->tuning_enable, common->base + sdm->tuning_reg);
	spin_unlock_irqrestore(common->lock, flags);

	spin_lock_irqsave(common->lock, flags);
	reg = readl(common->base + common->reg);
	writel(reg | sdm->enable, common->base + common->reg);
	spin_unlock_irqrestore(common->lock, flags);
}
EXPORT_SYMBOL_NS_GPL(ccu_sdm_helper_enable, SUNXI_CCU);

void ccu_sdm_helper_disable(struct ccu_common *common,
			    struct ccu_sdm_internal *sdm)
{
	unsigned long flags;
	u32 reg;

	if (!(common->features & CCU_FEATURE_SIGMA_DELTA_MOD))
		return;

	spin_lock_irqsave(common->lock, flags);
	reg = readl(common->base + common->reg);
	writel(reg & ~sdm->enable, common->base + common->reg);
	spin_unlock_irqrestore(common->lock, flags);

	spin_lock_irqsave(common->lock, flags);
	reg = readl(common->base + sdm->tuning_reg);
	writel(reg & ~sdm->tuning_enable, common->base + sdm->tuning_reg);
	spin_unlock_irqrestore(common->lock, flags);
}
EXPORT_SYMBOL_NS_GPL(ccu_sdm_helper_disable, SUNXI_CCU);

 
bool ccu_sdm_helper_has_rate(struct ccu_common *common,
			     struct ccu_sdm_internal *sdm,
			     unsigned long rate)
{
	unsigned int i;

	if (!(common->features & CCU_FEATURE_SIGMA_DELTA_MOD))
		return false;

	for (i = 0; i < sdm->table_size; i++)
		if (sdm->table[i].rate == rate)
			return true;

	return false;
}
EXPORT_SYMBOL_NS_GPL(ccu_sdm_helper_has_rate, SUNXI_CCU);

unsigned long ccu_sdm_helper_read_rate(struct ccu_common *common,
				       struct ccu_sdm_internal *sdm,
				       u32 m, u32 n)
{
	unsigned int i;
	u32 reg;

	pr_debug("%s: Read sigma-delta modulation setting\n",
		 clk_hw_get_name(&common->hw));

	if (!(common->features & CCU_FEATURE_SIGMA_DELTA_MOD))
		return 0;

	pr_debug("%s: clock is sigma-delta modulated\n",
		 clk_hw_get_name(&common->hw));

	reg = readl(common->base + sdm->tuning_reg);

	pr_debug("%s: pattern reg is 0x%x",
		 clk_hw_get_name(&common->hw), reg);

	for (i = 0; i < sdm->table_size; i++)
		if (sdm->table[i].pattern == reg &&
		    sdm->table[i].m == m && sdm->table[i].n == n)
			return sdm->table[i].rate;

	 
	return 0;
}
EXPORT_SYMBOL_NS_GPL(ccu_sdm_helper_read_rate, SUNXI_CCU);

int ccu_sdm_helper_get_factors(struct ccu_common *common,
			       struct ccu_sdm_internal *sdm,
			       unsigned long rate,
			       unsigned long *m, unsigned long *n)
{
	unsigned int i;

	if (!(common->features & CCU_FEATURE_SIGMA_DELTA_MOD))
		return -EINVAL;

	for (i = 0; i < sdm->table_size; i++)
		if (sdm->table[i].rate == rate) {
			*m = sdm->table[i].m;
			*n = sdm->table[i].n;
			return 0;
		}

	 
	return -EINVAL;
}
EXPORT_SYMBOL_NS_GPL(ccu_sdm_helper_get_factors, SUNXI_CCU);
