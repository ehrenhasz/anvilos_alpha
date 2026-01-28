#ifndef __ASM_FTRACE_H
#define __ASM_FTRACE_H
#include <asm/insn.h>
#define HAVE_FUNCTION_GRAPH_FP_TEST
#define HAVE_FUNCTION_GRAPH_RET_ADDR_PTR
#ifdef CONFIG_DYNAMIC_FTRACE_WITH_ARGS
#define ARCH_SUPPORTS_FTRACE_OPS 1
#else
#define MCOUNT_ADDR		((unsigned long)_mcount)
#endif
#define MCOUNT_INSN_SIZE	AARCH64_INSN_SIZE
#define FTRACE_PLT_IDX		0
#define NR_FTRACE_PLTS		1
#define ARCH_FTRACE_SHIFT_STACK_TRACER 1
#ifndef __ASSEMBLY__
#include <linux/compat.h>
extern void _mcount(unsigned long);
extern void *return_address(unsigned int);
struct dyn_arch_ftrace {
};
extern unsigned long ftrace_graph_call;
extern void return_to_handler(void);
unsigned long ftrace_call_adjust(unsigned long addr);
#ifdef CONFIG_DYNAMIC_FTRACE_WITH_ARGS
struct dyn_ftrace;
struct ftrace_ops;
#define arch_ftrace_get_regs(regs) NULL
struct ftrace_regs {
	unsigned long regs[9];
#ifdef CONFIG_DYNAMIC_FTRACE_WITH_DIRECT_CALLS
	unsigned long direct_tramp;
#else
	unsigned long __unused;
#endif
	unsigned long fp;
	unsigned long lr;
	unsigned long sp;
	unsigned long pc;
};
static __always_inline unsigned long
ftrace_regs_get_instruction_pointer(const struct ftrace_regs *fregs)
{
	return fregs->pc;
}
static __always_inline void
ftrace_regs_set_instruction_pointer(struct ftrace_regs *fregs,
				    unsigned long pc)
{
	fregs->pc = pc;
}
static __always_inline unsigned long
ftrace_regs_get_stack_pointer(const struct ftrace_regs *fregs)
{
	return fregs->sp;
}
static __always_inline unsigned long
ftrace_regs_get_argument(struct ftrace_regs *fregs, unsigned int n)
{
	if (n < 8)
		return fregs->regs[n];
	return 0;
}
static __always_inline unsigned long
ftrace_regs_get_return_value(const struct ftrace_regs *fregs)
{
	return fregs->regs[0];
}
static __always_inline void
ftrace_regs_set_return_value(struct ftrace_regs *fregs,
			     unsigned long ret)
{
	fregs->regs[0] = ret;
}
static __always_inline void
ftrace_override_function_with_return(struct ftrace_regs *fregs)
{
	fregs->pc = fregs->lr;
}
int ftrace_regs_query_register_offset(const char *name);
int ftrace_init_nop(struct module *mod, struct dyn_ftrace *rec);
#define ftrace_init_nop ftrace_init_nop
void ftrace_graph_func(unsigned long ip, unsigned long parent_ip,
		       struct ftrace_ops *op, struct ftrace_regs *fregs);
#define ftrace_graph_func ftrace_graph_func
#ifdef CONFIG_DYNAMIC_FTRACE_WITH_DIRECT_CALLS
static inline void arch_ftrace_set_direct_caller(struct ftrace_regs *fregs,
						 unsigned long addr)
{
	fregs->direct_tramp = addr;
}
#endif  
#endif
#define ftrace_return_address(n) return_address(n)
#define ARCH_TRACE_IGNORE_COMPAT_SYSCALLS
static inline bool arch_trace_is_compat_syscall(struct pt_regs *regs)
{
	return is_compat_task();
}
#define ARCH_HAS_SYSCALL_MATCH_SYM_NAME
static inline bool arch_syscall_match_sym_name(const char *sym,
					       const char *name)
{
	return !strcmp(sym + 8, name);
}
#endif  
#ifndef __ASSEMBLY__
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
struct fgraph_ret_regs {
	unsigned long regs[8];
	unsigned long fp;
	unsigned long __unused;
};
static inline unsigned long fgraph_ret_regs_return_value(struct fgraph_ret_regs *ret_regs)
{
	return ret_regs->regs[0];
}
static inline unsigned long fgraph_ret_regs_frame_pointer(struct fgraph_ret_regs *ret_regs)
{
	return ret_regs->fp;
}
void prepare_ftrace_return(unsigned long self_addr, unsigned long *parent,
			   unsigned long frame_pointer);
#endif  
#endif
#endif  
