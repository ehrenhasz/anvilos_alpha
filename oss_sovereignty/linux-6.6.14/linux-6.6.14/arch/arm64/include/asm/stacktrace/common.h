#ifndef __ASM_STACKTRACE_COMMON_H
#define __ASM_STACKTRACE_COMMON_H
#include <linux/kprobes.h>
#include <linux/types.h>
struct stack_info {
	unsigned long low;
	unsigned long high;
};
struct unwind_state {
	unsigned long fp;
	unsigned long pc;
#ifdef CONFIG_KRETPROBES
	struct llist_node *kr_cur;
#endif
	struct task_struct *task;
	struct stack_info stack;
	struct stack_info *stacks;
	int nr_stacks;
};
static inline struct stack_info stackinfo_get_unknown(void)
{
	return (struct stack_info) {
		.low = 0,
		.high = 0,
	};
}
static inline bool stackinfo_on_stack(const struct stack_info *info,
				      unsigned long sp, unsigned long size)
{
	if (!info->low)
		return false;
	if (sp < info->low || sp + size < sp || sp + size > info->high)
		return false;
	return true;
}
static inline void unwind_init_common(struct unwind_state *state,
				      struct task_struct *task)
{
	state->task = task;
#ifdef CONFIG_KRETPROBES
	state->kr_cur = NULL;
#endif
	state->stack = stackinfo_get_unknown();
}
static struct stack_info *unwind_find_next_stack(const struct unwind_state *state,
						 unsigned long sp,
						 unsigned long size)
{
	for (int i = 0; i < state->nr_stacks; i++) {
		struct stack_info *info = &state->stacks[i];
		if (stackinfo_on_stack(info, sp, size))
			return info;
	}
	return NULL;
}
static inline int unwind_consume_stack(struct unwind_state *state,
				       unsigned long sp,
				       unsigned long size)
{
	struct stack_info *next;
	if (stackinfo_on_stack(&state->stack, sp, size))
		goto found;
	next = unwind_find_next_stack(state, sp, size);
	if (!next)
		return -EINVAL;
	state->stack = *next;
	*next = stackinfo_get_unknown();
found:
	state->stack.low = sp + size;
	return 0;
}
static inline int
unwind_next_frame_record(struct unwind_state *state)
{
	unsigned long fp = state->fp;
	int err;
	if (fp & 0x7)
		return -EINVAL;
	err = unwind_consume_stack(state, fp, 16);
	if (err)
		return err;
	state->fp = READ_ONCE(*(unsigned long *)(fp));
	state->pc = READ_ONCE(*(unsigned long *)(fp + 8));
	return 0;
}
#endif	 
