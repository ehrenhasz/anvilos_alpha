#ifndef __MIPS_ASM_PM_CPS_H__
#define __MIPS_ASM_PM_CPS_H__
#if defined(CONFIG_CPU_MIPSR6)
# define coupled_coherence cpu_has_vp
#elif defined(CONFIG_MIPS_MT)
# define coupled_coherence cpu_has_mipsmt
#else
# define coupled_coherence 0
#endif
enum cps_pm_state {
	CPS_PM_NC_WAIT,		 
	CPS_PM_CLOCK_GATED,	 
	CPS_PM_POWER_GATED,	 
	CPS_PM_STATE_COUNT,
};
extern bool cps_pm_support_state(enum cps_pm_state state);
extern int cps_pm_enter_state(enum cps_pm_state state);
#endif  
