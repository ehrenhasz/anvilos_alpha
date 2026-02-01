
 

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mod_devicetable.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/watchdog.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/err.h>

#define MODULE_NAME "DAVINCI-WDT: "

#define DEFAULT_HEARTBEAT 60
#define MAX_HEARTBEAT     600	 

 
#define PID12	(0x0)
#define EMUMGT	(0x4)
#define TIM12	(0x10)
#define TIM34	(0x14)
#define PRD12	(0x18)
#define PRD34	(0x1C)
#define TCR	(0x20)
#define TGCR	(0x24)
#define WDTCR	(0x28)

 
#define ENAMODE12_DISABLED	(0 << 6)
#define ENAMODE12_ONESHOT	(1 << 6)
#define ENAMODE12_PERIODIC	(2 << 6)

 
#define TIM12RS_UNRESET		(1 << 0)
#define TIM34RS_UNRESET		(1 << 1)
#define TIMMODE_64BIT_WDOG      (2 << 2)

 
#define WDEN			(1 << 14)
#define WDFLAG			(1 << 15)
#define WDKEY_SEQ0		(0xa5c6 << 16)
#define WDKEY_SEQ1		(0xda7e << 16)

static int heartbeat;

 
struct davinci_wdt_device {
	void __iomem		*base;
	struct clk		*clk;
	struct watchdog_device	wdd;
};

static int davinci_wdt_start(struct watchdog_device *wdd)
{
	u32 tgcr;
	u32 timer_margin;
	unsigned long wdt_freq;
	struct davinci_wdt_device *davinci_wdt = watchdog_get_drvdata(wdd);

	wdt_freq = clk_get_rate(davinci_wdt->clk);

	 
	iowrite32(0, davinci_wdt->base + TCR);
	 
	iowrite32(0, davinci_wdt->base + TGCR);
	tgcr = TIMMODE_64BIT_WDOG | TIM12RS_UNRESET | TIM34RS_UNRESET;
	iowrite32(tgcr, davinci_wdt->base + TGCR);
	 
	iowrite32(0, davinci_wdt->base + TIM12);
	iowrite32(0, davinci_wdt->base + TIM34);
	 
	timer_margin = (((u64)wdd->timeout * wdt_freq) & 0xffffffff);
	iowrite32(timer_margin, davinci_wdt->base + PRD12);
	timer_margin = (((u64)wdd->timeout * wdt_freq) >> 32);
	iowrite32(timer_margin, davinci_wdt->base + PRD34);
	 
	iowrite32(ENAMODE12_PERIODIC, davinci_wdt->base + TCR);
	 
	 
	iowrite32(WDKEY_SEQ0 | WDEN, davinci_wdt->base + WDTCR);
	 
	iowrite32(WDKEY_SEQ1 | WDEN, davinci_wdt->base + WDTCR);
	return 0;
}

static int davinci_wdt_ping(struct watchdog_device *wdd)
{
	struct davinci_wdt_device *davinci_wdt = watchdog_get_drvdata(wdd);

	 
	iowrite32(WDKEY_SEQ0, davinci_wdt->base + WDTCR);
	 
	iowrite32(WDKEY_SEQ1, davinci_wdt->base + WDTCR);
	return 0;
}

static unsigned int davinci_wdt_get_timeleft(struct watchdog_device *wdd)
{
	u64 timer_counter;
	unsigned long freq;
	u32 val;
	struct davinci_wdt_device *davinci_wdt = watchdog_get_drvdata(wdd);

	 
	val = ioread32(davinci_wdt->base + WDTCR);
	if (val & WDFLAG)
		return 0;

	freq = clk_get_rate(davinci_wdt->clk);

	if (!freq)
		return 0;

	timer_counter = ioread32(davinci_wdt->base + TIM12);
	timer_counter |= ((u64)ioread32(davinci_wdt->base + TIM34) << 32);

	timer_counter = div64_ul(timer_counter, freq);

	return wdd->timeout - timer_counter;
}

static int davinci_wdt_restart(struct watchdog_device *wdd,
			       unsigned long action, void *data)
{
	struct davinci_wdt_device *davinci_wdt = watchdog_get_drvdata(wdd);
	u32 tgcr, wdtcr;

	 
	iowrite32(0, davinci_wdt->base + TCR);

	 
	tgcr = 0;
	iowrite32(tgcr, davinci_wdt->base + TGCR);
	tgcr = TIMMODE_64BIT_WDOG | TIM12RS_UNRESET | TIM34RS_UNRESET;
	iowrite32(tgcr, davinci_wdt->base + TGCR);

	 
	iowrite32(0, davinci_wdt->base + TIM12);
	iowrite32(0, davinci_wdt->base + TIM34);
	iowrite32(0, davinci_wdt->base + PRD12);
	iowrite32(0, davinci_wdt->base + PRD34);

	 
	wdtcr = WDKEY_SEQ0 | WDEN;
	iowrite32(wdtcr, davinci_wdt->base + WDTCR);

	 
	wdtcr = WDKEY_SEQ1 | WDEN;
	iowrite32(wdtcr, davinci_wdt->base + WDTCR);

	 
	wdtcr = 0x00004000;
	iowrite32(wdtcr, davinci_wdt->base + WDTCR);

	return 0;
}

static const struct watchdog_info davinci_wdt_info = {
	.options = WDIOF_KEEPALIVEPING,
	.identity = "DaVinci/Keystone Watchdog",
};

static const struct watchdog_ops davinci_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= davinci_wdt_start,
	.stop		= davinci_wdt_ping,
	.ping		= davinci_wdt_ping,
	.get_timeleft	= davinci_wdt_get_timeleft,
	.restart	= davinci_wdt_restart,
};

static int davinci_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct watchdog_device *wdd;
	struct davinci_wdt_device *davinci_wdt;

	davinci_wdt = devm_kzalloc(dev, sizeof(*davinci_wdt), GFP_KERNEL);
	if (!davinci_wdt)
		return -ENOMEM;

	davinci_wdt->clk = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(davinci_wdt->clk))
		return dev_err_probe(dev, PTR_ERR(davinci_wdt->clk),
				     "failed to get clock node\n");

	platform_set_drvdata(pdev, davinci_wdt);

	wdd			= &davinci_wdt->wdd;
	wdd->info		= &davinci_wdt_info;
	wdd->ops		= &davinci_wdt_ops;
	wdd->min_timeout	= 1;
	wdd->max_timeout	= MAX_HEARTBEAT;
	wdd->timeout		= DEFAULT_HEARTBEAT;
	wdd->parent		= dev;

	watchdog_init_timeout(wdd, heartbeat, dev);

	dev_info(dev, "heartbeat %d sec\n", wdd->timeout);

	watchdog_set_drvdata(wdd, davinci_wdt);
	watchdog_set_nowayout(wdd, 1);
	watchdog_set_restart_priority(wdd, 128);

	davinci_wdt->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(davinci_wdt->base))
		return PTR_ERR(davinci_wdt->base);

	return devm_watchdog_register_device(dev, wdd);
}

static const struct of_device_id davinci_wdt_of_match[] = {
	{ .compatible = "ti,davinci-wdt", },
	{},
};
MODULE_DEVICE_TABLE(of, davinci_wdt_of_match);

static struct platform_driver platform_wdt_driver = {
	.driver = {
		.name = "davinci-wdt",
		.of_match_table = davinci_wdt_of_match,
	},
	.probe = davinci_wdt_probe,
};

module_platform_driver(platform_wdt_driver);

MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("DaVinci Watchdog Driver");

module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat,
		 "Watchdog heartbeat period in seconds from 1 to "
		 __MODULE_STRING(MAX_HEARTBEAT) ", default "
		 __MODULE_STRING(DEFAULT_HEARTBEAT));

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:davinci-wdt");
