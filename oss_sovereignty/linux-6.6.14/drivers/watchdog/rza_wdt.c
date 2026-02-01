
 

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>

#define DEFAULT_TIMEOUT		30

 
#define WTCSR			0
#define WTCSR_MAGIC		0xA500
#define WTSCR_WT		BIT(6)
#define WTSCR_TME		BIT(5)
#define WTSCR_CKS(i)		(i)

#define WTCNT			2
#define WTCNT_MAGIC		0x5A00

#define WRCSR			4
#define WRCSR_MAGIC		0x5A00
#define WRCSR_RSTE		BIT(6)
#define WRCSR_CLEAR_WOVF	0xA500	 

 
#define CKS_3BIT		0x7
#define CKS_4BIT		0xF

#define DIVIDER_3BIT		16384	 
#define DIVIDER_4BIT		4194304	 

struct rza_wdt {
	struct watchdog_device wdev;
	void __iomem *base;
	struct clk *clk;
	u8 count;
	u8 cks;
};

static void rza_wdt_calc_timeout(struct rza_wdt *priv, int timeout)
{
	unsigned long rate = clk_get_rate(priv->clk);
	unsigned int ticks;

	if (priv->cks == CKS_4BIT) {
		ticks = DIV_ROUND_UP(timeout * rate, DIVIDER_4BIT);

		 
		priv->count = 256 - ticks;

	} else {
		 
		priv->count = 0;
	}

	pr_debug("%s: timeout set to %u (WTCNT=%d)\n", __func__,
		 timeout, priv->count);
}

static int rza_wdt_start(struct watchdog_device *wdev)
{
	struct rza_wdt *priv = watchdog_get_drvdata(wdev);

	 
	writew(WTCSR_MAGIC | 0, priv->base + WTCSR);

	 
	readb(priv->base + WRCSR);
	writew(WRCSR_CLEAR_WOVF, priv->base + WRCSR);

	rza_wdt_calc_timeout(priv, wdev->timeout);

	writew(WRCSR_MAGIC | WRCSR_RSTE, priv->base + WRCSR);
	writew(WTCNT_MAGIC | priv->count, priv->base + WTCNT);
	writew(WTCSR_MAGIC | WTSCR_WT | WTSCR_TME |
	       WTSCR_CKS(priv->cks), priv->base + WTCSR);

	return 0;
}

static int rza_wdt_stop(struct watchdog_device *wdev)
{
	struct rza_wdt *priv = watchdog_get_drvdata(wdev);

	writew(WTCSR_MAGIC | 0, priv->base + WTCSR);

	return 0;
}

static int rza_wdt_ping(struct watchdog_device *wdev)
{
	struct rza_wdt *priv = watchdog_get_drvdata(wdev);

	writew(WTCNT_MAGIC | priv->count, priv->base + WTCNT);

	pr_debug("%s: timeout = %u\n", __func__, wdev->timeout);

	return 0;
}

static int rza_set_timeout(struct watchdog_device *wdev, unsigned int timeout)
{
	wdev->timeout = timeout;
	rza_wdt_start(wdev);
	return 0;
}

static int rza_wdt_restart(struct watchdog_device *wdev, unsigned long action,
			    void *data)
{
	struct rza_wdt *priv = watchdog_get_drvdata(wdev);

	 
	writew(WTCSR_MAGIC | 0, priv->base + WTCSR);

	 
	readb(priv->base + WRCSR);
	writew(WRCSR_CLEAR_WOVF, priv->base + WRCSR);

	 
	writew(WRCSR_MAGIC | WRCSR_RSTE, priv->base + WRCSR);
	writew(WTCNT_MAGIC | 255, priv->base + WTCNT);
	writew(WTCSR_MAGIC | WTSCR_WT | WTSCR_TME, priv->base + WTCSR);

	 
	wmb();

	 
	udelay(20);

	return 0;
}

static const struct watchdog_info rza_wdt_ident = {
	.options = WDIOF_MAGICCLOSE | WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT,
	.identity = "Renesas RZ/A WDT Watchdog",
};

static const struct watchdog_ops rza_wdt_ops = {
	.owner = THIS_MODULE,
	.start = rza_wdt_start,
	.stop = rza_wdt_stop,
	.ping = rza_wdt_ping,
	.set_timeout = rza_set_timeout,
	.restart = rza_wdt_restart,
};

static int rza_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rza_wdt *priv;
	unsigned long rate;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	priv->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(priv->clk))
		return PTR_ERR(priv->clk);

	rate = clk_get_rate(priv->clk);
	if (rate < 16384) {
		dev_err(dev, "invalid clock rate (%ld)\n", rate);
		return -ENOENT;
	}

	priv->wdev.info = &rza_wdt_ident;
	priv->wdev.ops = &rza_wdt_ops;
	priv->wdev.parent = dev;

	priv->cks = (u8)(uintptr_t) of_device_get_match_data(dev);
	if (priv->cks == CKS_4BIT) {
		 
		priv->wdev.max_timeout = (DIVIDER_4BIT * U8_MAX) / rate;

	} else if (priv->cks == CKS_3BIT) {
		 
		rate /= DIVIDER_3BIT;

		 
		priv->wdev.max_hw_heartbeat_ms = (1000 * U8_MAX) / rate;
		dev_dbg(dev, "max hw timeout of %dms\n",
			priv->wdev.max_hw_heartbeat_ms);
	}

	priv->wdev.min_timeout = 1;
	priv->wdev.timeout = DEFAULT_TIMEOUT;

	watchdog_init_timeout(&priv->wdev, 0, dev);
	watchdog_set_drvdata(&priv->wdev, priv);

	ret = devm_watchdog_register_device(dev, &priv->wdev);
	if (ret)
		dev_err(dev, "Cannot register watchdog device\n");

	return ret;
}

static const struct of_device_id rza_wdt_of_match[] = {
	{ .compatible = "renesas,r7s9210-wdt",	.data = (void *)CKS_4BIT, },
	{ .compatible = "renesas,rza-wdt",	.data = (void *)CKS_3BIT, },
	{   }
};
MODULE_DEVICE_TABLE(of, rza_wdt_of_match);

static struct platform_driver rza_wdt_driver = {
	.probe = rza_wdt_probe,
	.driver = {
		.name = "rza_wdt",
		.of_match_table = rza_wdt_of_match,
	},
};

module_platform_driver(rza_wdt_driver);

MODULE_DESCRIPTION("Renesas RZ/A WDT Driver");
MODULE_AUTHOR("Chris Brandt <chris.brandt@renesas.com>");
MODULE_LICENSE("GPL v2");
