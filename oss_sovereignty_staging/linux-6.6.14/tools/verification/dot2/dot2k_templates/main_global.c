
#include <linux/ftrace.h>
#include <linux/tracepoint.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/rv.h>
#include <rv/instrumentation.h>
#include <rv/da_monitor.h>

#define MODULE_NAME "MODEL_NAME"

 
#include <trace/events/rv.h>

 
#include "MODEL_NAME.h"

 
static struct rv_monitor rv_MODEL_NAME;
DECLARE_DA_MON_GLOBAL(MODEL_NAME, MIN_TYPE);

 
TRACEPOINT_HANDLERS_SKEL
static int enable_MODEL_NAME(void)
{
	int retval;

	retval = da_monitor_init_MODEL_NAME();
	if (retval)
		return retval;

TRACEPOINT_ATTACH

	return 0;
}

static void disable_MODEL_NAME(void)
{
	rv_MODEL_NAME.enabled = 0;

TRACEPOINT_DETACH

	da_monitor_destroy_MODEL_NAME();
}

 
static struct rv_monitor rv_MODEL_NAME = {
	.name = "MODEL_NAME",
	.description = "auto-generated MODEL_NAME",
	.enable = enable_MODEL_NAME,
	.disable = disable_MODEL_NAME,
	.reset = da_monitor_reset_all_MODEL_NAME,
	.enabled = 0,
};

static int __init register_MODEL_NAME(void)
{
	rv_register_monitor(&rv_MODEL_NAME);
	return 0;
}

static void __exit unregister_MODEL_NAME(void)
{
	rv_unregister_monitor(&rv_MODEL_NAME);
}

module_init(register_MODEL_NAME);
module_exit(unregister_MODEL_NAME);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dot2k: auto-generated");
MODULE_DESCRIPTION("MODEL_NAME");
