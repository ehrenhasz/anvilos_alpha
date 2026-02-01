
 

 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/dmi.h>
#include <linux/fs.h>
#include <linux/watchdog.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/kref.h>

 
static const unsigned short normal_i2c[] = { 0x73, I2C_CLIENT_END };

 
static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
	__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

enum chips { fscpos, fscher, fscscy, fschrc, fschmd, fschds, fscsyl };

 

 
#define FSCHMD_REG_IDENT_0		0x00
#define FSCHMD_REG_IDENT_1		0x01
#define FSCHMD_REG_IDENT_2		0x02
#define FSCHMD_REG_REVISION		0x03

 
#define FSCHMD_REG_EVENT_STATE		0x04
#define FSCHMD_REG_CONTROL		0x05

#define FSCHMD_CONTROL_ALERT_LED	0x01

 
static const u8 FSCHMD_REG_WDOG_CONTROL[7] = {
	0x21, 0x21, 0x21, 0x21, 0x21, 0x28, 0x28 };
static const u8 FSCHMD_REG_WDOG_STATE[7] = {
	0x23, 0x23, 0x23, 0x23, 0x23, 0x29, 0x29 };
static const u8 FSCHMD_REG_WDOG_PRESET[7] = {
	0x28, 0x28, 0x28, 0x28, 0x28, 0x2a, 0x2a };

#define FSCHMD_WDOG_CONTROL_TRIGGER	0x10
#define FSCHMD_WDOG_CONTROL_STARTED	0x10  
#define FSCHMD_WDOG_CONTROL_STOP	0x20
#define FSCHMD_WDOG_CONTROL_RESOLUTION	0x40

#define FSCHMD_WDOG_STATE_CARDRESET	0x02

 
static const u8 FSCHMD_REG_VOLT[7][6] = {
	{ 0x45, 0x42, 0x48 },				 
	{ 0x45, 0x42, 0x48 },				 
	{ 0x45, 0x42, 0x48 },				 
	{ 0x45, 0x42, 0x48 },				 
	{ 0x45, 0x42, 0x48 },				 
	{ 0x21, 0x20, 0x22 },				 
	{ 0x21, 0x20, 0x22, 0x23, 0x24, 0x25 },		 
};

static const int FSCHMD_NO_VOLT_SENSORS[7] = { 3, 3, 3, 3, 3, 3, 6 };

 
static const u8 FSCHMD_REG_FAN_MIN[7][7] = {
	{ 0x55, 0x65 },					 
	{ 0x55, 0x65, 0xb5 },				 
	{ 0x65, 0x65, 0x55, 0xa5, 0x55, 0xa5 },		 
	{ 0x55, 0x65, 0xa5, 0xb5 },			 
	{ 0x55, 0x65, 0xa5, 0xb5, 0xc5 },		 
	{ 0x55, 0x65, 0xa5, 0xb5, 0xc5 },		 
	{ 0x54, 0x64, 0x74, 0x84, 0x94, 0xa4, 0xb4 },	 
};

 
static const u8 FSCHMD_REG_FAN_ACT[7][7] = {
	{ 0x0e, 0x6b, 0xab },				 
	{ 0x0e, 0x6b, 0xbb },				 
	{ 0x6b, 0x6c, 0x0e, 0xab, 0x5c, 0xbb },		 
	{ 0x0e, 0x6b, 0xab, 0xbb },			 
	{ 0x5b, 0x6b, 0xab, 0xbb, 0xcb },		 
	{ 0x5b, 0x6b, 0xab, 0xbb, 0xcb },		 
	{ 0x57, 0x67, 0x77, 0x87, 0x97, 0xa7, 0xb7 },	 
};

 
static const u8 FSCHMD_REG_FAN_STATE[7][7] = {
	{ 0x0d, 0x62, 0xa2 },				 
	{ 0x0d, 0x62, 0xb2 },				 
	{ 0x62, 0x61, 0x0d, 0xa2, 0x52, 0xb2 },		 
	{ 0x0d, 0x62, 0xa2, 0xb2 },			 
	{ 0x52, 0x62, 0xa2, 0xb2, 0xc2 },		 
	{ 0x52, 0x62, 0xa2, 0xb2, 0xc2 },		 
	{ 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0, 0xb0 },	 
};

 
static const u8 FSCHMD_REG_FAN_RIPPLE[7][7] = {
	{ 0x0f, 0x6f, 0xaf },				 
	{ 0x0f, 0x6f, 0xbf },				 
	{ 0x6f, 0x6f, 0x0f, 0xaf, 0x0f, 0xbf },		 
	{ 0x0f, 0x6f, 0xaf, 0xbf },			 
	{ 0x5f, 0x6f, 0xaf, 0xbf, 0xcf },		 
	{ 0x5f, 0x6f, 0xaf, 0xbf, 0xcf },		 
	{ 0x56, 0x66, 0x76, 0x86, 0x96, 0xa6, 0xb6 },	 
};

static const int FSCHMD_NO_FAN_SENSORS[7] = { 3, 3, 6, 4, 5, 5, 7 };

 
#define FSCHMD_FAN_ALARM	0x04  
#define FSCHMD_FAN_NOT_PRESENT	0x08
#define FSCHMD_FAN_DISABLED	0x80


 
static const u8 FSCHMD_REG_TEMP_ACT[7][11] = {
	{ 0x64, 0x32, 0x35 },				 
	{ 0x64, 0x32, 0x35 },				 
	{ 0x64, 0xD0, 0x32, 0x35 },			 
	{ 0x64, 0x32, 0x35 },				 
	{ 0x70, 0x80, 0x90, 0xd0, 0xe0 },		 
	{ 0x70, 0x80, 0x90, 0xd0, 0xe0 },		 
	{ 0x58, 0x68, 0x78, 0x88, 0x98, 0xa8,		 
	  0xb8, 0xc8, 0xd8, 0xe8, 0xf8 },
};

 
static const u8 FSCHMD_REG_TEMP_STATE[7][11] = {
	{ 0x71, 0x81, 0x91 },				 
	{ 0x71, 0x81, 0x91 },				 
	{ 0x71, 0xd1, 0x81, 0x91 },			 
	{ 0x71, 0x81, 0x91 },				 
	{ 0x71, 0x81, 0x91, 0xd1, 0xe1 },		 
	{ 0x71, 0x81, 0x91, 0xd1, 0xe1 },		 
	{ 0x59, 0x69, 0x79, 0x89, 0x99, 0xa9,		 
	  0xb9, 0xc9, 0xd9, 0xe9, 0xf9 },
};

 
static const u8 FSCHMD_REG_TEMP_LIMIT[7][11] = {
	{ 0, 0, 0 },					 
	{ 0x76, 0x86, 0x96 },				 
	{ 0x76, 0xd6, 0x86, 0x96 },			 
	{ 0x76, 0x86, 0x96 },				 
	{ 0x76, 0x86, 0x96, 0xd6, 0xe6 },		 
	{ 0x76, 0x86, 0x96, 0xd6, 0xe6 },		 
	{ 0x5a, 0x6a, 0x7a, 0x8a, 0x9a, 0xaa,		 
	  0xba, 0xca, 0xda, 0xea, 0xfa },
};

 

static const int FSCHMD_NO_TEMP_SENSORS[7] = { 3, 3, 4, 3, 5, 5, 11 };

 
#define FSCHMD_TEMP_WORKING	0x01
#define FSCHMD_TEMP_ALERT	0x02
#define FSCHMD_TEMP_DISABLED	0x80
 
#define FSCHMD_TEMP_ALARM_MASK \
	(FSCHMD_TEMP_WORKING | FSCHMD_TEMP_ALERT)

 

static int fschmd_probe(struct i2c_client *client);
static int fschmd_detect(struct i2c_client *client,
			 struct i2c_board_info *info);
static void fschmd_remove(struct i2c_client *client);
static struct fschmd_data *fschmd_update_device(struct device *dev);

 

static const struct i2c_device_id fschmd_id[] = {
	{ "fscpos", fscpos },
	{ "fscher", fscher },
	{ "fscscy", fscscy },
	{ "fschrc", fschrc },
	{ "fschmd", fschmd },
	{ "fschds", fschds },
	{ "fscsyl", fscsyl },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fschmd_id);

static struct i2c_driver fschmd_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= "fschmd",
	},
	.probe		= fschmd_probe,
	.remove		= fschmd_remove,
	.id_table	= fschmd_id,
	.detect		= fschmd_detect,
	.address_list	= normal_i2c,
};

 

struct fschmd_data {
	struct i2c_client *client;
	struct device *hwmon_dev;
	struct mutex update_lock;
	struct mutex watchdog_lock;
	struct list_head list;  
	struct kref kref;
	struct miscdevice watchdog_miscdev;
	enum chips kind;
	unsigned long watchdog_is_open;
	char watchdog_expect_close;
	char watchdog_name[10];  
	bool valid;  
	unsigned long last_updated;  

	 
	u8 revision;             
	u8 global_control;	 
	u8 watchdog_control;     
	u8 watchdog_state;       
	u8 watchdog_preset;      
	u8 volt[6];		 
	u8 temp_act[11];	 
	u8 temp_status[11];	 
	u8 temp_max[11];	 
	u8 fan_act[7];		 
	u8 fan_status[7];	 
	u8 fan_min[7];		 
	u8 fan_ripple[7];	 
};

 
static int dmi_mult[6] = { 490, 200, 100, 100, 200, 100 };
static int dmi_offset[6] = { 0, 0, 0, 0, 0, 0 };
static int dmi_vref = -1;

 
static LIST_HEAD(watchdog_data_list);
 
static DEFINE_MUTEX(watchdog_data_mutex);

 
static void fschmd_release_resources(struct kref *ref)
{
	struct fschmd_data *data = container_of(ref, struct fschmd_data, kref);
	kfree(data);
}

 

static ssize_t in_value_show(struct device *dev,
			     struct device_attribute *devattr, char *buf)
{
	const int max_reading[3] = { 14200, 6600, 3300 };
	int index = to_sensor_dev_attr(devattr)->index;
	struct fschmd_data *data = fschmd_update_device(dev);

	if (data->kind == fscher || data->kind >= fschrc)
		return sprintf(buf, "%d\n", (data->volt[index] * dmi_vref *
			dmi_mult[index]) / 255 + dmi_offset[index]);
	else
		return sprintf(buf, "%d\n", (data->volt[index] *
			max_reading[index] + 128) / 255);
}


#define TEMP_FROM_REG(val)	(((val) - 128) * 1000)

static ssize_t temp_value_show(struct device *dev,
			       struct device_attribute *devattr, char *buf)
{
	int index = to_sensor_dev_attr(devattr)->index;
	struct fschmd_data *data = fschmd_update_device(dev);

	return sprintf(buf, "%d\n", TEMP_FROM_REG(data->temp_act[index]));
}

static ssize_t temp_max_show(struct device *dev,
			     struct device_attribute *devattr, char *buf)
{
	int index = to_sensor_dev_attr(devattr)->index;
	struct fschmd_data *data = fschmd_update_device(dev);

	return sprintf(buf, "%d\n", TEMP_FROM_REG(data->temp_max[index]));
}

static ssize_t temp_max_store(struct device *dev,
			      struct device_attribute *devattr,
			      const char *buf, size_t count)
{
	int index = to_sensor_dev_attr(devattr)->index;
	struct fschmd_data *data = dev_get_drvdata(dev);
	long v;
	int err;

	err = kstrtol(buf, 10, &v);
	if (err)
		return err;

	v = clamp_val(v / 1000, -128, 127) + 128;

	mutex_lock(&data->update_lock);
	i2c_smbus_write_byte_data(to_i2c_client(dev),
		FSCHMD_REG_TEMP_LIMIT[data->kind][index], v);
	data->temp_max[index] = v;
	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t temp_fault_show(struct device *dev,
			       struct device_attribute *devattr, char *buf)
{
	int index = to_sensor_dev_attr(devattr)->index;
	struct fschmd_data *data = fschmd_update_device(dev);

	 
	if (data->temp_status[index] & FSCHMD_TEMP_WORKING)
		return sprintf(buf, "0\n");
	else
		return sprintf(buf, "1\n");
}

static ssize_t temp_alarm_show(struct device *dev,
			       struct device_attribute *devattr, char *buf)
{
	int index = to_sensor_dev_attr(devattr)->index;
	struct fschmd_data *data = fschmd_update_device(dev);

	if ((data->temp_status[index] & FSCHMD_TEMP_ALARM_MASK) ==
			FSCHMD_TEMP_ALARM_MASK)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}


#define RPM_FROM_REG(val)	((val) * 60)

static ssize_t fan_value_show(struct device *dev,
			      struct device_attribute *devattr, char *buf)
{
	int index = to_sensor_dev_attr(devattr)->index;
	struct fschmd_data *data = fschmd_update_device(dev);

	return sprintf(buf, "%u\n", RPM_FROM_REG(data->fan_act[index]));
}

static ssize_t fan_div_show(struct device *dev,
			    struct device_attribute *devattr, char *buf)
{
	int index = to_sensor_dev_attr(devattr)->index;
	struct fschmd_data *data = fschmd_update_device(dev);

	 
	return sprintf(buf, "%d\n", 1 << (data->fan_ripple[index] & 3));
}

static ssize_t fan_div_store(struct device *dev,
			     struct device_attribute *devattr,
			     const char *buf, size_t count)
{
	u8 reg;
	int index = to_sensor_dev_attr(devattr)->index;
	struct fschmd_data *data = dev_get_drvdata(dev);
	 
	unsigned long v;
	int err;

	err = kstrtoul(buf, 10, &v);
	if (err)
		return err;

	switch (v) {
	case 2:
		v = 1;
		break;
	case 4:
		v = 2;
		break;
	case 8:
		v = 3;
		break;
	default:
		dev_err(dev,
			"fan_div value %lu not supported. Choose one of 2, 4 or 8!\n",
			v);
		return -EINVAL;
	}

	mutex_lock(&data->update_lock);

	reg = i2c_smbus_read_byte_data(to_i2c_client(dev),
		FSCHMD_REG_FAN_RIPPLE[data->kind][index]);

	 
	reg &= ~0x03;
	reg |= v;

	i2c_smbus_write_byte_data(to_i2c_client(dev),
		FSCHMD_REG_FAN_RIPPLE[data->kind][index], reg);

	data->fan_ripple[index] = reg;

	mutex_unlock(&data->update_lock);

	return count;
}

static ssize_t fan_alarm_show(struct device *dev,
			      struct device_attribute *devattr, char *buf)
{
	int index = to_sensor_dev_attr(devattr)->index;
	struct fschmd_data *data = fschmd_update_device(dev);

	if (data->fan_status[index] & FSCHMD_FAN_ALARM)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}

static ssize_t fan_fault_show(struct device *dev,
			      struct device_attribute *devattr, char *buf)
{
	int index = to_sensor_dev_attr(devattr)->index;
	struct fschmd_data *data = fschmd_update_device(dev);

	if (data->fan_status[index] & FSCHMD_FAN_NOT_PRESENT)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}


static ssize_t pwm_auto_point1_pwm_show(struct device *dev,
					struct device_attribute *devattr,
					char *buf)
{
	int index = to_sensor_dev_attr(devattr)->index;
	struct fschmd_data *data = fschmd_update_device(dev);
	int val = data->fan_min[index];

	 
	if (val || data->kind == fscsyl)
		val = val / 2 + 128;

	return sprintf(buf, "%d\n", val);
}

static ssize_t pwm_auto_point1_pwm_store(struct device *dev,
					 struct device_attribute *devattr,
					 const char *buf, size_t count)
{
	int index = to_sensor_dev_attr(devattr)->index;
	struct fschmd_data *data = dev_get_drvdata(dev);
	unsigned long v;
	int err;

	err = kstrtoul(buf, 10, &v);
	if (err)
		return err;

	 
	if (v || data->kind == fscsyl) {
		v = clamp_val(v, 128, 255);
		v = (v - 128) * 2 + 1;
	}

	mutex_lock(&data->update_lock);

	i2c_smbus_write_byte_data(to_i2c_client(dev),
		FSCHMD_REG_FAN_MIN[data->kind][index], v);
	data->fan_min[index] = v;

	mutex_unlock(&data->update_lock);

	return count;
}


 
static ssize_t alert_led_show(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct fschmd_data *data = fschmd_update_device(dev);

	if (data->global_control & FSCHMD_CONTROL_ALERT_LED)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}

static ssize_t alert_led_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	u8 reg;
	struct fschmd_data *data = dev_get_drvdata(dev);
	unsigned long v;
	int err;

	err = kstrtoul(buf, 10, &v);
	if (err)
		return err;

	mutex_lock(&data->update_lock);

	reg = i2c_smbus_read_byte_data(to_i2c_client(dev), FSCHMD_REG_CONTROL);

	if (v)
		reg |= FSCHMD_CONTROL_ALERT_LED;
	else
		reg &= ~FSCHMD_CONTROL_ALERT_LED;

	i2c_smbus_write_byte_data(to_i2c_client(dev), FSCHMD_REG_CONTROL, reg);

	data->global_control = reg;

	mutex_unlock(&data->update_lock);

	return count;
}

static DEVICE_ATTR_RW(alert_led);

static struct sensor_device_attribute fschmd_attr[] = {
	SENSOR_ATTR_RO(in0_input, in_value, 0),
	SENSOR_ATTR_RO(in1_input, in_value, 1),
	SENSOR_ATTR_RO(in2_input, in_value, 2),
	SENSOR_ATTR_RO(in3_input, in_value, 3),
	SENSOR_ATTR_RO(in4_input, in_value, 4),
	SENSOR_ATTR_RO(in5_input, in_value, 5),
};

static struct sensor_device_attribute fschmd_temp_attr[] = {
	SENSOR_ATTR_RO(temp1_input, temp_value, 0),
	SENSOR_ATTR_RW(temp1_max, temp_max, 0),
	SENSOR_ATTR_RO(temp1_fault, temp_fault, 0),
	SENSOR_ATTR_RO(temp1_alarm, temp_alarm, 0),
	SENSOR_ATTR_RO(temp2_input, temp_value, 1),
	SENSOR_ATTR_RW(temp2_max, temp_max, 1),
	SENSOR_ATTR_RO(temp2_fault, temp_fault, 1),
	SENSOR_ATTR_RO(temp2_alarm, temp_alarm, 1),
	SENSOR_ATTR_RO(temp3_input, temp_value, 2),
	SENSOR_ATTR_RW(temp3_max, temp_max, 2),
	SENSOR_ATTR_RO(temp3_fault, temp_fault, 2),
	SENSOR_ATTR_RO(temp3_alarm, temp_alarm, 2),
	SENSOR_ATTR_RO(temp4_input, temp_value, 3),
	SENSOR_ATTR_RW(temp4_max, temp_max, 3),
	SENSOR_ATTR_RO(temp4_fault, temp_fault, 3),
	SENSOR_ATTR_RO(temp4_alarm, temp_alarm, 3),
	SENSOR_ATTR_RO(temp5_input, temp_value, 4),
	SENSOR_ATTR_RW(temp5_max, temp_max, 4),
	SENSOR_ATTR_RO(temp5_fault, temp_fault, 4),
	SENSOR_ATTR_RO(temp5_alarm, temp_alarm, 4),
	SENSOR_ATTR_RO(temp6_input, temp_value, 5),
	SENSOR_ATTR_RW(temp6_max, temp_max, 5),
	SENSOR_ATTR_RO(temp6_fault, temp_fault, 5),
	SENSOR_ATTR_RO(temp6_alarm, temp_alarm, 5),
	SENSOR_ATTR_RO(temp7_input, temp_value, 6),
	SENSOR_ATTR_RW(temp7_max, temp_max, 6),
	SENSOR_ATTR_RO(temp7_fault, temp_fault, 6),
	SENSOR_ATTR_RO(temp7_alarm, temp_alarm, 6),
	SENSOR_ATTR_RO(temp8_input, temp_value, 7),
	SENSOR_ATTR_RW(temp8_max, temp_max, 7),
	SENSOR_ATTR_RO(temp8_fault, temp_fault, 7),
	SENSOR_ATTR_RO(temp8_alarm, temp_alarm, 7),
	SENSOR_ATTR_RO(temp9_input, temp_value, 8),
	SENSOR_ATTR_RW(temp9_max, temp_max, 8),
	SENSOR_ATTR_RO(temp9_fault, temp_fault, 8),
	SENSOR_ATTR_RO(temp9_alarm, temp_alarm, 8),
	SENSOR_ATTR_RO(temp10_input, temp_value, 9),
	SENSOR_ATTR_RW(temp10_max, temp_max, 9),
	SENSOR_ATTR_RO(temp10_fault, temp_fault, 9),
	SENSOR_ATTR_RO(temp10_alarm, temp_alarm, 9),
	SENSOR_ATTR_RO(temp11_input, temp_value, 10),
	SENSOR_ATTR_RW(temp11_max, temp_max, 10),
	SENSOR_ATTR_RO(temp11_fault, temp_fault, 10),
	SENSOR_ATTR_RO(temp11_alarm, temp_alarm, 10),
};

static struct sensor_device_attribute fschmd_fan_attr[] = {
	SENSOR_ATTR_RO(fan1_input, fan_value, 0),
	SENSOR_ATTR_RW(fan1_div, fan_div, 0),
	SENSOR_ATTR_RO(fan1_alarm, fan_alarm, 0),
	SENSOR_ATTR_RO(fan1_fault, fan_fault, 0),
	SENSOR_ATTR_RW(pwm1_auto_point1_pwm, pwm_auto_point1_pwm, 0),
	SENSOR_ATTR_RO(fan2_input, fan_value, 1),
	SENSOR_ATTR_RW(fan2_div, fan_div, 1),
	SENSOR_ATTR_RO(fan2_alarm, fan_alarm, 1),
	SENSOR_ATTR_RO(fan2_fault, fan_fault, 1),
	SENSOR_ATTR_RW(pwm2_auto_point1_pwm, pwm_auto_point1_pwm, 1),
	SENSOR_ATTR_RO(fan3_input, fan_value, 2),
	SENSOR_ATTR_RW(fan3_div, fan_div, 2),
	SENSOR_ATTR_RO(fan3_alarm, fan_alarm, 2),
	SENSOR_ATTR_RO(fan3_fault, fan_fault, 2),
	SENSOR_ATTR_RW(pwm3_auto_point1_pwm, pwm_auto_point1_pwm, 2),
	SENSOR_ATTR_RO(fan4_input, fan_value, 3),
	SENSOR_ATTR_RW(fan4_div, fan_div, 3),
	SENSOR_ATTR_RO(fan4_alarm, fan_alarm, 3),
	SENSOR_ATTR_RO(fan4_fault, fan_fault, 3),
	SENSOR_ATTR_RW(pwm4_auto_point1_pwm, pwm_auto_point1_pwm, 3),
	SENSOR_ATTR_RO(fan5_input, fan_value, 4),
	SENSOR_ATTR_RW(fan5_div, fan_div, 4),
	SENSOR_ATTR_RO(fan5_alarm, fan_alarm, 4),
	SENSOR_ATTR_RO(fan5_fault, fan_fault, 4),
	SENSOR_ATTR_RW(pwm5_auto_point1_pwm, pwm_auto_point1_pwm, 4),
	SENSOR_ATTR_RO(fan6_input, fan_value, 5),
	SENSOR_ATTR_RW(fan6_div, fan_div, 5),
	SENSOR_ATTR_RO(fan6_alarm, fan_alarm, 5),
	SENSOR_ATTR_RO(fan6_fault, fan_fault, 5),
	SENSOR_ATTR_RW(pwm6_auto_point1_pwm, pwm_auto_point1_pwm, 5),
	SENSOR_ATTR_RO(fan7_input, fan_value, 6),
	SENSOR_ATTR_RW(fan7_div, fan_div, 6),
	SENSOR_ATTR_RO(fan7_alarm, fan_alarm, 6),
	SENSOR_ATTR_RO(fan7_fault, fan_fault, 6),
	SENSOR_ATTR_RW(pwm7_auto_point1_pwm, pwm_auto_point1_pwm, 6),
};


 

static int watchdog_set_timeout(struct fschmd_data *data, int timeout)
{
	int ret, resolution;
	int kind = data->kind + 1;  

	 
	if (timeout <= 510 || kind == fscpos || kind == fscscy)
		resolution = 2;
	else
		resolution = 60;

	if (timeout < resolution || timeout > (resolution * 255))
		return -EINVAL;

	mutex_lock(&data->watchdog_lock);
	if (!data->client) {
		ret = -ENODEV;
		goto leave;
	}

	if (resolution == 2)
		data->watchdog_control &= ~FSCHMD_WDOG_CONTROL_RESOLUTION;
	else
		data->watchdog_control |= FSCHMD_WDOG_CONTROL_RESOLUTION;

	data->watchdog_preset = DIV_ROUND_UP(timeout, resolution);

	 
	i2c_smbus_write_byte_data(data->client,
		FSCHMD_REG_WDOG_PRESET[data->kind], data->watchdog_preset);
	 
	i2c_smbus_write_byte_data(data->client,
		FSCHMD_REG_WDOG_CONTROL[data->kind],
		data->watchdog_control & ~FSCHMD_WDOG_CONTROL_TRIGGER);

	ret = data->watchdog_preset * resolution;

leave:
	mutex_unlock(&data->watchdog_lock);
	return ret;
}

static int watchdog_get_timeout(struct fschmd_data *data)
{
	int timeout;

	mutex_lock(&data->watchdog_lock);
	if (data->watchdog_control & FSCHMD_WDOG_CONTROL_RESOLUTION)
		timeout = data->watchdog_preset * 60;
	else
		timeout = data->watchdog_preset * 2;
	mutex_unlock(&data->watchdog_lock);

	return timeout;
}

static int watchdog_trigger(struct fschmd_data *data)
{
	int ret = 0;

	mutex_lock(&data->watchdog_lock);
	if (!data->client) {
		ret = -ENODEV;
		goto leave;
	}

	data->watchdog_control |= FSCHMD_WDOG_CONTROL_TRIGGER;
	i2c_smbus_write_byte_data(data->client,
				  FSCHMD_REG_WDOG_CONTROL[data->kind],
				  data->watchdog_control);
leave:
	mutex_unlock(&data->watchdog_lock);
	return ret;
}

static int watchdog_stop(struct fschmd_data *data)
{
	int ret = 0;

	mutex_lock(&data->watchdog_lock);
	if (!data->client) {
		ret = -ENODEV;
		goto leave;
	}

	data->watchdog_control &= ~FSCHMD_WDOG_CONTROL_STARTED;
	 
	i2c_smbus_write_byte_data(data->client,
		FSCHMD_REG_WDOG_CONTROL[data->kind],
		data->watchdog_control | FSCHMD_WDOG_CONTROL_STOP);
leave:
	mutex_unlock(&data->watchdog_lock);
	return ret;
}

static int watchdog_open(struct inode *inode, struct file *filp)
{
	struct fschmd_data *pos, *data = NULL;
	int watchdog_is_open;

	 
	if (!mutex_trylock(&watchdog_data_mutex))
		return -ERESTARTSYS;
	list_for_each_entry(pos, &watchdog_data_list, list) {
		if (pos->watchdog_miscdev.minor == iminor(inode)) {
			data = pos;
			break;
		}
	}
	 
	watchdog_is_open = test_and_set_bit(0, &data->watchdog_is_open);
	if (!watchdog_is_open)
		kref_get(&data->kref);
	mutex_unlock(&watchdog_data_mutex);

	if (watchdog_is_open)
		return -EBUSY;

	 
	watchdog_trigger(data);
	filp->private_data = data;

	return stream_open(inode, filp);
}

static int watchdog_release(struct inode *inode, struct file *filp)
{
	struct fschmd_data *data = filp->private_data;

	if (data->watchdog_expect_close) {
		watchdog_stop(data);
		data->watchdog_expect_close = 0;
	} else {
		watchdog_trigger(data);
		dev_crit(&data->client->dev,
			"unexpected close, not stopping watchdog!\n");
	}

	clear_bit(0, &data->watchdog_is_open);

	mutex_lock(&watchdog_data_mutex);
	kref_put(&data->kref, fschmd_release_resources);
	mutex_unlock(&watchdog_data_mutex);

	return 0;
}

static ssize_t watchdog_write(struct file *filp, const char __user *buf,
	size_t count, loff_t *offset)
{
	int ret;
	struct fschmd_data *data = filp->private_data;

	if (count) {
		if (!nowayout) {
			size_t i;

			 
			data->watchdog_expect_close = 0;

			for (i = 0; i != count; i++) {
				char c;
				if (get_user(c, buf + i))
					return -EFAULT;
				if (c == 'V')
					data->watchdog_expect_close = 1;
			}
		}
		ret = watchdog_trigger(data);
		if (ret < 0)
			return ret;
	}
	return count;
}

static long watchdog_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long arg)
{
	struct watchdog_info ident = {
		.options = WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT |
				WDIOF_CARDRESET,
		.identity = "FSC watchdog"
	};
	int i, ret = 0;
	struct fschmd_data *data = filp->private_data;

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		ident.firmware_version = data->revision;
		if (!nowayout)
			ident.options |= WDIOF_MAGICCLOSE;
		if (copy_to_user((void __user *)arg, &ident, sizeof(ident)))
			ret = -EFAULT;
		break;

	case WDIOC_GETSTATUS:
		ret = put_user(0, (int __user *)arg);
		break;

	case WDIOC_GETBOOTSTATUS:
		if (data->watchdog_state & FSCHMD_WDOG_STATE_CARDRESET)
			ret = put_user(WDIOF_CARDRESET, (int __user *)arg);
		else
			ret = put_user(0, (int __user *)arg);
		break;

	case WDIOC_KEEPALIVE:
		ret = watchdog_trigger(data);
		break;

	case WDIOC_GETTIMEOUT:
		i = watchdog_get_timeout(data);
		ret = put_user(i, (int __user *)arg);
		break;

	case WDIOC_SETTIMEOUT:
		if (get_user(i, (int __user *)arg)) {
			ret = -EFAULT;
			break;
		}
		ret = watchdog_set_timeout(data, i);
		if (ret > 0)
			ret = put_user(ret, (int __user *)arg);
		break;

	case WDIOC_SETOPTIONS:
		if (get_user(i, (int __user *)arg)) {
			ret = -EFAULT;
			break;
		}

		if (i & WDIOS_DISABLECARD)
			ret = watchdog_stop(data);
		else if (i & WDIOS_ENABLECARD)
			ret = watchdog_trigger(data);
		else
			ret = -EINVAL;

		break;
	default:
		ret = -ENOTTY;
	}
	return ret;
}

static const struct file_operations watchdog_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.open = watchdog_open,
	.release = watchdog_release,
	.write = watchdog_write,
	.unlocked_ioctl = watchdog_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
};


 

 
static void fschmd_dmi_decode(const struct dmi_header *header, void *dummy)
{
	int i, mult[3] = { 0 }, offset[3] = { 0 }, vref = 0, found = 0;

	 
	u8 *dmi_data = (u8 *)header;

	 
	if (header->type != 185)
		return;

	 
	if (header->length < 5 || dmi_data[4] != 19)
		return;

	 
	for (i = 6; (i + 4) < header->length; i += 5) {
		 
		if (dmi_data[i] >= 1 && dmi_data[i] <= 3) {
			 
			const int shuffle[3] = { 1, 0, 2 };
			int in = shuffle[dmi_data[i] - 1];

			 
			if (found & (1 << in))
				return;

			mult[in] = dmi_data[i + 1] | (dmi_data[i + 2] << 8);
			offset[in] = dmi_data[i + 3] | (dmi_data[i + 4] << 8);

			found |= 1 << in;
		}

		 
		if (dmi_data[i] == 7) {
			 
			if (found & 0x08)
				return;

			vref = dmi_data[i + 1] | (dmi_data[i + 2] << 8);

			found |= 0x08;
		}
	}

	if (found == 0x0F) {
		for (i = 0; i < 3; i++) {
			dmi_mult[i] = mult[i] * 10;
			dmi_offset[i] = offset[i] * 10;
		}
		 
		dmi_mult[3] = dmi_mult[2];
		dmi_mult[4] = dmi_mult[1];
		dmi_mult[5] = dmi_mult[2];
		dmi_offset[3] = dmi_offset[2];
		dmi_offset[4] = dmi_offset[1];
		dmi_offset[5] = dmi_offset[2];
		dmi_vref = vref;
	}
}

static int fschmd_detect(struct i2c_client *client,
			 struct i2c_board_info *info)
{
	enum chips kind;
	struct i2c_adapter *adapter = client->adapter;
	char id[4];

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	 
	id[0] = i2c_smbus_read_byte_data(client, FSCHMD_REG_IDENT_0);
	id[1] = i2c_smbus_read_byte_data(client, FSCHMD_REG_IDENT_1);
	id[2] = i2c_smbus_read_byte_data(client, FSCHMD_REG_IDENT_2);
	id[3] = '\0';

	if (!strcmp(id, "PEG"))
		kind = fscpos;
	else if (!strcmp(id, "HER"))
		kind = fscher;
	else if (!strcmp(id, "SCY"))
		kind = fscscy;
	else if (!strcmp(id, "HRC"))
		kind = fschrc;
	else if (!strcmp(id, "HMD"))
		kind = fschmd;
	else if (!strcmp(id, "HDS"))
		kind = fschds;
	else if (!strcmp(id, "SYL"))
		kind = fscsyl;
	else
		return -ENODEV;

	strscpy(info->type, fschmd_id[kind].name, I2C_NAME_SIZE);

	return 0;
}

static int fschmd_probe(struct i2c_client *client)
{
	struct fschmd_data *data;
	static const char * const names[7] = { "Poseidon", "Hermes", "Scylla",
				"Heracles", "Heimdall", "Hades", "Syleus" };
	static const int watchdog_minors[] = { WATCHDOG_MINOR, 212, 213, 214, 215 };
	int i, err;
	enum chips kind = i2c_match_id(fschmd_id, client)->driver_data;

	data = kzalloc(sizeof(struct fschmd_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);
	mutex_init(&data->watchdog_lock);
	INIT_LIST_HEAD(&data->list);
	kref_init(&data->kref);
	 
	data->client = client;
	data->kind = kind;

	if (kind == fscpos) {
		 
		data->temp_max[0] = 70 + 128;
		data->temp_max[1] = 50 + 128;
		data->temp_max[2] = 50 + 128;
	}

	 
	if ((kind == fscher || kind >= fschrc) && dmi_vref == -1) {
		dmi_walk(fschmd_dmi_decode, NULL);
		if (dmi_vref == -1) {
			dev_warn(&client->dev,
				"Couldn't get voltage scaling factors from "
				"BIOS DMI table, using builtin defaults\n");
			dmi_vref = 33;
		}
	}

	 
	data->revision = i2c_smbus_read_byte_data(client, FSCHMD_REG_REVISION);
	data->global_control = i2c_smbus_read_byte_data(client,
					FSCHMD_REG_CONTROL);
	data->watchdog_control = i2c_smbus_read_byte_data(client,
					FSCHMD_REG_WDOG_CONTROL[data->kind]);
	data->watchdog_state = i2c_smbus_read_byte_data(client,
					FSCHMD_REG_WDOG_STATE[data->kind]);
	data->watchdog_preset = i2c_smbus_read_byte_data(client,
					FSCHMD_REG_WDOG_PRESET[data->kind]);

	err = device_create_file(&client->dev, &dev_attr_alert_led);
	if (err)
		goto exit_detach;

	for (i = 0; i < FSCHMD_NO_VOLT_SENSORS[data->kind]; i++) {
		err = device_create_file(&client->dev,
					&fschmd_attr[i].dev_attr);
		if (err)
			goto exit_detach;
	}

	for (i = 0; i < (FSCHMD_NO_TEMP_SENSORS[data->kind] * 4); i++) {
		 
		if (kind == fscpos && fschmd_temp_attr[i].dev_attr.show ==
				temp_max_show)
			continue;

		if (kind == fscsyl) {
			if (i % 4 == 0)
				data->temp_status[i / 4] =
					i2c_smbus_read_byte_data(client,
						FSCHMD_REG_TEMP_STATE
						[data->kind][i / 4]);
			if (data->temp_status[i / 4] & FSCHMD_TEMP_DISABLED)
				continue;
		}

		err = device_create_file(&client->dev,
					&fschmd_temp_attr[i].dev_attr);
		if (err)
			goto exit_detach;
	}

	for (i = 0; i < (FSCHMD_NO_FAN_SENSORS[data->kind] * 5); i++) {
		 
		if (kind == fscpos &&
				!strcmp(fschmd_fan_attr[i].dev_attr.attr.name,
					"pwm3_auto_point1_pwm"))
			continue;

		if (kind == fscsyl) {
			if (i % 5 == 0)
				data->fan_status[i / 5] =
					i2c_smbus_read_byte_data(client,
						FSCHMD_REG_FAN_STATE
						[data->kind][i / 5]);
			if (data->fan_status[i / 5] & FSCHMD_FAN_DISABLED)
				continue;
		}

		err = device_create_file(&client->dev,
					&fschmd_fan_attr[i].dev_attr);
		if (err)
			goto exit_detach;
	}

	data->hwmon_dev = hwmon_device_register(&client->dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		data->hwmon_dev = NULL;
		goto exit_detach;
	}

	 
	mutex_lock(&watchdog_data_mutex);
	for (i = 0; i < ARRAY_SIZE(watchdog_minors); i++) {
		 
		snprintf(data->watchdog_name, sizeof(data->watchdog_name),
			"watchdog%c", (i == 0) ? '\0' : ('0' + i));
		data->watchdog_miscdev.name = data->watchdog_name;
		data->watchdog_miscdev.fops = &watchdog_fops;
		data->watchdog_miscdev.minor = watchdog_minors[i];
		err = misc_register(&data->watchdog_miscdev);
		if (err == -EBUSY)
			continue;
		if (err) {
			data->watchdog_miscdev.minor = 0;
			dev_err(&client->dev,
				"Registering watchdog chardev: %d\n", err);
			break;
		}

		list_add(&data->list, &watchdog_data_list);
		watchdog_set_timeout(data, 60);
		dev_info(&client->dev,
			"Registered watchdog chardev major 10, minor: %d\n",
			watchdog_minors[i]);
		break;
	}
	if (i == ARRAY_SIZE(watchdog_minors)) {
		data->watchdog_miscdev.minor = 0;
		dev_warn(&client->dev,
			 "Couldn't register watchdog chardev (due to no free minor)\n");
	}
	mutex_unlock(&watchdog_data_mutex);

	dev_info(&client->dev, "Detected FSC %s chip, revision: %d\n",
		names[data->kind], (int) data->revision);

	return 0;

exit_detach:
	fschmd_remove(client);  
	return err;
}

static void fschmd_remove(struct i2c_client *client)
{
	struct fschmd_data *data = i2c_get_clientdata(client);
	int i;

	 
	if (data->watchdog_miscdev.minor) {
		misc_deregister(&data->watchdog_miscdev);
		if (data->watchdog_is_open) {
			dev_warn(&client->dev,
				"i2c client detached with watchdog open! "
				"Stopping watchdog.\n");
			watchdog_stop(data);
		}
		mutex_lock(&watchdog_data_mutex);
		list_del(&data->list);
		mutex_unlock(&watchdog_data_mutex);
		 
		mutex_lock(&data->watchdog_lock);
		data->client = NULL;
		mutex_unlock(&data->watchdog_lock);
	}

	 
	if (data->hwmon_dev)
		hwmon_device_unregister(data->hwmon_dev);

	device_remove_file(&client->dev, &dev_attr_alert_led);
	for (i = 0; i < (FSCHMD_NO_VOLT_SENSORS[data->kind]); i++)
		device_remove_file(&client->dev, &fschmd_attr[i].dev_attr);
	for (i = 0; i < (FSCHMD_NO_TEMP_SENSORS[data->kind] * 4); i++)
		device_remove_file(&client->dev,
					&fschmd_temp_attr[i].dev_attr);
	for (i = 0; i < (FSCHMD_NO_FAN_SENSORS[data->kind] * 5); i++)
		device_remove_file(&client->dev,
					&fschmd_fan_attr[i].dev_attr);

	mutex_lock(&watchdog_data_mutex);
	kref_put(&data->kref, fschmd_release_resources);
	mutex_unlock(&watchdog_data_mutex);
}

static struct fschmd_data *fschmd_update_device(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fschmd_data *data = i2c_get_clientdata(client);
	int i;

	mutex_lock(&data->update_lock);

	if (time_after(jiffies, data->last_updated + 2 * HZ) || !data->valid) {

		for (i = 0; i < FSCHMD_NO_TEMP_SENSORS[data->kind]; i++) {
			data->temp_act[i] = i2c_smbus_read_byte_data(client,
					FSCHMD_REG_TEMP_ACT[data->kind][i]);
			data->temp_status[i] = i2c_smbus_read_byte_data(client,
					FSCHMD_REG_TEMP_STATE[data->kind][i]);

			 
			if (FSCHMD_REG_TEMP_LIMIT[data->kind][i])
				data->temp_max[i] = i2c_smbus_read_byte_data(
					client,
					FSCHMD_REG_TEMP_LIMIT[data->kind][i]);

			 
			if ((data->temp_status[i] & FSCHMD_TEMP_ALARM_MASK) ==
					FSCHMD_TEMP_ALARM_MASK &&
					data->temp_act[i] < data->temp_max[i])
				i2c_smbus_write_byte_data(client,
					FSCHMD_REG_TEMP_STATE[data->kind][i],
					data->temp_status[i]);
		}

		for (i = 0; i < FSCHMD_NO_FAN_SENSORS[data->kind]; i++) {
			data->fan_act[i] = i2c_smbus_read_byte_data(client,
					FSCHMD_REG_FAN_ACT[data->kind][i]);
			data->fan_status[i] = i2c_smbus_read_byte_data(client,
					FSCHMD_REG_FAN_STATE[data->kind][i]);
			data->fan_ripple[i] = i2c_smbus_read_byte_data(client,
					FSCHMD_REG_FAN_RIPPLE[data->kind][i]);

			 
			if (FSCHMD_REG_FAN_MIN[data->kind][i])
				data->fan_min[i] = i2c_smbus_read_byte_data(
					client,
					FSCHMD_REG_FAN_MIN[data->kind][i]);

			 
			if ((data->fan_status[i] & FSCHMD_FAN_ALARM) &&
					data->fan_act[i])
				i2c_smbus_write_byte_data(client,
					FSCHMD_REG_FAN_STATE[data->kind][i],
					data->fan_status[i]);
		}

		for (i = 0; i < FSCHMD_NO_VOLT_SENSORS[data->kind]; i++)
			data->volt[i] = i2c_smbus_read_byte_data(client,
					       FSCHMD_REG_VOLT[data->kind][i]);

		data->last_updated = jiffies;
		data->valid = true;
	}

	mutex_unlock(&data->update_lock);

	return data;
}

module_i2c_driver(fschmd_driver);

MODULE_AUTHOR("Hans de Goede <hdegoede@redhat.com>");
MODULE_DESCRIPTION("FSC Poseidon, Hermes, Scylla, Heracles, Heimdall, Hades "
			"and Syleus driver");
MODULE_LICENSE("GPL");
