#ifndef _ASM_RISCV_SUSPEND_H
#define _ASM_RISCV_SUSPEND_H
#include <asm/ptrace.h>
struct suspend_context {
	struct pt_regs regs;
	unsigned long scratch;
	unsigned long tvec;
	unsigned long ie;
#ifdef CONFIG_MMU
	unsigned long satp;
#endif
};
extern int in_suspend;
int __cpu_suspend_enter(struct suspend_context *context);
int cpu_suspend(unsigned long arg,
		int (*finish)(unsigned long arg,
			      unsigned long entry,
			      unsigned long context));
int __cpu_resume_enter(unsigned long hartid, unsigned long context);
void suspend_save_csrs(struct suspend_context *context);
void suspend_restore_csrs(struct suspend_context *context);
int swsusp_arch_suspend(void);
int swsusp_arch_resume(void);
int arch_hibernation_header_save(void *addr, unsigned int max_size);
int arch_hibernation_header_restore(void *addr);
int __hibernate_cpu_resume(void);
int hibernate_resume_nonboot_cpu_disable(void);
asmlinkage void hibernate_restore_image(unsigned long resume_satp, unsigned long satp_temp,
					unsigned long cpu_resume);
asmlinkage int hibernate_core_restore_code(void);
#endif
