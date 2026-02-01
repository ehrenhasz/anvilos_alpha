
 

#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kstrtox.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

struct aspeed_wdt_config {
	u32 ext_pulse_width_mask;
	u32 irq_shift;
	u32 irq_mask;
};

struct aspeed_wdt {
	struct watchdog_device	wdd;
	void __iomem		*base;
	u32			ctrl;
	const struct aspeed_wdt_config *cfg;
};

static const struct aspeed_wdt_config ast2400_config = {
	.ext_pulse_width_mask = 0xff,
	.irq_shift = 0,
	.irq_mask = 0,
};

static const struct aspeed_wdt_config ast2500_config = {
	.ext_pulse_width_mask = 0xfffff,
	.irq_shift = 12,
	.irq_mask = GENMASK(31, 12),
};

static const struct aspeed_wdt_config ast2600_config = {
	.ext_pulse_width_mask = 0xfffff,
	.irq_shift = 0,
	.irq_mask = GENMASK(31, 10),
};

static const struct of_device_id aspeed_wdt_of_table[] = {
	{ .compatible = "aspeed,ast2400-wdt", .data = &ast2400_config },
	{ .compatible = "aspeed,ast2500-wdt", .data = &ast2500_config },
	{ .compatible = "aspeed,ast2600-wdt", .data = &ast2600_config },
	{ },
};
MODULE_DEVICE_TABLE(of, aspeed_wdt_of_table);

#define WDT_STATUS		0x00
#define WDT_RELOAD_VALUE	0x04
#define WDT_RESTART		0x08
#define WDT_CTRL		0x0C
#define   WDT_CTRL_BOOT_SECONDARY	BIT(7)
#define   WDT_CTRL_RESET_MODE_SOC	(0x00 << 5)
#define   WDT_CTRL_RESET_MODE_FULL_CHIP	(0x01 << 5)
#define   WDT_CTRL_RESET_MODE_ARM_CPU	(0x10 << 5)
#define   WDT_CTRL_1MHZ_CLK		BIT(4)
#define   WDT_CTRL_WDT_EXT		BIT(3)
#define   WDT_CTRL_WDT_INTR		BIT(2)
#define   WDT_CTRL_RESET_SYSTEM		BIT(1)
#define   WDT_CTRL_ENABLE		BIT(0)
#define WDT_TIMEOUT_STATUS	0x10
#define   WDT_TIMEOUT_STATUS_IRQ		BIT(2)
#define   WDT_TIMEOUT_STATUS_BOOT_SECONDARY	BIT(1)
#define WDT_CLEAR_TIMEOUT_STATUS	0x14
#define   WDT_CLEAR_TIMEOUT_AND_BOOT_CODE_SELECTION	BIT(0)

 
#define WDT_RESET_WIDTH		0x18
#define   WDT_RESET_WIDTH_ACTIVE_HIGH	BIT(31)
#define     WDT_ACTIVE_HIGH_MAGIC	(0xA5 << 24)
#define     WDT_ACTIVE_LOW_MAGIC	(0x5A << 24)
#define   WDT_RESET_WIDTH_PUSH_PULL	BIT(30)
#define     WDT_PUSH_PULL_MAGIC		(0xA8 << 24)
#define     WDT_OPEN_DRAIN_MAGIC	(0x8A << 24)

#define WDT_RESTART_MAGIC	0x4755

 
#define WDT_MAX_TIMEOUT_MS	4294967
#define WDT_DEFAULT_TIMEOUT	30
#define WDT_RATE_1MHZ		1000000

static struct aspeed_wdt *to_aspeed_wdt(struct watchdog_device *wdd)
{
	return container_of(wdd, struct aspeed_wdt, wdd);
}

static void aspeed_wdt_enable(struct aspeed_wdt *wdt, int count)
{
	wdt->ctrl |= WDT_CTRL_ENABLE;

	writel(0, wdt->base + WDT_CTRL);
	writel(count, wdt->base + WDT_RELOAD_VALUE);
	writel(WDT_RESTART_MAGIC, wdt->base + WDT_RESTART);
	writel(wdt->ctrl, wdt->base + WDT_CTRL);
}

static int aspeed_wdt_start(struct watchdog_device *wdd)
{
	struct aspeed_wdt *wdt = to_aspeed_wdt(wdd);

	aspeed_wdt_enable(wdt, wdd->timeout * WDT_RATE_1MHZ);

	return 0;
}

static int aspeed_wdt_stop(struct watchdog_device *wdd)
{
	struct aspeed_wdt *wdt = to_aspeed_wdt(wdd);

	wdt->ctrl &= ~WDT_CTRL_ENABLE;
	writel(wdt->ctrl, wdt->base + WDT_CTRL);

	return 0;
}

static int aspeed_wdt_ping(struct watchdog_device *wdd)
{
	struct aspeed_wdt *wdt = to_aspeed_wdt(wdd);

	writel(WDT_RESTART_MAGIC, wdt->base + WDT_RESTART);

	return 0;
}

static int aspeed_wdt_set_timeout(struct watchdog_device *wdd,
				  unsigned int timeout)
{
	struct aspeed_wdt *wdt = to_aspeed_wdt(wdd);
	u32 actual;

	wdd->timeout = timeout;

	actual = min(timeout, wdd->max_hw_heartbeat_ms / 1000);

	writel(actual * WDT_RATE_1MHZ, wdt->base + WDT_RELOAD_VALUE);
	writel(WDT_RESTART_MAGIC, wdt->base + WDT_RESTART);

	return 0;
}

static int aspeed_wdt_set_pretimeout(struct watchdog_device *wdd,
				     unsigned int pretimeout)
{
	struct aspeed_wdt *wdt = to_aspeed_wdt(wdd);
	u32 actual = pretimeout * WDT_RATE_1MHZ;
	u32 s = wdt->cfg->irq_shift;
	u32 m = wdt->cfg->irq_mask;

	wdd->pretimeout = pretimeout;
	wdt->ctrl &= ~m;
	if (pretimeout)
		wdt->ctrl |= ((actual << s) & m) | WDT_CTRL_WDT_INTR;
	else
		wdt->ctrl &= ~WDT_CTRL_WDT_INTR;

	writel(wdt->ctrl, wdt->base + WDT_CTRL);

	return 0;
}

static int aspeed_wdt_restart(struct watchdog_device *wdd,
			      unsigned long action, void *data)
{
	struct aspeed_wdt *wdt = to_aspeed_wdt(wdd);

	wdt->ctrl &= ~WDT_CTRL_BOOT_SECONDARY;
	aspeed_wdt_enable(wdt, 128 * WDT_RATE_1MHZ / 1000);

	mdelay(1000);

	return 0;
}

 
static ssize_t access_cs0_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct aspeed_wdt *wdt = dev_get_drvdata(dev);
	u32 status = readl(wdt->base + WDT_TIMEOUT_STATUS);

	return sysfs_emit(buf, "%u\n",
			  !(status & WDT_TIMEOUT_STATUS_BOOT_SECONDARY));
}

static ssize_t access_cs0_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct aspeed_wdt *wdt = dev_get_drvdata(dev);
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
		writel(WDT_CLEAR_TIMEOUT_AND_BOOT_CODE_SELECTION,
		       wdt->base + WDT_CLEAR_TIMEOUT_STATUS);

	return size;
}

 
static DEVICE_ATTR_RW(access_cs0);

static struct attribute *bswitch_attrs[] = {
	&dev_attr_access_cs0.attr,
	NULL
};
ATTRIBUTE_GROUPS(bswitch);

static const struct watchdog_ops aspeed_wdt_ops = {
	.start		= aspeed_wdt_start,
	.stop		= aspeed_wdt_stop,
	.ping		= aspeed_wdt_ping,
	.set_timeout	= aspeed_wdt_set_timeout,
	.set_pretimeout = aspeed_wdt_set_pretimeout,
	.restart	= aspeed_wdt_restart,
	.owner		= THIS_MODULE,
};

static const struct watchdog_info aspeed_wdt_info = {
	.options	= WDIOF_KEEPALIVEPING
			| WDIOF_MAGICCLOSE
			| WDIOF_SETTIMEOUT,
	.identity	= KBUILD_MODNAME,
};

static const struct watchdog_info aspeed_wdt_pretimeout_info = {
	.options	= WDIOF_KEEPALIVEPING
			| WDIOF_PRETIMEOUT
			| WDIOF_MAGICCLOSE
			| WDIOF_SETTIMEOUT,
	.identity	= KBUILD_MODNAME,
};

static irqreturn_t aspeed_wdt_irq(int irq, void *arg)
{
	struct watchdog_device *wdd = arg;
	struct aspeed_wdt *wdt = to_aspeed_wdt(wdd);
	u32 status = readl(wdt->base + WDT_TIMEOUT_STATUS);

	if (status & WDT_TIMEOUT_STATUS_IRQ)
		watchdog_notify_pretimeout(wdd);

	return IRQ_HANDLED;
}

static int aspeed_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *ofdid;
	struct aspeed_wdt *wdt;
	struct device_node *np;
	const char *reset_type;
	u32 duration;
	u32 status;
	int ret;

	wdt = devm_kzalloc(dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	np = dev->of_node;

	ofdid = of_match_node(aspeed_wdt_of_table, np);
	if (!ofdid)
		return -EINVAL;
	wdt->cfg = ofdid->data;

	wdt->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(wdt->base))
		return PTR_ERR(wdt->base);

	wdt->wdd.info = &aspeed_wdt_info;

	if (wdt->cfg->irq_mask) {
		int irq = platform_get_irq_optional(pdev, 0);

		if (irq > 0) {
			ret = devm_request_irq(dev, irq, aspeed_wdt_irq,
					       IRQF_SHARED, dev_name(dev),
					       wdt);
			if (ret)
				return ret;

			wdt->wdd.info = &aspeed_wdt_pretimeout_info;
		}
	}

	wdt->wdd.ops = &aspeed_wdt_ops;
	wdt->wdd.max_hw_heartbeat_ms = WDT_MAX_TIMEOUT_MS;
	wdt->wdd.parent = dev;

	wdt->wdd.timeout = WDT_DEFAULT_TIMEOUT;
	watchdog_init_timeout(&wdt->wdd, 0, dev);

	watchdog_set_nowayout(&wdt->wdd, nowayout);

	 
	if (of_device_is_compatible(np, "aspeed,ast2400-wdt"))
		wdt->ctrl = WDT_CTRL_1MHZ_CLK;

	 
	ret = of_property_read_string(np, "aspeed,reset-type", &reset_type);
	if (ret) {
		wdt->ctrl |= WDT_CTRL_RESET_MODE_SOC | WDT_CTRL_RESET_SYSTEM;
	} else {
		if (!strcmp(reset_type, "cpu"))
			wdt->ctrl |= WDT_CTRL_RESET_MODE_ARM_CPU |
				     WDT_CTRL_RESET_SYSTEM;
		else if (!strcmp(reset_type, "soc"))
			wdt->ctrl |= WDT_CTRL_RESET_MODE_SOC |
				     WDT_CTRL_RESET_SYSTEM;
		else if (!strcmp(reset_type, "system"))
			wdt->ctrl |= WDT_CTRL_RESET_MODE_FULL_CHIP |
				     WDT_CTRL_RESET_SYSTEM;
		else if (strcmp(reset_type, "none"))
			return -EINVAL;
	}
	if (of_property_read_bool(np, "aspeed,external-signal"))
		wdt->ctrl |= WDT_CTRL_WDT_EXT;
	if (of_property_read_bool(np, "aspeed,alt-boot"))
		wdt->ctrl |= WDT_CTRL_BOOT_SECONDARY;

	if (readl(wdt->base + WDT_CTRL) & WDT_CTRL_ENABLE)  {
		 
		aspeed_wdt_start(&wdt->wdd);
		set_bit(WDOG_HW_RUNNING, &wdt->wdd.status);
	}

	if ((of_device_is_compatible(np, "aspeed,ast2500-wdt")) ||
		(of_device_is_compatible(np, "aspeed,ast2600-wdt"))) {
		u32 reg = readl(wdt->base + WDT_RESET_WIDTH);

		reg &= wdt->cfg->ext_pulse_width_mask;
		if (of_property_read_bool(np, "aspeed,ext-active-high"))
			reg |= WDT_ACTIVE_HIGH_MAGIC;
		else
			reg |= WDT_ACTIVE_LOW_MAGIC;

		writel(reg, wdt->base + WDT_RESET_WIDTH);

		reg &= wdt->cfg->ext_pulse_width_mask;
		if (of_property_read_bool(np, "aspeed,ext-push-pull"))
			reg |= WDT_PUSH_PULL_MAGIC;
		else
			reg |= WDT_OPEN_DRAIN_MAGIC;

		writel(reg, wdt->base + WDT_RESET_WIDTH);
	}

	if (!of_property_read_u32(np, "aspeed,ext-pulse-duration", &duration)) {
		u32 max_duration = wdt->cfg->ext_pulse_width_mask + 1;

		if (duration == 0 || duration > max_duration) {
			dev_err(dev, "Invalid pulse duration: %uus\n",
				duration);
			duration = max(1U, min(max_duration, duration));
			dev_info(dev, "Pulse duration set to %uus\n",
				 duration);
		}

		 
		writel(duration - 1, wdt->base + WDT_RESET_WIDTH);
	}

	status = readl(wdt->base + WDT_TIMEOUT_STATUS);
	if (status & WDT_TIMEOUT_STATUS_BOOT_SECONDARY) {
		wdt->wdd.bootstatus = WDIOF_CARDRESET;

		if (of_device_is_compatible(np, "aspeed,ast2400-wdt") ||
		    of_device_is_compatible(np, "aspeed,ast2500-wdt"))
			wdt->wdd.groups = bswitch_groups;
	}

	dev_set_drvdata(dev, wdt);

	return devm_watchdog_register_device(dev, &wdt->wdd);
}

static struct platform_driver aspeed_watchdog_driver = {
	.probe = aspeed_wdt_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = aspeed_wdt_of_table,
	},
};

static int __init aspeed_wdt_init(void)
{
	return platform_driver_register(&aspeed_watchdog_driver);
}
arch_initcall(aspeed_wdt_init);

static void __exit aspeed_wdt_exit(void)
{
	platform_driver_unregister(&aspeed_watchdog_driver);
}
module_exit(aspeed_wdt_exit);

MODULE_DESCRIPTION("Aspeed Watchdog Driver");
MODULE_LICENSE("GPL");
