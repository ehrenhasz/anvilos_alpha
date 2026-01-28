#ifndef _MACH_DAVINCI_PM_H
#define _MACH_DAVINCI_PM_H
struct davinci_pm_config {
	void __iomem *ddr2_ctlr_base;
	void __iomem *ddrpsc_reg_base;
	int ddrpsc_num;
	void __iomem *ddrpll_reg_base;
	void __iomem *deepsleep_reg;
	void __iomem *cpupll_reg_base;
	int sleepcount;
};
extern unsigned int davinci_cpu_suspend_sz;
extern void davinci_cpu_suspend(struct davinci_pm_config *);
#endif
