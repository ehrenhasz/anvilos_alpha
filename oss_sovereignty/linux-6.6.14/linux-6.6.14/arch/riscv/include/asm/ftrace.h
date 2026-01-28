#ifndef _ASM_RISCV_FTRACE_H
#define _ASM_RISCV_FTRACE_H
#if defined(CONFIG_FUNCTION_GRAPH_TRACER) && defined(CONFIG_FRAME_POINTER)
#define HAVE_FUNCTION_GRAPH_FP_TEST
#endif
#define HAVE_FUNCTION_GRAPH_RET_ADDR_PTR
#if defined(CONFIG_CC_IS_GCC) || CONFIG_CLANG_VERSION >= 130000
#define MCOUNT_NAME _mcount
#else
#define MCOUNT_NAME mcount
#endif
#define ARCH_SUPPORTS_FTRACE_OPS 1
#ifndef __ASSEMBLY__
void MCOUNT_NAME(void);
static inline unsigned long ftrace_call_adjust(unsigned long addr)
{
	return addr;
}
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
struct dyn_arch_ftrace {
};
#endif
#ifdef CONFIG_DYNAMIC_FTRACE
#define MCOUNT_ADDR		((unsigned long)MCOUNT_NAME)
#define JALR_SIGN_MASK		(0x00000800)
#define JALR_OFFSET_MASK	(0x00000fff)
#define AUIPC_OFFSET_MASK	(0xfffff000)
#define AUIPC_PAD		(0x00001000)
#define JALR_SHIFT		20
#define JALR_RA			(0x000080e7)
#define AUIPC_RA		(0x00000097)
#define JALR_T0			(0x000282e7)
#define AUIPC_T0		(0x00000297)
#define NOP4			(0x00000013)
#define to_jalr_t0(offset)						\
	(((offset & JALR_OFFSET_MASK) << JALR_SHIFT) | JALR_T0)
#define to_auipc_t0(offset)						\
	((offset & JALR_SIGN_MASK) ?					\
	(((offset & AUIPC_OFFSET_MASK) + AUIPC_PAD) | AUIPC_T0) :	\
	((offset & AUIPC_OFFSET_MASK) | AUIPC_T0))
#define make_call_t0(caller, callee, call)				\
do {									\
	unsigned int offset =						\
		(unsigned long) callee - (unsigned long) caller;	\
	call[0] = to_auipc_t0(offset);					\
	call[1] = to_jalr_t0(offset);					\
} while (0)
#define to_jalr_ra(offset)						\
	(((offset & JALR_OFFSET_MASK) << JALR_SHIFT) | JALR_RA)
#define to_auipc_ra(offset)						\
	((offset & JALR_SIGN_MASK) ?					\
	(((offset & AUIPC_OFFSET_MASK) + AUIPC_PAD) | AUIPC_RA) :	\
	((offset & AUIPC_OFFSET_MASK) | AUIPC_RA))
#define make_call_ra(caller, callee, call)				\
do {									\
	unsigned int offset =						\
		(unsigned long) callee - (unsigned long) caller;	\
	call[0] = to_auipc_ra(offset);					\
	call[1] = to_jalr_ra(offset);					\
} while (0)
#define MCOUNT_INSN_SIZE 8
#ifndef __ASSEMBLY__
struct dyn_ftrace;
int ftrace_init_nop(struct module *mod, struct dyn_ftrace *rec);
#define ftrace_init_nop ftrace_init_nop
#endif
#endif  
#ifndef __ASSEMBLY__
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
struct fgraph_ret_regs {
	unsigned long a1;
	unsigned long a0;
	unsigned long s0;
	unsigned long ra;
};
static inline unsigned long fgraph_ret_regs_return_value(struct fgraph_ret_regs *ret_regs)
{
	return ret_regs->a0;
}
static inline unsigned long fgraph_ret_regs_frame_pointer(struct fgraph_ret_regs *ret_regs)
{
	return ret_regs->s0;
}
#endif  
#endif
#endif  
