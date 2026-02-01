
 

#include <linux/clk.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>

#define CDNS_WDT_DEFAULT_TIMEOUT	10
 
#define CDNS_WDT_MIN_TIMEOUT	1
#define CDNS_WDT_MAX_TIMEOUT	516

 
#define CDNS_WDT_RESTART_KEY 0x00001999

 
#define CDNS_WDT_REGISTER_ACCESS_KEY 0x00920000

 
#define CDNS_WDT_COUNTER_VALUE_DIVISOR 0x1000

 
#define CDNS_WDT_PRESCALE_64	64
#define CDNS_WDT_PRESCALE_512	512
#define CDNS_WDT_PRESCALE_4096	4096
#define CDNS_WDT_PRESCALE_SELECT_64	1
#define CDNS_WDT_PRESCALE_SELECT_512	2
#define CDNS_WDT_PRESCALE_SELECT_4096	3

 
#define CDNS_WDT_CLK_10MHZ	10000000
#define CDNS_WDT_CLK_75MHZ	75000000

 
#define CDNS_WDT_COUNTER_MAX 0xFFF

static int wdt_timeout;
static int nowayout = WATCHDOG_NOWAYOUT;

module_param(wdt_timeout, int, 0644);
MODULE_PARM_DESC(wdt_timeout,
		 "Watchdog time in seconds. (default="
		 __MODULE_STRING(CDNS_WDT_DEFAULT_TIMEOUT) ")");

module_param(nowayout, int, 0644);
MODULE_PARM_DESC(nowayout,
		 "Watchdog cannot be stopped once started (default="
		 __MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

 
struct cdns_wdt {
	void __iomem		*regs;
	bool			rst;
	struct clk		*clk;
	u32			prescaler;
	u32			ctrl_clksel;
	spinlock_t		io_lock;
	struct watchdog_device	cdns_wdt_device;
};

 
static inline void cdns_wdt_writereg(struct cdns_wdt *wdt, u32 offset, u32 val)
{
	writel_relaxed(val, wdt->regs + offset);
}

 

 
#define CDNS_WDT_ZMR_OFFSET	0x0	 
#define CDNS_WDT_CCR_OFFSET	0x4	 
#define CDNS_WDT_RESTART_OFFSET	0x8	 
#define CDNS_WDT_SR_OFFSET	0xC	 

 
#define CDNS_WDT_ZMR_WDEN_MASK	0x00000001  
#define CDNS_WDT_ZMR_RSTEN_MASK	0x00000002  
#define CDNS_WDT_ZMR_IRQEN_MASK	0x00000004  
#define CDNS_WDT_ZMR_RSTLEN_16	0x00000030  
#define CDNS_WDT_ZMR_ZKEY_VAL	0x00ABC000  
 
#define CDNS_WDT_CCR_CRV_MASK	0x00003FFC  

 
static int cdns_wdt_stop(struct watchdog_device *wdd)
{
	struct cdns_wdt *wdt = watchdog_get_drvdata(wdd);

	spin_lock(&wdt->io_lock);
	cdns_wdt_writereg(wdt, CDNS_WDT_ZMR_OFFSET,
			  CDNS_WDT_ZMR_ZKEY_VAL & (~CDNS_WDT_ZMR_WDEN_MASK));
	spin_unlock(&wdt->io_lock);

	return 0;
}

 
static int cdns_wdt_reload(struct watchdog_device *wdd)
{
	struct cdns_wdt *wdt = watchdog_get_drvdata(wdd);

	spin_lock(&wdt->io_lock);
	cdns_wdt_writereg(wdt, CDNS_WDT_RESTART_OFFSET,
			  CDNS_WDT_RESTART_KEY);
	spin_unlock(&wdt->io_lock);

	return 0;
}

 
static int cdns_wdt_start(struct watchdog_device *wdd)
{
	struct cdns_wdt *wdt = watchdog_get_drvdata(wdd);
	unsigned int data = 0;
	unsigned short count;
	unsigned long clock_f = clk_get_rate(wdt->clk);

	 
	count = (wdd->timeout * (clock_f / wdt->prescaler)) /
		 CDNS_WDT_COUNTER_VALUE_DIVISOR + 1;

	if (count > CDNS_WDT_COUNTER_MAX)
		count = CDNS_WDT_COUNTER_MAX;

	spin_lock(&wdt->io_lock);
	cdns_wdt_writereg(wdt, CDNS_WDT_ZMR_OFFSET,
			  CDNS_WDT_ZMR_ZKEY_VAL);

	count = (count << 2) & CDNS_WDT_CCR_CRV_MASK;

	 
	data = count | CDNS_WDT_REGISTER_ACCESS_KEY | wdt->ctrl_clksel;
	cdns_wdt_writereg(wdt, CDNS_WDT_CCR_OFFSET, data);
	data = CDNS_WDT_ZMR_WDEN_MASK | CDNS_WDT_ZMR_RSTLEN_16 |
	       CDNS_WDT_ZMR_ZKEY_VAL;

	 
	if (wdt->rst) {
		data |= CDNS_WDT_ZMR_RSTEN_MASK;
		data &= ~CDNS_WDT_ZMR_IRQEN_MASK;
	} else {
		data &= ~CDNS_WDT_ZMR_RSTEN_MASK;
		data |= CDNS_WDT_ZMR_IRQEN_MASK;
	}
	cdns_wdt_writereg(wdt, CDNS_WDT_ZMR_OFFSET, data);
	cdns_wdt_writereg(wdt, CDNS_WDT_RESTART_OFFSET,
			  CDNS_WDT_RESTART_KEY);
	spin_unlock(&wdt->io_lock);

	return 0;
}

 
static int cdns_wdt_settimeout(struct watchdog_device *wdd,
			       unsigned int new_time)
{
	wdd->timeout = new_time;

	return cdns_wdt_start(wdd);
}

 
static irqreturn_t cdns_wdt_irq_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;

	dev_info(&pdev->dev,
		 "Watchdog timed out. Internal reset not enabled\n");

	return IRQ_HANDLED;
}

 
static const struct watchdog_info cdns_wdt_info = {
	.identity	= "cdns_wdt watchdog",
	.options	= WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING |
			  WDIOF_MAGICCLOSE,
};

 
static const struct watchdog_ops cdns_wdt_ops = {
	.owner = THIS_MODULE,
	.start = cdns_wdt_start,
	.stop = cdns_wdt_stop,
	.ping = cdns_wdt_reload,
	.set_timeout = cdns_wdt_settimeout,
};

 
 
static int cdns_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret, irq;
	unsigned long clock_f;
	struct cdns_wdt *wdt;
	struct watchdog_device *cdns_wdt_device;

	wdt = devm_kzalloc(dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	cdns_wdt_device = &wdt->cdns_wdt_device;
	cdns_wdt_device->info = &cdns_wdt_info;
	cdns_wdt_device->ops = &cdns_wdt_ops;
	cdns_wdt_device->timeout = CDNS_WDT_DEFAULT_TIMEOUT;
	cdns_wdt_device->min_timeout = CDNS_WDT_MIN_TIMEOUT;
	cdns_wdt_device->max_timeout = CDNS_WDT_MAX_TIMEOUT;

	wdt->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(wdt->regs))
		return PTR_ERR(wdt->regs);

	 
	wdt->rst = of_property_read_bool(dev->of_node, "reset-on-timeout");
	irq = platform_get_irq(pdev, 0);
	if (!wdt->rst && irq >= 0) {
		ret = devm_request_irq(dev, irq, cdns_wdt_irq_handler, 0,
				       pdev->name, pdev);
		if (ret) {
			dev_err(dev,
				"cannot register interrupt handler err=%d\n",
				ret);
			return ret;
		}
	}

	 
	cdns_wdt_device->parent = dev;

	watchdog_init_timeout(cdns_wdt_device, wdt_timeout, dev);
	watchdog_set_nowayout(cdns_wdt_device, nowayout);
	watchdog_stop_on_reboot(cdns_wdt_device);
	watchdog_set_drvdata(cdns_wdt_device, wdt);

	wdt->clk = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(wdt->clk))
		return dev_err_probe(dev, PTR_ERR(wdt->clk),
				     "input clock not found\n");

	clock_f = clk_get_rate(wdt->clk);
	if (clock_f <= CDNS_WDT_CLK_75MHZ) {
		wdt->prescaler = CDNS_WDT_PRESCALE_512;
		wdt->ctrl_clksel = CDNS_WDT_PRESCALE_SELECT_512;
	} else {
		wdt->prescaler = CDNS_WDT_PRESCALE_4096;
		wdt->ctrl_clksel = CDNS_WDT_PRESCALE_SELECT_4096;
	}

	spin_lock_init(&wdt->io_lock);

	watchdog_stop_on_reboot(cdns_wdt_device);
	watchdog_stop_on_unregister(cdns_wdt_device);
	ret = devm_watchdog_register_device(dev, cdns_wdt_device);
	if (ret)
		return ret;
	platform_set_drvdata(pdev, wdt);

	dev_info(dev, "Xilinx Watchdog Timer with timeout %ds%s\n",
		 cdns_wdt_device->timeout, nowayout ? ", nowayout" : "");

	return 0;
}

 
static int __maybe_unused cdns_wdt_suspend(struct device *dev)
{
	struct cdns_wdt *wdt = dev_get_drvdata(dev);

	if (watchdog_active(&wdt->cdns_wdt_device)) {
		cdns_wdt_stop(&wdt->cdns_wdt_device);
		clk_disable_unprepare(wdt->clk);
	}

	return 0;
}

 
static int __maybe_unused cdns_wdt_resume(struct device *dev)
{
	int ret;
	struct cdns_wdt *wdt = dev_get_drvdata(dev);

	if (watchdog_active(&wdt->cdns_wdt_device)) {
		ret = clk_prepare_enable(wdt->clk);
		if (ret) {
			dev_err(dev, "unable to enable clock\n");
			return ret;
		}
		cdns_wdt_start(&wdt->cdns_wdt_device);
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(cdns_wdt_pm_ops, cdns_wdt_suspend, cdns_wdt_resume);

static const struct of_device_id cdns_wdt_of_match[] = {
	{ .compatible = "cdns,wdt-r1p2", },
	{   }
};
MODULE_DEVICE_TABLE(of, cdns_wdt_of_match);

 
static struct platform_driver cdns_wdt_driver = {
	.probe		= cdns_wdt_probe,
	.driver		= {
		.name	= "cdns-wdt",
		.of_match_table = cdns_wdt_of_match,
		.pm	= &cdns_wdt_pm_ops,
	},
};

module_platform_driver(cdns_wdt_driver);

MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION("Watchdog driver for Cadence WDT");
MODULE_LICENSE("GPL");
