
 

 

#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeirq.h>
#include <linux/rtc.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/of.h>

 

#define DTCMR     0x00            
#define DTCLR     0x04            

#define DCAMR     0x08            
#define DCALR     0x0c            
#define DCAMR_UNSET  0xFFFFFFFF   

#define DCR       0x10            
#define DCR_TDCHL (1 << 30)       
#define DCR_TDCSL (1 << 29)       
#define DCR_KSSL  (1 << 27)       
#define DCR_MCHL  (1 << 20)       
#define DCR_MCSL  (1 << 19)       
#define DCR_TCHL  (1 << 18)       
#define DCR_TCSL  (1 << 17)       
#define DCR_FSHL  (1 << 16)       
#define DCR_TCE   (1 << 3)        
#define DCR_MCE   (1 << 2)        

#define DSR       0x14            
#define DSR_WTD   (1 << 23)       
#define DSR_ETBD  (1 << 22)       
#define DSR_ETAD  (1 << 21)       
#define DSR_EBD   (1 << 20)       
#define DSR_SAD   (1 << 19)       
#define DSR_TTD   (1 << 18)       
#define DSR_CTD   (1 << 17)       
#define DSR_VTD   (1 << 16)       
#define DSR_WBF   (1 << 10)       
#define DSR_WNF   (1 << 9)        
#define DSR_WCF   (1 << 8)        
#define DSR_WEF   (1 << 7)        
#define DSR_CAF   (1 << 4)        
#define DSR_MCO   (1 << 3)        
#define DSR_TCO   (1 << 2)        
#define DSR_NVF   (1 << 1)        
#define DSR_SVF   (1 << 0)        

#define DIER      0x18            
#define DIER_WNIE (1 << 9)        
#define DIER_WCIE (1 << 8)        
#define DIER_WEIE (1 << 7)        
#define DIER_CAIE (1 << 4)        
#define DIER_SVIE (1 << 0)        

#define DMCR      0x1c            

#define DTCR      0x28            
#define DTCR_MOE  (1 << 9)        
#define DTCR_TOE  (1 << 8)        
#define DTCR_WTE  (1 << 7)        
#define DTCR_ETBE (1 << 6)        
#define DTCR_ETAE (1 << 5)        
#define DTCR_EBE  (1 << 4)        
#define DTCR_SAIE (1 << 3)        
#define DTCR_TTE  (1 << 2)        
#define DTCR_CTE  (1 << 1)        
#define DTCR_VTE  (1 << 0)        

#define DGPR      0x3c            

 
struct imxdi_dev {
	struct platform_device *pdev;
	struct rtc_device *rtc;
	void __iomem *ioaddr;
	struct clk *clk;
	u32 dsr;
	spinlock_t irq_lock;
	wait_queue_head_t write_wait;
	struct mutex write_mutex;
	struct work_struct work;
};

 

 
static void di_write_busy_wait(const struct imxdi_dev *imxdi, u32 val,
			       unsigned reg)
{
	 
	writel(val, imxdi->ioaddr + reg);

	 
	usleep_range(130, 200);
}

static void di_report_tamper_info(struct imxdi_dev *imxdi,  u32 dsr)
{
	u32 dtcr;

	dtcr = readl(imxdi->ioaddr + DTCR);

	dev_emerg(&imxdi->pdev->dev, "DryIce tamper event detected\n");
	 
	if (dsr & DSR_VTD)
		dev_emerg(&imxdi->pdev->dev, "%sVoltage Tamper Event\n",
			  dtcr & DTCR_VTE ? "" : "Spurious ");

	if (dsr & DSR_CTD)
		dev_emerg(&imxdi->pdev->dev, "%s32768 Hz Clock Tamper Event\n",
			  dtcr & DTCR_CTE ? "" : "Spurious ");

	if (dsr & DSR_TTD)
		dev_emerg(&imxdi->pdev->dev, "%sTemperature Tamper Event\n",
			  dtcr & DTCR_TTE ? "" : "Spurious ");

	if (dsr & DSR_SAD)
		dev_emerg(&imxdi->pdev->dev,
			  "%sSecure Controller Alarm Event\n",
			  dtcr & DTCR_SAIE ? "" : "Spurious ");

	if (dsr & DSR_EBD)
		dev_emerg(&imxdi->pdev->dev, "%sExternal Boot Tamper Event\n",
			  dtcr & DTCR_EBE ? "" : "Spurious ");

	if (dsr & DSR_ETAD)
		dev_emerg(&imxdi->pdev->dev, "%sExternal Tamper A Event\n",
			  dtcr & DTCR_ETAE ? "" : "Spurious ");

	if (dsr & DSR_ETBD)
		dev_emerg(&imxdi->pdev->dev, "%sExternal Tamper B Event\n",
			  dtcr & DTCR_ETBE ? "" : "Spurious ");

	if (dsr & DSR_WTD)
		dev_emerg(&imxdi->pdev->dev, "%sWire-mesh Tamper Event\n",
			  dtcr & DTCR_WTE ? "" : "Spurious ");

	if (dsr & DSR_MCO)
		dev_emerg(&imxdi->pdev->dev,
			  "%sMonotonic-counter Overflow Event\n",
			  dtcr & DTCR_MOE ? "" : "Spurious ");

	if (dsr & DSR_TCO)
		dev_emerg(&imxdi->pdev->dev, "%sTimer-counter Overflow Event\n",
			  dtcr & DTCR_TOE ? "" : "Spurious ");
}

static void di_what_is_to_be_done(struct imxdi_dev *imxdi,
				  const char *power_supply)
{
	dev_emerg(&imxdi->pdev->dev, "Please cycle the %s power supply in order to get the DryIce/RTC unit working again\n",
		  power_supply);
}

static int di_handle_failure_state(struct imxdi_dev *imxdi, u32 dsr)
{
	u32 dcr;

	dev_dbg(&imxdi->pdev->dev, "DSR register reports: %08X\n", dsr);

	 
	di_report_tamper_info(imxdi, dsr);

	dcr = readl(imxdi->ioaddr + DCR);

	if (dcr & DCR_FSHL) {
		 
		di_what_is_to_be_done(imxdi, "battery");
		return -ENODEV;
	}
	 
	di_what_is_to_be_done(imxdi, "main");

	return -ENODEV;
}

static int di_handle_valid_state(struct imxdi_dev *imxdi, u32 dsr)
{
	 
	di_write_busy_wait(imxdi, DCAMR_UNSET, DCAMR);
	di_write_busy_wait(imxdi, 0, DCALR);

	 
	if (dsr & DSR_CAF)
		di_write_busy_wait(imxdi, DSR_CAF, DSR);

	return 0;
}

static int di_handle_invalid_state(struct imxdi_dev *imxdi, u32 dsr)
{
	u32 dcr, sec;

	 
	di_write_busy_wait(imxdi, 0x00000000, DTCR);
	 
	di_write_busy_wait(imxdi, DCR_TDCSL, DCR);

	sec = readl(imxdi->ioaddr + DTCMR);
	if (sec != 0)
		dev_warn(&imxdi->pdev->dev,
			 "The security violation has happened at %u seconds\n",
			 sec);
	 
	dcr = readl(imxdi->ioaddr + DCR);
	if (!(dcr & DCR_TCE)) {
		if (dcr & DCR_TCHL) {
			 
			di_what_is_to_be_done(imxdi, "battery");
			return -ENODEV;
		}
		if (dcr & DCR_TCSL) {
			di_what_is_to_be_done(imxdi, "main");
			return -ENODEV;
		}
	}
	 
	 
	di_write_busy_wait(imxdi, DSR_NVF, DSR);
	 
	di_write_busy_wait(imxdi, DSR_TCO, DSR);
	 
	di_write_busy_wait(imxdi, dcr | DCR_TCE, DCR);
	 
	di_write_busy_wait(imxdi, sec, DTCMR);

	 
	return di_handle_valid_state(imxdi, __raw_readl(imxdi->ioaddr + DSR));
}

static int di_handle_invalid_and_failure_state(struct imxdi_dev *imxdi, u32 dsr)
{
	u32 dcr;

	 
	if (dsr & (DSR_WTD | DSR_ETBD | DSR_ETAD | DSR_EBD | DSR_SAD |
			DSR_TTD | DSR_CTD | DSR_VTD | DSR_MCO | DSR_TCO)) {
		dcr = __raw_readl(imxdi->ioaddr + DCR);
		if (dcr & DCR_TDCHL) {
			 
			 
			di_what_is_to_be_done(imxdi, "battery");
			return -ENODEV;
		}
		if (dcr & DCR_TDCSL) {
			 
			di_what_is_to_be_done(imxdi, "main");
			return -ENODEV;
		}
	}

	 
	di_write_busy_wait(imxdi, 0x00000000, DTCR);

	 
	di_write_busy_wait(imxdi, dsr & (DSR_WTD | DSR_ETBD | DSR_ETAD |
			DSR_EBD | DSR_SAD | DSR_TTD | DSR_CTD | DSR_VTD |
			DSR_MCO | DSR_TCO), DSR);

	dsr = readl(imxdi->ioaddr + DSR);
	if ((dsr & ~(DSR_NVF | DSR_SVF | DSR_WBF | DSR_WNF |
			DSR_WCF | DSR_WEF)) != 0)
		dev_warn(&imxdi->pdev->dev,
			 "There are still some sources of pain in DSR: %08x!\n",
			 dsr & ~(DSR_NVF | DSR_SVF | DSR_WBF | DSR_WNF |
				 DSR_WCF | DSR_WEF));

	 
	di_write_busy_wait(imxdi, DSR_SVF, DSR);

	 
	dsr = readl(imxdi->ioaddr + DSR);
	if (dsr & DSR_SVF) {
		dev_crit(&imxdi->pdev->dev,
			 "Cannot clear the security violation flag. We are ending up in an endless loop!\n");
		 
		di_what_is_to_be_done(imxdi, "battery");
		return -ENODEV;
	}

	 
	return di_handle_invalid_state(imxdi, dsr);
}

static int di_handle_state(struct imxdi_dev *imxdi)
{
	int rc;
	u32 dsr;

	dsr = readl(imxdi->ioaddr + DSR);

	switch (dsr & (DSR_NVF | DSR_SVF)) {
	case DSR_NVF:
		dev_warn(&imxdi->pdev->dev, "Invalid stated unit detected\n");
		rc = di_handle_invalid_state(imxdi, dsr);
		break;
	case DSR_SVF:
		dev_warn(&imxdi->pdev->dev, "Failure stated unit detected\n");
		rc = di_handle_failure_state(imxdi, dsr);
		break;
	case DSR_NVF | DSR_SVF:
		dev_warn(&imxdi->pdev->dev,
			 "Failure+Invalid stated unit detected\n");
		rc = di_handle_invalid_and_failure_state(imxdi, dsr);
		break;
	default:
		dev_notice(&imxdi->pdev->dev, "Unlocked unit detected\n");
		rc = di_handle_valid_state(imxdi, dsr);
	}

	return rc;
}

 
static void di_int_enable(struct imxdi_dev *imxdi, u32 intr)
{
	unsigned long flags;

	spin_lock_irqsave(&imxdi->irq_lock, flags);
	writel(readl(imxdi->ioaddr + DIER) | intr,
	       imxdi->ioaddr + DIER);
	spin_unlock_irqrestore(&imxdi->irq_lock, flags);
}

 
static void di_int_disable(struct imxdi_dev *imxdi, u32 intr)
{
	unsigned long flags;

	spin_lock_irqsave(&imxdi->irq_lock, flags);
	writel(readl(imxdi->ioaddr + DIER) & ~intr,
	       imxdi->ioaddr + DIER);
	spin_unlock_irqrestore(&imxdi->irq_lock, flags);
}

 
static void clear_write_error(struct imxdi_dev *imxdi)
{
	int cnt;

	dev_warn(&imxdi->pdev->dev, "WARNING: Register write error!\n");

	 
	writel(DSR_WEF, imxdi->ioaddr + DSR);

	 
	for (cnt = 0; cnt < 1000; cnt++) {
		if ((readl(imxdi->ioaddr + DSR) & DSR_WEF) == 0)
			return;
		udelay(10);
	}
	dev_err(&imxdi->pdev->dev,
			"ERROR: Cannot clear write-error flag!\n");
}

 
static int di_write_wait(struct imxdi_dev *imxdi, u32 val, int reg)
{
	int ret;
	int rc = 0;

	 
	mutex_lock(&imxdi->write_mutex);

	 
	di_int_enable(imxdi, DIER_WCIE);

	imxdi->dsr = 0;

	 
	writel(val, imxdi->ioaddr + reg);

	 
	ret = wait_event_interruptible_timeout(imxdi->write_wait,
			imxdi->dsr & (DSR_WCF | DSR_WEF), msecs_to_jiffies(1));
	if (ret < 0) {
		rc = ret;
		goto out;
	} else if (ret == 0) {
		dev_warn(&imxdi->pdev->dev,
				"Write-wait timeout "
				"val = 0x%08x reg = 0x%08x\n", val, reg);
	}

	 
	if (imxdi->dsr & DSR_WEF) {
		clear_write_error(imxdi);
		rc = -EIO;
	}

out:
	mutex_unlock(&imxdi->write_mutex);

	return rc;
}

 
static int dryice_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct imxdi_dev *imxdi = dev_get_drvdata(dev);
	unsigned long now;

	now = readl(imxdi->ioaddr + DTCMR);
	rtc_time64_to_tm(now, tm);

	return 0;
}

 
static int dryice_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct imxdi_dev *imxdi = dev_get_drvdata(dev);
	u32 dcr, dsr;
	int rc;

	dcr = readl(imxdi->ioaddr + DCR);
	dsr = readl(imxdi->ioaddr + DSR);

	if (!(dcr & DCR_TCE) || (dsr & DSR_SVF)) {
		if (dcr & DCR_TCHL) {
			 
			di_what_is_to_be_done(imxdi, "battery");
			return -EPERM;
		}
		if ((dcr & DCR_TCSL) || (dsr & DSR_SVF)) {
			 
			di_what_is_to_be_done(imxdi, "main");
			return -EPERM;
		}
	}

	 
	rc = di_write_wait(imxdi, 0, DTCLR);
	if (rc != 0)
		return rc;

	rc = di_write_wait(imxdi, rtc_tm_to_time64(tm), DTCMR);
	if (rc != 0)
		return rc;

	return di_write_wait(imxdi, readl(imxdi->ioaddr + DCR) | DCR_TCE, DCR);
}

static int dryice_rtc_alarm_irq_enable(struct device *dev,
		unsigned int enabled)
{
	struct imxdi_dev *imxdi = dev_get_drvdata(dev);

	if (enabled)
		di_int_enable(imxdi, DIER_CAIE);
	else
		di_int_disable(imxdi, DIER_CAIE);

	return 0;
}

 
static int dryice_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct imxdi_dev *imxdi = dev_get_drvdata(dev);
	u32 dcamr;

	dcamr = readl(imxdi->ioaddr + DCAMR);
	rtc_time64_to_tm(dcamr, &alarm->time);

	 
	alarm->enabled = (readl(imxdi->ioaddr + DIER) & DIER_CAIE) != 0;

	 
	mutex_lock(&imxdi->write_mutex);

	 
	alarm->pending = (readl(imxdi->ioaddr + DSR) & DSR_CAF) != 0;

	mutex_unlock(&imxdi->write_mutex);

	return 0;
}

 
static int dryice_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct imxdi_dev *imxdi = dev_get_drvdata(dev);
	int rc;

	 
	rc = di_write_wait(imxdi, rtc_tm_to_time64(&alarm->time), DCAMR);
	if (rc)
		return rc;

	if (alarm->enabled)
		di_int_enable(imxdi, DIER_CAIE);   
	else
		di_int_disable(imxdi, DIER_CAIE);  

	return 0;
}

static const struct rtc_class_ops dryice_rtc_ops = {
	.read_time		= dryice_rtc_read_time,
	.set_time		= dryice_rtc_set_time,
	.alarm_irq_enable	= dryice_rtc_alarm_irq_enable,
	.read_alarm		= dryice_rtc_read_alarm,
	.set_alarm		= dryice_rtc_set_alarm,
};

 
static irqreturn_t dryice_irq(int irq, void *dev_id)
{
	struct imxdi_dev *imxdi = dev_id;
	u32 dsr, dier;
	irqreturn_t rc = IRQ_NONE;

	dier = readl(imxdi->ioaddr + DIER);
	dsr = readl(imxdi->ioaddr + DSR);

	 
	if (dier & DIER_SVIE) {
		if (dsr & DSR_SVF) {
			 
			di_int_disable(imxdi, DIER_SVIE);
			 
			di_report_tamper_info(imxdi, dsr);
			rc = IRQ_HANDLED;
		}
	}

	 
	if (dier & DIER_WCIE) {
		 
		if (list_empty_careful(&imxdi->write_wait.head))
			return rc;

		 
		if (dsr & (DSR_WCF | DSR_WEF)) {
			 
			di_int_disable(imxdi, DIER_WCIE);

			 
			imxdi->dsr |= dsr;

			wake_up_interruptible(&imxdi->write_wait);
			rc = IRQ_HANDLED;
		}
	}

	 
	if (dier & DIER_CAIE) {
		 
		if (dsr & DSR_CAF) {
			 
			di_int_disable(imxdi, DIER_CAIE);

			 
			schedule_work(&imxdi->work);
			rc = IRQ_HANDLED;
		}
	}
	return rc;
}

 
static void dryice_work(struct work_struct *work)
{
	struct imxdi_dev *imxdi = container_of(work,
			struct imxdi_dev, work);

	 
	di_write_wait(imxdi, DSR_CAF, DSR);

	 
	rtc_update_irq(imxdi->rtc, 1, RTC_AF | RTC_IRQF);
}

 
static int __init dryice_rtc_probe(struct platform_device *pdev)
{
	struct imxdi_dev *imxdi;
	int norm_irq, sec_irq;
	int rc;

	imxdi = devm_kzalloc(&pdev->dev, sizeof(*imxdi), GFP_KERNEL);
	if (!imxdi)
		return -ENOMEM;

	imxdi->pdev = pdev;

	imxdi->ioaddr = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(imxdi->ioaddr))
		return PTR_ERR(imxdi->ioaddr);

	spin_lock_init(&imxdi->irq_lock);

	norm_irq = platform_get_irq(pdev, 0);
	if (norm_irq < 0)
		return norm_irq;

	 
	sec_irq = platform_get_irq(pdev, 1);
	if (sec_irq <= 0)
		sec_irq = IRQ_NOTCONNECTED;

	init_waitqueue_head(&imxdi->write_wait);

	INIT_WORK(&imxdi->work, dryice_work);

	mutex_init(&imxdi->write_mutex);

	imxdi->rtc = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(imxdi->rtc))
		return PTR_ERR(imxdi->rtc);

	imxdi->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(imxdi->clk))
		return PTR_ERR(imxdi->clk);
	rc = clk_prepare_enable(imxdi->clk);
	if (rc)
		return rc;

	 

	 
	writel(0, imxdi->ioaddr + DIER);

	rc = di_handle_state(imxdi);
	if (rc != 0)
		goto err;

	rc = devm_request_irq(&pdev->dev, norm_irq, dryice_irq,
			      IRQF_SHARED, pdev->name, imxdi);
	if (rc) {
		dev_warn(&pdev->dev, "interrupt not available.\n");
		goto err;
	}

	rc = devm_request_irq(&pdev->dev, sec_irq, dryice_irq,
			      IRQF_SHARED, pdev->name, imxdi);
	if (rc) {
		dev_warn(&pdev->dev, "security violation interrupt not available.\n");
		 
	}

	platform_set_drvdata(pdev, imxdi);

	device_init_wakeup(&pdev->dev, true);
	dev_pm_set_wake_irq(&pdev->dev, norm_irq);

	imxdi->rtc->ops = &dryice_rtc_ops;
	imxdi->rtc->range_max = U32_MAX;

	rc = devm_rtc_register_device(imxdi->rtc);
	if (rc)
		goto err;

	return 0;

err:
	clk_disable_unprepare(imxdi->clk);

	return rc;
}

static int __exit dryice_rtc_remove(struct platform_device *pdev)
{
	struct imxdi_dev *imxdi = platform_get_drvdata(pdev);

	flush_work(&imxdi->work);

	 
	writel(0, imxdi->ioaddr + DIER);

	clk_disable_unprepare(imxdi->clk);

	return 0;
}

static const struct of_device_id dryice_dt_ids[] = {
	{ .compatible = "fsl,imx25-rtc" },
	{   }
};

MODULE_DEVICE_TABLE(of, dryice_dt_ids);

static struct platform_driver dryice_rtc_driver = {
	.driver = {
		   .name = "imxdi_rtc",
		   .of_match_table = dryice_dt_ids,
		   },
	.remove = __exit_p(dryice_rtc_remove),
};

module_platform_driver_probe(dryice_rtc_driver, dryice_rtc_probe);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_AUTHOR("Baruch Siach <baruch@tkos.co.il>");
MODULE_DESCRIPTION("IMX DryIce Realtime Clock Driver (RTC)");
MODULE_LICENSE("GPL");
