#ifndef __MIPS_ASM_SMP_CPS_H__
#define __MIPS_ASM_SMP_CPS_H__
#define CPS_ENTRY_PATCH_INSNS	6
#ifndef __ASSEMBLY__
struct vpe_boot_config {
	unsigned long pc;
	unsigned long sp;
	unsigned long gp;
};
struct core_boot_config {
	atomic_t vpe_mask;
	struct vpe_boot_config *vpe_config;
};
extern struct core_boot_config *mips_cps_core_bootcfg;
extern void mips_cps_core_entry(void);
extern void mips_cps_core_init(void);
extern void mips_cps_boot_vpes(struct core_boot_config *cfg, unsigned vpe);
extern void mips_cps_pm_save(void);
extern void mips_cps_pm_restore(void);
extern void *mips_cps_core_entry_patch_end;
#ifdef CONFIG_MIPS_CPS
extern bool mips_cps_smp_in_use(void);
#else  
static inline bool mips_cps_smp_in_use(void) { return false; }
#endif  
#else  
.extern mips_cps_bootcfg;
#endif  
#endif  
