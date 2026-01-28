#ifndef _ASM_POWERNV_H
#define _ASM_POWERNV_H
#ifdef CONFIG_PPC_POWERNV
extern void powernv_set_nmmu_ptcr(unsigned long ptcr);
void pnv_program_cpu_hotplug_lpcr(unsigned int cpu, u64 lpcr_val);
void pnv_tm_init(void);
#else
static inline void powernv_set_nmmu_ptcr(unsigned long ptcr) { }
static inline void pnv_tm_init(void) { }
#endif
#endif  
