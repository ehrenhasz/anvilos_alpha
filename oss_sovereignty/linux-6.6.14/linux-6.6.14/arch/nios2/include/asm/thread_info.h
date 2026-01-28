#ifndef _ASM_NIOS2_THREAD_INFO_H
#define _ASM_NIOS2_THREAD_INFO_H
#ifdef __KERNEL__
#define THREAD_SIZE_ORDER	1
#define THREAD_SIZE		8192  
#ifndef __ASSEMBLY__
struct thread_info {
	struct task_struct	*task;		 
	unsigned long		flags;		 
	__u32			cpu;		 
	int			preempt_count;	 
	struct pt_regs		*regs;
};
#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.flags		= 0,			\
	.cpu		= 0,			\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
}
static inline struct thread_info *current_thread_info(void)
{
	register unsigned long sp asm("sp");
	return (struct thread_info *)(sp & ~(THREAD_SIZE - 1));
}
#endif  
#define TIF_SYSCALL_TRACE	0	 
#define TIF_NOTIFY_RESUME	1	 
#define TIF_SIGPENDING		2	 
#define TIF_NEED_RESCHED	3	 
#define TIF_MEMDIE		4	 
#define TIF_SECCOMP		5	 
#define TIF_SYSCALL_AUDIT	6	 
#define TIF_NOTIFY_SIGNAL	7	 
#define TIF_RESTORE_SIGMASK	9	 
#define TIF_POLLING_NRFLAG	16	 
#define _TIF_SYSCALL_TRACE	(1 << TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME	(1 << TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING		(1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define _TIF_SECCOMP		(1 << TIF_SECCOMP)
#define _TIF_SYSCALL_AUDIT	(1 << TIF_SYSCALL_AUDIT)
#define _TIF_NOTIFY_SIGNAL	(1 << TIF_NOTIFY_SIGNAL)
#define _TIF_RESTORE_SIGMASK	(1 << TIF_RESTORE_SIGMASK)
#define _TIF_POLLING_NRFLAG	(1 << TIF_POLLING_NRFLAG)
#define _TIF_WORK_MASK		0x0000FFFE
#endif  
#endif  
