#ifndef __ASM_XTENSA_HW_BREAKPOINT_H
#define __ASM_XTENSA_HW_BREAKPOINT_H
#ifdef CONFIG_HAVE_HW_BREAKPOINT
#include <linux/kdebug.h>
#include <linux/types.h>
#include <uapi/linux/hw_breakpoint.h>
#define XTENSA_BREAKPOINT_EXECUTE	0
#define XTENSA_BREAKPOINT_LOAD		1
#define XTENSA_BREAKPOINT_STORE		2
struct arch_hw_breakpoint {
	unsigned long address;
	u16 len;
	u16 type;
};
struct perf_event_attr;
struct perf_event;
struct pt_regs;
struct task_struct;
int hw_breakpoint_slots(int type);
int arch_check_bp_in_kernelspace(struct arch_hw_breakpoint *hw);
int hw_breakpoint_arch_parse(struct perf_event *bp,
			     const struct perf_event_attr *attr,
			     struct arch_hw_breakpoint *hw);
int hw_breakpoint_exceptions_notify(struct notifier_block *unused,
				    unsigned long val, void *data);
int arch_install_hw_breakpoint(struct perf_event *bp);
void arch_uninstall_hw_breakpoint(struct perf_event *bp);
void hw_breakpoint_pmu_read(struct perf_event *bp);
int check_hw_breakpoint(struct pt_regs *regs);
void clear_ptrace_hw_breakpoint(struct task_struct *tsk);
void restore_dbreak(void);
#else
struct task_struct;
static inline void clear_ptrace_hw_breakpoint(struct task_struct *tsk)
{
}
#endif  
#endif  
