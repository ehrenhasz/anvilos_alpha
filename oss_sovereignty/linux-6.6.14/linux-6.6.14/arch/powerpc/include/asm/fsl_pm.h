#ifndef __PPC_FSL_PM_H
#define __PPC_FSL_PM_H
#define E500_PM_PH10	1
#define E500_PM_PH15	2
#define E500_PM_PH20	3
#define E500_PM_PH30	4
#define E500_PM_DOZE	E500_PM_PH10
#define E500_PM_NAP	E500_PM_PH15
#define PLAT_PM_SLEEP	20
#define PLAT_PM_LPM20	30
#define FSL_PM_SLEEP		(1 << 0)
#define FSL_PM_DEEP_SLEEP	(1 << 1)
struct fsl_pm_ops {
	void (*irq_mask)(int cpu);
	void (*irq_unmask)(int cpu);
	void (*cpu_enter_state)(int cpu, int state);
	void (*cpu_exit_state)(int cpu, int state);
	void (*cpu_up_prepare)(int cpu);
	void (*cpu_die)(int cpu);
	int (*plat_enter_sleep)(void);
	void (*freeze_time_base)(bool freeze);
	void (*set_ip_power)(bool enable, u32 mask);
	unsigned int (*get_pm_modes)(void);
};
extern const struct fsl_pm_ops *qoriq_pm_ops;
int __init fsl_rcpm_init(void);
#endif  
