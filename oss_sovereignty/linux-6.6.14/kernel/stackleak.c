
 

#include <linux/stackleak.h>
#include <linux/kprobes.h>

#ifdef CONFIG_STACKLEAK_RUNTIME_DISABLE
#include <linux/jump_label.h>
#include <linux/sysctl.h>
#include <linux/init.h>

static DEFINE_STATIC_KEY_FALSE(stack_erasing_bypass);

#ifdef CONFIG_SYSCTL
static int stack_erasing_sysctl(struct ctl_table *table, int write,
			void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret = 0;
	int state = !static_branch_unlikely(&stack_erasing_bypass);
	int prev_state = state;

	table->data = &state;
	table->maxlen = sizeof(int);
	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	state = !!state;
	if (ret || !write || state == prev_state)
		return ret;

	if (state)
		static_branch_disable(&stack_erasing_bypass);
	else
		static_branch_enable(&stack_erasing_bypass);

	pr_warn("stackleak: kernel stack erasing is %s\n",
					state ? "enabled" : "disabled");
	return ret;
}
static struct ctl_table stackleak_sysctls[] = {
	{
		.procname	= "stack_erasing",
		.data		= NULL,
		.maxlen		= sizeof(int),
		.mode		= 0600,
		.proc_handler	= stack_erasing_sysctl,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{}
};

static int __init stackleak_sysctls_init(void)
{
	register_sysctl_init("kernel", stackleak_sysctls);
	return 0;
}
late_initcall(stackleak_sysctls_init);
#endif  

#define skip_erasing()	static_branch_unlikely(&stack_erasing_bypass)
#else
#define skip_erasing()	false
#endif  

#ifndef __stackleak_poison
static __always_inline void __stackleak_poison(unsigned long erase_low,
					       unsigned long erase_high,
					       unsigned long poison)
{
	while (erase_low < erase_high) {
		*(unsigned long *)erase_low = poison;
		erase_low += sizeof(unsigned long);
	}
}
#endif

static __always_inline void __stackleak_erase(bool on_task_stack)
{
	const unsigned long task_stack_low = stackleak_task_low_bound(current);
	const unsigned long task_stack_high = stackleak_task_high_bound(current);
	unsigned long erase_low, erase_high;

	erase_low = stackleak_find_top_of_poison(task_stack_low,
						 current->lowest_stack);

#ifdef CONFIG_STACKLEAK_METRICS
	current->prev_lowest_stack = erase_low;
#endif

	 
	if (on_task_stack)
		erase_high = current_stack_pointer;
	else
		erase_high = task_stack_high;

	__stackleak_poison(erase_low, erase_high, STACKLEAK_POISON);

	 
	current->lowest_stack = task_stack_high;
}

 
asmlinkage void noinstr stackleak_erase(void)
{
	if (skip_erasing())
		return;

	__stackleak_erase(on_thread_stack());
}

 
asmlinkage void noinstr stackleak_erase_on_task_stack(void)
{
	if (skip_erasing())
		return;

	__stackleak_erase(true);
}

 
asmlinkage void noinstr stackleak_erase_off_task_stack(void)
{
	if (skip_erasing())
		return;

	__stackleak_erase(false);
}

void __used __no_caller_saved_registers noinstr stackleak_track_stack(void)
{
	unsigned long sp = current_stack_pointer;

	 
	BUILD_BUG_ON(CONFIG_STACKLEAK_TRACK_MIN_SIZE > STACKLEAK_SEARCH_DEPTH);

	 
	sp = ALIGN(sp, sizeof(unsigned long));
	if (sp < current->lowest_stack &&
	    sp >= stackleak_task_low_bound(current)) {
		current->lowest_stack = sp;
	}
}
EXPORT_SYMBOL(stackleak_track_stack);
