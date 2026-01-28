#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H
#ifdef __KERNEL__
#ifndef __ASSEMBLY__
#include <asm/ptrace.h>
#include <asm/page.h>
#define NSWINS 8
struct thread_info {
	unsigned long		uwinmask;
	struct task_struct	*task;		 
	unsigned long		flags;		 
	int			cpu;		 
	int			preempt_count;	 
	int			softirq_count;
	int			hardirq_count;
	u32 __unused;
	unsigned long ksp;	 
	unsigned long kpc;
	unsigned long kpsr;
	unsigned long kwim;
	struct reg_window32	reg_window[NSWINS];	 
	unsigned long		rwbuf_stkptrs[NSWINS];
	unsigned long		w_saved;
};
#define INIT_THREAD_INFO(tsk)				\
{							\
	.uwinmask	=	0,			\
	.task		=	&tsk,			\
	.flags		=	0,			\
	.cpu		=	0,			\
	.preempt_count	=	INIT_PREEMPT_COUNT,	\
}
register struct thread_info *current_thread_info_reg asm("g6");
#define current_thread_info()   (current_thread_info_reg)
#define THREAD_SIZE_ORDER  1
#endif  
#define THREAD_SIZE		(2 * PAGE_SIZE)
#define TI_UWINMASK	0x00	 
#define TI_TASK		0x04
#define TI_FLAGS	0x08
#define TI_CPU		0x0c
#define TI_PREEMPT	0x10	 
#define TI_SOFTIRQ	0x14	 
#define TI_HARDIRQ	0x18	 
#define TI_KSP		0x20	 
#define TI_KPC		0x24	 
#define TI_KPSR		0x28	 
#define TI_KWIM		0x2c	 
#define TI_REG_WINDOW	0x30
#define TI_RWIN_SPTRS	0x230
#define TI_W_SAVED	0x250
#define TIF_SYSCALL_TRACE	0	 
#define TIF_NOTIFY_RESUME	1	 
#define TIF_SIGPENDING		2	 
#define TIF_NEED_RESCHED	3	 
#define TIF_RESTORE_SIGMASK	4	 
#define TIF_NOTIFY_SIGNAL	5	 
#define TIF_USEDFPU		8	 
#define TIF_POLLING_NRFLAG	9	 
#define TIF_MEMDIE		10	 
#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_NOTIFY_SIGNAL	(1<<TIF_NOTIFY_SIGNAL)
#define _TIF_USEDFPU		(1<<TIF_USEDFPU)
#define _TIF_POLLING_NRFLAG	(1<<TIF_POLLING_NRFLAG)
#define _TIF_DO_NOTIFY_RESUME_MASK	(_TIF_NOTIFY_RESUME | \
					 _TIF_SIGPENDING | _TIF_NOTIFY_SIGNAL)
#define is_32bit_task()	(1)
#endif  
#endif  
