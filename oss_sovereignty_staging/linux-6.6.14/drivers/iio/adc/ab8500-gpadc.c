
 
#include <linux/init.h>
#include <linux/bits.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/regulator/consumer.h>
#include <linux/random.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mfd/abx500.h>
#include <linux/mfd/abx500/ab8500.h>

 

#define AB8500_GPADC_CTRL1_REG		0x00
 
#define AB8500_GPADC_CTRL1_DISABLE		0x00
#define AB8500_GPADC_CTRL1_ENABLE		BIT(0)
#define AB8500_GPADC_CTRL1_TRIG_ENA		BIT(1)
#define AB8500_GPADC_CTRL1_START_SW_CONV	BIT(2)
#define AB8500_GPADC_CTRL1_BTEMP_PULL_UP	BIT(3)
 
#define AB8500_GPADC_CTRL1_TRIG_EDGE		BIT(4)
 
#define AB8500_GPADC_CTRL1_PUPSUPSEL		BIT(5)
#define AB8500_GPADC_CTRL1_BUF_ENA		BIT(6)
#define AB8500_GPADC_CTRL1_ICHAR_ENA		BIT(7)

#define AB8500_GPADC_CTRL2_REG		0x01
#define AB8500_GPADC_CTRL3_REG		0x02
 
#define AB8500_GPADC_CTRL2_AVG_1		0x00
#define AB8500_GPADC_CTRL2_AVG_4		BIT(5)
#define AB8500_GPADC_CTRL2_AVG_8		BIT(6)
#define AB8500_GPADC_CTRL2_AVG_16		(BIT(5) | BIT(6))

enum ab8500_gpadc_channel {
	AB8500_GPADC_CHAN_UNUSED = 0x00,
	AB8500_GPADC_CHAN_BAT_CTRL = 0x01,
	AB8500_GPADC_CHAN_BAT_TEMP = 0x02,
	 
	AB8500_GPADC_CHAN_MAIN_CHARGER = 0x03,
	AB8500_GPADC_CHAN_ACC_DET_1 = 0x04,
	AB8500_GPADC_CHAN_ACC_DET_2 = 0x05,
	AB8500_GPADC_CHAN_ADC_AUX_1 = 0x06,
	AB8500_GPADC_CHAN_ADC_AUX_2 = 0x07,
	AB8500_GPADC_CHAN_VBAT_A = 0x08,
	AB8500_GPADC_CHAN_VBUS = 0x09,
	AB8500_GPADC_CHAN_MAIN_CHARGER_CURRENT = 0x0a,
	AB8500_GPADC_CHAN_USB_CHARGER_CURRENT = 0x0b,
	AB8500_GPADC_CHAN_BACKUP_BAT = 0x0c,
	 
	AB8505_GPADC_CHAN_DIE_TEMP = 0x0d,
	AB8500_GPADC_CHAN_ID = 0x0e,
	AB8500_GPADC_CHAN_INTERNAL_TEST_1 = 0x0f,
	AB8500_GPADC_CHAN_INTERNAL_TEST_2 = 0x10,
	AB8500_GPADC_CHAN_INTERNAL_TEST_3 = 0x11,
	 
	AB8500_GPADC_CHAN_XTAL_TEMP = 0x12,
	AB8500_GPADC_CHAN_VBAT_TRUE_MEAS = 0x13,
	 
	AB8500_GPADC_CHAN_BAT_CTRL_AND_IBAT = 0x1c,
	AB8500_GPADC_CHAN_VBAT_MEAS_AND_IBAT = 0x1d,
	AB8500_GPADC_CHAN_VBAT_TRUE_MEAS_AND_IBAT = 0x1e,
	AB8500_GPADC_CHAN_BAT_TEMP_AND_IBAT = 0x1f,
	 
	AB8500_GPADC_CHAN_IBAT_VIRTUAL = 0xFF,
};

#define AB8500_GPADC_AUTO_TIMER_REG	0x03

#define AB8500_GPADC_STAT_REG		0x04
#define AB8500_GPADC_STAT_BUSY		BIT(0)

#define AB8500_GPADC_MANDATAL_REG	0x05
#define AB8500_GPADC_MANDATAH_REG	0x06
#define AB8500_GPADC_AUTODATAL_REG	0x07
#define AB8500_GPADC_AUTODATAH_REG	0x08
#define AB8500_GPADC_MUX_CTRL_REG	0x09
#define AB8540_GPADC_MANDATA2L_REG	0x09
#define AB8540_GPADC_MANDATA2H_REG	0x0A
#define AB8540_GPADC_APEAAX_REG		0x10
#define AB8540_GPADC_APEAAT_REG		0x11
#define AB8540_GPADC_APEAAM_REG		0x12
#define AB8540_GPADC_APEAAH_REG		0x13
#define AB8540_GPADC_APEAAL_REG		0x14

 
#define AB8500_GPADC_CAL_1	0x0F
#define AB8500_GPADC_CAL_2	0x10
#define AB8500_GPADC_CAL_3	0x11
#define AB8500_GPADC_CAL_4	0x12
#define AB8500_GPADC_CAL_5	0x13
#define AB8500_GPADC_CAL_6	0x14
#define AB8500_GPADC_CAL_7	0x15
 
#define AB8540_GPADC_OTP4_REG_7	0x38
#define AB8540_GPADC_OTP4_REG_6	0x39
#define AB8540_GPADC_OTP4_REG_5	0x3A

#define AB8540_GPADC_DIS_ZERO	0x00
#define AB8540_GPADC_EN_VBIAS_XTAL_TEMP	0x02

 
#define AB8500_ADC_RESOLUTION		1024
#define AB8500_ADC_CH_BTEMP_MIN		0
#define AB8500_ADC_CH_BTEMP_MAX		1350
#define AB8500_ADC_CH_DIETEMP_MIN	0
#define AB8500_ADC_CH_DIETEMP_MAX	1350
#define AB8500_ADC_CH_CHG_V_MIN		0
#define AB8500_ADC_CH_CHG_V_MAX		20030
#define AB8500_ADC_CH_ACCDET2_MIN	0
#define AB8500_ADC_CH_ACCDET2_MAX	2500
#define AB8500_ADC_CH_VBAT_MIN		2300
#define AB8500_ADC_CH_VBAT_MAX		4800
#define AB8500_ADC_CH_CHG_I_MIN		0
#define AB8500_ADC_CH_CHG_I_MAX		1500
#define AB8500_ADC_CH_BKBAT_MIN		0
#define AB8500_ADC_CH_BKBAT_MAX		3200

 
#define AB8500_ADC_CH_IBAT_MIN		(-6000)  
#define AB8500_ADC_CH_IBAT_MAX		6000
#define AB8500_ADC_CH_IBAT_MIN_V	(-60)	 
#define AB8500_ADC_CH_IBAT_MAX_V	60
#define AB8500_GPADC_IBAT_VDROP_L	(-56)   
#define AB8500_GPADC_IBAT_VDROP_H	56

 
#define AB8500_GPADC_CALIB_SCALE	1000
 
#define AB8500_GPADC_CALIB_SHIFT_IBAT	20

 
#define AB8500_GPADC_AUTOSUSPEND_DELAY	1

#define AB8500_GPADC_CONVERSION_TIME	500  

enum ab8500_cal_channels {
	AB8500_CAL_VMAIN = 0,
	AB8500_CAL_BTEMP,
	AB8500_CAL_VBAT,
	AB8500_CAL_IBAT,
	AB8500_CAL_NR,
};

 
struct ab8500_adc_cal_data {
	s64 gain;
	s64 offset;
	u16 otp_calib_hi;
	u16 otp_calib_lo;
};

 
struct ab8500_gpadc_chan_info {
	const char *name;
	u8 id;
	bool hardware_control;
	bool falling_edge;
	u8 avg_sample;
	u8 trig_timer;
};

 
struct ab8500_gpadc {
	struct device *dev;
	struct ab8500 *ab8500;
	struct ab8500_gpadc_chan_info *chans;
	unsigned int nchans;
	struct completion complete;
	struct regulator *vddadc;
	int irq_sw;
	int irq_hw;
	struct ab8500_adc_cal_data cal_data[AB8500_CAL_NR];
};

static struct ab8500_gpadc_chan_info *
ab8500_gpadc_get_channel(struct ab8500_gpadc *gpadc, u8 chan)
{
	struct ab8500_gpadc_chan_info *ch;
	int i;

	for (i = 0; i < gpadc->nchans; i++) {
		ch = &gpadc->chans[i];
		if (ch->id == chan)
			break;
	}
	if (i == gpadc->nchans)
		return NULL;

	return ch;
}

 
static int ab8500_gpadc_ad_to_voltage(struct ab8500_gpadc *gpadc,
				      enum ab8500_gpadc_channel ch,
				      int ad_value)
{
	int res;

	switch (ch) {
	case AB8500_GPADC_CHAN_MAIN_CHARGER:
		 
		if (!gpadc->cal_data[AB8500_CAL_VMAIN].gain) {
			res = AB8500_ADC_CH_CHG_V_MIN + (AB8500_ADC_CH_CHG_V_MAX -
				AB8500_ADC_CH_CHG_V_MIN) * ad_value /
				AB8500_ADC_RESOLUTION;
			break;
		}
		 
		res = (int) (ad_value * gpadc->cal_data[AB8500_CAL_VMAIN].gain +
			gpadc->cal_data[AB8500_CAL_VMAIN].offset) / AB8500_GPADC_CALIB_SCALE;
		break;

	case AB8500_GPADC_CHAN_BAT_CTRL:
	case AB8500_GPADC_CHAN_BAT_TEMP:
	case AB8500_GPADC_CHAN_ACC_DET_1:
	case AB8500_GPADC_CHAN_ADC_AUX_1:
	case AB8500_GPADC_CHAN_ADC_AUX_2:
	case AB8500_GPADC_CHAN_XTAL_TEMP:
		 
		if (!gpadc->cal_data[AB8500_CAL_BTEMP].gain) {
			res = AB8500_ADC_CH_BTEMP_MIN + (AB8500_ADC_CH_BTEMP_MAX -
				AB8500_ADC_CH_BTEMP_MIN) * ad_value /
				AB8500_ADC_RESOLUTION;
			break;
		}
		 
		res = (int) (ad_value * gpadc->cal_data[AB8500_CAL_BTEMP].gain +
			gpadc->cal_data[AB8500_CAL_BTEMP].offset) / AB8500_GPADC_CALIB_SCALE;
		break;

	case AB8500_GPADC_CHAN_VBAT_A:
	case AB8500_GPADC_CHAN_VBAT_TRUE_MEAS:
		 
		if (!gpadc->cal_data[AB8500_CAL_VBAT].gain) {
			res = AB8500_ADC_CH_VBAT_MIN + (AB8500_ADC_CH_VBAT_MAX -
				AB8500_ADC_CH_VBAT_MIN) * ad_value /
				AB8500_ADC_RESOLUTION;
			break;
		}
		 
		res = (int) (ad_value * gpadc->cal_data[AB8500_CAL_VBAT].gain +
			gpadc->cal_data[AB8500_CAL_VBAT].offset) / AB8500_GPADC_CALIB_SCALE;
		break;

	case AB8505_GPADC_CHAN_DIE_TEMP:
		res = AB8500_ADC_CH_DIETEMP_MIN +
			(AB8500_ADC_CH_DIETEMP_MAX - AB8500_ADC_CH_DIETEMP_MIN) * ad_value /
			AB8500_ADC_RESOLUTION;
		break;

	case AB8500_GPADC_CHAN_ACC_DET_2:
		res = AB8500_ADC_CH_ACCDET2_MIN +
			(AB8500_ADC_CH_ACCDET2_MAX - AB8500_ADC_CH_ACCDET2_MIN) * ad_value /
			AB8500_ADC_RESOLUTION;
		break;

	case AB8500_GPADC_CHAN_VBUS:
		res = AB8500_ADC_CH_CHG_V_MIN +
			(AB8500_ADC_CH_CHG_V_MAX - AB8500_ADC_CH_CHG_V_MIN) * ad_value /
			AB8500_ADC_RESOLUTION;
		break;

	case AB8500_GPADC_CHAN_MAIN_CHARGER_CURRENT:
	case AB8500_GPADC_CHAN_USB_CHARGER_CURRENT:
		res = AB8500_ADC_CH_CHG_I_MIN +
			(AB8500_ADC_CH_CHG_I_MAX - AB8500_ADC_CH_CHG_I_MIN) * ad_value /
			AB8500_ADC_RESOLUTION;
		break;

	case AB8500_GPADC_CHAN_BACKUP_BAT:
		res = AB8500_ADC_CH_BKBAT_MIN +
			(AB8500_ADC_CH_BKBAT_MAX - AB8500_ADC_CH_BKBAT_MIN) * ad_value /
			AB8500_ADC_RESOLUTION;
		break;

	case AB8500_GPADC_CHAN_IBAT_VIRTUAL:
		 
		if (!gpadc->cal_data[AB8500_CAL_IBAT].gain) {
			res = AB8500_ADC_CH_IBAT_MIN + (AB8500_ADC_CH_IBAT_MAX -
				AB8500_ADC_CH_IBAT_MIN) * ad_value /
				AB8500_ADC_RESOLUTION;
			break;
		}
		 
		res = (int) (ad_value * gpadc->cal_data[AB8500_CAL_IBAT].gain +
				gpadc->cal_data[AB8500_CAL_IBAT].offset)
				>> AB8500_GPADC_CALIB_SHIFT_IBAT;
		break;

	default:
		dev_err(gpadc->dev,
			"unknown channel ID: %d, not possible to convert\n",
			ch);
		res = -EINVAL;
		break;

	}

	return res;
}

static int ab8500_gpadc_read(struct ab8500_gpadc *gpadc,
			     const struct ab8500_gpadc_chan_info *ch,
			     int *ibat)
{
	int ret;
	int looplimit = 0;
	unsigned long completion_timeout;
	u8 val;
	u8 low_data, high_data, low_data2, high_data2;
	u8 ctrl1;
	u8 ctrl23;
	unsigned int delay_min = 0;
	unsigned int delay_max = 0;
	u8 data_low_addr, data_high_addr;

	if (!gpadc)
		return -ENODEV;

	 
	if ((gpadc->irq_sw <= 0) && !ch->hardware_control)
		return -ENOTSUPP;
	if ((gpadc->irq_hw <= 0) && ch->hardware_control)
		return -ENOTSUPP;

	 
	pm_runtime_get_sync(gpadc->dev);

	 
	do {
		ret = abx500_get_register_interruptible(gpadc->dev,
			AB8500_GPADC, AB8500_GPADC_STAT_REG, &val);
		if (ret < 0)
			goto out;
		if (!(val & AB8500_GPADC_STAT_BUSY))
			break;
		msleep(20);
	} while (++looplimit < 10);
	if (looplimit >= 10 && (val & AB8500_GPADC_STAT_BUSY)) {
		dev_err(gpadc->dev, "gpadc_conversion: GPADC busy");
		ret = -EINVAL;
		goto out;
	}

	 
	ctrl1 = AB8500_GPADC_CTRL1_ENABLE;

	 
	switch (ch->avg_sample) {
	case 1:
		ctrl23 = ch->id | AB8500_GPADC_CTRL2_AVG_1;
		break;
	case 4:
		ctrl23 = ch->id | AB8500_GPADC_CTRL2_AVG_4;
		break;
	case 8:
		ctrl23 = ch->id | AB8500_GPADC_CTRL2_AVG_8;
		break;
	default:
		ctrl23 = ch->id | AB8500_GPADC_CTRL2_AVG_16;
		break;
	}

	if (ch->hardware_control) {
		ret = abx500_set_register_interruptible(gpadc->dev,
				AB8500_GPADC, AB8500_GPADC_CTRL3_REG, ctrl23);
		ctrl1 |= AB8500_GPADC_CTRL1_TRIG_ENA;
		if (ch->falling_edge)
			ctrl1 |= AB8500_GPADC_CTRL1_TRIG_EDGE;
	} else {
		ret = abx500_set_register_interruptible(gpadc->dev,
				AB8500_GPADC, AB8500_GPADC_CTRL2_REG, ctrl23);
	}
	if (ret < 0) {
		dev_err(gpadc->dev,
			"gpadc_conversion: set avg samples failed\n");
		goto out;
	}

	 
	switch (ch->id) {
	case AB8500_GPADC_CHAN_MAIN_CHARGER_CURRENT:
	case AB8500_GPADC_CHAN_USB_CHARGER_CURRENT:
		ctrl1 |= AB8500_GPADC_CTRL1_BUF_ENA |
			AB8500_GPADC_CTRL1_ICHAR_ENA;
		break;
	case AB8500_GPADC_CHAN_BAT_TEMP:
		if (!is_ab8500_2p0_or_earlier(gpadc->ab8500)) {
			ctrl1 |= AB8500_GPADC_CTRL1_BUF_ENA |
				AB8500_GPADC_CTRL1_BTEMP_PULL_UP;
			 
			delay_min = 1000;  
			delay_max = 10000;  
			break;
		}
		fallthrough;
	default:
		ctrl1 |= AB8500_GPADC_CTRL1_BUF_ENA;
		break;
	}

	 
	ret = abx500_set_register_interruptible(gpadc->dev,
		AB8500_GPADC, AB8500_GPADC_CTRL1_REG, ctrl1);
	if (ret < 0) {
		dev_err(gpadc->dev,
			"gpadc_conversion: set Control register failed\n");
		goto out;
	}

	if (delay_min != 0)
		usleep_range(delay_min, delay_max);

	if (ch->hardware_control) {
		 
		ret = abx500_set_register_interruptible(gpadc->dev,
			AB8500_GPADC, AB8500_GPADC_AUTO_TIMER_REG,
			ch->trig_timer);
		if (ret < 0) {
			dev_err(gpadc->dev,
				"gpadc_conversion: trig timer failed\n");
			goto out;
		}
		completion_timeout = 2 * HZ;
		data_low_addr = AB8500_GPADC_AUTODATAL_REG;
		data_high_addr = AB8500_GPADC_AUTODATAH_REG;
	} else {
		 
		ret = abx500_mask_and_set_register_interruptible(gpadc->dev,
			AB8500_GPADC, AB8500_GPADC_CTRL1_REG,
			AB8500_GPADC_CTRL1_START_SW_CONV,
			AB8500_GPADC_CTRL1_START_SW_CONV);
		if (ret < 0) {
			dev_err(gpadc->dev,
				"gpadc_conversion: start s/w conv failed\n");
			goto out;
		}
		completion_timeout = msecs_to_jiffies(AB8500_GPADC_CONVERSION_TIME);
		data_low_addr = AB8500_GPADC_MANDATAL_REG;
		data_high_addr = AB8500_GPADC_MANDATAH_REG;
	}

	 
	if (!wait_for_completion_timeout(&gpadc->complete,
			completion_timeout)) {
		dev_err(gpadc->dev,
			"timeout didn't receive GPADC conv interrupt\n");
		ret = -EINVAL;
		goto out;
	}

	 
	ret = abx500_get_register_interruptible(gpadc->dev,
			AB8500_GPADC, data_low_addr, &low_data);
	if (ret < 0) {
		dev_err(gpadc->dev,
			"gpadc_conversion: read low data failed\n");
		goto out;
	}

	ret = abx500_get_register_interruptible(gpadc->dev,
		AB8500_GPADC, data_high_addr, &high_data);
	if (ret < 0) {
		dev_err(gpadc->dev,
			"gpadc_conversion: read high data failed\n");
		goto out;
	}

	 
	if ((ch->id == AB8500_GPADC_CHAN_BAT_CTRL_AND_IBAT) ||
	    (ch->id == AB8500_GPADC_CHAN_VBAT_MEAS_AND_IBAT) ||
	    (ch->id == AB8500_GPADC_CHAN_VBAT_TRUE_MEAS_AND_IBAT) ||
	    (ch->id == AB8500_GPADC_CHAN_BAT_TEMP_AND_IBAT)) {

		if (ch->hardware_control) {
			 
			ret = -ENOTSUPP;
			dev_err(gpadc->dev,
				"gpadc_conversion: only SW double conversion supported\n");
			goto out;
		} else {
			 
			ret = abx500_get_register_interruptible(gpadc->dev,
				AB8500_GPADC, AB8540_GPADC_MANDATA2L_REG,
				&low_data2);
			if (ret < 0) {
				dev_err(gpadc->dev,
					"gpadc_conversion: read sw low data 2 failed\n");
				goto out;
			}

			ret = abx500_get_register_interruptible(gpadc->dev,
				AB8500_GPADC, AB8540_GPADC_MANDATA2H_REG,
				&high_data2);
			if (ret < 0) {
				dev_err(gpadc->dev,
					"gpadc_conversion: read sw high data 2 failed\n");
				goto out;
			}
			if (ibat != NULL) {
				*ibat = (high_data2 << 8) | low_data2;
			} else {
				dev_warn(gpadc->dev,
					"gpadc_conversion: ibat not stored\n");
			}

		}
	}

	 
	ret = abx500_set_register_interruptible(gpadc->dev, AB8500_GPADC,
		AB8500_GPADC_CTRL1_REG, AB8500_GPADC_CTRL1_DISABLE);
	if (ret < 0) {
		dev_err(gpadc->dev, "gpadc_conversion: disable gpadc failed\n");
		goto out;
	}

	 
	pm_runtime_mark_last_busy(gpadc->dev);
	pm_runtime_put_autosuspend(gpadc->dev);

	return (high_data << 8) | low_data;

out:
	 
	(void) abx500_set_register_interruptible(gpadc->dev, AB8500_GPADC,
		AB8500_GPADC_CTRL1_REG, AB8500_GPADC_CTRL1_DISABLE);
	pm_runtime_put(gpadc->dev);
	dev_err(gpadc->dev,
		"gpadc_conversion: Failed to AD convert channel %d\n", ch->id);

	return ret;
}

 
static irqreturn_t ab8500_bm_gpadcconvend_handler(int irq, void *data)
{
	struct ab8500_gpadc *gpadc = data;

	complete(&gpadc->complete);

	return IRQ_HANDLED;
}

static int otp_cal_regs[] = {
	AB8500_GPADC_CAL_1,
	AB8500_GPADC_CAL_2,
	AB8500_GPADC_CAL_3,
	AB8500_GPADC_CAL_4,
	AB8500_GPADC_CAL_5,
	AB8500_GPADC_CAL_6,
	AB8500_GPADC_CAL_7,
};

static int otp4_cal_regs[] = {
	AB8540_GPADC_OTP4_REG_7,
	AB8540_GPADC_OTP4_REG_6,
	AB8540_GPADC_OTP4_REG_5,
};

static void ab8500_gpadc_read_calibration_data(struct ab8500_gpadc *gpadc)
{
	int i;
	int ret[ARRAY_SIZE(otp_cal_regs)];
	u8 gpadc_cal[ARRAY_SIZE(otp_cal_regs)];
	int ret_otp4[ARRAY_SIZE(otp4_cal_regs)];
	u8 gpadc_otp4[ARRAY_SIZE(otp4_cal_regs)];
	int vmain_high, vmain_low;
	int btemp_high, btemp_low;
	int vbat_high, vbat_low;
	int ibat_high, ibat_low;
	s64 V_gain, V_offset, V2A_gain, V2A_offset;

	 
	for (i = 0; i < ARRAY_SIZE(otp_cal_regs); i++) {
		ret[i] = abx500_get_register_interruptible(gpadc->dev,
			AB8500_OTP_EMUL, otp_cal_regs[i],  &gpadc_cal[i]);
		if (ret[i] < 0) {
			 
			dev_err(gpadc->dev, "%s: read otp reg 0x%02x failed\n",
				__func__, otp_cal_regs[i]);
		} else {
			 
			add_device_randomness(&ret[i], sizeof(ret[i]));
		}
	}

	 

	if (is_ab8540(gpadc->ab8500)) {
		 
		if (!(ret[1] < 0 || ret[2] < 0)) {
			vmain_high = (((gpadc_cal[1] & 0xFF) << 2) |
				((gpadc_cal[2] & 0xC0) >> 6));
			vmain_low = ((gpadc_cal[2] & 0x3E) >> 1);

			gpadc->cal_data[AB8500_CAL_VMAIN].otp_calib_hi =
				(u16)vmain_high;
			gpadc->cal_data[AB8500_CAL_VMAIN].otp_calib_lo =
				(u16)vmain_low;

			gpadc->cal_data[AB8500_CAL_VMAIN].gain = AB8500_GPADC_CALIB_SCALE *
				(19500 - 315) / (vmain_high - vmain_low);
			gpadc->cal_data[AB8500_CAL_VMAIN].offset = AB8500_GPADC_CALIB_SCALE *
				19500 - (AB8500_GPADC_CALIB_SCALE * (19500 - 315) /
				(vmain_high - vmain_low)) * vmain_high;
		} else {
			gpadc->cal_data[AB8500_CAL_VMAIN].gain = 0;
		}

		 
		for (i = 0; i < ARRAY_SIZE(otp4_cal_regs); i++) {
			ret_otp4[i] = abx500_get_register_interruptible(
					gpadc->dev, AB8500_OTP_EMUL,
					otp4_cal_regs[i],  &gpadc_otp4[i]);
			if (ret_otp4[i] < 0)
				dev_err(gpadc->dev,
					"%s: read otp4 reg 0x%02x failed\n",
					__func__, otp4_cal_regs[i]);
		}

		 
		if (!(ret_otp4[0] < 0 || ret_otp4[1] < 0 || ret_otp4[2] < 0)) {
			ibat_high = (((gpadc_otp4[0] & 0x07) << 7) |
				((gpadc_otp4[1] & 0xFE) >> 1));
			ibat_low = (((gpadc_otp4[1] & 0x01) << 5) |
				((gpadc_otp4[2] & 0xF8) >> 3));

			gpadc->cal_data[AB8500_CAL_IBAT].otp_calib_hi =
				(u16)ibat_high;
			gpadc->cal_data[AB8500_CAL_IBAT].otp_calib_lo =
				(u16)ibat_low;

			V_gain = ((AB8500_GPADC_IBAT_VDROP_H - AB8500_GPADC_IBAT_VDROP_L)
				<< AB8500_GPADC_CALIB_SHIFT_IBAT) / (ibat_high - ibat_low);

			V_offset = (AB8500_GPADC_IBAT_VDROP_H << AB8500_GPADC_CALIB_SHIFT_IBAT) -
				(((AB8500_GPADC_IBAT_VDROP_H - AB8500_GPADC_IBAT_VDROP_L) <<
				AB8500_GPADC_CALIB_SHIFT_IBAT) / (ibat_high - ibat_low))
				* ibat_high;
			 
			V2A_gain = (AB8500_ADC_CH_IBAT_MAX - AB8500_ADC_CH_IBAT_MIN)/
				(AB8500_ADC_CH_IBAT_MAX_V - AB8500_ADC_CH_IBAT_MIN_V);
			V2A_offset = ((AB8500_ADC_CH_IBAT_MAX_V * AB8500_ADC_CH_IBAT_MIN -
				AB8500_ADC_CH_IBAT_MAX * AB8500_ADC_CH_IBAT_MIN_V)
				<< AB8500_GPADC_CALIB_SHIFT_IBAT)
				/ (AB8500_ADC_CH_IBAT_MAX_V - AB8500_ADC_CH_IBAT_MIN_V);

			gpadc->cal_data[AB8500_CAL_IBAT].gain =
				V_gain * V2A_gain;
			gpadc->cal_data[AB8500_CAL_IBAT].offset =
				V_offset * V2A_gain + V2A_offset;
		} else {
			gpadc->cal_data[AB8500_CAL_IBAT].gain = 0;
		}
	} else {
		 
		if (!(ret[0] < 0 || ret[1] < 0 || ret[2] < 0)) {
			vmain_high = (((gpadc_cal[0] & 0x03) << 8) |
				((gpadc_cal[1] & 0x3F) << 2) |
				((gpadc_cal[2] & 0xC0) >> 6));
			vmain_low = ((gpadc_cal[2] & 0x3E) >> 1);

			gpadc->cal_data[AB8500_CAL_VMAIN].otp_calib_hi =
				(u16)vmain_high;
			gpadc->cal_data[AB8500_CAL_VMAIN].otp_calib_lo =
				(u16)vmain_low;

			gpadc->cal_data[AB8500_CAL_VMAIN].gain = AB8500_GPADC_CALIB_SCALE *
				(19500 - 315) / (vmain_high - vmain_low);

			gpadc->cal_data[AB8500_CAL_VMAIN].offset = AB8500_GPADC_CALIB_SCALE *
				19500 - (AB8500_GPADC_CALIB_SCALE * (19500 - 315) /
				(vmain_high - vmain_low)) * vmain_high;
		} else {
			gpadc->cal_data[AB8500_CAL_VMAIN].gain = 0;
		}
	}

	 
	if (!(ret[2] < 0 || ret[3] < 0 || ret[4] < 0)) {
		btemp_high = (((gpadc_cal[2] & 0x01) << 9) |
			(gpadc_cal[3] << 1) | ((gpadc_cal[4] & 0x80) >> 7));
		btemp_low = ((gpadc_cal[4] & 0x7C) >> 2);

		gpadc->cal_data[AB8500_CAL_BTEMP].otp_calib_hi = (u16)btemp_high;
		gpadc->cal_data[AB8500_CAL_BTEMP].otp_calib_lo = (u16)btemp_low;

		gpadc->cal_data[AB8500_CAL_BTEMP].gain =
			AB8500_GPADC_CALIB_SCALE * (1300 - 21) / (btemp_high - btemp_low);
		gpadc->cal_data[AB8500_CAL_BTEMP].offset = AB8500_GPADC_CALIB_SCALE * 1300 -
			(AB8500_GPADC_CALIB_SCALE * (1300 - 21) / (btemp_high - btemp_low))
			* btemp_high;
	} else {
		gpadc->cal_data[AB8500_CAL_BTEMP].gain = 0;
	}

	 
	if (!(ret[4] < 0 || ret[5] < 0 || ret[6] < 0)) {
		vbat_high = (((gpadc_cal[4] & 0x03) << 8) | gpadc_cal[5]);
		vbat_low = ((gpadc_cal[6] & 0xFC) >> 2);

		gpadc->cal_data[AB8500_CAL_VBAT].otp_calib_hi = (u16)vbat_high;
		gpadc->cal_data[AB8500_CAL_VBAT].otp_calib_lo = (u16)vbat_low;

		gpadc->cal_data[AB8500_CAL_VBAT].gain = AB8500_GPADC_CALIB_SCALE *
			(4700 - 2380) /	(vbat_high - vbat_low);
		gpadc->cal_data[AB8500_CAL_VBAT].offset = AB8500_GPADC_CALIB_SCALE * 4700 -
			(AB8500_GPADC_CALIB_SCALE * (4700 - 2380) /
			(vbat_high - vbat_low)) * vbat_high;
	} else {
		gpadc->cal_data[AB8500_CAL_VBAT].gain = 0;
	}
}

static int ab8500_gpadc_read_raw(struct iio_dev *indio_dev,
				 struct iio_chan_spec const *chan,
				 int *val, int *val2, long mask)
{
	struct ab8500_gpadc *gpadc = iio_priv(indio_dev);
	const struct ab8500_gpadc_chan_info *ch;
	int raw_val;
	int processed;

	ch = ab8500_gpadc_get_channel(gpadc, chan->address);
	if (!ch) {
		dev_err(gpadc->dev, "no such channel %lu\n",
			chan->address);
		return -EINVAL;
	}

	raw_val = ab8500_gpadc_read(gpadc, ch, NULL);
	if (raw_val < 0)
		return raw_val;

	if (mask == IIO_CHAN_INFO_RAW) {
		*val = raw_val;
		return IIO_VAL_INT;
	}

	if (mask == IIO_CHAN_INFO_PROCESSED) {
		processed = ab8500_gpadc_ad_to_voltage(gpadc, ch->id, raw_val);
		if (processed < 0)
			return processed;

		 
		*val = processed;
		return IIO_VAL_INT;
	}

	return -EINVAL;
}

static int ab8500_gpadc_fwnode_xlate(struct iio_dev *indio_dev,
				     const struct fwnode_reference_args *iiospec)
{
	int i;

	for (i = 0; i < indio_dev->num_channels; i++)
		if (indio_dev->channels[i].channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info ab8500_gpadc_info = {
	.fwnode_xlate = ab8500_gpadc_fwnode_xlate,
	.read_raw = ab8500_gpadc_read_raw,
};

static int ab8500_gpadc_runtime_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ab8500_gpadc *gpadc = iio_priv(indio_dev);

	regulator_disable(gpadc->vddadc);

	return 0;
}

static int ab8500_gpadc_runtime_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ab8500_gpadc *gpadc = iio_priv(indio_dev);
	int ret;

	ret = regulator_enable(gpadc->vddadc);
	if (ret)
		dev_err(dev, "Failed to enable vddadc: %d\n", ret);

	return ret;
}

 
static int ab8500_gpadc_parse_channel(struct device *dev,
				      struct fwnode_handle *fwnode,
				      struct ab8500_gpadc_chan_info *ch,
				      struct iio_chan_spec *iio_chan)
{
	const char *name = fwnode_get_name(fwnode);
	u32 chan;
	int ret;

	ret = fwnode_property_read_u32(fwnode, "reg", &chan);
	if (ret) {
		dev_err(dev, "invalid channel number %s\n", name);
		return ret;
	}
	if (chan > AB8500_GPADC_CHAN_BAT_TEMP_AND_IBAT) {
		dev_err(dev, "%s channel number out of range %d\n", name, chan);
		return -EINVAL;
	}

	iio_chan->channel = chan;
	iio_chan->datasheet_name = name;
	iio_chan->indexed = 1;
	iio_chan->address = chan;
	iio_chan->info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
		BIT(IIO_CHAN_INFO_PROCESSED);
	 
	if ((chan == AB8500_GPADC_CHAN_MAIN_CHARGER_CURRENT) ||
	    (chan == AB8500_GPADC_CHAN_USB_CHARGER_CURRENT))
		iio_chan->type = IIO_CURRENT;
	else
		iio_chan->type = IIO_VOLTAGE;

	ch->id = chan;

	 
	ch->avg_sample = 16;
	ch->hardware_control = false;
	ch->falling_edge = false;
	ch->trig_timer = 0;

	return 0;
}

 
static int ab8500_gpadc_parse_channels(struct ab8500_gpadc *gpadc,
				       struct iio_chan_spec **chans_parsed,
				       unsigned int *nchans_parsed)
{
	struct fwnode_handle *child;
	struct ab8500_gpadc_chan_info *ch;
	struct iio_chan_spec *iio_chans;
	unsigned int nchans;
	int i;

	nchans = device_get_child_node_count(gpadc->dev);
	if (!nchans) {
		dev_err(gpadc->dev, "no channel children\n");
		return -ENODEV;
	}
	dev_info(gpadc->dev, "found %d ADC channels\n", nchans);

	iio_chans = devm_kcalloc(gpadc->dev, nchans,
				 sizeof(*iio_chans), GFP_KERNEL);
	if (!iio_chans)
		return -ENOMEM;

	gpadc->chans = devm_kcalloc(gpadc->dev, nchans,
				    sizeof(*gpadc->chans), GFP_KERNEL);
	if (!gpadc->chans)
		return -ENOMEM;

	i = 0;
	device_for_each_child_node(gpadc->dev, child) {
		struct iio_chan_spec *iio_chan;
		int ret;

		ch = &gpadc->chans[i];
		iio_chan = &iio_chans[i];

		ret = ab8500_gpadc_parse_channel(gpadc->dev, child, ch,
						 iio_chan);
		if (ret) {
			fwnode_handle_put(child);
			return ret;
		}
		i++;
	}
	gpadc->nchans = nchans;
	*chans_parsed = iio_chans;
	*nchans_parsed = nchans;

	return 0;
}

static int ab8500_gpadc_probe(struct platform_device *pdev)
{
	struct ab8500_gpadc *gpadc;
	struct iio_dev *indio_dev;
	struct device *dev = &pdev->dev;
	struct iio_chan_spec *iio_chans;
	unsigned int n_iio_chans;
	int ret;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*gpadc));
	if (!indio_dev)
		return -ENOMEM;

	platform_set_drvdata(pdev, indio_dev);
	gpadc = iio_priv(indio_dev);

	gpadc->dev = dev;
	gpadc->ab8500 = dev_get_drvdata(dev->parent);

	ret = ab8500_gpadc_parse_channels(gpadc, &iio_chans, &n_iio_chans);
	if (ret)
		return ret;

	gpadc->irq_sw = platform_get_irq_byname(pdev, "SW_CONV_END");
	if (gpadc->irq_sw < 0)
		return gpadc->irq_sw;

	if (is_ab8500(gpadc->ab8500)) {
		gpadc->irq_hw = platform_get_irq_byname(pdev, "HW_CONV_END");
		if (gpadc->irq_hw < 0)
			return gpadc->irq_hw;
	} else {
		gpadc->irq_hw = 0;
	}

	 
	init_completion(&gpadc->complete);

	 
	ret = devm_request_threaded_irq(dev, gpadc->irq_sw, NULL,
		ab8500_bm_gpadcconvend_handler,	IRQF_NO_SUSPEND | IRQF_ONESHOT,
		"ab8500-gpadc-sw", gpadc);
	if (ret < 0) {
		dev_err(dev,
			"failed to request sw conversion irq %d\n",
			gpadc->irq_sw);
		return ret;
	}

	if (gpadc->irq_hw) {
		ret = devm_request_threaded_irq(dev, gpadc->irq_hw, NULL,
			ab8500_bm_gpadcconvend_handler,	IRQF_NO_SUSPEND | IRQF_ONESHOT,
			"ab8500-gpadc-hw", gpadc);
		if (ret < 0) {
			dev_err(dev,
				"Failed to request hw conversion irq: %d\n",
				gpadc->irq_hw);
			return ret;
		}
	}

	 
	gpadc->vddadc = devm_regulator_get(dev, "vddadc");
	if (IS_ERR(gpadc->vddadc))
		return dev_err_probe(dev, PTR_ERR(gpadc->vddadc),
				     "failed to get vddadc\n");

	ret = regulator_enable(gpadc->vddadc);
	if (ret) {
		dev_err(dev, "failed to enable vddadc: %d\n", ret);
		return ret;
	}

	 
	pm_runtime_get_noresume(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_set_autosuspend_delay(dev, AB8500_GPADC_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(dev);

	ab8500_gpadc_read_calibration_data(gpadc);

	pm_runtime_put(dev);

	indio_dev->name = "ab8500-gpadc";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &ab8500_gpadc_info;
	indio_dev->channels = iio_chans;
	indio_dev->num_channels = n_iio_chans;

	ret = devm_iio_device_register(dev, indio_dev);
	if (ret)
		goto out_dis_pm;

	return 0;

out_dis_pm:
	pm_runtime_get_sync(dev);
	pm_runtime_put_noidle(dev);
	pm_runtime_disable(dev);
	regulator_disable(gpadc->vddadc);

	return ret;
}

static int ab8500_gpadc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct ab8500_gpadc *gpadc = iio_priv(indio_dev);

	pm_runtime_get_sync(gpadc->dev);
	pm_runtime_put_noidle(gpadc->dev);
	pm_runtime_disable(gpadc->dev);
	regulator_disable(gpadc->vddadc);

	return 0;
}

static DEFINE_RUNTIME_DEV_PM_OPS(ab8500_gpadc_pm_ops,
				 ab8500_gpadc_runtime_suspend,
				 ab8500_gpadc_runtime_resume, NULL);

static struct platform_driver ab8500_gpadc_driver = {
	.probe = ab8500_gpadc_probe,
	.remove = ab8500_gpadc_remove,
	.driver = {
		.name = "ab8500-gpadc",
		.pm = pm_ptr(&ab8500_gpadc_pm_ops),
	},
};
builtin_platform_driver(ab8500_gpadc_driver);
