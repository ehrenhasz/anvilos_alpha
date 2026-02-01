
 

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/time.h>

 

#define AT91_RTT_MR		0x00		 
#define AT91_RTT_RTPRES		(0xffff << 0)	 
#define AT91_RTT_ALMIEN		BIT(16)		 
#define AT91_RTT_RTTINCIEN	BIT(17)		 
#define AT91_RTT_RTTRST		BIT(18)		 

#define AT91_RTT_AR		0x04		 
#define AT91_RTT_ALMV		(0xffffffff)	 

#define AT91_RTT_VR		0x08		 
#define AT91_RTT_CRTV		(0xffffffff)	 

#define AT91_RTT_SR		0x0c		 
#define AT91_RTT_ALMS		BIT(0)		 
#define AT91_RTT_RTTINC		BIT(1)		 

 
#define ALARM_DISABLED	((u32)~0)

struct sam9_rtc {
	void __iomem		*rtt;
	struct rtc_device	*rtcdev;
	u32			imr;
	struct regmap		*gpbr;
	unsigned int		gpbr_offset;
	int			irq;
	struct clk		*sclk;
	bool			suspended;
	unsigned long		events;
	spinlock_t		lock;
};

#define rtt_readl(rtc, field) \
	readl((rtc)->rtt + AT91_RTT_ ## field)
#define rtt_writel(rtc, field, val) \
	writel((val), (rtc)->rtt + AT91_RTT_ ## field)

static inline unsigned int gpbr_readl(struct sam9_rtc *rtc)
{
	unsigned int val;

	regmap_read(rtc->gpbr, rtc->gpbr_offset, &val);

	return val;
}

static inline void gpbr_writel(struct sam9_rtc *rtc, unsigned int val)
{
	regmap_write(rtc->gpbr, rtc->gpbr_offset, val);
}

 
static int at91_rtc_readtime(struct device *dev, struct rtc_time *tm)
{
	struct sam9_rtc *rtc = dev_get_drvdata(dev);
	u32 secs, secs2;
	u32 offset;

	 
	offset = gpbr_readl(rtc);
	if (offset == 0)
		return -EILSEQ;

	 
	secs = rtt_readl(rtc, VR);
	secs2 = rtt_readl(rtc, VR);
	if (secs != secs2)
		secs = rtt_readl(rtc, VR);

	rtc_time64_to_tm(offset + secs, tm);

	dev_dbg(dev, "%s: %ptR\n", __func__, tm);

	return 0;
}

 
static int at91_rtc_settime(struct device *dev, struct rtc_time *tm)
{
	struct sam9_rtc *rtc = dev_get_drvdata(dev);
	u32 offset, alarm, mr;
	unsigned long secs;

	dev_dbg(dev, "%s: %ptR\n", __func__, tm);

	secs = rtc_tm_to_time64(tm);

	mr = rtt_readl(rtc, MR);

	 
	rtt_writel(rtc, MR, mr & ~(AT91_RTT_ALMIEN | AT91_RTT_RTTINCIEN));

	 
	offset = gpbr_readl(rtc);

	 
	secs += 1;
	gpbr_writel(rtc, secs);

	 
	alarm = rtt_readl(rtc, AR);
	if (alarm != ALARM_DISABLED) {
		if (offset > secs) {
			 
			alarm += (offset - secs);
		} else if ((alarm + offset) > secs) {
			 
			alarm -= (secs - offset);
		} else {
			 
			alarm = ALARM_DISABLED;
			mr &= ~AT91_RTT_ALMIEN;
		}
		rtt_writel(rtc, AR, alarm);
	}

	 
	rtt_writel(rtc, MR, mr | AT91_RTT_RTTRST);

	return 0;
}

static int at91_rtc_readalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct sam9_rtc *rtc = dev_get_drvdata(dev);
	struct rtc_time *tm = &alrm->time;
	u32 alarm = rtt_readl(rtc, AR);
	u32 offset;

	offset = gpbr_readl(rtc);
	if (offset == 0)
		return -EILSEQ;

	memset(alrm, 0, sizeof(*alrm));
	if (alarm != ALARM_DISABLED) {
		rtc_time64_to_tm(offset + alarm, tm);

		dev_dbg(dev, "%s: %ptR\n", __func__, tm);

		if (rtt_readl(rtc, MR) & AT91_RTT_ALMIEN)
			alrm->enabled = 1;
	}

	return 0;
}

static int at91_rtc_setalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct sam9_rtc *rtc = dev_get_drvdata(dev);
	struct rtc_time *tm = &alrm->time;
	unsigned long secs;
	u32 offset;
	u32 mr;

	secs = rtc_tm_to_time64(tm);

	offset = gpbr_readl(rtc);
	if (offset == 0) {
		 
		return -EILSEQ;
	}
	mr = rtt_readl(rtc, MR);
	rtt_writel(rtc, MR, mr & ~AT91_RTT_ALMIEN);

	 
	if (secs <= offset) {
		rtt_writel(rtc, AR, ALARM_DISABLED);
		return 0;
	}

	 
	rtt_writel(rtc, AR, secs - offset);
	if (alrm->enabled)
		rtt_writel(rtc, MR, mr | AT91_RTT_ALMIEN);

	dev_dbg(dev, "%s: %ptR\n", __func__, tm);

	return 0;
}

static int at91_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct sam9_rtc *rtc = dev_get_drvdata(dev);
	u32 mr = rtt_readl(rtc, MR);

	dev_dbg(dev, "alarm_irq_enable: enabled=%08x, mr %08x\n", enabled, mr);
	if (enabled)
		rtt_writel(rtc, MR, mr | AT91_RTT_ALMIEN);
	else
		rtt_writel(rtc, MR, mr & ~AT91_RTT_ALMIEN);
	return 0;
}

 
static int at91_rtc_proc(struct device *dev, struct seq_file *seq)
{
	struct sam9_rtc *rtc = dev_get_drvdata(dev);
	u32 mr = rtt_readl(rtc, MR);

	seq_printf(seq, "update_IRQ\t: %s\n",
		   (mr & AT91_RTT_RTTINCIEN) ? "yes" : "no");
	return 0;
}

static irqreturn_t at91_rtc_cache_events(struct sam9_rtc *rtc)
{
	u32 sr, mr;

	 
	mr = rtt_readl(rtc, MR) & (AT91_RTT_ALMIEN | AT91_RTT_RTTINCIEN);
	sr = rtt_readl(rtc, SR) & (mr >> 16);
	if (!sr)
		return IRQ_NONE;

	 
	if (sr & AT91_RTT_ALMS)
		rtc->events |= (RTC_AF | RTC_IRQF);

	 
	if (sr & AT91_RTT_RTTINC)
		rtc->events |= (RTC_UF | RTC_IRQF);

	return IRQ_HANDLED;
}

static void at91_rtc_flush_events(struct sam9_rtc *rtc)
{
	if (!rtc->events)
		return;

	rtc_update_irq(rtc->rtcdev, 1, rtc->events);
	rtc->events = 0;

	pr_debug("%s: num=%ld, events=0x%02lx\n", __func__,
		 rtc->events >> 8, rtc->events & 0x000000FF);
}

 
static irqreturn_t at91_rtc_interrupt(int irq, void *_rtc)
{
	struct sam9_rtc *rtc = _rtc;
	int ret;

	spin_lock(&rtc->lock);

	ret = at91_rtc_cache_events(rtc);

	 
	if (rtc->suspended) {
		 
		rtt_writel(rtc, MR,
			   rtt_readl(rtc, MR) &
			   ~(AT91_RTT_ALMIEN | AT91_RTT_RTTINCIEN));
		 
		pm_system_wakeup();
	} else {
		at91_rtc_flush_events(rtc);
	}

	spin_unlock(&rtc->lock);

	return ret;
}

static const struct rtc_class_ops at91_rtc_ops = {
	.read_time	= at91_rtc_readtime,
	.set_time	= at91_rtc_settime,
	.read_alarm	= at91_rtc_readalarm,
	.set_alarm	= at91_rtc_setalarm,
	.proc		= at91_rtc_proc,
	.alarm_irq_enable = at91_rtc_alarm_irq_enable,
};

 
static int at91_rtc_probe(struct platform_device *pdev)
{
	struct sam9_rtc	*rtc;
	int		ret, irq;
	u32		mr;
	unsigned int	sclk_rate;
	struct of_phandle_args args;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	spin_lock_init(&rtc->lock);
	rtc->irq = irq;

	 
	if (!device_can_wakeup(&pdev->dev))
		device_init_wakeup(&pdev->dev, 1);

	platform_set_drvdata(pdev, rtc);

	rtc->rtt = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(rtc->rtt))
		return PTR_ERR(rtc->rtt);

	ret = of_parse_phandle_with_fixed_args(pdev->dev.of_node,
					       "atmel,rtt-rtc-time-reg", 1, 0,
					       &args);
	if (ret)
		return ret;

	rtc->gpbr = syscon_node_to_regmap(args.np);
	rtc->gpbr_offset = args.args[0];
	if (IS_ERR(rtc->gpbr)) {
		dev_err(&pdev->dev, "failed to retrieve gpbr regmap, aborting.\n");
		return -ENOMEM;
	}

	rtc->sclk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(rtc->sclk))
		return PTR_ERR(rtc->sclk);

	ret = clk_prepare_enable(rtc->sclk);
	if (ret) {
		dev_err(&pdev->dev, "Could not enable slow clock\n");
		return ret;
	}

	sclk_rate = clk_get_rate(rtc->sclk);
	if (!sclk_rate || sclk_rate > AT91_RTT_RTPRES) {
		dev_err(&pdev->dev, "Invalid slow clock rate\n");
		ret = -EINVAL;
		goto err_clk;
	}

	mr = rtt_readl(rtc, MR);

	 
	if ((mr & AT91_RTT_RTPRES) != sclk_rate) {
		mr = AT91_RTT_RTTRST | (sclk_rate & AT91_RTT_RTPRES);
		gpbr_writel(rtc, 0);
	}

	 
	mr &= ~(AT91_RTT_ALMIEN | AT91_RTT_RTTINCIEN);
	rtt_writel(rtc, MR, mr);

	rtc->rtcdev = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(rtc->rtcdev)) {
		ret = PTR_ERR(rtc->rtcdev);
		goto err_clk;
	}

	rtc->rtcdev->ops = &at91_rtc_ops;
	rtc->rtcdev->range_max = U32_MAX;

	 
	ret = devm_request_irq(&pdev->dev, rtc->irq, at91_rtc_interrupt,
			       IRQF_SHARED | IRQF_COND_SUSPEND,
			       dev_name(&rtc->rtcdev->dev), rtc);
	if (ret) {
		dev_dbg(&pdev->dev, "can't share IRQ %d?\n", rtc->irq);
		goto err_clk;
	}

	 

	if (gpbr_readl(rtc) == 0)
		dev_warn(&pdev->dev, "%s: SET TIME!\n",
			 dev_name(&rtc->rtcdev->dev));

	return devm_rtc_register_device(rtc->rtcdev);

err_clk:
	clk_disable_unprepare(rtc->sclk);

	return ret;
}

 
static void at91_rtc_remove(struct platform_device *pdev)
{
	struct sam9_rtc	*rtc = platform_get_drvdata(pdev);
	u32		mr = rtt_readl(rtc, MR);

	 
	rtt_writel(rtc, MR, mr & ~(AT91_RTT_ALMIEN | AT91_RTT_RTTINCIEN));

	clk_disable_unprepare(rtc->sclk);
}

static void at91_rtc_shutdown(struct platform_device *pdev)
{
	struct sam9_rtc	*rtc = platform_get_drvdata(pdev);
	u32		mr = rtt_readl(rtc, MR);

	rtc->imr = mr & (AT91_RTT_ALMIEN | AT91_RTT_RTTINCIEN);
	rtt_writel(rtc, MR, mr & ~rtc->imr);
}

#ifdef CONFIG_PM_SLEEP

 

static int at91_rtc_suspend(struct device *dev)
{
	struct sam9_rtc	*rtc = dev_get_drvdata(dev);
	u32		mr = rtt_readl(rtc, MR);

	 
	rtc->imr = mr & (AT91_RTT_ALMIEN | AT91_RTT_RTTINCIEN);
	if (rtc->imr) {
		if (device_may_wakeup(dev) && (mr & AT91_RTT_ALMIEN)) {
			unsigned long flags;

			enable_irq_wake(rtc->irq);
			spin_lock_irqsave(&rtc->lock, flags);
			rtc->suspended = true;
			spin_unlock_irqrestore(&rtc->lock, flags);
			 
			if (mr & AT91_RTT_RTTINCIEN)
				rtt_writel(rtc, MR, mr & ~AT91_RTT_RTTINCIEN);
		} else {
			rtt_writel(rtc, MR, mr & ~rtc->imr);
		}
	}

	return 0;
}

static int at91_rtc_resume(struct device *dev)
{
	struct sam9_rtc	*rtc = dev_get_drvdata(dev);
	u32		mr;

	if (rtc->imr) {
		unsigned long flags;

		if (device_may_wakeup(dev))
			disable_irq_wake(rtc->irq);
		mr = rtt_readl(rtc, MR);
		rtt_writel(rtc, MR, mr | rtc->imr);

		spin_lock_irqsave(&rtc->lock, flags);
		rtc->suspended = false;
		at91_rtc_cache_events(rtc);
		at91_rtc_flush_events(rtc);
		spin_unlock_irqrestore(&rtc->lock, flags);
	}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(at91_rtc_pm_ops, at91_rtc_suspend, at91_rtc_resume);

static const struct of_device_id at91_rtc_dt_ids[] = {
	{ .compatible = "atmel,at91sam9260-rtt" },
	{   }
};
MODULE_DEVICE_TABLE(of, at91_rtc_dt_ids);

static struct platform_driver at91_rtc_driver = {
	.probe		= at91_rtc_probe,
	.remove_new	= at91_rtc_remove,
	.shutdown	= at91_rtc_shutdown,
	.driver		= {
		.name	= "rtc-at91sam9",
		.pm	= &at91_rtc_pm_ops,
		.of_match_table = at91_rtc_dt_ids,
	},
};

module_platform_driver(at91_rtc_driver);

MODULE_AUTHOR("Michel Benoit");
MODULE_DESCRIPTION("RTC driver for Atmel AT91SAM9x");
MODULE_LICENSE("GPL");
