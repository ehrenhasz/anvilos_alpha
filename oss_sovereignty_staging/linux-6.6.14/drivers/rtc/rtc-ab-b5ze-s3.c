
 

#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>

#define DRV_NAME "rtc-ab-b5ze-s3"

 
#define ABB5ZES3_REG_CTRL1	   0x00	    
#define ABB5ZES3_REG_CTRL1_CIE	   BIT(0)   
#define ABB5ZES3_REG_CTRL1_AIE	   BIT(1)   
#define ABB5ZES3_REG_CTRL1_SIE	   BIT(2)   
#define ABB5ZES3_REG_CTRL1_PM	   BIT(3)   
#define ABB5ZES3_REG_CTRL1_SR	   BIT(4)   
#define ABB5ZES3_REG_CTRL1_STOP	   BIT(5)   
#define ABB5ZES3_REG_CTRL1_CAP	   BIT(7)

#define ABB5ZES3_REG_CTRL2	   0x01	    
#define ABB5ZES3_REG_CTRL2_CTBIE   BIT(0)   
#define ABB5ZES3_REG_CTRL2_CTAIE   BIT(1)   
#define ABB5ZES3_REG_CTRL2_WTAIE   BIT(2)   
#define ABB5ZES3_REG_CTRL2_AF	   BIT(3)   
#define ABB5ZES3_REG_CTRL2_SF	   BIT(4)   
#define ABB5ZES3_REG_CTRL2_CTBF	   BIT(5)   
#define ABB5ZES3_REG_CTRL2_CTAF	   BIT(6)   
#define ABB5ZES3_REG_CTRL2_WTAF	   BIT(7)   

#define ABB5ZES3_REG_CTRL3	   0x02	    
#define ABB5ZES3_REG_CTRL3_PM2	   BIT(7)   
#define ABB5ZES3_REG_CTRL3_PM1	   BIT(6)   
#define ABB5ZES3_REG_CTRL3_PM0	   BIT(5)   
#define ABB5ZES3_REG_CTRL3_BSF	   BIT(3)   
#define ABB5ZES3_REG_CTRL3_BLF	   BIT(2)   
#define ABB5ZES3_REG_CTRL3_BSIE	   BIT(1)   
#define ABB5ZES3_REG_CTRL3_BLIE	   BIT(0)   

#define ABB5ZES3_CTRL_SEC_LEN	   3

 
#define ABB5ZES3_REG_RTC_SC	   0x03	    
#define ABB5ZES3_REG_RTC_SC_OSC	   BIT(7)   
#define ABB5ZES3_REG_RTC_MN	   0x04	    
#define ABB5ZES3_REG_RTC_HR	   0x05	    
#define ABB5ZES3_REG_RTC_HR_PM	   BIT(5)   
#define ABB5ZES3_REG_RTC_DT	   0x06	    
#define ABB5ZES3_REG_RTC_DW	   0x07	    
#define ABB5ZES3_REG_RTC_MO	   0x08	    
#define ABB5ZES3_REG_RTC_YR	   0x09	    

#define ABB5ZES3_RTC_SEC_LEN	   7

 
#define ABB5ZES3_REG_ALRM_MN	   0x0A	    
#define ABB5ZES3_REG_ALRM_MN_AE	   BIT(7)   
#define ABB5ZES3_REG_ALRM_HR	   0x0B	    
#define ABB5ZES3_REG_ALRM_HR_AE	   BIT(7)   
#define ABB5ZES3_REG_ALRM_DT	   0x0C	    
#define ABB5ZES3_REG_ALRM_DT_AE	   BIT(7)   
#define ABB5ZES3_REG_ALRM_DW	   0x0D	    
#define ABB5ZES3_REG_ALRM_DW_AE	   BIT(7)   

#define ABB5ZES3_ALRM_SEC_LEN	   4

 
#define ABB5ZES3_REG_FREQ_OF	   0x0E	    
#define ABB5ZES3_REG_FREQ_OF_MODE  0x0E	    

 
#define ABB5ZES3_REG_TIM_CLK	   0x0F	    
#define ABB5ZES3_REG_TIM_CLK_TAM   BIT(7)   
#define ABB5ZES3_REG_TIM_CLK_TBM   BIT(6)   
#define ABB5ZES3_REG_TIM_CLK_COF2  BIT(5)   
#define ABB5ZES3_REG_TIM_CLK_COF1  BIT(4)   
#define ABB5ZES3_REG_TIM_CLK_COF0  BIT(3)   
#define ABB5ZES3_REG_TIM_CLK_TAC1  BIT(2)   
#define ABB5ZES3_REG_TIM_CLK_TAC0  BIT(1)   
#define ABB5ZES3_REG_TIM_CLK_TBC   BIT(0)   

 
#define ABB5ZES3_REG_TIMA_CLK	   0x10	    
#define ABB5ZES3_REG_TIMA_CLK_TAQ2 BIT(2)   
#define ABB5ZES3_REG_TIMA_CLK_TAQ1 BIT(1)   
#define ABB5ZES3_REG_TIMA_CLK_TAQ0 BIT(0)   
#define ABB5ZES3_REG_TIMA	   0x11	    

#define ABB5ZES3_TIMA_SEC_LEN	   2

 
#define ABB5ZES3_REG_TIMB_CLK	   0x12	    
#define ABB5ZES3_REG_TIMB_CLK_TBW2 BIT(6)
#define ABB5ZES3_REG_TIMB_CLK_TBW1 BIT(5)
#define ABB5ZES3_REG_TIMB_CLK_TBW0 BIT(4)
#define ABB5ZES3_REG_TIMB_CLK_TAQ2 BIT(2)
#define ABB5ZES3_REG_TIMB_CLK_TAQ1 BIT(1)
#define ABB5ZES3_REG_TIMB_CLK_TAQ0 BIT(0)
#define ABB5ZES3_REG_TIMB	   0x13	    
#define ABB5ZES3_TIMB_SEC_LEN	   2

#define ABB5ZES3_MEM_MAP_LEN	   0x14

struct abb5zes3_rtc_data {
	struct rtc_device *rtc;
	struct regmap *regmap;

	int irq;

	bool battery_low;
	bool timer_alarm;  
};

 
static int abb5zes3_i2c_validate_chip(struct regmap *regmap)
{
	u8 regs[ABB5ZES3_MEM_MAP_LEN];
	static const u8 mask[ABB5ZES3_MEM_MAP_LEN] = { 0x00, 0x00, 0x10, 0x00,
						       0x80, 0xc0, 0xc0, 0xf8,
						       0xe0, 0x00, 0x00, 0x40,
						       0x40, 0x78, 0x00, 0x00,
						       0xf8, 0x00, 0x88, 0x00 };
	int ret, i;

	ret = regmap_bulk_read(regmap, 0, regs, ABB5ZES3_MEM_MAP_LEN);
	if (ret)
		return ret;

	for (i = 0; i < ABB5ZES3_MEM_MAP_LEN; ++i) {
		if (regs[i] & mask[i])  
			return -ENODEV;
	}

	return 0;
}

 
static int _abb5zes3_rtc_clear_alarm(struct device *dev)
{
	struct abb5zes3_rtc_data *data = dev_get_drvdata(dev);
	int ret;

	ret = regmap_update_bits(data->regmap, ABB5ZES3_REG_CTRL2,
				 ABB5ZES3_REG_CTRL2_AF, 0);
	if (ret)
		dev_err(dev, "%s: clearing alarm failed (%d)\n", __func__, ret);

	return ret;
}

 
static int _abb5zes3_rtc_update_alarm(struct device *dev, bool enable)
{
	struct abb5zes3_rtc_data *data = dev_get_drvdata(dev);
	int ret;

	ret = regmap_update_bits(data->regmap, ABB5ZES3_REG_CTRL1,
				 ABB5ZES3_REG_CTRL1_AIE,
				 enable ? ABB5ZES3_REG_CTRL1_AIE : 0);
	if (ret)
		dev_err(dev, "%s: writing alarm INT failed (%d)\n",
			__func__, ret);

	return ret;
}

 
static int _abb5zes3_rtc_update_timer(struct device *dev, bool enable)
{
	struct abb5zes3_rtc_data *data = dev_get_drvdata(dev);
	int ret;

	ret = regmap_update_bits(data->regmap, ABB5ZES3_REG_CTRL2,
				 ABB5ZES3_REG_CTRL2_WTAIE,
				 enable ? ABB5ZES3_REG_CTRL2_WTAIE : 0);
	if (ret)
		dev_err(dev, "%s: writing timer INT failed (%d)\n",
			__func__, ret);

	return ret;
}

 
static int _abb5zes3_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct abb5zes3_rtc_data *data = dev_get_drvdata(dev);
	u8 regs[ABB5ZES3_REG_RTC_SC + ABB5ZES3_RTC_SEC_LEN];
	int ret = 0;

	 
	ret = regmap_bulk_read(data->regmap, ABB5ZES3_REG_CTRL1, regs,
			       sizeof(regs));
	if (ret) {
		dev_err(dev, "%s: reading RTC time failed (%d)\n",
			__func__, ret);
		return ret;
	}

	 
	if (regs[ABB5ZES3_REG_RTC_SC] & ABB5ZES3_REG_RTC_SC_OSC)
		return -ENODATA;

	tm->tm_sec = bcd2bin(regs[ABB5ZES3_REG_RTC_SC] & 0x7F);
	tm->tm_min = bcd2bin(regs[ABB5ZES3_REG_RTC_MN]);

	if (regs[ABB5ZES3_REG_CTRL1] & ABB5ZES3_REG_CTRL1_PM) {  
		tm->tm_hour = bcd2bin(regs[ABB5ZES3_REG_RTC_HR] & 0x1f);
		if (regs[ABB5ZES3_REG_RTC_HR] & ABB5ZES3_REG_RTC_HR_PM)  
			tm->tm_hour += 12;
	} else {						 
		tm->tm_hour = bcd2bin(regs[ABB5ZES3_REG_RTC_HR]);
	}

	tm->tm_mday = bcd2bin(regs[ABB5ZES3_REG_RTC_DT]);
	tm->tm_wday = bcd2bin(regs[ABB5ZES3_REG_RTC_DW]);
	tm->tm_mon  = bcd2bin(regs[ABB5ZES3_REG_RTC_MO]) - 1;  
	tm->tm_year = bcd2bin(regs[ABB5ZES3_REG_RTC_YR]) + 100;

	return ret;
}

static int abb5zes3_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct abb5zes3_rtc_data *data = dev_get_drvdata(dev);
	u8 regs[ABB5ZES3_REG_RTC_SC + ABB5ZES3_RTC_SEC_LEN];
	int ret;

	regs[ABB5ZES3_REG_RTC_SC] = bin2bcd(tm->tm_sec);  
	regs[ABB5ZES3_REG_RTC_MN] = bin2bcd(tm->tm_min);
	regs[ABB5ZES3_REG_RTC_HR] = bin2bcd(tm->tm_hour);  
	regs[ABB5ZES3_REG_RTC_DT] = bin2bcd(tm->tm_mday);
	regs[ABB5ZES3_REG_RTC_DW] = bin2bcd(tm->tm_wday);
	regs[ABB5ZES3_REG_RTC_MO] = bin2bcd(tm->tm_mon + 1);
	regs[ABB5ZES3_REG_RTC_YR] = bin2bcd(tm->tm_year - 100);

	ret = regmap_bulk_write(data->regmap, ABB5ZES3_REG_RTC_SC,
				regs + ABB5ZES3_REG_RTC_SC,
				ABB5ZES3_RTC_SEC_LEN);

	return ret;
}

 
static inline void sec_to_timer_a(u8 secs, u8 *taq, u8 *timer_a)
{
	*taq = ABB5ZES3_REG_TIMA_CLK_TAQ1;  
	*timer_a = secs;
}

 
static inline int sec_from_timer_a(u8 *secs, u8 taq, u8 timer_a)
{
	if (taq != ABB5ZES3_REG_TIMA_CLK_TAQ1)  
		return -EINVAL;

	*secs = timer_a;

	return 0;
}

 
static int _abb5zes3_rtc_read_timer(struct device *dev,
				    struct rtc_wkalrm *alarm)
{
	struct abb5zes3_rtc_data *data = dev_get_drvdata(dev);
	struct rtc_time rtc_tm, *alarm_tm = &alarm->time;
	u8 regs[ABB5ZES3_TIMA_SEC_LEN + 1];
	unsigned long rtc_secs;
	unsigned int reg;
	u8 timer_secs;
	int ret;

	 
	ret = regmap_bulk_read(data->regmap, ABB5ZES3_REG_TIM_CLK, regs,
			       ABB5ZES3_TIMA_SEC_LEN + 1);
	if (ret) {
		dev_err(dev, "%s: reading Timer A section failed (%d)\n",
			__func__, ret);
		return ret;
	}

	 
	ret = _abb5zes3_rtc_read_time(dev, &rtc_tm);
	if (ret)
		return ret;

	 
	rtc_secs = rtc_tm_to_time64(&rtc_tm);

	 
	ret = sec_from_timer_a(&timer_secs, regs[1], regs[2]);
	if (ret)
		return ret;

	 
	rtc_time64_to_tm(rtc_secs + timer_secs, alarm_tm);

	ret = regmap_read(data->regmap, ABB5ZES3_REG_CTRL2, &reg);
	if (ret) {
		dev_err(dev, "%s: reading ctrl reg failed (%d)\n",
			__func__, ret);
		return ret;
	}

	alarm->enabled = !!(reg & ABB5ZES3_REG_CTRL2_WTAIE);

	return 0;
}

 
static int _abb5zes3_rtc_read_alarm(struct device *dev,
				    struct rtc_wkalrm *alarm)
{
	struct abb5zes3_rtc_data *data = dev_get_drvdata(dev);
	struct rtc_time rtc_tm, *alarm_tm = &alarm->time;
	unsigned long rtc_secs, alarm_secs;
	u8 regs[ABB5ZES3_ALRM_SEC_LEN];
	unsigned int reg;
	int ret;

	ret = regmap_bulk_read(data->regmap, ABB5ZES3_REG_ALRM_MN, regs,
			       ABB5ZES3_ALRM_SEC_LEN);
	if (ret) {
		dev_err(dev, "%s: reading alarm section failed (%d)\n",
			__func__, ret);
		return ret;
	}

	alarm_tm->tm_sec  = 0;
	alarm_tm->tm_min  = bcd2bin(regs[0] & 0x7f);
	alarm_tm->tm_hour = bcd2bin(regs[1] & 0x3f);
	alarm_tm->tm_mday = bcd2bin(regs[2] & 0x3f);
	alarm_tm->tm_wday = -1;

	 
	ret = _abb5zes3_rtc_read_time(dev, &rtc_tm);
	if (ret)
		return ret;

	alarm_tm->tm_year = rtc_tm.tm_year;
	alarm_tm->tm_mon = rtc_tm.tm_mon;

	rtc_secs = rtc_tm_to_time64(&rtc_tm);
	alarm_secs = rtc_tm_to_time64(alarm_tm);

	if (alarm_secs < rtc_secs) {
		if (alarm_tm->tm_mon == 11) {
			alarm_tm->tm_mon = 0;
			alarm_tm->tm_year += 1;
		} else {
			alarm_tm->tm_mon += 1;
		}
	}

	ret = regmap_read(data->regmap, ABB5ZES3_REG_CTRL1, &reg);
	if (ret) {
		dev_err(dev, "%s: reading ctrl reg failed (%d)\n",
			__func__, ret);
		return ret;
	}

	alarm->enabled = !!(reg & ABB5ZES3_REG_CTRL1_AIE);

	return 0;
}

 
static int abb5zes3_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct abb5zes3_rtc_data *data = dev_get_drvdata(dev);
	int ret;

	if (data->timer_alarm)
		ret = _abb5zes3_rtc_read_timer(dev, alarm);
	else
		ret = _abb5zes3_rtc_read_alarm(dev, alarm);

	return ret;
}

 
static int _abb5zes3_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct abb5zes3_rtc_data *data = dev_get_drvdata(dev);
	struct rtc_time *alarm_tm = &alarm->time;
	u8 regs[ABB5ZES3_ALRM_SEC_LEN];
	struct rtc_time rtc_tm;
	int ret, enable = 1;

	if (!alarm->enabled) {
		enable = 0;
	} else {
		unsigned long rtc_secs, alarm_secs;

		 
		ret = _abb5zes3_rtc_read_time(dev, &rtc_tm);
		if (ret)
			return ret;

		if (rtc_tm.tm_mon == 11) {  
			rtc_tm.tm_mon = 0;
			rtc_tm.tm_year += 1;
		} else {
			rtc_tm.tm_mon += 1;
		}

		rtc_secs = rtc_tm_to_time64(&rtc_tm);
		alarm_secs = rtc_tm_to_time64(alarm_tm);

		if (alarm_secs > rtc_secs) {
			dev_err(dev, "%s: alarm maximum is one month in the future (%d)\n",
				__func__, ret);
			return -EINVAL;
		}
	}

	 
	regs[0] = bin2bcd(alarm_tm->tm_min) & 0x7f;
	regs[1] = bin2bcd(alarm_tm->tm_hour) & 0x3f;
	regs[2] = bin2bcd(alarm_tm->tm_mday) & 0x3f;
	regs[3] = ABB5ZES3_REG_ALRM_DW_AE;  

	ret = regmap_bulk_write(data->regmap, ABB5ZES3_REG_ALRM_MN, regs,
				ABB5ZES3_ALRM_SEC_LEN);
	if (ret < 0) {
		dev_err(dev, "%s: writing ALARM section failed (%d)\n",
			__func__, ret);
		return ret;
	}

	 
	data->timer_alarm = 0;

	 
	return _abb5zes3_rtc_update_alarm(dev, enable);
}

 
static int _abb5zes3_rtc_set_timer(struct device *dev, struct rtc_wkalrm *alarm,
				   u8 secs)
{
	struct abb5zes3_rtc_data *data = dev_get_drvdata(dev);
	u8 regs[ABB5ZES3_TIMA_SEC_LEN];
	u8 mask = ABB5ZES3_REG_TIM_CLK_TAC0 | ABB5ZES3_REG_TIM_CLK_TAC1;
	int ret = 0;

	 
	sec_to_timer_a(secs, &regs[0], &regs[1]);
	ret = regmap_bulk_write(data->regmap, ABB5ZES3_REG_TIMA_CLK, regs,
				ABB5ZES3_TIMA_SEC_LEN);
	if (ret < 0) {
		dev_err(dev, "%s: writing timer section failed\n", __func__);
		return ret;
	}

	 
	ret = regmap_update_bits(data->regmap, ABB5ZES3_REG_TIM_CLK,
				 mask, ABB5ZES3_REG_TIM_CLK_TAC1);
	if (ret)
		dev_err(dev, "%s: failed to update timer\n", __func__);

	 
	data->timer_alarm = 1;

	 
	return _abb5zes3_rtc_update_timer(dev, alarm->enabled);
}

 
static int abb5zes3_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct abb5zes3_rtc_data *data = dev_get_drvdata(dev);
	struct rtc_time *alarm_tm = &alarm->time;
	unsigned long rtc_secs, alarm_secs;
	struct rtc_time rtc_tm;
	int ret;

	ret = _abb5zes3_rtc_read_time(dev, &rtc_tm);
	if (ret)
		return ret;

	rtc_secs = rtc_tm_to_time64(&rtc_tm);
	alarm_secs = rtc_tm_to_time64(alarm_tm);

	 
	ret = _abb5zes3_rtc_update_alarm(dev, false);
	if (ret < 0) {
		dev_err(dev, "%s: unable to disable alarm (%d)\n", __func__,
			ret);
		return ret;
	}
	ret = _abb5zes3_rtc_update_timer(dev, false);
	if (ret < 0) {
		dev_err(dev, "%s: unable to disable timer (%d)\n", __func__,
			ret);
		return ret;
	}

	data->timer_alarm = 0;

	 
	if ((alarm_secs > rtc_secs) && ((alarm_secs - rtc_secs) <= 240))
		ret = _abb5zes3_rtc_set_timer(dev, alarm,
					      alarm_secs - rtc_secs);
	else
		ret = _abb5zes3_rtc_set_alarm(dev, alarm);

	if (ret)
		dev_err(dev, "%s: unable to configure alarm (%d)\n", __func__,
			ret);

	return ret;
}

 
static inline int _abb5zes3_rtc_battery_low_irq_enable(struct regmap *regmap,
						       bool enable)
{
	return regmap_update_bits(regmap, ABB5ZES3_REG_CTRL3,
				  ABB5ZES3_REG_CTRL3_BLIE,
				  enable ? ABB5ZES3_REG_CTRL3_BLIE : 0);
}

 
static int abb5zes3_rtc_check_setup(struct device *dev)
{
	struct abb5zes3_rtc_data *data = dev_get_drvdata(dev);
	struct regmap *regmap = data->regmap;
	unsigned int reg;
	int ret;
	u8 mask;

	 
	mask = (ABB5ZES3_REG_TIM_CLK_TBC | ABB5ZES3_REG_TIM_CLK_TAC0 |
		ABB5ZES3_REG_TIM_CLK_TAC1 | ABB5ZES3_REG_TIM_CLK_COF0 |
		ABB5ZES3_REG_TIM_CLK_COF1 | ABB5ZES3_REG_TIM_CLK_COF2 |
		ABB5ZES3_REG_TIM_CLK_TBM | ABB5ZES3_REG_TIM_CLK_TAM);
	ret = regmap_update_bits(regmap, ABB5ZES3_REG_TIM_CLK, mask,
				 ABB5ZES3_REG_TIM_CLK_COF0 |
				 ABB5ZES3_REG_TIM_CLK_COF1 |
				 ABB5ZES3_REG_TIM_CLK_COF2);
	if (ret < 0) {
		dev_err(dev, "%s: unable to initialize clkout register (%d)\n",
			__func__, ret);
		return ret;
	}

	 
	mask = (ABB5ZES3_REG_ALRM_MN_AE | ABB5ZES3_REG_ALRM_HR_AE |
		ABB5ZES3_REG_ALRM_DT_AE | ABB5ZES3_REG_ALRM_DW_AE);
	ret = regmap_update_bits(regmap, ABB5ZES3_REG_CTRL2, mask, mask);
	if (ret < 0) {
		dev_err(dev, "%s: unable to disable alarm setting (%d)\n",
			__func__, ret);
		return ret;
	}

	 
	mask = (ABB5ZES3_REG_CTRL1_CIE | ABB5ZES3_REG_CTRL1_AIE |
		ABB5ZES3_REG_CTRL1_SIE | ABB5ZES3_REG_CTRL1_PM |
		ABB5ZES3_REG_CTRL1_CAP | ABB5ZES3_REG_CTRL1_STOP);
	ret = regmap_update_bits(regmap, ABB5ZES3_REG_CTRL1, mask, 0);
	if (ret < 0) {
		dev_err(dev, "%s: unable to initialize CTRL1 register (%d)\n",
			__func__, ret);
		return ret;
	}

	 
	mask = (ABB5ZES3_REG_CTRL2_CTBIE | ABB5ZES3_REG_CTRL2_CTAIE |
		ABB5ZES3_REG_CTRL2_WTAIE | ABB5ZES3_REG_CTRL2_AF |
		ABB5ZES3_REG_CTRL2_SF | ABB5ZES3_REG_CTRL2_CTBF |
		ABB5ZES3_REG_CTRL2_CTAF);
	ret = regmap_update_bits(regmap, ABB5ZES3_REG_CTRL2, mask, 0);
	if (ret < 0) {
		dev_err(dev, "%s: unable to initialize CTRL2 register (%d)\n",
			__func__, ret);
		return ret;
	}

	 
	mask = (ABB5ZES3_REG_CTRL3_PM0  | ABB5ZES3_REG_CTRL3_PM1 |
		ABB5ZES3_REG_CTRL3_PM2  | ABB5ZES3_REG_CTRL3_BLIE |
		ABB5ZES3_REG_CTRL3_BSIE | ABB5ZES3_REG_CTRL3_BSF);
	ret = regmap_update_bits(regmap, ABB5ZES3_REG_CTRL3, mask, 0);
	if (ret < 0) {
		dev_err(dev, "%s: unable to initialize CTRL3 register (%d)\n",
			__func__, ret);
		return ret;
	}

	 
	ret = regmap_read(regmap, ABB5ZES3_REG_RTC_SC, &reg);
	if (ret < 0) {
		dev_err(dev, "%s: unable to read osc. integrity flag (%d)\n",
			__func__, ret);
		return ret;
	}

	if (reg & ABB5ZES3_REG_RTC_SC_OSC) {
		dev_err(dev, "clock integrity not guaranteed. Osc. has stopped or has been interrupted.\n");
		dev_err(dev, "change battery (if not already done) and then set time to reset osc. failure flag.\n");
	}

	 
	ret = regmap_read(regmap, ABB5ZES3_REG_CTRL3, &reg);
	if (ret < 0) {
		dev_err(dev, "%s: unable to read battery low flag (%d)\n",
			__func__, ret);
		return ret;
	}

	data->battery_low = reg & ABB5ZES3_REG_CTRL3_BLF;
	if (data->battery_low) {
		dev_err(dev, "RTC battery is low; please, consider changing it!\n");

		ret = _abb5zes3_rtc_battery_low_irq_enable(regmap, false);
		if (ret)
			dev_err(dev, "%s: disabling battery low interrupt generation failed (%d)\n",
				__func__, ret);
	}

	return ret;
}

static int abb5zes3_rtc_alarm_irq_enable(struct device *dev,
					 unsigned int enable)
{
	struct abb5zes3_rtc_data *rtc_data = dev_get_drvdata(dev);
	int ret = 0;

	if (rtc_data->irq) {
		if (rtc_data->timer_alarm)
			ret = _abb5zes3_rtc_update_timer(dev, enable);
		else
			ret = _abb5zes3_rtc_update_alarm(dev, enable);
	}

	return ret;
}

static irqreturn_t _abb5zes3_rtc_interrupt(int irq, void *data)
{
	struct i2c_client *client = data;
	struct device *dev = &client->dev;
	struct abb5zes3_rtc_data *rtc_data = dev_get_drvdata(dev);
	struct rtc_device *rtc = rtc_data->rtc;
	u8 regs[ABB5ZES3_CTRL_SEC_LEN];
	int ret, handled = IRQ_NONE;

	ret = regmap_bulk_read(rtc_data->regmap, 0, regs,
			       ABB5ZES3_CTRL_SEC_LEN);
	if (ret) {
		dev_err(dev, "%s: unable to read control section (%d)!\n",
			__func__, ret);
		return handled;
	}

	 
	if (regs[ABB5ZES3_REG_CTRL3] & ABB5ZES3_REG_CTRL3_BLF) {
		dev_err(dev, "RTC battery is low; please change it!\n");

		_abb5zes3_rtc_battery_low_irq_enable(rtc_data->regmap, false);

		handled = IRQ_HANDLED;
	}

	 
	if (regs[ABB5ZES3_REG_CTRL2] & ABB5ZES3_REG_CTRL2_AF) {
		dev_dbg(dev, "RTC alarm!\n");

		rtc_update_irq(rtc, 1, RTC_IRQF | RTC_AF);

		 
		_abb5zes3_rtc_clear_alarm(dev);
		_abb5zes3_rtc_update_alarm(dev, 0);

		handled = IRQ_HANDLED;
	}

	 
	if (regs[ABB5ZES3_REG_CTRL2] & ABB5ZES3_REG_CTRL2_WTAF) {
		dev_dbg(dev, "RTC timer!\n");

		rtc_update_irq(rtc, 1, RTC_IRQF | RTC_AF);

		 
		_abb5zes3_rtc_update_timer(dev, 0);

		rtc_data->timer_alarm = 0;

		handled = IRQ_HANDLED;
	}

	return handled;
}

static const struct rtc_class_ops rtc_ops = {
	.read_time = _abb5zes3_rtc_read_time,
	.set_time = abb5zes3_rtc_set_time,
	.read_alarm = abb5zes3_rtc_read_alarm,
	.set_alarm = abb5zes3_rtc_set_alarm,
	.alarm_irq_enable = abb5zes3_rtc_alarm_irq_enable,
};

static const struct regmap_config abb5zes3_rtc_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int abb5zes3_probe(struct i2c_client *client)
{
	struct abb5zes3_rtc_data *data = NULL;
	struct device *dev = &client->dev;
	struct regmap *regmap;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C |
				     I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_I2C_BLOCK))
		return -ENODEV;

	regmap = devm_regmap_init_i2c(client, &abb5zes3_rtc_regmap_config);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(dev, "%s: regmap allocation failed: %d\n",
			__func__, ret);
		return ret;
	}

	ret = abb5zes3_i2c_validate_chip(regmap);
	if (ret)
		return ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->regmap = regmap;
	dev_set_drvdata(dev, data);

	ret = abb5zes3_rtc_check_setup(dev);
	if (ret)
		return ret;

	data->rtc = devm_rtc_allocate_device(dev);
	ret = PTR_ERR_OR_ZERO(data->rtc);
	if (ret) {
		dev_err(dev, "%s: unable to allocate RTC device (%d)\n",
			__func__, ret);
		return ret;
	}

	if (client->irq > 0) {
		ret = devm_request_threaded_irq(dev, client->irq, NULL,
						_abb5zes3_rtc_interrupt,
						IRQF_SHARED | IRQF_ONESHOT,
						DRV_NAME, client);
		if (!ret) {
			device_init_wakeup(dev, true);
			data->irq = client->irq;
			dev_dbg(dev, "%s: irq %d used by RTC\n", __func__,
				client->irq);
		} else {
			dev_err(dev, "%s: irq %d unavailable (%d)\n",
				__func__, client->irq, ret);
			goto err;
		}
	}

	data->rtc->ops = &rtc_ops;
	data->rtc->range_min = RTC_TIMESTAMP_BEGIN_2000;
	data->rtc->range_max = RTC_TIMESTAMP_END_2099;

	 
	if (!data->battery_low && data->irq) {
		ret = _abb5zes3_rtc_battery_low_irq_enable(regmap, true);
		if (ret) {
			dev_err(dev, "%s: enabling battery low interrupt generation failed (%d)\n",
				__func__, ret);
			goto err;
		}
	}

	ret = devm_rtc_register_device(data->rtc);

err:
	if (ret && data->irq)
		device_init_wakeup(dev, false);
	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int abb5zes3_rtc_suspend(struct device *dev)
{
	struct abb5zes3_rtc_data *rtc_data = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		return enable_irq_wake(rtc_data->irq);

	return 0;
}

static int abb5zes3_rtc_resume(struct device *dev)
{
	struct abb5zes3_rtc_data *rtc_data = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		return disable_irq_wake(rtc_data->irq);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(abb5zes3_rtc_pm_ops, abb5zes3_rtc_suspend,
			 abb5zes3_rtc_resume);

#ifdef CONFIG_OF
static const struct of_device_id abb5zes3_dt_match[] = {
	{ .compatible = "abracon,abb5zes3" },
	{ },
};
MODULE_DEVICE_TABLE(of, abb5zes3_dt_match);
#endif

static const struct i2c_device_id abb5zes3_id[] = {
	{ "abb5zes3", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, abb5zes3_id);

static struct i2c_driver abb5zes3_driver = {
	.driver = {
		.name = DRV_NAME,
		.pm = &abb5zes3_rtc_pm_ops,
		.of_match_table = of_match_ptr(abb5zes3_dt_match),
	},
	.probe = abb5zes3_probe,
	.id_table = abb5zes3_id,
};
module_i2c_driver(abb5zes3_driver);

MODULE_AUTHOR("Arnaud EBALARD <arno@natisbad.org>");
MODULE_DESCRIPTION("Abracon AB-RTCMC-32.768kHz-B5ZE-S3 RTC/Alarm driver");
MODULE_LICENSE("GPL");
