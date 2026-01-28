#ifndef __UM_THREAD_INFO_H
#define __UM_THREAD_INFO_H
#define THREAD_SIZE_ORDER CONFIG_KERNEL_STACK_ORDER
#define THREAD_SIZE ((1 << CONFIG_KERNEL_STACK_ORDER) * PAGE_SIZE)
#ifndef __ASSEMBLY__
#include <asm/types.h>
#include <asm/page.h>
#include <asm/segment.h>
#include <sysdep/ptrace_user.h>
struct thread_info {
	struct task_struct	*task;		 
	unsigned long		flags;		 
	__u32			cpu;		 
	int			preempt_count;   
	struct thread_info	*real_thread;     
	unsigned long aux_fp_regs[FP_SIZE];	 
};
#define INIT_THREAD_INFO(tsk)			\
{						\
	.task =		&tsk,			\
	.flags =		0,		\
	.cpu =		0,			\
	.preempt_count = INIT_PREEMPT_COUNT,	\
	.real_thread = NULL,			\
}
static inline struct thread_info *current_thread_info(void)
{
	struct thread_info *ti;
	unsigned long mask = THREAD_SIZE - 1;
	void *p;
	asm volatile ("" : "=r" (p) : "0" (&ti));
	ti = (struct thread_info *) (((unsigned long)p) & ~mask);
	return ti;
}
#endif
#define TIF_SYSCALL_TRACE	0	 
#define TIF_SIGPENDING		1	 
#define TIF_NEED_RESCHED	2	 
#define TIF_NOTIFY_SIGNAL	3	 
#define TIF_RESTART_BLOCK	4
#define TIF_MEMDIE		5	 
#define TIF_SYSCALL_AUDIT	6
#define TIF_RESTORE_SIGMASK	7
#define TIF_NOTIFY_RESUME	8
#define TIF_SECCOMP		9	 
#define TIF_SINGLESTEP		10	 
#define _TIF_SYSCALL_TRACE	(1 << TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define _TIF_NOTIFY_SIGNAL	(1 << TIF_NOTIFY_SIGNAL)
#define _TIF_MEMDIE		(1 << TIF_MEMDIE)
#define _TIF_SYSCALL_AUDIT	(1 << TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP		(1 << TIF_SECCOMP)
#define _TIF_SINGLESTEP		(1 << TIF_SINGLESTEP)
#endif
