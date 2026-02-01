
 

#include <linux/iio/adc/qcom-vadc-common.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>

 

 
#define ADC_ARB_BTM_CNTRL1			0x17e
#define ADC_ARB_BTM_CNTRL1_EN_BTM		BIT(0)
#define ADC_ARB_BTM_CNTRL1_SEL_OP_MODE		BIT(1)
#define ADC_ARB_BTM_CNTRL1_MEAS_INTERVAL1	BIT(2)
#define ADC_ARB_BTM_CNTRL1_MEAS_INTERVAL2	BIT(3)
#define ADC_ARB_BTM_CNTRL1_MEAS_INTERVAL3	BIT(4)
#define ADC_ARB_BTM_CNTRL1_MEAS_INTERVAL4	BIT(5)
#define ADC_ARB_BTM_CNTRL1_EOC			BIT(6)
#define ADC_ARB_BTM_CNTRL1_REQ			BIT(7)

#define ADC_ARB_BTM_AMUX_CNTRL			0x17f
#define ADC_ARB_BTM_ANA_PARAM			0x180
#define ADC_ARB_BTM_DIG_PARAM			0x181
#define ADC_ARB_BTM_RSV				0x182
#define ADC_ARB_BTM_DATA1			0x183
#define ADC_ARB_BTM_DATA0			0x184
#define ADC_ARB_BTM_BAT_COOL_THR1		0x185
#define ADC_ARB_BTM_BAT_COOL_THR0		0x186
#define ADC_ARB_BTM_BAT_WARM_THR1		0x187
#define ADC_ARB_BTM_BAT_WARM_THR0		0x188
#define ADC_ARB_BTM_CNTRL2			0x18c

 

#define ADC_ARB_USRP_CNTRL			0x197
#define ADC_ARB_USRP_CNTRL_EN_ARB		BIT(0)
#define ADC_ARB_USRP_CNTRL_RSV1			BIT(1)
#define ADC_ARB_USRP_CNTRL_RSV2			BIT(2)
#define ADC_ARB_USRP_CNTRL_RSV3			BIT(3)
#define ADC_ARB_USRP_CNTRL_RSV4			BIT(4)
#define ADC_ARB_USRP_CNTRL_RSV5			BIT(5)
#define ADC_ARB_USRP_CNTRL_EOC			BIT(6)
#define ADC_ARB_USRP_CNTRL_REQ			BIT(7)

#define ADC_ARB_USRP_AMUX_CNTRL			0x198
 
#define ADC_ARB_USRP_AMUX_CNTRL_CHAN_MASK	0xfc
#define ADC_ARB_USRP_AMUX_CNTRL_RSV0		BIT(0)
#define ADC_ARB_USRP_AMUX_CNTRL_RSV1		BIT(1)
 
#define ADC_ARB_USRP_AMUX_CNTRL_PRESCALEMUX0	BIT(2)
#define ADC_ARB_USRP_AMUX_CNTRL_PRESCALEMUX1	BIT(3)
#define ADC_ARB_USRP_AMUX_CNTRL_SEL0		BIT(4)
#define ADC_ARB_USRP_AMUX_CNTRL_SEL1		BIT(5)
#define ADC_ARB_USRP_AMUX_CNTRL_SEL2		BIT(6)
#define ADC_ARB_USRP_AMUX_CNTRL_SEL3		BIT(7)
#define ADC_AMUX_PREMUX_SHIFT			2
#define ADC_AMUX_SEL_SHIFT			4

 
#define ADC_ARB_USRP_ANA_PARAM			0x199
#define ADC_ARB_USRP_ANA_PARAM_DIS		0xFE
#define ADC_ARB_USRP_ANA_PARAM_EN		0xFF

#define ADC_ARB_USRP_DIG_PARAM			0x19A
#define ADC_ARB_USRP_DIG_PARAM_SEL_SHIFT0	BIT(0)
#define ADC_ARB_USRP_DIG_PARAM_SEL_SHIFT1	BIT(1)
#define ADC_ARB_USRP_DIG_PARAM_CLK_RATE0	BIT(2)
#define ADC_ARB_USRP_DIG_PARAM_CLK_RATE1	BIT(3)
#define ADC_ARB_USRP_DIG_PARAM_EOC		BIT(4)
 
#define ADC_ARB_USRP_DIG_PARAM_DEC_RATE0	BIT(5)
#define ADC_ARB_USRP_DIG_PARAM_DEC_RATE1	BIT(6)
#define ADC_ARB_USRP_DIG_PARAM_EN		BIT(7)
#define ADC_DIG_PARAM_DEC_SHIFT			5

#define ADC_ARB_USRP_RSV			0x19B
#define ADC_ARB_USRP_RSV_RST			BIT(0)
#define ADC_ARB_USRP_RSV_DTEST0			BIT(1)
#define ADC_ARB_USRP_RSV_DTEST1			BIT(2)
#define ADC_ARB_USRP_RSV_OP			BIT(3)
#define ADC_ARB_USRP_RSV_IP_SEL0		BIT(4)
#define ADC_ARB_USRP_RSV_IP_SEL1		BIT(5)
#define ADC_ARB_USRP_RSV_IP_SEL2		BIT(6)
#define ADC_ARB_USRP_RSV_TRM			BIT(7)
#define ADC_RSV_IP_SEL_SHIFT			4

#define ADC_ARB_USRP_DATA0			0x19D
#define ADC_ARB_USRP_DATA1			0x19C

 
#define PM8XXX_CHANNEL_INTERNAL		0x0c
#define PM8XXX_CHANNEL_125V		0x0d
#define PM8XXX_CHANNEL_INTERNAL_2	0x0e
#define PM8XXX_CHANNEL_MUXOFF		0x0f

 
#define PM8058_AMUX_PRESCALE_0 0x0  
#define PM8058_AMUX_PRESCALE_1 0x1  
#define PM8058_AMUX_PRESCALE_1_DIV3 0x2  

 
#define AMUX_RSV0 0x0  
#define AMUX_RSV1 0x1  
#define AMUX_RSV2 0x2  
#define AMUX_RSV3 0x3  
#define AMUX_RSV4 0x4  
#define AMUX_RSV5 0x5  
#define XOADC_RSV_MAX 5  

 
struct xoadc_channel {
	const char *datasheet_name;
	u8 pre_scale_mux:2;
	u8 amux_channel:4;
	const struct u32_fract prescale;
	enum iio_chan_type type;
	enum vadc_scale_fn_type scale_fn_type;
	u8 amux_ip_rsv:3;
};

 
struct xoadc_variant {
	const char name[16];
	const struct xoadc_channel *channels;
	bool broken_ratiometric;
	bool prescaling;
	bool second_level_mux;
};

 
#define XOADC_CHAN(_dname, _presmux, _amux, _type, _prenum, _preden, _scale, _amip) \
	{								\
		.datasheet_name = __stringify(_dname),			\
		.pre_scale_mux = _presmux,				\
		.amux_channel = _amux,					\
		.prescale = {						\
			.numerator = _prenum, .denominator = _preden,	\
		},							\
		.type = _type,						\
		.scale_fn_type = _scale,				\
		.amux_ip_rsv = _amip,					\
	}

 
static const struct xoadc_channel pm8018_xoadc_channels[] = {
	XOADC_CHAN(VCOIN, 0x00, 0x00, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(VBAT, 0x00, 0x01, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(VPH_PWR, 0x00, 0x02, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(DIE_TEMP, 0x00, 0x0b, IIO_TEMP, 1, 1, SCALE_PMIC_THERM, AMUX_RSV1),
	 
	XOADC_CHAN(AMUX8, 0x00, 0x08, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV2),
	XOADC_CHAN(INTERNAL, 0x00, 0x0c, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(125V, 0x00, 0x0d, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(MUXOFF, 0x00, 0x0f, IIO_TEMP, 1, 1, SCALE_XOTHERM, AMUX_RSV0),
	{ },  
};

 
static const struct xoadc_channel pm8038_xoadc_channels[] = {
	XOADC_CHAN(VCOIN, 0x00, 0x00, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(VBAT, 0x00, 0x01, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(DCIN, 0x00, 0x02, IIO_VOLTAGE, 1, 6, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ICHG, 0x00, 0x03, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(VPH_PWR, 0x00, 0x04, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX5, 0x00, 0x05, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX6, 0x00, 0x06, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX7, 0x00, 0x07, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	 
	XOADC_CHAN(AMUX8, 0x00, 0x08, IIO_TEMP, 1, 1, SCALE_THERM_100K_PULLUP, AMUX_RSV2),
	XOADC_CHAN(AMUX9, 0x00, 0x09, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(USB_VBUS, 0x00, 0x0a, IIO_VOLTAGE, 1, 4, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(DIE_TEMP, 0x00, 0x0b, IIO_TEMP, 1, 1, SCALE_PMIC_THERM, AMUX_RSV1),
	XOADC_CHAN(INTERNAL, 0x00, 0x0c, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(125V, 0x00, 0x0d, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(INTERNAL_2, 0x00, 0x0e, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(MUXOFF, 0x00, 0x0f, IIO_TEMP, 1, 1, SCALE_XOTHERM, AMUX_RSV0),
	{ },  
};

 
static const struct xoadc_channel pm8058_xoadc_channels[] = {
	XOADC_CHAN(VCOIN, 0x00, 0x00, IIO_VOLTAGE, 1, 2, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(VBAT, 0x00, 0x01, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(DCIN, 0x00, 0x02, IIO_VOLTAGE, 1, 10, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ICHG, 0x00, 0x03, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(VPH_PWR, 0x00, 0x04, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	 
	XOADC_CHAN(AMUX5, 0x00, 0x05, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX6, 0x00, 0x06, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX7, 0x00, 0x07, IIO_VOLTAGE, 1, 2, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX8, 0x00, 0x08, IIO_VOLTAGE, 1, 2, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX9, 0x00, 0x09, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(USB_VBUS, 0x00, 0x0a, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(DIE_TEMP, 0x00, 0x0b, IIO_TEMP, 1, 1, SCALE_PMIC_THERM, AMUX_RSV1),
	XOADC_CHAN(INTERNAL, 0x00, 0x0c, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(125V, 0x00, 0x0d, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(INTERNAL_2, 0x00, 0x0e, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(MUXOFF, 0x00, 0x0f, IIO_TEMP, 1, 1, SCALE_XOTHERM, AMUX_RSV0),
	 
	{ },  
};

 
static const struct xoadc_channel pm8921_xoadc_channels[] = {
	XOADC_CHAN(VCOIN, 0x00, 0x00, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(VBAT, 0x00, 0x01, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(DCIN, 0x00, 0x02, IIO_VOLTAGE, 1, 6, SCALE_DEFAULT, AMUX_RSV1),
	 
	XOADC_CHAN(VPH_PWR, 0x00, 0x04, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(IBAT, 0x00, 0x05, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	 
	XOADC_CHAN(BATT_THERM, 0x00, 0x08, IIO_TEMP, 1, 1, SCALE_THERM_100K_PULLUP, AMUX_RSV1),
	XOADC_CHAN(BATT_ID, 0x00, 0x09, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(USB_VBUS, 0x00, 0x0a, IIO_VOLTAGE, 1, 4, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(DIE_TEMP, 0x00, 0x0b, IIO_TEMP, 1, 1, SCALE_PMIC_THERM, AMUX_RSV1),
	XOADC_CHAN(INTERNAL, 0x00, 0x0c, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(125V, 0x00, 0x0d, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	 
	XOADC_CHAN(CHG_TEMP, 0x00, 0x0e, IIO_TEMP, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(MUXOFF, 0x00, 0x0f, IIO_TEMP, 1, 1, SCALE_XOTHERM, AMUX_RSV0),
	 
	XOADC_CHAN(ATEST_8, 0x01, 0x00, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	 
	XOADC_CHAN(USB_SNS_DIV20, 0x01, 0x01, IIO_VOLTAGE, 1, 2, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(DCIN_SNS_DIV20, 0x01, 0x02, IIO_VOLTAGE, 1, 2, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX3, 0x01, 0x03, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX4, 0x01, 0x04, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX5, 0x01, 0x05, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX6, 0x01, 0x06, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX7, 0x01, 0x07, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX8, 0x01, 0x08, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	 
	XOADC_CHAN(ATEST_1, 0x01, 0x09, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ATEST_2, 0x01, 0x0a, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ATEST_3, 0x01, 0x0b, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ATEST_4, 0x01, 0x0c, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ATEST_5, 0x01, 0x0d, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ATEST_6, 0x01, 0x0e, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ATEST_7, 0x01, 0x0f, IIO_VOLTAGE, 1, 1, SCALE_DEFAULT, AMUX_RSV1),
	 
	 
	XOADC_CHAN(ATEST_8, 0x02, 0x00, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	 
	XOADC_CHAN(USB_SNS_DIV20_DIV3, 0x02, 0x01, IIO_VOLTAGE, 1, 6, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(DCIN_SNS_DIV20_DIV3, 0x02, 0x02, IIO_VOLTAGE, 1, 6, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX3_DIV3, 0x02, 0x03, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX4_DIV3, 0x02, 0x04, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX5_DIV3, 0x02, 0x05, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX6_DIV3, 0x02, 0x06, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX7_DIV3, 0x02, 0x07, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(AMUX8_DIV3, 0x02, 0x08, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ATEST_1_DIV3, 0x02, 0x09, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ATEST_2_DIV3, 0x02, 0x0a, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ATEST_3_DIV3, 0x02, 0x0b, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ATEST_4_DIV3, 0x02, 0x0c, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ATEST_5_DIV3, 0x02, 0x0d, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ATEST_6_DIV3, 0x02, 0x0e, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	XOADC_CHAN(ATEST_7_DIV3, 0x02, 0x0f, IIO_VOLTAGE, 1, 3, SCALE_DEFAULT, AMUX_RSV1),
	{ },  
};

 
struct pm8xxx_chan_info {
	const char *name;
	const struct xoadc_channel *hwchan;
	enum vadc_calibration calibration;
	u8 decimation:2;
	u8 amux_ip_rsv:3;
};

 
struct pm8xxx_xoadc {
	struct device *dev;
	struct regmap *map;
	const struct xoadc_variant *variant;
	struct regulator *vref;
	unsigned int nchans;
	struct pm8xxx_chan_info *chans;
	struct iio_chan_spec *iio_chans;
	struct vadc_linear_graph graph[2];
	struct completion complete;
	struct mutex lock;
};

static irqreturn_t pm8xxx_eoc_irq(int irq, void *d)
{
	struct iio_dev *indio_dev = d;
	struct pm8xxx_xoadc *adc = iio_priv(indio_dev);

	complete(&adc->complete);

	return IRQ_HANDLED;
}

static struct pm8xxx_chan_info *
pm8xxx_get_channel(struct pm8xxx_xoadc *adc, u8 chan)
{
	int i;

	for (i = 0; i < adc->nchans; i++) {
		struct pm8xxx_chan_info *ch = &adc->chans[i];
		if (ch->hwchan->amux_channel == chan)
			return ch;
	}
	return NULL;
}

static int pm8xxx_read_channel_rsv(struct pm8xxx_xoadc *adc,
				   const struct pm8xxx_chan_info *ch,
				   u8 rsv, u16 *adc_code,
				   bool force_ratiometric)
{
	int ret;
	unsigned int val;
	u8 rsvmask, rsvval;
	u8 lsb, msb;

	dev_dbg(adc->dev, "read channel \"%s\", amux %d, prescale/mux: %d, rsv %d\n",
		ch->name, ch->hwchan->amux_channel, ch->hwchan->pre_scale_mux, rsv);

	mutex_lock(&adc->lock);

	 
	val = ch->hwchan->amux_channel << ADC_AMUX_SEL_SHIFT;
	val |= ch->hwchan->pre_scale_mux << ADC_AMUX_PREMUX_SHIFT;
	ret = regmap_write(adc->map, ADC_ARB_USRP_AMUX_CNTRL, val);
	if (ret)
		goto unlock;

	 
	rsvmask = (ADC_ARB_USRP_RSV_RST | ADC_ARB_USRP_RSV_DTEST0 |
		   ADC_ARB_USRP_RSV_DTEST1 | ADC_ARB_USRP_RSV_OP);
	if (adc->variant->broken_ratiometric && !force_ratiometric) {
		 
		if (ch->hwchan->amux_channel == PM8XXX_CHANNEL_MUXOFF)
			rsvval = ADC_ARB_USRP_RSV_IP_SEL0;
		else
			rsvval = ADC_ARB_USRP_RSV_IP_SEL1;
	} else {
		if (rsv == 0xff)
			rsvval = (ch->amux_ip_rsv << ADC_RSV_IP_SEL_SHIFT) |
				ADC_ARB_USRP_RSV_TRM;
		else
			rsvval = (rsv << ADC_RSV_IP_SEL_SHIFT) |
				ADC_ARB_USRP_RSV_TRM;
	}

	ret = regmap_update_bits(adc->map,
				 ADC_ARB_USRP_RSV,
				 ~rsvmask,
				 rsvval);
	if (ret)
		goto unlock;

	ret = regmap_write(adc->map, ADC_ARB_USRP_ANA_PARAM,
			   ADC_ARB_USRP_ANA_PARAM_DIS);
	if (ret)
		goto unlock;

	 
	ret = regmap_write(adc->map, ADC_ARB_USRP_DIG_PARAM,
			   ADC_ARB_USRP_DIG_PARAM_SEL_SHIFT0 |
			   ADC_ARB_USRP_DIG_PARAM_SEL_SHIFT1 |
			   ch->decimation << ADC_DIG_PARAM_DEC_SHIFT);
	if (ret)
		goto unlock;

	ret = regmap_write(adc->map, ADC_ARB_USRP_ANA_PARAM,
			   ADC_ARB_USRP_ANA_PARAM_EN);
	if (ret)
		goto unlock;

	 
	ret = regmap_write(adc->map, ADC_ARB_USRP_CNTRL,
			   ADC_ARB_USRP_CNTRL_EN_ARB);
	if (ret)
		goto unlock;
	ret = regmap_write(adc->map, ADC_ARB_USRP_CNTRL,
			   ADC_ARB_USRP_CNTRL_EN_ARB);
	if (ret)
		goto unlock;


	 
	reinit_completion(&adc->complete);
	ret = regmap_write(adc->map, ADC_ARB_USRP_CNTRL,
			   ADC_ARB_USRP_CNTRL_EN_ARB |
			   ADC_ARB_USRP_CNTRL_REQ);
	if (ret)
		goto unlock;

	 
	ret = wait_for_completion_timeout(&adc->complete,
					  VADC_CONV_TIME_MAX_US);
	if (!ret) {
		dev_err(adc->dev, "conversion timed out\n");
		ret = -ETIMEDOUT;
		goto unlock;
	}

	ret = regmap_read(adc->map, ADC_ARB_USRP_DATA0, &val);
	if (ret)
		goto unlock;
	lsb = val;
	ret = regmap_read(adc->map, ADC_ARB_USRP_DATA1, &val);
	if (ret)
		goto unlock;
	msb = val;
	*adc_code = (msb << 8) | lsb;

	 
	ret = regmap_write(adc->map, ADC_ARB_USRP_CNTRL, 0);
	if (ret)
		goto unlock;
	ret = regmap_write(adc->map, ADC_ARB_USRP_CNTRL, 0);
	if (ret)
		goto unlock;

unlock:
	mutex_unlock(&adc->lock);
	return ret;
}

static int pm8xxx_read_channel(struct pm8xxx_xoadc *adc,
			       const struct pm8xxx_chan_info *ch,
			       u16 *adc_code)
{
	 
	return pm8xxx_read_channel_rsv(adc, ch, 0xff, adc_code, false);
}

static int pm8xxx_calibrate_device(struct pm8xxx_xoadc *adc)
{
	const struct pm8xxx_chan_info *ch;
	u16 read_1250v;
	u16 read_0625v;
	u16 read_nomux_rsv5;
	u16 read_nomux_rsv4;
	int ret;

	adc->graph[VADC_CALIB_ABSOLUTE].dx = VADC_ABSOLUTE_RANGE_UV;
	adc->graph[VADC_CALIB_RATIOMETRIC].dx = VADC_RATIOMETRIC_RANGE;

	 
	ch = pm8xxx_get_channel(adc, PM8XXX_CHANNEL_125V);
	if (!ch)
		return -ENODEV;
	ret = pm8xxx_read_channel(adc, ch, &read_1250v);
	if (ret) {
		dev_err(adc->dev, "could not read 1.25V reference channel\n");
		return -ENODEV;
	}
	ch = pm8xxx_get_channel(adc, PM8XXX_CHANNEL_INTERNAL);
	if (!ch)
		return -ENODEV;
	ret = pm8xxx_read_channel(adc, ch, &read_0625v);
	if (ret) {
		dev_err(adc->dev, "could not read 0.625V reference channel\n");
		return -ENODEV;
	}
	if (read_1250v == read_0625v) {
		dev_err(adc->dev, "read same ADC code for 1.25V and 0.625V\n");
		return -ENODEV;
	}

	adc->graph[VADC_CALIB_ABSOLUTE].dy = read_1250v - read_0625v;
	adc->graph[VADC_CALIB_ABSOLUTE].gnd = read_0625v;

	dev_info(adc->dev, "absolute calibration dx = %d uV, dy = %d units\n",
		 VADC_ABSOLUTE_RANGE_UV, adc->graph[VADC_CALIB_ABSOLUTE].dy);

	 
	ch = pm8xxx_get_channel(adc, PM8XXX_CHANNEL_MUXOFF);
	if (!ch)
		return -ENODEV;
	ret = pm8xxx_read_channel_rsv(adc, ch, AMUX_RSV5,
				      &read_nomux_rsv5, true);
	if (ret) {
		dev_err(adc->dev, "could not read MUXOFF reference channel\n");
		return -ENODEV;
	}
	ret = pm8xxx_read_channel_rsv(adc, ch, AMUX_RSV4,
				      &read_nomux_rsv4, true);
	if (ret) {
		dev_err(adc->dev, "could not read MUXOFF reference channel\n");
		return -ENODEV;
	}
	adc->graph[VADC_CALIB_RATIOMETRIC].dy =
		read_nomux_rsv5 - read_nomux_rsv4;
	adc->graph[VADC_CALIB_RATIOMETRIC].gnd = read_nomux_rsv4;

	dev_info(adc->dev, "ratiometric calibration dx = %d, dy = %d units\n",
		 VADC_RATIOMETRIC_RANGE,
		 adc->graph[VADC_CALIB_RATIOMETRIC].dy);

	return 0;
}

static int pm8xxx_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2, long mask)
{
	struct pm8xxx_xoadc *adc = iio_priv(indio_dev);
	const struct pm8xxx_chan_info *ch;
	u16 adc_code;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
		ch = pm8xxx_get_channel(adc, chan->address);
		if (!ch) {
			dev_err(adc->dev, "no such channel %lu\n",
				chan->address);
			return -EINVAL;
		}
		ret = pm8xxx_read_channel(adc, ch, &adc_code);
		if (ret)
			return ret;

		ret = qcom_vadc_scale(ch->hwchan->scale_fn_type,
				      &adc->graph[ch->calibration],
				      &ch->hwchan->prescale,
				      (ch->calibration == VADC_CALIB_ABSOLUTE),
				      adc_code, val);
		if (ret)
			return ret;

		return IIO_VAL_INT;
	case IIO_CHAN_INFO_RAW:
		ch = pm8xxx_get_channel(adc, chan->address);
		if (!ch) {
			dev_err(adc->dev, "no such channel %lu\n",
				chan->address);
			return -EINVAL;
		}
		ret = pm8xxx_read_channel(adc, ch, &adc_code);
		if (ret)
			return ret;

		*val = (int)adc_code;
		return IIO_VAL_INT;
	default:
		return -EINVAL;
	}
}

static int pm8xxx_fwnode_xlate(struct iio_dev *indio_dev,
			       const struct fwnode_reference_args *iiospec)
{
	struct pm8xxx_xoadc *adc = iio_priv(indio_dev);
	u8 pre_scale_mux;
	u8 amux_channel;
	unsigned int i;

	 
	if (iiospec->nargs != 2) {
		dev_err(&indio_dev->dev, "wrong number of arguments for %pfwP need 2 got %d\n",
			iiospec->fwnode,
			iiospec->nargs);
		return -EINVAL;
	}
	pre_scale_mux = (u8)iiospec->args[0];
	amux_channel = (u8)iiospec->args[1];
	dev_dbg(&indio_dev->dev, "pre scale/mux: %02x, amux: %02x\n",
		pre_scale_mux, amux_channel);

	 
	for (i = 0; i < adc->nchans; i++)
		if (adc->chans[i].hwchan->pre_scale_mux == pre_scale_mux &&
		    adc->chans[i].hwchan->amux_channel == amux_channel)
			return i;

	return -EINVAL;
}

static const struct iio_info pm8xxx_xoadc_info = {
	.fwnode_xlate = pm8xxx_fwnode_xlate,
	.read_raw = pm8xxx_read_raw,
};

static int pm8xxx_xoadc_parse_channel(struct device *dev,
				      struct fwnode_handle *fwnode,
				      const struct xoadc_channel *hw_channels,
				      struct iio_chan_spec *iio_chan,
				      struct pm8xxx_chan_info *ch)
{
	const char *name = fwnode_get_name(fwnode);
	const struct xoadc_channel *hwchan;
	u32 pre_scale_mux, amux_channel, reg[2];
	u32 rsv, dec;
	int ret;
	int chid;

	ret = fwnode_property_read_u32_array(fwnode, "reg", reg,
					     ARRAY_SIZE(reg));
	if (ret) {
		dev_err(dev, "invalid pre scale/mux or amux channel number %s\n",
			name);
		return ret;
	}

	pre_scale_mux = reg[0];
	amux_channel = reg[1];

	 
	chid = 0;
	hwchan = &hw_channels[0];
	while (hwchan->datasheet_name) {
		if (hwchan->pre_scale_mux == pre_scale_mux &&
		    hwchan->amux_channel == amux_channel)
			break;
		hwchan++;
		chid++;
	}
	 
	if (!hwchan->datasheet_name) {
		dev_err(dev, "could not locate channel %02x/%02x\n",
			pre_scale_mux, amux_channel);
		return -EINVAL;
	}
	ch->name = name;
	ch->hwchan = hwchan;
	 
	ch->calibration = VADC_CALIB_ABSOLUTE;
	 
	ch->decimation = VADC_DEF_DECIMATION;

	if (!fwnode_property_read_u32(fwnode, "qcom,ratiometric", &rsv)) {
		ch->calibration = VADC_CALIB_RATIOMETRIC;
		if (rsv > XOADC_RSV_MAX) {
			dev_err(dev, "%s too large RSV value %d\n", name, rsv);
			return -EINVAL;
		}
		if (rsv == AMUX_RSV3) {
			dev_err(dev, "%s invalid RSV value %d\n", name, rsv);
			return -EINVAL;
		}
	}

	 
	ret = fwnode_property_read_u32(fwnode, "qcom,decimation", &dec);
	if (!ret) {
		ret = qcom_vadc_decimation_from_dt(dec);
		if (ret < 0) {
			dev_err(dev, "%s invalid decimation %d\n",
				name, dec);
			return ret;
		}
		ch->decimation = ret;
	}

	iio_chan->channel = chid;
	iio_chan->address = hwchan->amux_channel;
	iio_chan->datasheet_name = hwchan->datasheet_name;
	iio_chan->type = hwchan->type;
	 
	iio_chan->info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
		BIT(IIO_CHAN_INFO_PROCESSED);
	iio_chan->indexed = 1;

	dev_dbg(dev,
		"channel [PRESCALE/MUX: %02x AMUX: %02x] \"%s\" ref voltage: %d, decimation %d prescale %d/%d, scale function %d\n",
		hwchan->pre_scale_mux, hwchan->amux_channel, ch->name,
		ch->amux_ip_rsv, ch->decimation, hwchan->prescale.numerator,
		hwchan->prescale.denominator, hwchan->scale_fn_type);

	return 0;
}

static int pm8xxx_xoadc_parse_channels(struct pm8xxx_xoadc *adc)
{
	struct fwnode_handle *child;
	struct pm8xxx_chan_info *ch;
	int ret;
	int i;

	adc->nchans = device_get_child_node_count(adc->dev);
	if (!adc->nchans) {
		dev_err(adc->dev, "no channel children\n");
		return -ENODEV;
	}
	dev_dbg(adc->dev, "found %d ADC channels\n", adc->nchans);

	adc->iio_chans = devm_kcalloc(adc->dev, adc->nchans,
				      sizeof(*adc->iio_chans), GFP_KERNEL);
	if (!adc->iio_chans)
		return -ENOMEM;

	adc->chans = devm_kcalloc(adc->dev, adc->nchans,
				  sizeof(*adc->chans), GFP_KERNEL);
	if (!adc->chans)
		return -ENOMEM;

	i = 0;
	device_for_each_child_node(adc->dev, child) {
		ch = &adc->chans[i];
		ret = pm8xxx_xoadc_parse_channel(adc->dev, child,
						 adc->variant->channels,
						 &adc->iio_chans[i],
						 ch);
		if (ret) {
			fwnode_handle_put(child);
			return ret;
		}
		i++;
	}

	 
	ch = pm8xxx_get_channel(adc, PM8XXX_CHANNEL_125V);
	if (!ch) {
		dev_err(adc->dev, "missing 1.25V reference channel\n");
		return -ENODEV;
	}
	ch = pm8xxx_get_channel(adc, PM8XXX_CHANNEL_INTERNAL);
	if (!ch) {
		dev_err(adc->dev, "missing 0.625V reference channel\n");
		return -ENODEV;
	}
	ch = pm8xxx_get_channel(adc, PM8XXX_CHANNEL_MUXOFF);
	if (!ch) {
		dev_err(adc->dev, "missing MUXOFF reference channel\n");
		return -ENODEV;
	}

	return 0;
}

static int pm8xxx_xoadc_probe(struct platform_device *pdev)
{
	const struct xoadc_variant *variant;
	struct pm8xxx_xoadc *adc;
	struct iio_dev *indio_dev;
	struct regmap *map;
	struct device *dev = &pdev->dev;
	int ret;

	variant = device_get_match_data(dev);
	if (!variant)
		return -ENODEV;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*adc));
	if (!indio_dev)
		return -ENOMEM;
	platform_set_drvdata(pdev, indio_dev);

	adc = iio_priv(indio_dev);
	adc->dev = dev;
	adc->variant = variant;
	init_completion(&adc->complete);
	mutex_init(&adc->lock);

	ret = pm8xxx_xoadc_parse_channels(adc);
	if (ret)
		return ret;

	map = dev_get_regmap(dev->parent, NULL);
	if (!map) {
		dev_err(dev, "parent regmap unavailable.\n");
		return -ENODEV;
	}
	adc->map = map;

	 
	adc->vref = devm_regulator_get(dev, "xoadc-ref");
	if (IS_ERR(adc->vref))
		return dev_err_probe(dev, PTR_ERR(adc->vref),
				     "failed to get XOADC VREF regulator\n");
	ret = regulator_enable(adc->vref);
	if (ret) {
		dev_err(dev, "failed to enable XOADC VREF regulator\n");
		return ret;
	}

	ret = devm_request_threaded_irq(dev, platform_get_irq(pdev, 0),
			pm8xxx_eoc_irq, NULL, 0, variant->name, indio_dev);
	if (ret) {
		dev_err(dev, "unable to request IRQ\n");
		goto out_disable_vref;
	}

	indio_dev->name = variant->name;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &pm8xxx_xoadc_info;
	indio_dev->channels = adc->iio_chans;
	indio_dev->num_channels = adc->nchans;

	ret = iio_device_register(indio_dev);
	if (ret)
		goto out_disable_vref;

	ret = pm8xxx_calibrate_device(adc);
	if (ret)
		goto out_unreg_device;

	dev_info(dev, "%s XOADC driver enabled\n", variant->name);

	return 0;

out_unreg_device:
	iio_device_unregister(indio_dev);
out_disable_vref:
	regulator_disable(adc->vref);

	return ret;
}

static int pm8xxx_xoadc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct pm8xxx_xoadc *adc = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);

	regulator_disable(adc->vref);

	return 0;
}

static const struct xoadc_variant pm8018_variant = {
	.name = "PM8018-XOADC",
	.channels = pm8018_xoadc_channels,
};

static const struct xoadc_variant pm8038_variant = {
	.name = "PM8038-XOADC",
	.channels = pm8038_xoadc_channels,
};

static const struct xoadc_variant pm8058_variant = {
	.name = "PM8058-XOADC",
	.channels = pm8058_xoadc_channels,
	.broken_ratiometric = true,
	.prescaling = true,
};

static const struct xoadc_variant pm8921_variant = {
	.name = "PM8921-XOADC",
	.channels = pm8921_xoadc_channels,
	.second_level_mux = true,
};

static const struct of_device_id pm8xxx_xoadc_id_table[] = {
	{
		.compatible = "qcom,pm8018-adc",
		.data = &pm8018_variant,
	},
	{
		.compatible = "qcom,pm8038-adc",
		.data = &pm8038_variant,
	},
	{
		.compatible = "qcom,pm8058-adc",
		.data = &pm8058_variant,
	},
	{
		.compatible = "qcom,pm8921-adc",
		.data = &pm8921_variant,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, pm8xxx_xoadc_id_table);

static struct platform_driver pm8xxx_xoadc_driver = {
	.driver		= {
		.name	= "pm8xxx-adc",
		.of_match_table = pm8xxx_xoadc_id_table,
	},
	.probe		= pm8xxx_xoadc_probe,
	.remove		= pm8xxx_xoadc_remove,
};
module_platform_driver(pm8xxx_xoadc_driver);

MODULE_DESCRIPTION("PM8xxx XOADC driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:pm8xxx-xoadc");
