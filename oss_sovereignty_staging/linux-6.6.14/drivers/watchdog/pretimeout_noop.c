
 

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/watchdog.h>

#include "watchdog_pretimeout.h"

 
static void pretimeout_noop(struct watchdog_device *wdd)
{
	pr_alert("watchdog%d: pretimeout event\n", wdd->id);
}

static struct watchdog_governor watchdog_gov_noop = {
	.name		= "noop",
	.pretimeout	= pretimeout_noop,
};

static int __init watchdog_gov_noop_register(void)
{
	return watchdog_register_governor(&watchdog_gov_noop);
}

static void __exit watchdog_gov_noop_unregister(void)
{
	watchdog_unregister_governor(&watchdog_gov_noop);
}
module_init(watchdog_gov_noop_register);
module_exit(watchdog_gov_noop_unregister);

MODULE_AUTHOR("Vladimir Zapolskiy <vladimir_zapolskiy@mentor.com>");
MODULE_DESCRIPTION("Panic watchdog pretimeout governor");
MODULE_LICENSE("GPL");
