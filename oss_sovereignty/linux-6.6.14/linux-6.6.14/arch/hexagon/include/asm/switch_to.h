#ifndef _ASM_SWITCH_TO_H
#define _ASM_SWITCH_TO_H
struct thread_struct;
extern struct task_struct *__switch_to(struct task_struct *,
	struct task_struct *,
	struct task_struct *);
#define switch_to(p, n, r) do {\
	r = __switch_to((p), (n), (r));\
} while (0)
#endif  
