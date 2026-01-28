#ifndef _ASM_SPARC64_FTRACE
#define _ASM_SPARC64_FTRACE
#ifdef CONFIG_MCOUNT
#define MCOUNT_ADDR		((unsigned long)(_mcount))
#define MCOUNT_INSN_SIZE	4  
#ifndef __ASSEMBLY__
void _mcount(void);
#endif
#endif  
#if defined(CONFIG_SPARC64) && !defined(CC_USE_FENTRY)
#define HAVE_FUNCTION_GRAPH_FP_TEST
#endif
#ifdef CONFIG_DYNAMIC_FTRACE
static inline unsigned long ftrace_call_adjust(unsigned long addr)
{
	return addr;
}
struct dyn_arch_ftrace {
};
#endif  
unsigned long prepare_ftrace_return(unsigned long parent,
				    unsigned long self_addr,
				    unsigned long frame_pointer);
#endif  
