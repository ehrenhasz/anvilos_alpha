
 

#include <linux/bits.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/hwmon.h>
#include <linux/kstrtox.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

 
#define MAX_CHANNELS	3

 

static const unsigned short normal_i2c[] = {
	0x18, 0x19, 0x1a, 0x29, 0x2a, 0x2b, 0x48, 0x49, 0x4a, 0x4b, 0x4c,
	0x4d, 0x4e, 0x4f, I2C_CLIENT_END };

enum chips { adm1023, adm1032, adt7461, adt7461a, adt7481,
	g781, lm84, lm90, lm99,
	max1617, max6642, max6646, max6648, max6654, max6657, max6659, max6680, max6696,
	nct210, nct72, ne1618, sa56004, tmp451, tmp461, w83l771,
};

 

#define LM90_REG_MAN_ID			0xFE
#define LM90_REG_CHIP_ID		0xFF
#define LM90_REG_CONFIG1		0x03
#define LM90_REG_CONFIG2		0xBF
#define LM90_REG_CONVRATE		0x04
#define LM90_REG_STATUS			0x02
#define LM90_REG_LOCAL_TEMP		0x00
#define LM90_REG_LOCAL_HIGH		0x05
#define LM90_REG_LOCAL_LOW		0x06
#define LM90_REG_LOCAL_CRIT		0x20
#define LM90_REG_REMOTE_TEMPH		0x01
#define LM90_REG_REMOTE_TEMPL		0x10
#define LM90_REG_REMOTE_OFFSH		0x11
#define LM90_REG_REMOTE_OFFSL		0x12
#define LM90_REG_REMOTE_HIGHH		0x07
#define LM90_REG_REMOTE_HIGHL		0x13
#define LM90_REG_REMOTE_LOWH		0x08
#define LM90_REG_REMOTE_LOWL		0x14
#define LM90_REG_REMOTE_CRIT		0x19
#define LM90_REG_TCRIT_HYST		0x21

 

#define MAX6657_REG_LOCAL_TEMPL		0x11
#define MAX6696_REG_STATUS2		0x12
#define MAX6659_REG_REMOTE_EMERG	0x16
#define MAX6659_REG_LOCAL_EMERG		0x17

 

#define SA56004_REG_LOCAL_TEMPL		0x22

#define LM90_MAX_CONVRATE_MS	16000	 

 
#define TMP451_REG_LOCAL_TEMPL		0x15
#define TMP451_REG_CONALERT		0x22

#define TMP461_REG_CHEN			0x16
#define TMP461_REG_DFC			0x24

 
#define ADT7481_REG_STATUS2		0x23
#define ADT7481_REG_CONFIG2		0x24

#define ADT7481_REG_MAN_ID		0x3e
#define ADT7481_REG_CHIP_ID		0x3d

 
#define LM90_HAVE_EXTENDED_TEMP	BIT(0)	 
#define LM90_HAVE_OFFSET	BIT(1)	 
#define LM90_HAVE_UNSIGNED_TEMP	BIT(2)	 
#define LM90_HAVE_REM_LIMIT_EXT	BIT(3)	 
#define LM90_HAVE_EMERGENCY	BIT(4)	 
#define LM90_HAVE_EMERGENCY_ALARM BIT(5) 
#define LM90_HAVE_TEMP3		BIT(6)	 
#define LM90_HAVE_BROKEN_ALERT	BIT(7)	 
#define LM90_PAUSE_FOR_CONFIG	BIT(8)	 
#define LM90_HAVE_CRIT		BIT(9)	 
#define LM90_HAVE_CRIT_ALRM_SWP	BIT(10)	 
#define LM90_HAVE_PEC		BIT(11)	 
#define LM90_HAVE_PARTIAL_PEC	BIT(12)	 
#define LM90_HAVE_ALARMS	BIT(13)	 
#define LM90_HAVE_EXT_UNSIGNED	BIT(14)	 
#define LM90_HAVE_LOW		BIT(15)	 
#define LM90_HAVE_CONVRATE	BIT(16)	 
#define LM90_HAVE_REMOTE_EXT	BIT(17)	 
#define LM90_HAVE_FAULTQUEUE	BIT(18)	 

 
#define LM90_STATUS_LTHRM	BIT(0)	 
#define LM90_STATUS_RTHRM	BIT(1)	 
#define LM90_STATUS_ROPEN	BIT(2)	 
#define LM90_STATUS_RLOW	BIT(3)	 
#define LM90_STATUS_RHIGH	BIT(4)	 
#define LM90_STATUS_LLOW	BIT(5)	 
#define LM90_STATUS_LHIGH	BIT(6)	 
#define LM90_STATUS_BUSY	BIT(7)	 

 
#define MAX6696_STATUS2_R2THRM	BIT(1)	 
#define MAX6696_STATUS2_R2OPEN	BIT(2)	 
#define MAX6696_STATUS2_R2LOW	BIT(3)	 
#define MAX6696_STATUS2_R2HIGH	BIT(4)	 
#define MAX6696_STATUS2_ROT2	BIT(5)	 
#define MAX6696_STATUS2_R2OT2	BIT(6)	 
#define MAX6696_STATUS2_LOT2	BIT(7)	 

 

static const struct i2c_device_id lm90_id[] = {
	{ "adm1020", max1617 },
	{ "adm1021", max1617 },
	{ "adm1023", adm1023 },
	{ "adm1032", adm1032 },
	{ "adt7421", adt7461a },
	{ "adt7461", adt7461 },
	{ "adt7461a", adt7461a },
	{ "adt7481", adt7481 },
	{ "adt7482", adt7481 },
	{ "adt7483a", adt7481 },
	{ "g781", g781 },
	{ "gl523sm", max1617 },
	{ "lm84", lm84 },
	{ "lm86", lm90 },
	{ "lm89", lm90 },
	{ "lm90", lm90 },
	{ "lm99", lm99 },
	{ "max1617", max1617 },
	{ "max6642", max6642 },
	{ "max6646", max6646 },
	{ "max6647", max6646 },
	{ "max6648", max6648 },
	{ "max6649", max6646 },
	{ "max6654", max6654 },
	{ "max6657", max6657 },
	{ "max6658", max6657 },
	{ "max6659", max6659 },
	{ "max6680", max6680 },
	{ "max6681", max6680 },
	{ "max6690", max6654 },
	{ "max6692", max6648 },
	{ "max6695", max6696 },
	{ "max6696", max6696 },
	{ "mc1066", max1617 },
	{ "nct1008", adt7461a },
	{ "nct210", nct210 },
	{ "nct214", nct72 },
	{ "nct218", nct72 },
	{ "nct72", nct72 },
	{ "ne1618", ne1618 },
	{ "w83l771", w83l771 },
	{ "sa56004", sa56004 },
	{ "thmc10", max1617 },
	{ "tmp451", tmp451 },
	{ "tmp461", tmp461 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm90_id);

static const struct of_device_id __maybe_unused lm90_of_match[] = {
	{
		.compatible = "adi,adm1032",
		.data = (void *)adm1032
	},
	{
		.compatible = "adi,adt7461",
		.data = (void *)adt7461
	},
	{
		.compatible = "adi,adt7461a",
		.data = (void *)adt7461a
	},
	{
		.compatible = "adi,adt7481",
		.data = (void *)adt7481
	},
	{
		.compatible = "gmt,g781",
		.data = (void *)g781
	},
	{
		.compatible = "national,lm90",
		.data = (void *)lm90
	},
	{
		.compatible = "national,lm86",
		.data = (void *)lm90
	},
	{
		.compatible = "national,lm89",
		.data = (void *)lm90
	},
	{
		.compatible = "national,lm99",
		.data = (void *)lm99
	},
	{
		.compatible = "dallas,max6646",
		.data = (void *)max6646
	},
	{
		.compatible = "dallas,max6647",
		.data = (void *)max6646
	},
	{
		.compatible = "dallas,max6649",
		.data = (void *)max6646
	},
	{
		.compatible = "dallas,max6654",
		.data = (void *)max6654
	},
	{
		.compatible = "dallas,max6657",
		.data = (void *)max6657
	},
	{
		.compatible = "dallas,max6658",
		.data = (void *)max6657
	},
	{
		.compatible = "dallas,max6659",
		.data = (void *)max6659
	},
	{
		.compatible = "dallas,max6680",
		.data = (void *)max6680
	},
	{
		.compatible = "dallas,max6681",
		.data = (void *)max6680
	},
	{
		.compatible = "dallas,max6695",
		.data = (void *)max6696
	},
	{
		.compatible = "dallas,max6696",
		.data = (void *)max6696
	},
	{
		.compatible = "onnn,nct1008",
		.data = (void *)adt7461a
	},
	{
		.compatible = "onnn,nct214",
		.data = (void *)nct72
	},
	{
		.compatible = "onnn,nct218",
		.data = (void *)nct72
	},
	{
		.compatible = "onnn,nct72",
		.data = (void *)nct72
	},
	{
		.compatible = "winbond,w83l771",
		.data = (void *)w83l771
	},
	{
		.compatible = "nxp,sa56004",
		.data = (void *)sa56004
	},
	{
		.compatible = "ti,tmp451",
		.data = (void *)tmp451
	},
	{
		.compatible = "ti,tmp461",
		.data = (void *)tmp461
	},
	{ },
};
MODULE_DEVICE_TABLE(of, lm90_of_match);

 
struct lm90_params {
	u32 flags;		 
	u16 alert_alarms;	 
				 
	u8 max_convrate;	 
	u8 resolution;		 
	u8 reg_status2;		 
	u8 reg_local_ext;	 
	u8 faultqueue_mask;	 
	u8 faultqueue_depth;	 
};

static const struct lm90_params lm90_params[] = {
	[adm1023] = {
		.flags = LM90_HAVE_ALARMS | LM90_HAVE_OFFSET | LM90_HAVE_BROKEN_ALERT
		  | LM90_HAVE_REM_LIMIT_EXT | LM90_HAVE_LOW | LM90_HAVE_CONVRATE
		  | LM90_HAVE_REMOTE_EXT,
		.alert_alarms = 0x7c,
		.resolution = 8,
		.max_convrate = 7,
	},
	[adm1032] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT
		  | LM90_HAVE_BROKEN_ALERT | LM90_HAVE_CRIT
		  | LM90_HAVE_PARTIAL_PEC | LM90_HAVE_ALARMS
		  | LM90_HAVE_LOW | LM90_HAVE_CONVRATE | LM90_HAVE_REMOTE_EXT
		  | LM90_HAVE_FAULTQUEUE,
		.alert_alarms = 0x7c,
		.max_convrate = 10,
	},
	[adt7461] = {
		 
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT
		  | LM90_HAVE_BROKEN_ALERT | LM90_HAVE_EXTENDED_TEMP
		  | LM90_HAVE_CRIT | LM90_HAVE_PARTIAL_PEC
		  | LM90_HAVE_ALARMS | LM90_HAVE_LOW | LM90_HAVE_CONVRATE
		  | LM90_HAVE_REMOTE_EXT | LM90_HAVE_FAULTQUEUE,
		.alert_alarms = 0x7c,
		.max_convrate = 10,
		.resolution = 10,
	},
	[adt7461a] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT
		  | LM90_HAVE_BROKEN_ALERT | LM90_HAVE_EXTENDED_TEMP
		  | LM90_HAVE_CRIT | LM90_HAVE_PEC | LM90_HAVE_ALARMS
		  | LM90_HAVE_LOW | LM90_HAVE_CONVRATE | LM90_HAVE_REMOTE_EXT
		  | LM90_HAVE_FAULTQUEUE,
		.alert_alarms = 0x7c,
		.max_convrate = 10,
	},
	[adt7481] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT
		  | LM90_HAVE_BROKEN_ALERT | LM90_HAVE_EXTENDED_TEMP
		  | LM90_HAVE_UNSIGNED_TEMP | LM90_HAVE_PEC
		  | LM90_HAVE_TEMP3 | LM90_HAVE_CRIT | LM90_HAVE_LOW
		  | LM90_HAVE_CONVRATE | LM90_HAVE_REMOTE_EXT
		  | LM90_HAVE_FAULTQUEUE,
		.alert_alarms = 0x1c7c,
		.max_convrate = 11,
		.resolution = 10,
		.reg_status2 = ADT7481_REG_STATUS2,
	},
	[g781] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT
		  | LM90_HAVE_BROKEN_ALERT | LM90_HAVE_CRIT
		  | LM90_HAVE_ALARMS | LM90_HAVE_LOW | LM90_HAVE_CONVRATE
		  | LM90_HAVE_REMOTE_EXT | LM90_HAVE_FAULTQUEUE,
		.alert_alarms = 0x7c,
		.max_convrate = 7,
	},
	[lm84] = {
		.flags = LM90_HAVE_ALARMS,
		.resolution = 8,
	},
	[lm90] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT
		  | LM90_HAVE_CRIT | LM90_HAVE_ALARMS | LM90_HAVE_LOW
		  | LM90_HAVE_CONVRATE | LM90_HAVE_REMOTE_EXT
		  | LM90_HAVE_FAULTQUEUE,
		.alert_alarms = 0x7b,
		.max_convrate = 9,
		.faultqueue_mask = BIT(0),
		.faultqueue_depth = 3,
	},
	[lm99] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT
		  | LM90_HAVE_CRIT | LM90_HAVE_ALARMS | LM90_HAVE_LOW
		  | LM90_HAVE_CONVRATE | LM90_HAVE_REMOTE_EXT
		  | LM90_HAVE_FAULTQUEUE,
		.alert_alarms = 0x7b,
		.max_convrate = 9,
		.faultqueue_mask = BIT(0),
		.faultqueue_depth = 3,
	},
	[max1617] = {
		.flags = LM90_HAVE_CONVRATE | LM90_HAVE_BROKEN_ALERT |
		  LM90_HAVE_LOW | LM90_HAVE_ALARMS,
		.alert_alarms = 0x78,
		.resolution = 8,
		.max_convrate = 7,
	},
	[max6642] = {
		.flags = LM90_HAVE_BROKEN_ALERT | LM90_HAVE_EXT_UNSIGNED
		  | LM90_HAVE_REMOTE_EXT | LM90_HAVE_FAULTQUEUE,
		.alert_alarms = 0x50,
		.resolution = 10,
		.reg_local_ext = MAX6657_REG_LOCAL_TEMPL,
		.faultqueue_mask = BIT(4),
		.faultqueue_depth = 2,
	},
	[max6646] = {
		.flags = LM90_HAVE_CRIT | LM90_HAVE_BROKEN_ALERT
		  | LM90_HAVE_EXT_UNSIGNED | LM90_HAVE_ALARMS | LM90_HAVE_LOW
		  | LM90_HAVE_CONVRATE | LM90_HAVE_REMOTE_EXT,
		.alert_alarms = 0x7c,
		.max_convrate = 6,
		.reg_local_ext = MAX6657_REG_LOCAL_TEMPL,
	},
	[max6648] = {
		.flags = LM90_HAVE_UNSIGNED_TEMP | LM90_HAVE_CRIT
		  | LM90_HAVE_BROKEN_ALERT | LM90_HAVE_LOW
		  | LM90_HAVE_CONVRATE | LM90_HAVE_REMOTE_EXT,
		.alert_alarms = 0x7c,
		.max_convrate = 6,
		.reg_local_ext = MAX6657_REG_LOCAL_TEMPL,
	},
	[max6654] = {
		.flags = LM90_HAVE_BROKEN_ALERT | LM90_HAVE_ALARMS | LM90_HAVE_LOW
		  | LM90_HAVE_CONVRATE | LM90_HAVE_REMOTE_EXT,
		.alert_alarms = 0x7c,
		.max_convrate = 7,
		.reg_local_ext = MAX6657_REG_LOCAL_TEMPL,
	},
	[max6657] = {
		.flags = LM90_PAUSE_FOR_CONFIG | LM90_HAVE_CRIT
		  | LM90_HAVE_ALARMS | LM90_HAVE_LOW | LM90_HAVE_CONVRATE
		  | LM90_HAVE_REMOTE_EXT,
		.alert_alarms = 0x7c,
		.max_convrate = 8,
		.reg_local_ext = MAX6657_REG_LOCAL_TEMPL,
	},
	[max6659] = {
		.flags = LM90_HAVE_EMERGENCY | LM90_HAVE_CRIT
		  | LM90_HAVE_ALARMS | LM90_HAVE_LOW | LM90_HAVE_CONVRATE
		  | LM90_HAVE_REMOTE_EXT,
		.alert_alarms = 0x7c,
		.max_convrate = 8,
		.reg_local_ext = MAX6657_REG_LOCAL_TEMPL,
	},
	[max6680] = {
		 
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_CRIT
		  | LM90_HAVE_CRIT_ALRM_SWP | LM90_HAVE_BROKEN_ALERT
		  | LM90_HAVE_ALARMS | LM90_HAVE_LOW | LM90_HAVE_CONVRATE
		  | LM90_HAVE_REMOTE_EXT,
		.alert_alarms = 0x7c,
		.max_convrate = 7,
	},
	[max6696] = {
		.flags = LM90_HAVE_EMERGENCY
		  | LM90_HAVE_EMERGENCY_ALARM | LM90_HAVE_TEMP3 | LM90_HAVE_CRIT
		  | LM90_HAVE_ALARMS | LM90_HAVE_LOW | LM90_HAVE_CONVRATE
		  | LM90_HAVE_REMOTE_EXT | LM90_HAVE_FAULTQUEUE,
		.alert_alarms = 0x1c7c,
		.max_convrate = 6,
		.reg_status2 = MAX6696_REG_STATUS2,
		.reg_local_ext = MAX6657_REG_LOCAL_TEMPL,
		.faultqueue_mask = BIT(5),
		.faultqueue_depth = 4,
	},
	[nct72] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT
		  | LM90_HAVE_BROKEN_ALERT | LM90_HAVE_EXTENDED_TEMP
		  | LM90_HAVE_CRIT | LM90_HAVE_PEC | LM90_HAVE_UNSIGNED_TEMP
		  | LM90_HAVE_LOW | LM90_HAVE_CONVRATE | LM90_HAVE_REMOTE_EXT
		  | LM90_HAVE_FAULTQUEUE,
		.alert_alarms = 0x7c,
		.max_convrate = 10,
		.resolution = 10,
	},
	[nct210] = {
		.flags = LM90_HAVE_ALARMS | LM90_HAVE_BROKEN_ALERT
		  | LM90_HAVE_REM_LIMIT_EXT | LM90_HAVE_LOW | LM90_HAVE_CONVRATE
		  | LM90_HAVE_REMOTE_EXT,
		.alert_alarms = 0x7c,
		.resolution = 11,
		.max_convrate = 7,
	},
	[ne1618] = {
		.flags = LM90_PAUSE_FOR_CONFIG | LM90_HAVE_BROKEN_ALERT
		  | LM90_HAVE_LOW | LM90_HAVE_CONVRATE | LM90_HAVE_REMOTE_EXT,
		.alert_alarms = 0x7c,
		.resolution = 11,
		.max_convrate = 7,
	},
	[w83l771] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT | LM90_HAVE_CRIT
		  | LM90_HAVE_ALARMS | LM90_HAVE_LOW | LM90_HAVE_CONVRATE
		  | LM90_HAVE_REMOTE_EXT,
		.alert_alarms = 0x7c,
		.max_convrate = 8,
	},
	[sa56004] = {
		 
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT | LM90_HAVE_CRIT
		  | LM90_HAVE_ALARMS | LM90_HAVE_LOW | LM90_HAVE_CONVRATE
		  | LM90_HAVE_REMOTE_EXT | LM90_HAVE_FAULTQUEUE,
		.alert_alarms = 0x7b,
		.max_convrate = 9,
		.reg_local_ext = SA56004_REG_LOCAL_TEMPL,
		.faultqueue_mask = BIT(0),
		.faultqueue_depth = 3,
	},
	[tmp451] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT
		  | LM90_HAVE_BROKEN_ALERT | LM90_HAVE_EXTENDED_TEMP | LM90_HAVE_CRIT
		  | LM90_HAVE_UNSIGNED_TEMP | LM90_HAVE_ALARMS | LM90_HAVE_LOW
		  | LM90_HAVE_CONVRATE | LM90_HAVE_REMOTE_EXT | LM90_HAVE_FAULTQUEUE,
		.alert_alarms = 0x7c,
		.max_convrate = 9,
		.resolution = 12,
		.reg_local_ext = TMP451_REG_LOCAL_TEMPL,
	},
	[tmp461] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT
		  | LM90_HAVE_BROKEN_ALERT | LM90_HAVE_EXTENDED_TEMP | LM90_HAVE_CRIT
		  | LM90_HAVE_ALARMS | LM90_HAVE_LOW | LM90_HAVE_CONVRATE
		  | LM90_HAVE_REMOTE_EXT | LM90_HAVE_FAULTQUEUE,
		.alert_alarms = 0x7c,
		.max_convrate = 9,
		.resolution = 12,
		.reg_local_ext = TMP451_REG_LOCAL_TEMPL,
	},
};

 
enum lm90_temp_reg_index {
	LOCAL_LOW = 0,
	LOCAL_HIGH,
	LOCAL_CRIT,
	REMOTE_CRIT,
	LOCAL_EMERG,	 
	REMOTE_EMERG,	 
	REMOTE2_CRIT,	 
	REMOTE2_EMERG,	 

	REMOTE_TEMP,
	REMOTE_LOW,
	REMOTE_HIGH,
	REMOTE_OFFSET,	 
	LOCAL_TEMP,
	REMOTE2_TEMP,	 
	REMOTE2_LOW,	 
	REMOTE2_HIGH,	 
	REMOTE2_OFFSET,

	TEMP_REG_NUM
};

 

struct lm90_data {
	struct i2c_client *client;
	struct device *hwmon_dev;
	u32 chip_config[2];
	u32 channel_config[MAX_CHANNELS + 1];
	const char *channel_label[MAX_CHANNELS];
	struct hwmon_channel_info chip_info;
	struct hwmon_channel_info temp_info;
	const struct hwmon_channel_info *info[3];
	struct hwmon_chip_info chip;
	struct mutex update_lock;
	struct delayed_work alert_work;
	struct work_struct report_work;
	bool valid;		 
	bool alarms_valid;	 
	unsigned long last_updated;  
	unsigned long alarms_updated;  
	int kind;
	u32 flags;

	unsigned int update_interval;  

	u8 config;		 
	u8 config_orig;		 
	u8 convrate_orig;	 
	u8 resolution;		 
	u16 alert_alarms;	 
				 
	u8 max_convrate;	 
	u8 reg_status2;		 
	u8 reg_local_ext;	 
	u8 reg_remote_ext;	 
	u8 faultqueue_mask;	 
	u8 faultqueue_depth;	 

	 
	u16 temp[TEMP_REG_NUM];
	u8 temp_hyst;
	u8 conalert;
	u16 reported_alarms;	 
	u16 current_alarms;	 
	u16 alarms;		 
};

 

 
static inline s32 lm90_write_no_pec(struct i2c_client *client, u8 value)
{
	return i2c_smbus_xfer(client->adapter, client->addr,
			      client->flags & ~I2C_CLIENT_PEC,
			      I2C_SMBUS_WRITE, value, I2C_SMBUS_BYTE, NULL);
}

 
static int lm90_read_reg(struct i2c_client *client, u8 reg)
{
	struct lm90_data *data = i2c_get_clientdata(client);
	bool partial_pec = (client->flags & I2C_CLIENT_PEC) &&
			(data->flags & LM90_HAVE_PARTIAL_PEC);
	int err;

	if (partial_pec) {
		err = lm90_write_no_pec(client, reg);
		if (err)
			return err;
		return i2c_smbus_read_byte(client);
	}
	return i2c_smbus_read_byte_data(client, reg);
}

 
static u8 lm90_write_reg_addr(u8 reg)
{
	if (reg >= LM90_REG_CONFIG1 && reg <= LM90_REG_REMOTE_LOWH)
		return reg + 6;
	return reg;
}

 
static int lm90_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	return i2c_smbus_write_byte_data(client, lm90_write_reg_addr(reg), val);
}

 
static int lm90_write16(struct i2c_client *client, u8 regh, u8 regl, u16 val)
{
	int ret;

	ret = lm90_write_reg(client, regh, val >> 8);
	if (ret < 0 || !regl)
		return ret;
	return lm90_write_reg(client, regl, val & 0xff);
}

static int lm90_read16(struct i2c_client *client, u8 regh, u8 regl,
		       bool is_volatile)
{
	int oldh, newh, l;

	oldh = lm90_read_reg(client, regh);
	if (oldh < 0)
		return oldh;

	if (!regl)
		return oldh << 8;

	l = lm90_read_reg(client, regl);
	if (l < 0)
		return l;

	if (!is_volatile)
		return (oldh << 8) | l;

	 
	newh = lm90_read_reg(client, regh);
	if (newh < 0)
		return newh;
	if (oldh != newh) {
		l = lm90_read_reg(client, regl);
		if (l < 0)
			return l;
	}
	return (newh << 8) | l;
}

static int lm90_update_confreg(struct lm90_data *data, u8 config)
{
	if (data->config != config) {
		int err;

		err = lm90_write_reg(data->client, LM90_REG_CONFIG1, config);
		if (err)
			return err;
		data->config = config;
	}
	return 0;
}

 
static int lm90_select_remote_channel(struct lm90_data *data, bool second)
{
	u8 config = data->config & ~0x08;

	if (second)
		config |= 0x08;

	return lm90_update_confreg(data, config);
}

static int lm90_write_convrate(struct lm90_data *data, int val)
{
	u8 config = data->config;
	int err;

	 
	if (data->flags & LM90_PAUSE_FOR_CONFIG) {
		err = lm90_update_confreg(data, config | 0x40);
		if (err < 0)
			return err;
	}

	 
	err = lm90_write_reg(data->client, LM90_REG_CONVRATE, val);

	 
	lm90_update_confreg(data, config);

	return err;
}

 
static int lm90_set_convrate(struct i2c_client *client, struct lm90_data *data,
			     unsigned int interval)
{
	unsigned int update_interval;
	int i, err;

	 
	interval <<= 6;

	 
	for (i = 0, update_interval = LM90_MAX_CONVRATE_MS << 6;
	     i < data->max_convrate; i++, update_interval >>= 1)
		if (interval >= update_interval * 3 / 4)
			break;

	err = lm90_write_convrate(data, i);
	data->update_interval = DIV_ROUND_CLOSEST(update_interval, 64);
	return err;
}

static int lm90_set_faultqueue(struct i2c_client *client,
			       struct lm90_data *data, int val)
{
	int err;

	if (data->faultqueue_mask) {
		err = lm90_update_confreg(data, val <= data->faultqueue_depth / 2 ?
					  data->config & ~data->faultqueue_mask :
					  data->config | data->faultqueue_mask);
	} else {
		static const u8 values[4] = {0, 2, 6, 0x0e};

		data->conalert = (data->conalert & 0xf1) | values[val - 1];
		err = lm90_write_reg(data->client, TMP451_REG_CONALERT,
				     data->conalert);
	}

	return err;
}

static int lm90_update_limits(struct device *dev)
{
	struct lm90_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int val;

	if (data->flags & LM90_HAVE_CRIT) {
		val = lm90_read_reg(client, LM90_REG_LOCAL_CRIT);
		if (val < 0)
			return val;
		data->temp[LOCAL_CRIT] = val << 8;

		val = lm90_read_reg(client, LM90_REG_REMOTE_CRIT);
		if (val < 0)
			return val;
		data->temp[REMOTE_CRIT] = val << 8;

		val = lm90_read_reg(client, LM90_REG_TCRIT_HYST);
		if (val < 0)
			return val;
		data->temp_hyst = val;
	}
	if ((data->flags & LM90_HAVE_FAULTQUEUE) && !data->faultqueue_mask) {
		val = lm90_read_reg(client, TMP451_REG_CONALERT);
		if (val < 0)
			return val;
		data->conalert = val;
	}

	val = lm90_read16(client, LM90_REG_REMOTE_LOWH,
			  (data->flags & LM90_HAVE_REM_LIMIT_EXT) ? LM90_REG_REMOTE_LOWL : 0,
			  false);
	if (val < 0)
		return val;
	data->temp[REMOTE_LOW] = val;

	val = lm90_read16(client, LM90_REG_REMOTE_HIGHH,
			  (data->flags & LM90_HAVE_REM_LIMIT_EXT) ? LM90_REG_REMOTE_HIGHL : 0,
			  false);
	if (val < 0)
		return val;
	data->temp[REMOTE_HIGH] = val;

	if (data->flags & LM90_HAVE_OFFSET) {
		val = lm90_read16(client, LM90_REG_REMOTE_OFFSH,
				  LM90_REG_REMOTE_OFFSL, false);
		if (val < 0)
			return val;
		data->temp[REMOTE_OFFSET] = val;
	}

	if (data->flags & LM90_HAVE_EMERGENCY) {
		val = lm90_read_reg(client, MAX6659_REG_LOCAL_EMERG);
		if (val < 0)
			return val;
		data->temp[LOCAL_EMERG] = val << 8;

		val = lm90_read_reg(client, MAX6659_REG_REMOTE_EMERG);
		if (val < 0)
			return val;
		data->temp[REMOTE_EMERG] = val << 8;
	}

	if (data->flags & LM90_HAVE_TEMP3) {
		val = lm90_select_remote_channel(data, true);
		if (val < 0)
			return val;

		val = lm90_read_reg(client, LM90_REG_REMOTE_CRIT);
		if (val < 0)
			return val;
		data->temp[REMOTE2_CRIT] = val << 8;

		if (data->flags & LM90_HAVE_EMERGENCY) {
			val = lm90_read_reg(client, MAX6659_REG_REMOTE_EMERG);
			if (val < 0)
				return val;
			data->temp[REMOTE2_EMERG] = val << 8;
		}

		val = lm90_read_reg(client, LM90_REG_REMOTE_LOWH);
		if (val < 0)
			return val;
		data->temp[REMOTE2_LOW] = val << 8;

		val = lm90_read_reg(client, LM90_REG_REMOTE_HIGHH);
		if (val < 0)
			return val;
		data->temp[REMOTE2_HIGH] = val << 8;

		if (data->flags & LM90_HAVE_OFFSET) {
			val = lm90_read16(client, LM90_REG_REMOTE_OFFSH,
					  LM90_REG_REMOTE_OFFSL, false);
			if (val < 0)
				return val;
			data->temp[REMOTE2_OFFSET] = val;
		}

		lm90_select_remote_channel(data, false);
	}

	return 0;
}

static void lm90_report_alarms(struct work_struct *work)
{
	struct lm90_data *data = container_of(work, struct lm90_data, report_work);
	u16 cleared_alarms, new_alarms, current_alarms;
	struct device *hwmon_dev = data->hwmon_dev;
	struct device *dev = &data->client->dev;
	int st, st2;

	current_alarms = data->current_alarms;
	cleared_alarms = data->reported_alarms & ~current_alarms;
	new_alarms = current_alarms & ~data->reported_alarms;

	if (!cleared_alarms && !new_alarms)
		return;

	st = new_alarms & 0xff;
	st2 = new_alarms >> 8;

	if ((st & (LM90_STATUS_LLOW | LM90_STATUS_LHIGH | LM90_STATUS_LTHRM)) ||
	    (st2 & MAX6696_STATUS2_LOT2))
		dev_dbg(dev, "temp%d out of range, please check!\n", 1);
	if ((st & (LM90_STATUS_RLOW | LM90_STATUS_RHIGH | LM90_STATUS_RTHRM)) ||
	    (st2 & MAX6696_STATUS2_ROT2))
		dev_dbg(dev, "temp%d out of range, please check!\n", 2);
	if (st & LM90_STATUS_ROPEN)
		dev_dbg(dev, "temp%d diode open, please check!\n", 2);
	if (st2 & (MAX6696_STATUS2_R2LOW | MAX6696_STATUS2_R2HIGH |
		   MAX6696_STATUS2_R2THRM | MAX6696_STATUS2_R2OT2))
		dev_dbg(dev, "temp%d out of range, please check!\n", 3);
	if (st2 & MAX6696_STATUS2_R2OPEN)
		dev_dbg(dev, "temp%d diode open, please check!\n", 3);

	st |= cleared_alarms & 0xff;
	st2 |= cleared_alarms >> 8;

	if (st & LM90_STATUS_LLOW)
		hwmon_notify_event(hwmon_dev, hwmon_temp, hwmon_temp_min_alarm, 0);
	if (st & LM90_STATUS_RLOW)
		hwmon_notify_event(hwmon_dev, hwmon_temp, hwmon_temp_min_alarm, 1);
	if (st2 & MAX6696_STATUS2_R2LOW)
		hwmon_notify_event(hwmon_dev, hwmon_temp, hwmon_temp_min_alarm, 2);

	if (st & LM90_STATUS_LHIGH)
		hwmon_notify_event(hwmon_dev, hwmon_temp, hwmon_temp_max_alarm, 0);
	if (st & LM90_STATUS_RHIGH)
		hwmon_notify_event(hwmon_dev, hwmon_temp, hwmon_temp_max_alarm, 1);
	if (st2 & MAX6696_STATUS2_R2HIGH)
		hwmon_notify_event(hwmon_dev, hwmon_temp, hwmon_temp_max_alarm, 2);

	if (st & LM90_STATUS_LTHRM)
		hwmon_notify_event(hwmon_dev, hwmon_temp, hwmon_temp_crit_alarm, 0);
	if (st & LM90_STATUS_RTHRM)
		hwmon_notify_event(hwmon_dev, hwmon_temp, hwmon_temp_crit_alarm, 1);
	if (st2 & MAX6696_STATUS2_R2THRM)
		hwmon_notify_event(hwmon_dev, hwmon_temp, hwmon_temp_crit_alarm, 2);

	if (st2 & MAX6696_STATUS2_LOT2)
		hwmon_notify_event(hwmon_dev, hwmon_temp, hwmon_temp_emergency_alarm, 0);
	if (st2 & MAX6696_STATUS2_ROT2)
		hwmon_notify_event(hwmon_dev, hwmon_temp, hwmon_temp_emergency_alarm, 1);
	if (st2 & MAX6696_STATUS2_R2OT2)
		hwmon_notify_event(hwmon_dev, hwmon_temp, hwmon_temp_emergency_alarm, 2);

	data->reported_alarms = current_alarms;
}

static int lm90_update_alarms_locked(struct lm90_data *data, bool force)
{
	if (force || !data->alarms_valid ||
	    time_after(jiffies, data->alarms_updated + msecs_to_jiffies(data->update_interval))) {
		struct i2c_client *client = data->client;
		bool check_enable;
		u16 alarms;
		int val;

		data->alarms_valid = false;

		val = lm90_read_reg(client, LM90_REG_STATUS);
		if (val < 0)
			return val;
		alarms = val & ~LM90_STATUS_BUSY;

		if (data->reg_status2) {
			val = lm90_read_reg(client, data->reg_status2);
			if (val < 0)
				return val;
			alarms |= val << 8;
		}
		 
		if (force && data->alarms_valid)
			data->current_alarms |= alarms;
		else
			data->current_alarms = alarms;
		data->alarms |= alarms;

		check_enable = (client->irq || !(data->config_orig & 0x80)) &&
			(data->config & 0x80);

		if (force || check_enable)
			schedule_work(&data->report_work);

		 
		if (check_enable) {
			if (!(data->current_alarms & data->alert_alarms)) {
				dev_dbg(&client->dev, "Re-enabling ALERT#\n");
				lm90_update_confreg(data, data->config & ~0x80);
				 
				cancel_delayed_work(&data->alert_work);
			} else {
				schedule_delayed_work(&data->alert_work,
					max_t(int, HZ, msecs_to_jiffies(data->update_interval)));
			}
		}
		data->alarms_updated = jiffies;
		data->alarms_valid = true;
	}
	return 0;
}

static int lm90_update_alarms(struct lm90_data *data, bool force)
{
	int err;

	mutex_lock(&data->update_lock);
	err = lm90_update_alarms_locked(data, force);
	mutex_unlock(&data->update_lock);

	return err;
}

static void lm90_alert_work(struct work_struct *__work)
{
	struct delayed_work *delayed_work = container_of(__work, struct delayed_work, work);
	struct lm90_data *data = container_of(delayed_work, struct lm90_data, alert_work);

	 
	if (!(data->config & 0x80))
		return;

	lm90_update_alarms(data, true);
}

static int lm90_update_device(struct device *dev)
{
	struct lm90_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	unsigned long next_update;
	int val;

	if (!data->valid) {
		val = lm90_update_limits(dev);
		if (val < 0)
			return val;
	}

	next_update = data->last_updated +
		      msecs_to_jiffies(data->update_interval);
	if (time_after(jiffies, next_update) || !data->valid) {
		dev_dbg(&client->dev, "Updating lm90 data.\n");

		data->valid = false;

		val = lm90_read_reg(client, LM90_REG_LOCAL_LOW);
		if (val < 0)
			return val;
		data->temp[LOCAL_LOW] = val << 8;

		val = lm90_read_reg(client, LM90_REG_LOCAL_HIGH);
		if (val < 0)
			return val;
		data->temp[LOCAL_HIGH] = val << 8;

		val = lm90_read16(client, LM90_REG_LOCAL_TEMP,
				  data->reg_local_ext, true);
		if (val < 0)
			return val;
		data->temp[LOCAL_TEMP] = val;
		val = lm90_read16(client, LM90_REG_REMOTE_TEMPH,
				  data->reg_remote_ext, true);
		if (val < 0)
			return val;
		data->temp[REMOTE_TEMP] = val;

		if (data->flags & LM90_HAVE_TEMP3) {
			val = lm90_select_remote_channel(data, true);
			if (val < 0)
				return val;

			val = lm90_read16(client, LM90_REG_REMOTE_TEMPH,
					  data->reg_remote_ext, true);
			if (val < 0) {
				lm90_select_remote_channel(data, false);
				return val;
			}
			data->temp[REMOTE2_TEMP] = val;

			lm90_select_remote_channel(data, false);
		}

		val = lm90_update_alarms_locked(data, false);
		if (val < 0)
			return val;

		data->last_updated = jiffies;
		data->valid = true;
	}

	return 0;
}

 
static ssize_t pec_show(struct device *dev, struct device_attribute *dummy,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);

	return sprintf(buf, "%d\n", !!(client->flags & I2C_CLIENT_PEC));
}

static ssize_t pec_store(struct device *dev, struct device_attribute *dummy,
			 const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err < 0)
		return err;

	switch (val) {
	case 0:
		client->flags &= ~I2C_CLIENT_PEC;
		break;
	case 1:
		client->flags |= I2C_CLIENT_PEC;
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR_RW(pec);

static int lm90_temp_get_resolution(struct lm90_data *data, int index)
{
	switch (index) {
	case REMOTE_TEMP:
		if (data->reg_remote_ext)
			return data->resolution;
		return 8;
	case REMOTE_OFFSET:
	case REMOTE2_OFFSET:
	case REMOTE2_TEMP:
		return data->resolution;
	case LOCAL_TEMP:
		if (data->reg_local_ext)
			return data->resolution;
		return 8;
	case REMOTE_LOW:
	case REMOTE_HIGH:
	case REMOTE2_LOW:
	case REMOTE2_HIGH:
		if (data->flags & LM90_HAVE_REM_LIMIT_EXT)
			return data->resolution;
		return 8;
	default:
		return 8;
	}
}

static int lm90_temp_from_reg(u32 flags, u16 regval, u8 resolution)
{
	int val;

	if (flags & LM90_HAVE_EXTENDED_TEMP)
		val = regval - 0x4000;
	else if (flags & (LM90_HAVE_UNSIGNED_TEMP | LM90_HAVE_EXT_UNSIGNED))
		val = regval;
	else
		val = (s16)regval;

	return ((val >> (16 - resolution)) * 1000) >> (resolution - 8);
}

static int lm90_get_temp(struct lm90_data *data, int index, int channel)
{
	int temp = lm90_temp_from_reg(data->flags, data->temp[index],
				      lm90_temp_get_resolution(data, index));

	 
	if (data->kind == lm99 && channel)
		temp += 16000;

	return temp;
}

static u16 lm90_temp_to_reg(u32 flags, long val, u8 resolution)
{
	int fraction = resolution > 8 ?
			1000 - DIV_ROUND_CLOSEST(1000, BIT(resolution - 8)) : 0;

	if (flags & LM90_HAVE_EXTENDED_TEMP) {
		val = clamp_val(val, -64000, 191000 + fraction);
		val += 64000;
	} else if (flags & LM90_HAVE_EXT_UNSIGNED) {
		val = clamp_val(val, 0, 255000 + fraction);
	} else if (flags & LM90_HAVE_UNSIGNED_TEMP) {
		val = clamp_val(val, 0, 127000 + fraction);
	} else {
		val = clamp_val(val, -128000, 127000 + fraction);
	}

	return DIV_ROUND_CLOSEST(val << (resolution - 8), 1000) << (16 - resolution);
}

static int lm90_set_temp(struct lm90_data *data, int index, int channel, long val)
{
	static const u8 regs[] = {
		[LOCAL_LOW] = LM90_REG_LOCAL_LOW,
		[LOCAL_HIGH] = LM90_REG_LOCAL_HIGH,
		[LOCAL_CRIT] = LM90_REG_LOCAL_CRIT,
		[REMOTE_CRIT] = LM90_REG_REMOTE_CRIT,
		[LOCAL_EMERG] = MAX6659_REG_LOCAL_EMERG,
		[REMOTE_EMERG] = MAX6659_REG_REMOTE_EMERG,
		[REMOTE2_CRIT] = LM90_REG_REMOTE_CRIT,
		[REMOTE2_EMERG] = MAX6659_REG_REMOTE_EMERG,
		[REMOTE_LOW] = LM90_REG_REMOTE_LOWH,
		[REMOTE_HIGH] = LM90_REG_REMOTE_HIGHH,
		[REMOTE2_LOW] = LM90_REG_REMOTE_LOWH,
		[REMOTE2_HIGH] = LM90_REG_REMOTE_HIGHH,
	};
	struct i2c_client *client = data->client;
	u8 regh = regs[index];
	u8 regl = 0;
	int err;

	if (channel && (data->flags & LM90_HAVE_REM_LIMIT_EXT)) {
		if (index == REMOTE_LOW || index == REMOTE2_LOW)
			regl = LM90_REG_REMOTE_LOWL;
		else if (index == REMOTE_HIGH || index == REMOTE2_HIGH)
			regl = LM90_REG_REMOTE_HIGHL;
	}

	 
	if (data->kind == lm99 && channel) {
		 
		val = max(val, -128000l);
		val -= 16000;
	}

	data->temp[index] = lm90_temp_to_reg(data->flags, val,
					     lm90_temp_get_resolution(data, index));

	if (channel > 1)
		lm90_select_remote_channel(data, true);

	err = lm90_write16(client, regh, regl, data->temp[index]);

	if (channel > 1)
		lm90_select_remote_channel(data, false);

	return err;
}

static int lm90_get_temphyst(struct lm90_data *data, int index, int channel)
{
	int temp = lm90_get_temp(data, index, channel);

	return temp - data->temp_hyst * 1000;
}

static int lm90_set_temphyst(struct lm90_data *data, long val)
{
	int temp = lm90_get_temp(data, LOCAL_CRIT, 0);

	 
	val = clamp_val(val, -128000l, 255000l);
	data->temp_hyst = clamp_val(DIV_ROUND_CLOSEST(temp - val, 1000), 0, 31);

	return lm90_write_reg(data->client, LM90_REG_TCRIT_HYST, data->temp_hyst);
}

static int lm90_get_temp_offset(struct lm90_data *data, int index)
{
	int res = lm90_temp_get_resolution(data, index);

	return lm90_temp_from_reg(0, data->temp[index], res);
}

static int lm90_set_temp_offset(struct lm90_data *data, int index, int channel, long val)
{
	int err;

	val = lm90_temp_to_reg(0, val, lm90_temp_get_resolution(data, index));

	 
	if (channel > 1)
		lm90_select_remote_channel(data, true);

	err = lm90_write16(data->client, LM90_REG_REMOTE_OFFSH, LM90_REG_REMOTE_OFFSL, val);

	if (channel > 1)
		lm90_select_remote_channel(data, false);

	if (err)
		return err;

	data->temp[index] = val;

	return 0;
}

static const u8 lm90_temp_index[MAX_CHANNELS] = {
	LOCAL_TEMP, REMOTE_TEMP, REMOTE2_TEMP
};

static const u8 lm90_temp_min_index[MAX_CHANNELS] = {
	LOCAL_LOW, REMOTE_LOW, REMOTE2_LOW
};

static const u8 lm90_temp_max_index[MAX_CHANNELS] = {
	LOCAL_HIGH, REMOTE_HIGH, REMOTE2_HIGH
};

static const u8 lm90_temp_crit_index[MAX_CHANNELS] = {
	LOCAL_CRIT, REMOTE_CRIT, REMOTE2_CRIT
};

static const u8 lm90_temp_emerg_index[MAX_CHANNELS] = {
	LOCAL_EMERG, REMOTE_EMERG, REMOTE2_EMERG
};

static const s8 lm90_temp_offset_index[MAX_CHANNELS] = {
	-1, REMOTE_OFFSET, REMOTE2_OFFSET
};

static const u16 lm90_min_alarm_bits[MAX_CHANNELS] = { BIT(5), BIT(3), BIT(11) };
static const u16 lm90_max_alarm_bits[MAX_CHANNELS] = { BIT(6), BIT(4), BIT(12) };
static const u16 lm90_crit_alarm_bits[MAX_CHANNELS] = { BIT(0), BIT(1), BIT(9) };
static const u16 lm90_crit_alarm_bits_swapped[MAX_CHANNELS] = { BIT(1), BIT(0), BIT(9) };
static const u16 lm90_emergency_alarm_bits[MAX_CHANNELS] = { BIT(15), BIT(13), BIT(14) };
static const u16 lm90_fault_bits[MAX_CHANNELS] = { BIT(0), BIT(2), BIT(10) };

static int lm90_temp_read(struct device *dev, u32 attr, int channel, long *val)
{
	struct lm90_data *data = dev_get_drvdata(dev);
	int err;
	u16 bit;

	mutex_lock(&data->update_lock);
	err = lm90_update_device(dev);
	mutex_unlock(&data->update_lock);
	if (err)
		return err;

	switch (attr) {
	case hwmon_temp_input:
		*val = lm90_get_temp(data, lm90_temp_index[channel], channel);
		break;
	case hwmon_temp_min_alarm:
	case hwmon_temp_max_alarm:
	case hwmon_temp_crit_alarm:
	case hwmon_temp_emergency_alarm:
	case hwmon_temp_fault:
		switch (attr) {
		case hwmon_temp_min_alarm:
			bit = lm90_min_alarm_bits[channel];
			break;
		case hwmon_temp_max_alarm:
			bit = lm90_max_alarm_bits[channel];
			break;
		case hwmon_temp_crit_alarm:
			if (data->flags & LM90_HAVE_CRIT_ALRM_SWP)
				bit = lm90_crit_alarm_bits_swapped[channel];
			else
				bit = lm90_crit_alarm_bits[channel];
			break;
		case hwmon_temp_emergency_alarm:
			bit = lm90_emergency_alarm_bits[channel];
			break;
		case hwmon_temp_fault:
			bit = lm90_fault_bits[channel];
			break;
		}
		*val = !!(data->alarms & bit);
		data->alarms &= ~bit;
		data->alarms |= data->current_alarms;
		break;
	case hwmon_temp_min:
		*val = lm90_get_temp(data, lm90_temp_min_index[channel], channel);
		break;
	case hwmon_temp_max:
		*val = lm90_get_temp(data, lm90_temp_max_index[channel], channel);
		break;
	case hwmon_temp_crit:
		*val = lm90_get_temp(data, lm90_temp_crit_index[channel], channel);
		break;
	case hwmon_temp_crit_hyst:
		*val = lm90_get_temphyst(data, lm90_temp_crit_index[channel], channel);
		break;
	case hwmon_temp_emergency:
		*val = lm90_get_temp(data, lm90_temp_emerg_index[channel], channel);
		break;
	case hwmon_temp_emergency_hyst:
		*val = lm90_get_temphyst(data, lm90_temp_emerg_index[channel], channel);
		break;
	case hwmon_temp_offset:
		*val = lm90_get_temp_offset(data, lm90_temp_offset_index[channel]);
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

static int lm90_temp_write(struct device *dev, u32 attr, int channel, long val)
{
	struct lm90_data *data = dev_get_drvdata(dev);
	int err;

	mutex_lock(&data->update_lock);

	err = lm90_update_device(dev);
	if (err)
		goto error;

	switch (attr) {
	case hwmon_temp_min:
		err = lm90_set_temp(data, lm90_temp_min_index[channel],
				    channel, val);
		break;
	case hwmon_temp_max:
		err = lm90_set_temp(data, lm90_temp_max_index[channel],
				    channel, val);
		break;
	case hwmon_temp_crit:
		err = lm90_set_temp(data, lm90_temp_crit_index[channel],
				    channel, val);
		break;
	case hwmon_temp_crit_hyst:
		err = lm90_set_temphyst(data, val);
		break;
	case hwmon_temp_emergency:
		err = lm90_set_temp(data, lm90_temp_emerg_index[channel],
				    channel, val);
		break;
	case hwmon_temp_offset:
		err = lm90_set_temp_offset(data, lm90_temp_offset_index[channel],
					   channel, val);
		break;
	default:
		err = -EOPNOTSUPP;
		break;
	}
error:
	mutex_unlock(&data->update_lock);

	return err;
}

static umode_t lm90_temp_is_visible(const void *data, u32 attr, int channel)
{
	switch (attr) {
	case hwmon_temp_input:
	case hwmon_temp_min_alarm:
	case hwmon_temp_max_alarm:
	case hwmon_temp_crit_alarm:
	case hwmon_temp_emergency_alarm:
	case hwmon_temp_emergency_hyst:
	case hwmon_temp_fault:
	case hwmon_temp_label:
		return 0444;
	case hwmon_temp_min:
	case hwmon_temp_max:
	case hwmon_temp_crit:
	case hwmon_temp_emergency:
	case hwmon_temp_offset:
		return 0644;
	case hwmon_temp_crit_hyst:
		if (channel == 0)
			return 0644;
		return 0444;
	default:
		return 0;
	}
}

static int lm90_chip_read(struct device *dev, u32 attr, int channel, long *val)
{
	struct lm90_data *data = dev_get_drvdata(dev);
	int err;

	mutex_lock(&data->update_lock);
	err = lm90_update_device(dev);
	mutex_unlock(&data->update_lock);
	if (err)
		return err;

	switch (attr) {
	case hwmon_chip_update_interval:
		*val = data->update_interval;
		break;
	case hwmon_chip_alarms:
		*val = data->alarms;
		break;
	case hwmon_chip_temp_samples:
		if (data->faultqueue_mask) {
			*val = (data->config & data->faultqueue_mask) ?
				data->faultqueue_depth : 1;
		} else {
			switch (data->conalert & 0x0e) {
			case 0x0:
			default:
				*val = 1;
				break;
			case 0x2:
				*val = 2;
				break;
			case 0x6:
				*val = 3;
				break;
			case 0xe:
				*val = 4;
				break;
			}
		}
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int lm90_chip_write(struct device *dev, u32 attr, int channel, long val)
{
	struct lm90_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int err;

	mutex_lock(&data->update_lock);

	err = lm90_update_device(dev);
	if (err)
		goto error;

	switch (attr) {
	case hwmon_chip_update_interval:
		err = lm90_set_convrate(client, data,
					clamp_val(val, 0, 100000));
		break;
	case hwmon_chip_temp_samples:
		err = lm90_set_faultqueue(client, data, clamp_val(val, 1, 4));
		break;
	default:
		err = -EOPNOTSUPP;
		break;
	}
error:
	mutex_unlock(&data->update_lock);

	return err;
}

static umode_t lm90_chip_is_visible(const void *data, u32 attr, int channel)
{
	switch (attr) {
	case hwmon_chip_update_interval:
	case hwmon_chip_temp_samples:
		return 0644;
	case hwmon_chip_alarms:
		return 0444;
	default:
		return 0;
	}
}

static int lm90_read(struct device *dev, enum hwmon_sensor_types type,
		     u32 attr, int channel, long *val)
{
	switch (type) {
	case hwmon_chip:
		return lm90_chip_read(dev, attr, channel, val);
	case hwmon_temp:
		return lm90_temp_read(dev, attr, channel, val);
	default:
		return -EOPNOTSUPP;
	}
}

static int lm90_read_string(struct device *dev, enum hwmon_sensor_types type,
			    u32 attr, int channel, const char **str)
{
	struct lm90_data *data = dev_get_drvdata(dev);

	*str = data->channel_label[channel];

	return 0;
}

static int lm90_write(struct device *dev, enum hwmon_sensor_types type,
		      u32 attr, int channel, long val)
{
	switch (type) {
	case hwmon_chip:
		return lm90_chip_write(dev, attr, channel, val);
	case hwmon_temp:
		return lm90_temp_write(dev, attr, channel, val);
	default:
		return -EOPNOTSUPP;
	}
}

static umode_t lm90_is_visible(const void *data, enum hwmon_sensor_types type,
			       u32 attr, int channel)
{
	switch (type) {
	case hwmon_chip:
		return lm90_chip_is_visible(data, attr, channel);
	case hwmon_temp:
		return lm90_temp_is_visible(data, attr, channel);
	default:
		return 0;
	}
}

static const char *lm90_detect_lm84(struct i2c_client *client)
{
	static const u8 regs[] = {
		LM90_REG_STATUS, LM90_REG_LOCAL_TEMP, LM90_REG_LOCAL_HIGH,
		LM90_REG_REMOTE_TEMPH, LM90_REG_REMOTE_HIGHH
	};
	int status = i2c_smbus_read_byte_data(client, LM90_REG_STATUS);
	int reg1, reg2, reg3, reg4;
	bool nonzero = false;
	u8 ff = 0xff;
	int i;

	if (status < 0 || (status & 0xab))
		return NULL;

	 
	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		reg1 = i2c_smbus_read_byte_data(client, regs[i]);
		reg2 = i2c_smbus_read_byte_data(client, LM90_REG_REMOTE_TEMPL);
		reg3 = i2c_smbus_read_byte_data(client, LM90_REG_LOCAL_LOW);
		reg4 = i2c_smbus_read_byte_data(client, LM90_REG_REMOTE_LOWH);

		if (reg1 < 0)
			return NULL;

		 
		if (reg2 != reg1 || reg3 != reg1 || reg4 != reg1)
			return NULL;

		nonzero |= reg1 || reg2 || reg3 || reg4;
		ff &= reg1;
	}
	 
	return nonzero && ff != 0xff ? "lm84" : NULL;
}

static const char *lm90_detect_max1617(struct i2c_client *client, int config1)
{
	int status = i2c_smbus_read_byte_data(client, LM90_REG_STATUS);
	int llo, rlo, lhi, rhi;

	if (status < 0 || (status & 0x03))
		return NULL;

	if (config1 & 0x3f)
		return NULL;

	 
	if (i2c_smbus_read_byte_data(client, LM90_REG_REMOTE_TEMPL) != 0xff ||
	    i2c_smbus_read_byte_data(client, MAX6657_REG_LOCAL_TEMPL) != 0xff ||
	    i2c_smbus_read_byte_data(client, LM90_REG_REMOTE_LOWL) != 0xff ||
	    i2c_smbus_read_byte(client) != 0xff)
		return NULL;

	llo = i2c_smbus_read_byte_data(client, LM90_REG_LOCAL_LOW);
	rlo = i2c_smbus_read_byte_data(client, LM90_REG_REMOTE_LOWH);

	lhi = i2c_smbus_read_byte_data(client, LM90_REG_LOCAL_HIGH);
	rhi = i2c_smbus_read_byte_data(client, LM90_REG_REMOTE_HIGHH);

	if (llo < 0 || rlo < 0)
		return NULL;

	 
	if (i2c_smbus_read_byte(client) != rhi)
		return NULL;

	 

	 
	if ((s8)lhi < 0 || (s8)rhi < 0)
		return NULL;

	 
	if ((s8)llo >= lhi || (s8)rlo >= rhi)
		return NULL;

	if (i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		 
		if (i2c_smbus_read_word_data(client, LM90_REG_REMOTE_TEMPL) !=
						0xffff)
			return NULL;
		if (i2c_smbus_read_word_data(client, LM90_REG_CONFIG1) !=
						(config1 | 0xff00))
			return NULL;
		if (i2c_smbus_read_word_data(client, LM90_REG_LOCAL_HIGH) !=
						(lhi | 0xff00))
			return NULL;
	}

	return "max1617";
}

static const char *lm90_detect_national(struct i2c_client *client, int chip_id,
					int config1, int convrate)
{
	int config2 = i2c_smbus_read_byte_data(client, LM90_REG_CONFIG2);
	int address = client->addr;
	const char *name = NULL;

	if (config2 < 0)
		return NULL;

	if ((config1 & 0x2a) || (config2 & 0xf8) || convrate > 0x09)
		return NULL;

	if (address != 0x4c && address != 0x4d)
		return NULL;

	switch (chip_id & 0xf0) {
	case 0x10:	 
		if (address == 0x4c)
			name = "lm86";
		break;
	case 0x20:	 
		if (address == 0x4c)
			name = "lm90";
		break;
	case 0x30:	 
		name = "lm99";	 
		break;
	default:
		break;
	}

	return name;
}

static const char *lm90_detect_on(struct i2c_client *client, int chip_id, int config1,
				  int convrate)
{
	int address = client->addr;
	const char *name = NULL;

	switch (chip_id) {
	case 0xca:		 
		if ((address == 0x4c || address == 0x4d) && !(config1 & 0x1b) &&
		    convrate <= 0x0a)
			name = "nct218";
		break;
	default:
		break;
	}
	return name;
}

static const char *lm90_detect_analog(struct i2c_client *client, bool common_address,
				      int chip_id, int config1, int convrate)
{
	int status = i2c_smbus_read_byte_data(client, LM90_REG_STATUS);
	int config2 = i2c_smbus_read_byte_data(client, ADT7481_REG_CONFIG2);
	int man_id2 = i2c_smbus_read_byte_data(client, ADT7481_REG_MAN_ID);
	int chip_id2 = i2c_smbus_read_byte_data(client, ADT7481_REG_CHIP_ID);
	int address = client->addr;
	const char *name = NULL;

	if (status < 0 || config2 < 0 || man_id2 < 0 || chip_id2 < 0)
		return NULL;

	 
	switch (chip_id) {
	case 0x00 ... 0x03:	 
	case 0x05 ... 0x0f:
		if (man_id2 == 0x00 && chip_id2 == 0x00 && common_address &&
		    !(status & 0x03) && !(config1 & 0x3f) && !(convrate & 0xf8))
			name = "adm1021";
		break;
	case 0x04:		 
		if (man_id2 == 0x41 && chip_id2 == 0x21 &&
		    (address == 0x4c || address == 0x4d) &&
		    (config1 & 0x0b) == 0x08 && convrate <= 0x0a)
			name = "adt7421";
		break;
	case 0x30 ... 0x38:	 
	case 0x3a ... 0x3e:
		 
		if (man_id2 == 0x00 && chip_id2 == 0x00 && common_address &&
		    !(status & 0x03) && !(config1 & 0x3f) && !(convrate & 0xf8))
			name = "adm1023";
		break;
	case 0x39:		 
		if (man_id2 == 0x00 && chip_id2 == 0x00 &&
		    (address == 0x4c || address == 0x4d || address == 0x4e) &&
		    !(status & 0x03) && !(config1 & 0x3f) && !(convrate & 0xf8))
			name = "adm1020";
		break;
	case 0x3f:		 
		if (man_id2 == 0x00 && chip_id2 == 0x00 && common_address &&
		    !(status & 0x03) && !(config1 & 0x3f) && !(convrate & 0xf8))
			name = "nct210";
		break;
	case 0x40 ... 0x4f:	 
		if (man_id2 == 0x00 && chip_id2 == 0x00 &&
		    (address == 0x4c || address == 0x4d) && !(config1 & 0x3f) &&
		    convrate <= 0x0a)
			name = "adm1032";
		break;
	case 0x51:	 
		if (man_id2 == 0x00 && chip_id2 == 0x00 &&
		    (address == 0x4c || address == 0x4d) && !(config1 & 0x1b) &&
		    convrate <= 0x0a)
			name = "adt7461";
		break;
	case 0x54:	 
		if (man_id2 == 0x41 && chip_id2 == 0x61 &&
		    (address == 0x4c || address == 0x4d) && !(config1 & 0x1b) &&
		    convrate <= 0x0a)
			name = "nct1008";
		break;
	case 0x55:	 
		if (man_id2 == 0x41 && chip_id2 == 0x61 &&
		    (address == 0x4c || address == 0x4d) && !(config1 & 0x1b) &&
		    convrate <= 0x0a)
			name = "nct72";
		break;
	case 0x57:	 
		if (man_id2 == 0x41 && chip_id2 == 0x61 &&
		    (address == 0x4c || address == 0x4d) && !(config1 & 0x1b) &&
		    convrate <= 0x0a)
			name = "adt7461a";
		break;
	case 0x5a:	 
		if (man_id2 == 0x41 && chip_id2 == 0x61 &&
		    common_address && !(config1 & 0x1b) && convrate <= 0x0a)
			name = "nct214";
		break;
	case 0x62:	 
		if (man_id2 == 0x41 && chip_id2 == 0x81 &&
		    (address == 0x4b || address == 0x4c) && !(config1 & 0x10) &&
		    !(config2 & 0x7f) && (convrate & 0x0f) <= 0x0b) {
			name = "adt7481";
		}
		break;
	case 0x65:	 
	case 0x75:	 
		if (man_id2 == 0x41 && chip_id2 == 0x82 &&
		    address == 0x4c && !(config1 & 0x10) && !(config2 & 0x7f) &&
		    convrate <= 0x0a)
			name = "adt7482";
		break;
	case 0x94:	 
		if (man_id2 == 0x41 && chip_id2 == 0x83 &&
		    common_address &&
		    ((address >= 0x18 && address <= 0x1a) ||
		     (address >= 0x29 && address <= 0x2b) ||
		     (address >= 0x4c && address <= 0x4e)) &&
		    !(config1 & 0x10) && !(config2 & 0x7f) && convrate <= 0x0a)
			name = "adt7483a";
		break;
	default:
		break;
	}

	return name;
}

static const char *lm90_detect_maxim(struct i2c_client *client, bool common_address,
				     int chip_id, int config1, int convrate)
{
	int man_id, emerg, emerg2, status2;
	int address = client->addr;
	const char *name = NULL;

	switch (chip_id) {
	case 0x01:
		if (!common_address)
			break;

		 
		emerg = i2c_smbus_read_byte_data(client,
						 MAX6659_REG_REMOTE_EMERG);
		man_id = i2c_smbus_read_byte_data(client,
						  LM90_REG_MAN_ID);
		emerg2 = i2c_smbus_read_byte_data(client,
						  MAX6659_REG_REMOTE_EMERG);
		status2 = i2c_smbus_read_byte_data(client,
						   MAX6696_REG_STATUS2);
		if (emerg < 0 || man_id < 0 || emerg2 < 0 || status2 < 0)
			return NULL;

		 
		if (!(config1 & 0x10) && !(status2 & 0x01) && emerg == emerg2 &&
		    convrate <= 0x07)
			name = "max6696";
		 
		else if (!(config1 & 0x03) && convrate <= 0x07 &&
			 emerg2 == man_id && emerg2 != status2)
			name = "max6680";
		 
		else if (!(config1 & 0x03f) && convrate <= 0x07 &&
			 emerg == 0x01 && emerg2 == 0x01 && status2 == 0x01)
			name = "max1617";
		break;
	case 0x08:
		 
		if (common_address && !(config1 & 0x07) && convrate <= 0x07)
			name = "max6654";
		break;
	case 0x09:
		 
		if (common_address && !(config1 & 0x07) && convrate <= 0x07)
			name = "max6690";
		break;
	case 0x4d:
		 
		if (address >= 0x48 && address <= 0x4f && config1 == convrate &&
		    !(config1 & 0x0f)) {
			int regval;

			 

			 
			if (i2c_smbus_read_byte_data(client, LM90_REG_MAN_ID) != 0x4d)
				break;

			 
			if (i2c_smbus_read_byte_data(client, LM90_REG_CONVRATE) != 0x4d ||
			    i2c_smbus_read_byte_data(client, LM90_REG_LOCAL_LOW) != 0x4d ||
			    i2c_smbus_read_byte_data(client, LM90_REG_REMOTE_LOWH) != 0x4d)
				break;

			 
			regval = i2c_smbus_read_byte_data(client, LM90_REG_STATUS);
			if (regval < 0 || (regval & 0x2b))
				break;

			 
			if (i2c_smbus_read_byte_data(client, LM90_REG_CONVRATE) != regval ||
			    i2c_smbus_read_byte_data(client, LM90_REG_LOCAL_LOW) != regval ||
			    i2c_smbus_read_byte_data(client, LM90_REG_REMOTE_LOWH) != regval)
				break;

			name = "max6642";
		} else if ((address == 0x4c || address == 0x4d || address == 0x4e) &&
			   (config1 & 0x1f) == 0x0d && convrate <= 0x09) {
			if (address == 0x4c)
				name = "max6657";
			else
				name = "max6659";
		}
		break;
	case 0x59:
		 
		if (!(config1 & 0x3f) && convrate <= 0x07) {
			int temp;

			switch (address) {
			case 0x4c:
				 
				temp = i2c_smbus_read_byte_data(client,
								LM90_REG_REMOTE_TEMPH);
				if (temp == 0x80)
					name = "max6648";
				else
					name = "max6649";
				break;
			case 0x4d:
				name = "max6646";
				break;
			case 0x4e:
				name = "max6647";
				break;
			default:
				break;
			}
		}
		break;
	default:
		break;
	}

	return name;
}

static const char *lm90_detect_nuvoton(struct i2c_client *client, int chip_id,
				       int config1, int convrate)
{
	int config2 = i2c_smbus_read_byte_data(client, LM90_REG_CONFIG2);
	int address = client->addr;
	const char *name = NULL;

	if (config2 < 0)
		return NULL;

	if (address == 0x4c && !(config1 & 0x2a) && !(config2 & 0xf8)) {
		if (chip_id == 0x01 && convrate <= 0x09) {
			 
			name = "w83l771";
		} else if ((chip_id & 0xfe) == 0x10 && convrate <= 0x08) {
			 
			name = "w83l771";
		}
	}
	return name;
}

static const char *lm90_detect_nxp(struct i2c_client *client, bool common_address,
				   int chip_id, int config1, int convrate)
{
	int address = client->addr;
	const char *name = NULL;
	int config2;

	switch (chip_id) {
	case 0x00:
		config2 = i2c_smbus_read_byte_data(client, LM90_REG_CONFIG2);
		if (config2 < 0)
			return NULL;
		if (address >= 0x48 && address <= 0x4f &&
		    !(config1 & 0x2a) && !(config2 & 0xfe) && convrate <= 0x09)
			name = "sa56004";
		break;
	case 0x80:
		if (common_address && !(config1 & 0x3f) && convrate <= 0x07)
			name = "ne1618";
		break;
	default:
		break;
	}
	return name;
}

static const char *lm90_detect_gmt(struct i2c_client *client, int chip_id,
				   int config1, int convrate)
{
	int address = client->addr;

	 
	if ((chip_id == 0x01 || chip_id == 0x03) &&
	    (address == 0x4c || address == 0x4d) &&
	    !(config1 & 0x3f) && convrate <= 0x08) {
		int reg;

		reg = i2c_smbus_read_byte_data(client, LM90_REG_REMOTE_OFFSL);
		if (reg < 0 || reg & 0x1f)
			return NULL;
		reg = i2c_smbus_read_byte_data(client, TMP451_REG_CONALERT);
		if (reg < 0 || reg & 0xf1)
			return NULL;

		return "g781";
	}

	return NULL;
}

static const char *lm90_detect_ti49(struct i2c_client *client, bool common_address,
				    int chip_id, int config1, int convrate)
{
	if (common_address && chip_id == 0x00 && !(config1 & 0x3f) && !(convrate & 0xf8)) {
		 
		if (i2c_smbus_read_byte_data(client, LM90_REG_REMOTE_TEMPL) == 0xff &&
		    i2c_smbus_read_byte_data(client, LM90_REG_REMOTE_CRIT) == 0xff)
			return "thmc10";
	}
	return NULL;
}

static const char *lm90_detect_ti(struct i2c_client *client, int chip_id,
				  int config1, int convrate)
{
	int address = client->addr;
	const char *name = NULL;

	if (chip_id == 0x00 && !(config1 & 0x1b) && convrate <= 0x09) {
		int local_ext, conalert, chen, dfc;

		local_ext = i2c_smbus_read_byte_data(client,
						     TMP451_REG_LOCAL_TEMPL);
		conalert = i2c_smbus_read_byte_data(client,
						    TMP451_REG_CONALERT);
		chen = i2c_smbus_read_byte_data(client, TMP461_REG_CHEN);
		dfc = i2c_smbus_read_byte_data(client, TMP461_REG_DFC);

		if (!(local_ext & 0x0f) && (conalert & 0xf1) == 0x01 &&
		    (chen & 0xfc) == 0x00 && (dfc & 0xfc) == 0x00) {
			if (address == 0x4c && !(chen & 0x03))
				name = "tmp451";
			else if (address >= 0x48 && address <= 0x4f)
				name = "tmp461";
		}
	}

	return name;
}

 
static int lm90_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	int man_id, chip_id, config1, convrate, lhigh;
	const char *name = NULL;
	int address = client->addr;
	bool common_address =
			(address >= 0x18 && address <= 0x1a) ||
			(address >= 0x29 && address <= 0x2b) ||
			(address >= 0x4c && address <= 0x4e);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	 
	lhigh = i2c_smbus_read_byte_data(client, LM90_REG_LOCAL_HIGH);

	 
	man_id = i2c_smbus_read_byte_data(client, LM90_REG_MAN_ID);
	chip_id = i2c_smbus_read_byte_data(client, LM90_REG_CHIP_ID);
	config1 = i2c_smbus_read_byte_data(client, LM90_REG_CONFIG1);
	convrate = i2c_smbus_read_byte_data(client, LM90_REG_CONVRATE);
	if (man_id < 0 || chip_id < 0 || config1 < 0 || convrate < 0 || lhigh < 0)
		return -ENODEV;

	 
	if (lhigh == man_id && lhigh == chip_id && lhigh == config1 && lhigh == convrate)
		return -ENODEV;

	 
	if (man_id == lhigh && chip_id == lhigh) {
		convrate = i2c_smbus_read_byte_data(client, LM90_REG_CONVRATE);
		man_id = i2c_smbus_read_byte_data(client, LM90_REG_MAN_ID);
		chip_id = i2c_smbus_read_byte_data(client, LM90_REG_CHIP_ID);
		if (convrate < 0 || man_id < 0 || chip_id < 0)
			return -ENODEV;
		if (man_id == convrate && chip_id == convrate)
			man_id = -1;
	}
	switch (man_id) {
	case -1:	 
		if (common_address && !convrate && !(config1 & 0x7f))
			name = lm90_detect_lm84(client);
		break;
	case 0x01:	 
		name = lm90_detect_national(client, chip_id, config1, convrate);
		break;
	case 0x1a:	 
		name = lm90_detect_on(client, chip_id, config1, convrate);
		break;
	case 0x23:	 
		if (common_address && !(config1 & 0x3f) && !(convrate & 0xf8))
			name = "gl523sm";
		break;
	case 0x41:	 
		name = lm90_detect_analog(client, common_address, chip_id, config1,
					  convrate);
		break;
	case 0x47:	 
		name = lm90_detect_gmt(client, chip_id, config1, convrate);
		break;
	case 0x49:	 
		name = lm90_detect_ti49(client, common_address, chip_id, config1, convrate);
		break;
	case 0x4d:	 
		name = lm90_detect_maxim(client, common_address, chip_id,
					 config1, convrate);
		break;
	case 0x54:	 
		if (common_address && !(config1 & 0x3f) && !(convrate & 0xf8))
			name = "mc1066";
		break;
	case 0x55:	 
		name = lm90_detect_ti(client, chip_id, config1, convrate);
		break;
	case 0x5c:	 
		name = lm90_detect_nuvoton(client, chip_id, config1, convrate);
		break;
	case 0xa1:	 
		name = lm90_detect_nxp(client, common_address, chip_id, config1, convrate);
		break;
	case 0xff:	 
		if (common_address && chip_id == 0xff && convrate < 8)
			name = lm90_detect_max1617(client, config1);
		break;
	default:
		break;
	}

	if (!name) {	 
		dev_dbg(&adapter->dev,
			"Unsupported chip at 0x%02x (man_id=0x%02X, chip_id=0x%02X)\n",
			client->addr, man_id, chip_id);
		return -ENODEV;
	}

	strscpy(info->type, name, I2C_NAME_SIZE);

	return 0;
}

static void lm90_restore_conf(void *_data)
{
	struct lm90_data *data = _data;
	struct i2c_client *client = data->client;

	cancel_delayed_work_sync(&data->alert_work);
	cancel_work_sync(&data->report_work);

	 
	if (data->flags & LM90_HAVE_CONVRATE)
		lm90_write_convrate(data, data->convrate_orig);
	lm90_write_reg(client, LM90_REG_CONFIG1, data->config_orig);
}

static int lm90_init_client(struct i2c_client *client, struct lm90_data *data)
{
	struct device_node *np = client->dev.of_node;
	int config, convrate;

	if (data->flags & LM90_HAVE_CONVRATE) {
		convrate = lm90_read_reg(client, LM90_REG_CONVRATE);
		if (convrate < 0)
			return convrate;
		data->convrate_orig = convrate;
		lm90_set_convrate(client, data, 500);  
	} else {
		data->update_interval = 500;
	}

	 
	config = lm90_read_reg(client, LM90_REG_CONFIG1);
	if (config < 0)
		return config;
	data->config_orig = config;
	data->config = config;

	 
	if (data->flags & LM90_HAVE_EXTENDED_TEMP) {
		if (of_property_read_bool(np, "ti,extended-range-enable"))
			config |= 0x04;
		if (!(config & 0x04))
			data->flags &= ~LM90_HAVE_EXTENDED_TEMP;
	}

	 
	if (data->kind == max6680)
		config |= 0x18;

	 
	if (data->kind == max6654)
		config |= 0x20;

	 
	if (data->flags & LM90_HAVE_TEMP3)
		config &= ~0x08;

	 
	if (client->irq)
		config &= ~0x80;

	config &= 0xBF;	 
	lm90_update_confreg(data, config);

	return devm_add_action_or_reset(&client->dev, lm90_restore_conf, data);
}

static bool lm90_is_tripped(struct i2c_client *client)
{
	struct lm90_data *data = i2c_get_clientdata(client);
	int ret;

	ret = lm90_update_alarms(data, true);
	if (ret < 0)
		return false;

	return !!data->current_alarms;
}

static irqreturn_t lm90_irq_thread(int irq, void *dev_id)
{
	struct i2c_client *client = dev_id;

	if (lm90_is_tripped(client))
		return IRQ_HANDLED;
	else
		return IRQ_NONE;
}

static void lm90_remove_pec(void *dev)
{
	device_remove_file(dev, &dev_attr_pec);
}

static int lm90_probe_channel_from_dt(struct i2c_client *client,
				      struct device_node *child,
				      struct lm90_data *data)
{
	u32 id;
	s32 val;
	int err;
	struct device *dev = &client->dev;

	err = of_property_read_u32(child, "reg", &id);
	if (err) {
		dev_err(dev, "missing reg property of %pOFn\n", child);
		return err;
	}

	if (id >= MAX_CHANNELS) {
		dev_err(dev, "invalid reg property value %d in %pOFn\n", id, child);
		return -EINVAL;
	}

	err = of_property_read_string(child, "label", &data->channel_label[id]);
	if (err == -ENODATA || err == -EILSEQ) {
		dev_err(dev, "invalid label property in %pOFn\n", child);
		return err;
	}

	if (data->channel_label[id])
		data->channel_config[id] |= HWMON_T_LABEL;

	err = of_property_read_s32(child, "temperature-offset-millicelsius", &val);
	if (!err) {
		if (id == 0) {
			dev_err(dev, "temperature-offset-millicelsius can't be set for internal channel\n");
			return -EINVAL;
		}

		err = lm90_set_temp_offset(data, lm90_temp_offset_index[id], id, val);
		if (err) {
			dev_err(dev, "can't set temperature offset %d for channel %d (%d)\n",
				val, id, err);
			return err;
		}
	}

	return 0;
}

static int lm90_parse_dt_channel_info(struct i2c_client *client,
				      struct lm90_data *data)
{
	int err;
	struct device_node *child;
	struct device *dev = &client->dev;
	const struct device_node *np = dev->of_node;

	for_each_child_of_node(np, child) {
		if (strcmp(child->name, "channel"))
			continue;

		err = lm90_probe_channel_from_dt(client, child, data);
		if (err) {
			of_node_put(child);
			return err;
		}
	}

	return 0;
}

static const struct hwmon_ops lm90_ops = {
	.is_visible = lm90_is_visible,
	.read = lm90_read,
	.read_string = lm90_read_string,
	.write = lm90_write,
};

static int lm90_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct i2c_adapter *adapter = client->adapter;
	struct hwmon_channel_info *info;
	struct device *hwmon_dev;
	struct lm90_data *data;
	int err;

	err = devm_regulator_get_enable(dev, "vcc");
	if (err)
		return dev_err_probe(dev, err, "Failed to enable regulator\n");

	data = devm_kzalloc(dev, sizeof(struct lm90_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);
	INIT_DELAYED_WORK(&data->alert_work, lm90_alert_work);
	INIT_WORK(&data->report_work, lm90_report_alarms);

	 
	if (client->dev.of_node)
		data->kind = (uintptr_t)of_device_get_match_data(&client->dev);
	else
		data->kind = i2c_match_id(lm90_id, client)->driver_data;

	 
	data->alert_alarms = lm90_params[data->kind].alert_alarms;
	data->resolution = lm90_params[data->kind].resolution ? : 11;

	 
	data->flags = lm90_params[data->kind].flags;

	if ((data->flags & (LM90_HAVE_PEC | LM90_HAVE_PARTIAL_PEC)) &&
	    !i2c_check_functionality(adapter, I2C_FUNC_SMBUS_PEC))
		data->flags &= ~(LM90_HAVE_PEC | LM90_HAVE_PARTIAL_PEC);

	if ((data->flags & LM90_HAVE_PARTIAL_PEC) &&
	    !i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		data->flags &= ~LM90_HAVE_PARTIAL_PEC;

	data->chip.ops = &lm90_ops;
	data->chip.info = data->info;

	data->info[0] = &data->chip_info;
	info = &data->chip_info;
	info->type = hwmon_chip;
	info->config = data->chip_config;

	data->chip_config[0] = HWMON_C_REGISTER_TZ;
	if (data->flags & LM90_HAVE_ALARMS)
		data->chip_config[0] |= HWMON_C_ALARMS;
	if (data->flags & LM90_HAVE_CONVRATE)
		data->chip_config[0] |= HWMON_C_UPDATE_INTERVAL;
	if (data->flags & LM90_HAVE_FAULTQUEUE)
		data->chip_config[0] |= HWMON_C_TEMP_SAMPLES;
	data->info[1] = &data->temp_info;

	info = &data->temp_info;
	info->type = hwmon_temp;
	info->config = data->channel_config;

	data->channel_config[0] = HWMON_T_INPUT | HWMON_T_MAX |
		HWMON_T_MAX_ALARM;
	data->channel_config[1] = HWMON_T_INPUT | HWMON_T_MAX |
		HWMON_T_MAX_ALARM | HWMON_T_FAULT;

	if (data->flags & LM90_HAVE_LOW) {
		data->channel_config[0] |= HWMON_T_MIN | HWMON_T_MIN_ALARM;
		data->channel_config[1] |= HWMON_T_MIN | HWMON_T_MIN_ALARM;
	}

	if (data->flags & LM90_HAVE_CRIT) {
		data->channel_config[0] |= HWMON_T_CRIT | HWMON_T_CRIT_ALARM | HWMON_T_CRIT_HYST;
		data->channel_config[1] |= HWMON_T_CRIT | HWMON_T_CRIT_ALARM | HWMON_T_CRIT_HYST;
	}

	if (data->flags & LM90_HAVE_OFFSET)
		data->channel_config[1] |= HWMON_T_OFFSET;

	if (data->flags & LM90_HAVE_EMERGENCY) {
		data->channel_config[0] |= HWMON_T_EMERGENCY |
			HWMON_T_EMERGENCY_HYST;
		data->channel_config[1] |= HWMON_T_EMERGENCY |
			HWMON_T_EMERGENCY_HYST;
	}

	if (data->flags & LM90_HAVE_EMERGENCY_ALARM) {
		data->channel_config[0] |= HWMON_T_EMERGENCY_ALARM;
		data->channel_config[1] |= HWMON_T_EMERGENCY_ALARM;
	}

	if (data->flags & LM90_HAVE_TEMP3) {
		data->channel_config[2] = HWMON_T_INPUT |
			HWMON_T_MIN | HWMON_T_MAX |
			HWMON_T_CRIT | HWMON_T_CRIT_HYST |
			HWMON_T_MIN_ALARM | HWMON_T_MAX_ALARM |
			HWMON_T_CRIT_ALARM | HWMON_T_FAULT;
		if (data->flags & LM90_HAVE_EMERGENCY) {
			data->channel_config[2] |= HWMON_T_EMERGENCY |
				HWMON_T_EMERGENCY_HYST;
		}
		if (data->flags & LM90_HAVE_EMERGENCY_ALARM)
			data->channel_config[2] |= HWMON_T_EMERGENCY_ALARM;
		if (data->flags & LM90_HAVE_OFFSET)
			data->channel_config[2] |= HWMON_T_OFFSET;
	}

	data->faultqueue_mask = lm90_params[data->kind].faultqueue_mask;
	data->faultqueue_depth = lm90_params[data->kind].faultqueue_depth;
	data->reg_local_ext = lm90_params[data->kind].reg_local_ext;
	if (data->flags & LM90_HAVE_REMOTE_EXT)
		data->reg_remote_ext = LM90_REG_REMOTE_TEMPL;
	data->reg_status2 = lm90_params[data->kind].reg_status2;

	 
	data->max_convrate = lm90_params[data->kind].max_convrate;

	 
	if (client->dev.of_node) {
		err = lm90_parse_dt_channel_info(client, data);
		if (err)
			return err;
	}

	 
	err = lm90_init_client(client, data);
	if (err < 0) {
		dev_err(dev, "Failed to initialize device\n");
		return err;
	}

	 
	if (data->flags & (LM90_HAVE_PEC | LM90_HAVE_PARTIAL_PEC)) {
		err = device_create_file(dev, &dev_attr_pec);
		if (err)
			return err;
		err = devm_add_action_or_reset(dev, lm90_remove_pec, dev);
		if (err)
			return err;
	}

	hwmon_dev = devm_hwmon_device_register_with_info(dev, client->name,
							 data, &data->chip,
							 NULL);
	if (IS_ERR(hwmon_dev))
		return PTR_ERR(hwmon_dev);

	data->hwmon_dev = hwmon_dev;

	if (client->irq) {
		dev_dbg(dev, "IRQ: %d\n", client->irq);
		err = devm_request_threaded_irq(dev, client->irq,
						NULL, lm90_irq_thread,
						IRQF_ONESHOT, "lm90", client);
		if (err < 0) {
			dev_err(dev, "cannot request IRQ %d\n", client->irq);
			return err;
		}
	}

	return 0;
}

static void lm90_alert(struct i2c_client *client, enum i2c_alert_protocol type,
		       unsigned int flag)
{
	if (type != I2C_PROTOCOL_SMBUS_ALERT)
		return;

	if (lm90_is_tripped(client)) {
		 
		struct lm90_data *data = i2c_get_clientdata(client);

		if ((data->flags & LM90_HAVE_BROKEN_ALERT) &&
		    (data->current_alarms & data->alert_alarms)) {
			if (!(data->config & 0x80)) {
				dev_dbg(&client->dev, "Disabling ALERT#\n");
				lm90_update_confreg(data, data->config | 0x80);
			}
			schedule_delayed_work(&data->alert_work,
				max_t(int, HZ, msecs_to_jiffies(data->update_interval)));
		}
	} else {
		dev_dbg(&client->dev, "Everything OK\n");
	}
}

static int lm90_suspend(struct device *dev)
{
	struct lm90_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	if (client->irq)
		disable_irq(client->irq);

	return 0;
}

static int lm90_resume(struct device *dev)
{
	struct lm90_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	if (client->irq)
		enable_irq(client->irq);

	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(lm90_pm_ops, lm90_suspend, lm90_resume);

static struct i2c_driver lm90_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= "lm90",
		.of_match_table = of_match_ptr(lm90_of_match),
		.pm	= pm_sleep_ptr(&lm90_pm_ops),
	},
	.probe		= lm90_probe,
	.alert		= lm90_alert,
	.id_table	= lm90_id,
	.detect		= lm90_detect,
	.address_list	= normal_i2c,
};

module_i2c_driver(lm90_driver);

MODULE_AUTHOR("Jean Delvare <jdelvare@suse.de>");
MODULE_DESCRIPTION("LM90/ADM1032 driver");
MODULE_LICENSE("GPL");
