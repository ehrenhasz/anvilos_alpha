#ifndef __ASM_SH_THREAD_INFO_H
#define __ASM_SH_THREAD_INFO_H
#include <asm/page.h>
#define FAULT_CODE_WRITE	(1 << 0)	 
#define FAULT_CODE_INITIAL	(1 << 1)	 
#define FAULT_CODE_ITLB		(1 << 2)	 
#define FAULT_CODE_PROT		(1 << 3)	 
#define FAULT_CODE_USER		(1 << 4)	 
#ifndef __ASSEMBLY__
#include <asm/processor.h>
struct thread_info {
	struct task_struct	*task;		 
	unsigned long		flags;		 
	__u32			status;		 
	__u32			cpu;
	int			preempt_count;  
	unsigned long		previous_sp;	 
	__u8			supervisor_stack[];
};
#endif
#if defined(CONFIG_4KSTACKS)
#define THREAD_SHIFT	12
#else
#define THREAD_SHIFT	13
#endif
#define THREAD_SIZE	(1 << THREAD_SHIFT)
#define STACK_WARN	(THREAD_SIZE >> 3)
#ifndef __ASSEMBLY__
#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.flags		= 0,			\
	.status		= 0,			\
	.cpu		= 0,			\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
}
register unsigned long current_stack_pointer asm("r15") __used;
static inline struct thread_info *current_thread_info(void)
{
	struct thread_info *ti;
#if defined(CONFIG_CPU_HAS_SR_RB)
	__asm__ __volatile__ ("stc	r7_bank, %0" : "=r" (ti));
#else
	unsigned long __dummy;
	__asm__ __volatile__ (
		"mov	r15, %0\n\t"
		"and	%1, %0\n\t"
		: "=&r" (ti), "=r" (__dummy)
		: "1" (~(THREAD_SIZE - 1))
		: "memory");
#endif
	return ti;
}
#define THREAD_SIZE_ORDER	(THREAD_SHIFT - PAGE_SHIFT)
extern void init_thread_xstate(void);
#endif  
#define TIF_SYSCALL_TRACE	0	 
#define TIF_SIGPENDING		1	 
#define TIF_NEED_RESCHED	2	 
#define TIF_NOTIFY_SIGNAL	3	 
#define TIF_SINGLESTEP		4	 
#define TIF_SYSCALL_AUDIT	5	 
#define TIF_SECCOMP		6	 
#define TIF_NOTIFY_RESUME	7	 
#define TIF_SYSCALL_TRACEPOINT	8	 
#define TIF_POLLING_NRFLAG	17	 
#define TIF_MEMDIE		18	 
#define _TIF_SYSCALL_TRACE	(1 << TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define _TIF_NOTIFY_SIGNAL	(1 << TIF_NOTIFY_SIGNAL)
#define _TIF_SINGLESTEP		(1 << TIF_SINGLESTEP)
#define _TIF_SYSCALL_AUDIT	(1 << TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP		(1 << TIF_SECCOMP)
#define _TIF_NOTIFY_RESUME	(1 << TIF_NOTIFY_RESUME)
#define _TIF_SYSCALL_TRACEPOINT	(1 << TIF_SYSCALL_TRACEPOINT)
#define _TIF_POLLING_NRFLAG	(1 << TIF_POLLING_NRFLAG)
#define _TIF_WORK_SYSCALL_MASK	(_TIF_SYSCALL_TRACE | _TIF_SINGLESTEP | \
				 _TIF_SYSCALL_AUDIT | _TIF_SECCOMP    | \
				 _TIF_SYSCALL_TRACEPOINT)
#define _TIF_ALLWORK_MASK	(_TIF_SYSCALL_TRACE | _TIF_SIGPENDING      | \
				 _TIF_NEED_RESCHED  | _TIF_SYSCALL_AUDIT   | \
				 _TIF_SINGLESTEP    | _TIF_NOTIFY_RESUME   | \
				 _TIF_SYSCALL_TRACEPOINT | _TIF_NOTIFY_SIGNAL)
#define _TIF_WORK_MASK		(_TIF_ALLWORK_MASK & ~(_TIF_SYSCALL_TRACE | \
				 _TIF_SYSCALL_AUDIT | _TIF_SINGLESTEP))
#define TS_USEDFPU		0x0002	 
#ifndef __ASSEMBLY__
#define TI_FLAG_FAULT_CODE_SHIFT	24
static inline void set_thread_fault_code(unsigned int val)
{
	struct thread_info *ti = current_thread_info();
	ti->flags = (ti->flags & (~0 >> (32 - TI_FLAG_FAULT_CODE_SHIFT)))
		| (val << TI_FLAG_FAULT_CODE_SHIFT);
}
static inline unsigned int get_thread_fault_code(void)
{
	struct thread_info *ti = current_thread_info();
	return ti->flags >> TI_FLAG_FAULT_CODE_SHIFT;
}
#endif	 
#endif  
