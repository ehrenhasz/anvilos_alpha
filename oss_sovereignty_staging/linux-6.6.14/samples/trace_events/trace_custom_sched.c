
 

#define pr_fmt(fmt) fmt

#include <linux/trace_events.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/sched.h>

 
#include <trace/events/sched.h>

 
#define CREATE_CUSTOM_TRACE_EVENTS

#include "trace_custom_sched.h"

 
static void fct(struct tracepoint *tp, void *priv)
{
	trace_custom_event_sched_switch_update(tp);
	trace_custom_event_sched_waking_update(tp);
}

static int __init trace_sched_init(void)
{
	for_each_kernel_tracepoint(fct, NULL);
	return 0;
}

static void __exit trace_sched_exit(void)
{
}

module_init(trace_sched_init);
module_exit(trace_sched_exit);

MODULE_AUTHOR("Steven Rostedt");
MODULE_DESCRIPTION("Custom scheduling events");
MODULE_LICENSE("GPL");
