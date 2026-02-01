
 

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/mfd/bcm2835-pm.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/watchdog.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

#define PM_RSTC				0x1c
#define PM_RSTS				0x20
#define PM_WDOG				0x24

#define PM_PASSWORD			0x5a000000

#define PM_WDOG_TIME_SET		0x000fffff
#define PM_RSTC_WRCFG_CLR		0xffffffcf
#define PM_RSTS_HADWRH_SET		0x00000040
#define PM_RSTC_WRCFG_SET		0x00000030
#define PM_RSTC_WRCFG_FULL_RESET	0x00000020
#define PM_RSTC_RESET			0x00000102

 
#define PM_RSTS_RASPBERRYPI_HALT	0x555

#define SECS_TO_WDOG_TICKS(x) ((x) << 16)
#define WDOG_TICKS_TO_SECS(x) ((x) >> 16)
#define WDOG_TICKS_TO_MSECS(x) ((x) * 1000 >> 16)

struct bcm2835_wdt {
	void __iomem		*base;
	spinlock_t		lock;
};

static struct bcm2835_wdt *bcm2835_power_off_wdt;

static unsigned int heartbeat;
static bool nowayout = WATCHDOG_NOWAYOUT;

static bool bcm2835_wdt_is_running(struct bcm2835_wdt *wdt)
{
	uint32_t cur;

	cur = readl(wdt->base + PM_RSTC);

	return !!(cur & PM_RSTC_WRCFG_FULL_RESET);
}

static int bcm2835_wdt_start(struct watchdog_device *wdog)
{
	struct bcm2835_wdt *wdt = watchdog_get_drvdata(wdog);
	uint32_t cur;
	unsigned long flags;

	spin_lock_irqsave(&wdt->lock, flags);

	writel_relaxed(PM_PASSWORD | (SECS_TO_WDOG_TICKS(wdog->timeout) &
				PM_WDOG_TIME_SET), wdt->base + PM_WDOG);
	cur = readl_relaxed(wdt->base + PM_RSTC);
	writel_relaxed(PM_PASSWORD | (cur & PM_RSTC_WRCFG_CLR) |
		  PM_RSTC_WRCFG_FULL_RESET, wdt->base + PM_RSTC);

	spin_unlock_irqrestore(&wdt->lock, flags);

	return 0;
}

static int bcm2835_wdt_stop(struct watchdog_device *wdog)
{
	struct bcm2835_wdt *wdt = watchdog_get_drvdata(wdog);

	writel_relaxed(PM_PASSWORD | PM_RSTC_RESET, wdt->base + PM_RSTC);
	return 0;
}

static unsigned int bcm2835_wdt_get_timeleft(struct watchdog_device *wdog)
{
	struct bcm2835_wdt *wdt = watchdog_get_drvdata(wdog);

	uint32_t ret = readl_relaxed(wdt->base + PM_WDOG);
	return WDOG_TICKS_TO_SECS(ret & PM_WDOG_TIME_SET);
}

static void __bcm2835_restart(struct bcm2835_wdt *wdt)
{
	u32 val;

	 
	writel_relaxed(10 | PM_PASSWORD, wdt->base + PM_WDOG);
	val = readl_relaxed(wdt->base + PM_RSTC);
	val &= PM_RSTC_WRCFG_CLR;
	val |= PM_PASSWORD | PM_RSTC_WRCFG_FULL_RESET;
	writel_relaxed(val, wdt->base + PM_RSTC);

	 
	mdelay(1);
}

static int bcm2835_restart(struct watchdog_device *wdog,
			   unsigned long action, void *data)
{
	struct bcm2835_wdt *wdt = watchdog_get_drvdata(wdog);

	__bcm2835_restart(wdt);

	return 0;
}

static const struct watchdog_ops bcm2835_wdt_ops = {
	.owner =	THIS_MODULE,
	.start =	bcm2835_wdt_start,
	.stop =		bcm2835_wdt_stop,
	.get_timeleft =	bcm2835_wdt_get_timeleft,
	.restart =	bcm2835_restart,
};

static const struct watchdog_info bcm2835_wdt_info = {
	.options =	WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE |
			WDIOF_KEEPALIVEPING,
	.identity =	"Broadcom BCM2835 Watchdog timer",
};

static struct watchdog_device bcm2835_wdt_wdd = {
	.info =		&bcm2835_wdt_info,
	.ops =		&bcm2835_wdt_ops,
	.min_timeout =	1,
	.max_hw_heartbeat_ms =	WDOG_TICKS_TO_MSECS(PM_WDOG_TIME_SET),
	.timeout =	WDOG_TICKS_TO_SECS(PM_WDOG_TIME_SET),
};

 
static void bcm2835_power_off(void)
{
	struct bcm2835_wdt *wdt = bcm2835_power_off_wdt;
	u32 val;

	 
	val = readl_relaxed(wdt->base + PM_RSTS);
	val |= PM_PASSWORD | PM_RSTS_RASPBERRYPI_HALT;
	writel_relaxed(val, wdt->base + PM_RSTS);

	 
	__bcm2835_restart(wdt);
}

static int bcm2835_wdt_probe(struct platform_device *pdev)
{
	struct bcm2835_pm *pm = dev_get_drvdata(pdev->dev.parent);
	struct device *dev = &pdev->dev;
	struct bcm2835_wdt *wdt;
	int err;

	wdt = devm_kzalloc(dev, sizeof(struct bcm2835_wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	spin_lock_init(&wdt->lock);

	wdt->base = pm->base;

	watchdog_set_drvdata(&bcm2835_wdt_wdd, wdt);
	watchdog_init_timeout(&bcm2835_wdt_wdd, heartbeat, dev);
	watchdog_set_nowayout(&bcm2835_wdt_wdd, nowayout);
	bcm2835_wdt_wdd.parent = dev;
	if (bcm2835_wdt_is_running(wdt)) {
		 
		set_bit(WDOG_HW_RUNNING, &bcm2835_wdt_wdd.status);
	}

	watchdog_set_restart_priority(&bcm2835_wdt_wdd, 128);

	watchdog_stop_on_reboot(&bcm2835_wdt_wdd);
	err = devm_watchdog_register_device(dev, &bcm2835_wdt_wdd);
	if (err)
		return err;

	if (of_device_is_system_power_controller(pdev->dev.parent->of_node)) {
		if (!pm_power_off) {
			pm_power_off = bcm2835_power_off;
			bcm2835_power_off_wdt = wdt;
		} else {
			dev_info(dev, "Poweroff handler already present!\n");
		}
	}

	dev_info(dev, "Broadcom BCM2835 watchdog timer");
	return 0;
}

static void bcm2835_wdt_remove(struct platform_device *pdev)
{
	if (pm_power_off == bcm2835_power_off)
		pm_power_off = NULL;
}

static struct platform_driver bcm2835_wdt_driver = {
	.probe		= bcm2835_wdt_probe,
	.remove_new	= bcm2835_wdt_remove,
	.driver = {
		.name =		"bcm2835-wdt",
	},
};
module_platform_driver(bcm2835_wdt_driver);

module_param(heartbeat, uint, 0);
MODULE_PARM_DESC(heartbeat, "Initial watchdog heartbeat in seconds");

module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

MODULE_ALIAS("platform:bcm2835-wdt");
MODULE_AUTHOR("Lubomir Rintel <lkundrak@v3.sk>");
MODULE_DESCRIPTION("Driver for Broadcom BCM2835 watchdog timer");
MODULE_LICENSE("GPL");
