#ifndef __IA64_CPUTIME_H
#define __IA64_CPUTIME_H
#ifdef CONFIG_VIRT_CPU_ACCOUNTING_NATIVE
extern void arch_vtime_task_switch(struct task_struct *tsk);
#endif  
#endif  
