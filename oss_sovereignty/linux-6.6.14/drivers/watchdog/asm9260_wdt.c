
 

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/watchdog.h>

#define CLOCK_FREQ	1000000

 
#define HW_WDMOD			0x00
 
#define BM_MOD_WDINT			BIT(3)
 
#define BM_MOD_WDTOF			BIT(2)
 
#define BM_MOD_WDRESET			BIT(1)
 
#define BM_MOD_WDEN			BIT(0)

 
#define HW_WDTC				0x04
#define BM_WDTC_MAX(freq)		(0x7fffffff / (freq))

 
#define HW_WDFEED			0x08

 
#define HW_WDTV				0x0c

#define ASM9260_WDT_DEFAULT_TIMEOUT	30

enum asm9260_wdt_mode {
	HW_RESET,
	SW_RESET,
	DEBUG,
};

struct asm9260_wdt_priv {
	struct device		*dev;
	struct watchdog_device	wdd;
	struct clk		*clk;
	struct clk		*clk_ahb;
	struct reset_control	*rst;

	void __iomem		*iobase;
	int			irq;
	unsigned long		wdt_freq;
	enum asm9260_wdt_mode	mode;
};

static int asm9260_wdt_feed(struct watchdog_device *wdd)
{
	struct asm9260_wdt_priv *priv = watchdog_get_drvdata(wdd);

	iowrite32(0xaa, priv->iobase + HW_WDFEED);
	iowrite32(0x55, priv->iobase + HW_WDFEED);

	return 0;
}

static unsigned int asm9260_wdt_gettimeleft(struct watchdog_device *wdd)
{
	struct asm9260_wdt_priv *priv = watchdog_get_drvdata(wdd);
	u32 counter;

	counter = ioread32(priv->iobase + HW_WDTV);

	return counter / priv->wdt_freq;
}

static int asm9260_wdt_updatetimeout(struct watchdog_device *wdd)
{
	struct asm9260_wdt_priv *priv = watchdog_get_drvdata(wdd);
	u32 counter;

	counter = wdd->timeout * priv->wdt_freq;

	iowrite32(counter, priv->iobase + HW_WDTC);

	return 0;
}

static int asm9260_wdt_enable(struct watchdog_device *wdd)
{
	struct asm9260_wdt_priv *priv = watchdog_get_drvdata(wdd);
	u32 mode = 0;

	if (priv->mode == HW_RESET)
		mode = BM_MOD_WDRESET;

	iowrite32(BM_MOD_WDEN | mode, priv->iobase + HW_WDMOD);

	asm9260_wdt_updatetimeout(wdd);

	asm9260_wdt_feed(wdd);

	return 0;
}

static int asm9260_wdt_disable(struct watchdog_device *wdd)
{
	struct asm9260_wdt_priv *priv = watchdog_get_drvdata(wdd);

	 
	reset_control_assert(priv->rst);
	reset_control_deassert(priv->rst);

	return 0;
}

static int asm9260_wdt_settimeout(struct watchdog_device *wdd, unsigned int to)
{
	wdd->timeout = to;
	asm9260_wdt_updatetimeout(wdd);

	return 0;
}

static void asm9260_wdt_sys_reset(struct asm9260_wdt_priv *priv)
{
	 

	iowrite32(BM_MOD_WDEN | BM_MOD_WDRESET, priv->iobase + HW_WDMOD);

	iowrite32(0xff, priv->iobase + HW_WDTC);
	 
	asm9260_wdt_feed(&priv->wdd);
	 
	iowrite32(0xff, priv->iobase + HW_WDFEED);

	mdelay(1000);
}

static irqreturn_t asm9260_wdt_irq(int irq, void *devid)
{
	struct asm9260_wdt_priv *priv = devid;
	u32 stat;

	stat = ioread32(priv->iobase + HW_WDMOD);
	if (!(stat & BM_MOD_WDINT))
		return IRQ_NONE;

	if (priv->mode == DEBUG) {
		dev_info(priv->dev, "Watchdog Timeout. Do nothing.\n");
	} else {
		dev_info(priv->dev, "Watchdog Timeout. Doing SW Reset.\n");
		asm9260_wdt_sys_reset(priv);
	}

	return IRQ_HANDLED;
}

static int asm9260_restart(struct watchdog_device *wdd, unsigned long action,
			   void *data)
{
	struct asm9260_wdt_priv *priv = watchdog_get_drvdata(wdd);

	asm9260_wdt_sys_reset(priv);

	return 0;
}

static const struct watchdog_info asm9260_wdt_ident = {
	.options          =     WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING
				| WDIOF_MAGICCLOSE,
	.identity         =	"Alphascale asm9260 Watchdog",
};

static const struct watchdog_ops asm9260_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= asm9260_wdt_enable,
	.stop		= asm9260_wdt_disable,
	.get_timeleft	= asm9260_wdt_gettimeleft,
	.ping		= asm9260_wdt_feed,
	.set_timeout	= asm9260_wdt_settimeout,
	.restart	= asm9260_restart,
};

static void asm9260_clk_disable_unprepare(void *data)
{
	clk_disable_unprepare(data);
}

static int asm9260_wdt_get_dt_clks(struct asm9260_wdt_priv *priv)
{
	int err;
	unsigned long clk;

	priv->clk = devm_clk_get(priv->dev, "mod");
	if (IS_ERR(priv->clk)) {
		dev_err(priv->dev, "Failed to get \"mod\" clk\n");
		return PTR_ERR(priv->clk);
	}

	 
	priv->clk_ahb = devm_clk_get(priv->dev, "ahb");
	if (IS_ERR(priv->clk_ahb)) {
		dev_err(priv->dev, "Failed to get \"ahb\" clk\n");
		return PTR_ERR(priv->clk_ahb);
	}

	err = clk_prepare_enable(priv->clk_ahb);
	if (err) {
		dev_err(priv->dev, "Failed to enable ahb_clk!\n");
		return err;
	}
	err = devm_add_action_or_reset(priv->dev,
				       asm9260_clk_disable_unprepare,
				       priv->clk_ahb);
	if (err)
		return err;

	err = clk_set_rate(priv->clk, CLOCK_FREQ);
	if (err) {
		dev_err(priv->dev, "Failed to set rate!\n");
		return err;
	}

	err = clk_prepare_enable(priv->clk);
	if (err) {
		dev_err(priv->dev, "Failed to enable clk!\n");
		return err;
	}
	err = devm_add_action_or_reset(priv->dev,
				       asm9260_clk_disable_unprepare,
				       priv->clk);
	if (err)
		return err;

	 
	clk = clk_get_rate(priv->clk);
	if (!clk) {
		dev_err(priv->dev, "Failed, clk is 0!\n");
		return -EINVAL;
	}

	priv->wdt_freq = clk / 2;

	return 0;
}

static void asm9260_wdt_get_dt_mode(struct asm9260_wdt_priv *priv)
{
	const char *tmp;
	int ret;

	 
	priv->mode = HW_RESET;

	ret = of_property_read_string(priv->dev->of_node,
				      "alphascale,mode", &tmp);
	if (ret < 0)
		return;

	if (!strcmp(tmp, "hw"))
		priv->mode = HW_RESET;
	else if (!strcmp(tmp, "sw"))
		priv->mode = SW_RESET;
	else if (!strcmp(tmp, "debug"))
		priv->mode = DEBUG;
	else
		dev_warn(priv->dev, "unknown reset-type: %s. Using default \"hw\" mode.",
			 tmp);
}

static int asm9260_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct asm9260_wdt_priv *priv;
	struct watchdog_device *wdd;
	int ret;
	static const char * const mode_name[] = { "hw", "sw", "debug", };

	priv = devm_kzalloc(dev, sizeof(struct asm9260_wdt_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;

	priv->iobase = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->iobase))
		return PTR_ERR(priv->iobase);

	priv->rst = devm_reset_control_get_exclusive(dev, "wdt_rst");
	if (IS_ERR(priv->rst))
		return PTR_ERR(priv->rst);

	ret = asm9260_wdt_get_dt_clks(priv);
	if (ret)
		return ret;

	wdd = &priv->wdd;
	wdd->info = &asm9260_wdt_ident;
	wdd->ops = &asm9260_wdt_ops;
	wdd->min_timeout = 1;
	wdd->max_timeout = BM_WDTC_MAX(priv->wdt_freq);
	wdd->parent = dev;

	watchdog_set_drvdata(wdd, priv);

	 
	wdd->timeout = ASM9260_WDT_DEFAULT_TIMEOUT;
	watchdog_init_timeout(wdd, 0, dev);

	asm9260_wdt_get_dt_mode(priv);

	if (priv->mode != HW_RESET)
		priv->irq = platform_get_irq(pdev, 0);

	if (priv->irq > 0) {
		 
		ret = devm_request_irq(dev, priv->irq, asm9260_wdt_irq, 0,
				       pdev->name, priv);
		if (ret < 0)
			dev_warn(dev, "failed to request IRQ\n");
	}

	watchdog_set_restart_priority(wdd, 128);

	watchdog_stop_on_reboot(wdd);
	watchdog_stop_on_unregister(wdd);
	ret = devm_watchdog_register_device(dev, wdd);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, priv);

	dev_info(dev, "Watchdog enabled (timeout: %d sec, mode: %s)\n",
		 wdd->timeout, mode_name[priv->mode]);
	return 0;
}

static const struct of_device_id asm9260_wdt_of_match[] = {
	{ .compatible = "alphascale,asm9260-wdt"},
	{},
};
MODULE_DEVICE_TABLE(of, asm9260_wdt_of_match);

static struct platform_driver asm9260_wdt_driver = {
	.driver = {
		.name = "asm9260-wdt",
		.of_match_table	= asm9260_wdt_of_match,
	},
	.probe = asm9260_wdt_probe,
};
module_platform_driver(asm9260_wdt_driver);

MODULE_DESCRIPTION("asm9260 WatchDog Timer Driver");
MODULE_AUTHOR("Oleksij Rempel <linux@rempel-privat.de>");
MODULE_LICENSE("GPL");
