#ifndef __ASM_SPARC_PROCESSOR_H
#define __ASM_SPARC_PROCESSOR_H
#include <asm/psr.h>
#include <asm/ptrace.h>
#include <asm/head.h>
#include <asm/signal.h>
#include <asm/page.h>
#define TASK_SIZE	PAGE_OFFSET
#ifdef __KERNEL__
#define STACK_TOP	(PAGE_OFFSET - PAGE_SIZE)
#define STACK_TOP_MAX	STACK_TOP
#endif  
struct task_struct;
#ifdef __KERNEL__
struct fpq {
	unsigned long *insn_addr;
	unsigned long insn;
};
#endif
struct thread_struct {
	struct pt_regs *kregs;
	unsigned int _pad1;
	unsigned long fork_kpsr __attribute__ ((aligned (8)));
	unsigned long fork_kwim;
	unsigned long   float_regs[32] __attribute__ ((aligned (8)));
	unsigned long   fsr;
	unsigned long   fpqdepth;
	struct fpq	fpqueue[16];
};
#define INIT_THREAD  { \
	.kregs = (struct pt_regs *)(init_stack+THREAD_SIZE)-1 \
}
static inline void start_thread(struct pt_regs * regs, unsigned long pc,
				    unsigned long sp)
{
	register unsigned long zero asm("g1");
	regs->psr = (regs->psr & (PSR_CWP)) | PSR_S;
	regs->pc = ((pc & (~3)) - 4);
	regs->npc = regs->pc + 4;
	regs->y = 0;
	zero = 0;
	__asm__ __volatile__("std\t%%g0, [%0 + %3 + 0x00]\n\t"
			     "std\t%%g0, [%0 + %3 + 0x08]\n\t"
			     "std\t%%g0, [%0 + %3 + 0x10]\n\t"
			     "std\t%%g0, [%0 + %3 + 0x18]\n\t"
			     "std\t%%g0, [%0 + %3 + 0x20]\n\t"
			     "std\t%%g0, [%0 + %3 + 0x28]\n\t"
			     "std\t%%g0, [%0 + %3 + 0x30]\n\t"
			     "st\t%1, [%0 + %3 + 0x38]\n\t"
			     "st\t%%g0, [%0 + %3 + 0x3c]"
			     :  
			     : "r" (regs),
			       "r" (sp - sizeof(struct reg_window32)),
			       "r" (zero),
			       "i" ((const unsigned long)(&((struct pt_regs *)0)->u_regs[0]))
			     : "memory");
}
unsigned long __get_wchan(struct task_struct *);
#define task_pt_regs(tsk) ((tsk)->thread.kregs)
#define KSTK_EIP(tsk)  ((tsk)->thread.kregs->pc)
#define KSTK_ESP(tsk)  ((tsk)->thread.kregs->u_regs[UREG_FP])
#ifdef __KERNEL__
extern struct task_struct *last_task_used_math;
int do_mathemu(struct pt_regs *regs, struct task_struct *fpt);
#define cpu_relax()	barrier()
extern void (*sparc_idle)(void);
#endif
#endif  
