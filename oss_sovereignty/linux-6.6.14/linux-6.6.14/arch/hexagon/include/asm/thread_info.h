#ifndef _ASM_THREAD_INFO_H
#define _ASM_THREAD_INFO_H
#ifdef __KERNEL__
#ifndef __ASSEMBLY__
#include <asm/processor.h>
#include <asm/registers.h>
#include <asm/page.h>
#endif
#define THREAD_SHIFT		12
#define THREAD_SIZE		(1<<THREAD_SHIFT)
#define THREAD_SIZE_ORDER	(THREAD_SHIFT - PAGE_SHIFT)
#ifndef __ASSEMBLY__
struct thread_info {
	struct task_struct	*task;		 
	unsigned long		flags;           
	__u32                   cpu;             
	int                     preempt_count;   
	struct pt_regs		*regs;
	unsigned long		sp;
};
#else  
#include <asm/asm-offsets.h>
#endif   
#ifndef __ASSEMBLY__
#define INIT_THREAD_INFO(tsk)                   \
{                                               \
	.task           = &tsk,                 \
	.flags          = 0,                    \
	.cpu            = 0,                    \
	.preempt_count  = 1,                    \
	.sp = 0,				\
	.regs = NULL,			\
}
#define	qqstr(s) qstr(s)
#define qstr(s) #s
#define QUOTED_THREADINFO_REG qqstr(THREADINFO_REG)
register struct thread_info *__current_thread_info asm(QUOTED_THREADINFO_REG);
#define current_thread_info()  __current_thread_info
#endif  
#define TIF_SYSCALL_TRACE       0        
#define TIF_NOTIFY_RESUME       1        
#define TIF_SIGPENDING          2        
#define TIF_NEED_RESCHED        3        
#define TIF_SINGLESTEP          4        
#define TIF_RESTORE_SIGMASK     6        
#define TIF_NOTIFY_SIGNAL	7        
#define TIF_MEMDIE              17       
#define _TIF_SYSCALL_TRACE      (1 << TIF_SYSCALL_TRACE)
#define _TIF_NOTIFY_RESUME      (1 << TIF_NOTIFY_RESUME)
#define _TIF_SIGPENDING         (1 << TIF_SIGPENDING)
#define _TIF_NEED_RESCHED       (1 << TIF_NEED_RESCHED)
#define _TIF_SINGLESTEP         (1 << TIF_SINGLESTEP)
#define _TIF_NOTIFY_SIGNAL	(1 << TIF_NOTIFY_SIGNAL)
#define _TIF_WORK_MASK          (0x0000FFFF & ~_TIF_SYSCALL_TRACE)
#define _TIF_ALLWORK_MASK       0x0000FFFF
#endif  
#endif
