#ifndef __ALPINE_CPU_PM_H__
#define __ALPINE_CPU_PM_H__
void alpine_cpu_pm_init(void);
int alpine_cpu_wakeup(unsigned int phys_cpu, uint32_t phys_resume_addr);
#endif  
