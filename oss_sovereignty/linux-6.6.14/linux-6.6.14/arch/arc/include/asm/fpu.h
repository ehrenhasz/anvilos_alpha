#ifndef _ASM_ARC_FPU_H
#define _ASM_ARC_FPU_H
#ifdef CONFIG_ARC_FPU_SAVE_RESTORE
#include <asm/ptrace.h>
#ifdef CONFIG_ISA_ARCOMPACT
struct arc_fpu {
	struct {
		unsigned int l, h;
	} aux_dpfp[2];
};
#define fpu_init_task(regs)
#else
struct arc_fpu {
	unsigned int ctrl, status;
};
extern void fpu_init_task(struct pt_regs *regs);
#endif	 
struct task_struct;
extern void fpu_save_restore(struct task_struct *p, struct task_struct *n);
#else	 
#define fpu_save_restore(p, n)
#define fpu_init_task(regs)
#endif	 
#endif	 
