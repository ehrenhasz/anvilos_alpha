
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/uaccess.h>


 

#define WDT_INTERVAL (HZ/4+1)

 

#define WATCHDOG_TIMEOUT 30		 
 
static int timeout = WATCHDOG_TIMEOUT;
module_param(timeout, int, 0);
MODULE_PARM_DESC(timeout,
	"Watchdog timeout in seconds. (1 <= timeout <= 3600, default="
				__MODULE_STRING(WATCHDOG_TIMEOUT) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
		"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

 
#define MMCR_BASE	0xfffef000	 
#define OFFS_WDTMRCTL	0xCB0	 

 
#define WDT_EXP_SEL_01	0x0001	 
#define WDT_EXP_SEL_02	0x0002	 
#define WDT_EXP_SEL_03	0x0004	 
#define WDT_EXP_SEL_04	0x0008	 
#define WDT_EXP_SEL_05	0x0010	 
#define WDT_EXP_SEL_06	0x0020	 
#define WDT_EXP_SEL_07	0x0040	 
#define WDT_EXP_SEL_08	0x0080	 
#define WDT_IRQ_FLG	0x1000	 
#define WDT_WRST_ENB	0x4000	 
#define WDT_ENB		0x8000	 

static __u16 __iomem *wdtmrctl;

static void wdt_timer_ping(struct timer_list *);
static DEFINE_TIMER(timer, wdt_timer_ping);
static unsigned long next_heartbeat;
static unsigned long wdt_is_open;
static char wdt_expect_close;
static DEFINE_SPINLOCK(wdt_spinlock);

 

static void wdt_timer_ping(struct timer_list *unused)
{
	 
	if (time_before(jiffies, next_heartbeat)) {
		 
		spin_lock(&wdt_spinlock);
		writew(0xAAAA, wdtmrctl);
		writew(0x5555, wdtmrctl);
		spin_unlock(&wdt_spinlock);

		 
		mod_timer(&timer, jiffies + WDT_INTERVAL);
	} else
		pr_warn("Heartbeat lost! Will not ping the watchdog\n");
}

 

static void wdt_config(int writeval)
{
	unsigned long flags;

	 
	spin_lock_irqsave(&wdt_spinlock, flags);
	readw(wdtmrctl);	 
	writew(0xAAAA, wdtmrctl);
	writew(0x5555, wdtmrctl);
	 
	writew(0x3333, wdtmrctl);
	writew(0xCCCC, wdtmrctl);
	 
	writew(writeval, wdtmrctl);
	spin_unlock_irqrestore(&wdt_spinlock, flags);
}

static int wdt_startup(void)
{
	next_heartbeat = jiffies + (timeout * HZ);

	 
	mod_timer(&timer, jiffies + WDT_INTERVAL);

	 
	wdt_config(WDT_ENB | WDT_WRST_ENB | WDT_EXP_SEL_04);

	pr_info("Watchdog timer is now enabled\n");
	return 0;
}

static int wdt_turnoff(void)
{
	 
	del_timer_sync(&timer);

	 
	wdt_config(0);

	pr_info("Watchdog timer is now disabled...\n");
	return 0;
}

static int wdt_keepalive(void)
{
	 
	next_heartbeat = jiffies + (timeout * HZ);
	return 0;
}

static int wdt_set_heartbeat(int t)
{
	if ((t < 1) || (t > 3600))	 
		return -EINVAL;

	timeout = t;
	return 0;
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
		pr_crit("Unexpected close, not stopping watchdog!\n");
		wdt_keepalive();
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
		.options = WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT
							| WDIOF_MAGICCLOSE,
		.firmware_version = 1,
		.identity = "SC520",
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

		if (wdt_set_heartbeat(new_timeout))
			return -EINVAL;

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
	.minor	= WATCHDOG_MINOR,
	.name	= "watchdog",
	.fops	= &wdt_fops,
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

static void __exit sc520_wdt_unload(void)
{
	if (!nowayout)
		wdt_turnoff();

	 
	misc_deregister(&wdt_miscdev);
	unregister_reboot_notifier(&wdt_notifier);
	iounmap(wdtmrctl);
}

static int __init sc520_wdt_init(void)
{
	int rc = -EBUSY;

	 
	if (wdt_set_heartbeat(timeout)) {
		wdt_set_heartbeat(WATCHDOG_TIMEOUT);
		pr_info("timeout value must be 1 <= timeout <= 3600, using %d\n",
			WATCHDOG_TIMEOUT);
	}

	wdtmrctl = ioremap(MMCR_BASE + OFFS_WDTMRCTL, 2);
	if (!wdtmrctl) {
		pr_err("Unable to remap memory\n");
		rc = -ENOMEM;
		goto err_out_region2;
	}

	rc = register_reboot_notifier(&wdt_notifier);
	if (rc) {
		pr_err("cannot register reboot notifier (err=%d)\n", rc);
		goto err_out_ioremap;
	}

	rc = misc_register(&wdt_miscdev);
	if (rc) {
		pr_err("cannot register miscdev on minor=%d (err=%d)\n",
		       WATCHDOG_MINOR, rc);
		goto err_out_notifier;
	}

	pr_info("WDT driver for SC520 initialised. timeout=%d sec (nowayout=%d)\n",
		timeout, nowayout);

	return 0;

err_out_notifier:
	unregister_reboot_notifier(&wdt_notifier);
err_out_ioremap:
	iounmap(wdtmrctl);
err_out_region2:
	return rc;
}

module_init(sc520_wdt_init);
module_exit(sc520_wdt_unload);

MODULE_AUTHOR("Scott and Bill Jennings");
MODULE_DESCRIPTION(
	"Driver for watchdog timer in AMD \"Elan\" SC520 uProcessor");
MODULE_LICENSE("GPL");
