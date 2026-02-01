
 

#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>

 
#define APPLE_WDT_WD0_CUR_TIME		0x00
#define APPLE_WDT_WD0_BITE_TIME		0x04
#define APPLE_WDT_WD0_BARK_TIME		0x08
#define APPLE_WDT_WD0_CTRL		0x0c

#define APPLE_WDT_WD1_CUR_TIME		0x10
#define APPLE_WDT_WD1_BITE_TIME		0x14
#define APPLE_WDT_WD1_CTRL		0x1c

#define APPLE_WDT_WD2_CUR_TIME		0x20
#define APPLE_WDT_WD2_BITE_TIME		0x24
#define APPLE_WDT_WD2_CTRL		0x2c

#define APPLE_WDT_CTRL_IRQ_EN		BIT(0)
#define APPLE_WDT_CTRL_IRQ_STATUS	BIT(1)
#define APPLE_WDT_CTRL_RESET_EN		BIT(2)

#define APPLE_WDT_TIMEOUT_DEFAULT	30

struct apple_wdt {
	struct watchdog_device wdd;
	void __iomem *regs;
	unsigned long clk_rate;
};

static struct apple_wdt *to_apple_wdt(struct watchdog_device *wdd)
{
	return container_of(wdd, struct apple_wdt, wdd);
}

static int apple_wdt_start(struct watchdog_device *wdd)
{
	struct apple_wdt *wdt = to_apple_wdt(wdd);

	writel_relaxed(0, wdt->regs + APPLE_WDT_WD1_CUR_TIME);
	writel_relaxed(APPLE_WDT_CTRL_RESET_EN, wdt->regs + APPLE_WDT_WD1_CTRL);

	return 0;
}

static int apple_wdt_stop(struct watchdog_device *wdd)
{
	struct apple_wdt *wdt = to_apple_wdt(wdd);

	writel_relaxed(0, wdt->regs + APPLE_WDT_WD1_CTRL);

	return 0;
}

static int apple_wdt_ping(struct watchdog_device *wdd)
{
	struct apple_wdt *wdt = to_apple_wdt(wdd);

	writel_relaxed(0, wdt->regs + APPLE_WDT_WD1_CUR_TIME);

	return 0;
}

static int apple_wdt_set_timeout(struct watchdog_device *wdd, unsigned int s)
{
	struct apple_wdt *wdt = to_apple_wdt(wdd);

	writel_relaxed(0, wdt->regs + APPLE_WDT_WD1_CUR_TIME);
	writel_relaxed(wdt->clk_rate * s, wdt->regs + APPLE_WDT_WD1_BITE_TIME);

	wdd->timeout = s;

	return 0;
}

static unsigned int apple_wdt_get_timeleft(struct watchdog_device *wdd)
{
	struct apple_wdt *wdt = to_apple_wdt(wdd);
	u32 cur_time, reset_time;

	cur_time = readl_relaxed(wdt->regs + APPLE_WDT_WD1_CUR_TIME);
	reset_time = readl_relaxed(wdt->regs + APPLE_WDT_WD1_BITE_TIME);

	return (reset_time - cur_time) / wdt->clk_rate;
}

static int apple_wdt_restart(struct watchdog_device *wdd, unsigned long mode,
			     void *cmd)
{
	struct apple_wdt *wdt = to_apple_wdt(wdd);

	writel_relaxed(APPLE_WDT_CTRL_RESET_EN, wdt->regs + APPLE_WDT_WD1_CTRL);
	writel_relaxed(0, wdt->regs + APPLE_WDT_WD1_BITE_TIME);
	writel_relaxed(0, wdt->regs + APPLE_WDT_WD1_CUR_TIME);

	 
	(void)readl_relaxed(wdt->regs + APPLE_WDT_WD1_CUR_TIME);
	mdelay(50);

	return 0;
}

static struct watchdog_ops apple_wdt_ops = {
	.owner = THIS_MODULE,
	.start = apple_wdt_start,
	.stop = apple_wdt_stop,
	.ping = apple_wdt_ping,
	.set_timeout = apple_wdt_set_timeout,
	.get_timeleft = apple_wdt_get_timeleft,
	.restart = apple_wdt_restart,
};

static struct watchdog_info apple_wdt_info = {
	.identity = "Apple SoC Watchdog",
	.options = WDIOF_MAGICCLOSE | WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT,
};

static int apple_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct apple_wdt *wdt;
	struct clk *clk;
	u32 wdt_ctrl;

	wdt = devm_kzalloc(dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	wdt->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(wdt->regs))
		return PTR_ERR(wdt->regs);

	clk = devm_clk_get_enabled(dev, NULL);
	if (IS_ERR(clk))
		return PTR_ERR(clk);
	wdt->clk_rate = clk_get_rate(clk);
	if (!wdt->clk_rate)
		return -EINVAL;

	wdt->wdd.ops = &apple_wdt_ops;
	wdt->wdd.info = &apple_wdt_info;
	wdt->wdd.max_timeout = U32_MAX / wdt->clk_rate;
	wdt->wdd.timeout = APPLE_WDT_TIMEOUT_DEFAULT;

	wdt_ctrl = readl_relaxed(wdt->regs + APPLE_WDT_WD1_CTRL);
	if (wdt_ctrl & APPLE_WDT_CTRL_RESET_EN)
		set_bit(WDOG_HW_RUNNING, &wdt->wdd.status);

	watchdog_init_timeout(&wdt->wdd, 0, dev);
	apple_wdt_set_timeout(&wdt->wdd, wdt->wdd.timeout);
	watchdog_stop_on_unregister(&wdt->wdd);
	watchdog_set_restart_priority(&wdt->wdd, 128);

	return devm_watchdog_register_device(dev, &wdt->wdd);
}

static const struct of_device_id apple_wdt_of_match[] = {
	{ .compatible = "apple,wdt" },
	{},
};
MODULE_DEVICE_TABLE(of, apple_wdt_of_match);

static struct platform_driver apple_wdt_driver = {
	.driver = {
		.name = "apple-watchdog",
		.of_match_table = apple_wdt_of_match,
	},
	.probe = apple_wdt_probe,
};
module_platform_driver(apple_wdt_driver);

MODULE_DESCRIPTION("Apple SoC watchdog driver");
MODULE_AUTHOR("Sven Peter <sven@svenpeter.dev>");
MODULE_LICENSE("Dual MIT/GPL");
