
 

#include <linux/hid.h>
#include <linux/hwmon.h>
#include <linux/math.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include <asm/byteorder.h>
#include <asm/unaligned.h>

 
#define FAN_CHANNELS 3
#define FAN_CHANNELS_MAX 8

#define UPDATE_INTERVAL_DEFAULT_MS 1000

 
static const char *const fan_label[] = {
	"FAN 1",
	"FAN 2",
	"FAN 3",
};

static const char *const curr_label[] = {
	"FAN 1 Current",
	"FAN 2 Current",
	"FAN 3 Current",
};

static const char *const in_label[] = {
	"FAN 1 Voltage",
	"FAN 2 Voltage",
	"FAN 3 Voltage",
};

enum {
	INPUT_REPORT_ID_FAN_CONFIG = 0x61,
	INPUT_REPORT_ID_FAN_STATUS = 0x67,
};

enum {
	FAN_STATUS_REPORT_SPEED = 0x02,
	FAN_STATUS_REPORT_VOLTAGE = 0x04,
};

enum {
	FAN_TYPE_NONE = 0,
	FAN_TYPE_DC = 1,
	FAN_TYPE_PWM = 2,
};

struct unknown_static_data {
	 
	u8 unknown1[14];
} __packed;

 
struct fan_config_report {
	 
	u8 report_id;
	 
	u8 magic;
	struct unknown_static_data unknown_data;
	 
	u8 fan_type[FAN_CHANNELS_MAX];
} __packed;

 
struct fan_status_report {
	 
	u8 report_id;
	 
	u8 type;
	struct unknown_static_data unknown_data;
	 
	u8 fan_type[FAN_CHANNELS_MAX];

	union {
		 
		struct {
			 
			__le16 fan_rpm[FAN_CHANNELS_MAX];
			 
			u8 duty_percent[FAN_CHANNELS_MAX];
			 
			u8 duty_percent_dup[FAN_CHANNELS_MAX];
			 
			u8 noise_db;
		} __packed fan_speed;
		 
		struct {
			 
			__le16 fan_in[FAN_CHANNELS_MAX];
			 
			__le16 fan_current[FAN_CHANNELS_MAX];
		} __packed fan_voltage;
	} __packed;
} __packed;

#define OUTPUT_REPORT_SIZE 64

enum {
	OUTPUT_REPORT_ID_INIT_COMMAND = 0x60,
	OUTPUT_REPORT_ID_SET_FAN_SPEED = 0x62,
};

enum {
	INIT_COMMAND_SET_UPDATE_INTERVAL = 0x02,
	INIT_COMMAND_DETECT_FANS = 0x03,
};

 
struct set_fan_speed_report {
	 
	u8 report_id;
	 
	u8 magic;
	 
	u8 channel_bit_mask;
	 
	u8 duty_percent[FAN_CHANNELS_MAX];
} __packed;

struct drvdata {
	struct hid_device *hid;
	struct device *hwmon;

	u8 fan_duty_percent[FAN_CHANNELS];
	u16 fan_rpm[FAN_CHANNELS];
	bool pwm_status_received;

	u16 fan_in[FAN_CHANNELS];
	u16 fan_curr[FAN_CHANNELS];
	bool voltage_status_received;

	u8 fan_type[FAN_CHANNELS];
	bool fan_config_received;

	 
	wait_queue_head_t wq;
	 
	struct mutex mutex;
	long update_interval;
	u8 output_buffer[OUTPUT_REPORT_SIZE];
};

static long scale_pwm_value(long val, long orig_max, long new_max)
{
	if (val <= 0)
		return 0;

	 
	return max(1L, DIV_ROUND_CLOSEST(min(val, orig_max) * new_max, orig_max));
}

static void handle_fan_config_report(struct drvdata *drvdata, void *data, int size)
{
	struct fan_config_report *report = data;
	int i;

	if (size < sizeof(struct fan_config_report))
		return;

	if (report->magic != 0x03)
		return;

	spin_lock(&drvdata->wq.lock);

	for (i = 0; i < FAN_CHANNELS; i++)
		drvdata->fan_type[i] = report->fan_type[i];

	drvdata->fan_config_received = true;
	wake_up_all_locked(&drvdata->wq);
	spin_unlock(&drvdata->wq.lock);
}

static void handle_fan_status_report(struct drvdata *drvdata, void *data, int size)
{
	struct fan_status_report *report = data;
	int i;

	if (size < sizeof(struct fan_status_report))
		return;

	spin_lock(&drvdata->wq.lock);

	 
	if (!drvdata->fan_config_received) {
		spin_unlock(&drvdata->wq.lock);
		return;
	}

	for (i = 0; i < FAN_CHANNELS; i++) {
		if (drvdata->fan_type[i] == report->fan_type[i])
			continue;

		 
		hid_warn_once(drvdata->hid,
			      "Fan %d type changed unexpectedly from %d to %d",
			      i, drvdata->fan_type[i], report->fan_type[i]);
		drvdata->fan_type[i] = report->fan_type[i];
	}

	switch (report->type) {
	case FAN_STATUS_REPORT_SPEED:
		for (i = 0; i < FAN_CHANNELS; i++) {
			drvdata->fan_rpm[i] =
				get_unaligned_le16(&report->fan_speed.fan_rpm[i]);
			drvdata->fan_duty_percent[i] =
				report->fan_speed.duty_percent[i];
		}

		drvdata->pwm_status_received = true;
		wake_up_all_locked(&drvdata->wq);
		break;

	case FAN_STATUS_REPORT_VOLTAGE:
		for (i = 0; i < FAN_CHANNELS; i++) {
			drvdata->fan_in[i] =
				get_unaligned_le16(&report->fan_voltage.fan_in[i]);
			drvdata->fan_curr[i] =
				get_unaligned_le16(&report->fan_voltage.fan_current[i]);
		}

		drvdata->voltage_status_received = true;
		wake_up_all_locked(&drvdata->wq);
		break;
	}

	spin_unlock(&drvdata->wq.lock);
}

static umode_t nzxt_smart2_hwmon_is_visible(const void *data,
					    enum hwmon_sensor_types type,
					    u32 attr, int channel)
{
	switch (type) {
	case hwmon_pwm:
		switch (attr) {
		case hwmon_pwm_input:
		case hwmon_pwm_enable:
			return 0644;

		default:
			return 0444;
		}

	case hwmon_chip:
		switch (attr) {
		case hwmon_chip_update_interval:
			return 0644;

		default:
			return 0444;
		}

	default:
		return 0444;
	}
}

static int nzxt_smart2_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
				  u32 attr, int channel, long *val)
{
	struct drvdata *drvdata = dev_get_drvdata(dev);
	int res = -EINVAL;

	if (type == hwmon_chip) {
		switch (attr) {
		case hwmon_chip_update_interval:
			*val = drvdata->update_interval;
			return 0;

		default:
			return -EINVAL;
		}
	}

	spin_lock_irq(&drvdata->wq.lock);

	switch (type) {
	case hwmon_pwm:
		 
		switch (attr) {
		case hwmon_pwm_enable:
			res = wait_event_interruptible_locked_irq(drvdata->wq,
								  drvdata->fan_config_received);
			if (res)
				goto unlock;

			*val = drvdata->fan_type[channel] != FAN_TYPE_NONE;
			break;

		case hwmon_pwm_mode:
			res = wait_event_interruptible_locked_irq(drvdata->wq,
								  drvdata->fan_config_received);
			if (res)
				goto unlock;

			*val = drvdata->fan_type[channel] == FAN_TYPE_PWM;
			break;

		case hwmon_pwm_input:
			res = wait_event_interruptible_locked_irq(drvdata->wq,
								  drvdata->pwm_status_received);
			if (res)
				goto unlock;

			*val = scale_pwm_value(drvdata->fan_duty_percent[channel],
					       100, 255);
			break;
		}
		break;

	case hwmon_fan:
		 
		if (attr == hwmon_fan_input) {
			res = wait_event_interruptible_locked_irq(drvdata->wq,
								  drvdata->pwm_status_received);
			if (res)
				goto unlock;

			*val = drvdata->fan_rpm[channel];
		}
		break;

	case hwmon_in:
		if (attr == hwmon_in_input) {
			res = wait_event_interruptible_locked_irq(drvdata->wq,
								  drvdata->voltage_status_received);
			if (res)
				goto unlock;

			*val = drvdata->fan_in[channel];
		}
		break;

	case hwmon_curr:
		if (attr == hwmon_curr_input) {
			res = wait_event_interruptible_locked_irq(drvdata->wq,
								  drvdata->voltage_status_received);
			if (res)
				goto unlock;

			*val = drvdata->fan_curr[channel];
		}
		break;

	default:
		break;
	}

unlock:
	spin_unlock_irq(&drvdata->wq.lock);
	return res;
}

static int send_output_report(struct drvdata *drvdata, const void *data,
			      size_t data_size)
{
	int ret;

	if (data_size > sizeof(drvdata->output_buffer))
		return -EINVAL;

	memcpy(drvdata->output_buffer, data, data_size);

	if (data_size < sizeof(drvdata->output_buffer))
		memset(drvdata->output_buffer + data_size, 0,
		       sizeof(drvdata->output_buffer) - data_size);

	ret = hid_hw_output_report(drvdata->hid, drvdata->output_buffer,
				   sizeof(drvdata->output_buffer));
	return ret < 0 ? ret : 0;
}

static int set_pwm(struct drvdata *drvdata, int channel, long val)
{
	int ret;
	u8 duty_percent = scale_pwm_value(val, 255, 100);

	struct set_fan_speed_report report = {
		.report_id = OUTPUT_REPORT_ID_SET_FAN_SPEED,
		.magic = 1,
		.channel_bit_mask = 1 << channel
	};

	ret = mutex_lock_interruptible(&drvdata->mutex);
	if (ret)
		return ret;

	report.duty_percent[channel] = duty_percent;
	ret = send_output_report(drvdata, &report, sizeof(report));
	if (ret)
		goto unlock;

	 
	spin_lock_bh(&drvdata->wq.lock);
	drvdata->fan_duty_percent[channel] = duty_percent;
	spin_unlock_bh(&drvdata->wq.lock);

unlock:
	mutex_unlock(&drvdata->mutex);
	return ret;
}

 
static int set_pwm_enable(struct drvdata *drvdata, int channel, long val)
{
	long expected_val;
	int res;

	spin_lock_irq(&drvdata->wq.lock);

	res = wait_event_interruptible_locked_irq(drvdata->wq,
						  drvdata->fan_config_received);
	if (res) {
		spin_unlock_irq(&drvdata->wq.lock);
		return res;
	}

	expected_val = drvdata->fan_type[channel] != FAN_TYPE_NONE;

	spin_unlock_irq(&drvdata->wq.lock);

	return (val == expected_val) ? 0 : -EOPNOTSUPP;
}

 
static u8 update_interval_to_control_byte(long interval)
{
	if (interval <= 250)
		return 0;

	return clamp_val(1 + DIV_ROUND_CLOSEST(interval - 488, 256), 0, 255);
}

static long control_byte_to_update_interval(u8 control_byte)
{
	if (control_byte == 0)
		return 250;

	return 488 + (control_byte - 1) * 256;
}

static int set_update_interval(struct drvdata *drvdata, long val)
{
	u8 control = update_interval_to_control_byte(val);
	u8 report[] = {
		OUTPUT_REPORT_ID_INIT_COMMAND,
		INIT_COMMAND_SET_UPDATE_INTERVAL,
		0x01,
		0xe8,
		control,
		0x01,
		0xe8,
		control,
	};
	int ret;

	ret = send_output_report(drvdata, report, sizeof(report));
	if (ret)
		return ret;

	drvdata->update_interval = control_byte_to_update_interval(control);
	return 0;
}

static int init_device(struct drvdata *drvdata, long update_interval)
{
	int ret;
	static const u8 detect_fans_report[] = {
		OUTPUT_REPORT_ID_INIT_COMMAND,
		INIT_COMMAND_DETECT_FANS,
	};

	ret = send_output_report(drvdata, detect_fans_report,
				 sizeof(detect_fans_report));
	if (ret)
		return ret;

	return set_update_interval(drvdata, update_interval);
}

static int nzxt_smart2_hwmon_write(struct device *dev,
				   enum hwmon_sensor_types type, u32 attr,
				   int channel, long val)
{
	struct drvdata *drvdata = dev_get_drvdata(dev);
	int ret;

	switch (type) {
	case hwmon_pwm:
		switch (attr) {
		case hwmon_pwm_enable:
			return set_pwm_enable(drvdata, channel, val);

		case hwmon_pwm_input:
			return set_pwm(drvdata, channel, val);

		default:
			return -EINVAL;
		}

	case hwmon_chip:
		switch (attr) {
		case hwmon_chip_update_interval:
			ret = mutex_lock_interruptible(&drvdata->mutex);
			if (ret)
				return ret;

			ret = set_update_interval(drvdata, val);

			mutex_unlock(&drvdata->mutex);
			return ret;

		default:
			return -EINVAL;
		}

	default:
		return -EINVAL;
	}
}

static int nzxt_smart2_hwmon_read_string(struct device *dev,
					 enum hwmon_sensor_types type, u32 attr,
					 int channel, const char **str)
{
	switch (type) {
	case hwmon_fan:
		*str = fan_label[channel];
		return 0;
	case hwmon_curr:
		*str = curr_label[channel];
		return 0;
	case hwmon_in:
		*str = in_label[channel];
		return 0;
	default:
		return -EINVAL;
	}
}

static const struct hwmon_ops nzxt_smart2_hwmon_ops = {
	.is_visible = nzxt_smart2_hwmon_is_visible,
	.read = nzxt_smart2_hwmon_read,
	.read_string = nzxt_smart2_hwmon_read_string,
	.write = nzxt_smart2_hwmon_write,
};

static const struct hwmon_channel_info * const nzxt_smart2_channel_info[] = {
	HWMON_CHANNEL_INFO(fan, HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL,
			   HWMON_F_INPUT | HWMON_F_LABEL),
	HWMON_CHANNEL_INFO(pwm, HWMON_PWM_INPUT | HWMON_PWM_MODE | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_MODE | HWMON_PWM_ENABLE,
			   HWMON_PWM_INPUT | HWMON_PWM_MODE | HWMON_PWM_ENABLE),
	HWMON_CHANNEL_INFO(in, HWMON_I_INPUT | HWMON_I_LABEL,
			   HWMON_I_INPUT | HWMON_I_LABEL,
			   HWMON_I_INPUT | HWMON_I_LABEL),
	HWMON_CHANNEL_INFO(curr, HWMON_C_INPUT | HWMON_C_LABEL,
			   HWMON_C_INPUT | HWMON_C_LABEL,
			   HWMON_C_INPUT | HWMON_C_LABEL),
	HWMON_CHANNEL_INFO(chip, HWMON_C_UPDATE_INTERVAL),
	NULL
};

static const struct hwmon_chip_info nzxt_smart2_chip_info = {
	.ops = &nzxt_smart2_hwmon_ops,
	.info = nzxt_smart2_channel_info,
};

static int nzxt_smart2_hid_raw_event(struct hid_device *hdev,
				     struct hid_report *report, u8 *data, int size)
{
	struct drvdata *drvdata = hid_get_drvdata(hdev);
	u8 report_id = *data;

	switch (report_id) {
	case INPUT_REPORT_ID_FAN_CONFIG:
		handle_fan_config_report(drvdata, data, size);
		break;

	case INPUT_REPORT_ID_FAN_STATUS:
		handle_fan_status_report(drvdata, data, size);
		break;
	}

	return 0;
}

static int __maybe_unused nzxt_smart2_hid_reset_resume(struct hid_device *hdev)
{
	struct drvdata *drvdata = hid_get_drvdata(hdev);

	 
	spin_lock_bh(&drvdata->wq.lock);
	drvdata->fan_config_received = false;
	drvdata->pwm_status_received = false;
	drvdata->voltage_status_received = false;
	spin_unlock_bh(&drvdata->wq.lock);

	return init_device(drvdata, drvdata->update_interval);
}

static void mutex_fini(void *lock)
{
	mutex_destroy(lock);
}

static int nzxt_smart2_hid_probe(struct hid_device *hdev,
				 const struct hid_device_id *id)
{
	struct drvdata *drvdata;
	int ret;

	drvdata = devm_kzalloc(&hdev->dev, sizeof(struct drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->hid = hdev;
	hid_set_drvdata(hdev, drvdata);

	init_waitqueue_head(&drvdata->wq);

	mutex_init(&drvdata->mutex);
	ret = devm_add_action_or_reset(&hdev->dev, mutex_fini, &drvdata->mutex);
	if (ret)
		return ret;

	ret = hid_parse(hdev);
	if (ret)
		return ret;

	ret = hid_hw_start(hdev, HID_CONNECT_HIDRAW);
	if (ret)
		return ret;

	ret = hid_hw_open(hdev);
	if (ret)
		goto out_hw_stop;

	hid_device_io_start(hdev);

	init_device(drvdata, UPDATE_INTERVAL_DEFAULT_MS);

	drvdata->hwmon =
		hwmon_device_register_with_info(&hdev->dev, "nzxtsmart2", drvdata,
						&nzxt_smart2_chip_info, NULL);
	if (IS_ERR(drvdata->hwmon)) {
		ret = PTR_ERR(drvdata->hwmon);
		goto out_hw_close;
	}

	return 0;

out_hw_close:
	hid_hw_close(hdev);

out_hw_stop:
	hid_hw_stop(hdev);
	return ret;
}

static void nzxt_smart2_hid_remove(struct hid_device *hdev)
{
	struct drvdata *drvdata = hid_get_drvdata(hdev);

	hwmon_device_unregister(drvdata->hwmon);

	hid_hw_close(hdev);
	hid_hw_stop(hdev);
}

static const struct hid_device_id nzxt_smart2_hid_id_table[] = {
	{ HID_USB_DEVICE(0x1e71, 0x2006) },  
	{ HID_USB_DEVICE(0x1e71, 0x200d) },  
	{ HID_USB_DEVICE(0x1e71, 0x200f) },  
	{ HID_USB_DEVICE(0x1e71, 0x2009) },  
	{ HID_USB_DEVICE(0x1e71, 0x200e) },  
	{ HID_USB_DEVICE(0x1e71, 0x2010) },  
	{ HID_USB_DEVICE(0x1e71, 0x2011) },  
	{ HID_USB_DEVICE(0x1e71, 0x2019) },  
	{},
};

static struct hid_driver nzxt_smart2_hid_driver = {
	.name = "nzxt-smart2",
	.id_table = nzxt_smart2_hid_id_table,
	.probe = nzxt_smart2_hid_probe,
	.remove = nzxt_smart2_hid_remove,
	.raw_event = nzxt_smart2_hid_raw_event,
#ifdef CONFIG_PM
	.reset_resume = nzxt_smart2_hid_reset_resume,
#endif
};

static int __init nzxt_smart2_init(void)
{
	return hid_register_driver(&nzxt_smart2_hid_driver);
}

static void __exit nzxt_smart2_exit(void)
{
	hid_unregister_driver(&nzxt_smart2_hid_driver);
}

MODULE_DEVICE_TABLE(hid, nzxt_smart2_hid_id_table);
MODULE_AUTHOR("Aleksandr Mezin <mezin.alexander@gmail.com>");
MODULE_DESCRIPTION("Driver for NZXT RGB & Fan Controller/Smart Device V2");
MODULE_LICENSE("GPL");

 
late_initcall(nzxt_smart2_init);
module_exit(nzxt_smart2_exit);
