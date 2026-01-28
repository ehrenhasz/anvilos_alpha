#ifndef __ASM_ARC_PROCESSOR_H
#define __ASM_ARC_PROCESSOR_H
#ifndef __ASSEMBLY__
#include <asm/ptrace.h>
#include <asm/dsp.h>
#include <asm/fpu.h>
struct thread_struct {
	unsigned long callee_reg;	 
	unsigned long fault_address;	 
#ifdef CONFIG_ARC_DSP_SAVE_RESTORE_REGS
	struct dsp_callee_regs dsp;
#endif
#ifdef CONFIG_ARC_FPU_SAVE_RESTORE
	struct arc_fpu fpu;
#endif
};
#define INIT_THREAD  { }
struct task_struct;
#define task_pt_regs(p) \
	((struct pt_regs *)(THREAD_SIZE + (void *)task_stack_page(p)) - 1)
#define cpu_relax()		barrier()
#define KSTK_EIP(tsk)   (task_pt_regs(tsk)->ret)
#define KSTK_ESP(tsk)   (task_pt_regs(tsk)->sp)
#define TSK_K_ESP(tsk)		(task_thread_info(tsk)->ksp)
#define TSK_K_REG(tsk, off)	(*((unsigned long *)(TSK_K_ESP(tsk) + \
					sizeof(struct callee_regs) + off)))
#define TSK_K_BLINK(tsk)	TSK_K_REG(tsk, 4)
#define TSK_K_FP(tsk)		TSK_K_REG(tsk, 0)
extern void start_thread(struct pt_regs * regs, unsigned long pc,
			 unsigned long usp);
extern unsigned int __get_wchan(struct task_struct *p);
#endif  
#define TASK_SIZE	0x60000000
#define VMALLOC_START	(PAGE_OFFSET - (CONFIG_ARC_KVADDR_SIZE << 20))
#define VMALLOC_SIZE	((CONFIG_ARC_KVADDR_SIZE << 20) - PMD_SIZE * 4)
#define VMALLOC_END	(VMALLOC_START + VMALLOC_SIZE)
#define USER_KERNEL_GUTTER    (VMALLOC_START - TASK_SIZE)
#define STACK_TOP       TASK_SIZE
#define STACK_TOP_MAX   STACK_TOP
#define TASK_UNMAPPED_BASE      (TASK_SIZE / 3)
#endif  
