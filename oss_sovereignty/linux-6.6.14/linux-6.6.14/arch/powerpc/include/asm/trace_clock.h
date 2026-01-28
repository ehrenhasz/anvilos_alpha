#ifndef _ASM_PPC_TRACE_CLOCK_H
#define _ASM_PPC_TRACE_CLOCK_H
#include <linux/compiler.h>
#include <linux/types.h>
extern u64 notrace trace_clock_ppc_tb(void);
#define ARCH_TRACE_CLOCKS { trace_clock_ppc_tb, "ppc-tb", 0 },
#endif   
