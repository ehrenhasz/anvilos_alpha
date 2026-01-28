#ifndef __ASM_SH_FTRACE_H
#define __ASM_SH_FTRACE_H
#ifdef CONFIG_FUNCTION_TRACER
#define MCOUNT_INSN_SIZE	4  
#define FTRACE_SYSCALL_MAX	NR_syscalls
#ifndef __ASSEMBLY__
extern void mcount(void);
#define MCOUNT_ADDR		((unsigned long)(mcount))
#ifdef CONFIG_DYNAMIC_FTRACE
#define CALL_ADDR		((long)(ftrace_call))
#define STUB_ADDR		((long)(ftrace_stub))
#define GRAPH_ADDR		((long)(ftrace_graph_call))
#define CALLER_ADDR		((long)(ftrace_caller))
#define MCOUNT_INSN_OFFSET	((STUB_ADDR - CALL_ADDR) - 4)
#define GRAPH_INSN_OFFSET	((CALLER_ADDR - GRAPH_ADDR) - 4)
struct dyn_arch_ftrace {
};
#endif  
static inline unsigned long ftrace_call_adjust(unsigned long addr)
{
	return addr;
}
#endif  
#endif  
#ifndef __ASSEMBLY__
extern void *return_address(unsigned int);
#define ftrace_return_address(n) return_address(n)
#endif  
#endif  
