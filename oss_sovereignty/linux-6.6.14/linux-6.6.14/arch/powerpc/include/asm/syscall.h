#ifndef _ASM_SYSCALL_H
#define _ASM_SYSCALL_H	1
#include <uapi/linux/audit.h>
#include <linux/sched.h>
#include <linux/thread_info.h>
#ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
typedef long (*syscall_fn)(const struct pt_regs *);
#else
typedef long (*syscall_fn)(unsigned long, unsigned long, unsigned long,
			   unsigned long, unsigned long, unsigned long);
#endif
extern const syscall_fn sys_call_table[];
extern const syscall_fn compat_sys_call_table[];
static inline int syscall_get_nr(struct task_struct *task, struct pt_regs *regs)
{
	if (trap_is_syscall(regs))
		return regs->gpr[0];
	else
		return -1;
}
static inline void syscall_rollback(struct task_struct *task,
				    struct pt_regs *regs)
{
	regs->gpr[3] = regs->orig_gpr3;
}
static inline long syscall_get_error(struct task_struct *task,
				     struct pt_regs *regs)
{
	if (trap_is_scv(regs)) {
		unsigned long error = regs->gpr[3];
		return IS_ERR_VALUE(error) ? error : 0;
	} else {
		return (regs->ccr & 0x10000000UL) ? -regs->gpr[3] : 0;
	}
}
static inline long syscall_get_return_value(struct task_struct *task,
					    struct pt_regs *regs)
{
	return regs->gpr[3];
}
static inline void syscall_set_return_value(struct task_struct *task,
					    struct pt_regs *regs,
					    int error, long val)
{
	if (trap_is_scv(regs)) {
		regs->gpr[3] = (long) error ?: val;
	} else {
		if (error) {
			regs->ccr |= 0x10000000L;
			regs->gpr[3] = error;
		} else {
			regs->ccr &= ~0x10000000L;
			regs->gpr[3] = val;
		}
	}
}
static inline void syscall_get_arguments(struct task_struct *task,
					 struct pt_regs *regs,
					 unsigned long *args)
{
	unsigned long val, mask = -1UL;
	unsigned int n = 6;
	if (is_tsk_32bit_task(task))
		mask = 0xffffffff;
	while (n--) {
		if (n == 0)
			val = regs->orig_gpr3;
		else
			val = regs->gpr[3 + n];
		args[n] = val & mask;
	}
}
static inline int syscall_get_arch(struct task_struct *task)
{
	if (is_tsk_32bit_task(task))
		return AUDIT_ARCH_PPC;
	else if (IS_ENABLED(CONFIG_CPU_LITTLE_ENDIAN))
		return AUDIT_ARCH_PPC64LE;
	else
		return AUDIT_ARCH_PPC64;
}
#endif	 
