#ifndef _ASM_M68K_THREAD_INFO_H
#define _ASM_M68K_THREAD_INFO_H
#include <asm/types.h>
#include <asm/page.h>
#if PAGE_SHIFT < 13
#ifdef CONFIG_4KSTACKS
#define THREAD_SIZE	4096
#else
#define THREAD_SIZE	8192
#endif
#else
#define THREAD_SIZE	PAGE_SIZE
#endif
#define THREAD_SIZE_ORDER	((THREAD_SIZE / PAGE_SIZE) - 1)
#ifndef __ASSEMBLY__
struct thread_info {
	struct task_struct	*task;		 
	unsigned long		flags;
	int			preempt_count;	 
	__u32			cpu;		 
	unsigned long		tp_value;	 
};
#endif  
#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
}
#ifndef __ASSEMBLY__
static inline struct thread_info *current_thread_info(void)
{
	struct thread_info *ti;
	__asm__(
		"move.l %%sp, %0 \n\t"
		"and.l  %1, %0"
		: "=&d"(ti)
		: "di" (~(THREAD_SIZE-1))
		);
	return ti;
}
#endif
#define TIF_NOTIFY_SIGNAL	4
#define TIF_NOTIFY_RESUME	5	 
#define TIF_SIGPENDING		6	 
#define TIF_NEED_RESCHED	7	 
#define TIF_SECCOMP		13	 
#define TIF_DELAYED_TRACE	14	 
#define TIF_SYSCALL_TRACE	15	 
#define TIF_MEMDIE		16	 
#define TIF_RESTORE_SIGMASK	18	 
#define _TIF_NOTIFY_RESUME	(1 << TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING		(1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define _TIF_SECCOMP		(1 << TIF_SECCOMP)
#define _TIF_DELAYED_TRACE	(1 << TIF_DELAYED_TRACE)
#define _TIF_SYSCALL_TRACE	(1 << TIF_SYSCALL_TRACE)
#define _TIF_MEMDIE		(1 << TIF_MEMDIE)
#define _TIF_RESTORE_SIGMASK	(1 << TIF_RESTORE_SIGMASK)
#endif	 
