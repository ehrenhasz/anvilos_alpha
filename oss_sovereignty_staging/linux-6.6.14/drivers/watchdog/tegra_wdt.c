
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>

 
#define MIN_WDT_TIMEOUT			1
#define MAX_WDT_TIMEOUT			255

 
#define WDT_BASE			0x100
#define WDT_ID				0

 
#define WDT_TIMER_BASE			0x60
#define WDT_TIMER_ID			5

 
#define WDT_CFG				0x0
#define WDT_CFG_PERIOD_SHIFT		4
#define WDT_CFG_PERIOD_MASK		0xff
#define WDT_CFG_INT_EN			(1 << 12)
#define WDT_CFG_PMC2CAR_RST_EN		(1 << 15)
#define WDT_STS				0x4
#define WDT_STS_COUNT_SHIFT		4
#define WDT_STS_COUNT_MASK		0xff
#define WDT_STS_EXP_SHIFT		12
#define WDT_STS_EXP_MASK		0x3
#define WDT_CMD				0x8
#define WDT_CMD_START_COUNTER		(1 << 0)
#define WDT_CMD_DISABLE_COUNTER		(1 << 1)
#define WDT_UNLOCK			(0xc)
#define WDT_UNLOCK_PATTERN		(0xc45a << 0)

 
#define TIMER_PTV			0x0
#define TIMER_EN			(1 << 31)
#define TIMER_PERIODIC			(1 << 30)

struct tegra_wdt {
	struct watchdog_device	wdd;
	void __iomem		*wdt_regs;
	void __iomem		*tmr_regs;
};

#define WDT_HEARTBEAT 120
static int heartbeat = WDT_HEARTBEAT;
module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat,
	"Watchdog heartbeats in seconds. (default = "
	__MODULE_STRING(WDT_HEARTBEAT) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
	"Watchdog cannot be stopped once started (default="
	__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static int tegra_wdt_start(struct watchdog_device *wdd)
{
	struct tegra_wdt *wdt = watchdog_get_drvdata(wdd);
	u32 val;

	 
	val = 1000000ul / 4;
	val |= (TIMER_EN | TIMER_PERIODIC);
	writel(val, wdt->tmr_regs + TIMER_PTV);

	 
	val = WDT_TIMER_ID |
	      (wdd->timeout << WDT_CFG_PERIOD_SHIFT) |
	      WDT_CFG_PMC2CAR_RST_EN;
	writel(val, wdt->wdt_regs + WDT_CFG);

	writel(WDT_CMD_START_COUNTER, wdt->wdt_regs + WDT_CMD);

	return 0;
}

static int tegra_wdt_stop(struct watchdog_device *wdd)
{
	struct tegra_wdt *wdt = watchdog_get_drvdata(wdd);

	writel(WDT_UNLOCK_PATTERN, wdt->wdt_regs + WDT_UNLOCK);
	writel(WDT_CMD_DISABLE_COUNTER, wdt->wdt_regs + WDT_CMD);
	writel(0, wdt->tmr_regs + TIMER_PTV);

	return 0;
}

static int tegra_wdt_ping(struct watchdog_device *wdd)
{
	struct tegra_wdt *wdt = watchdog_get_drvdata(wdd);

	writel(WDT_CMD_START_COUNTER, wdt->wdt_regs + WDT_CMD);

	return 0;
}

static int tegra_wdt_set_timeout(struct watchdog_device *wdd,
				 unsigned int timeout)
{
	wdd->timeout = timeout;

	if (watchdog_active(wdd)) {
		tegra_wdt_stop(wdd);
		return tegra_wdt_start(wdd);
	}

	return 0;
}

static unsigned int tegra_wdt_get_timeleft(struct watchdog_device *wdd)
{
	struct tegra_wdt *wdt = watchdog_get_drvdata(wdd);
	u32 val;
	int count;
	int exp;

	val = readl(wdt->wdt_regs + WDT_STS);

	 
	count = (val >> WDT_STS_COUNT_SHIFT) & WDT_STS_COUNT_MASK;

	 
	exp = (val >> WDT_STS_EXP_SHIFT) & WDT_STS_EXP_MASK;

	 
	return (((3 - exp) * wdd->timeout) + count) / 4;
}

static const struct watchdog_info tegra_wdt_info = {
	.options	= WDIOF_SETTIMEOUT |
			  WDIOF_MAGICCLOSE |
			  WDIOF_KEEPALIVEPING,
	.firmware_version = 0,
	.identity	= "Tegra Watchdog",
};

static const struct watchdog_ops tegra_wdt_ops = {
	.owner = THIS_MODULE,
	.start = tegra_wdt_start,
	.stop = tegra_wdt_stop,
	.ping = tegra_wdt_ping,
	.set_timeout = tegra_wdt_set_timeout,
	.get_timeleft = tegra_wdt_get_timeleft,
};

static int tegra_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct watchdog_device *wdd;
	struct tegra_wdt *wdt;
	void __iomem *regs;
	int ret;

	 
	regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	 
	wdt = devm_kzalloc(dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	 
	wdt->wdt_regs = regs + WDT_BASE;
	wdt->tmr_regs = regs + WDT_TIMER_BASE;

	 
	wdd = &wdt->wdd;
	wdd->timeout = heartbeat;
	wdd->info = &tegra_wdt_info;
	wdd->ops = &tegra_wdt_ops;
	wdd->min_timeout = MIN_WDT_TIMEOUT;
	wdd->max_timeout = MAX_WDT_TIMEOUT;
	wdd->parent = dev;

	watchdog_set_drvdata(wdd, wdt);

	watchdog_set_nowayout(wdd, nowayout);

	watchdog_stop_on_unregister(wdd);
	ret = devm_watchdog_register_device(dev, wdd);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, wdt);

	dev_info(dev, "initialized (heartbeat = %d sec, nowayout = %d)\n",
		 heartbeat, nowayout);

	return 0;
}

static int tegra_wdt_suspend(struct device *dev)
{
	struct tegra_wdt *wdt = dev_get_drvdata(dev);

	if (watchdog_active(&wdt->wdd))
		tegra_wdt_stop(&wdt->wdd);

	return 0;
}

static int tegra_wdt_resume(struct device *dev)
{
	struct tegra_wdt *wdt = dev_get_drvdata(dev);

	if (watchdog_active(&wdt->wdd))
		tegra_wdt_start(&wdt->wdd);

	return 0;
}

static const struct of_device_id tegra_wdt_of_match[] = {
	{ .compatible = "nvidia,tegra30-timer", },
	{ },
};
MODULE_DEVICE_TABLE(of, tegra_wdt_of_match);

static DEFINE_SIMPLE_DEV_PM_OPS(tegra_wdt_pm_ops,
				tegra_wdt_suspend, tegra_wdt_resume);

static struct platform_driver tegra_wdt_driver = {
	.probe		= tegra_wdt_probe,
	.driver		= {
		.name	= "tegra-wdt",
		.pm	= pm_sleep_ptr(&tegra_wdt_pm_ops),
		.of_match_table = tegra_wdt_of_match,
	},
};
module_platform_driver(tegra_wdt_driver);

MODULE_AUTHOR("NVIDIA Corporation");
MODULE_DESCRIPTION("Tegra Watchdog Driver");
MODULE_LICENSE("GPL v2");
