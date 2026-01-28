#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H
#ifdef __KERNEL__
#ifndef __ASSEMBLY__
#include <asm/processor.h>
struct thread_info {
	struct task_struct	*task;		 
	unsigned long		flags;		 
	unsigned long		tp_value;	 
	__u32			cpu;		 
	int			preempt_count;	 
	struct pt_regs		*regs;
	unsigned long		syscall;	 
	unsigned long		syscall_work;	 
};
#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.flags		= _TIF_FIXADE,		\
	.cpu		= 0,			\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
}
register struct thread_info *__current_thread_info __asm__("$tp");
static inline struct thread_info *current_thread_info(void)
{
	return __current_thread_info;
}
register unsigned long current_stack_pointer __asm__("$sp");
#endif  
#define THREAD_SIZE		SZ_16K
#define THREAD_MASK		(THREAD_SIZE - 1UL)
#define THREAD_SIZE_ORDER	ilog2(THREAD_SIZE / PAGE_SIZE)
#define TIF_SIGPENDING		1	 
#define TIF_NEED_RESCHED	2	 
#define TIF_NOTIFY_RESUME	3	 
#define TIF_NOTIFY_SIGNAL	4	 
#define TIF_RESTORE_SIGMASK	5	 
#define TIF_NOHZ		6	 
#define TIF_UPROBE		7	 
#define TIF_USEDFPU		8	 
#define TIF_USEDSIMD		9	 
#define TIF_MEMDIE		10	 
#define TIF_FIXADE		11	 
#define TIF_LOGADE		12	 
#define TIF_32BIT_REGS		13	 
#define TIF_32BIT_ADDR		14	 
#define TIF_LOAD_WATCH		15	 
#define TIF_SINGLESTEP		16	 
#define TIF_LSX_CTX_LIVE	17	 
#define TIF_LASX_CTX_LIVE	18	 
#define TIF_USEDLBT		19	 
#define TIF_LBT_CTX_LIVE	20	 
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)
#define _TIF_NOTIFY_SIGNAL	(1<<TIF_NOTIFY_SIGNAL)
#define _TIF_NOHZ		(1<<TIF_NOHZ)
#define _TIF_UPROBE		(1<<TIF_UPROBE)
#define _TIF_USEDFPU		(1<<TIF_USEDFPU)
#define _TIF_USEDSIMD		(1<<TIF_USEDSIMD)
#define _TIF_FIXADE		(1<<TIF_FIXADE)
#define _TIF_LOGADE		(1<<TIF_LOGADE)
#define _TIF_32BIT_REGS		(1<<TIF_32BIT_REGS)
#define _TIF_32BIT_ADDR		(1<<TIF_32BIT_ADDR)
#define _TIF_LOAD_WATCH		(1<<TIF_LOAD_WATCH)
#define _TIF_SINGLESTEP		(1<<TIF_SINGLESTEP)
#define _TIF_LSX_CTX_LIVE	(1<<TIF_LSX_CTX_LIVE)
#define _TIF_LASX_CTX_LIVE	(1<<TIF_LASX_CTX_LIVE)
#define _TIF_USEDLBT		(1<<TIF_USEDLBT)
#define _TIF_LBT_CTX_LIVE	(1<<TIF_LBT_CTX_LIVE)
#endif  
#endif  
