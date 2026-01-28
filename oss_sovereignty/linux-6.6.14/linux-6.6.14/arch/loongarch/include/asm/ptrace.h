#ifndef _ASM_PTRACE_H
#define _ASM_PTRACE_H
#include <asm/page.h>
#include <asm/irqflags.h>
#include <asm/thread_info.h>
#include <uapi/asm/ptrace.h>
struct pt_regs {
	unsigned long regs[32];
	unsigned long orig_a0;
	unsigned long csr_era;
	unsigned long csr_badvaddr;
	unsigned long csr_crmd;
	unsigned long csr_prmd;
	unsigned long csr_euen;
	unsigned long csr_ecfg;
	unsigned long csr_estat;
	unsigned long __last[];
} __aligned(8);
static inline int regs_irqs_disabled(struct pt_regs *regs)
{
	return arch_irqs_disabled_flags(regs->csr_prmd);
}
static inline unsigned long kernel_stack_pointer(struct pt_regs *regs)
{
	return regs->regs[3];
}
static inline void instruction_pointer_set(struct pt_regs *regs, unsigned long val)
{
	regs->csr_era = val;
}
extern int regs_query_register_offset(const char *name);
#define MAX_REG_OFFSET (offsetof(struct pt_regs, __last))
static inline unsigned long regs_get_register(struct pt_regs *regs, unsigned int offset)
{
	if (unlikely(offset > MAX_REG_OFFSET))
		return 0;
	return *(unsigned long *)((unsigned long)regs + offset);
}
static inline int regs_within_kernel_stack(struct pt_regs *regs, unsigned long addr)
{
	return ((addr & ~(THREAD_SIZE - 1))  ==
		(kernel_stack_pointer(regs) & ~(THREAD_SIZE - 1)));
}
static inline unsigned long regs_get_kernel_stack_nth(struct pt_regs *regs, unsigned int n)
{
	unsigned long *addr = (unsigned long *)kernel_stack_pointer(regs);
	addr += n;
	if (regs_within_kernel_stack(regs, (unsigned long)addr))
		return *addr;
	else
		return 0;
}
struct task_struct;
static inline unsigned long regs_get_kernel_argument(struct pt_regs *regs,
						     unsigned int n)
{
#define NR_REG_ARGUMENTS 8
	static const unsigned int args[] = {
		offsetof(struct pt_regs, regs[4]),
		offsetof(struct pt_regs, regs[5]),
		offsetof(struct pt_regs, regs[6]),
		offsetof(struct pt_regs, regs[7]),
		offsetof(struct pt_regs, regs[8]),
		offsetof(struct pt_regs, regs[9]),
		offsetof(struct pt_regs, regs[10]),
		offsetof(struct pt_regs, regs[11]),
	};
	if (n < NR_REG_ARGUMENTS)
		return regs_get_register(regs, args[n]);
	else {
		n -= NR_REG_ARGUMENTS;
		return regs_get_kernel_stack_nth(regs, n);
	}
}
#define user_mode(regs) (((regs)->csr_prmd & PLV_MASK) == PLV_USER)
static inline long regs_return_value(struct pt_regs *regs)
{
	return regs->regs[4];
}
static inline void regs_set_return_value(struct pt_regs *regs, unsigned long val)
{
	regs->regs[4] = val;
}
#define instruction_pointer(regs) ((regs)->csr_era)
#define profile_pc(regs) instruction_pointer(regs)
extern void die(const char *str, struct pt_regs *regs);
static inline void die_if_kernel(const char *str, struct pt_regs *regs)
{
	if (unlikely(!user_mode(regs)))
		die(str, regs);
}
#define current_pt_regs()						\
({									\
	unsigned long sp = (unsigned long)__builtin_frame_address(0);	\
	(struct pt_regs *)((sp | (THREAD_SIZE - 1)) + 1) - 1;		\
})
static inline unsigned long user_stack_pointer(struct pt_regs *regs)
{
	return regs->regs[3];
}
static inline void user_stack_pointer_set(struct pt_regs *regs,
	unsigned long val)
{
	regs->regs[3] = val;
}
#ifdef CONFIG_HAVE_HW_BREAKPOINT
#define arch_has_single_step()		(1)
#endif
#endif  
