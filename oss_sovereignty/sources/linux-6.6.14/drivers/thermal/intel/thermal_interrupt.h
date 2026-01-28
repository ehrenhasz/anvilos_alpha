
#ifndef _INTEL_THERMAL_INTERRUPT_H
#define _INTEL_THERMAL_INTERRUPT_H

#define CORE_LEVEL	0
#define PACKAGE_LEVEL	1


extern int (*platform_thermal_package_notify)(__u64 msr_val);


extern int (*platform_thermal_notify)(__u64 msr_val);


extern bool (*platform_thermal_package_rate_control)(void);


extern void notify_hwp_interrupt(void);


extern void thermal_clear_package_intr_status(int level, u64 bit_mask);

#endif 
