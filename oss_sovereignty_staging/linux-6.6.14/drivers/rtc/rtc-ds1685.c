
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/bcd.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/workqueue.h>

#include <linux/rtc/ds1685.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif


 
 

 
static u8
ds1685_read(struct ds1685_priv *rtc, int reg)
{
	return readb((u8 __iomem *)rtc->regs +
		     (reg * rtc->regstep));
}

 
static void
ds1685_write(struct ds1685_priv *rtc, int reg, u8 value)
{
	writeb(value, ((u8 __iomem *)rtc->regs +
		       (reg * rtc->regstep)));
}
 

 

 
static u8
ds1685_indirect_read(struct ds1685_priv *rtc, int reg)
{
	writeb(reg, rtc->regs);
	return readb(rtc->data);
}

 
static void
ds1685_indirect_write(struct ds1685_priv *rtc, int reg, u8 value)
{
	writeb(reg, rtc->regs);
	writeb(value, rtc->data);
}

 
 

 
static inline u8
ds1685_rtc_bcd2bin(struct ds1685_priv *rtc, u8 val, u8 bcd_mask, u8 bin_mask)
{
	if (rtc->bcd_mode)
		return (bcd2bin(val) & bcd_mask);

	return (val & bin_mask);
}

 
static inline u8
ds1685_rtc_bin2bcd(struct ds1685_priv *rtc, u8 val, u8 bin_mask, u8 bcd_mask)
{
	if (rtc->bcd_mode)
		return (bin2bcd(val) & bcd_mask);

	return (val & bin_mask);
}

 
static inline int
ds1685_rtc_check_mday(struct ds1685_priv *rtc, u8 mday)
{
	if (rtc->bcd_mode) {
		if (mday < 0x01 || mday > 0x31 || (mday & 0x0f) > 0x09)
			return -EDOM;
	} else {
		if (mday < 1 || mday > 31)
			return -EDOM;
	}
	return 0;
}

 
static inline void
ds1685_rtc_switch_to_bank0(struct ds1685_priv *rtc)
{
	rtc->write(rtc, RTC_CTRL_A,
		   (rtc->read(rtc, RTC_CTRL_A) & ~(RTC_CTRL_A_DV0)));
}

 
static inline void
ds1685_rtc_switch_to_bank1(struct ds1685_priv *rtc)
{
	rtc->write(rtc, RTC_CTRL_A,
		   (rtc->read(rtc, RTC_CTRL_A) | RTC_CTRL_A_DV0));
}

 
static inline void
ds1685_rtc_begin_data_access(struct ds1685_priv *rtc)
{
	 
	rtc->write(rtc, RTC_CTRL_B,
		   (rtc->read(rtc, RTC_CTRL_B) | RTC_CTRL_B_SET));

	 
	ds1685_rtc_switch_to_bank1(rtc);

	 
	while (rtc->read(rtc, RTC_EXT_CTRL_4A) & RTC_CTRL_4A_INCR)
		cpu_relax();
}

 
static inline void
ds1685_rtc_end_data_access(struct ds1685_priv *rtc)
{
	 
	ds1685_rtc_switch_to_bank0(rtc);

	 
	rtc->write(rtc, RTC_CTRL_B,
		   (rtc->read(rtc, RTC_CTRL_B) & ~(RTC_CTRL_B_SET)));
}

 
static inline void
ds1685_rtc_get_ssn(struct ds1685_priv *rtc, u8 *ssn)
{
	ssn[0] = rtc->read(rtc, RTC_BANK1_SSN_MODEL);
	ssn[1] = rtc->read(rtc, RTC_BANK1_SSN_BYTE_1);
	ssn[2] = rtc->read(rtc, RTC_BANK1_SSN_BYTE_2);
	ssn[3] = rtc->read(rtc, RTC_BANK1_SSN_BYTE_3);
	ssn[4] = rtc->read(rtc, RTC_BANK1_SSN_BYTE_4);
	ssn[5] = rtc->read(rtc, RTC_BANK1_SSN_BYTE_5);
	ssn[6] = rtc->read(rtc, RTC_BANK1_SSN_BYTE_6);
	ssn[7] = rtc->read(rtc, RTC_BANK1_SSN_CRC);
}
 


 
 

 
static int
ds1685_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct ds1685_priv *rtc = dev_get_drvdata(dev);
	u8 century;
	u8 seconds, minutes, hours, wday, mday, month, years;

	 
	ds1685_rtc_begin_data_access(rtc);
	seconds = rtc->read(rtc, RTC_SECS);
	minutes = rtc->read(rtc, RTC_MINS);
	hours   = rtc->read(rtc, RTC_HRS);
	wday    = rtc->read(rtc, RTC_WDAY);
	mday    = rtc->read(rtc, RTC_MDAY);
	month   = rtc->read(rtc, RTC_MONTH);
	years   = rtc->read(rtc, RTC_YEAR);
	century = rtc->read(rtc, RTC_CENTURY);
	ds1685_rtc_end_data_access(rtc);

	 
	years        = ds1685_rtc_bcd2bin(rtc, years, RTC_YEAR_BCD_MASK,
					  RTC_YEAR_BIN_MASK);
	century      = ds1685_rtc_bcd2bin(rtc, century, RTC_CENTURY_MASK,
					  RTC_CENTURY_MASK);
	tm->tm_sec   = ds1685_rtc_bcd2bin(rtc, seconds, RTC_SECS_BCD_MASK,
					  RTC_SECS_BIN_MASK);
	tm->tm_min   = ds1685_rtc_bcd2bin(rtc, minutes, RTC_MINS_BCD_MASK,
					  RTC_MINS_BIN_MASK);
	tm->tm_hour  = ds1685_rtc_bcd2bin(rtc, hours, RTC_HRS_24_BCD_MASK,
					  RTC_HRS_24_BIN_MASK);
	tm->tm_wday  = (ds1685_rtc_bcd2bin(rtc, wday, RTC_WDAY_MASK,
					   RTC_WDAY_MASK) - 1);
	tm->tm_mday  = ds1685_rtc_bcd2bin(rtc, mday, RTC_MDAY_BCD_MASK,
					  RTC_MDAY_BIN_MASK);
	tm->tm_mon   = (ds1685_rtc_bcd2bin(rtc, month, RTC_MONTH_BCD_MASK,
					   RTC_MONTH_BIN_MASK) - 1);
	tm->tm_year  = ((years + (century * 100)) - 1900);
	tm->tm_yday  = rtc_year_days(tm->tm_mday, tm->tm_mon, tm->tm_year);
	tm->tm_isdst = 0;  

	return 0;
}

 
static int
ds1685_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct ds1685_priv *rtc = dev_get_drvdata(dev);
	u8 ctrlb, seconds, minutes, hours, wday, mday, month, years, century;

	 
	seconds = ds1685_rtc_bin2bcd(rtc, tm->tm_sec, RTC_SECS_BIN_MASK,
				     RTC_SECS_BCD_MASK);
	minutes = ds1685_rtc_bin2bcd(rtc, tm->tm_min, RTC_MINS_BIN_MASK,
				     RTC_MINS_BCD_MASK);
	hours   = ds1685_rtc_bin2bcd(rtc, tm->tm_hour, RTC_HRS_24_BIN_MASK,
				     RTC_HRS_24_BCD_MASK);
	wday    = ds1685_rtc_bin2bcd(rtc, (tm->tm_wday + 1), RTC_WDAY_MASK,
				     RTC_WDAY_MASK);
	mday    = ds1685_rtc_bin2bcd(rtc, tm->tm_mday, RTC_MDAY_BIN_MASK,
				     RTC_MDAY_BCD_MASK);
	month   = ds1685_rtc_bin2bcd(rtc, (tm->tm_mon + 1), RTC_MONTH_BIN_MASK,
				     RTC_MONTH_BCD_MASK);
	years   = ds1685_rtc_bin2bcd(rtc, (tm->tm_year % 100),
				     RTC_YEAR_BIN_MASK, RTC_YEAR_BCD_MASK);
	century = ds1685_rtc_bin2bcd(rtc, ((tm->tm_year + 1900) / 100),
				     RTC_CENTURY_MASK, RTC_CENTURY_MASK);

	 
	if ((tm->tm_mon > 11) || (mday == 0))
		return -EDOM;

	if (tm->tm_mday > rtc_month_days(tm->tm_mon, tm->tm_year))
		return -EDOM;

	if ((tm->tm_hour >= 24) || (tm->tm_min >= 60) ||
	    (tm->tm_sec >= 60)  || (wday > 7))
		return -EDOM;

	 
	ds1685_rtc_begin_data_access(rtc);
	ctrlb = rtc->read(rtc, RTC_CTRL_B);
	if (rtc->bcd_mode)
		ctrlb &= ~(RTC_CTRL_B_DM);
	else
		ctrlb |= RTC_CTRL_B_DM;
	rtc->write(rtc, RTC_CTRL_B, ctrlb);
	rtc->write(rtc, RTC_SECS, seconds);
	rtc->write(rtc, RTC_MINS, minutes);
	rtc->write(rtc, RTC_HRS, hours);
	rtc->write(rtc, RTC_WDAY, wday);
	rtc->write(rtc, RTC_MDAY, mday);
	rtc->write(rtc, RTC_MONTH, month);
	rtc->write(rtc, RTC_YEAR, years);
	rtc->write(rtc, RTC_CENTURY, century);
	ds1685_rtc_end_data_access(rtc);

	return 0;
}

 
static int
ds1685_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct ds1685_priv *rtc = dev_get_drvdata(dev);
	u8 seconds, minutes, hours, mday, ctrlb, ctrlc;
	int ret;

	 
	ds1685_rtc_begin_data_access(rtc);
	seconds	= rtc->read(rtc, RTC_SECS_ALARM);
	minutes	= rtc->read(rtc, RTC_MINS_ALARM);
	hours	= rtc->read(rtc, RTC_HRS_ALARM);
	mday	= rtc->read(rtc, RTC_MDAY_ALARM);
	ctrlb	= rtc->read(rtc, RTC_CTRL_B);
	ctrlc	= rtc->read(rtc, RTC_CTRL_C);
	ds1685_rtc_end_data_access(rtc);

	 
	ret = ds1685_rtc_check_mday(rtc, mday);
	if (ret)
		return ret;

	 
	if (likely(seconds < 0xc0))
		alrm->time.tm_sec = ds1685_rtc_bcd2bin(rtc, seconds,
						       RTC_SECS_BCD_MASK,
						       RTC_SECS_BIN_MASK);

	if (likely(minutes < 0xc0))
		alrm->time.tm_min = ds1685_rtc_bcd2bin(rtc, minutes,
						       RTC_MINS_BCD_MASK,
						       RTC_MINS_BIN_MASK);

	if (likely(hours < 0xc0))
		alrm->time.tm_hour = ds1685_rtc_bcd2bin(rtc, hours,
							RTC_HRS_24_BCD_MASK,
							RTC_HRS_24_BIN_MASK);

	 
	alrm->time.tm_mday = ds1685_rtc_bcd2bin(rtc, mday, RTC_MDAY_BCD_MASK,
						RTC_MDAY_BIN_MASK);
	alrm->enabled = !!(ctrlb & RTC_CTRL_B_AIE);
	alrm->pending = !!(ctrlc & RTC_CTRL_C_AF);

	return 0;
}

 
static int
ds1685_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct ds1685_priv *rtc = dev_get_drvdata(dev);
	u8 ctrlb, seconds, minutes, hours, mday;
	int ret;

	 
	seconds	= ds1685_rtc_bin2bcd(rtc, alrm->time.tm_sec,
				     RTC_SECS_BIN_MASK,
				     RTC_SECS_BCD_MASK);
	minutes	= ds1685_rtc_bin2bcd(rtc, alrm->time.tm_min,
				     RTC_MINS_BIN_MASK,
				     RTC_MINS_BCD_MASK);
	hours	= ds1685_rtc_bin2bcd(rtc, alrm->time.tm_hour,
				     RTC_HRS_24_BIN_MASK,
				     RTC_HRS_24_BCD_MASK);
	mday	= ds1685_rtc_bin2bcd(rtc, alrm->time.tm_mday,
				     RTC_MDAY_BIN_MASK,
				     RTC_MDAY_BCD_MASK);

	 
	ret = ds1685_rtc_check_mday(rtc, mday);
	if (ret)
		return ret;

	 
	if (unlikely(seconds >= 0xc0))
		seconds = 0xff;

	if (unlikely(minutes >= 0xc0))
		minutes = 0xff;

	if (unlikely(hours >= 0xc0))
		hours = 0xff;

	alrm->time.tm_mon	= -1;
	alrm->time.tm_year	= -1;
	alrm->time.tm_wday	= -1;
	alrm->time.tm_yday	= -1;
	alrm->time.tm_isdst	= -1;

	 
	ds1685_rtc_begin_data_access(rtc);
	ctrlb = rtc->read(rtc, RTC_CTRL_B);
	rtc->write(rtc, RTC_CTRL_B, (ctrlb & ~(RTC_CTRL_B_AIE)));

	 
	rtc->read(rtc, RTC_CTRL_C);

	 
	ctrlb = rtc->read(rtc, RTC_CTRL_B);
	if (rtc->bcd_mode)
		ctrlb &= ~(RTC_CTRL_B_DM);
	else
		ctrlb |= RTC_CTRL_B_DM;
	rtc->write(rtc, RTC_CTRL_B, ctrlb);
	rtc->write(rtc, RTC_SECS_ALARM, seconds);
	rtc->write(rtc, RTC_MINS_ALARM, minutes);
	rtc->write(rtc, RTC_HRS_ALARM, hours);
	rtc->write(rtc, RTC_MDAY_ALARM, mday);

	 
	if (alrm->enabled) {
		ctrlb = rtc->read(rtc, RTC_CTRL_B);
		ctrlb |= RTC_CTRL_B_AIE;
		rtc->write(rtc, RTC_CTRL_B, ctrlb);
	}

	 
	ds1685_rtc_end_data_access(rtc);

	return 0;
}
 


 
 

 
static int
ds1685_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct ds1685_priv *rtc = dev_get_drvdata(dev);

	 
	if (enabled)
		rtc->write(rtc, RTC_CTRL_B, (rtc->read(rtc, RTC_CTRL_B) |
					     RTC_CTRL_B_AIE));
	else
		rtc->write(rtc, RTC_CTRL_B, (rtc->read(rtc, RTC_CTRL_B) &
					     ~(RTC_CTRL_B_AIE)));

	 
	rtc->read(rtc, RTC_CTRL_C);

	return 0;
}
 


 
 

 
static void
ds1685_rtc_extended_irq(struct ds1685_priv *rtc, struct platform_device *pdev)
{
	u8 ctrl4a, ctrl4b;

	ds1685_rtc_switch_to_bank1(rtc);
	ctrl4a = rtc->read(rtc, RTC_EXT_CTRL_4A);
	ctrl4b = rtc->read(rtc, RTC_EXT_CTRL_4B);

	 
	if ((ctrl4b & RTC_CTRL_4B_KSE) && (ctrl4a & RTC_CTRL_4A_KF)) {
		 
		rtc->write(rtc, RTC_EXT_CTRL_4B,
			   (rtc->read(rtc, RTC_EXT_CTRL_4B) &
			    ~(RTC_CTRL_4B_KSE)));

		 
		rtc->write(rtc, RTC_EXT_CTRL_4A,
			   (ctrl4a & ~(RTC_CTRL_4A_KF)));


		 
		msleep(500);
		rtc->write(rtc, RTC_EXT_CTRL_4B,
			   (rtc->read(rtc, RTC_EXT_CTRL_4B) |
			    RTC_CTRL_4B_KSE));

		 
		if (rtc->prepare_poweroff != NULL)
			rtc->prepare_poweroff();
		else
			ds1685_rtc_poweroff(pdev);
	}

	 
	if ((ctrl4b & RTC_CTRL_4B_WIE) && (ctrl4a & RTC_CTRL_4A_WF)) {
		rtc->write(rtc, RTC_EXT_CTRL_4A,
			   (ctrl4a & ~(RTC_CTRL_4A_WF)));

		 
		if (rtc->wake_alarm != NULL)
			rtc->wake_alarm();
		else
			dev_warn(&pdev->dev,
				 "Wake Alarm IRQ just occurred!\n");
	}

	 
	if ((ctrl4b & RTC_CTRL_4B_RIE) && (ctrl4a & RTC_CTRL_4A_RF)) {
		rtc->write(rtc, RTC_EXT_CTRL_4A,
			   (ctrl4a & ~(RTC_CTRL_4A_RF)));
		msleep(150);

		 
		if (rtc->post_ram_clear != NULL)
			rtc->post_ram_clear();
		else
			dev_warn(&pdev->dev,
				 "RAM-Clear IRQ just occurred!\n");
	}
	ds1685_rtc_switch_to_bank0(rtc);
}

 
static irqreturn_t
ds1685_rtc_irq_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct ds1685_priv *rtc = platform_get_drvdata(pdev);
	u8 ctrlb, ctrlc;
	unsigned long events = 0;
	u8 num_irqs = 0;

	 
	if (unlikely(!rtc))
		return IRQ_HANDLED;

	rtc_lock(rtc->dev);

	 
	ctrlb = rtc->read(rtc, RTC_CTRL_B);
	ctrlc = rtc->read(rtc, RTC_CTRL_C);

	 
	if (likely(ctrlc & RTC_CTRL_C_IRQF)) {
		 
		if (likely(ctrlc & RTC_CTRL_B_PAU_MASK)) {
			events = RTC_IRQF;

			 
			if ((ctrlb & RTC_CTRL_B_PIE) &&
			    (ctrlc & RTC_CTRL_C_PF)) {
				events |= RTC_PF;
				num_irqs++;
			}

			 
			if ((ctrlb & RTC_CTRL_B_AIE) &&
			    (ctrlc & RTC_CTRL_C_AF)) {
				events |= RTC_AF;
				num_irqs++;
			}

			 
			if ((ctrlb & RTC_CTRL_B_UIE) &&
			    (ctrlc & RTC_CTRL_C_UF)) {
				events |= RTC_UF;
				num_irqs++;
			}
		} else {
			 
			ds1685_rtc_extended_irq(rtc, pdev);
		}
	}
	rtc_update_irq(rtc->dev, num_irqs, events);
	rtc_unlock(rtc->dev);

	return events ? IRQ_HANDLED : IRQ_NONE;
}
 


 
 

#ifdef CONFIG_PROC_FS
#define NUM_REGS	6	 
#define NUM_BITS	8	 
#define NUM_SPACES	4	 

 
static const char *ds1685_rtc_pirq_rate[16] = {
	"none", "3.90625ms", "7.8125ms", "0.122070ms", "0.244141ms",
	"0.488281ms", "0.9765625ms", "1.953125ms", "3.90625ms", "7.8125ms",
	"15.625ms", "31.25ms", "62.5ms", "125ms", "250ms", "500ms"
};

 
static const char *ds1685_rtc_sqw_freq[16] = {
	"none", "256Hz", "128Hz", "8192Hz", "4096Hz", "2048Hz", "1024Hz",
	"512Hz", "256Hz", "128Hz", "64Hz", "32Hz", "16Hz", "8Hz", "4Hz", "2Hz"
};

 
static int
ds1685_rtc_proc(struct device *dev, struct seq_file *seq)
{
	struct ds1685_priv *rtc = dev_get_drvdata(dev);
	u8 ctrla, ctrlb, ctrld, ctrl4a, ctrl4b, ssn[8];
	char *model;

	 
	ds1685_rtc_switch_to_bank1(rtc);
	ds1685_rtc_get_ssn(rtc, ssn);
	ctrla = rtc->read(rtc, RTC_CTRL_A);
	ctrlb = rtc->read(rtc, RTC_CTRL_B);
	ctrld = rtc->read(rtc, RTC_CTRL_D);
	ctrl4a = rtc->read(rtc, RTC_EXT_CTRL_4A);
	ctrl4b = rtc->read(rtc, RTC_EXT_CTRL_4B);
	ds1685_rtc_switch_to_bank0(rtc);

	 
	switch (ssn[0]) {
	case RTC_MODEL_DS1685:
		model = "DS1685/DS1687\0";
		break;
	case RTC_MODEL_DS1689:
		model = "DS1689/DS1693\0";
		break;
	case RTC_MODEL_DS17285:
		model = "DS17285/DS17287\0";
		break;
	case RTC_MODEL_DS17485:
		model = "DS17485/DS17487\0";
		break;
	case RTC_MODEL_DS17885:
		model = "DS17885/DS17887\0";
		break;
	default:
		model = "Unknown\0";
		break;
	}

	 
	seq_printf(seq,
	   "Model\t\t: %s\n"
	   "Oscillator\t: %s\n"
	   "12/24hr\t\t: %s\n"
	   "DST\t\t: %s\n"
	   "Data mode\t: %s\n"
	   "Battery\t\t: %s\n"
	   "Aux batt\t: %s\n"
	   "Update IRQ\t: %s\n"
	   "Periodic IRQ\t: %s\n"
	   "Periodic Rate\t: %s\n"
	   "SQW Freq\t: %s\n"
	   "Serial #\t: %8phC\n",
	   model,
	   ((ctrla & RTC_CTRL_A_DV1) ? "enabled" : "disabled"),
	   ((ctrlb & RTC_CTRL_B_2412) ? "24-hour" : "12-hour"),
	   ((ctrlb & RTC_CTRL_B_DSE) ? "enabled" : "disabled"),
	   ((ctrlb & RTC_CTRL_B_DM) ? "binary" : "BCD"),
	   ((ctrld & RTC_CTRL_D_VRT) ? "ok" : "exhausted or n/a"),
	   ((ctrl4a & RTC_CTRL_4A_VRT2) ? "ok" : "exhausted or n/a"),
	   ((ctrlb & RTC_CTRL_B_UIE) ? "yes" : "no"),
	   ((ctrlb & RTC_CTRL_B_PIE) ? "yes" : "no"),
	   (!(ctrl4b & RTC_CTRL_4B_E32K) ?
	    ds1685_rtc_pirq_rate[(ctrla & RTC_CTRL_A_RS_MASK)] : "none"),
	   (!((ctrl4b & RTC_CTRL_4B_E32K)) ?
	    ds1685_rtc_sqw_freq[(ctrla & RTC_CTRL_A_RS_MASK)] : "32768Hz"),
	   ssn);
	return 0;
}
#else
#define ds1685_rtc_proc NULL
#endif  
 


 
 

static const struct rtc_class_ops
ds1685_rtc_ops = {
	.proc = ds1685_rtc_proc,
	.read_time = ds1685_rtc_read_time,
	.set_time = ds1685_rtc_set_time,
	.read_alarm = ds1685_rtc_read_alarm,
	.set_alarm = ds1685_rtc_set_alarm,
	.alarm_irq_enable = ds1685_rtc_alarm_irq_enable,
};
 

static int ds1685_nvram_read(void *priv, unsigned int pos, void *val,
			     size_t size)
{
	struct ds1685_priv *rtc = priv;
	struct mutex *rtc_mutex = &rtc->dev->ops_lock;
	ssize_t count;
	u8 *buf = val;
	int err;

	err = mutex_lock_interruptible(rtc_mutex);
	if (err)
		return err;

	ds1685_rtc_switch_to_bank0(rtc);

	 
	for (count = 0; size > 0 && pos < NVRAM_TOTAL_SZ_BANK0;
	     count++, size--) {
		if (count < NVRAM_SZ_TIME)
			*buf++ = rtc->read(rtc, (NVRAM_TIME_BASE + pos++));
		else
			*buf++ = rtc->read(rtc, (NVRAM_BANK0_BASE + pos++));
	}

#ifndef CONFIG_RTC_DRV_DS1689
	if (size > 0) {
		ds1685_rtc_switch_to_bank1(rtc);

#ifndef CONFIG_RTC_DRV_DS1685
		 
		rtc->write(rtc, RTC_EXT_CTRL_4A,
			   (rtc->read(rtc, RTC_EXT_CTRL_4A) |
			    RTC_CTRL_4A_BME));

		 
		rtc->write(rtc, RTC_BANK1_RAM_ADDR_LSB,
			   (pos - NVRAM_TOTAL_SZ_BANK0));
#endif

		 
		for (count = 0; size > 0 && pos < NVRAM_TOTAL_SZ;
		     count++, size--) {
#ifdef CONFIG_RTC_DRV_DS1685
			 
			rtc->write(rtc, RTC_BANK1_RAM_ADDR,
				   (pos - NVRAM_TOTAL_SZ_BANK0));
#endif
			*buf++ = rtc->read(rtc, RTC_BANK1_RAM_DATA_PORT);
			pos++;
		}

#ifndef CONFIG_RTC_DRV_DS1685
		 
		rtc->write(rtc, RTC_EXT_CTRL_4A,
			   (rtc->read(rtc, RTC_EXT_CTRL_4A) &
			    ~(RTC_CTRL_4A_BME)));
#endif
		ds1685_rtc_switch_to_bank0(rtc);
	}
#endif  
	mutex_unlock(rtc_mutex);

	return 0;
}

static int ds1685_nvram_write(void *priv, unsigned int pos, void *val,
			      size_t size)
{
	struct ds1685_priv *rtc = priv;
	struct mutex *rtc_mutex = &rtc->dev->ops_lock;
	ssize_t count;
	u8 *buf = val;
	int err;

	err = mutex_lock_interruptible(rtc_mutex);
	if (err)
		return err;

	ds1685_rtc_switch_to_bank0(rtc);

	 
	for (count = 0; size > 0 && pos < NVRAM_TOTAL_SZ_BANK0;
	     count++, size--)
		if (count < NVRAM_SZ_TIME)
			rtc->write(rtc, (NVRAM_TIME_BASE + pos++),
				   *buf++);
		else
			rtc->write(rtc, (NVRAM_BANK0_BASE), *buf++);

#ifndef CONFIG_RTC_DRV_DS1689
	if (size > 0) {
		ds1685_rtc_switch_to_bank1(rtc);

#ifndef CONFIG_RTC_DRV_DS1685
		 
		rtc->write(rtc, RTC_EXT_CTRL_4A,
			   (rtc->read(rtc, RTC_EXT_CTRL_4A) |
			    RTC_CTRL_4A_BME));

		 
		rtc->write(rtc, RTC_BANK1_RAM_ADDR_LSB,
			   (pos - NVRAM_TOTAL_SZ_BANK0));
#endif

		 
		for (count = 0; size > 0 && pos < NVRAM_TOTAL_SZ;
		     count++, size--) {
#ifdef CONFIG_RTC_DRV_DS1685
			 
			rtc->write(rtc, RTC_BANK1_RAM_ADDR,
				   (pos - NVRAM_TOTAL_SZ_BANK0));
#endif
			rtc->write(rtc, RTC_BANK1_RAM_DATA_PORT, *buf++);
			pos++;
		}

#ifndef CONFIG_RTC_DRV_DS1685
		 
		rtc->write(rtc, RTC_EXT_CTRL_4A,
			   (rtc->read(rtc, RTC_EXT_CTRL_4A) &
			    ~(RTC_CTRL_4A_BME)));
#endif
		ds1685_rtc_switch_to_bank0(rtc);
	}
#endif  
	mutex_unlock(rtc_mutex);

	return 0;
}

 
 

 
static ssize_t
ds1685_rtc_sysfs_battery_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct ds1685_priv *rtc = dev_get_drvdata(dev->parent);
	u8 ctrld;

	ctrld = rtc->read(rtc, RTC_CTRL_D);

	return sprintf(buf, "%s\n",
			(ctrld & RTC_CTRL_D_VRT) ? "ok" : "not ok or N/A");
}
static DEVICE_ATTR(battery, S_IRUGO, ds1685_rtc_sysfs_battery_show, NULL);

 
static ssize_t
ds1685_rtc_sysfs_auxbatt_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct ds1685_priv *rtc = dev_get_drvdata(dev->parent);
	u8 ctrl4a;

	ds1685_rtc_switch_to_bank1(rtc);
	ctrl4a = rtc->read(rtc, RTC_EXT_CTRL_4A);
	ds1685_rtc_switch_to_bank0(rtc);

	return sprintf(buf, "%s\n",
			(ctrl4a & RTC_CTRL_4A_VRT2) ? "ok" : "not ok or N/A");
}
static DEVICE_ATTR(auxbatt, S_IRUGO, ds1685_rtc_sysfs_auxbatt_show, NULL);

 
static ssize_t
ds1685_rtc_sysfs_serial_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct ds1685_priv *rtc = dev_get_drvdata(dev->parent);
	u8 ssn[8];

	ds1685_rtc_switch_to_bank1(rtc);
	ds1685_rtc_get_ssn(rtc, ssn);
	ds1685_rtc_switch_to_bank0(rtc);

	return sprintf(buf, "%8phC\n", ssn);
}
static DEVICE_ATTR(serial, S_IRUGO, ds1685_rtc_sysfs_serial_show, NULL);

 
static struct attribute*
ds1685_rtc_sysfs_misc_attrs[] = {
	&dev_attr_battery.attr,
	&dev_attr_auxbatt.attr,
	&dev_attr_serial.attr,
	NULL,
};

 
static const struct attribute_group
ds1685_rtc_sysfs_misc_grp = {
	.name = "misc",
	.attrs = ds1685_rtc_sysfs_misc_attrs,
};

 
 

 
static int
ds1685_rtc_probe(struct platform_device *pdev)
{
	struct rtc_device *rtc_dev;
	struct ds1685_priv *rtc;
	struct ds1685_rtc_platform_data *pdata;
	u8 ctrla, ctrlb, hours;
	unsigned char am_pm;
	int ret = 0;
	struct nvmem_config nvmem_cfg = {
		.name = "ds1685_nvram",
		.size = NVRAM_TOTAL_SZ,
		.reg_read = ds1685_nvram_read,
		.reg_write = ds1685_nvram_write,
	};

	 
	pdata = (struct ds1685_rtc_platform_data *) pdev->dev.platform_data;
	if (!pdata)
		return -ENODEV;

	 
	rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	 
	switch (pdata->access_type) {
	case ds1685_reg_direct:
		rtc->regs = devm_platform_ioremap_resource(pdev, 0);
		if (IS_ERR(rtc->regs))
			return PTR_ERR(rtc->regs);
		rtc->read = ds1685_read;
		rtc->write = ds1685_write;
		break;
	case ds1685_reg_indirect:
		rtc->regs = devm_platform_ioremap_resource(pdev, 0);
		if (IS_ERR(rtc->regs))
			return PTR_ERR(rtc->regs);
		rtc->data = devm_platform_ioremap_resource(pdev, 1);
		if (IS_ERR(rtc->data))
			return PTR_ERR(rtc->data);
		rtc->read = ds1685_indirect_read;
		rtc->write = ds1685_indirect_write;
		break;
	}

	if (!rtc->read || !rtc->write)
		return -ENXIO;

	 
	if (pdata->regstep > 0)
		rtc->regstep = pdata->regstep;
	else
		rtc->regstep = 1;

	 
	if (pdata->plat_prepare_poweroff)
		rtc->prepare_poweroff = pdata->plat_prepare_poweroff;

	 
	if (pdata->plat_wake_alarm)
		rtc->wake_alarm = pdata->plat_wake_alarm;

	 
	if (pdata->plat_post_ram_clear)
		rtc->post_ram_clear = pdata->plat_post_ram_clear;

	 
	platform_set_drvdata(pdev, rtc);

	 
	ctrla = rtc->read(rtc, RTC_CTRL_A);
	if (!(ctrla & RTC_CTRL_A_DV1))
		ctrla |= RTC_CTRL_A_DV1;

	 
	ctrla &= ~(RTC_CTRL_A_DV2);

	 
	ctrla &= ~(RTC_CTRL_A_RS_MASK);

	 
	ctrla |= RTC_CTRL_A_DV0;
	rtc->write(rtc, RTC_CTRL_A, ctrla);

	 
	rtc->write(rtc, RTC_EXT_CTRL_4B,
		   (rtc->read(rtc, RTC_EXT_CTRL_4B) | RTC_CTRL_4B_E32K));

	 
	rtc->write(rtc, RTC_CTRL_B,
		   (rtc->read(rtc, RTC_CTRL_B) | RTC_CTRL_B_SET));

	 
	while (rtc->read(rtc, RTC_EXT_CTRL_4A) & RTC_CTRL_4A_INCR)
		cpu_relax();

	 
	ctrlb = rtc->read(rtc, RTC_CTRL_B);
	if (pdata->bcd_mode)
		ctrlb &= ~(RTC_CTRL_B_DM);
	else
		ctrlb |= RTC_CTRL_B_DM;
	rtc->bcd_mode = pdata->bcd_mode;

	 
	if (ctrlb & RTC_CTRL_B_DSE)
		ctrlb &= ~(RTC_CTRL_B_DSE);

	 
	if (!(ctrlb & RTC_CTRL_B_2412)) {
		 
		hours = rtc->read(rtc, RTC_HRS);
		am_pm = hours & RTC_HRS_AMPM_MASK;
		hours = ds1685_rtc_bcd2bin(rtc, hours, RTC_HRS_12_BCD_MASK,
					   RTC_HRS_12_BIN_MASK);
		hours = ((hours == 12) ? 0 : ((am_pm) ? hours + 12 : hours));

		 
		ctrlb |= RTC_CTRL_B_2412;

		 
		rtc->write(rtc, RTC_CTRL_B, ctrlb);

		 
		rtc->write(rtc, RTC_HRS,
			   ds1685_rtc_bin2bcd(rtc, hours,
					      RTC_HRS_24_BIN_MASK,
					      RTC_HRS_24_BCD_MASK));

		 
		hours = rtc->read(rtc, RTC_HRS_ALARM);
		am_pm = hours & RTC_HRS_AMPM_MASK;
		hours = ds1685_rtc_bcd2bin(rtc, hours, RTC_HRS_12_BCD_MASK,
					   RTC_HRS_12_BIN_MASK);
		hours = ((hours == 12) ? 0 : ((am_pm) ? hours + 12 : hours));

		 
		rtc->write(rtc, RTC_HRS_ALARM,
			   ds1685_rtc_bin2bcd(rtc, hours,
					      RTC_HRS_24_BIN_MASK,
					      RTC_HRS_24_BCD_MASK));
	} else {
		 
		rtc->write(rtc, RTC_CTRL_B, ctrlb);
	}

	 
	rtc->write(rtc, RTC_CTRL_B,
		   (rtc->read(rtc, RTC_CTRL_B) & ~(RTC_CTRL_B_SET)));

	 
	if (!(rtc->read(rtc, RTC_CTRL_D) & RTC_CTRL_D_VRT))
		dev_warn(&pdev->dev,
			 "Main battery is exhausted! RTC may be invalid!\n");

	 
	if (!(rtc->read(rtc, RTC_EXT_CTRL_4A) & RTC_CTRL_4A_VRT2))
		dev_warn(&pdev->dev,
			 "Aux battery is exhausted or not available.\n");

	 
	rtc->write(rtc, RTC_CTRL_B,
		   (rtc->read(rtc, RTC_CTRL_B) & ~(RTC_CTRL_B_PAU_MASK)));

	 
	rtc->read(rtc, RTC_CTRL_C);

	 
	rtc->write(rtc, RTC_EXT_CTRL_4B,
		   (rtc->read(rtc, RTC_EXT_CTRL_4B) & ~(RTC_CTRL_4B_RWK_MASK)));

	 
	rtc->write(rtc, RTC_EXT_CTRL_4A,
		   (rtc->read(rtc, RTC_EXT_CTRL_4A) & ~(RTC_CTRL_4A_RWK_MASK)));

	 
	rtc->write(rtc, RTC_EXT_CTRL_4B,
		   (rtc->read(rtc, RTC_EXT_CTRL_4B) | RTC_CTRL_4B_KSE));

	rtc_dev = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(rtc_dev))
		return PTR_ERR(rtc_dev);

	rtc_dev->ops = &ds1685_rtc_ops;

	 
	rtc_dev->range_min = RTC_TIMESTAMP_BEGIN_2000;
	rtc_dev->range_max = RTC_TIMESTAMP_END_2099;

	 
	rtc_dev->max_user_freq = RTC_MAX_USER_FREQ;

	 
	if (pdata->uie_unsupported)
		clear_bit(RTC_FEATURE_UPDATE_INTERRUPT, rtc_dev->features);

	rtc->dev = rtc_dev;

	 
	rtc->irq_num = platform_get_irq(pdev, 0);
	if (rtc->irq_num <= 0) {
		clear_bit(RTC_FEATURE_ALARM, rtc_dev->features);
	} else {
		 
		ret = devm_request_threaded_irq(&pdev->dev, rtc->irq_num,
				       NULL, ds1685_rtc_irq_handler,
				       IRQF_SHARED | IRQF_ONESHOT,
				       pdev->name, pdev);

		 
		if (unlikely(ret)) {
			dev_warn(&pdev->dev,
				 "RTC interrupt not available\n");
			rtc->irq_num = 0;
		}
	}

	 
	ds1685_rtc_switch_to_bank0(rtc);

	ret = rtc_add_group(rtc_dev, &ds1685_rtc_sysfs_misc_grp);
	if (ret)
		return ret;

	nvmem_cfg.priv = rtc;
	ret = devm_rtc_nvmem_register(rtc_dev, &nvmem_cfg);
	if (ret)
		return ret;

	return devm_rtc_register_device(rtc_dev);
}

 
static void
ds1685_rtc_remove(struct platform_device *pdev)
{
	struct ds1685_priv *rtc = platform_get_drvdata(pdev);

	 
	rtc->write(rtc, RTC_CTRL_B,
		   (rtc->read(rtc, RTC_CTRL_B) &
		    ~(RTC_CTRL_B_PAU_MASK)));

	 
	rtc->read(rtc, RTC_CTRL_C);

	 
	rtc->write(rtc, RTC_EXT_CTRL_4B,
		   (rtc->read(rtc, RTC_EXT_CTRL_4B) &
		    ~(RTC_CTRL_4B_RWK_MASK)));

	 
	rtc->write(rtc, RTC_EXT_CTRL_4A,
		   (rtc->read(rtc, RTC_EXT_CTRL_4A) &
		    ~(RTC_CTRL_4A_RWK_MASK)));
}

 
static struct platform_driver ds1685_rtc_driver = {
	.driver		= {
		.name	= "rtc-ds1685",
	},
	.probe		= ds1685_rtc_probe,
	.remove_new	= ds1685_rtc_remove,
};
module_platform_driver(ds1685_rtc_driver);
 


 
 

 
void __noreturn
ds1685_rtc_poweroff(struct platform_device *pdev)
{
	u8 ctrla, ctrl4a, ctrl4b;
	struct ds1685_priv *rtc;

	 
	if (unlikely(!pdev)) {
		pr_emerg("platform device data not available, spinning forever ...\n");
		while(1);
		unreachable();
	} else {
		 
		rtc = platform_get_drvdata(pdev);

		 
		if (rtc->irq_num)
			disable_irq_nosync(rtc->irq_num);

		 
		ctrla = rtc->read(rtc, RTC_CTRL_A);
		ctrla |= RTC_CTRL_A_DV1;
		ctrla &= ~(RTC_CTRL_A_DV2);
		rtc->write(rtc, RTC_CTRL_A, ctrla);

		 
		ds1685_rtc_switch_to_bank1(rtc);
		ctrl4a = rtc->read(rtc, RTC_EXT_CTRL_4A);
		if (ctrl4a & RTC_CTRL_4A_VRT2) {
			 
			ctrl4a &= ~(RTC_CTRL_4A_RWK_MASK);
			rtc->write(rtc, RTC_EXT_CTRL_4A, ctrl4a);

			 
			ctrl4b = rtc->read(rtc, RTC_EXT_CTRL_4B);
			ctrl4b |= (RTC_CTRL_4B_ABE | RTC_CTRL_4B_WIE |
				   RTC_CTRL_4B_KSE);
			rtc->write(rtc, RTC_EXT_CTRL_4B, ctrl4b);
		}

		 
		dev_warn(&pdev->dev, "Powerdown.\n");
		msleep(20);
		rtc->write(rtc, RTC_EXT_CTRL_4A,
			   (ctrl4a | RTC_CTRL_4A_PAB));

		 
		while(1);
		unreachable();
	}
}
EXPORT_SYMBOL_GPL(ds1685_rtc_poweroff);
 


MODULE_AUTHOR("Joshua Kinard <kumba@gentoo.org>");
MODULE_AUTHOR("Matthias Fuchs <matthias.fuchs@esd-electronics.com>");
MODULE_DESCRIPTION("Dallas/Maxim DS1685/DS1687-series RTC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtc-ds1685");
