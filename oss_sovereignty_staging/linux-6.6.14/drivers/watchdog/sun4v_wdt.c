
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/watchdog.h>
#include <asm/hypervisor.h>
#include <asm/mdesc.h>

#define WDT_TIMEOUT			60
#define WDT_MAX_TIMEOUT			31536000
#define WDT_MIN_TIMEOUT			1
#define WDT_DEFAULT_RESOLUTION_MS	1000	 

static unsigned int timeout;
module_param(timeout, uint, 0);
MODULE_PARM_DESC(timeout, "Watchdog timeout in seconds (default="
	__MODULE_STRING(WDT_TIMEOUT) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, S_IRUGO);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
	__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static int sun4v_wdt_stop(struct watchdog_device *wdd)
{
	sun4v_mach_set_watchdog(0, NULL);

	return 0;
}

static int sun4v_wdt_ping(struct watchdog_device *wdd)
{
	int hverr;

	 
	hverr = sun4v_mach_set_watchdog(wdd->timeout * 1000, NULL);
	if (hverr == HV_EINVAL)
		return -EINVAL;

	return 0;
}

static int sun4v_wdt_set_timeout(struct watchdog_device *wdd,
				 unsigned int timeout)
{
	wdd->timeout = timeout;

	return 0;
}

static const struct watchdog_info sun4v_wdt_ident = {
	.options =	WDIOF_SETTIMEOUT |
			WDIOF_MAGICCLOSE |
			WDIOF_KEEPALIVEPING,
	.identity =	"sun4v hypervisor watchdog",
	.firmware_version = 0,
};

static const struct watchdog_ops sun4v_wdt_ops = {
	.owner =	THIS_MODULE,
	.start =	sun4v_wdt_ping,
	.stop =		sun4v_wdt_stop,
	.ping =		sun4v_wdt_ping,
	.set_timeout =	sun4v_wdt_set_timeout,
};

static struct watchdog_device wdd = {
	.info = &sun4v_wdt_ident,
	.ops = &sun4v_wdt_ops,
	.min_timeout = WDT_MIN_TIMEOUT,
	.max_timeout = WDT_MAX_TIMEOUT,
	.timeout = WDT_TIMEOUT,
};

static int __init sun4v_wdt_init(void)
{
	struct mdesc_handle *handle;
	u64 node;
	const u64 *value;
	int err = 0;
	unsigned long major = 1, minor = 1;

	 

	handle = mdesc_grab();
	if (!handle)
		return -ENODEV;

	node = mdesc_node_by_name(handle, MDESC_NODE_NULL, "platform");
	err = -ENODEV;
	if (node == MDESC_NODE_NULL)
		goto out_release;

	 
	if (sun4v_hvapi_register(HV_GRP_CORE, major, &minor))
		goto out_hv_unreg;

	 
	value = mdesc_get_property(handle, node, "watchdog-resolution", NULL);
	err = -EINVAL;
	if (value) {
		if (*value == 0 ||
		    *value > WDT_DEFAULT_RESOLUTION_MS)
			goto out_hv_unreg;
	}

	value = mdesc_get_property(handle, node, "watchdog-max-timeout", NULL);
	if (value) {
		 
		if (*value < wdd.min_timeout * 1000)
			goto out_hv_unreg;

		 
		if (*value < wdd.max_timeout * 1000)
			wdd.max_timeout = *value  / 1000;
	}

	watchdog_init_timeout(&wdd, timeout, NULL);

	watchdog_set_nowayout(&wdd, nowayout);

	err = watchdog_register_device(&wdd);
	if (err)
		goto out_hv_unreg;

	pr_info("initialized (timeout=%ds, nowayout=%d)\n",
		 wdd.timeout, nowayout);

	mdesc_release(handle);

	return 0;

out_hv_unreg:
	sun4v_hvapi_unregister(HV_GRP_CORE);

out_release:
	mdesc_release(handle);
	return err;
}

static void __exit sun4v_wdt_exit(void)
{
	sun4v_hvapi_unregister(HV_GRP_CORE);
	watchdog_unregister_device(&wdd);
}

module_init(sun4v_wdt_init);
module_exit(sun4v_wdt_exit);

MODULE_AUTHOR("Wim Coekaerts <wim.coekaerts@oracle.com>");
MODULE_DESCRIPTION("sun4v watchdog driver");
MODULE_LICENSE("GPL");
