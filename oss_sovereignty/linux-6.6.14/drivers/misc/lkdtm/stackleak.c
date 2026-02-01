
 

#include "lkdtm.h"
#include <linux/stackleak.h>

#if defined(CONFIG_GCC_PLUGIN_STACKLEAK)
 
static void noinstr check_stackleak_irqoff(void)
{
	const unsigned long task_stack_base = (unsigned long)task_stack_page(current);
	const unsigned long task_stack_low = stackleak_task_low_bound(current);
	const unsigned long task_stack_high = stackleak_task_high_bound(current);
	const unsigned long current_sp = current_stack_pointer;
	const unsigned long lowest_sp = current->lowest_stack;
	unsigned long untracked_high;
	unsigned long poison_high, poison_low;
	bool test_failed = false;

	 
	if (current_sp < task_stack_low || current_sp >= task_stack_high) {
		instrumentation_begin();
		pr_err("FAIL: current_stack_pointer (0x%lx) outside of task stack bounds [0x%lx..0x%lx]\n",
		       current_sp, task_stack_low, task_stack_high - 1);
		test_failed = true;
		goto out;
	}
	if (lowest_sp < task_stack_low || lowest_sp >= task_stack_high) {
		instrumentation_begin();
		pr_err("FAIL: current->lowest_stack (0x%lx) outside of task stack bounds [0x%lx..0x%lx]\n",
		       lowest_sp, task_stack_low, task_stack_high - 1);
		test_failed = true;
		goto out;
	}

	 
	untracked_high = min(current_sp, lowest_sp);
	untracked_high = ALIGN_DOWN(untracked_high, sizeof(unsigned long));

	 
	poison_high = stackleak_find_top_of_poison(task_stack_low, untracked_high);

	 
	poison_low = poison_high;
	while (poison_low > task_stack_low) {
		poison_low -= sizeof(unsigned long);

		if (*(unsigned long *)poison_low == STACKLEAK_POISON)
			continue;

		instrumentation_begin();
		pr_err("FAIL: non-poison value %lu bytes below poison boundary: 0x%lx\n",
		       poison_high - poison_low, *(unsigned long *)poison_low);
		test_failed = true;
		goto out;
	}

	instrumentation_begin();
	pr_info("stackleak stack usage:\n"
		"  high offset: %lu bytes\n"
		"  current:     %lu bytes\n"
		"  lowest:      %lu bytes\n"
		"  tracked:     %lu bytes\n"
		"  untracked:   %lu bytes\n"
		"  poisoned:    %lu bytes\n"
		"  low offset:  %lu bytes\n",
		task_stack_base + THREAD_SIZE - task_stack_high,
		task_stack_high - current_sp,
		task_stack_high - lowest_sp,
		task_stack_high - untracked_high,
		untracked_high - poison_high,
		poison_high - task_stack_low,
		task_stack_low - task_stack_base);

out:
	if (test_failed) {
		pr_err("FAIL: the thread stack is NOT properly erased!\n");
	} else {
		pr_info("OK: the rest of the thread stack is properly erased\n");
	}
	instrumentation_end();
}

static void lkdtm_STACKLEAK_ERASING(void)
{
	unsigned long flags;

	local_irq_save(flags);
	check_stackleak_irqoff();
	local_irq_restore(flags);
}
#else  
static void lkdtm_STACKLEAK_ERASING(void)
{
	if (IS_ENABLED(CONFIG_HAVE_ARCH_STACKLEAK)) {
		pr_err("XFAIL: stackleak is not enabled (CONFIG_GCC_PLUGIN_STACKLEAK=n)\n");
	} else {
		pr_err("XFAIL: stackleak is not supported on this arch (HAVE_ARCH_STACKLEAK=n)\n");
	}
}
#endif  

static struct crashtype crashtypes[] = {
	CRASHTYPE(STACKLEAK_ERASING),
};

struct crashtype_category stackleak_crashtypes = {
	.crashtypes = crashtypes,
	.len	    = ARRAY_SIZE(crashtypes),
};
