
 

#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/fsl/ftm.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/acpi.h>
#include <linux/pm_wakeirq.h>

#define FTM_SC_CLK(c)		((c) << FTM_SC_CLK_MASK_SHIFT)

 
#define FTM_SC_CLKS_FIXED_FREQ	0x02
#define FIXED_FREQ_CLK		32000

 
#define MAX_FREQ_DIV		(1 << FTM_SC_PS_MASK)

 
#define MAX_COUNT_VAL		0xffff

struct ftm_rtc {
	struct rtc_device *rtc_dev;
	void __iomem *base;
	bool big_endian;
	u32 alarm_freq;
};

static inline u32 rtc_readl(struct ftm_rtc *dev, u32 reg)
{
	if (dev->big_endian)
		return ioread32be(dev->base + reg);
	else
		return ioread32(dev->base + reg);
}

static inline void rtc_writel(struct ftm_rtc *dev, u32 reg, u32 val)
{
	if (dev->big_endian)
		iowrite32be(val, dev->base + reg);
	else
		iowrite32(val, dev->base + reg);
}

static inline void ftm_counter_enable(struct ftm_rtc *rtc)
{
	u32 val;

	 
	val = rtc_readl(rtc, FTM_SC);
	val &= ~(FTM_SC_PS_MASK | FTM_SC_CLK_MASK);
	val |= (FTM_SC_PS_MASK | FTM_SC_CLK(FTM_SC_CLKS_FIXED_FREQ));
	rtc_writel(rtc, FTM_SC, val);
}

static inline void ftm_counter_disable(struct ftm_rtc *rtc)
{
	u32 val;

	 
	val = rtc_readl(rtc, FTM_SC);
	val &= ~(FTM_SC_PS_MASK | FTM_SC_CLK_MASK);
	rtc_writel(rtc, FTM_SC, val);
}

static inline void ftm_irq_acknowledge(struct ftm_rtc *rtc)
{
	unsigned int timeout = 100;

	 
	while ((FTM_SC_TOF & rtc_readl(rtc, FTM_SC)) && timeout--)
		rtc_writel(rtc, FTM_SC, rtc_readl(rtc, FTM_SC) & (~FTM_SC_TOF));
}

static inline void ftm_irq_enable(struct ftm_rtc *rtc)
{
	u32 val;

	val = rtc_readl(rtc, FTM_SC);
	val |= FTM_SC_TOIE;
	rtc_writel(rtc, FTM_SC, val);
}

static inline void ftm_irq_disable(struct ftm_rtc *rtc)
{
	u32 val;

	val = rtc_readl(rtc, FTM_SC);
	val &= ~FTM_SC_TOIE;
	rtc_writel(rtc, FTM_SC, val);
}

static inline void ftm_reset_counter(struct ftm_rtc *rtc)
{
	 
	rtc_writel(rtc, FTM_CNT, 0x00);
}

static void ftm_clean_alarm(struct ftm_rtc *rtc)
{
	ftm_counter_disable(rtc);

	rtc_writel(rtc, FTM_CNTIN, 0x00);
	rtc_writel(rtc, FTM_MOD, ~0U);

	ftm_reset_counter(rtc);
}

static irqreturn_t ftm_rtc_alarm_interrupt(int irq, void *dev)
{
	struct ftm_rtc *rtc = dev;

	rtc_update_irq(rtc->rtc_dev, 1, RTC_IRQF | RTC_AF);

	ftm_irq_acknowledge(rtc);
	ftm_irq_disable(rtc);
	ftm_clean_alarm(rtc);

	return IRQ_HANDLED;
}

static int ftm_rtc_alarm_irq_enable(struct device *dev,
		unsigned int enabled)
{
	struct ftm_rtc *rtc = dev_get_drvdata(dev);

	if (enabled)
		ftm_irq_enable(rtc);
	else
		ftm_irq_disable(rtc);

	return 0;
}

 
static int ftm_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	rtc_time64_to_tm(ktime_get_real_seconds(), tm);

	return 0;
}

static int ftm_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	return 0;
}

 
static int ftm_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	time64_t alm_time;
	unsigned long long cycle;
	struct ftm_rtc *rtc = dev_get_drvdata(dev);

	alm_time = rtc_tm_to_time64(&alm->time);

	ftm_clean_alarm(rtc);
	cycle = (alm_time - ktime_get_real_seconds()) * rtc->alarm_freq;
	if (cycle > MAX_COUNT_VAL) {
		pr_err("Out of alarm range {0~262} seconds.\n");
		return -ERANGE;
	}

	ftm_irq_disable(rtc);

	 
	rtc_writel(rtc, FTM_MOD, cycle - 1);

	ftm_counter_enable(rtc);
	ftm_irq_enable(rtc);

	return 0;

}

static const struct rtc_class_ops ftm_rtc_ops = {
	.read_time		= ftm_rtc_read_time,
	.read_alarm		= ftm_rtc_read_alarm,
	.set_alarm		= ftm_rtc_set_alarm,
	.alarm_irq_enable	= ftm_rtc_alarm_irq_enable,
};

static int ftm_rtc_probe(struct platform_device *pdev)
{
	int irq;
	int ret;
	struct ftm_rtc *rtc;

	rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (unlikely(!rtc)) {
		dev_err(&pdev->dev, "cannot alloc memory for rtc\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, rtc);

	rtc->rtc_dev = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(rtc->rtc_dev))
		return PTR_ERR(rtc->rtc_dev);

	rtc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(rtc->base)) {
		dev_err(&pdev->dev, "cannot ioremap resource for rtc\n");
		return PTR_ERR(rtc->base);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_irq(&pdev->dev, irq, ftm_rtc_alarm_interrupt,
			       0, dev_name(&pdev->dev), rtc);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request irq\n");
		return ret;
	}

	rtc->big_endian =
		device_property_read_bool(&pdev->dev, "big-endian");

	rtc->alarm_freq = (u32)FIXED_FREQ_CLK / (u32)MAX_FREQ_DIV;
	rtc->rtc_dev->ops = &ftm_rtc_ops;

	device_init_wakeup(&pdev->dev, true);
	ret = dev_pm_set_wake_irq(&pdev->dev, irq);
	if (ret)
		dev_err(&pdev->dev, "failed to enable irq wake\n");

	ret = devm_rtc_register_device(rtc->rtc_dev);
	if (ret) {
		dev_err(&pdev->dev, "can't register rtc device\n");
		return ret;
	}

	return 0;
}

static const struct of_device_id ftm_rtc_match[] = {
	{ .compatible = "fsl,ls1012a-ftm-alarm", },
	{ .compatible = "fsl,ls1021a-ftm-alarm", },
	{ .compatible = "fsl,ls1028a-ftm-alarm", },
	{ .compatible = "fsl,ls1043a-ftm-alarm", },
	{ .compatible = "fsl,ls1046a-ftm-alarm", },
	{ .compatible = "fsl,ls1088a-ftm-alarm", },
	{ .compatible = "fsl,ls208xa-ftm-alarm", },
	{ .compatible = "fsl,lx2160a-ftm-alarm", },
	{ },
};
MODULE_DEVICE_TABLE(of, ftm_rtc_match);

static const struct acpi_device_id ftm_imx_acpi_ids[] = {
	{"NXP0014",},
	{ }
};
MODULE_DEVICE_TABLE(acpi, ftm_imx_acpi_ids);

static struct platform_driver ftm_rtc_driver = {
	.probe		= ftm_rtc_probe,
	.driver		= {
		.name	= "ftm-alarm",
		.of_match_table = ftm_rtc_match,
		.acpi_match_table = ACPI_PTR(ftm_imx_acpi_ids),
	},
};

module_platform_driver(ftm_rtc_driver);

MODULE_DESCRIPTION("NXP/Freescale FlexTimer alarm driver");
MODULE_AUTHOR("Biwen Li <biwen.li@nxp.com>");
MODULE_LICENSE("GPL");
