#ifndef _ASM_S390_FTRACE_H
#define _ASM_S390_FTRACE_H
#define HAVE_FUNCTION_GRAPH_RET_ADDR_PTR
#define ARCH_SUPPORTS_FTRACE_OPS 1
#define MCOUNT_INSN_SIZE	6
#ifndef __ASSEMBLY__
#ifdef CONFIG_CC_IS_CLANG
#define ftrace_return_address(n) 0UL
#else
#define ftrace_return_address(n) __builtin_return_address(n)
#endif
void ftrace_caller(void);
extern void *ftrace_func;
struct dyn_arch_ftrace { };
#define MCOUNT_ADDR 0
#define FTRACE_ADDR ((unsigned long)ftrace_caller)
#define KPROBE_ON_FTRACE_NOP	0
#define KPROBE_ON_FTRACE_CALL	1
struct module;
struct dyn_ftrace;
bool ftrace_need_init_nop(void);
#define ftrace_need_init_nop ftrace_need_init_nop
int ftrace_init_nop(struct module *mod, struct dyn_ftrace *rec);
#define ftrace_init_nop ftrace_init_nop
static inline unsigned long ftrace_call_adjust(unsigned long addr)
{
	return addr;
}
struct ftrace_regs {
	struct pt_regs regs;
};
static __always_inline struct pt_regs *arch_ftrace_get_regs(struct ftrace_regs *fregs)
{
	struct pt_regs *regs = &fregs->regs;
	if (test_pt_regs_flag(regs, PIF_FTRACE_FULL_REGS))
		return regs;
	return NULL;
}
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
struct fgraph_ret_regs {
	unsigned long gpr2;
	unsigned long fp;
};
static __always_inline unsigned long fgraph_ret_regs_return_value(struct fgraph_ret_regs *ret_regs)
{
	return ret_regs->gpr2;
}
static __always_inline unsigned long fgraph_ret_regs_frame_pointer(struct fgraph_ret_regs *ret_regs)
{
	return ret_regs->fp;
}
#endif  
static __always_inline unsigned long
ftrace_regs_get_instruction_pointer(const struct ftrace_regs *fregs)
{
	return fregs->regs.psw.addr;
}
static __always_inline void
ftrace_regs_set_instruction_pointer(struct ftrace_regs *fregs,
				    unsigned long ip)
{
	fregs->regs.psw.addr = ip;
}
#define ftrace_regs_get_argument(fregs, n) \
	regs_get_kernel_argument(&(fregs)->regs, n)
#define ftrace_regs_get_stack_pointer(fregs) \
	kernel_stack_pointer(&(fregs)->regs)
#define ftrace_regs_return_value(fregs) \
	regs_return_value(&(fregs)->regs)
#define ftrace_regs_set_return_value(fregs, ret) \
	regs_set_return_value(&(fregs)->regs, ret)
#define ftrace_override_function_with_return(fregs) \
	override_function_with_return(&(fregs)->regs)
#define ftrace_regs_query_register_offset(name) \
	regs_query_register_offset(name)
#ifdef CONFIG_DYNAMIC_FTRACE_WITH_DIRECT_CALLS
static inline void arch_ftrace_set_direct_caller(struct ftrace_regs *fregs, unsigned long addr)
{
	struct pt_regs *regs = &fregs->regs;
	regs->orig_gpr2 = addr;
}
#endif  
#define ARCH_TRACE_IGNORE_COMPAT_SYSCALLS
static inline bool arch_trace_is_compat_syscall(struct pt_regs *regs)
{
	return is_compat_task();
}
#define ARCH_HAS_SYSCALL_MATCH_SYM_NAME
static inline bool arch_syscall_match_sym_name(const char *sym,
					       const char *name)
{
	return !strcmp(sym + 7, name) || !strcmp(sym + 8, name);
}
#endif  
#ifdef CONFIG_FUNCTION_TRACER
#define FTRACE_NOP_INSN .word 0xc004, 0x0000, 0x0000  
#ifndef CC_USING_HOTPATCH
#define FTRACE_GEN_MCOUNT_RECORD(name)		\
	.section __mcount_loc, "a", @progbits;	\
	.quad name;				\
	.previous;
#else  
#define FTRACE_GEN_MCOUNT_RECORD(name)
#endif  
#define FTRACE_GEN_NOP_ASM(name)		\
	FTRACE_GEN_MCOUNT_RECORD(name)		\
	FTRACE_NOP_INSN
#else  
#define FTRACE_GEN_NOP_ASM(name)
#endif  
#endif  
