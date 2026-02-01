
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/trace.h>
#include <linux/trace_events.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>

 
#define CREATE_TRACE_POINTS
#include "sample-trace-array.h"

struct trace_array *tr;
static void mytimer_handler(struct timer_list *unused);
static struct task_struct *simple_tsk;

static void trace_work_fn(struct work_struct *work)
{
	 
	trace_array_set_clr_event(tr, "sample-subsystem", "sample_event",
			false);
}
static DECLARE_WORK(trace_work, trace_work_fn);

 
static DEFINE_TIMER(mytimer, mytimer_handler);

static void mytimer_handler(struct timer_list *unused)
{
	schedule_work(&trace_work);
}

static void simple_thread_func(int count)
{
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(HZ);

	 
	trace_array_printk(tr, _THIS_IP_, "trace_array_printk: count=%d\n",
			count);
	 
	trace_sample_event(count, jiffies);
}

static int simple_thread(void *arg)
{
	int count = 0;
	unsigned long delay = msecs_to_jiffies(5000);

	 
	trace_array_set_clr_event(tr, "sample-subsystem", "sample_event", true);

	 
	add_timer(&mytimer);
	mod_timer(&mytimer, jiffies+delay);

	while (!kthread_should_stop())
		simple_thread_func(count++);

	del_timer(&mytimer);
	cancel_work_sync(&trace_work);

	 
	trace_array_put(tr);

	return 0;
}

static int __init sample_trace_array_init(void)
{
	 
	tr = trace_array_get_by_name("sample-instance");

	if (!tr)
		return -1;
	 
	trace_printk_init_buffers();

	simple_tsk = kthread_run(simple_thread, NULL, "sample-instance");
	if (IS_ERR(simple_tsk)) {
		trace_array_put(tr);
		trace_array_destroy(tr);
		return -1;
	}

	return 0;
}

static void __exit sample_trace_array_exit(void)
{
	kthread_stop(simple_tsk);

	 
	trace_array_destroy(tr);
}

module_init(sample_trace_array_init);
module_exit(sample_trace_array_exit);

MODULE_AUTHOR("Divya Indi");
MODULE_DESCRIPTION("Sample module for kernel access to Ftrace instances");
MODULE_LICENSE("GPL");
