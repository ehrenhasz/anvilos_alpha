#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H
#include <asm/page.h>
#ifdef CONFIG_16KSTACKS
#define THREAD_SIZE_ORDER 1
#else
#define THREAD_SIZE_ORDER 0
#endif
#define THREAD_SIZE     (PAGE_SIZE << THREAD_SIZE_ORDER)
#define THREAD_SHIFT	(PAGE_SHIFT << THREAD_SIZE_ORDER)
#ifndef __ASSEMBLY__
#include <linux/thread_info.h>
struct thread_info {
	unsigned long flags;		 
	unsigned long ksp;		 
	int preempt_count;		 
	int cpu;			 
	unsigned long thr_ptr;		 
	struct task_struct *task;	 
};
#define INIT_THREAD_INFO(tsk)			\
{						\
	.task       = &tsk,			\
	.flags      = 0,			\
	.cpu        = 0,			\
	.preempt_count  = INIT_PREEMPT_COUNT,	\
}
static inline __attribute_const__ struct thread_info *current_thread_info(void)
{
	register unsigned long sp asm("sp");
	return (struct thread_info *)(sp & ~(THREAD_SIZE - 1));
}
#endif  
#define TIF_RESTORE_SIGMASK	0	 
#define TIF_NOTIFY_RESUME	1	 
#define TIF_SIGPENDING		2	 
#define TIF_NEED_RESCHED	3	 
#define TIF_SYSCALL_AUDIT	4	 
#define TIF_NOTIFY_SIGNAL	5	 
#define TIF_SYSCALL_TRACE	15	 
#define TIF_MEMDIE		16
#define TIF_SYSCALL_TRACEPOINT	17	 
#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_SYSCALL_AUDIT	(1<<TIF_SYSCALL_AUDIT)
#define _TIF_NOTIFY_SIGNAL	(1<<TIF_NOTIFY_SIGNAL)
#define _TIF_MEMDIE		(1<<TIF_MEMDIE)
#define _TIF_SYSCALL_TRACEPOINT	(1<<TIF_SYSCALL_TRACEPOINT)
#define _TIF_WORK_MASK		(_TIF_NEED_RESCHED | _TIF_SIGPENDING | \
				 _TIF_NOTIFY_RESUME | _TIF_NOTIFY_SIGNAL)
#define _TIF_SYSCALL_WORK	(_TIF_SYSCALL_TRACE | _TIF_SYSCALL_TRACEPOINT)
#endif  
