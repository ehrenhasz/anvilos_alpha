#ifndef _ASM_SH_STACKTRACE_H
#define _ASM_SH_STACKTRACE_H
struct stacktrace_ops {
	void (*address)(void *data, unsigned long address, int reliable);
};
void dump_trace(struct task_struct *tsk, struct pt_regs *regs,
		unsigned long *stack,
		const struct stacktrace_ops *ops, void *data);
#endif  
