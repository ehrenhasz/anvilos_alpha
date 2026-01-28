#ifndef _ASM_PTRACE_H
#define _ASM_PTRACE_H
#include <asm/registers.h>
#define instruction_pointer(regs) pt_elr(regs)
#define user_stack_pointer(regs) ((regs)->r29)
#define profile_pc(regs) instruction_pointer(regs)
extern int regs_query_register_offset(const char *name);
extern const char *regs_query_register_name(unsigned int offset);
#define current_pt_regs() \
	((struct pt_regs *) \
	 ((unsigned long)current_thread_info() + THREAD_SIZE) - 1)
#if CONFIG_HEXAGON_ARCH_VERSION >= 4
#define arch_has_single_step()	(1)
#endif
#endif
