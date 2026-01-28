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
	long			syscall;	 
};
#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.flags		= _TIF_FIXADE,		\
	.cpu		= 0,			\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
}
#ifdef __VDSO__
extern struct thread_info *__current_thread_info;
#else
register struct thread_info *__current_thread_info __asm__("$28");
#endif
static inline struct thread_info *current_thread_info(void)
{
	return __current_thread_info;
}
#ifdef CONFIG_ARCH_HAS_CURRENT_STACK_POINTER
register unsigned long current_stack_pointer __asm__("sp");
#endif
#endif  
#if defined(CONFIG_PAGE_SIZE_4KB) && defined(CONFIG_32BIT)
#define THREAD_SIZE_ORDER (1)
#endif
#if defined(CONFIG_PAGE_SIZE_4KB) && defined(CONFIG_64BIT)
#define THREAD_SIZE_ORDER (2)
#endif
#ifdef CONFIG_PAGE_SIZE_8KB
#define THREAD_SIZE_ORDER (1)
#endif
#ifdef CONFIG_PAGE_SIZE_16KB
#define THREAD_SIZE_ORDER (0)
#endif
#ifdef CONFIG_PAGE_SIZE_32KB
#define THREAD_SIZE_ORDER (0)
#endif
#ifdef CONFIG_PAGE_SIZE_64KB
#define THREAD_SIZE_ORDER (0)
#endif
#define THREAD_SIZE (PAGE_SIZE << THREAD_SIZE_ORDER)
#define THREAD_MASK (THREAD_SIZE - 1UL)
#define STACK_WARN	(THREAD_SIZE / 8)
#define TIF_SIGPENDING		1	 
#define TIF_NEED_RESCHED	2	 
#define TIF_SYSCALL_AUDIT	3	 
#define TIF_SECCOMP		4	 
#define TIF_NOTIFY_RESUME	5	 
#define TIF_UPROBE		6	 
#define TIF_NOTIFY_SIGNAL	7	 
#define TIF_RESTORE_SIGMASK	9	 
#define TIF_USEDFPU		16	 
#define TIF_MEMDIE		18	 
#define TIF_NOHZ		19	 
#define TIF_FIXADE		20	 
#define TIF_LOGADE		21	 
#define TIF_32BIT_REGS		22	 
#define TIF_32BIT_ADDR		23	 
#define TIF_FPUBOUND		24	 
#define TIF_LOAD_WATCH		25	 
#define TIF_SYSCALL_TRACEPOINT	26	 
#define TIF_32BIT_FPREGS	27	 
#define TIF_HYBRID_FPREGS	28	 
#define TIF_USEDMSA		29	 
#define TIF_MSA_CTX_LIVE	30	 
#define TIF_SYSCALL_TRACE	31	 
#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_SYSCALL_AUDIT	(1<<TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP		(1<<TIF_SECCOMP)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)
#define _TIF_UPROBE		(1<<TIF_UPROBE)
#define _TIF_NOTIFY_SIGNAL	(1<<TIF_NOTIFY_SIGNAL)
#define _TIF_USEDFPU		(1<<TIF_USEDFPU)
#define _TIF_NOHZ		(1<<TIF_NOHZ)
#define _TIF_FIXADE		(1<<TIF_FIXADE)
#define _TIF_LOGADE		(1<<TIF_LOGADE)
#define _TIF_32BIT_REGS		(1<<TIF_32BIT_REGS)
#define _TIF_32BIT_ADDR		(1<<TIF_32BIT_ADDR)
#define _TIF_FPUBOUND		(1<<TIF_FPUBOUND)
#define _TIF_LOAD_WATCH		(1<<TIF_LOAD_WATCH)
#define _TIF_32BIT_FPREGS	(1<<TIF_32BIT_FPREGS)
#define _TIF_HYBRID_FPREGS	(1<<TIF_HYBRID_FPREGS)
#define _TIF_USEDMSA		(1<<TIF_USEDMSA)
#define _TIF_MSA_CTX_LIVE	(1<<TIF_MSA_CTX_LIVE)
#define _TIF_SYSCALL_TRACEPOINT	(1<<TIF_SYSCALL_TRACEPOINT)
#define _TIF_WORK_SYSCALL_ENTRY	(_TIF_NOHZ | _TIF_SYSCALL_TRACE |	\
				 _TIF_SYSCALL_AUDIT | \
				 _TIF_SYSCALL_TRACEPOINT | _TIF_SECCOMP)
#define _TIF_WORK_SYSCALL_EXIT	(_TIF_NOHZ | _TIF_SYSCALL_TRACE |	\
				 _TIF_SYSCALL_AUDIT | _TIF_SYSCALL_TRACEPOINT)
#define _TIF_WORK_MASK		\
	(_TIF_SIGPENDING | _TIF_NEED_RESCHED | _TIF_NOTIFY_RESUME |	\
	 _TIF_UPROBE | _TIF_NOTIFY_SIGNAL)
#define _TIF_ALLWORK_MASK	(_TIF_NOHZ | _TIF_WORK_MASK |		\
				 _TIF_WORK_SYSCALL_EXIT |		\
				 _TIF_SYSCALL_TRACEPOINT)
#if   defined(CONFIG_MIPS_PGD_C0_CONTEXT)
#define SMP_CPUID_REG		20, 0	 
#define ASM_SMP_CPUID_REG	$20
#define SMP_CPUID_PTRSHIFT	48
#else
#define SMP_CPUID_REG		4, 0	 
#define ASM_SMP_CPUID_REG	$4
#define SMP_CPUID_PTRSHIFT	23
#endif
#ifdef CONFIG_64BIT
#define SMP_CPUID_REGSHIFT	(SMP_CPUID_PTRSHIFT + 3)
#else
#define SMP_CPUID_REGSHIFT	(SMP_CPUID_PTRSHIFT + 2)
#endif
#define ASM_CPUID_MFC0		MFC0
#define UASM_i_CPUID_MFC0	UASM_i_MFC0
#endif  
#endif  
