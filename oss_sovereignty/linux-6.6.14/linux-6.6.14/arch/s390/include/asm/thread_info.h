#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H
#include <linux/bits.h>
#ifndef ASM_OFFSETS_C
#include <asm/asm-offsets.h>
#endif
#ifdef CONFIG_KASAN
#define THREAD_SIZE_ORDER 4
#else
#define THREAD_SIZE_ORDER 2
#endif
#define BOOT_STACK_SIZE (PAGE_SIZE << 2)
#define THREAD_SIZE (PAGE_SIZE << THREAD_SIZE_ORDER)
#define STACK_INIT_OFFSET (THREAD_SIZE - STACK_FRAME_OVERHEAD - __PT_SIZE)
#ifndef __ASSEMBLY__
#include <asm/lowcore.h>
#include <asm/page.h>
struct thread_info {
	unsigned long		flags;		 
	unsigned long		syscall_work;	 
	unsigned int		cpu;		 
};
#define INIT_THREAD_INFO(tsk)			\
{						\
	.flags		= 0,			\
}
struct task_struct;
void arch_setup_new_exec(void);
#define arch_setup_new_exec arch_setup_new_exec
#endif
#define TIF_NOTIFY_RESUME	0	 
#define TIF_SIGPENDING		1	 
#define TIF_NEED_RESCHED	2	 
#define TIF_UPROBE		3	 
#define TIF_GUARDED_STORAGE	4	 
#define TIF_PATCH_PENDING	5	 
#define TIF_PGSTE		6	 
#define TIF_NOTIFY_SIGNAL	7	 
#define TIF_ISOLATE_BP_GUEST	9	 
#define TIF_PER_TRAP		10	 
#define TIF_31BIT		16	 
#define TIF_MEMDIE		17	 
#define TIF_RESTORE_SIGMASK	18	 
#define TIF_SINGLE_STEP		19	 
#define TIF_BLOCK_STEP		20	 
#define TIF_UPROBE_SINGLESTEP	21	 
#define TIF_SYSCALL_TRACE	24	 
#define TIF_SYSCALL_AUDIT	25	 
#define TIF_SECCOMP		26	 
#define TIF_SYSCALL_TRACEPOINT	27	 
#define _TIF_NOTIFY_RESUME	BIT(TIF_NOTIFY_RESUME)
#define _TIF_NOTIFY_SIGNAL	BIT(TIF_NOTIFY_SIGNAL)
#define _TIF_SIGPENDING		BIT(TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	BIT(TIF_NEED_RESCHED)
#define _TIF_UPROBE		BIT(TIF_UPROBE)
#define _TIF_GUARDED_STORAGE	BIT(TIF_GUARDED_STORAGE)
#define _TIF_PATCH_PENDING	BIT(TIF_PATCH_PENDING)
#define _TIF_ISOLATE_BP_GUEST	BIT(TIF_ISOLATE_BP_GUEST)
#define _TIF_PER_TRAP		BIT(TIF_PER_TRAP)
#define _TIF_31BIT		BIT(TIF_31BIT)
#define _TIF_SINGLE_STEP	BIT(TIF_SINGLE_STEP)
#define _TIF_SYSCALL_TRACE	BIT(TIF_SYSCALL_TRACE)
#define _TIF_SYSCALL_AUDIT	BIT(TIF_SYSCALL_AUDIT)
#define _TIF_SECCOMP		BIT(TIF_SECCOMP)
#define _TIF_SYSCALL_TRACEPOINT	BIT(TIF_SYSCALL_TRACEPOINT)
#endif  
