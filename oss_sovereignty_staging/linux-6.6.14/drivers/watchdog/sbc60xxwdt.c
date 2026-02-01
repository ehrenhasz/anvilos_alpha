
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/uaccess.h>


#define OUR_NAME "sbc60xxwdt"
#define PFX OUR_NAME ": "

 

static int wdt_stop = 0x45;
module_param(wdt_stop, int, 0);
MODULE_PARM_DESC(wdt_stop, "SBC60xx WDT 'stop' io port (default 0x45)");

static int wdt_start = 0x443;
module_param(wdt_start, int, 0);
MODULE_PARM_DESC(wdt_start, "SBC60xx WDT 'start' io port (default 0x443)");

 

#define WDT_INTERVAL (HZ/4+1)

 

#define WATCHDOG_TIMEOUT 30		 
static int timeout = WATCHDOG_TIMEOUT;	 
module_param(timeout, int, 0);
MODULE_PARM_DESC(timeout,
	"Watchdog timeout in seconds. (1<=timeout<=3600, default="
				__MODULE_STRING(WATCHDOG_TIMEOUT) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
	"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static void wdt_timer_ping(struct timer_list *);
static DEFINE_TIMER(timer, wdt_timer_ping);
static unsigned long next_heartbeat;
static unsigned long wdt_is_open;
static char wdt_expect_close;

 

static void wdt_timer_ping(struct timer_list *unused)
{
	 
	if (time_before(jiffies, next_heartbeat)) {
		 
		inb_p(wdt_start);
		 
		mod_timer(&timer, jiffies + WDT_INTERVAL);
	} else
		pr_warn("Heartbeat lost! Will not ping the watchdog\n");
}

 

static void wdt_startup(void)
{
	next_heartbeat = jiffies + (timeout * HZ);

	 
	mod_timer(&timer, jiffies + WDT_INTERVAL);
	pr_info("Watchdog timer is now enabled\n");
}

static void wdt_turnoff(void)
{
	 
	del_timer_sync(&timer);
	inb_p(wdt_stop);
	pr_info("Watchdog timer is now disabled...\n");
}

static void wdt_keepalive(void)
{
	 
	next_heartbeat = jiffies + (timeout * HZ);
}

 

static ssize_t fop_write(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos)
{
	 
	if (count) {
		if (!nowayout) {
			size_t ofs;

			 
			wdt_expect_close = 0;

			 
			for (ofs = 0; ofs != count; ofs++) {
				char c;
				if (get_user(c, buf + ofs))
					return -EFAULT;
				if (c == 'V')
					wdt_expect_close = 42;
			}
		}

		 
		wdt_keepalive();
	}
	return count;
}

static int fop_open(struct inode *inode, struct file *file)
{
	 
	if (test_and_set_bit(0, &wdt_is_open))
		return -EBUSY;

	if (nowayout)
		__module_get(THIS_MODULE);

	 
	wdt_startup();
	return stream_open(inode, file);
}

static int fop_close(struct inode *inode, struct file *file)
{
	if (wdt_expect_close == 42)
		wdt_turnoff();
	else {
		del_timer(&timer);
		pr_crit("device file closed unexpectedly. Will not stop the WDT!\n");
	}
	clear_bit(0, &wdt_is_open);
	wdt_expect_close = 0;
	return 0;
}

static long fop_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	static const struct watchdog_info ident = {
		.options = WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT |
							WDIOF_MAGICCLOSE,
		.firmware_version = 1,
		.identity = "SBC60xx",
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user(argp, &ident, sizeof(ident)) ? -EFAULT : 0;
	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		return put_user(0, p);
	case WDIOC_SETOPTIONS:
	{
		int new_options, retval = -EINVAL;
		if (get_user(new_options, p))
			return -EFAULT;
		if (new_options & WDIOS_DISABLECARD) {
			wdt_turnoff();
			retval = 0;
		}
		if (new_options & WDIOS_ENABLECARD) {
			wdt_startup();
			retval = 0;
		}
		return retval;
	}
	case WDIOC_KEEPALIVE:
		wdt_keepalive();
		return 0;
	case WDIOC_SETTIMEOUT:
	{
		int new_timeout;
		if (get_user(new_timeout, p))
			return -EFAULT;
		 
		if (new_timeout < 1 || new_timeout > 3600)
			return -EINVAL;

		timeout = new_timeout;
		wdt_keepalive();
	}
		fallthrough;
	case WDIOC_GETTIMEOUT:
		return put_user(timeout, p);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations wdt_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= fop_write,
	.open		= fop_open,
	.release	= fop_close,
	.unlocked_ioctl	= fop_ioctl,
	.compat_ioctl	= compat_ptr_ioctl,
};

static struct miscdevice wdt_miscdev = {
	.minor = WATCHDOG_MINOR,
	.name = "watchdog",
	.fops = &wdt_fops,
};

 

static int wdt_notify_sys(struct notifier_block *this, unsigned long code,
	void *unused)
{
	if (code == SYS_DOWN || code == SYS_HALT)
		wdt_turnoff();
	return NOTIFY_DONE;
}

 

static struct notifier_block wdt_notifier = {
	.notifier_call = wdt_notify_sys,
};

static void __exit sbc60xxwdt_unload(void)
{
	wdt_turnoff();

	 
	misc_deregister(&wdt_miscdev);

	unregister_reboot_notifier(&wdt_notifier);
	if ((wdt_stop != 0x45) && (wdt_stop != wdt_start))
		release_region(wdt_stop, 1);
	release_region(wdt_start, 1);
}

static int __init sbc60xxwdt_init(void)
{
	int rc = -EBUSY;

	if (timeout < 1 || timeout > 3600) {  
		timeout = WATCHDOG_TIMEOUT;
		pr_info("timeout value must be 1 <= x <= 3600, using %d\n",
			timeout);
	}

	if (!request_region(wdt_start, 1, "SBC 60XX WDT")) {
		pr_err("I/O address 0x%04x already in use\n", wdt_start);
		rc = -EIO;
		goto err_out;
	}

	 
	if (wdt_stop != 0x45 && wdt_stop != wdt_start) {
		if (!request_region(wdt_stop, 1, "SBC 60XX WDT")) {
			pr_err("I/O address 0x%04x already in use\n", wdt_stop);
			rc = -EIO;
			goto err_out_region1;
		}
	}

	rc = register_reboot_notifier(&wdt_notifier);
	if (rc) {
		pr_err("cannot register reboot notifier (err=%d)\n", rc);
		goto err_out_region2;
	}

	rc = misc_register(&wdt_miscdev);
	if (rc) {
		pr_err("cannot register miscdev on minor=%d (err=%d)\n",
		       wdt_miscdev.minor, rc);
		goto err_out_reboot;
	}
	pr_info("WDT driver for 60XX single board computer initialised. timeout=%d sec (nowayout=%d)\n",
		timeout, nowayout);

	return 0;

err_out_reboot:
	unregister_reboot_notifier(&wdt_notifier);
err_out_region2:
	if (wdt_stop != 0x45 && wdt_stop != wdt_start)
		release_region(wdt_stop, 1);
err_out_region1:
	release_region(wdt_start, 1);
err_out:
	return rc;
}

module_init(sbc60xxwdt_init);
module_exit(sbc60xxwdt_unload);

MODULE_AUTHOR("Jakob Oestergaard <jakob@unthought.net>");
MODULE_DESCRIPTION("60xx Single Board Computer Watchdog Timer driver");
MODULE_LICENSE("GPL");
