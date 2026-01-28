#ifndef _ASM_MICROBLAZE_THREAD_INFO_H
#define _ASM_MICROBLAZE_THREAD_INFO_H
#ifdef __KERNEL__
#define THREAD_SHIFT		13
#define THREAD_SIZE		(1 << THREAD_SHIFT)
#define THREAD_SIZE_ORDER	1
#ifndef __ASSEMBLY__
# include <linux/types.h>
# include <asm/processor.h>
struct cpu_context {
	__u32	r1;  
	__u32	r2;
	__u32	r13;
	__u32	r14;
	__u32	r15;
	__u32	r16;
	__u32	r17;
	__u32	r18;
	__u32	r19;
	__u32	r20;
	__u32	r21;
	__u32	r22;
	__u32	r23;
	__u32	r24;
	__u32	r25;
	__u32	r26;
	__u32	r27;
	__u32	r28;
	__u32	r29;
	__u32	r30;
	__u32	msr;
	__u32	ear;
	__u32	esr;
	__u32	fsr;
};
struct thread_info {
	struct task_struct	*task;  
	unsigned long		flags;  
	unsigned long		status;  
	__u32			cpu;  
	__s32			preempt_count;  
	struct cpu_context	cpu_context;
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
	register unsigned long sp asm("r1");
	return (struct thread_info *)(sp & ~(THREAD_SIZE-1));
}
#endif  
#define TIF_SYSCALL_TRACE	0  
#define TIF_NOTIFY_RESUME	1  
#define TIF_SIGPENDING		2  
#define TIF_NEED_RESCHED	3  
#define TIF_SINGLESTEP		4
#define TIF_NOTIFY_SIGNAL	5	 
#define TIF_MEMDIE		6	 
#define TIF_SYSCALL_AUDIT	9        
#define TIF_SECCOMP		10       
#define TIF_POLLING_NRFLAG	16
#define _TIF_SYSCALL_TRACE	(1 << TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME	(1 << TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING		(1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define _TIF_SINGLESTEP		(1 << TIF_SINGLESTEP)
#define _TIF_NOTIFY_SIGNAL	(1 << TIF_NOTIFY_SIGNAL)
#define _TIF_POLLING_NRFLAG	(1 << TIF_POLLING_NRFLAG)
#define _TIF_SYSCALL_AUDIT	(1 << TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP		(1 << TIF_SECCOMP)
#define _TIF_WORK_SYSCALL_MASK  (_TIF_SYSCALL_TRACE | _TIF_SINGLESTEP | \
				 _TIF_SYSCALL_AUDIT | _TIF_SECCOMP)
#define _TIF_WORK_MASK		0x0000FFFE
#define _TIF_ALLWORK_MASK	0x0000FFFF
#define TS_USEDFPU		0x0001
#endif  
#endif  
