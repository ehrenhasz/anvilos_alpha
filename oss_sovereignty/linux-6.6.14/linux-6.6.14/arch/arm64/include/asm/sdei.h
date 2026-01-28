#ifndef __ASM_SDEI_H
#define __ASM_SDEI_H
#define SDEI_EXIT_HVC  0
#define SDEI_EXIT_SMC  1
#define SDEI_STACK_SIZE		IRQ_STACK_SIZE
#ifndef __ASSEMBLY__
#include <linux/linkage.h>
#include <linux/preempt.h>
#include <linux/types.h>
#include <asm/virt.h>
DECLARE_PER_CPU(struct sdei_registered_event *, sdei_active_normal_event);
DECLARE_PER_CPU(struct sdei_registered_event *, sdei_active_critical_event);
extern unsigned long sdei_exit_mode;
asmlinkage void __sdei_asm_handler(unsigned long event_num, unsigned long arg,
				   unsigned long pc, unsigned long pstate);
asmlinkage void __sdei_asm_entry_trampoline(unsigned long event_num,
						   unsigned long arg,
						   unsigned long pc,
						   unsigned long pstate);
void __sdei_handler_abort(void);
struct sdei_registered_event;
asmlinkage unsigned long __sdei_handler(struct pt_regs *regs,
					struct sdei_registered_event *arg);
unsigned long do_sdei_event(struct pt_regs *regs,
			    struct sdei_registered_event *arg);
unsigned long sdei_arch_get_entry_point(int conduit);
#define sdei_arch_get_entry_point(x)	sdei_arch_get_entry_point(x)
#endif  
#endif	 
