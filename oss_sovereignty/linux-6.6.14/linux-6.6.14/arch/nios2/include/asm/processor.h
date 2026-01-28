#ifndef _ASM_NIOS2_PROCESSOR_H
#define _ASM_NIOS2_PROCESSOR_H
#include <asm/ptrace.h>
#include <asm/registers.h>
#include <asm/page.h>
#define NIOS2_FLAG_KTHREAD	0x00000001	 
#define NIOS2_OP_NOP		0x1883a
#define NIOS2_OP_BREAK		0x3da03a
#ifdef __KERNEL__
#define STACK_TOP	TASK_SIZE
#define STACK_TOP_MAX	STACK_TOP
#endif  
#define KUSER_BASE		0x1000
#define KUSER_SIZE		(PAGE_SIZE)
#ifndef __ASSEMBLY__
# define TASK_SIZE		0x7FFF0000UL
# define TASK_UNMAPPED_BASE	(PAGE_ALIGN(TASK_SIZE / 3))
struct thread_struct {
	struct pt_regs *kregs;
	unsigned long ksp;
	unsigned long kpsr;
};
# define INIT_THREAD {			\
	.kregs	= NULL,			\
	.ksp	= 0,			\
	.kpsr	= 0,			\
}
extern void start_thread(struct pt_regs *regs, unsigned long pc,
			unsigned long sp);
struct task_struct;
extern unsigned long __get_wchan(struct task_struct *p);
#define task_pt_regs(p) \
	((struct pt_regs *)(THREAD_SIZE + task_stack_page(p)) - 1)
#define KSTK_EIP(tsk)	((tsk)->thread.kregs->ea)
#define KSTK_ESP(tsk)	((tsk)->thread.kregs->sp)
#define cpu_relax()	barrier()
#endif  
#endif  
