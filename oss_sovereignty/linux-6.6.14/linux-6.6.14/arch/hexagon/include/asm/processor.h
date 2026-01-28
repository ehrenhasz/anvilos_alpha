#ifndef _ASM_PROCESSOR_H
#define _ASM_PROCESSOR_H
#ifndef __ASSEMBLY__
#include <asm/mem-layout.h>
#include <asm/registers.h>
#include <asm/hexagon_vm.h>
struct task_struct;
extern void start_thread(struct pt_regs *, unsigned long, unsigned long);
struct thread_struct {
	void *switch_sp;
};
#define INIT_THREAD { \
}
#define cpu_relax() __vmyield()
#define TASK_UNMAPPED_BASE	(PAGE_ALIGN(TASK_SIZE/3))
#define task_pt_regs(task) \
	((struct pt_regs *)(task_stack_page(task) + THREAD_SIZE) - 1)
#define KSTK_EIP(tsk) (pt_elr(task_pt_regs(tsk)))
#define KSTK_ESP(tsk) (pt_psp(task_pt_regs(tsk)))
extern unsigned long __get_wchan(struct task_struct *p);
struct hexagon_switch_stack {
	union {
		struct {
			unsigned long r16;
			unsigned long r17;
		};
		unsigned long long	r1716;
	};
	union {
		struct {
			unsigned long r18;
			unsigned long r19;
		};
		unsigned long long	r1918;
	};
	union {
		struct {
			unsigned long r20;
			unsigned long r21;
		};
		unsigned long long	r2120;
	};
	union {
		struct {
			unsigned long r22;
			unsigned long r23;
		};
		unsigned long long	r2322;
	};
	union {
		struct {
			unsigned long r24;
			unsigned long r25;
		};
		unsigned long long	r2524;
	};
	union {
		struct {
			unsigned long r26;
			unsigned long r27;
		};
		unsigned long long	r2726;
	};
	unsigned long		fp;
	unsigned long		lr;
};
#endif  
#endif
