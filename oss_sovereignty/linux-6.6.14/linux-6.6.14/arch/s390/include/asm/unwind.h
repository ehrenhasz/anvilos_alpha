#ifndef _ASM_S390_UNWIND_H
#define _ASM_S390_UNWIND_H
#include <linux/sched.h>
#include <linux/ftrace.h>
#include <linux/rethook.h>
#include <linux/llist.h>
#include <asm/ptrace.h>
#include <asm/stacktrace.h>
struct unwind_state {
	struct stack_info stack_info;
	unsigned long stack_mask;
	struct task_struct *task;
	struct pt_regs *regs;
	unsigned long sp, ip;
	int graph_idx;
	struct llist_node *kr_cur;
	bool reliable;
	bool error;
};
static inline unsigned long unwind_recover_ret_addr(struct unwind_state *state,
						    unsigned long ip)
{
	ip = ftrace_graph_ret_addr(state->task, &state->graph_idx, ip, (void *)state->sp);
#ifdef CONFIG_RETHOOK
	if (is_rethook_trampoline(ip))
		ip = rethook_find_ret_addr(state->task, state->sp, &state->kr_cur);
#endif
	return ip;
}
void __unwind_start(struct unwind_state *state, struct task_struct *task,
		    struct pt_regs *regs, unsigned long first_frame);
bool unwind_next_frame(struct unwind_state *state);
unsigned long unwind_get_return_address(struct unwind_state *state);
static inline bool unwind_done(struct unwind_state *state)
{
	return state->stack_info.type == STACK_TYPE_UNKNOWN;
}
static inline bool unwind_error(struct unwind_state *state)
{
	return state->error;
}
static __always_inline void unwind_start(struct unwind_state *state,
					 struct task_struct *task,
					 struct pt_regs *regs,
					 unsigned long first_frame)
{
	task = task ?: current;
	first_frame = first_frame ?: get_stack_pointer(task, regs);
	__unwind_start(state, task, regs, first_frame);
}
static inline struct pt_regs *unwind_get_entry_regs(struct unwind_state *state)
{
	return unwind_done(state) ? NULL : state->regs;
}
#define unwind_for_each_frame(state, task, regs, first_frame)	\
	for (unwind_start(state, task, regs, first_frame);	\
	     !unwind_done(state);				\
	     unwind_next_frame(state))
static inline void unwind_init(void) {}
static inline void unwind_module_init(struct module *mod, void *orc_ip,
				      size_t orc_ip_size, void *orc,
				      size_t orc_size) {}
#endif  
