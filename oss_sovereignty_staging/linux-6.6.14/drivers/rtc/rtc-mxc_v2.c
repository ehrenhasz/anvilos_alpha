
 

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeirq.h>
#include <linux/rtc.h>

#define SRTC_LPPDR_INIT       0x41736166	 

#define SRTC_LPCR_EN_LP       BIT(3)	 
#define SRTC_LPCR_WAE         BIT(4)	 
#define SRTC_LPCR_ALP         BIT(7)	 
#define SRTC_LPCR_NSA         BIT(11)	 
#define SRTC_LPCR_NVE         BIT(14)	 
#define SRTC_LPCR_IE          BIT(15)	 

#define SRTC_LPSR_ALP         BIT(3)	 
#define SRTC_LPSR_NVES        BIT(14)	 
#define SRTC_LPSR_IES         BIT(15)	 

#define SRTC_LPSCMR	0x00	 
#define SRTC_LPSCLR	0x04	 
#define SRTC_LPSAR	0x08	 
#define SRTC_LPCR	0x10	 
#define SRTC_LPSR	0x14	 
#define SRTC_LPPDR	0x18	 

 
#define REG_READ_TIMEOUT 2000

struct mxc_rtc_data {
	struct rtc_device *rtc;
	void __iomem *ioaddr;
	struct clk *clk;
	spinlock_t lock;  
	int irq;
};

 
static void mxc_rtc_sync_lp_locked(struct device *dev, void __iomem *ioaddr)
{
	unsigned int i;

	 
	for (i = 0; i < 3; i++) {
		const u32 count = readl(ioaddr + SRTC_LPSCLR);
		unsigned int timeout = REG_READ_TIMEOUT;

		while ((readl(ioaddr + SRTC_LPSCLR)) == count) {
			if (!--timeout) {
				dev_err_once(dev, "SRTC_LPSCLR stuck! Check your hw.\n");
				return;
			}
		}
	}
}

 
static irqreturn_t mxc_rtc_interrupt(int irq, void *dev_id)
{
	struct device *dev = dev_id;
	struct mxc_rtc_data *pdata = dev_get_drvdata(dev);
	void __iomem *ioaddr = pdata->ioaddr;
	u32 lp_status;
	u32 lp_cr;

	spin_lock(&pdata->lock);
	if (clk_enable(pdata->clk)) {
		spin_unlock(&pdata->lock);
		return IRQ_NONE;
	}

	lp_status = readl(ioaddr + SRTC_LPSR);
	lp_cr = readl(ioaddr + SRTC_LPCR);

	 
	if (lp_status & SRTC_LPSR_ALP) {
		if (lp_cr & SRTC_LPCR_ALP)
			rtc_update_irq(pdata->rtc, 1, RTC_AF | RTC_IRQF);

		 
		lp_cr &= ~(SRTC_LPCR_ALP | SRTC_LPCR_WAE);
	}

	 
	writel(lp_cr, ioaddr + SRTC_LPCR);

	 
	writel(lp_status, ioaddr + SRTC_LPSR);

	mxc_rtc_sync_lp_locked(dev, ioaddr);
	clk_disable(pdata->clk);
	spin_unlock(&pdata->lock);
	return IRQ_HANDLED;
}

 
static int mxc_rtc_lock(struct mxc_rtc_data *const pdata)
{
	int ret;

	spin_lock_irq(&pdata->lock);
	ret = clk_enable(pdata->clk);
	if (ret) {
		spin_unlock_irq(&pdata->lock);
		return ret;
	}
	return 0;
}

static int mxc_rtc_unlock(struct mxc_rtc_data *const pdata)
{
	clk_disable(pdata->clk);
	spin_unlock_irq(&pdata->lock);
	return 0;
}

 
static int mxc_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct mxc_rtc_data *pdata = dev_get_drvdata(dev);
	const int clk_failed = clk_enable(pdata->clk);

	if (!clk_failed) {
		const time64_t now = readl(pdata->ioaddr + SRTC_LPSCMR);

		rtc_time64_to_tm(now, tm);
		clk_disable(pdata->clk);
		return 0;
	}
	return clk_failed;
}

 
static int mxc_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct mxc_rtc_data *pdata = dev_get_drvdata(dev);
	time64_t time = rtc_tm_to_time64(tm);
	int ret;

	ret = mxc_rtc_lock(pdata);
	if (ret)
		return ret;

	writel(time, pdata->ioaddr + SRTC_LPSCMR);
	mxc_rtc_sync_lp_locked(dev, pdata->ioaddr);
	return mxc_rtc_unlock(pdata);
}

 
static int mxc_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct mxc_rtc_data *pdata = dev_get_drvdata(dev);
	void __iomem *ioaddr = pdata->ioaddr;
	int ret;

	ret = mxc_rtc_lock(pdata);
	if (ret)
		return ret;

	rtc_time64_to_tm(readl(ioaddr + SRTC_LPSAR), &alrm->time);
	alrm->pending = !!(readl(ioaddr + SRTC_LPSR) & SRTC_LPSR_ALP);
	return mxc_rtc_unlock(pdata);
}

 
static void mxc_rtc_alarm_irq_enable_locked(struct mxc_rtc_data *pdata,
					    unsigned int enable)
{
	u32 lp_cr = readl(pdata->ioaddr + SRTC_LPCR);

	if (enable)
		lp_cr |= (SRTC_LPCR_ALP | SRTC_LPCR_WAE);
	else
		lp_cr &= ~(SRTC_LPCR_ALP | SRTC_LPCR_WAE);

	writel(lp_cr, pdata->ioaddr + SRTC_LPCR);
}

static int mxc_rtc_alarm_irq_enable(struct device *dev, unsigned int enable)
{
	struct mxc_rtc_data *pdata = dev_get_drvdata(dev);
	int ret = mxc_rtc_lock(pdata);

	if (ret)
		return ret;

	mxc_rtc_alarm_irq_enable_locked(pdata, enable);
	return mxc_rtc_unlock(pdata);
}

 
static int mxc_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	const time64_t time = rtc_tm_to_time64(&alrm->time);
	struct mxc_rtc_data *pdata = dev_get_drvdata(dev);
	int ret = mxc_rtc_lock(pdata);

	if (ret)
		return ret;

	writel((u32)time, pdata->ioaddr + SRTC_LPSAR);

	 
	writel(SRTC_LPSR_ALP, pdata->ioaddr + SRTC_LPSR);
	mxc_rtc_sync_lp_locked(dev, pdata->ioaddr);

	mxc_rtc_alarm_irq_enable_locked(pdata, alrm->enabled);
	mxc_rtc_sync_lp_locked(dev, pdata->ioaddr);
	mxc_rtc_unlock(pdata);
	return ret;
}

static const struct rtc_class_ops mxc_rtc_ops = {
	.read_time = mxc_rtc_read_time,
	.set_time = mxc_rtc_set_time,
	.read_alarm = mxc_rtc_read_alarm,
	.set_alarm = mxc_rtc_set_alarm,
	.alarm_irq_enable = mxc_rtc_alarm_irq_enable,
};

static int mxc_rtc_wait_for_flag(void __iomem *ioaddr, int flag)
{
	unsigned int timeout = REG_READ_TIMEOUT;

	while (!(readl(ioaddr) & flag)) {
		if (!--timeout)
			return -EBUSY;
	}
	return 0;
}

static int mxc_rtc_probe(struct platform_device *pdev)
{
	struct mxc_rtc_data *pdata;
	void __iomem *ioaddr;
	int ret = 0;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->ioaddr = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(pdata->ioaddr))
		return PTR_ERR(pdata->ioaddr);

	ioaddr = pdata->ioaddr;

	pdata->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(pdata->clk)) {
		dev_err(&pdev->dev, "unable to get rtc clock!\n");
		return PTR_ERR(pdata->clk);
	}

	spin_lock_init(&pdata->lock);
	pdata->irq = platform_get_irq(pdev, 0);
	if (pdata->irq < 0)
		return pdata->irq;

	device_init_wakeup(&pdev->dev, 1);
	ret = dev_pm_set_wake_irq(&pdev->dev, pdata->irq);
	if (ret)
		dev_err(&pdev->dev, "failed to enable irq wake\n");

	ret = clk_prepare_enable(pdata->clk);
	if (ret)
		return ret;
	 
	writel(SRTC_LPPDR_INIT, ioaddr + SRTC_LPPDR);

	 
	writel(0xFFFFFFFF, ioaddr + SRTC_LPSR);

	 
	writel((SRTC_LPCR_IE | SRTC_LPCR_NSA), ioaddr + SRTC_LPCR);
	ret = mxc_rtc_wait_for_flag(ioaddr + SRTC_LPSR, SRTC_LPSR_IES);
	if (ret) {
		dev_err(&pdev->dev, "Timeout waiting for SRTC_LPSR_IES\n");
		clk_disable_unprepare(pdata->clk);
		return ret;
	}

	 
	writel((SRTC_LPCR_IE | SRTC_LPCR_NVE | SRTC_LPCR_NSA |
		SRTC_LPCR_EN_LP), ioaddr + SRTC_LPCR);
	ret = mxc_rtc_wait_for_flag(ioaddr + SRTC_LPSR, SRTC_LPSR_NVES);
	if (ret) {
		dev_err(&pdev->dev, "Timeout waiting for SRTC_LPSR_NVES\n");
		clk_disable_unprepare(pdata->clk);
		return ret;
	}

	pdata->rtc = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(pdata->rtc)) {
		clk_disable_unprepare(pdata->clk);
		return PTR_ERR(pdata->rtc);
	}

	pdata->rtc->ops = &mxc_rtc_ops;
	pdata->rtc->range_max = U32_MAX;

	clk_disable(pdata->clk);
	platform_set_drvdata(pdev, pdata);
	ret =
	    devm_request_irq(&pdev->dev, pdata->irq, mxc_rtc_interrupt, 0,
			     pdev->name, &pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "interrupt not available.\n");
		clk_unprepare(pdata->clk);
		return ret;
	}

	ret = devm_rtc_register_device(pdata->rtc);
	if (ret < 0)
		clk_unprepare(pdata->clk);

	return ret;
}

static void mxc_rtc_remove(struct platform_device *pdev)
{
	struct mxc_rtc_data *pdata = platform_get_drvdata(pdev);

	clk_disable_unprepare(pdata->clk);
}

static const struct of_device_id mxc_ids[] = {
	{ .compatible = "fsl,imx53-rtc", },
	{}
};
MODULE_DEVICE_TABLE(of, mxc_ids);

static struct platform_driver mxc_rtc_driver = {
	.driver = {
		.name = "mxc_rtc_v2",
		.of_match_table = mxc_ids,
	},
	.probe = mxc_rtc_probe,
	.remove_new = mxc_rtc_remove,
};

module_platform_driver(mxc_rtc_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Real Time Clock (RTC) Driver for i.MX53");
MODULE_LICENSE("GPL");
