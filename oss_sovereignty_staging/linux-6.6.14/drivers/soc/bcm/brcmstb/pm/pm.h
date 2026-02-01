 
 

#ifndef __BRCMSTB_PM_H__
#define __BRCMSTB_PM_H__

#define AON_CTRL_RESET_CTRL		0x00
#define AON_CTRL_PM_CTRL		0x04
#define AON_CTRL_PM_STATUS		0x08
#define AON_CTRL_PM_CPU_WAIT_COUNT	0x10
#define AON_CTRL_PM_INITIATE		0x88
#define AON_CTRL_HOST_MISC_CMDS		0x8c
#define AON_CTRL_SYSTEM_DATA_RAM_OFS	0x200

 
 
#define DDR40_PHY_CONTROL_REGS_0_PLL_STATUS	0x10
#define DDR40_PHY_CONTROL_REGS_0_STANDBY_CTRL	0xa4

 
#define TIMER_TIMER1_CTRL		0x0c
#define TIMER_TIMER1_STAT		0x1c

 
#define RESET_TIMER			0x0
#define START_TIMER			0xbfffffff
#define TIMER_MASK			0x3fffffff

 
#define PM_FAST_PWRDOWN			(1 << 6)
#define PM_WARM_BOOT			(1 << 5)
#define PM_DEEP_STANDBY			(1 << 4)
#define PM_CPU_PWR			(1 << 3)
#define PM_USE_CPU_RDY			(1 << 2)
#define PM_PLL_PWRDOWN			(1 << 1)
#define PM_PWR_DOWN			(1 << 0)

 
#define PM_DPHY_STANDBY_CLEAR		(1 << 20)
#define PM_MIN_S3_WIDTH_TIMER_BYPASS	(1 << 7)

#define PM_S2_COMMAND	(PM_PLL_PWRDOWN | PM_USE_CPU_RDY | PM_PWR_DOWN)

 
#define PM_COLD_CONFIG	(PM_PLL_PWRDOWN | PM_DEEP_STANDBY)
#define PM_WARM_CONFIG	(PM_COLD_CONFIG | PM_USE_CPU_RDY | PM_WARM_BOOT)

 
#define M1_PM_WARM_CONFIG (PM_DPHY_STANDBY_CLEAR | \
			   PM_MIN_S3_WIDTH_TIMER_BYPASS | \
			   PM_WARM_BOOT | PM_DEEP_STANDBY | \
			   PM_PLL_PWRDOWN | PM_PWR_DOWN)

#define M1_PM_COLD_CONFIG (PM_DPHY_STANDBY_CLEAR | \
			   PM_MIN_S3_WIDTH_TIMER_BYPASS | \
			   PM_DEEP_STANDBY | \
			   PM_PLL_PWRDOWN | PM_PWR_DOWN)

#ifndef __ASSEMBLY__

#ifndef CONFIG_MIPS
extern const unsigned long brcmstb_pm_do_s2_sz;
extern asmlinkage int brcmstb_pm_do_s2(void __iomem *aon_ctrl_base,
		void __iomem *ddr_phy_pll_status);
#else
 
extern asmlinkage int brcm_pm_do_s2(u32 *s2_params);

 
extern asmlinkage int brcm_pm_do_s3(void __iomem *aon_ctrl_base,
		int dcache_linesz);
extern int s3_reentry;
#endif  

#endif 

#endif  
