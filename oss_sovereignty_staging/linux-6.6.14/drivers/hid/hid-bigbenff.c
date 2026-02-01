

 

#include <linux/input.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/leds.h>
#include <linux/hid.h>

#include "hid-ids.h"


 

#define PID0902_RDESC_ORIG_SIZE 137

 
static __u8 pid0902_rdesc_fixed[] = {
	0x05, 0x01,         
	0x09, 0x05,         
	0xA1, 0x01,         
	0x15, 0x00,         
	0x25, 0x01,         
	0x35, 0x00,         
	0x45, 0x01,         
	0x75, 0x01,         
	0x95, 0x0D,         
	0x05, 0x09,         
	0x09, 0x05,         
	0x09, 0x01,         
	0x09, 0x02,         
	0x09, 0x04,         
	0x09, 0x07,         
	0x09, 0x08,         
	0x09, 0x09,         
	0x09, 0x0A,         
	0x09, 0x0B,         
	0x09, 0x0C,         
	0x09, 0x0E,         
	0x09, 0x0F,         
	0x09, 0x0D,         
	0x81, 0x02,         
	0x75, 0x01,         
	0x95, 0x03,         
	0x81, 0x01,         
	0x05, 0x01,         
	0x25, 0x07,         
	0x46, 0x3B, 0x01,   
	0x75, 0x04,         
	0x95, 0x01,         
	0x65, 0x14,         
	0x09, 0x39,         
	0x81, 0x42,         
	0x65, 0x00,         
	0x95, 0x01,         
	0x81, 0x01,         
	0x26, 0xFF, 0x00,   
	0x46, 0xFF, 0x00,   
	0x09, 0x30,         
	0x09, 0x31,         
	0x09, 0x33,         
	0x09, 0x34,         
	0x75, 0x08,         
	0x95, 0x04,         
	0x81, 0x02,         
	0x95, 0x0A,         
	0x81, 0x01,         
	0x05, 0x01,         
	0x26, 0xFF, 0x00,   
	0x46, 0xFF, 0x00,   
	0x09, 0x32,         
	0x09, 0x35,         
	0x95, 0x02,         
	0x81, 0x02,         
	0x95, 0x08,         
	0x81, 0x01,         
	0x06, 0x00, 0xFF,   
	0xB1, 0x02,         
	0x0A, 0x21, 0x26,   
	0x95, 0x08,         
	0x91, 0x02,         
	0x0A, 0x21, 0x26,   
	0x95, 0x08,         
	0x81, 0x02,         
	0xC0,               
};

#define NUM_LEDS 4

struct bigben_device {
	struct hid_device *hid;
	struct hid_report *report;
	spinlock_t lock;
	bool removed;
	u8 led_state;          
	u8 right_motor_on;     
	u8 left_motor_force;   
	struct led_classdev *leds[NUM_LEDS];
	bool work_led;
	bool work_ff;
	struct work_struct worker;
};

static inline void bigben_schedule_work(struct bigben_device *bigben)
{
	unsigned long flags;

	spin_lock_irqsave(&bigben->lock, flags);
	if (!bigben->removed)
		schedule_work(&bigben->worker);
	spin_unlock_irqrestore(&bigben->lock, flags);
}

static void bigben_worker(struct work_struct *work)
{
	struct bigben_device *bigben = container_of(work,
		struct bigben_device, worker);
	struct hid_field *report_field = bigben->report->field[0];
	bool do_work_led = false;
	bool do_work_ff = false;
	u8 *buf;
	u32 len;
	unsigned long flags;

	buf = hid_alloc_report_buf(bigben->report, GFP_KERNEL);
	if (!buf)
		return;

	len = hid_report_len(bigben->report);

	 
	spin_lock_irqsave(&bigben->lock, flags);

	if (bigben->work_led) {
		bigben->work_led = false;
		do_work_led = true;
		report_field->value[0] = 0x01;  
		report_field->value[1] = 0x08;  
		report_field->value[2] = bigben->led_state;
		report_field->value[3] = 0x00;  
		report_field->value[4] = 0x00;  
		report_field->value[5] = 0x00;  
		report_field->value[6] = 0x00;  
		report_field->value[7] = 0x00;  
		hid_output_report(bigben->report, buf);
	}

	spin_unlock_irqrestore(&bigben->lock, flags);

	if (do_work_led) {
		hid_hw_raw_request(bigben->hid, bigben->report->id, buf, len,
				   bigben->report->type, HID_REQ_SET_REPORT);
	}

	 
	spin_lock_irqsave(&bigben->lock, flags);

	if (bigben->work_ff) {
		bigben->work_ff = false;
		do_work_ff = true;
		report_field->value[0] = 0x02;  
		report_field->value[1] = 0x08;  
		report_field->value[2] = bigben->right_motor_on;
		report_field->value[3] = bigben->left_motor_force;
		report_field->value[4] = 0xff;  
		report_field->value[5] = 0x00;  
		report_field->value[6] = 0x00;  
		report_field->value[7] = 0x00;  
		hid_output_report(bigben->report, buf);
	}

	spin_unlock_irqrestore(&bigben->lock, flags);

	if (do_work_ff) {
		hid_hw_raw_request(bigben->hid, bigben->report->id, buf, len,
				   bigben->report->type, HID_REQ_SET_REPORT);
	}

	kfree(buf);
}

static int hid_bigben_play_effect(struct input_dev *dev, void *data,
			 struct ff_effect *effect)
{
	struct hid_device *hid = input_get_drvdata(dev);
	struct bigben_device *bigben = hid_get_drvdata(hid);
	u8 right_motor_on;
	u8 left_motor_force;
	unsigned long flags;

	if (!bigben) {
		hid_err(hid, "no device data\n");
		return 0;
	}

	if (effect->type != FF_RUMBLE)
		return 0;

	right_motor_on   = effect->u.rumble.weak_magnitude ? 1 : 0;
	left_motor_force = effect->u.rumble.strong_magnitude / 256;

	if (right_motor_on != bigben->right_motor_on ||
			left_motor_force != bigben->left_motor_force) {
		spin_lock_irqsave(&bigben->lock, flags);
		bigben->right_motor_on   = right_motor_on;
		bigben->left_motor_force = left_motor_force;
		bigben->work_ff = true;
		spin_unlock_irqrestore(&bigben->lock, flags);

		bigben_schedule_work(bigben);
	}

	return 0;
}

static void bigben_set_led(struct led_classdev *led,
	enum led_brightness value)
{
	struct device *dev = led->dev->parent;
	struct hid_device *hid = to_hid_device(dev);
	struct bigben_device *bigben = hid_get_drvdata(hid);
	int n;
	bool work;
	unsigned long flags;

	if (!bigben) {
		hid_err(hid, "no device data\n");
		return;
	}

	for (n = 0; n < NUM_LEDS; n++) {
		if (led == bigben->leds[n]) {
			spin_lock_irqsave(&bigben->lock, flags);
			if (value == LED_OFF) {
				work = (bigben->led_state & BIT(n));
				bigben->led_state &= ~BIT(n);
			} else {
				work = !(bigben->led_state & BIT(n));
				bigben->led_state |= BIT(n);
			}
			spin_unlock_irqrestore(&bigben->lock, flags);

			if (work) {
				bigben->work_led = true;
				bigben_schedule_work(bigben);
			}
			return;
		}
	}
}

static enum led_brightness bigben_get_led(struct led_classdev *led)
{
	struct device *dev = led->dev->parent;
	struct hid_device *hid = to_hid_device(dev);
	struct bigben_device *bigben = hid_get_drvdata(hid);
	int n;

	if (!bigben) {
		hid_err(hid, "no device data\n");
		return LED_OFF;
	}

	for (n = 0; n < NUM_LEDS; n++) {
		if (led == bigben->leds[n])
			return (bigben->led_state & BIT(n)) ? LED_ON : LED_OFF;
	}

	return LED_OFF;
}

static void bigben_remove(struct hid_device *hid)
{
	struct bigben_device *bigben = hid_get_drvdata(hid);
	unsigned long flags;

	spin_lock_irqsave(&bigben->lock, flags);
	bigben->removed = true;
	spin_unlock_irqrestore(&bigben->lock, flags);

	cancel_work_sync(&bigben->worker);
	hid_hw_stop(hid);
}

static int bigben_probe(struct hid_device *hid,
	const struct hid_device_id *id)
{
	struct bigben_device *bigben;
	struct hid_input *hidinput;
	struct led_classdev *led;
	char *name;
	size_t name_sz;
	int n, error;

	bigben = devm_kzalloc(&hid->dev, sizeof(*bigben), GFP_KERNEL);
	if (!bigben)
		return -ENOMEM;
	hid_set_drvdata(hid, bigben);
	bigben->hid = hid;
	bigben->removed = false;

	error = hid_parse(hid);
	if (error) {
		hid_err(hid, "parse failed\n");
		return error;
	}

	error = hid_hw_start(hid, HID_CONNECT_DEFAULT & ~HID_CONNECT_FF);
	if (error) {
		hid_err(hid, "hw start failed\n");
		return error;
	}

	bigben->report = hid_validate_values(hid, HID_OUTPUT_REPORT, 0, 0, 8);
	if (!bigben->report) {
		hid_err(hid, "no output report found\n");
		error = -ENODEV;
		goto error_hw_stop;
	}

	if (list_empty(&hid->inputs)) {
		hid_err(hid, "no inputs found\n");
		error = -ENODEV;
		goto error_hw_stop;
	}

	hidinput = list_first_entry(&hid->inputs, struct hid_input, list);
	set_bit(FF_RUMBLE, hidinput->input->ffbit);

	INIT_WORK(&bigben->worker, bigben_worker);
	spin_lock_init(&bigben->lock);

	error = input_ff_create_memless(hidinput->input, NULL,
		hid_bigben_play_effect);
	if (error)
		goto error_hw_stop;

	name_sz = strlen(dev_name(&hid->dev)) + strlen(":red:bigben#") + 1;

	for (n = 0; n < NUM_LEDS; n++) {
		led = devm_kzalloc(
			&hid->dev,
			sizeof(struct led_classdev) + name_sz,
			GFP_KERNEL
		);
		if (!led) {
			error = -ENOMEM;
			goto error_hw_stop;
		}
		name = (void *)(&led[1]);
		snprintf(name, name_sz,
			"%s:red:bigben%d",
			dev_name(&hid->dev), n + 1
		);
		led->name = name;
		led->brightness = (n == 0) ? LED_ON : LED_OFF;
		led->max_brightness = 1;
		led->brightness_get = bigben_get_led;
		led->brightness_set = bigben_set_led;
		bigben->leds[n] = led;
		error = devm_led_classdev_register(&hid->dev, led);
		if (error)
			goto error_hw_stop;
	}

	 
	bigben->led_state = BIT(0);
	bigben->right_motor_on = 0;
	bigben->left_motor_force = 0;
	bigben->work_led = true;
	bigben->work_ff = true;
	bigben_schedule_work(bigben);

	hid_info(hid, "LED and force feedback support for BigBen gamepad\n");

	return 0;

error_hw_stop:
	hid_hw_stop(hid);
	return error;
}

static __u8 *bigben_report_fixup(struct hid_device *hid, __u8 *rdesc,
	unsigned int *rsize)
{
	if (*rsize == PID0902_RDESC_ORIG_SIZE) {
		rdesc = pid0902_rdesc_fixed;
		*rsize = sizeof(pid0902_rdesc_fixed);
	} else
		hid_warn(hid, "unexpected rdesc, please submit for review\n");
	return rdesc;
}

static const struct hid_device_id bigben_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_BIGBEN, USB_DEVICE_ID_BIGBEN_PS3OFMINIPAD) },
	{ }
};
MODULE_DEVICE_TABLE(hid, bigben_devices);

static struct hid_driver bigben_driver = {
	.name = "bigben",
	.id_table = bigben_devices,
	.probe = bigben_probe,
	.report_fixup = bigben_report_fixup,
	.remove = bigben_remove,
};
module_hid_driver(bigben_driver);

MODULE_LICENSE("GPL");
