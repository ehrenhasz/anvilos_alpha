#ifndef _XTENSA_THREAD_INFO_H
#define _XTENSA_THREAD_INFO_H
#include <linux/stringify.h>
#include <asm/kmem_layout.h>
#define CURRENT_SHIFT KERNEL_STACK_SHIFT
#ifndef __ASSEMBLY__
# include <asm/processor.h>
#endif
#ifndef __ASSEMBLY__
#if XTENSA_HAVE_COPROCESSORS
typedef struct xtregs_coprocessor {
	xtregs_cp0_t cp0;
	xtregs_cp1_t cp1;
	xtregs_cp2_t cp2;
	xtregs_cp3_t cp3;
	xtregs_cp4_t cp4;
	xtregs_cp5_t cp5;
	xtregs_cp6_t cp6;
	xtregs_cp7_t cp7;
} xtregs_coprocessor_t;
#endif
struct thread_info {
	struct task_struct	*task;		 
	unsigned long		flags;		 
	unsigned long		status;		 
	__u32			cpu;		 
	__s32			preempt_count;	 
#if XCHAL_HAVE_EXCLUSIVE
	unsigned long		atomctl8;
#endif
#ifdef CONFIG_USER_ABI_CALL0_PROBE
	unsigned long		ps_woe_fix_addr;
#endif
	unsigned long		cpenable;
	u32			cp_owner_cpu;
#if XTENSA_HAVE_COPROCESSORS
	xtregs_coprocessor_t	xtregs_cp;
#endif
	xtregs_user_t		xtregs_user;
};
#endif
#ifndef __ASSEMBLY__
#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.flags		= 0,			\
	.cpu		= 0,			\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
}
static inline struct thread_info *current_thread_info(void)
{
	struct thread_info *ti;
	 __asm__("extui %0, a1, 0, "__stringify(CURRENT_SHIFT)"\n\t"
	         "xor %0, a1, %0" : "=&r" (ti) : );
	return ti;
}
#else  
#define GET_THREAD_INFO(reg,sp) \
	extui reg, sp, 0, CURRENT_SHIFT; \
	xor   reg, sp, reg
#endif
#define TIF_SYSCALL_TRACE	0	 
#define TIF_SIGPENDING		1	 
#define TIF_NEED_RESCHED	2	 
#define TIF_SINGLESTEP		3	 
#define TIF_SYSCALL_TRACEPOINT	4	 
#define TIF_NOTIFY_SIGNAL	5	 
#define TIF_RESTORE_SIGMASK	6	 
#define TIF_NOTIFY_RESUME	7	 
#define TIF_DB_DISABLED		8	 
#define TIF_SYSCALL_AUDIT	9	 
#define TIF_SECCOMP		10	 
#define TIF_MEMDIE		11	 
#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_SINGLESTEP		(1<<TIF_SINGLESTEP)
#define _TIF_SYSCALL_TRACEPOINT	(1<<TIF_SYSCALL_TRACEPOINT)
#define _TIF_NOTIFY_SIGNAL	(1<<TIF_NOTIFY_SIGNAL)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)
#define _TIF_SYSCALL_AUDIT	(1<<TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP		(1<<TIF_SECCOMP)
#define _TIF_WORK_MASK		(_TIF_SYSCALL_TRACE | _TIF_SINGLESTEP | \
				 _TIF_SYSCALL_TRACEPOINT | \
				 _TIF_SYSCALL_AUDIT | _TIF_SECCOMP)
#define THREAD_SIZE KERNEL_STACK_SIZE
#define THREAD_SIZE_ORDER (KERNEL_STACK_SHIFT - PAGE_SHIFT)
#endif	 
