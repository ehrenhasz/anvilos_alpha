#ifndef __ASM_CPU_OPS_SBI_H
#define __ASM_CPU_OPS_SBI_H
#ifndef __ASSEMBLY__
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/threads.h>
extern const struct cpu_operations cpu_ops_sbi;
struct sbi_hart_boot_data {
	void *task_ptr;
	void *stack_ptr;
};
#endif
#endif  
