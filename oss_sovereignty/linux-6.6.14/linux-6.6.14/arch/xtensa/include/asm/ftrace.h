#ifndef _XTENSA_FTRACE_H
#define _XTENSA_FTRACE_H
#include <asm/processor.h>
#ifndef __ASSEMBLY__
extern unsigned long return_address(unsigned level);
#define ftrace_return_address(n) return_address(n)
#endif  
#ifdef CONFIG_FUNCTION_TRACER
#define MCOUNT_ADDR ((unsigned long)(_mcount))
#define MCOUNT_INSN_SIZE 3
#ifndef __ASSEMBLY__
extern void _mcount(void);
#define mcount _mcount
#endif  
#endif  
#endif  
