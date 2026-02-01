
 

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/reset.h>
#include <linux/watchdog.h>

#define WDOG_CONTROL_REG_OFFSET		    0x00
#define WDOG_CONTROL_REG_WDT_EN_MASK	    0x01
#define WDOG_CONTROL_REG_RESP_MODE_MASK	    0x02
#define WDOG_TIMEOUT_RANGE_REG_OFFSET	    0x04
#define WDOG_TIMEOUT_RANGE_TOPINIT_SHIFT    4
#define WDOG_CURRENT_COUNT_REG_OFFSET	    0x08
#define WDOG_COUNTER_RESTART_REG_OFFSET     0x0c
#define WDOG_COUNTER_RESTART_KICK_VALUE	    0x76
#define WDOG_INTERRUPT_STATUS_REG_OFFSET    0x10
#define WDOG_INTERRUPT_CLEAR_REG_OFFSET     0x14
#define WDOG_COMP_PARAMS_5_REG_OFFSET       0xe4
#define WDOG_COMP_PARAMS_4_REG_OFFSET       0xe8
#define WDOG_COMP_PARAMS_3_REG_OFFSET       0xec
#define WDOG_COMP_PARAMS_2_REG_OFFSET       0xf0
#define WDOG_COMP_PARAMS_1_REG_OFFSET       0xf4
#define WDOG_COMP_PARAMS_1_USE_FIX_TOP      BIT(6)
#define WDOG_COMP_VERSION_REG_OFFSET        0xf8
#define WDOG_COMP_TYPE_REG_OFFSET           0xfc

 
#define DW_WDT_NUM_TOPS		16
#define DW_WDT_FIX_TOP(_idx)	(1U << (16 + _idx))

#define DW_WDT_DEFAULT_SECONDS	30

static const u32 dw_wdt_fix_tops[DW_WDT_NUM_TOPS] = {
	DW_WDT_FIX_TOP(0), DW_WDT_FIX_TOP(1), DW_WDT_FIX_TOP(2),
	DW_WDT_FIX_TOP(3), DW_WDT_FIX_TOP(4), DW_WDT_FIX_TOP(5),
	DW_WDT_FIX_TOP(6), DW_WDT_FIX_TOP(7), DW_WDT_FIX_TOP(8),
	DW_WDT_FIX_TOP(9), DW_WDT_FIX_TOP(10), DW_WDT_FIX_TOP(11),
	DW_WDT_FIX_TOP(12), DW_WDT_FIX_TOP(13), DW_WDT_FIX_TOP(14),
	DW_WDT_FIX_TOP(15)
};

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started "
		 "(default=" __MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

enum dw_wdt_rmod {
	DW_WDT_RMOD_RESET = 1,
	DW_WDT_RMOD_IRQ = 2
};

struct dw_wdt_timeout {
	u32 top_val;
	unsigned int sec;
	unsigned int msec;
};

struct dw_wdt {
	void __iomem		*regs;
	struct clk		*clk;
	struct clk		*pclk;
	unsigned long		rate;
	enum dw_wdt_rmod	rmod;
	struct dw_wdt_timeout	timeouts[DW_WDT_NUM_TOPS];
	struct watchdog_device	wdd;
	struct reset_control	*rst;
	 
	u32			control;
	u32			timeout;

#ifdef CONFIG_DEBUG_FS
	struct dentry		*dbgfs_dir;
#endif
};

#define to_dw_wdt(wdd)	container_of(wdd, struct dw_wdt, wdd)

static inline int dw_wdt_is_enabled(struct dw_wdt *dw_wdt)
{
	return readl(dw_wdt->regs + WDOG_CONTROL_REG_OFFSET) &
		WDOG_CONTROL_REG_WDT_EN_MASK;
}

static void dw_wdt_update_mode(struct dw_wdt *dw_wdt, enum dw_wdt_rmod rmod)
{
	u32 val;

	val = readl(dw_wdt->regs + WDOG_CONTROL_REG_OFFSET);
	if (rmod == DW_WDT_RMOD_IRQ)
		val |= WDOG_CONTROL_REG_RESP_MODE_MASK;
	else
		val &= ~WDOG_CONTROL_REG_RESP_MODE_MASK;
	writel(val, dw_wdt->regs + WDOG_CONTROL_REG_OFFSET);

	dw_wdt->rmod = rmod;
}

static unsigned int dw_wdt_find_best_top(struct dw_wdt *dw_wdt,
					 unsigned int timeout, u32 *top_val)
{
	int idx;

	 
	for (idx = 0; idx < DW_WDT_NUM_TOPS; ++idx) {
		if (dw_wdt->timeouts[idx].sec >= timeout)
			break;
	}

	if (idx == DW_WDT_NUM_TOPS)
		--idx;

	*top_val = dw_wdt->timeouts[idx].top_val;

	return dw_wdt->timeouts[idx].sec;
}

static unsigned int dw_wdt_get_min_timeout(struct dw_wdt *dw_wdt)
{
	int idx;

	 
	for (idx = 0; idx < DW_WDT_NUM_TOPS; ++idx) {
		if (dw_wdt->timeouts[idx].sec)
			break;
	}

	return dw_wdt->timeouts[idx].sec;
}

static unsigned int dw_wdt_get_max_timeout_ms(struct dw_wdt *dw_wdt)
{
	struct dw_wdt_timeout *timeout = &dw_wdt->timeouts[DW_WDT_NUM_TOPS - 1];
	u64 msec;

	msec = (u64)timeout->sec * MSEC_PER_SEC + timeout->msec;

	return msec < UINT_MAX ? msec : UINT_MAX;
}

static unsigned int dw_wdt_get_timeout(struct dw_wdt *dw_wdt)
{
	int top_val = readl(dw_wdt->regs + WDOG_TIMEOUT_RANGE_REG_OFFSET) & 0xF;
	int idx;

	for (idx = 0; idx < DW_WDT_NUM_TOPS; ++idx) {
		if (dw_wdt->timeouts[idx].top_val == top_val)
			break;
	}

	 
	return dw_wdt->timeouts[idx].sec * dw_wdt->rmod;
}

static int dw_wdt_ping(struct watchdog_device *wdd)
{
	struct dw_wdt *dw_wdt = to_dw_wdt(wdd);

	writel(WDOG_COUNTER_RESTART_KICK_VALUE, dw_wdt->regs +
	       WDOG_COUNTER_RESTART_REG_OFFSET);

	return 0;
}

static int dw_wdt_set_timeout(struct watchdog_device *wdd, unsigned int top_s)
{
	struct dw_wdt *dw_wdt = to_dw_wdt(wdd);
	unsigned int timeout;
	u32 top_val;

	 
	timeout = dw_wdt_find_best_top(dw_wdt, DIV_ROUND_UP(top_s, dw_wdt->rmod),
				       &top_val);
	if (dw_wdt->rmod == DW_WDT_RMOD_IRQ)
		wdd->pretimeout = timeout;
	else
		wdd->pretimeout = 0;

	 
	writel(top_val | top_val << WDOG_TIMEOUT_RANGE_TOPINIT_SHIFT,
	       dw_wdt->regs + WDOG_TIMEOUT_RANGE_REG_OFFSET);

	 
	if (watchdog_active(wdd))
		dw_wdt_ping(wdd);

	 
	if (top_s * 1000 <= wdd->max_hw_heartbeat_ms)
		wdd->timeout = timeout * dw_wdt->rmod;
	else
		wdd->timeout = top_s;

	return 0;
}

static int dw_wdt_set_pretimeout(struct watchdog_device *wdd, unsigned int req)
{
	struct dw_wdt *dw_wdt = to_dw_wdt(wdd);

	 
	dw_wdt_update_mode(dw_wdt, req ? DW_WDT_RMOD_IRQ : DW_WDT_RMOD_RESET);
	dw_wdt_set_timeout(wdd, wdd->timeout);

	return 0;
}

static void dw_wdt_arm_system_reset(struct dw_wdt *dw_wdt)
{
	u32 val = readl(dw_wdt->regs + WDOG_CONTROL_REG_OFFSET);

	 
	if (dw_wdt->rmod == DW_WDT_RMOD_IRQ)
		val |= WDOG_CONTROL_REG_RESP_MODE_MASK;
	else
		val &= ~WDOG_CONTROL_REG_RESP_MODE_MASK;
	 
	val |= WDOG_CONTROL_REG_WDT_EN_MASK;
	writel(val, dw_wdt->regs + WDOG_CONTROL_REG_OFFSET);
}

static int dw_wdt_start(struct watchdog_device *wdd)
{
	struct dw_wdt *dw_wdt = to_dw_wdt(wdd);

	dw_wdt_set_timeout(wdd, wdd->timeout);
	dw_wdt_ping(&dw_wdt->wdd);
	dw_wdt_arm_system_reset(dw_wdt);

	return 0;
}

static int dw_wdt_stop(struct watchdog_device *wdd)
{
	struct dw_wdt *dw_wdt = to_dw_wdt(wdd);

	if (!dw_wdt->rst) {
		set_bit(WDOG_HW_RUNNING, &wdd->status);
		return 0;
	}

	reset_control_assert(dw_wdt->rst);
	reset_control_deassert(dw_wdt->rst);

	return 0;
}

static int dw_wdt_restart(struct watchdog_device *wdd,
			  unsigned long action, void *data)
{
	struct dw_wdt *dw_wdt = to_dw_wdt(wdd);

	writel(0, dw_wdt->regs + WDOG_TIMEOUT_RANGE_REG_OFFSET);
	dw_wdt_update_mode(dw_wdt, DW_WDT_RMOD_RESET);
	if (dw_wdt_is_enabled(dw_wdt))
		writel(WDOG_COUNTER_RESTART_KICK_VALUE,
		       dw_wdt->regs + WDOG_COUNTER_RESTART_REG_OFFSET);
	else
		dw_wdt_arm_system_reset(dw_wdt);

	 
	mdelay(500);

	return 0;
}

static unsigned int dw_wdt_get_timeleft(struct watchdog_device *wdd)
{
	struct dw_wdt *dw_wdt = to_dw_wdt(wdd);
	unsigned int sec;
	u32 val;

	val = readl(dw_wdt->regs + WDOG_CURRENT_COUNT_REG_OFFSET);
	sec = val / dw_wdt->rate;

	if (dw_wdt->rmod == DW_WDT_RMOD_IRQ) {
		val = readl(dw_wdt->regs + WDOG_INTERRUPT_STATUS_REG_OFFSET);
		if (!val)
			sec += wdd->pretimeout;
	}

	return sec;
}

static const struct watchdog_info dw_wdt_ident = {
	.options	= WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT |
			  WDIOF_MAGICCLOSE,
	.identity	= "Synopsys DesignWare Watchdog",
};

static const struct watchdog_info dw_wdt_pt_ident = {
	.options	= WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT |
			  WDIOF_PRETIMEOUT | WDIOF_MAGICCLOSE,
	.identity	= "Synopsys DesignWare Watchdog",
};

static const struct watchdog_ops dw_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= dw_wdt_start,
	.stop		= dw_wdt_stop,
	.ping		= dw_wdt_ping,
	.set_timeout	= dw_wdt_set_timeout,
	.set_pretimeout	= dw_wdt_set_pretimeout,
	.get_timeleft	= dw_wdt_get_timeleft,
	.restart	= dw_wdt_restart,
};

static irqreturn_t dw_wdt_irq(int irq, void *devid)
{
	struct dw_wdt *dw_wdt = devid;
	u32 val;

	 
	val = readl(dw_wdt->regs + WDOG_INTERRUPT_STATUS_REG_OFFSET);
	if (!val)
		return IRQ_NONE;

	watchdog_notify_pretimeout(&dw_wdt->wdd);

	return IRQ_HANDLED;
}

static int dw_wdt_suspend(struct device *dev)
{
	struct dw_wdt *dw_wdt = dev_get_drvdata(dev);

	dw_wdt->control = readl(dw_wdt->regs + WDOG_CONTROL_REG_OFFSET);
	dw_wdt->timeout = readl(dw_wdt->regs + WDOG_TIMEOUT_RANGE_REG_OFFSET);

	clk_disable_unprepare(dw_wdt->pclk);
	clk_disable_unprepare(dw_wdt->clk);

	return 0;
}

static int dw_wdt_resume(struct device *dev)
{
	struct dw_wdt *dw_wdt = dev_get_drvdata(dev);
	int err = clk_prepare_enable(dw_wdt->clk);

	if (err)
		return err;

	err = clk_prepare_enable(dw_wdt->pclk);
	if (err) {
		clk_disable_unprepare(dw_wdt->clk);
		return err;
	}

	writel(dw_wdt->timeout, dw_wdt->regs + WDOG_TIMEOUT_RANGE_REG_OFFSET);
	writel(dw_wdt->control, dw_wdt->regs + WDOG_CONTROL_REG_OFFSET);

	dw_wdt_ping(&dw_wdt->wdd);

	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(dw_wdt_pm_ops, dw_wdt_suspend, dw_wdt_resume);

 

static void dw_wdt_handle_tops(struct dw_wdt *dw_wdt, const u32 *tops)
{
	struct dw_wdt_timeout tout, *dst;
	int val, tidx;
	u64 msec;

	 
	for (val = 0; val < DW_WDT_NUM_TOPS; ++val) {
		tout.top_val = val;
		tout.sec = tops[val] / dw_wdt->rate;
		msec = (u64)tops[val] * MSEC_PER_SEC;
		do_div(msec, dw_wdt->rate);
		tout.msec = msec - ((u64)tout.sec * MSEC_PER_SEC);

		 
		for (tidx = 0; tidx < val; ++tidx) {
			dst = &dw_wdt->timeouts[tidx];
			if (tout.sec > dst->sec || (tout.sec == dst->sec &&
			    tout.msec >= dst->msec))
				continue;
			else
				swap(*dst, tout);
		}

		dw_wdt->timeouts[val] = tout;
	}
}

static int dw_wdt_init_timeouts(struct dw_wdt *dw_wdt, struct device *dev)
{
	u32 data, of_tops[DW_WDT_NUM_TOPS];
	const u32 *tops;
	int ret;

	 
	data = readl(dw_wdt->regs + WDOG_COMP_PARAMS_1_REG_OFFSET);
	if (data & WDOG_COMP_PARAMS_1_USE_FIX_TOP) {
		tops = dw_wdt_fix_tops;
	} else {
		ret = of_property_read_variable_u32_array(dev_of_node(dev),
			"snps,watchdog-tops", of_tops, DW_WDT_NUM_TOPS,
			DW_WDT_NUM_TOPS);
		if (ret < 0) {
			dev_warn(dev, "No valid TOPs array specified\n");
			tops = dw_wdt_fix_tops;
		} else {
			tops = of_tops;
		}
	}

	 
	dw_wdt_handle_tops(dw_wdt, tops);
	if (!dw_wdt->timeouts[DW_WDT_NUM_TOPS - 1].sec) {
		dev_err(dev, "No any valid TOP detected\n");
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_DEBUG_FS

#define DW_WDT_DBGFS_REG(_name, _off) \
{				      \
	.name = _name,		      \
	.offset = _off		      \
}

static const struct debugfs_reg32 dw_wdt_dbgfs_regs[] = {
	DW_WDT_DBGFS_REG("cr", WDOG_CONTROL_REG_OFFSET),
	DW_WDT_DBGFS_REG("torr", WDOG_TIMEOUT_RANGE_REG_OFFSET),
	DW_WDT_DBGFS_REG("ccvr", WDOG_CURRENT_COUNT_REG_OFFSET),
	DW_WDT_DBGFS_REG("crr", WDOG_COUNTER_RESTART_REG_OFFSET),
	DW_WDT_DBGFS_REG("stat", WDOG_INTERRUPT_STATUS_REG_OFFSET),
	DW_WDT_DBGFS_REG("param5", WDOG_COMP_PARAMS_5_REG_OFFSET),
	DW_WDT_DBGFS_REG("param4", WDOG_COMP_PARAMS_4_REG_OFFSET),
	DW_WDT_DBGFS_REG("param3", WDOG_COMP_PARAMS_3_REG_OFFSET),
	DW_WDT_DBGFS_REG("param2", WDOG_COMP_PARAMS_2_REG_OFFSET),
	DW_WDT_DBGFS_REG("param1", WDOG_COMP_PARAMS_1_REG_OFFSET),
	DW_WDT_DBGFS_REG("version", WDOG_COMP_VERSION_REG_OFFSET),
	DW_WDT_DBGFS_REG("type", WDOG_COMP_TYPE_REG_OFFSET)
};

static void dw_wdt_dbgfs_init(struct dw_wdt *dw_wdt)
{
	struct device *dev = dw_wdt->wdd.parent;
	struct debugfs_regset32 *regset;

	regset = devm_kzalloc(dev, sizeof(*regset), GFP_KERNEL);
	if (!regset)
		return;

	regset->regs = dw_wdt_dbgfs_regs;
	regset->nregs = ARRAY_SIZE(dw_wdt_dbgfs_regs);
	regset->base = dw_wdt->regs;

	dw_wdt->dbgfs_dir = debugfs_create_dir(dev_name(dev), NULL);

	debugfs_create_regset32("registers", 0444, dw_wdt->dbgfs_dir, regset);
}

static void dw_wdt_dbgfs_clear(struct dw_wdt *dw_wdt)
{
	debugfs_remove_recursive(dw_wdt->dbgfs_dir);
}

#else  

static void dw_wdt_dbgfs_init(struct dw_wdt *dw_wdt) {}
static void dw_wdt_dbgfs_clear(struct dw_wdt *dw_wdt) {}

#endif  

static int dw_wdt_drv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct watchdog_device *wdd;
	struct dw_wdt *dw_wdt;
	int ret;

	dw_wdt = devm_kzalloc(dev, sizeof(*dw_wdt), GFP_KERNEL);
	if (!dw_wdt)
		return -ENOMEM;

	dw_wdt->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dw_wdt->regs))
		return PTR_ERR(dw_wdt->regs);

	 
	dw_wdt->clk = devm_clk_get_enabled(dev, "tclk");
	if (IS_ERR(dw_wdt->clk)) {
		dw_wdt->clk = devm_clk_get_enabled(dev, NULL);
		if (IS_ERR(dw_wdt->clk))
			return PTR_ERR(dw_wdt->clk);
	}

	dw_wdt->rate = clk_get_rate(dw_wdt->clk);
	if (dw_wdt->rate == 0)
		return -EINVAL;

	 
	dw_wdt->pclk = devm_clk_get_optional_enabled(dev, "pclk");
	if (IS_ERR(dw_wdt->pclk))
		return PTR_ERR(dw_wdt->pclk);

	dw_wdt->rst = devm_reset_control_get_optional_shared(&pdev->dev, NULL);
	if (IS_ERR(dw_wdt->rst))
		return PTR_ERR(dw_wdt->rst);

	 
	dw_wdt_update_mode(dw_wdt, DW_WDT_RMOD_RESET);

	 
	ret = platform_get_irq_optional(pdev, 0);
	if (ret > 0) {
		ret = devm_request_irq(dev, ret, dw_wdt_irq,
				       IRQF_SHARED | IRQF_TRIGGER_RISING,
				       pdev->name, dw_wdt);
		if (ret)
			return ret;

		dw_wdt->wdd.info = &dw_wdt_pt_ident;
	} else {
		if (ret == -EPROBE_DEFER)
			return ret;

		dw_wdt->wdd.info = &dw_wdt_ident;
	}

	reset_control_deassert(dw_wdt->rst);

	ret = dw_wdt_init_timeouts(dw_wdt, dev);
	if (ret)
		goto out_assert_rst;

	wdd = &dw_wdt->wdd;
	wdd->ops = &dw_wdt_ops;
	wdd->min_timeout = dw_wdt_get_min_timeout(dw_wdt);
	wdd->max_hw_heartbeat_ms = dw_wdt_get_max_timeout_ms(dw_wdt);
	wdd->parent = dev;

	watchdog_set_drvdata(wdd, dw_wdt);
	watchdog_set_nowayout(wdd, nowayout);
	watchdog_init_timeout(wdd, 0, dev);

	 
	if (dw_wdt_is_enabled(dw_wdt)) {
		wdd->timeout = dw_wdt_get_timeout(dw_wdt);
		set_bit(WDOG_HW_RUNNING, &wdd->status);
	} else {
		wdd->timeout = DW_WDT_DEFAULT_SECONDS;
		watchdog_init_timeout(wdd, 0, dev);
	}

	platform_set_drvdata(pdev, dw_wdt);

	watchdog_set_restart_priority(wdd, 128);
	watchdog_stop_on_reboot(wdd);

	ret = watchdog_register_device(wdd);
	if (ret)
		goto out_assert_rst;

	dw_wdt_dbgfs_init(dw_wdt);

	return 0;

out_assert_rst:
	reset_control_assert(dw_wdt->rst);
	return ret;
}

static void dw_wdt_drv_remove(struct platform_device *pdev)
{
	struct dw_wdt *dw_wdt = platform_get_drvdata(pdev);

	dw_wdt_dbgfs_clear(dw_wdt);

	watchdog_unregister_device(&dw_wdt->wdd);
	reset_control_assert(dw_wdt->rst);
}

#ifdef CONFIG_OF
static const struct of_device_id dw_wdt_of_match[] = {
	{ .compatible = "snps,dw-wdt", },
	{   }
};
MODULE_DEVICE_TABLE(of, dw_wdt_of_match);
#endif

static struct platform_driver dw_wdt_driver = {
	.probe		= dw_wdt_drv_probe,
	.remove_new	= dw_wdt_drv_remove,
	.driver		= {
		.name	= "dw_wdt",
		.of_match_table = of_match_ptr(dw_wdt_of_match),
		.pm	= pm_sleep_ptr(&dw_wdt_pm_ops),
	},
};

module_platform_driver(dw_wdt_driver);

MODULE_AUTHOR("Jamie Iles");
MODULE_DESCRIPTION("Synopsys DesignWare Watchdog Driver");
MODULE_LICENSE("GPL");
