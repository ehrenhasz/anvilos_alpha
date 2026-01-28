#ifndef _XTENSA_PTRACE_H
#define _XTENSA_PTRACE_H
#include <asm/kmem_layout.h>
#include <uapi/asm/ptrace.h>
#define NO_SYSCALL (-1)
#ifndef __ASSEMBLY__
#include <asm/coprocessor.h>
#include <asm/core.h>
struct pt_regs {
	unsigned long pc;		 
	unsigned long ps;		 
	unsigned long depc;		 
	unsigned long exccause;		 
	unsigned long excvaddr;		 
	unsigned long debugcause;	 
	unsigned long wmask;		 
	unsigned long lbeg;		 
	unsigned long lend;		 
	unsigned long lcount;		 
	unsigned long sar;		 
	unsigned long windowbase;	 
	unsigned long windowstart;	 
	unsigned long syscall;		 
	unsigned long icountlevel;	 
	unsigned long scompare1;	 
	unsigned long threadptr;	 
	xtregs_opt_t xtregs_opt;
	int align[0] __attribute__ ((aligned(16)));
	unsigned long areg[XCHAL_NUM_AREGS];
};
# define arch_has_single_step()	(1)
# define task_pt_regs(tsk) ((struct pt_regs*) \
	(task_stack_page(tsk) + KERNEL_STACK_SIZE) - 1)
# define user_mode(regs) (((regs)->ps & 0x00000020)!=0)
# define instruction_pointer(regs) ((regs)->pc)
# define return_pointer(regs) (MAKE_PC_FROM_RA((regs)->areg[0], \
					       (regs)->areg[1]))
# ifndef CONFIG_SMP
#  define profile_pc(regs) instruction_pointer(regs)
# else
#  define profile_pc(regs)						\
	({								\
		in_lock_functions(instruction_pointer(regs)) ?		\
		return_pointer(regs) : instruction_pointer(regs);	\
	})
# endif
#define user_stack_pointer(regs) ((regs)->areg[1])
static inline unsigned long regs_return_value(struct pt_regs *regs)
{
	return regs->areg[2];
}
int do_syscall_trace_enter(struct pt_regs *regs);
void do_syscall_trace_leave(struct pt_regs *regs);
#else	 
# include <asm/asm-offsets.h>
#define PT_REGS_OFFSET	  (KERNEL_STACK_SIZE - PT_USER_SIZE)
#endif	 
#endif	 
