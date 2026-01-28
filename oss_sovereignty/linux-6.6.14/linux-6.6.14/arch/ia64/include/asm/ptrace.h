#ifndef _ASM_IA64_PTRACE_H
#define _ASM_IA64_PTRACE_H
#ifndef ASM_OFFSETS_C
#include <asm/asm-offsets.h>
#endif
#include <uapi/asm/ptrace.h>
#if defined(CONFIG_IA64_PAGE_SIZE_4KB)
# define KERNEL_STACK_SIZE_ORDER		3
#elif defined(CONFIG_IA64_PAGE_SIZE_8KB)
# define KERNEL_STACK_SIZE_ORDER		2
#elif defined(CONFIG_IA64_PAGE_SIZE_16KB)
# define KERNEL_STACK_SIZE_ORDER		1
#else
# define KERNEL_STACK_SIZE_ORDER		0
#endif
#define IA64_RBS_OFFSET			((IA64_TASK_SIZE + IA64_THREAD_INFO_SIZE + 31) & ~31)
#define IA64_STK_OFFSET			((1 << KERNEL_STACK_SIZE_ORDER)*PAGE_SIZE)
#define KERNEL_STACK_SIZE		IA64_STK_OFFSET
#ifndef __ASSEMBLY__
#include <asm/current.h>
#include <asm/page.h>
# define instruction_pointer(regs) ((regs)->cr_iip + ia64_psr(regs)->ri)
# define instruction_pointer_set(regs, val)	\
({						\
	ia64_psr(regs)->ri = (val & 0xf);	\
	regs->cr_iip = (val & ~0xfULL);		\
})
static inline unsigned long user_stack_pointer(struct pt_regs *regs)
{
	return regs->r12;
}
static inline int is_syscall_success(struct pt_regs *regs)
{
	return regs->r10 != -1;
}
static inline long regs_return_value(struct pt_regs *regs)
{
	if (is_syscall_success(regs))
		return regs->r8;
	else
		return -regs->r8;
}
#define profile_pc(regs)						\
({									\
	unsigned long __ip = instruction_pointer(regs);			\
	(__ip & ~3UL) + ((__ip & 3UL) << 2);				\
})
# define task_pt_regs(t)		(((struct pt_regs *) ((char *) (t) + IA64_STK_OFFSET)) - 1)
# define ia64_psr(regs)			((struct ia64_psr *) &(regs)->cr_ipsr)
# define user_mode(regs)		(((struct ia64_psr *) &(regs)->cr_ipsr)->cpl != 0)
# define user_stack(task,regs)	((long) regs - (long) task == IA64_STK_OFFSET - sizeof(*regs))
# define fsys_mode(task,regs)					\
  ({								\
	  struct task_struct *_task = (task);			\
	  struct pt_regs *_regs = (regs);			\
	  !user_mode(_regs) && user_stack(_task, _regs);	\
  })
# define force_successful_syscall_return()	(task_pt_regs(current)->r8 = 0)
  struct task_struct;			 
  struct unw_frame_info;		 
  extern unsigned long ia64_get_user_rbs_end (struct task_struct *, struct pt_regs *,
					      unsigned long *);
  extern long ia64_peek (struct task_struct *, struct switch_stack *, unsigned long,
			 unsigned long, long *);
  extern long ia64_poke (struct task_struct *, struct switch_stack *, unsigned long,
			 unsigned long, long);
  extern void ia64_flush_fph (struct task_struct *);
  extern void ia64_sync_fph (struct task_struct *);
  extern void ia64_sync_krbs(void);
  extern long ia64_sync_user_rbs (struct task_struct *, struct switch_stack *,
				  unsigned long, unsigned long);
  extern unsigned long ia64_get_scratch_nat_bits (struct pt_regs *pt, unsigned long scratch_unat);
  extern unsigned long ia64_put_scratch_nat_bits (struct pt_regs *pt, unsigned long nat);
  extern void ia64_increment_ip (struct pt_regs *pt);
  extern void ia64_decrement_ip (struct pt_regs *pt);
  extern void ia64_ptrace_stop(void);
  #define arch_ptrace_stop() \
	ia64_ptrace_stop()
  #define arch_ptrace_stop_needed() \
	(!test_thread_flag(TIF_RESTORE_RSE))
  #define arch_has_single_step()  (1)
  #define arch_has_block_step()   (1)
#endif  
#endif  
