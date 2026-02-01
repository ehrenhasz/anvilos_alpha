 
 

#ifndef __CLK_RENESAS_RCAR_CPG_LIB_H__
#define __CLK_RENESAS_RCAR_CPG_LIB_H__

extern spinlock_t cpg_lock;

struct cpg_simple_notifier {
	struct notifier_block nb;
	void __iomem *reg;
	u32 saved;
};

void cpg_simple_notifier_register(struct raw_notifier_head *notifiers,
				  struct cpg_simple_notifier *csn);

void cpg_reg_modify(void __iomem *reg, u32 clear, u32 set);

struct clk * __init cpg_sdh_clk_register(const char *name,
	void __iomem *sdnckcr, const char *parent_name,
	struct raw_notifier_head *notifiers);

struct clk * __init cpg_sd_clk_register(const char *name,
	void __iomem *sdnckcr, const char *parent_name);

struct clk * __init cpg_rpc_clk_register(const char *name,
	void __iomem *rpcckcr, const char *parent_name,
	struct raw_notifier_head *notifiers);

struct clk * __init cpg_rpcd2_clk_register(const char *name,
					   void __iomem *rpcckcr,
					   const char *parent_name);
#endif
