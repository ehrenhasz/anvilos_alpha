
 

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/completion.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/of.h>
#include <linux/mfd/core.h>
#include <linux/mfd/abx500/ab8500.h>
#include <linux/mfd/abx500.h>
#include <linux/usb/otg.h>
#include <linux/mutex.h>
#include <linux/iio/consumer.h>

#include "ab8500-bm.h"
#include "ab8500-chargalg.h"

 
#define NO_PW_CONN			0
#define AC_PW_CONN			1
#define USB_PW_CONN			2

#define MAIN_WDOG_ENA			0x01
#define MAIN_WDOG_KICK			0x02
#define MAIN_WDOG_DIS			0x00
#define CHARG_WD_KICK			0x01
#define MAIN_CH_ENA			0x01
#define MAIN_CH_NO_OVERSHOOT_ENA_N	0x02
#define USB_CH_ENA			0x01
#define USB_CHG_NO_OVERSHOOT_ENA_N	0x02
#define MAIN_CH_DET			0x01
#define MAIN_CH_CV_ON			0x04
#define USB_CH_CV_ON			0x08
#define VBUS_DET_DBNC100		0x02
#define VBUS_DET_DBNC1			0x01
#define OTP_ENABLE_WD			0x01
#define DROP_COUNT_RESET		0x01
#define USB_CH_DET			0x01

#define MAIN_CH_INPUT_CURR_SHIFT	4
#define VBUS_IN_CURR_LIM_SHIFT		4
#define AUTO_VBUS_IN_CURR_LIM_SHIFT	4
#define VBUS_IN_CURR_LIM_RETRY_SET_TIME	30  

#define LED_INDICATOR_PWM_ENA		0x01
#define LED_INDICATOR_PWM_DIS		0x00
#define LED_IND_CUR_5MA			0x04
#define LED_INDICATOR_PWM_DUTY_252_256	0xBF

 
#define MAIN_CH_TH_PROT			0x02
#define VBUS_CH_NOK			0x08
#define USB_CH_TH_PROT			0x02
#define VBUS_OVV_TH			0x01
#define MAIN_CH_NOK			0x01
#define VBUS_DET			0x80

#define MAIN_CH_STATUS2_MAINCHGDROP		0x80
#define MAIN_CH_STATUS2_MAINCHARGERDETDBNC	0x40
#define USB_CH_VBUSDROP				0x40
#define USB_CH_VBUSDETDBNC			0x01

 
#define AB8500_USB_LINK_STATUS		0x78
#define AB8505_USB_LINK_STATUS		0xF8
#define AB8500_STD_HOST_SUSP		0x18
#define USB_LINK_STATUS_SHIFT		3

 
#define WD_TIMER			0x30  
#define WD_KICK_INTERVAL		(60 * HZ)

 
#define LOW_VOLT_REG			0x4E

 
#define STEP_UDELAY			1000

#define CHARGER_STATUS_POLL 10  

#define CHG_WD_INTERVAL			(60 * HZ)

#define AB8500_SW_CONTROL_FALLBACK	0x03
 
#define WAIT_ACA_RID_ENUMERATION	(5 * 1000)
 
#define AB8500_SYS_CHARGER_CONTROL_REG		0x52
#define EXTERNAL_CHARGER_DISABLE_REG_VAL	0x03
#define EXTERNAL_CHARGER_ENABLE_REG_VAL		0x07

 
enum ab8500_charger_link_status {
	USB_STAT_NOT_CONFIGURED,
	USB_STAT_STD_HOST_NC,
	USB_STAT_STD_HOST_C_NS,
	USB_STAT_STD_HOST_C_S,
	USB_STAT_HOST_CHG_NM,
	USB_STAT_HOST_CHG_HS,
	USB_STAT_HOST_CHG_HS_CHIRP,
	USB_STAT_DEDICATED_CHG,
	USB_STAT_ACA_RID_A,
	USB_STAT_ACA_RID_B,
	USB_STAT_ACA_RID_C_NM,
	USB_STAT_ACA_RID_C_HS,
	USB_STAT_ACA_RID_C_HS_CHIRP,
	USB_STAT_HM_IDGND,
	USB_STAT_RESERVED,
	USB_STAT_NOT_VALID_LINK,
	USB_STAT_PHY_EN,
	USB_STAT_SUP_NO_IDGND_VBUS,
	USB_STAT_SUP_IDGND_VBUS,
	USB_STAT_CHARGER_LINE_1,
	USB_STAT_CARKIT_1,
	USB_STAT_CARKIT_2,
	USB_STAT_ACA_DOCK_CHARGER,
};

enum ab8500_usb_state {
	AB8500_BM_USB_STATE_RESET_HS,	 
	AB8500_BM_USB_STATE_RESET_FS,	 
	AB8500_BM_USB_STATE_CONFIGURED,
	AB8500_BM_USB_STATE_SUSPEND,
	AB8500_BM_USB_STATE_RESUME,
	AB8500_BM_USB_STATE_MAX,
};

 
#define USB_CH_IP_CUR_LVL_0P05		50000
#define USB_CH_IP_CUR_LVL_0P09		98000
#define USB_CH_IP_CUR_LVL_0P19		193000
#define USB_CH_IP_CUR_LVL_0P29		290000
#define USB_CH_IP_CUR_LVL_0P38		380000
#define USB_CH_IP_CUR_LVL_0P45		450000
#define USB_CH_IP_CUR_LVL_0P5		500000
#define USB_CH_IP_CUR_LVL_0P6		600000
#define USB_CH_IP_CUR_LVL_0P7		700000
#define USB_CH_IP_CUR_LVL_0P8		800000
#define USB_CH_IP_CUR_LVL_0P9		900000
#define USB_CH_IP_CUR_LVL_1P0		1000000
#define USB_CH_IP_CUR_LVL_1P1		1100000
#define USB_CH_IP_CUR_LVL_1P3		1300000
#define USB_CH_IP_CUR_LVL_1P4		1400000
#define USB_CH_IP_CUR_LVL_1P5		1500000

#define VBAT_TRESH_IP_CUR_RED		3800000

#define to_ab8500_charger_usb_device_info(x) container_of((x), \
	struct ab8500_charger, usb_chg)
#define to_ab8500_charger_ac_device_info(x) container_of((x), \
	struct ab8500_charger, ac_chg)

 
struct ab8500_charger_interrupts {
	char *name;
	irqreturn_t (*isr)(int irq, void *data);
};

struct ab8500_charger_info {
	int charger_connected;
	int charger_online;
	int charger_voltage_uv;
	int cv_active;
	bool wd_expired;
	int charger_current_ua;
};

struct ab8500_charger_event_flags {
	bool mainextchnotok;
	bool main_thermal_prot;
	bool usb_thermal_prot;
	bool vbus_ovv;
	bool usbchargernotok;
	bool chgwdexp;
	bool vbus_collapse;
	bool vbus_drop_end;
};

struct ab8500_charger_usb_state {
	int usb_current_ua;
	int usb_current_tmp_ua;
	enum ab8500_usb_state state;
	enum ab8500_usb_state state_tmp;
	spinlock_t usb_lock;
};

struct ab8500_charger_max_usb_in_curr {
	int usb_type_max_ua;
	int set_max_ua;
	int calculated_max_ua;
};

 
struct ab8500_charger {
	struct device *dev;
	bool vbus_detected;
	bool vbus_detected_start;
	bool ac_conn;
	bool vddadc_en_ac;
	bool vddadc_en_usb;
	int vbat;
	int old_vbat;
	bool usb_device_is_unrecognised;
	bool autopower;
	bool autopower_cfg;
	int invalid_charger_detect_state;
	int is_aca_rid;
	atomic_t current_stepping_sessions;
	struct ab8500 *parent;
	struct iio_channel *adc_main_charger_v;
	struct iio_channel *adc_main_charger_c;
	struct iio_channel *adc_vbus_v;
	struct iio_channel *adc_usb_charger_c;
	struct ab8500_bm_data *bm;
	struct ab8500_charger_event_flags flags;
	struct ab8500_charger_usb_state usb_state;
	struct ab8500_charger_max_usb_in_curr max_usb_in_curr;
	struct ux500_charger ac_chg;
	struct ux500_charger usb_chg;
	struct ab8500_charger_info ac;
	struct ab8500_charger_info usb;
	struct regulator *regu;
	struct workqueue_struct *charger_wq;
	struct mutex usb_ipt_crnt_lock;
	struct delayed_work check_vbat_work;
	struct delayed_work check_hw_failure_work;
	struct delayed_work check_usbchgnotok_work;
	struct delayed_work kick_wd_work;
	struct delayed_work usb_state_changed_work;
	struct delayed_work attach_work;
	struct delayed_work ac_charger_attached_work;
	struct delayed_work usb_charger_attached_work;
	struct delayed_work vbus_drop_end_work;
	struct work_struct ac_work;
	struct work_struct detect_usb_type_work;
	struct work_struct usb_link_status_work;
	struct work_struct check_main_thermal_prot_work;
	struct work_struct check_usb_thermal_prot_work;
	struct usb_phy *usb_phy;
	struct notifier_block nb;
	struct mutex charger_attached_mutex;
};

 
static enum power_supply_property ab8500_charger_ac_props[] = {
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

 
static enum power_supply_property ab8500_charger_usb_props[] = {
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

 
static void ab8500_enable_disable_sw_fallback(struct ab8500_charger *di,
		bool fallback)
{
	u8 val;
	u8 reg;
	u8 bank;
	u8 bit;
	int ret;

	dev_dbg(di->dev, "SW Fallback: %d\n", fallback);

	if (is_ab8500(di->parent)) {
		bank = 0x15;
		reg = 0x0;
		bit = 3;
	} else {
		bank = AB8500_SYS_CTRL1_BLOCK;
		reg = AB8500_SW_CONTROL_FALLBACK;
		bit = 0;
	}

	 
	ret = abx500_get_register_interruptible(di->dev, bank, reg, &val);
	if (ret < 0) {
		dev_err(di->dev, "%d read failed\n", __LINE__);
		return;
	}

	if (is_ab8500(di->parent)) {
		 
		ret = abx500_set_register_interruptible(di->dev, 0x11, 0x00, 0x2);
		if (ret) {
			dev_err(di->dev, "%d write failed\n", __LINE__);
			goto disable_otp;
		}
	}

	if (fallback)
		val |= (1 << bit);
	else
		val &= ~(1 << bit);

	 
	ret = abx500_set_register_interruptible(di->dev, bank, reg, val);
	if (ret) {
		dev_err(di->dev, "%d write failed\n", __LINE__);
	}

disable_otp:
	if (is_ab8500(di->parent)) {
		 
		ret = abx500_set_register_interruptible(di->dev, 0x11, 0x00, 0x0);
		if (ret) {
			dev_err(di->dev, "%d write failed\n", __LINE__);
		}
	}
}

 
static void ab8500_power_supply_changed(struct ab8500_charger *di,
					struct power_supply *psy)
{
	 
	if (!psy)
		return;

	if (di->autopower_cfg) {
		if (!di->usb.charger_connected &&
		    !di->ac.charger_connected &&
		    di->autopower) {
			di->autopower = false;
			ab8500_enable_disable_sw_fallback(di, false);
		} else if (!di->autopower &&
			   (di->ac.charger_connected ||
			    di->usb.charger_connected)) {
			di->autopower = true;
			ab8500_enable_disable_sw_fallback(di, true);
		}
	}
	power_supply_changed(psy);
}

static void ab8500_charger_set_usb_connected(struct ab8500_charger *di,
	bool connected)
{
	if (connected != di->usb.charger_connected) {
		dev_dbg(di->dev, "USB connected:%i\n", connected);
		di->usb.charger_connected = connected;

		if (!connected)
			di->flags.vbus_drop_end = false;

		 
		if (di->usb_chg.psy) {
			sysfs_notify(&di->usb_chg.psy->dev.kobj, NULL,
				     "present");
		}

		if (connected) {
			mutex_lock(&di->charger_attached_mutex);
			mutex_unlock(&di->charger_attached_mutex);

			if (is_ab8500(di->parent))
				queue_delayed_work(di->charger_wq,
					   &di->usb_charger_attached_work,
					   HZ);
		} else {
			cancel_delayed_work_sync(&di->usb_charger_attached_work);
			mutex_lock(&di->charger_attached_mutex);
			mutex_unlock(&di->charger_attached_mutex);
		}
	}
}

 
static int ab8500_charger_get_ac_voltage(struct ab8500_charger *di)
{
	int vch, ret;

	 
	if (di->ac.charger_connected) {
		ret = iio_read_channel_processed(di->adc_main_charger_v, &vch);
		if (ret < 0)
			dev_err(di->dev, "%s ADC conv failed,\n", __func__);
	} else {
		vch = 0;
	}
	 
	return vch * 1000;
}

 
static int ab8500_charger_ac_cv(struct ab8500_charger *di)
{
	u8 val;
	int ret = 0;

	 
	if (di->ac.charger_online) {
		ret = abx500_get_register_interruptible(di->dev, AB8500_CHARGER,
			AB8500_CH_STATUS1_REG, &val);
		if (ret < 0) {
			dev_err(di->dev, "%s ab8500 read failed\n", __func__);
			return 0;
		}

		if (val & MAIN_CH_CV_ON)
			ret = 1;
		else
			ret = 0;
	}

	return ret;
}

 
static int ab8500_charger_get_vbus_voltage(struct ab8500_charger *di)
{
	int vch, ret;

	 
	if (di->usb.charger_connected) {
		ret = iio_read_channel_processed(di->adc_vbus_v, &vch);
		if (ret < 0)
			dev_err(di->dev, "%s ADC conv failed,\n", __func__);
	} else {
		vch = 0;
	}
	 
	return vch * 1000;
}

 
static int ab8500_charger_get_usb_current(struct ab8500_charger *di)
{
	int ich, ret;

	 
	if (di->usb.charger_online) {
		ret = iio_read_channel_processed(di->adc_usb_charger_c, &ich);
		if (ret < 0)
			dev_err(di->dev, "%s ADC conv failed,\n", __func__);
	} else {
		ich = 0;
	}
	 
	return ich * 1000;
}

 
static int ab8500_charger_get_ac_current(struct ab8500_charger *di)
{
	int ich, ret;

	 
	if (di->ac.charger_online) {
		ret = iio_read_channel_processed(di->adc_main_charger_c, &ich);
		if (ret < 0)
			dev_err(di->dev, "%s ADC conv failed,\n", __func__);
	} else {
		ich = 0;
	}
	 
	return ich * 1000;
}

 
static int ab8500_charger_usb_cv(struct ab8500_charger *di)
{
	int ret;
	u8 val;

	 
	if (di->usb.charger_online) {
		ret = abx500_get_register_interruptible(di->dev, AB8500_CHARGER,
			AB8500_CH_USBCH_STAT1_REG, &val);
		if (ret < 0) {
			dev_err(di->dev, "%s ab8500 read failed\n", __func__);
			return 0;
		}

		if (val & USB_CH_CV_ON)
			ret = 1;
		else
			ret = 0;
	} else {
		ret = 0;
	}

	return ret;
}

 
static int ab8500_charger_detect_chargers(struct ab8500_charger *di, bool probe)
{
	int result = NO_PW_CONN;
	int ret;
	u8 val;

	 
	ret = abx500_get_register_interruptible(di->dev, AB8500_CHARGER,
		AB8500_CH_STATUS1_REG, &val);
	if (ret < 0) {
		dev_err(di->dev, "%s ab8500 read failed\n", __func__);
		return ret;
	}

	if (val & MAIN_CH_DET)
		result = AC_PW_CONN;

	 

	if (!probe) {
		 
		msleep(110);
	}
	ret = abx500_get_register_interruptible(di->dev, AB8500_CHARGER,
		AB8500_CH_USBCH_STAT1_REG, &val);
	if (ret < 0) {
		dev_err(di->dev, "%s ab8500 read failed\n", __func__);
		return ret;
	}
	dev_dbg(di->dev,
		"%s AB8500_CH_USBCH_STAT1_REG %x\n", __func__,
		val);
	if ((val & VBUS_DET_DBNC1) && (val & VBUS_DET_DBNC100))
		result |= USB_PW_CONN;

	return result;
}

 
static int ab8500_charger_max_usb_curr(struct ab8500_charger *di,
		enum ab8500_charger_link_status link_status)
{
	int ret = 0;

	di->usb_device_is_unrecognised = false;

	 

	switch (link_status) {
	case USB_STAT_STD_HOST_NC:
	case USB_STAT_STD_HOST_C_NS:
	case USB_STAT_STD_HOST_C_S:
		dev_dbg(di->dev, "USB Type - Standard host is "
			"detected through USB driver\n");
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P5;
		di->is_aca_rid = 0;
		break;
	case USB_STAT_HOST_CHG_HS_CHIRP:
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P5;
		di->is_aca_rid = 0;
		break;
	case USB_STAT_HOST_CHG_HS:
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P5;
		di->is_aca_rid = 0;
		break;
	case USB_STAT_ACA_RID_C_HS:
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P9;
		di->is_aca_rid = 0;
		break;
	case USB_STAT_ACA_RID_A:
		 
		dev_dbg(di->dev, "USB_STAT_ACA_RID_A detected\n");
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P5;
		di->is_aca_rid = 1;
		break;
	case USB_STAT_ACA_RID_B:
		 
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_1P3;
		dev_dbg(di->dev, "USB Type - 0x%02x MaxCurr: %d", link_status,
				di->max_usb_in_curr.usb_type_max_ua);
		di->is_aca_rid = 1;
		break;
	case USB_STAT_HOST_CHG_NM:
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P5;
		di->is_aca_rid = 0;
		break;
	case USB_STAT_DEDICATED_CHG:
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_1P5;
		di->is_aca_rid = 0;
		break;
	case USB_STAT_ACA_RID_C_HS_CHIRP:
	case USB_STAT_ACA_RID_C_NM:
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_1P5;
		di->is_aca_rid = 1;
		break;
	case USB_STAT_NOT_CONFIGURED:
		if (di->vbus_detected) {
			di->usb_device_is_unrecognised = true;
			dev_dbg(di->dev, "USB Type - Legacy charger.\n");
			di->max_usb_in_curr.usb_type_max_ua =
						USB_CH_IP_CUR_LVL_1P5;
			break;
		}
		fallthrough;
	case USB_STAT_HM_IDGND:
		dev_err(di->dev, "USB Type - Charging not allowed\n");
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P05;
		ret = -ENXIO;
		break;
	case USB_STAT_RESERVED:
		if (is_ab8500(di->parent)) {
			di->flags.vbus_collapse = true;
			dev_err(di->dev, "USB Type - USB_STAT_RESERVED "
						"VBUS has collapsed\n");
			ret = -ENXIO;
			break;
		} else {
			dev_dbg(di->dev, "USB Type - Charging not allowed\n");
			di->max_usb_in_curr.usb_type_max_ua =
						USB_CH_IP_CUR_LVL_0P05;
			dev_dbg(di->dev, "USB Type - 0x%02x MaxCurr: %d",
				link_status,
				di->max_usb_in_curr.usb_type_max_ua);
			ret = -ENXIO;
			break;
		}
	case USB_STAT_CARKIT_1:
	case USB_STAT_CARKIT_2:
	case USB_STAT_ACA_DOCK_CHARGER:
	case USB_STAT_CHARGER_LINE_1:
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P5;
		dev_dbg(di->dev, "USB Type - 0x%02x MaxCurr: %d", link_status,
				di->max_usb_in_curr.usb_type_max_ua);
		break;
	case USB_STAT_NOT_VALID_LINK:
		dev_err(di->dev, "USB Type invalid - try charging anyway\n");
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P5;
		break;

	default:
		dev_err(di->dev, "USB Type - Unknown\n");
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P05;
		ret = -ENXIO;
		break;
	}

	di->max_usb_in_curr.set_max_ua = di->max_usb_in_curr.usb_type_max_ua;
	dev_dbg(di->dev, "USB Type - 0x%02x MaxCurr: %d",
		link_status, di->max_usb_in_curr.set_max_ua);

	return ret;
}

 
static int ab8500_charger_read_usb_type(struct ab8500_charger *di)
{
	int ret;
	u8 val;

	ret = abx500_get_register_interruptible(di->dev,
		AB8500_INTERRUPT, AB8500_IT_SOURCE21_REG, &val);
	if (ret < 0) {
		dev_err(di->dev, "%s ab8500 read failed\n", __func__);
		return ret;
	}
	if (is_ab8500(di->parent))
		ret = abx500_get_register_interruptible(di->dev, AB8500_USB,
			AB8500_USB_LINE_STAT_REG, &val);
	else
		ret = abx500_get_register_interruptible(di->dev,
			AB8500_USB, AB8500_USB_LINK1_STAT_REG, &val);
	if (ret < 0) {
		dev_err(di->dev, "%s ab8500 read failed\n", __func__);
		return ret;
	}

	 
	if (is_ab8500(di->parent))
		val = (val & AB8500_USB_LINK_STATUS) >> USB_LINK_STATUS_SHIFT;
	else
		val = (val & AB8505_USB_LINK_STATUS) >> USB_LINK_STATUS_SHIFT;
	ret = ab8500_charger_max_usb_curr(di,
		(enum ab8500_charger_link_status) val);

	return ret;
}

 
static int ab8500_charger_detect_usb_type(struct ab8500_charger *di)
{
	int i, ret;
	u8 val;

	 
	for (i = 0; i < 10; i++) {
		msleep(250);
		ret = abx500_get_register_interruptible(di->dev,
			AB8500_INTERRUPT, AB8500_IT_SOURCE21_REG,
			&val);
		dev_dbg(di->dev, "%s AB8500_IT_SOURCE21_REG %x\n",
			__func__, val);
		if (ret < 0) {
			dev_err(di->dev, "%s ab8500 read failed\n", __func__);
			return ret;
		}

		if (is_ab8500(di->parent))
			ret = abx500_get_register_interruptible(di->dev,
				AB8500_USB, AB8500_USB_LINE_STAT_REG, &val);
		else
			ret = abx500_get_register_interruptible(di->dev,
				AB8500_USB, AB8500_USB_LINK1_STAT_REG, &val);
		if (ret < 0) {
			dev_err(di->dev, "%s ab8500 read failed\n", __func__);
			return ret;
		}
		dev_dbg(di->dev, "%s AB8500_USB_LINE_STAT_REG %x\n", __func__,
			val);
		 

		 
		if (is_ab8500(di->parent))
			val = (val & AB8500_USB_LINK_STATUS) >>
							USB_LINK_STATUS_SHIFT;
		else
			val = (val & AB8505_USB_LINK_STATUS) >>
							USB_LINK_STATUS_SHIFT;
		if (val)
			break;
	}
	ret = ab8500_charger_max_usb_curr(di,
		(enum ab8500_charger_link_status) val);

	return ret;
}

 
static int ab8500_charger_voltage_map[] = {
	3500000,
	3525000,
	3550000,
	3575000,
	3600000,
	3625000,
	3650000,
	3675000,
	3700000,
	3725000,
	3750000,
	3775000,
	3800000,
	3825000,
	3850000,
	3875000,
	3900000,
	3925000,
	3950000,
	3975000,
	4000000,
	4025000,
	4050000,
	4060000,
	4070000,
	4080000,
	4090000,
	4100000,
	4110000,
	4120000,
	4130000,
	4140000,
	4150000,
	4160000,
	4170000,
	4180000,
	4190000,
	4200000,
	4210000,
	4220000,
	4230000,
	4240000,
	4250000,
	4260000,
	4270000,
	4280000,
	4290000,
	4300000,
	4310000,
	4320000,
	4330000,
	4340000,
	4350000,
	4360000,
	4370000,
	4380000,
	4390000,
	4400000,
	4410000,
	4420000,
	4430000,
	4440000,
	4450000,
	4460000,
	4470000,
	4480000,
	4490000,
	4500000,
	4510000,
	4520000,
	4530000,
	4540000,
	4550000,
	4560000,
	4570000,
	4580000,
	4590000,
	4600000,
};

static int ab8500_voltage_to_regval(int voltage_uv)
{
	int i;

	 
	if (voltage_uv < ab8500_charger_voltage_map[0])
		return LOW_VOLT_REG;

	for (i = 1; i < ARRAY_SIZE(ab8500_charger_voltage_map); i++) {
		if (voltage_uv < ab8500_charger_voltage_map[i])
			return i - 1;
	}

	 
	i = ARRAY_SIZE(ab8500_charger_voltage_map) - 1;
	if (voltage_uv == ab8500_charger_voltage_map[i])
		return i;
	else
		return -1;
}

 
static int ab8500_charge_input_curr_map[] = {
	50000, 98000, 193000, 290000, 380000, 450000, 500000, 600000,
	700000, 800000, 900000, 1000000, 1100000, 1300000, 1400000, 1500000,
};

 
static int ab8500_charge_output_curr_map[] = {
	100000, 200000, 300000, 400000, 500000, 600000, 700000, 800000,
	900000, 1000000, 1100000, 1200000, 1300000, 1400000, 1500000, 1500000,
};

static int ab8500_current_to_regval(struct ab8500_charger *di, int curr_ua)
{
	int i;

	if (curr_ua < ab8500_charge_output_curr_map[0])
		return 0;

	for (i = 0; i < ARRAY_SIZE(ab8500_charge_output_curr_map); i++) {
		if (curr_ua < ab8500_charge_output_curr_map[i])
			return i - 1;
	}

	 
	i =  ARRAY_SIZE(ab8500_charge_output_curr_map) - 1;
	if (curr_ua == ab8500_charge_output_curr_map[i])
		return i;
	else
		return -1;
}

static int ab8500_vbus_in_curr_to_regval(struct ab8500_charger *di, int curr_ua)
{
	int i;

	if (curr_ua < ab8500_charge_input_curr_map[0])
		return 0;

	for (i = 0; i < ARRAY_SIZE(ab8500_charge_input_curr_map); i++) {
		if (curr_ua < ab8500_charge_input_curr_map[i])
			return i - 1;
	}

	 
	i =  ARRAY_SIZE(ab8500_charge_input_curr_map) - 1;
	if (curr_ua == ab8500_charge_input_curr_map[i])
		return i;
	else
		return -1;
}

 
static int ab8500_charger_get_usb_cur(struct ab8500_charger *di)
{
	int ret = 0;
	switch (di->usb_state.usb_current_ua) {
	case 100000:
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P09;
		break;
	case 200000:
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P19;
		break;
	case 300000:
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P29;
		break;
	case 400000:
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P38;
		break;
	case 500000:
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P5;
		break;
	default:
		di->max_usb_in_curr.usb_type_max_ua = USB_CH_IP_CUR_LVL_0P05;
		ret = -EPERM;
		break;
	}
	di->max_usb_in_curr.set_max_ua = di->max_usb_in_curr.usb_type_max_ua;
	return ret;
}

 
static bool ab8500_charger_check_continue_stepping(struct ab8500_charger *di,
						   int reg)
{
	if (reg == AB8500_USBCH_IPT_CRNTLVL_REG)
		return !di->flags.vbus_drop_end;
	else
		return true;
}

 
static int ab8500_charger_set_current(struct ab8500_charger *di,
	int ich_ua, int reg)
{
	int ret = 0;
	int curr_index, prev_curr_index, shift_value, i;
	u8 reg_value;
	u32 step_udelay;
	bool no_stepping = false;

	atomic_inc(&di->current_stepping_sessions);

	ret = abx500_get_register_interruptible(di->dev, AB8500_CHARGER,
		reg, &reg_value);
	if (ret < 0) {
		dev_err(di->dev, "%s read failed\n", __func__);
		goto exit_set_current;
	}

	switch (reg) {
	case AB8500_MCH_IPT_CURLVL_REG:
		shift_value = MAIN_CH_INPUT_CURR_SHIFT;
		prev_curr_index = (reg_value >> shift_value);
		curr_index = ab8500_current_to_regval(di, ich_ua);
		step_udelay = STEP_UDELAY;
		if (!di->ac.charger_connected)
			no_stepping = true;
		break;
	case AB8500_USBCH_IPT_CRNTLVL_REG:
		shift_value = VBUS_IN_CURR_LIM_SHIFT;
		prev_curr_index = (reg_value >> shift_value);
		curr_index = ab8500_vbus_in_curr_to_regval(di, ich_ua);
		step_udelay = STEP_UDELAY * 100;

		if (!di->usb.charger_connected)
			no_stepping = true;
		break;
	case AB8500_CH_OPT_CRNTLVL_REG:
		shift_value = 0;
		prev_curr_index = (reg_value >> shift_value);
		curr_index = ab8500_current_to_regval(di, ich_ua);
		step_udelay = STEP_UDELAY;
		if (curr_index && (curr_index - prev_curr_index) > 1)
			step_udelay *= 100;

		if (!di->usb.charger_connected && !di->ac.charger_connected)
			no_stepping = true;

		break;
	default:
		dev_err(di->dev, "%s current register not valid\n", __func__);
		ret = -ENXIO;
		goto exit_set_current;
	}

	if (curr_index < 0) {
		dev_err(di->dev, "requested current limit out-of-range\n");
		ret = -ENXIO;
		goto exit_set_current;
	}

	 
	if (prev_curr_index == curr_index) {
		dev_dbg(di->dev, "%s current not changed for reg: 0x%02x\n",
			__func__, reg);
		ret = 0;
		goto exit_set_current;
	}

	dev_dbg(di->dev, "%s set charger current: %d uA for reg: 0x%02x\n",
		__func__, ich_ua, reg);

	if (no_stepping) {
		ret = abx500_set_register_interruptible(di->dev, AB8500_CHARGER,
					reg, (u8)curr_index << shift_value);
		if (ret)
			dev_err(di->dev, "%s write failed\n", __func__);
	} else if (prev_curr_index > curr_index) {
		for (i = prev_curr_index - 1; i >= curr_index; i--) {
			dev_dbg(di->dev, "curr change_1 to: %x for 0x%02x\n",
				(u8) i << shift_value, reg);
			ret = abx500_set_register_interruptible(di->dev,
				AB8500_CHARGER, reg, (u8)i << shift_value);
			if (ret) {
				dev_err(di->dev, "%s write failed\n", __func__);
				goto exit_set_current;
			}
			if (i != curr_index)
				usleep_range(step_udelay, step_udelay * 2);
		}
	} else {
		bool allow = true;
		for (i = prev_curr_index + 1; i <= curr_index && allow; i++) {
			dev_dbg(di->dev, "curr change_2 to: %x for 0x%02x\n",
				(u8)i << shift_value, reg);
			ret = abx500_set_register_interruptible(di->dev,
				AB8500_CHARGER, reg, (u8)i << shift_value);
			if (ret) {
				dev_err(di->dev, "%s write failed\n", __func__);
				goto exit_set_current;
			}
			if (i != curr_index)
				usleep_range(step_udelay, step_udelay * 2);

			allow = ab8500_charger_check_continue_stepping(di, reg);
		}
	}

exit_set_current:
	atomic_dec(&di->current_stepping_sessions);

	return ret;
}

 
static int ab8500_charger_set_vbus_in_curr(struct ab8500_charger *di,
		int ich_in_ua)
{
	int min_value;
	int ret;

	 
	min_value = min(di->bm->chg_params->usb_curr_max_ua, ich_in_ua);
	if (di->max_usb_in_curr.set_max_ua > 0)
		min_value = min(di->max_usb_in_curr.set_max_ua, min_value);

	if (di->usb_state.usb_current_ua >= 0)
		min_value = min(di->usb_state.usb_current_ua, min_value);

	switch (min_value) {
	case 100000:
		if (di->vbat < VBAT_TRESH_IP_CUR_RED)
			min_value = USB_CH_IP_CUR_LVL_0P05;
		break;
	case 500000:
		if (di->vbat < VBAT_TRESH_IP_CUR_RED)
			min_value = USB_CH_IP_CUR_LVL_0P45;
		break;
	default:
		break;
	}

	dev_info(di->dev, "VBUS input current limit set to %d uA\n", min_value);

	mutex_lock(&di->usb_ipt_crnt_lock);
	ret = ab8500_charger_set_current(di, min_value,
		AB8500_USBCH_IPT_CRNTLVL_REG);
	mutex_unlock(&di->usb_ipt_crnt_lock);

	return ret;
}

 
static int ab8500_charger_set_main_in_curr(struct ab8500_charger *di,
	int ich_in_ua)
{
	return ab8500_charger_set_current(di, ich_in_ua,
		AB8500_MCH_IPT_CURLVL_REG);
}

 
static int ab8500_charger_set_output_curr(struct ab8500_charger *di,
	int ich_out_ua)
{
	return ab8500_charger_set_current(di, ich_out_ua,
		AB8500_CH_OPT_CRNTLVL_REG);
}

 
static int ab8500_charger_led_en(struct ab8500_charger *di, int on)
{
	int ret;

	if (on) {
		 
		ret = abx500_set_register_interruptible(di->dev, AB8500_CHARGER,
			AB8500_LED_INDICATOR_PWM_CTRL,
			(LED_IND_CUR_5MA | LED_INDICATOR_PWM_ENA));
		if (ret) {
			dev_err(di->dev, "Power ON LED failed\n");
			return ret;
		}
		 
		ret = abx500_set_register_interruptible(di->dev, AB8500_CHARGER,
			AB8500_LED_INDICATOR_PWM_DUTY,
			LED_INDICATOR_PWM_DUTY_252_256);
		if (ret) {
			dev_err(di->dev, "Set LED PWM duty cycle failed\n");
			return ret;
		}
	} else {
		 
		ret = abx500_set_register_interruptible(di->dev, AB8500_CHARGER,
			AB8500_LED_INDICATOR_PWM_CTRL,
			LED_INDICATOR_PWM_DIS);
		if (ret) {
			dev_err(di->dev, "Power-off LED failed\n");
			return ret;
		}
	}

	return ret;
}

 
static int ab8500_charger_ac_en(struct ux500_charger *charger,
	int enable, int vset_uv, int iset_ua)
{
	int ret;
	int volt_index;
	int curr_index;
	int input_curr_index;
	u8 overshoot = 0;

	struct ab8500_charger *di = to_ab8500_charger_ac_device_info(charger);

	if (enable) {
		 
		if (!di->ac.charger_connected) {
			dev_err(di->dev, "AC charger not connected\n");
			return -ENXIO;
		}

		 
		dev_dbg(di->dev, "Enable AC: %duV %duA\n", vset_uv, iset_ua);

		 
		if (!di->vddadc_en_ac) {
			ret = regulator_enable(di->regu);
			if (ret)
				dev_warn(di->dev,
					"Failed to enable regulator\n");
			else
				di->vddadc_en_ac = true;
		}

		 
		volt_index = ab8500_voltage_to_regval(vset_uv);
		curr_index = ab8500_current_to_regval(di, iset_ua);
		input_curr_index = ab8500_current_to_regval(di,
			di->bm->chg_params->ac_curr_max_ua);
		if (volt_index < 0 || curr_index < 0 || input_curr_index < 0) {
			dev_err(di->dev,
				"Charger voltage or current too high, "
				"charging not started\n");
			return -ENXIO;
		}

		 
		ret = abx500_set_register_interruptible(di->dev, AB8500_CHARGER,
			AB8500_CH_VOLT_LVL_REG, (u8) volt_index);
		if (ret) {
			dev_err(di->dev, "%s write failed\n", __func__);
			return ret;
		}
		 
		ret = ab8500_charger_set_main_in_curr(di,
			di->bm->chg_params->ac_curr_max_ua);
		if (ret) {
			dev_err(di->dev, "%s Failed to set MainChInputCurr\n",
				__func__);
			return ret;
		}
		 
		ret = ab8500_charger_set_output_curr(di, iset_ua);
		if (ret) {
			dev_err(di->dev, "%s "
				"Failed to set ChOutputCurentLevel\n",
				__func__);
			return ret;
		}

		 
		if (!di->bm->enable_overshoot)
			overshoot = MAIN_CH_NO_OVERSHOOT_ENA_N;

		 
		ret = abx500_set_register_interruptible(di->dev, AB8500_CHARGER,
			AB8500_MCH_CTRL1, MAIN_CH_ENA | overshoot);
		if (ret) {
			dev_err(di->dev, "%s write failed\n", __func__);
			return ret;
		}

		 
		ret = ab8500_charger_led_en(di, true);
		if (ret < 0)
			dev_err(di->dev, "failed to enable LED\n");

		di->ac.charger_online = 1;
	} else {
		 
		if (is_ab8500_1p1_or_earlier(di->parent)) {
			 
			if (di->ac_conn) {
				queue_delayed_work(di->charger_wq,
					&di->kick_wd_work,
					round_jiffies(WD_KICK_INTERVAL));
			}

			 
			ret = abx500_set_register_interruptible(di->dev,
				AB8500_CHARGER,
				AB8500_CH_VOLT_LVL_REG, CH_VOL_LVL_3P5);
			if (ret) {
				dev_err(di->dev,
					"%s write failed\n", __func__);
				return ret;
			}

			ret = ab8500_charger_set_output_curr(di, 0);
			if (ret) {
				dev_err(di->dev, "%s "
					"Failed to set ChOutputCurentLevel\n",
					__func__);
				return ret;
			}
		} else {
			ret = abx500_set_register_interruptible(di->dev,
				AB8500_CHARGER,
				AB8500_MCH_CTRL1, 0);
			if (ret) {
				dev_err(di->dev,
					"%s write failed\n", __func__);
				return ret;
			}
		}

		ret = ab8500_charger_led_en(di, false);
		if (ret < 0)
			dev_err(di->dev, "failed to disable LED\n");

		di->ac.charger_online = 0;
		di->ac.wd_expired = false;

		 
		if (di->vddadc_en_ac) {
			regulator_disable(di->regu);
			di->vddadc_en_ac = false;
		}

		dev_dbg(di->dev, "%s Disabled AC charging\n", __func__);
	}
	ab8500_power_supply_changed(di, di->ac_chg.psy);

	return ret;
}

 
static int ab8500_charger_usb_en(struct ux500_charger *charger,
	int enable, int vset_uv, int ich_out_ua)
{
	int ret;
	int volt_index;
	int curr_index;
	u8 overshoot = 0;

	struct ab8500_charger *di = to_ab8500_charger_usb_device_info(charger);

	if (enable) {
		 
		if (!di->usb.charger_connected) {
			dev_err(di->dev, "USB charger not connected\n");
			return -ENXIO;
		}

		 
		if (!di->vddadc_en_usb) {
			ret = regulator_enable(di->regu);
			if (ret)
				dev_warn(di->dev,
					"Failed to enable regulator\n");
			else
				di->vddadc_en_usb = true;
		}

		 
		dev_dbg(di->dev, "Enable USB: %d uV %d uA\n", vset_uv, ich_out_ua);

		 
		volt_index = ab8500_voltage_to_regval(vset_uv);
		curr_index = ab8500_current_to_regval(di, ich_out_ua);
		if (volt_index < 0 || curr_index < 0) {
			dev_err(di->dev,
				"Charger voltage or current too high, "
				"charging not started\n");
			return -ENXIO;
		}

		 
		ret = abx500_set_register_interruptible(di->dev, AB8500_CHARGER,
			AB8500_CH_VOLT_LVL_REG, (u8) volt_index);
		if (ret) {
			dev_err(di->dev, "%s write failed\n", __func__);
			return ret;
		}
		 
		if (!di->bm->enable_overshoot)
			overshoot = USB_CHG_NO_OVERSHOOT_ENA_N;

		 
		dev_dbg(di->dev,
			"Enabling USB with write to AB8500_USBCH_CTRL1_REG\n");
		ret = abx500_set_register_interruptible(di->dev, AB8500_CHARGER,
			AB8500_USBCH_CTRL1_REG, USB_CH_ENA | overshoot);
		if (ret) {
			dev_err(di->dev, "%s write failed\n", __func__);
			return ret;
		}

		 
		ret = ab8500_charger_led_en(di, true);
		if (ret < 0)
			dev_err(di->dev, "failed to enable LED\n");

		di->usb.charger_online = 1;

		 
		ret = ab8500_charger_set_vbus_in_curr(di,
					di->max_usb_in_curr.usb_type_max_ua);
		if (ret) {
			dev_err(di->dev, "setting USBChInputCurr failed\n");
			return ret;
		}

		 
		ret = ab8500_charger_set_output_curr(di, ich_out_ua);
		if (ret) {
			dev_err(di->dev, "%s "
				"Failed to set ChOutputCurentLevel\n",
				__func__);
			return ret;
		}

		queue_delayed_work(di->charger_wq, &di->check_vbat_work, HZ);

	} else {
		 
		dev_dbg(di->dev, "%s Disabled USB charging\n", __func__);
		ret = abx500_set_register_interruptible(di->dev,
			AB8500_CHARGER,
			AB8500_USBCH_CTRL1_REG, 0);
		if (ret) {
			dev_err(di->dev,
				"%s write failed\n", __func__);
			return ret;
		}

		ret = ab8500_charger_led_en(di, false);
		if (ret < 0)
			dev_err(di->dev, "failed to disable LED\n");
		 
		ret = ab8500_charger_set_vbus_in_curr(di, 0);
		if (ret) {
			dev_err(di->dev, "setting USBChInputCurr failed\n");
			return ret;
		}

		 
		ret = ab8500_charger_set_output_curr(di, 0);
		if (ret) {
			dev_err(di->dev, "%s "
				"Failed to reset ChOutputCurentLevel\n",
				__func__);
			return ret;
		}
		di->usb.charger_online = 0;
		di->usb.wd_expired = false;

		 
		if (di->vddadc_en_usb) {
			regulator_disable(di->regu);
			di->vddadc_en_usb = false;
		}

		dev_dbg(di->dev, "%s Disabled USB charging\n", __func__);

		 
		cancel_delayed_work(&di->check_vbat_work);

	}
	ab8500_power_supply_changed(di, di->usb_chg.psy);

	return ret;
}

 
static int ab8500_charger_usb_check_enable(struct ux500_charger *charger,
	int vset_uv, int iset_ua)
{
	u8 usbch_ctrl1 = 0;
	int ret = 0;

	struct ab8500_charger *di = to_ab8500_charger_usb_device_info(charger);

	if (!di->usb.charger_connected)
		return ret;

	ret = abx500_get_register_interruptible(di->dev, AB8500_CHARGER,
				AB8500_USBCH_CTRL1_REG, &usbch_ctrl1);
	if (ret < 0) {
		dev_err(di->dev, "ab8500 read failed %d\n", __LINE__);
		return ret;
	}
	dev_dbg(di->dev, "USB charger ctrl: 0x%02x\n", usbch_ctrl1);

	if (!(usbch_ctrl1 & USB_CH_ENA)) {
		dev_info(di->dev, "Charging has been disabled abnormally and will be re-enabled\n");

		ret = abx500_mask_and_set_register_interruptible(di->dev,
					AB8500_CHARGER, AB8500_CHARGER_CTRL,
					DROP_COUNT_RESET, DROP_COUNT_RESET);
		if (ret < 0) {
			dev_err(di->dev, "ab8500 write failed %d\n", __LINE__);
			return ret;
		}

		ret = ab8500_charger_usb_en(&di->usb_chg, true, vset_uv, iset_ua);
		if (ret < 0) {
			dev_err(di->dev, "Failed to enable VBUS charger %d\n",
					__LINE__);
			return ret;
		}
	}
	return ret;
}

 
static int ab8500_charger_ac_check_enable(struct ux500_charger *charger,
	int vset_uv, int iset_ua)
{
	u8 mainch_ctrl1 = 0;
	int ret = 0;

	struct ab8500_charger *di = to_ab8500_charger_ac_device_info(charger);

	if (!di->ac.charger_connected)
		return ret;

	ret = abx500_get_register_interruptible(di->dev, AB8500_CHARGER,
				AB8500_MCH_CTRL1, &mainch_ctrl1);
	if (ret < 0) {
		dev_err(di->dev, "ab8500 read failed %d\n", __LINE__);
		return ret;
	}
	dev_dbg(di->dev, "AC charger ctrl: 0x%02x\n", mainch_ctrl1);

	if (!(mainch_ctrl1 & MAIN_CH_ENA)) {
		dev_info(di->dev, "Charging has been disabled abnormally and will be re-enabled\n");

		ret = abx500_mask_and_set_register_interruptible(di->dev,
					AB8500_CHARGER, AB8500_CHARGER_CTRL,
					DROP_COUNT_RESET, DROP_COUNT_RESET);

		if (ret < 0) {
			dev_err(di->dev, "ab8500 write failed %d\n", __LINE__);
			return ret;
		}

		ret = ab8500_charger_ac_en(&di->usb_chg, true, vset_uv, iset_ua);
		if (ret < 0) {
			dev_err(di->dev, "failed to enable AC charger %d\n",
				__LINE__);
			return ret;
		}
	}
	return ret;
}

 
static int ab8500_charger_watchdog_kick(struct ux500_charger *charger)
{
	int ret;
	struct ab8500_charger *di;

	if (charger->psy->desc->type == POWER_SUPPLY_TYPE_MAINS)
		di = to_ab8500_charger_ac_device_info(charger);
	else if (charger->psy->desc->type == POWER_SUPPLY_TYPE_USB)
		di = to_ab8500_charger_usb_device_info(charger);
	else
		return -ENXIO;

	ret = abx500_set_register_interruptible(di->dev, AB8500_CHARGER,
		AB8500_CHARG_WD_CTRL, CHARG_WD_KICK);
	if (ret)
		dev_err(di->dev, "Failed to kick WD!\n");

	return ret;
}

 
static int ab8500_charger_update_charger_current(struct ux500_charger *charger,
		int ich_out_ua)
{
	int ret;
	struct ab8500_charger *di;

	if (charger->psy->desc->type == POWER_SUPPLY_TYPE_MAINS)
		di = to_ab8500_charger_ac_device_info(charger);
	else if (charger->psy->desc->type == POWER_SUPPLY_TYPE_USB)
		di = to_ab8500_charger_usb_device_info(charger);
	else
		return -ENXIO;

	ret = ab8500_charger_set_output_curr(di, ich_out_ua);
	if (ret) {
		dev_err(di->dev, "%s "
			"Failed to set ChOutputCurentLevel\n",
			__func__);
		return ret;
	}

	 
	ret = abx500_set_register_interruptible(di->dev, AB8500_CHARGER,
				AB8500_CHARGER_CTRL, DROP_COUNT_RESET);
	if (ret) {
		dev_err(di->dev, "%s write failed\n", __func__);
		return ret;
	}

	return ret;
}

static int ab8500_charger_get_ext_psy_data(struct device *dev, void *data)
{
	struct power_supply *psy;
	struct power_supply *ext = dev_get_drvdata(dev);
	const char **supplicants = (const char **)ext->supplied_to;
	struct ab8500_charger *di;
	union power_supply_propval ret;
	int j;
	struct ux500_charger *usb_chg;

	usb_chg = (struct ux500_charger *)data;
	psy = usb_chg->psy;

	di = to_ab8500_charger_usb_device_info(usb_chg);

	 
	j = match_string(supplicants, ext->num_supplicants, psy->desc->name);
	if (j < 0)
		return 0;

	 
	for (j = 0; j < ext->desc->num_properties; j++) {
		enum power_supply_property prop;
		prop = ext->desc->properties[j];

		if (power_supply_get_property(ext, prop, &ret))
			continue;

		switch (prop) {
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			switch (ext->desc->type) {
			case POWER_SUPPLY_TYPE_BATTERY:
				 
				dev_dbg(di->dev, "get VBAT from %s\n",
					dev_name(&ext->dev));
				di->vbat = ret.intval;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}
	return 0;
}

 
static void ab8500_charger_check_vbat_work(struct work_struct *work)
{
	int t = 10;
	struct ab8500_charger *di = container_of(work,
		struct ab8500_charger, check_vbat_work.work);

	class_for_each_device(power_supply_class, NULL,
			      &di->usb_chg, ab8500_charger_get_ext_psy_data);

	 
	if (di->old_vbat == 0)
		di->old_vbat = di->vbat;

	if (!((di->old_vbat <= VBAT_TRESH_IP_CUR_RED &&
		di->vbat <= VBAT_TRESH_IP_CUR_RED) ||
		(di->old_vbat > VBAT_TRESH_IP_CUR_RED &&
		di->vbat > VBAT_TRESH_IP_CUR_RED))) {

		dev_dbg(di->dev, "Vbat did cross threshold, curr: %d, new: %d,"
			" old: %d\n", di->max_usb_in_curr.usb_type_max_ua,
			di->vbat, di->old_vbat);
		ab8500_charger_set_vbus_in_curr(di,
					di->max_usb_in_curr.usb_type_max_ua);
		power_supply_changed(di->usb_chg.psy);
	}

	di->old_vbat = di->vbat;

	 
	if (di->vbat < (VBAT_TRESH_IP_CUR_RED + 100000) &&
		(di->vbat > (VBAT_TRESH_IP_CUR_RED - 100000)))
			t = 1;

	queue_delayed_work(di->charger_wq, &di->check_vbat_work, t * HZ);
}

 
static void ab8500_charger_check_hw_failure_work(struct work_struct *work)
{
	int ret;
	u8 reg_value;

	struct ab8500_charger *di = container_of(work,
		struct ab8500_charger, check_hw_failure_work.work);

	 
	if (di->flags.mainextchnotok) {
		ret = abx500_get_register_interruptible(di->dev,
			AB8500_CHARGER, AB8500_CH_STATUS2_REG, &reg_value);
		if (ret < 0) {
			dev_err(di->dev, "%s ab8500 read failed\n", __func__);
			return;
		}
		if (!(reg_value & MAIN_CH_NOK)) {
			di->flags.mainextchnotok = false;
			ab8500_power_supply_changed(di, di->ac_chg.psy);
		}
	}
	if (di->flags.vbus_ovv) {
		ret = abx500_get_register_interruptible(di->dev,
			AB8500_CHARGER, AB8500_CH_USBCH_STAT2_REG,
			&reg_value);
		if (ret < 0) {
			dev_err(di->dev, "%s ab8500 read failed\n", __func__);
			return;
		}
		if (!(reg_value & VBUS_OVV_TH)) {
			di->flags.vbus_ovv = false;
			ab8500_power_supply_changed(di, di->usb_chg.psy);
		}
	}
	 
	if (di->flags.mainextchnotok || di->flags.vbus_ovv) {
		queue_delayed_work(di->charger_wq,
			&di->check_hw_failure_work, round_jiffies(HZ));
	}
}

 
static void ab8500_charger_kick_watchdog_work(struct work_struct *work)
{
	int ret;

	struct ab8500_charger *di = container_of(work,
		struct ab8500_charger, kick_wd_work.work);

	ret = abx500_set_register_interruptible(di->dev, AB8500_CHARGER,
		AB8500_CHARG_WD_CTRL, CHARG_WD_KICK);
	if (ret)
		dev_err(di->dev, "Failed to kick WD!\n");

	 
	queue_delayed_work(di->charger_wq,
		&di->kick_wd_work, round_jiffies(WD_KICK_INTERVAL));
}

 
static void ab8500_charger_ac_work(struct work_struct *work)
{
	int ret;

	struct ab8500_charger *di = container_of(work,
		struct ab8500_charger, ac_work);

	 
	ret = ab8500_charger_detect_chargers(di, false);
	if (ret < 0)
		return;

	if (ret & AC_PW_CONN) {
		di->ac.charger_connected = 1;
		di->ac_conn = true;
	} else {
		di->ac.charger_connected = 0;
	}

	ab8500_power_supply_changed(di, di->ac_chg.psy);
	sysfs_notify(&di->ac_chg.psy->dev.kobj, NULL, "present");
}

static void ab8500_charger_usb_attached_work(struct work_struct *work)
{
	struct ab8500_charger *di = container_of(work,
						 struct ab8500_charger,
						 usb_charger_attached_work.work);
	int usbch = (USB_CH_VBUSDROP | USB_CH_VBUSDETDBNC);
	int ret, i;
	u8 statval;

	for (i = 0; i < 10; i++) {
		ret = abx500_get_register_interruptible(di->dev,
							AB8500_CHARGER,
							AB8500_CH_USBCH_STAT1_REG,
							&statval);
		if (ret < 0) {
			dev_err(di->dev, "ab8500 read failed %d\n", __LINE__);
			goto reschedule;
		}
		if ((statval & usbch) != usbch)
			goto reschedule;

		msleep(CHARGER_STATUS_POLL);
	}

	ab8500_charger_usb_en(&di->usb_chg, 0, 0, 0);

	mutex_lock(&di->charger_attached_mutex);
	mutex_unlock(&di->charger_attached_mutex);

	return;

reschedule:
	queue_delayed_work(di->charger_wq,
			   &di->usb_charger_attached_work,
			   HZ);
}

static void ab8500_charger_ac_attached_work(struct work_struct *work)
{

	struct ab8500_charger *di = container_of(work,
						 struct ab8500_charger,
						 ac_charger_attached_work.work);
	int mainch = (MAIN_CH_STATUS2_MAINCHGDROP |
		      MAIN_CH_STATUS2_MAINCHARGERDETDBNC);
	int ret, i;
	u8 statval;

	for (i = 0; i < 10; i++) {
		ret = abx500_get_register_interruptible(di->dev,
							AB8500_CHARGER,
							AB8500_CH_STATUS2_REG,
							&statval);
		if (ret < 0) {
			dev_err(di->dev, "ab8500 read failed %d\n", __LINE__);
			goto reschedule;
		}

		if ((statval & mainch) != mainch)
			goto reschedule;

		msleep(CHARGER_STATUS_POLL);
	}

	ab8500_charger_ac_en(&di->ac_chg, 0, 0, 0);
	queue_work(di->charger_wq, &di->ac_work);

	mutex_lock(&di->charger_attached_mutex);
	mutex_unlock(&di->charger_attached_mutex);

	return;

reschedule:
	queue_delayed_work(di->charger_wq,
			   &di->ac_charger_attached_work,
			   HZ);
}

 
static void ab8500_charger_detect_usb_type_work(struct work_struct *work)
{
	int ret;

	struct ab8500_charger *di = container_of(work,
		struct ab8500_charger, detect_usb_type_work);

	 
	ret = ab8500_charger_detect_chargers(di, false);
	if (ret < 0)
		return;

	if (!(ret & USB_PW_CONN)) {
		dev_dbg(di->dev, "%s di->vbus_detected = false\n", __func__);
		di->vbus_detected = false;
		ab8500_charger_set_usb_connected(di, false);
		ab8500_power_supply_changed(di, di->usb_chg.psy);
	} else {
		dev_dbg(di->dev, "%s di->vbus_detected = true\n", __func__);
		di->vbus_detected = true;

		if (is_ab8500_1p1_or_earlier(di->parent)) {
			ret = ab8500_charger_detect_usb_type(di);
			if (!ret) {
				ab8500_charger_set_usb_connected(di, true);
				ab8500_power_supply_changed(di,
							    di->usb_chg.psy);
			}
		} else {
			 
			if (di->vbus_detected_start) {
				di->vbus_detected_start = false;
				ret = ab8500_charger_detect_usb_type(di);
				if (!ret) {
					ab8500_charger_set_usb_connected(di,
						true);
					ab8500_power_supply_changed(di,
						di->usb_chg.psy);
				}
			}
		}
	}
}

 
static void ab8500_charger_usb_link_attach_work(struct work_struct *work)
{
	struct ab8500_charger *di =
		container_of(work, struct ab8500_charger, attach_work.work);
	int ret;

	 
	if (!di->usb.charger_online) {
		ret = ab8500_charger_set_vbus_in_curr(di,
					di->max_usb_in_curr.usb_type_max_ua);
		if (ret)
			return;
	}

	ab8500_charger_set_usb_connected(di, true);
	ab8500_power_supply_changed(di, di->usb_chg.psy);
}

 
static void ab8500_charger_usb_link_status_work(struct work_struct *work)
{
	int detected_chargers;
	int ret;
	u8 val;
	u8 link_status;

	struct ab8500_charger *di = container_of(work,
		struct ab8500_charger, usb_link_status_work);

	 
	detected_chargers = ab8500_charger_detect_chargers(di, false);
	if (detected_chargers < 0)
		return;

	 
	if (is_ab8500(di->parent))
		ret = abx500_get_register_interruptible(di->dev, AB8500_USB,
					AB8500_USB_LINE_STAT_REG, &val);
	else
		ret = abx500_get_register_interruptible(di->dev, AB8500_USB,
					AB8500_USB_LINK1_STAT_REG, &val);

	if (ret >= 0)
		dev_dbg(di->dev, "UsbLineStatus register = 0x%02x\n", val);
	else
		dev_dbg(di->dev, "Error reading USB link status\n");

	if (is_ab8500(di->parent))
		link_status = AB8500_USB_LINK_STATUS;
	else
		link_status = AB8505_USB_LINK_STATUS;

	if (detected_chargers & USB_PW_CONN) {
		if (((val & link_status) >> USB_LINK_STATUS_SHIFT) ==
				USB_STAT_NOT_VALID_LINK &&
				di->invalid_charger_detect_state == 0) {
			dev_dbg(di->dev,
					"Invalid charger detected, state= 0\n");
			 
			abx500_mask_and_set_register_interruptible(di->dev,
					AB8500_CHARGER, AB8500_USBCH_CTRL1_REG,
					USB_CH_ENA, USB_CH_ENA);
			 
			abx500_mask_and_set_register_interruptible(di->dev,
					AB8500_USB, AB8500_USB_LINE_CTRL2_REG,
					USB_CH_DET, USB_CH_DET);
			di->invalid_charger_detect_state = 1;
			 
			return;

		}
		if (di->invalid_charger_detect_state == 1) {
			dev_dbg(di->dev,
					"Invalid charger detected, state= 1\n");
			 
			abx500_mask_and_set_register_interruptible(di->dev,
					AB8500_USB, AB8500_USB_LINE_CTRL2_REG,
					USB_CH_DET, 0x00);
			 
			if (is_ab8500(di->parent))
				ret = abx500_get_register_interruptible(di->dev,
					AB8500_USB, AB8500_USB_LINE_STAT_REG,
					&val);
			else
				ret = abx500_get_register_interruptible(di->dev,
					AB8500_USB, AB8500_USB_LINK1_STAT_REG,
					&val);

			dev_dbg(di->dev, "USB link status= 0x%02x\n",
				(val & link_status) >> USB_LINK_STATUS_SHIFT);
			di->invalid_charger_detect_state = 2;
		}
	} else {
		di->invalid_charger_detect_state = 0;
	}

	if (!(detected_chargers & USB_PW_CONN)) {
		di->vbus_detected = false;
		ab8500_charger_set_usb_connected(di, false);
		ab8500_power_supply_changed(di, di->usb_chg.psy);
		return;
	}

	dev_dbg(di->dev,"%s di->vbus_detected = true\n",__func__);
	di->vbus_detected = true;
	ret = ab8500_charger_read_usb_type(di);
	if (ret) {
		if (ret == -ENXIO) {
			 
			ab8500_charger_set_usb_connected(di, false);
			ab8500_power_supply_changed(di, di->usb_chg.psy);
		}
		return;
	}

	if (di->usb_device_is_unrecognised) {
		dev_dbg(di->dev,
			"Potential Legacy Charger device. "
			"Delay work for %d msec for USB enum "
			"to finish",
			WAIT_ACA_RID_ENUMERATION);
		queue_delayed_work(di->charger_wq,
				   &di->attach_work,
				   msecs_to_jiffies(WAIT_ACA_RID_ENUMERATION));
	} else if (di->is_aca_rid == 1) {
		 
		di->is_aca_rid++;
		dev_dbg(di->dev,
			"%s Wait %d msec for USB enum to finish",
			__func__, WAIT_ACA_RID_ENUMERATION);
		queue_delayed_work(di->charger_wq,
				   &di->attach_work,
				   msecs_to_jiffies(WAIT_ACA_RID_ENUMERATION));
	} else {
		queue_delayed_work(di->charger_wq,
				   &di->attach_work,
				   0);
	}
}

static void ab8500_charger_usb_state_changed_work(struct work_struct *work)
{
	int ret;
	unsigned long flags;

	struct ab8500_charger *di = container_of(work,
		struct ab8500_charger, usb_state_changed_work.work);

	if (!di->vbus_detected)	{
		dev_dbg(di->dev,
			"%s !di->vbus_detected\n",
			__func__);
		return;
	}

	spin_lock_irqsave(&di->usb_state.usb_lock, flags);
	di->usb_state.state = di->usb_state.state_tmp;
	di->usb_state.usb_current_ua = di->usb_state.usb_current_tmp_ua;
	spin_unlock_irqrestore(&di->usb_state.usb_lock, flags);

	dev_dbg(di->dev, "%s USB state: 0x%02x uA: %d\n",
		__func__, di->usb_state.state, di->usb_state.usb_current_ua);

	switch (di->usb_state.state) {
	case AB8500_BM_USB_STATE_RESET_HS:
	case AB8500_BM_USB_STATE_RESET_FS:
	case AB8500_BM_USB_STATE_SUSPEND:
	case AB8500_BM_USB_STATE_MAX:
		ab8500_charger_set_usb_connected(di, false);
		ab8500_power_supply_changed(di, di->usb_chg.psy);
		break;

	case AB8500_BM_USB_STATE_RESUME:
		 
		msleep(1000);
		fallthrough;
	case AB8500_BM_USB_STATE_CONFIGURED:
		 
		if (!ab8500_charger_get_usb_cur(di)) {
			 
			ret = ab8500_charger_set_vbus_in_curr(di,
					di->max_usb_in_curr.usb_type_max_ua);
			if (ret)
				return;

			ab8500_charger_set_usb_connected(di, true);
			ab8500_power_supply_changed(di, di->usb_chg.psy);
		}
		break;

	default:
		break;
	}
}

 
static void ab8500_charger_check_usbchargernotok_work(struct work_struct *work)
{
	int ret;
	u8 reg_value;
	bool prev_status;

	struct ab8500_charger *di = container_of(work,
		struct ab8500_charger, check_usbchgnotok_work.work);

	 
	ret = abx500_get_register_interruptible(di->dev,
		AB8500_CHARGER, AB8500_CH_USBCH_STAT2_REG, &reg_value);
	if (ret < 0) {
		dev_err(di->dev, "%s ab8500 read failed\n", __func__);
		return;
	}
	prev_status = di->flags.usbchargernotok;

	if (reg_value & VBUS_CH_NOK) {
		di->flags.usbchargernotok = true;
		 
		queue_delayed_work(di->charger_wq,
			&di->check_usbchgnotok_work, HZ);
	} else {
		di->flags.usbchargernotok = false;
		di->flags.vbus_collapse = false;
	}

	if (prev_status != di->flags.usbchargernotok)
		ab8500_power_supply_changed(di, di->usb_chg.psy);
}

 
static void ab8500_charger_check_main_thermal_prot_work(
	struct work_struct *work)
{
	int ret;
	u8 reg_value;

	struct ab8500_charger *di = container_of(work,
		struct ab8500_charger, check_main_thermal_prot_work);

	 
	ret = abx500_get_register_interruptible(di->dev,
		AB8500_CHARGER, AB8500_CH_STATUS2_REG, &reg_value);
	if (ret < 0) {
		dev_err(di->dev, "%s ab8500 read failed\n", __func__);
		return;
	}
	if (reg_value & MAIN_CH_TH_PROT)
		di->flags.main_thermal_prot = true;
	else
		di->flags.main_thermal_prot = false;

	ab8500_power_supply_changed(di, di->ac_chg.psy);
}

 
static void ab8500_charger_check_usb_thermal_prot_work(
	struct work_struct *work)
{
	int ret;
	u8 reg_value;

	struct ab8500_charger *di = container_of(work,
		struct ab8500_charger, check_usb_thermal_prot_work);

	 
	ret = abx500_get_register_interruptible(di->dev,
		AB8500_CHARGER, AB8500_CH_USBCH_STAT2_REG, &reg_value);
	if (ret < 0) {
		dev_err(di->dev, "%s ab8500 read failed\n", __func__);
		return;
	}
	if (reg_value & USB_CH_TH_PROT)
		di->flags.usb_thermal_prot = true;
	else
		di->flags.usb_thermal_prot = false;

	ab8500_power_supply_changed(di, di->usb_chg.psy);
}

 
static irqreturn_t ab8500_charger_mainchunplugdet_handler(int irq, void *_di)
{
	struct ab8500_charger *di = _di;

	dev_dbg(di->dev, "Main charger unplugged\n");
	queue_work(di->charger_wq, &di->ac_work);

	cancel_delayed_work_sync(&di->ac_charger_attached_work);
	mutex_lock(&di->charger_attached_mutex);
	mutex_unlock(&di->charger_attached_mutex);

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_charger_mainchplugdet_handler(int irq, void *_di)
{
	struct ab8500_charger *di = _di;

	dev_dbg(di->dev, "Main charger plugged\n");
	queue_work(di->charger_wq, &di->ac_work);

	mutex_lock(&di->charger_attached_mutex);
	mutex_unlock(&di->charger_attached_mutex);

	if (is_ab8500(di->parent))
		queue_delayed_work(di->charger_wq,
			   &di->ac_charger_attached_work,
			   HZ);
	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_charger_mainextchnotok_handler(int irq, void *_di)
{
	struct ab8500_charger *di = _di;

	dev_dbg(di->dev, "Main charger not ok\n");
	di->flags.mainextchnotok = true;
	ab8500_power_supply_changed(di, di->ac_chg.psy);

	 
	queue_delayed_work(di->charger_wq, &di->check_hw_failure_work, 0);

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_charger_mainchthprotr_handler(int irq, void *_di)
{
	struct ab8500_charger *di = _di;

	dev_dbg(di->dev,
		"Die temp above Main charger thermal protection threshold\n");
	queue_work(di->charger_wq, &di->check_main_thermal_prot_work);

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_charger_mainchthprotf_handler(int irq, void *_di)
{
	struct ab8500_charger *di = _di;

	dev_dbg(di->dev,
		"Die temp ok for Main charger thermal protection threshold\n");
	queue_work(di->charger_wq, &di->check_main_thermal_prot_work);

	return IRQ_HANDLED;
}

static void ab8500_charger_vbus_drop_end_work(struct work_struct *work)
{
	struct ab8500_charger *di = container_of(work,
		struct ab8500_charger, vbus_drop_end_work.work);
	int ret, curr_ua;
	u8 reg_value;

	di->flags.vbus_drop_end = false;

	 
	abx500_set_register_interruptible(di->dev,
				  AB8500_CHARGER, AB8500_CHARGER_CTRL, 0x01);

	ret = abx500_get_register_interruptible(di->dev, AB8500_CHARGER,
			AB8500_CH_USBCH_STAT2_REG, &reg_value);
	if (ret < 0) {
		dev_err(di->dev, "%s read failed\n", __func__);
		return;
	}

	curr_ua = ab8500_charge_input_curr_map[
		reg_value >> AUTO_VBUS_IN_CURR_LIM_SHIFT];

	if (di->max_usb_in_curr.calculated_max_ua != curr_ua) {
		 
		di->max_usb_in_curr.calculated_max_ua = curr_ua;
		dev_dbg(di->dev,
			 "VBUS input current limiting to %d uA\n",
			 di->max_usb_in_curr.calculated_max_ua);
	} else {
		 
		di->max_usb_in_curr.set_max_ua =
			di->max_usb_in_curr.calculated_max_ua;
		dev_dbg(di->dev,
			 "VBUS input current limited to %d uA\n",
			 di->max_usb_in_curr.set_max_ua);
	}

	if (di->usb.charger_connected)
		ab8500_charger_set_vbus_in_curr(di,
					di->max_usb_in_curr.usb_type_max_ua);
}

 
static irqreturn_t ab8500_charger_vbusdetf_handler(int irq, void *_di)
{
	struct ab8500_charger *di = _di;

	di->vbus_detected = false;
	dev_dbg(di->dev, "VBUS falling detected\n");
	queue_work(di->charger_wq, &di->detect_usb_type_work);

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_charger_vbusdetr_handler(int irq, void *_di)
{
	struct ab8500_charger *di = _di;

	di->vbus_detected = true;
	dev_dbg(di->dev, "VBUS rising detected\n");

	queue_work(di->charger_wq, &di->detect_usb_type_work);

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_charger_usblinkstatus_handler(int irq, void *_di)
{
	struct ab8500_charger *di = _di;

	dev_dbg(di->dev, "USB link status changed\n");

	queue_work(di->charger_wq, &di->usb_link_status_work);

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_charger_usbchthprotr_handler(int irq, void *_di)
{
	struct ab8500_charger *di = _di;

	dev_dbg(di->dev,
		"Die temp above USB charger thermal protection threshold\n");
	queue_work(di->charger_wq, &di->check_usb_thermal_prot_work);

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_charger_usbchthprotf_handler(int irq, void *_di)
{
	struct ab8500_charger *di = _di;

	dev_dbg(di->dev,
		"Die temp ok for USB charger thermal protection threshold\n");
	queue_work(di->charger_wq, &di->check_usb_thermal_prot_work);

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_charger_usbchargernotokr_handler(int irq, void *_di)
{
	struct ab8500_charger *di = _di;

	dev_dbg(di->dev, "Not allowed USB charger detected\n");
	queue_delayed_work(di->charger_wq, &di->check_usbchgnotok_work, 0);

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_charger_chwdexp_handler(int irq, void *_di)
{
	struct ab8500_charger *di = _di;

	dev_dbg(di->dev, "Charger watchdog expired\n");

	 
	if (di->ac.charger_online) {
		di->ac.wd_expired = true;
		ab8500_power_supply_changed(di, di->ac_chg.psy);
	}
	if (di->usb.charger_online) {
		di->usb.wd_expired = true;
		ab8500_power_supply_changed(di, di->usb_chg.psy);
	}

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_charger_vbuschdropend_handler(int irq, void *_di)
{
	struct ab8500_charger *di = _di;

	dev_dbg(di->dev, "VBUS charger drop ended\n");
	di->flags.vbus_drop_end = true;

	 
	queue_delayed_work(di->charger_wq, &di->vbus_drop_end_work,
			   round_jiffies(VBUS_IN_CURR_LIM_RETRY_SET_TIME * HZ));

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_charger_vbusovv_handler(int irq, void *_di)
{
	struct ab8500_charger *di = _di;

	dev_dbg(di->dev, "VBUS overvoltage detected\n");
	di->flags.vbus_ovv = true;
	ab8500_power_supply_changed(di, di->usb_chg.psy);

	 
	queue_delayed_work(di->charger_wq, &di->check_hw_failure_work, 0);

	return IRQ_HANDLED;
}

 
static int ab8500_charger_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	struct ab8500_charger *di;
	int ret;

	di = to_ab8500_charger_ac_device_info(psy_to_ux500_charger(psy));

	switch (psp) {
	case POWER_SUPPLY_PROP_HEALTH:
		if (di->flags.mainextchnotok)
			val->intval = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		else if (di->ac.wd_expired || di->usb.wd_expired)
			val->intval = POWER_SUPPLY_HEALTH_DEAD;
		else if (di->flags.main_thermal_prot)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = di->ac.charger_online;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = di->ac.charger_connected;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = ab8500_charger_get_ac_voltage(di);
		if (ret >= 0)
			di->ac.charger_voltage_uv = ret;
		 
		val->intval = di->ac.charger_voltage_uv;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		 
		di->ac.cv_active = ab8500_charger_ac_cv(di);
		val->intval = di->ac.cv_active;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = ab8500_charger_get_ac_current(di);
		if (ret >= 0)
			di->ac.charger_current_ua = ret;
		val->intval = di->ac.charger_current_ua;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

 
static int ab8500_charger_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	struct ab8500_charger *di;
	int ret;

	di = to_ab8500_charger_usb_device_info(psy_to_ux500_charger(psy));

	switch (psp) {
	case POWER_SUPPLY_PROP_HEALTH:
		if (di->flags.usbchargernotok)
			val->intval = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		else if (di->ac.wd_expired || di->usb.wd_expired)
			val->intval = POWER_SUPPLY_HEALTH_DEAD;
		else if (di->flags.usb_thermal_prot)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (di->flags.vbus_ovv)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = di->usb.charger_online;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = di->usb.charger_connected;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = ab8500_charger_get_vbus_voltage(di);
		if (ret >= 0)
			di->usb.charger_voltage_uv = ret;
		val->intval = di->usb.charger_voltage_uv;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		 
		di->usb.cv_active = ab8500_charger_usb_cv(di);
		val->intval = di->usb.cv_active;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = ab8500_charger_get_usb_current(di);
		if (ret >= 0)
			di->usb.charger_current_ua = ret;
		val->intval = di->usb.charger_current_ua;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		 
		if (di->flags.vbus_collapse)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

 
static int ab8500_charger_init_hw_registers(struct ab8500_charger *di)
{
	int ret = 0;

	 
	if (!is_ab8500_1p1_or_earlier(di->parent)) {
		ret = abx500_set_register_interruptible(di->dev,
			AB8500_CHARGER,
			AB8500_CH_VOLT_LVL_MAX_REG, CH_VOL_LVL_4P6);
		if (ret) {
			dev_err(di->dev,
				"failed to set CH_VOLT_LVL_MAX_REG\n");
			goto out;
		}

		ret = abx500_set_register_interruptible(di->dev,
			AB8500_CHARGER, AB8500_CH_OPT_CRNTLVL_MAX_REG,
			CH_OP_CUR_LVL_1P6);
		if (ret) {
			dev_err(di->dev,
				"failed to set CH_OPT_CRNTLVL_MAX_REG\n");
			goto out;
		}
	}

	if (is_ab8505_2p0(di->parent))
		ret = abx500_mask_and_set_register_interruptible(di->dev,
			AB8500_CHARGER,
			AB8500_USBCH_CTRL2_REG,
			VBUS_AUTO_IN_CURR_LIM_ENA,
			VBUS_AUTO_IN_CURR_LIM_ENA);
	else
		 
		ret = abx500_set_register_interruptible(di->dev,
			AB8500_CHARGER,
			AB8500_USBCH_CTRL2_REG,
			VBUS_OVV_SELECT_6P3V | VBUS_AUTO_IN_CURR_LIM_ENA);
	if (ret) {
		dev_err(di->dev,
			"failed to set automatic current limitation\n");
		goto out;
	}

	 
	ret = abx500_set_register_interruptible(di->dev,
		AB8500_OTP_EMUL, AB8500_OTP_CONF_15, OTP_ENABLE_WD);
	if (ret) {
		dev_err(di->dev, "failed to enable main WD in OTP\n");
		goto out;
	}

	 
	ret = abx500_set_register_interruptible(di->dev,
		AB8500_SYS_CTRL2_BLOCK,
		AB8500_MAIN_WDOG_CTRL_REG, MAIN_WDOG_ENA);
	if (ret) {
		dev_err(di->dev, "failed to enable main watchdog\n");
		goto out;
	}

	 
	udelay(63);

	 
	ret = abx500_set_register_interruptible(di->dev,
		AB8500_SYS_CTRL2_BLOCK,
		AB8500_MAIN_WDOG_CTRL_REG,
		(MAIN_WDOG_ENA | MAIN_WDOG_KICK));
	if (ret) {
		dev_err(di->dev, "failed to kick main watchdog\n");
		goto out;
	}

	 
	ret = abx500_set_register_interruptible(di->dev,
		AB8500_SYS_CTRL2_BLOCK,
		AB8500_MAIN_WDOG_CTRL_REG, MAIN_WDOG_DIS);
	if (ret) {
		dev_err(di->dev, "failed to disable main watchdog\n");
		goto out;
	}

	 
	ret = abx500_set_register_interruptible(di->dev, AB8500_CHARGER,
		AB8500_CH_WD_TIMER_REG, WD_TIMER);
	if (ret) {
		dev_err(di->dev, "failed to set charger watchdog timeout\n");
		goto out;
	}

	ret = ab8500_charger_led_en(di, false);
	if (ret < 0) {
		dev_err(di->dev, "failed to disable LED\n");
		goto out;
	}

	ret = abx500_set_register_interruptible(di->dev,
		AB8500_RTC,
		AB8500_RTC_BACKUP_CHG_REG,
		(di->bm->bkup_bat_v & 0x3) | di->bm->bkup_bat_i);
	if (ret) {
		dev_err(di->dev, "failed to setup backup battery charging\n");
		goto out;
	}

	 
	ret = abx500_mask_and_set_register_interruptible(di->dev,
		AB8500_RTC, AB8500_RTC_CTRL_REG,
		RTC_BUP_CH_ENA, RTC_BUP_CH_ENA);
	if (ret < 0) {
		dev_err(di->dev, "%s mask and set failed\n", __func__);
		goto out;
	}

out:
	return ret;
}

 
static struct ab8500_charger_interrupts ab8500_charger_irq[] = {
	{"MAIN_CH_UNPLUG_DET", ab8500_charger_mainchunplugdet_handler},
	{"MAIN_CHARGE_PLUG_DET", ab8500_charger_mainchplugdet_handler},
	{"MAIN_EXT_CH_NOT_OK", ab8500_charger_mainextchnotok_handler},
	{"MAIN_CH_TH_PROT_R", ab8500_charger_mainchthprotr_handler},
	{"MAIN_CH_TH_PROT_F", ab8500_charger_mainchthprotf_handler},
	{"VBUS_DET_F", ab8500_charger_vbusdetf_handler},
	{"VBUS_DET_R", ab8500_charger_vbusdetr_handler},
	{"USB_LINK_STATUS", ab8500_charger_usblinkstatus_handler},
	{"USB_CH_TH_PROT_R", ab8500_charger_usbchthprotr_handler},
	{"USB_CH_TH_PROT_F", ab8500_charger_usbchthprotf_handler},
	{"USB_CHARGER_NOT_OKR", ab8500_charger_usbchargernotokr_handler},
	{"VBUS_OVV", ab8500_charger_vbusovv_handler},
	{"CH_WD_EXP", ab8500_charger_chwdexp_handler},
	{"VBUS_CH_DROP_END", ab8500_charger_vbuschdropend_handler},
};

static int ab8500_charger_usb_notifier_call(struct notifier_block *nb,
		unsigned long event, void *power)
{
	struct ab8500_charger *di =
		container_of(nb, struct ab8500_charger, nb);
	enum ab8500_usb_state bm_usb_state;
	 
	unsigned mA = *((unsigned *)power);

	if (event != USB_EVENT_VBUS) {
		dev_dbg(di->dev, "not a standard host, returning\n");
		return NOTIFY_DONE;
	}

	 
	if ((di->usb_state.usb_current_ua == 2000) && (mA > 2))
		bm_usb_state = AB8500_BM_USB_STATE_RESUME;
	else if (mA == 0)
		bm_usb_state = AB8500_BM_USB_STATE_RESET_HS;
	else if (mA == 2)
		bm_usb_state = AB8500_BM_USB_STATE_SUSPEND;
	else if (mA >= 8)  
		bm_usb_state = AB8500_BM_USB_STATE_CONFIGURED;
	else  
		bm_usb_state = AB8500_BM_USB_STATE_RESET_FS;

	dev_dbg(di->dev, "%s usb_state: 0x%02x mA: %d\n",
		__func__, bm_usb_state, mA);

	spin_lock(&di->usb_state.usb_lock);
	di->usb_state.state_tmp = bm_usb_state;
	 
	di->usb_state.usb_current_tmp_ua = mA * 1000;
	spin_unlock(&di->usb_state.usb_lock);

	 
	queue_delayed_work(di->charger_wq, &di->usb_state_changed_work, HZ/2);

	return NOTIFY_OK;
}

static int __maybe_unused ab8500_charger_resume(struct device *dev)
{
	int ret;
	struct ab8500_charger *di = dev_get_drvdata(dev);

	 
	if (di->ac_conn && is_ab8500_1p1_or_earlier(di->parent)) {
		ret = abx500_set_register_interruptible(di->dev, AB8500_CHARGER,
			AB8500_CHARG_WD_CTRL, CHARG_WD_KICK);
		if (ret)
			dev_err(di->dev, "Failed to kick WD!\n");

		 
		queue_delayed_work(di->charger_wq, &di->kick_wd_work,
				   round_jiffies(WD_KICK_INTERVAL));
	}

	 
	if (di->flags.mainextchnotok || di->flags.vbus_ovv) {
		queue_delayed_work(di->charger_wq,
			&di->check_hw_failure_work, 0);
	}

	if (di->flags.vbus_drop_end)
		queue_delayed_work(di->charger_wq, &di->vbus_drop_end_work, 0);

	return 0;
}

static int __maybe_unused ab8500_charger_suspend(struct device *dev)
{
	struct ab8500_charger *di = dev_get_drvdata(dev);

	 
	cancel_delayed_work(&di->check_hw_failure_work);
	cancel_delayed_work(&di->vbus_drop_end_work);

	flush_delayed_work(&di->attach_work);
	flush_delayed_work(&di->usb_charger_attached_work);
	flush_delayed_work(&di->ac_charger_attached_work);
	flush_delayed_work(&di->check_usbchgnotok_work);
	flush_delayed_work(&di->check_vbat_work);
	flush_delayed_work(&di->kick_wd_work);

	flush_work(&di->usb_link_status_work);
	flush_work(&di->ac_work);
	flush_work(&di->detect_usb_type_work);

	if (atomic_read(&di->current_stepping_sessions))
		return -EAGAIN;

	return 0;
}

static char *supply_interface[] = {
	"ab8500_chargalg",
	"ab8500_fg",
	"ab8500_btemp",
};

static const struct power_supply_desc ab8500_ac_chg_desc = {
	.name		= "ab8500_ac",
	.type		= POWER_SUPPLY_TYPE_MAINS,
	.properties	= ab8500_charger_ac_props,
	.num_properties	= ARRAY_SIZE(ab8500_charger_ac_props),
	.get_property	= ab8500_charger_ac_get_property,
};

static const struct power_supply_desc ab8500_usb_chg_desc = {
	.name		= "ab8500_usb",
	.type		= POWER_SUPPLY_TYPE_USB,
	.properties	= ab8500_charger_usb_props,
	.num_properties	= ARRAY_SIZE(ab8500_charger_usb_props),
	.get_property	= ab8500_charger_usb_get_property,
};

static int ab8500_charger_bind(struct device *dev)
{
	struct ab8500_charger *di = dev_get_drvdata(dev);
	int ch_stat;
	int ret;

	 
	di->charger_wq = alloc_ordered_workqueue("ab8500_charger_wq",
						 WQ_MEM_RECLAIM);
	if (di->charger_wq == NULL) {
		dev_err(dev, "failed to create work queue\n");
		return -ENOMEM;
	}

	ch_stat = ab8500_charger_detect_chargers(di, false);

	if (ch_stat & AC_PW_CONN) {
		if (is_ab8500(di->parent))
			queue_delayed_work(di->charger_wq,
					   &di->ac_charger_attached_work,
					   HZ);
	}
	if (ch_stat & USB_PW_CONN) {
		if (is_ab8500(di->parent))
			queue_delayed_work(di->charger_wq,
					   &di->usb_charger_attached_work,
					   HZ);
		di->vbus_detected = true;
		di->vbus_detected_start = true;
		queue_work(di->charger_wq,
			   &di->detect_usb_type_work);
	}

	ret = component_bind_all(dev, di);
	if (ret) {
		dev_err(dev, "can't bind component devices\n");
		destroy_workqueue(di->charger_wq);
		return ret;
	}

	return 0;
}

static void ab8500_charger_unbind(struct device *dev)
{
	struct ab8500_charger *di = dev_get_drvdata(dev);
	int ret;

	 
	ab8500_charger_ac_en(&di->ac_chg, false, 0, 0);

	 
	ab8500_charger_usb_en(&di->usb_chg, false, 0, 0);

	 
	ret = abx500_mask_and_set_register_interruptible(di->dev,
		AB8500_RTC, AB8500_RTC_CTRL_REG, RTC_BUP_CH_ENA, 0);
	if (ret < 0)
		dev_err(di->dev, "%s mask and set failed\n", __func__);

	 
	destroy_workqueue(di->charger_wq);

	 
	component_unbind_all(dev, di);
}

static const struct component_master_ops ab8500_charger_comp_ops = {
	.bind = ab8500_charger_bind,
	.unbind = ab8500_charger_unbind,
};

static struct platform_driver *const ab8500_charger_component_drivers[] = {
	&ab8500_fg_driver,
	&ab8500_btemp_driver,
	&ab8500_chargalg_driver,
};

static int ab8500_charger_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct component_match *match = NULL;
	struct power_supply_config ac_psy_cfg = {}, usb_psy_cfg = {};
	struct ab8500_charger *di;
	int charger_status;
	int i, irq;
	int ret;

	di = devm_kzalloc(dev, sizeof(*di), GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	di->bm = &ab8500_bm_data;

	di->autopower_cfg = of_property_read_bool(np, "autopower_cfg");

	 
	di->dev = dev;
	di->parent = dev_get_drvdata(pdev->dev.parent);

	 
	if (!is_ab8505(di->parent)) {
		di->adc_main_charger_v = devm_iio_channel_get(dev, "main_charger_v");
		if (IS_ERR(di->adc_main_charger_v)) {
			ret = dev_err_probe(dev, PTR_ERR(di->adc_main_charger_v),
					    "failed to get ADC main charger voltage\n");
			return ret;
		}
		di->adc_main_charger_c = devm_iio_channel_get(dev, "main_charger_c");
		if (IS_ERR(di->adc_main_charger_c)) {
			ret = dev_err_probe(dev, PTR_ERR(di->adc_main_charger_c),
					    "failed to get ADC main charger current\n");
			return ret;
		}
	}
	di->adc_vbus_v = devm_iio_channel_get(dev, "vbus_v");
	if (IS_ERR(di->adc_vbus_v)) {
		ret = dev_err_probe(dev, PTR_ERR(di->adc_vbus_v),
				    "failed to get ADC USB charger voltage\n");
		return ret;
	}
	di->adc_usb_charger_c = devm_iio_channel_get(dev, "usb_charger_c");
	if (IS_ERR(di->adc_usb_charger_c)) {
		ret = dev_err_probe(dev, PTR_ERR(di->adc_usb_charger_c),
				    "failed to get ADC USB charger current\n");
		return ret;
	}

	 
	di->regu = devm_regulator_get(dev, "vddadc");
	if (IS_ERR(di->regu)) {
		ret = PTR_ERR(di->regu);
		dev_err(dev, "failed to get vddadc regulator\n");
		return ret;
	}

	 
	for (i = 0; i < ARRAY_SIZE(ab8500_charger_irq); i++) {
		irq = platform_get_irq_byname(pdev, ab8500_charger_irq[i].name);
		if (irq < 0)
			return irq;

		ret = devm_request_threaded_irq(dev,
			irq, NULL, ab8500_charger_irq[i].isr,
			IRQF_SHARED | IRQF_NO_SUSPEND | IRQF_ONESHOT,
			ab8500_charger_irq[i].name, di);

		if (ret != 0) {
			dev_err(dev, "failed to request %s IRQ %d: %d\n"
				, ab8500_charger_irq[i].name, irq, ret);
			return ret;
		}
		dev_dbg(dev, "Requested %s IRQ %d: %d\n",
			ab8500_charger_irq[i].name, irq, ret);
	}

	 
	spin_lock_init(&di->usb_state.usb_lock);
	mutex_init(&di->usb_ipt_crnt_lock);

	di->autopower = false;
	di->invalid_charger_detect_state = 0;

	 
	ac_psy_cfg.of_node = np;
	ac_psy_cfg.supplied_to = supply_interface;
	ac_psy_cfg.num_supplicants = ARRAY_SIZE(supply_interface);
	ac_psy_cfg.drv_data = &di->ac_chg;
	usb_psy_cfg.of_node = np;
	usb_psy_cfg.supplied_to = supply_interface;
	usb_psy_cfg.num_supplicants = ARRAY_SIZE(supply_interface);
	usb_psy_cfg.drv_data = &di->usb_chg;

	 
	 
	di->ac_chg.ops.enable = &ab8500_charger_ac_en;
	di->ac_chg.ops.check_enable = &ab8500_charger_ac_check_enable;
	di->ac_chg.ops.kick_wd = &ab8500_charger_watchdog_kick;
	di->ac_chg.ops.update_curr = &ab8500_charger_update_charger_current;
	di->ac_chg.max_out_volt_uv = ab8500_charger_voltage_map[
		ARRAY_SIZE(ab8500_charger_voltage_map) - 1];
	di->ac_chg.max_out_curr_ua =
		ab8500_charge_output_curr_map[ARRAY_SIZE(ab8500_charge_output_curr_map) - 1];
	di->ac_chg.wdt_refresh = CHG_WD_INTERVAL;
	 
	if (!is_ab8505(di->parent))
		di->ac_chg.enabled = true;

	 
	 
	di->usb_chg.ops.enable = &ab8500_charger_usb_en;
	di->usb_chg.ops.check_enable = &ab8500_charger_usb_check_enable;
	di->usb_chg.ops.kick_wd = &ab8500_charger_watchdog_kick;
	di->usb_chg.ops.update_curr = &ab8500_charger_update_charger_current;
	di->usb_chg.max_out_volt_uv = ab8500_charger_voltage_map[
		ARRAY_SIZE(ab8500_charger_voltage_map) - 1];
	di->usb_chg.max_out_curr_ua =
		ab8500_charge_output_curr_map[ARRAY_SIZE(ab8500_charge_output_curr_map) - 1];
	di->usb_chg.wdt_refresh = CHG_WD_INTERVAL;
	di->usb_state.usb_current_ua = -1;

	mutex_init(&di->charger_attached_mutex);

	 
	INIT_DEFERRABLE_WORK(&di->check_hw_failure_work,
		ab8500_charger_check_hw_failure_work);
	INIT_DEFERRABLE_WORK(&di->check_usbchgnotok_work,
		ab8500_charger_check_usbchargernotok_work);

	INIT_DELAYED_WORK(&di->ac_charger_attached_work,
			  ab8500_charger_ac_attached_work);
	INIT_DELAYED_WORK(&di->usb_charger_attached_work,
			  ab8500_charger_usb_attached_work);

	 
	INIT_DEFERRABLE_WORK(&di->kick_wd_work,
		ab8500_charger_kick_watchdog_work);

	INIT_DEFERRABLE_WORK(&di->check_vbat_work,
		ab8500_charger_check_vbat_work);

	INIT_DELAYED_WORK(&di->attach_work,
		ab8500_charger_usb_link_attach_work);

	INIT_DELAYED_WORK(&di->usb_state_changed_work,
		ab8500_charger_usb_state_changed_work);

	INIT_DELAYED_WORK(&di->vbus_drop_end_work,
		ab8500_charger_vbus_drop_end_work);

	 
	INIT_WORK(&di->usb_link_status_work,
		ab8500_charger_usb_link_status_work);
	INIT_WORK(&di->ac_work, ab8500_charger_ac_work);
	INIT_WORK(&di->detect_usb_type_work,
		ab8500_charger_detect_usb_type_work);

	 
	INIT_WORK(&di->check_main_thermal_prot_work,
		ab8500_charger_check_main_thermal_prot_work);
	INIT_WORK(&di->check_usb_thermal_prot_work,
		ab8500_charger_check_usb_thermal_prot_work);


	 
	ret = ab8500_charger_init_hw_registers(di);
	if (ret) {
		dev_err(dev, "failed to initialize ABB registers\n");
		return ret;
	}

	 
	if (di->ac_chg.enabled) {
		di->ac_chg.psy = devm_power_supply_register(dev,
						       &ab8500_ac_chg_desc,
						       &ac_psy_cfg);
		if (IS_ERR(di->ac_chg.psy)) {
			dev_err(dev, "failed to register AC charger\n");
			return PTR_ERR(di->ac_chg.psy);
		}
	}

	 
	di->usb_chg.psy = devm_power_supply_register(dev,
						     &ab8500_usb_chg_desc,
						     &usb_psy_cfg);
	if (IS_ERR(di->usb_chg.psy)) {
		dev_err(dev, "failed to register USB charger\n");
		return PTR_ERR(di->usb_chg.psy);
	}

	 
	ret = ab8500_bm_of_probe(di->usb_chg.psy, di->bm);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to get battery information\n");

	 
	charger_status = ab8500_charger_detect_chargers(di, true);
	if (charger_status & AC_PW_CONN) {
		di->ac.charger_connected = 1;
		di->ac_conn = true;
		ab8500_power_supply_changed(di, di->ac_chg.psy);
		sysfs_notify(&di->ac_chg.psy->dev.kobj, NULL, "present");
	}

	platform_set_drvdata(pdev, di);

	 
	for (i = 0; i < ARRAY_SIZE(ab8500_charger_component_drivers); i++) {
		struct device_driver *drv = &ab8500_charger_component_drivers[i]->driver;
		struct device *p = NULL, *d;

		while ((d = platform_find_device_by_driver(p, drv))) {
			put_device(p);
			component_match_add(dev, &match, component_compare_dev, d);
			p = d;
		}
		put_device(p);
	}
	if (!match) {
		dev_err(dev, "no matching components\n");
		ret = -ENODEV;
		goto remove_ab8500_bm;
	}
	if (IS_ERR(match)) {
		dev_err(dev, "could not create component match\n");
		ret = PTR_ERR(match);
		goto remove_ab8500_bm;
	}

	di->usb_phy = usb_get_phy(USB_PHY_TYPE_USB2);
	if (IS_ERR_OR_NULL(di->usb_phy)) {
		dev_err(dev, "failed to get usb transceiver\n");
		ret = -EINVAL;
		goto remove_ab8500_bm;
	}
	di->nb.notifier_call = ab8500_charger_usb_notifier_call;
	ret = usb_register_notifier(di->usb_phy, &di->nb);
	if (ret) {
		dev_err(dev, "failed to register usb notifier\n");
		goto put_usb_phy;
	}

	ret = component_master_add_with_match(&pdev->dev,
					      &ab8500_charger_comp_ops,
					      match);
	if (ret) {
		dev_err(dev, "failed to add component master\n");
		goto free_notifier;
	}

	return 0;

free_notifier:
	usb_unregister_notifier(di->usb_phy, &di->nb);
put_usb_phy:
	usb_put_phy(di->usb_phy);
remove_ab8500_bm:
	ab8500_bm_of_remove(di->usb_chg.psy, di->bm);
	return ret;
}

static int ab8500_charger_remove(struct platform_device *pdev)
{
	struct ab8500_charger *di = platform_get_drvdata(pdev);

	component_master_del(&pdev->dev, &ab8500_charger_comp_ops);

	usb_unregister_notifier(di->usb_phy, &di->nb);
	ab8500_bm_of_remove(di->usb_chg.psy, di->bm);
	usb_put_phy(di->usb_phy);

	return 0;
}

static SIMPLE_DEV_PM_OPS(ab8500_charger_pm_ops, ab8500_charger_suspend, ab8500_charger_resume);

static const struct of_device_id ab8500_charger_match[] = {
	{ .compatible = "stericsson,ab8500-charger", },
	{ },
};
MODULE_DEVICE_TABLE(of, ab8500_charger_match);

static struct platform_driver ab8500_charger_driver = {
	.probe = ab8500_charger_probe,
	.remove = ab8500_charger_remove,
	.driver = {
		.name = "ab8500-charger",
		.of_match_table = ab8500_charger_match,
		.pm = &ab8500_charger_pm_ops,
	},
};

static int __init ab8500_charger_init(void)
{
	int ret;

	ret = platform_register_drivers(ab8500_charger_component_drivers,
			ARRAY_SIZE(ab8500_charger_component_drivers));
	if (ret)
		return ret;

	ret = platform_driver_register(&ab8500_charger_driver);
	if (ret) {
		platform_unregister_drivers(ab8500_charger_component_drivers,
				ARRAY_SIZE(ab8500_charger_component_drivers));
		return ret;
	}

	return 0;
}

static void __exit ab8500_charger_exit(void)
{
	platform_unregister_drivers(ab8500_charger_component_drivers,
			ARRAY_SIZE(ab8500_charger_component_drivers));
	platform_driver_unregister(&ab8500_charger_driver);
}

module_init(ab8500_charger_init);
module_exit(ab8500_charger_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Johan Palsson, Karl Komierowski, Arun R Murthy");
MODULE_ALIAS("platform:ab8500-charger");
MODULE_DESCRIPTION("AB8500 charger management driver");
