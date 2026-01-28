#ifndef _ASM_RISCV_THREAD_INFO_H
#define _ASM_RISCV_THREAD_INFO_H
#include <asm/page.h>
#include <linux/const.h>
#define THREAD_SIZE_ORDER	CONFIG_THREAD_SIZE_ORDER
#define THREAD_SIZE		(PAGE_SIZE << THREAD_SIZE_ORDER)
#ifdef CONFIG_VMAP_STACK
#define THREAD_ALIGN            (2 * THREAD_SIZE)
#else
#define THREAD_ALIGN            THREAD_SIZE
#endif
#define THREAD_SHIFT            (PAGE_SHIFT + THREAD_SIZE_ORDER)
#define OVERFLOW_STACK_SIZE     SZ_4K
#define SHADOW_OVERFLOW_STACK_SIZE (1024)
#define IRQ_STACK_SIZE		THREAD_SIZE
#ifndef __ASSEMBLY__
#include <asm/processor.h>
#include <asm/csr.h>
struct thread_info {
	unsigned long		flags;		 
	int                     preempt_count;   
	long			kernel_sp;	 
	long			user_sp;	 
	int			cpu;
	unsigned long		syscall_work;	 
};
#define INIT_THREAD_INFO(tsk)			\
{						\
	.flags		= 0,			\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
}
void arch_release_task_struct(struct task_struct *tsk);
int arch_dup_task_struct(struct task_struct *dst, struct task_struct *src);
#endif  
#define TIF_NOTIFY_RESUME	1	 
#define TIF_SIGPENDING		2	 
#define TIF_NEED_RESCHED	3	 
#define TIF_RESTORE_SIGMASK	4	 
#define TIF_MEMDIE		5	 
#define TIF_NOTIFY_SIGNAL	9	 
#define TIF_UPROBE		10	 
#define TIF_32BIT		11	 
#define _TIF_NOTIFY_RESUME	(1 << TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING		(1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define _TIF_NOTIFY_SIGNAL	(1 << TIF_NOTIFY_SIGNAL)
#define _TIF_UPROBE		(1 << TIF_UPROBE)
#define _TIF_WORK_MASK \
	(_TIF_NOTIFY_RESUME | _TIF_SIGPENDING | _TIF_NEED_RESCHED | \
	 _TIF_NOTIFY_SIGNAL | _TIF_UPROBE)
#endif  
