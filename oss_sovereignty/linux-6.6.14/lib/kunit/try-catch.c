
 

#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/kthread.h>

#include "try-catch-impl.h"

void __noreturn kunit_try_catch_throw(struct kunit_try_catch *try_catch)
{
	try_catch->try_result = -EFAULT;
	kthread_complete_and_exit(try_catch->try_completion, -EFAULT);
}
EXPORT_SYMBOL_GPL(kunit_try_catch_throw);

static int kunit_generic_run_threadfn_adapter(void *data)
{
	struct kunit_try_catch *try_catch = data;

	try_catch->try(try_catch->context);

	kthread_complete_and_exit(try_catch->try_completion, 0);
}

static unsigned long kunit_test_timeout(void)
{
	 
	return 300 * msecs_to_jiffies(MSEC_PER_SEC);  
}

void kunit_try_catch_run(struct kunit_try_catch *try_catch, void *context)
{
	DECLARE_COMPLETION_ONSTACK(try_completion);
	struct kunit *test = try_catch->test;
	struct task_struct *task_struct;
	int exit_code, time_remaining;

	try_catch->context = context;
	try_catch->try_completion = &try_completion;
	try_catch->try_result = 0;
	task_struct = kthread_run(kunit_generic_run_threadfn_adapter,
				  try_catch,
				  "kunit_try_catch_thread");
	if (IS_ERR(task_struct)) {
		try_catch->catch(try_catch->context);
		return;
	}

	time_remaining = wait_for_completion_timeout(&try_completion,
						     kunit_test_timeout());
	if (time_remaining == 0) {
		kunit_err(test, "try timed out\n");
		try_catch->try_result = -ETIMEDOUT;
		kthread_stop(task_struct);
	}

	exit_code = try_catch->try_result;

	if (!exit_code)
		return;

	if (exit_code == -EFAULT)
		try_catch->try_result = 0;
	else if (exit_code == -EINTR)
		kunit_err(test, "wake_up_process() was never called\n");
	else if (exit_code)
		kunit_err(test, "Unknown error: %d\n", exit_code);

	try_catch->catch(try_catch->context);
}
EXPORT_SYMBOL_GPL(kunit_try_catch_run);
