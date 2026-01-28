#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H
#ifdef __KERNEL__
#ifndef __ASSEMBLY__
#include <asm/types.h>
#include <asm/processor.h>
#endif
#define THREAD_SIZE_ORDER 0
#define THREAD_SIZE       (PAGE_SIZE << THREAD_SIZE_ORDER)
#ifndef __ASSEMBLY__
struct thread_info {
	struct task_struct	*task;		 
	unsigned long		flags;		 
	__u32			cpu;		 
	__s32			preempt_count;  
	__u8			supervisor_stack[0];
	unsigned long           ksp;
};
#endif
#ifndef __ASSEMBLY__
#define INIT_THREAD_INFO(tsk)				\
{							\
	.task		= &tsk,				\
	.flags		= 0,				\
	.cpu		= 0,				\
	.preempt_count	= INIT_PREEMPT_COUNT,		\
	.ksp            = 0,                            \
}
register struct thread_info *current_thread_info_reg asm("r10");
#define current_thread_info()   (current_thread_info_reg)
#define get_thread_info(ti) get_task_struct((ti)->task)
#define put_thread_info(ti) put_task_struct((ti)->task)
#endif  
#define TIF_SYSCALL_TRACE	0	 
#define TIF_NOTIFY_RESUME	1	 
#define TIF_SIGPENDING		2	 
#define TIF_NEED_RESCHED	3	 
#define TIF_SINGLESTEP		4	 
#define TIF_NOTIFY_SIGNAL	5	 
#define TIF_SYSCALL_TRACEPOINT  8        
#define TIF_RESTORE_SIGMASK     9
#define TIF_POLLING_NRFLAG	16	 
#define TIF_MEMDIE              17
#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_SINGLESTEP		(1<<TIF_SINGLESTEP)
#define _TIF_NOTIFY_SIGNAL	(1<<TIF_NOTIFY_SIGNAL)
#define _TIF_POLLING_NRFLAG	(1<<TIF_POLLING_NRFLAG)
#define _TIF_WORK_MASK (0xff & ~(_TIF_SYSCALL_TRACE|_TIF_SINGLESTEP))
#endif  
#endif  
