
 

 

 

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/input.h>
#include <linux/spinlock.h>
#include "hid-wiimote.h"

 

static const __u16 wiimod_keys_map[] = {
	KEY_LEFT,	 
	KEY_RIGHT,	 
	KEY_UP,		 
	KEY_DOWN,	 
	KEY_NEXT,	 
	KEY_PREVIOUS,	 
	BTN_1,		 
	BTN_2,		 
	BTN_A,		 
	BTN_B,		 
	BTN_MODE,	 
};

static void wiimod_keys_in_keys(struct wiimote_data *wdata, const __u8 *keys)
{
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_LEFT],
							!!(keys[0] & 0x01));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_RIGHT],
							!!(keys[0] & 0x02));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_DOWN],
							!!(keys[0] & 0x04));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_UP],
							!!(keys[0] & 0x08));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_PLUS],
							!!(keys[0] & 0x10));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_TWO],
							!!(keys[1] & 0x01));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_ONE],
							!!(keys[1] & 0x02));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_B],
							!!(keys[1] & 0x04));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_A],
							!!(keys[1] & 0x08));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_MINUS],
							!!(keys[1] & 0x10));
	input_report_key(wdata->input, wiimod_keys_map[WIIPROTO_KEY_HOME],
							!!(keys[1] & 0x80));
	input_sync(wdata->input);
}

static int wiimod_keys_probe(const struct wiimod_ops *ops,
			     struct wiimote_data *wdata)
{
	unsigned int i;

	set_bit(EV_KEY, wdata->input->evbit);
	for (i = 0; i < WIIPROTO_KEY_COUNT; ++i)
		set_bit(wiimod_keys_map[i], wdata->input->keybit);

	return 0;
}

static const struct wiimod_ops wiimod_keys = {
	.flags = WIIMOD_FLAG_INPUT,
	.arg = 0,
	.probe = wiimod_keys_probe,
	.remove = NULL,
	.in_keys = wiimod_keys_in_keys,
};

 

 
static void wiimod_rumble_worker(struct work_struct *work)
{
	struct wiimote_data *wdata = container_of(work, struct wiimote_data,
						  rumble_worker);

	spin_lock_irq(&wdata->state.lock);
	wiiproto_req_rumble(wdata, wdata->state.cache_rumble);
	spin_unlock_irq(&wdata->state.lock);
}

static int wiimod_rumble_play(struct input_dev *dev, void *data,
			      struct ff_effect *eff)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	__u8 value;

	 

	if (eff->u.rumble.strong_magnitude || eff->u.rumble.weak_magnitude)
		value = 1;
	else
		value = 0;

	 
	wdata->state.cache_rumble = value;
	schedule_work(&wdata->rumble_worker);

	return 0;
}

static int wiimod_rumble_probe(const struct wiimod_ops *ops,
			       struct wiimote_data *wdata)
{
	INIT_WORK(&wdata->rumble_worker, wiimod_rumble_worker);

	set_bit(FF_RUMBLE, wdata->input->ffbit);
	if (input_ff_create_memless(wdata->input, NULL, wiimod_rumble_play))
		return -ENOMEM;

	return 0;
}

static void wiimod_rumble_remove(const struct wiimod_ops *ops,
				 struct wiimote_data *wdata)
{
	unsigned long flags;

	cancel_work_sync(&wdata->rumble_worker);

	spin_lock_irqsave(&wdata->state.lock, flags);
	wiiproto_req_rumble(wdata, 0);
	spin_unlock_irqrestore(&wdata->state.lock, flags);
}

static const struct wiimod_ops wiimod_rumble = {
	.flags = WIIMOD_FLAG_INPUT,
	.arg = 0,
	.probe = wiimod_rumble_probe,
	.remove = wiimod_rumble_remove,
};

 

static enum power_supply_property wiimod_battery_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_SCOPE,
};

static int wiimod_battery_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct wiimote_data *wdata = power_supply_get_drvdata(psy);
	int ret = 0, state;
	unsigned long flags;

	if (psp == POWER_SUPPLY_PROP_SCOPE) {
		val->intval = POWER_SUPPLY_SCOPE_DEVICE;
		return 0;
	} else if (psp != POWER_SUPPLY_PROP_CAPACITY) {
		return -EINVAL;
	}

	ret = wiimote_cmd_acquire(wdata);
	if (ret)
		return ret;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wiimote_cmd_set(wdata, WIIPROTO_REQ_SREQ, 0);
	wiiproto_req_status(wdata);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	wiimote_cmd_wait(wdata);
	wiimote_cmd_release(wdata);

	spin_lock_irqsave(&wdata->state.lock, flags);
	state = wdata->state.cmd_battery;
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	val->intval = state * 100 / 255;
	return ret;
}

static int wiimod_battery_probe(const struct wiimod_ops *ops,
				struct wiimote_data *wdata)
{
	struct power_supply_config psy_cfg = { .drv_data = wdata, };
	int ret;

	wdata->battery_desc.properties = wiimod_battery_props;
	wdata->battery_desc.num_properties = ARRAY_SIZE(wiimod_battery_props);
	wdata->battery_desc.get_property = wiimod_battery_get_property;
	wdata->battery_desc.type = POWER_SUPPLY_TYPE_BATTERY;
	wdata->battery_desc.use_for_apm = 0;
	wdata->battery_desc.name = kasprintf(GFP_KERNEL, "wiimote_battery_%s",
					     wdata->hdev->uniq);
	if (!wdata->battery_desc.name)
		return -ENOMEM;

	wdata->battery = power_supply_register(&wdata->hdev->dev,
					       &wdata->battery_desc,
					       &psy_cfg);
	if (IS_ERR(wdata->battery)) {
		hid_err(wdata->hdev, "cannot register battery device\n");
		ret = PTR_ERR(wdata->battery);
		goto err_free;
	}

	power_supply_powers(wdata->battery, &wdata->hdev->dev);
	return 0;

err_free:
	kfree(wdata->battery_desc.name);
	wdata->battery_desc.name = NULL;
	return ret;
}

static void wiimod_battery_remove(const struct wiimod_ops *ops,
				  struct wiimote_data *wdata)
{
	if (!wdata->battery_desc.name)
		return;

	power_supply_unregister(wdata->battery);
	kfree(wdata->battery_desc.name);
	wdata->battery_desc.name = NULL;
}

static const struct wiimod_ops wiimod_battery = {
	.flags = 0,
	.arg = 0,
	.probe = wiimod_battery_probe,
	.remove = wiimod_battery_remove,
};

 

static enum led_brightness wiimod_led_get(struct led_classdev *led_dev)
{
	struct device *dev = led_dev->dev->parent;
	struct wiimote_data *wdata = dev_to_wii(dev);
	int i;
	unsigned long flags;
	bool value = false;

	for (i = 0; i < 4; ++i) {
		if (wdata->leds[i] == led_dev) {
			spin_lock_irqsave(&wdata->state.lock, flags);
			value = wdata->state.flags & WIIPROTO_FLAG_LED(i + 1);
			spin_unlock_irqrestore(&wdata->state.lock, flags);
			break;
		}
	}

	return value ? LED_FULL : LED_OFF;
}

static void wiimod_led_set(struct led_classdev *led_dev,
			   enum led_brightness value)
{
	struct device *dev = led_dev->dev->parent;
	struct wiimote_data *wdata = dev_to_wii(dev);
	int i;
	unsigned long flags;
	__u8 state, flag;

	for (i = 0; i < 4; ++i) {
		if (wdata->leds[i] == led_dev) {
			flag = WIIPROTO_FLAG_LED(i + 1);
			spin_lock_irqsave(&wdata->state.lock, flags);
			state = wdata->state.flags;
			if (value == LED_OFF)
				wiiproto_req_leds(wdata, state & ~flag);
			else
				wiiproto_req_leds(wdata, state | flag);
			spin_unlock_irqrestore(&wdata->state.lock, flags);
			break;
		}
	}
}

static int wiimod_led_probe(const struct wiimod_ops *ops,
			    struct wiimote_data *wdata)
{
	struct device *dev = &wdata->hdev->dev;
	size_t namesz = strlen(dev_name(dev)) + 9;
	struct led_classdev *led;
	unsigned long flags;
	char *name;
	int ret;

	led = kzalloc(sizeof(struct led_classdev) + namesz, GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	name = (void*)&led[1];
	snprintf(name, namesz, "%s:blue:p%lu", dev_name(dev), ops->arg);
	led->name = name;
	led->brightness = 0;
	led->max_brightness = 1;
	led->brightness_get = wiimod_led_get;
	led->brightness_set = wiimod_led_set;

	wdata->leds[ops->arg] = led;
	ret = led_classdev_register(dev, led);
	if (ret)
		goto err_free;

	 
	if (ops->arg == 0) {
		spin_lock_irqsave(&wdata->state.lock, flags);
		wiiproto_req_leds(wdata, WIIPROTO_FLAG_LED1);
		spin_unlock_irqrestore(&wdata->state.lock, flags);
	}

	return 0;

err_free:
	wdata->leds[ops->arg] = NULL;
	kfree(led);
	return ret;
}

static void wiimod_led_remove(const struct wiimod_ops *ops,
			      struct wiimote_data *wdata)
{
	if (!wdata->leds[ops->arg])
		return;

	led_classdev_unregister(wdata->leds[ops->arg]);
	kfree(wdata->leds[ops->arg]);
	wdata->leds[ops->arg] = NULL;
}

static const struct wiimod_ops wiimod_leds[4] = {
	{
		.flags = 0,
		.arg = 0,
		.probe = wiimod_led_probe,
		.remove = wiimod_led_remove,
	},
	{
		.flags = 0,
		.arg = 1,
		.probe = wiimod_led_probe,
		.remove = wiimod_led_remove,
	},
	{
		.flags = 0,
		.arg = 2,
		.probe = wiimod_led_probe,
		.remove = wiimod_led_remove,
	},
	{
		.flags = 0,
		.arg = 3,
		.probe = wiimod_led_probe,
		.remove = wiimod_led_remove,
	},
};

 

static void wiimod_accel_in_accel(struct wiimote_data *wdata,
				  const __u8 *accel)
{
	__u16 x, y, z;

	if (!(wdata->state.flags & WIIPROTO_FLAG_ACCEL))
		return;

	 

	x = accel[2] << 2;
	y = accel[3] << 2;
	z = accel[4] << 2;

	x |= (accel[0] >> 5) & 0x3;
	y |= (accel[1] >> 4) & 0x2;
	z |= (accel[1] >> 5) & 0x2;

	input_report_abs(wdata->accel, ABS_RX, x - 0x200);
	input_report_abs(wdata->accel, ABS_RY, y - 0x200);
	input_report_abs(wdata->accel, ABS_RZ, z - 0x200);
	input_sync(wdata->accel);
}

static int wiimod_accel_open(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wiiproto_req_accel(wdata, true);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return 0;
}

static void wiimod_accel_close(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wiiproto_req_accel(wdata, false);
	spin_unlock_irqrestore(&wdata->state.lock, flags);
}

static int wiimod_accel_probe(const struct wiimod_ops *ops,
			      struct wiimote_data *wdata)
{
	int ret;

	wdata->accel = input_allocate_device();
	if (!wdata->accel)
		return -ENOMEM;

	input_set_drvdata(wdata->accel, wdata);
	wdata->accel->open = wiimod_accel_open;
	wdata->accel->close = wiimod_accel_close;
	wdata->accel->dev.parent = &wdata->hdev->dev;
	wdata->accel->id.bustype = wdata->hdev->bus;
	wdata->accel->id.vendor = wdata->hdev->vendor;
	wdata->accel->id.product = wdata->hdev->product;
	wdata->accel->id.version = wdata->hdev->version;
	wdata->accel->name = WIIMOTE_NAME " Accelerometer";

	set_bit(EV_ABS, wdata->accel->evbit);
	set_bit(ABS_RX, wdata->accel->absbit);
	set_bit(ABS_RY, wdata->accel->absbit);
	set_bit(ABS_RZ, wdata->accel->absbit);
	input_set_abs_params(wdata->accel, ABS_RX, -500, 500, 2, 4);
	input_set_abs_params(wdata->accel, ABS_RY, -500, 500, 2, 4);
	input_set_abs_params(wdata->accel, ABS_RZ, -500, 500, 2, 4);

	ret = input_register_device(wdata->accel);
	if (ret) {
		hid_err(wdata->hdev, "cannot register input device\n");
		goto err_free;
	}

	return 0;

err_free:
	input_free_device(wdata->accel);
	wdata->accel = NULL;
	return ret;
}

static void wiimod_accel_remove(const struct wiimod_ops *ops,
				struct wiimote_data *wdata)
{
	if (!wdata->accel)
		return;

	input_unregister_device(wdata->accel);
	wdata->accel = NULL;
}

static const struct wiimod_ops wiimod_accel = {
	.flags = 0,
	.arg = 0,
	.probe = wiimod_accel_probe,
	.remove = wiimod_accel_remove,
	.in_accel = wiimod_accel_in_accel,
};

 

static void wiimod_ir_in_ir(struct wiimote_data *wdata, const __u8 *ir,
			    bool packed, unsigned int id)
{
	__u16 x, y;
	__u8 xid, yid;
	bool sync = false;

	if (!(wdata->state.flags & WIIPROTO_FLAGS_IR))
		return;

	switch (id) {
	case 0:
		xid = ABS_HAT0X;
		yid = ABS_HAT0Y;
		break;
	case 1:
		xid = ABS_HAT1X;
		yid = ABS_HAT1Y;
		break;
	case 2:
		xid = ABS_HAT2X;
		yid = ABS_HAT2Y;
		break;
	case 3:
		xid = ABS_HAT3X;
		yid = ABS_HAT3Y;
		sync = true;
		break;
	default:
		return;
	}

	 

	if (packed) {
		x = ir[1] | ((ir[0] & 0x03) << 8);
		y = ir[2] | ((ir[0] & 0x0c) << 6);
	} else {
		x = ir[0] | ((ir[2] & 0x30) << 4);
		y = ir[1] | ((ir[2] & 0xc0) << 2);
	}

	input_report_abs(wdata->ir, xid, x);
	input_report_abs(wdata->ir, yid, y);

	if (sync)
		input_sync(wdata->ir);
}

static int wiimod_ir_change(struct wiimote_data *wdata, __u16 mode)
{
	int ret;
	unsigned long flags;
	__u8 format = 0;
	static const __u8 data_enable[] = { 0x01 };
	static const __u8 data_sens1[] = { 0x02, 0x00, 0x00, 0x71, 0x01,
						0x00, 0xaa, 0x00, 0x64 };
	static const __u8 data_sens2[] = { 0x63, 0x03 };
	static const __u8 data_fin[] = { 0x08 };

	spin_lock_irqsave(&wdata->state.lock, flags);

	if (mode == (wdata->state.flags & WIIPROTO_FLAGS_IR)) {
		spin_unlock_irqrestore(&wdata->state.lock, flags);
		return 0;
	}

	if (mode == 0) {
		wdata->state.flags &= ~WIIPROTO_FLAGS_IR;
		wiiproto_req_ir1(wdata, 0);
		wiiproto_req_ir2(wdata, 0);
		wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
		spin_unlock_irqrestore(&wdata->state.lock, flags);
		return 0;
	}

	spin_unlock_irqrestore(&wdata->state.lock, flags);

	ret = wiimote_cmd_acquire(wdata);
	if (ret)
		return ret;

	 
	spin_lock_irqsave(&wdata->state.lock, flags);
	wiimote_cmd_set(wdata, WIIPROTO_REQ_IR1, 0);
	wiiproto_req_ir1(wdata, 0x06);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	ret = wiimote_cmd_wait(wdata);
	if (ret)
		goto unlock;
	if (wdata->state.cmd_err) {
		ret = -EIO;
		goto unlock;
	}

	 
	spin_lock_irqsave(&wdata->state.lock, flags);
	wiimote_cmd_set(wdata, WIIPROTO_REQ_IR2, 0);
	wiiproto_req_ir2(wdata, 0x06);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	ret = wiimote_cmd_wait(wdata);
	if (ret)
		goto unlock;
	if (wdata->state.cmd_err) {
		ret = -EIO;
		goto unlock;
	}

	 
	ret = wiimote_cmd_write(wdata, 0xb00030, data_enable,
							sizeof(data_enable));
	if (ret)
		goto unlock;

	 
	ret = wiimote_cmd_write(wdata, 0xb00000, data_sens1,
							sizeof(data_sens1));
	if (ret)
		goto unlock;

	 
	ret = wiimote_cmd_write(wdata, 0xb0001a, data_sens2,
							sizeof(data_sens2));
	if (ret)
		goto unlock;

	 
	switch (mode) {
		case WIIPROTO_FLAG_IR_FULL:
			format = 5;
			break;
		case WIIPROTO_FLAG_IR_EXT:
			format = 3;
			break;
		case WIIPROTO_FLAG_IR_BASIC:
			format = 1;
			break;
	}
	ret = wiimote_cmd_write(wdata, 0xb00033, &format, sizeof(format));
	if (ret)
		goto unlock;

	 
	ret = wiimote_cmd_write(wdata, 0xb00030, data_fin, sizeof(data_fin));
	if (ret)
		goto unlock;

	 
	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags &= ~WIIPROTO_FLAGS_IR;
	wdata->state.flags |= mode & WIIPROTO_FLAGS_IR;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

unlock:
	wiimote_cmd_release(wdata);
	return ret;
}

static int wiimod_ir_open(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);

	return wiimod_ir_change(wdata, WIIPROTO_FLAG_IR_BASIC);
}

static void wiimod_ir_close(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);

	wiimod_ir_change(wdata, 0);
}

static int wiimod_ir_probe(const struct wiimod_ops *ops,
			   struct wiimote_data *wdata)
{
	int ret;

	wdata->ir = input_allocate_device();
	if (!wdata->ir)
		return -ENOMEM;

	input_set_drvdata(wdata->ir, wdata);
	wdata->ir->open = wiimod_ir_open;
	wdata->ir->close = wiimod_ir_close;
	wdata->ir->dev.parent = &wdata->hdev->dev;
	wdata->ir->id.bustype = wdata->hdev->bus;
	wdata->ir->id.vendor = wdata->hdev->vendor;
	wdata->ir->id.product = wdata->hdev->product;
	wdata->ir->id.version = wdata->hdev->version;
	wdata->ir->name = WIIMOTE_NAME " IR";

	set_bit(EV_ABS, wdata->ir->evbit);
	set_bit(ABS_HAT0X, wdata->ir->absbit);
	set_bit(ABS_HAT0Y, wdata->ir->absbit);
	set_bit(ABS_HAT1X, wdata->ir->absbit);
	set_bit(ABS_HAT1Y, wdata->ir->absbit);
	set_bit(ABS_HAT2X, wdata->ir->absbit);
	set_bit(ABS_HAT2Y, wdata->ir->absbit);
	set_bit(ABS_HAT3X, wdata->ir->absbit);
	set_bit(ABS_HAT3Y, wdata->ir->absbit);
	input_set_abs_params(wdata->ir, ABS_HAT0X, 0, 1023, 2, 4);
	input_set_abs_params(wdata->ir, ABS_HAT0Y, 0, 767, 2, 4);
	input_set_abs_params(wdata->ir, ABS_HAT1X, 0, 1023, 2, 4);
	input_set_abs_params(wdata->ir, ABS_HAT1Y, 0, 767, 2, 4);
	input_set_abs_params(wdata->ir, ABS_HAT2X, 0, 1023, 2, 4);
	input_set_abs_params(wdata->ir, ABS_HAT2Y, 0, 767, 2, 4);
	input_set_abs_params(wdata->ir, ABS_HAT3X, 0, 1023, 2, 4);
	input_set_abs_params(wdata->ir, ABS_HAT3Y, 0, 767, 2, 4);

	ret = input_register_device(wdata->ir);
	if (ret) {
		hid_err(wdata->hdev, "cannot register input device\n");
		goto err_free;
	}

	return 0;

err_free:
	input_free_device(wdata->ir);
	wdata->ir = NULL;
	return ret;
}

static void wiimod_ir_remove(const struct wiimod_ops *ops,
			     struct wiimote_data *wdata)
{
	if (!wdata->ir)
		return;

	input_unregister_device(wdata->ir);
	wdata->ir = NULL;
}

static const struct wiimod_ops wiimod_ir = {
	.flags = 0,
	.arg = 0,
	.probe = wiimod_ir_probe,
	.remove = wiimod_ir_remove,
	.in_ir = wiimod_ir_in_ir,
};

 

enum wiimod_nunchuk_keys {
	WIIMOD_NUNCHUK_KEY_C,
	WIIMOD_NUNCHUK_KEY_Z,
	WIIMOD_NUNCHUK_KEY_NUM,
};

static const __u16 wiimod_nunchuk_map[] = {
	BTN_C,		 
	BTN_Z,		 
};

static void wiimod_nunchuk_in_ext(struct wiimote_data *wdata, const __u8 *ext)
{
	__s16 x, y, z, bx, by;

	 

	bx = ext[0];
	by = ext[1];
	bx -= 128;
	by -= 128;

	x = ext[2] << 2;
	y = ext[3] << 2;
	z = ext[4] << 2;

	if (wdata->state.flags & WIIPROTO_FLAG_MP_ACTIVE) {
		x |= (ext[5] >> 3) & 0x02;
		y |= (ext[5] >> 4) & 0x02;
		z &= ~0x4;
		z |= (ext[5] >> 5) & 0x06;
	} else {
		x |= (ext[5] >> 2) & 0x03;
		y |= (ext[5] >> 4) & 0x03;
		z |= (ext[5] >> 6) & 0x03;
	}

	x -= 0x200;
	y -= 0x200;
	z -= 0x200;

	input_report_abs(wdata->extension.input, ABS_HAT0X, bx);
	input_report_abs(wdata->extension.input, ABS_HAT0Y, by);

	input_report_abs(wdata->extension.input, ABS_RX, x);
	input_report_abs(wdata->extension.input, ABS_RY, y);
	input_report_abs(wdata->extension.input, ABS_RZ, z);

	if (wdata->state.flags & WIIPROTO_FLAG_MP_ACTIVE) {
		input_report_key(wdata->extension.input,
			wiimod_nunchuk_map[WIIMOD_NUNCHUK_KEY_Z],
			!(ext[5] & 0x04));
		input_report_key(wdata->extension.input,
			wiimod_nunchuk_map[WIIMOD_NUNCHUK_KEY_C],
			!(ext[5] & 0x08));
	} else {
		input_report_key(wdata->extension.input,
			wiimod_nunchuk_map[WIIMOD_NUNCHUK_KEY_Z],
			!(ext[5] & 0x01));
		input_report_key(wdata->extension.input,
			wiimod_nunchuk_map[WIIMOD_NUNCHUK_KEY_C],
			!(ext[5] & 0x02));
	}

	input_sync(wdata->extension.input);
}

static int wiimod_nunchuk_open(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags |= WIIPROTO_FLAG_EXT_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return 0;
}

static void wiimod_nunchuk_close(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags &= ~WIIPROTO_FLAG_EXT_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);
}

static int wiimod_nunchuk_probe(const struct wiimod_ops *ops,
				struct wiimote_data *wdata)
{
	int ret, i;

	wdata->extension.input = input_allocate_device();
	if (!wdata->extension.input)
		return -ENOMEM;

	input_set_drvdata(wdata->extension.input, wdata);
	wdata->extension.input->open = wiimod_nunchuk_open;
	wdata->extension.input->close = wiimod_nunchuk_close;
	wdata->extension.input->dev.parent = &wdata->hdev->dev;
	wdata->extension.input->id.bustype = wdata->hdev->bus;
	wdata->extension.input->id.vendor = wdata->hdev->vendor;
	wdata->extension.input->id.product = wdata->hdev->product;
	wdata->extension.input->id.version = wdata->hdev->version;
	wdata->extension.input->name = WIIMOTE_NAME " Nunchuk";

	set_bit(EV_KEY, wdata->extension.input->evbit);
	for (i = 0; i < WIIMOD_NUNCHUK_KEY_NUM; ++i)
		set_bit(wiimod_nunchuk_map[i],
			wdata->extension.input->keybit);

	set_bit(EV_ABS, wdata->extension.input->evbit);
	set_bit(ABS_HAT0X, wdata->extension.input->absbit);
	set_bit(ABS_HAT0Y, wdata->extension.input->absbit);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT0X, -120, 120, 2, 4);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT0Y, -120, 120, 2, 4);
	set_bit(ABS_RX, wdata->extension.input->absbit);
	set_bit(ABS_RY, wdata->extension.input->absbit);
	set_bit(ABS_RZ, wdata->extension.input->absbit);
	input_set_abs_params(wdata->extension.input,
			     ABS_RX, -500, 500, 2, 4);
	input_set_abs_params(wdata->extension.input,
			     ABS_RY, -500, 500, 2, 4);
	input_set_abs_params(wdata->extension.input,
			     ABS_RZ, -500, 500, 2, 4);

	ret = input_register_device(wdata->extension.input);
	if (ret)
		goto err_free;

	return 0;

err_free:
	input_free_device(wdata->extension.input);
	wdata->extension.input = NULL;
	return ret;
}

static void wiimod_nunchuk_remove(const struct wiimod_ops *ops,
				  struct wiimote_data *wdata)
{
	if (!wdata->extension.input)
		return;

	input_unregister_device(wdata->extension.input);
	wdata->extension.input = NULL;
}

static const struct wiimod_ops wiimod_nunchuk = {
	.flags = 0,
	.arg = 0,
	.probe = wiimod_nunchuk_probe,
	.remove = wiimod_nunchuk_remove,
	.in_ext = wiimod_nunchuk_in_ext,
};

 

enum wiimod_classic_keys {
	WIIMOD_CLASSIC_KEY_A,
	WIIMOD_CLASSIC_KEY_B,
	WIIMOD_CLASSIC_KEY_X,
	WIIMOD_CLASSIC_KEY_Y,
	WIIMOD_CLASSIC_KEY_ZL,
	WIIMOD_CLASSIC_KEY_ZR,
	WIIMOD_CLASSIC_KEY_PLUS,
	WIIMOD_CLASSIC_KEY_MINUS,
	WIIMOD_CLASSIC_KEY_HOME,
	WIIMOD_CLASSIC_KEY_LEFT,
	WIIMOD_CLASSIC_KEY_RIGHT,
	WIIMOD_CLASSIC_KEY_UP,
	WIIMOD_CLASSIC_KEY_DOWN,
	WIIMOD_CLASSIC_KEY_LT,
	WIIMOD_CLASSIC_KEY_RT,
	WIIMOD_CLASSIC_KEY_NUM,
};

static const __u16 wiimod_classic_map[] = {
	BTN_A,		 
	BTN_B,		 
	BTN_X,		 
	BTN_Y,		 
	BTN_TL2,	 
	BTN_TR2,	 
	KEY_NEXT,	 
	KEY_PREVIOUS,	 
	BTN_MODE,	 
	KEY_LEFT,	 
	KEY_RIGHT,	 
	KEY_UP,		 
	KEY_DOWN,	 
	BTN_TL,		 
	BTN_TR,		 
};

static void wiimod_classic_in_ext(struct wiimote_data *wdata, const __u8 *ext)
{
	__s8 rx, ry, lx, ly, lt, rt;

	 

	static const s8 digital_to_analog[3] = {0x20, 0, -0x20};

	if (wdata->state.flags & WIIPROTO_FLAG_MP_ACTIVE) {
		if (wiimote_dpad_as_analog) {
			lx = digital_to_analog[1 - !(ext[4] & 0x80)
				+ !(ext[1] & 0x01)];
			ly = digital_to_analog[1 - !(ext[4] & 0x40)
				+ !(ext[0] & 0x01)];
		} else {
			lx = (ext[0] & 0x3e) - 0x20;
			ly = (ext[1] & 0x3e) - 0x20;
		}
	} else {
		if (wiimote_dpad_as_analog) {
			lx = digital_to_analog[1 - !(ext[4] & 0x80)
				+ !(ext[5] & 0x02)];
			ly = digital_to_analog[1 - !(ext[4] & 0x40)
				+ !(ext[5] & 0x01)];
		} else {
			lx = (ext[0] & 0x3f) - 0x20;
			ly = (ext[1] & 0x3f) - 0x20;
		}
	}

	rx = (ext[0] >> 3) & 0x18;
	rx |= (ext[1] >> 5) & 0x06;
	rx |= (ext[2] >> 7) & 0x01;
	ry = ext[2] & 0x1f;

	rt = ext[3] & 0x1f;
	lt = (ext[2] >> 2) & 0x18;
	lt |= (ext[3] >> 5) & 0x07;

	rx <<= 1;
	ry <<= 1;
	rt <<= 1;
	lt <<= 1;

	input_report_abs(wdata->extension.input, ABS_HAT1X, lx);
	input_report_abs(wdata->extension.input, ABS_HAT1Y, ly);
	input_report_abs(wdata->extension.input, ABS_HAT2X, rx - 0x20);
	input_report_abs(wdata->extension.input, ABS_HAT2Y, ry - 0x20);
	input_report_abs(wdata->extension.input, ABS_HAT3X, rt);
	input_report_abs(wdata->extension.input, ABS_HAT3Y, lt);

	input_report_key(wdata->extension.input,
			 wiimod_classic_map[WIIMOD_CLASSIC_KEY_LT],
			 !(ext[4] & 0x20));
	input_report_key(wdata->extension.input,
			 wiimod_classic_map[WIIMOD_CLASSIC_KEY_MINUS],
			 !(ext[4] & 0x10));
	input_report_key(wdata->extension.input,
			 wiimod_classic_map[WIIMOD_CLASSIC_KEY_HOME],
			 !(ext[4] & 0x08));
	input_report_key(wdata->extension.input,
			 wiimod_classic_map[WIIMOD_CLASSIC_KEY_PLUS],
			 !(ext[4] & 0x04));
	input_report_key(wdata->extension.input,
			 wiimod_classic_map[WIIMOD_CLASSIC_KEY_RT],
			 !(ext[4] & 0x02));
	input_report_key(wdata->extension.input,
			 wiimod_classic_map[WIIMOD_CLASSIC_KEY_ZL],
			 !(ext[5] & 0x80));
	input_report_key(wdata->extension.input,
			 wiimod_classic_map[WIIMOD_CLASSIC_KEY_B],
			 !(ext[5] & 0x40));
	input_report_key(wdata->extension.input,
			 wiimod_classic_map[WIIMOD_CLASSIC_KEY_Y],
			 !(ext[5] & 0x20));
	input_report_key(wdata->extension.input,
			 wiimod_classic_map[WIIMOD_CLASSIC_KEY_A],
			 !(ext[5] & 0x10));
	input_report_key(wdata->extension.input,
			 wiimod_classic_map[WIIMOD_CLASSIC_KEY_X],
			 !(ext[5] & 0x08));
	input_report_key(wdata->extension.input,
			 wiimod_classic_map[WIIMOD_CLASSIC_KEY_ZR],
			 !(ext[5] & 0x04));

	if (!wiimote_dpad_as_analog) {
		input_report_key(wdata->extension.input,
				 wiimod_classic_map[WIIMOD_CLASSIC_KEY_RIGHT],
				 !(ext[4] & 0x80));
		input_report_key(wdata->extension.input,
				 wiimod_classic_map[WIIMOD_CLASSIC_KEY_DOWN],
				 !(ext[4] & 0x40));

		if (wdata->state.flags & WIIPROTO_FLAG_MP_ACTIVE) {
			input_report_key(wdata->extension.input,
				 wiimod_classic_map[WIIMOD_CLASSIC_KEY_LEFT],
				 !(ext[1] & 0x01));
			input_report_key(wdata->extension.input,
				 wiimod_classic_map[WIIMOD_CLASSIC_KEY_UP],
				 !(ext[0] & 0x01));
		} else {
			input_report_key(wdata->extension.input,
				 wiimod_classic_map[WIIMOD_CLASSIC_KEY_LEFT],
				 !(ext[5] & 0x02));
			input_report_key(wdata->extension.input,
				 wiimod_classic_map[WIIMOD_CLASSIC_KEY_UP],
				 !(ext[5] & 0x01));
		}
	}

	input_sync(wdata->extension.input);
}

static int wiimod_classic_open(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags |= WIIPROTO_FLAG_EXT_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return 0;
}

static void wiimod_classic_close(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags &= ~WIIPROTO_FLAG_EXT_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);
}

static int wiimod_classic_probe(const struct wiimod_ops *ops,
				struct wiimote_data *wdata)
{
	int ret, i;

	wdata->extension.input = input_allocate_device();
	if (!wdata->extension.input)
		return -ENOMEM;

	input_set_drvdata(wdata->extension.input, wdata);
	wdata->extension.input->open = wiimod_classic_open;
	wdata->extension.input->close = wiimod_classic_close;
	wdata->extension.input->dev.parent = &wdata->hdev->dev;
	wdata->extension.input->id.bustype = wdata->hdev->bus;
	wdata->extension.input->id.vendor = wdata->hdev->vendor;
	wdata->extension.input->id.product = wdata->hdev->product;
	wdata->extension.input->id.version = wdata->hdev->version;
	wdata->extension.input->name = WIIMOTE_NAME " Classic Controller";

	set_bit(EV_KEY, wdata->extension.input->evbit);
	for (i = 0; i < WIIMOD_CLASSIC_KEY_NUM; ++i)
		set_bit(wiimod_classic_map[i],
			wdata->extension.input->keybit);

	set_bit(EV_ABS, wdata->extension.input->evbit);
	set_bit(ABS_HAT1X, wdata->extension.input->absbit);
	set_bit(ABS_HAT1Y, wdata->extension.input->absbit);
	set_bit(ABS_HAT2X, wdata->extension.input->absbit);
	set_bit(ABS_HAT2Y, wdata->extension.input->absbit);
	set_bit(ABS_HAT3X, wdata->extension.input->absbit);
	set_bit(ABS_HAT3Y, wdata->extension.input->absbit);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT1X, -30, 30, 1, 1);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT1Y, -30, 30, 1, 1);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT2X, -30, 30, 1, 1);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT2Y, -30, 30, 1, 1);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT3X, -30, 30, 1, 1);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT3Y, -30, 30, 1, 1);

	ret = input_register_device(wdata->extension.input);
	if (ret)
		goto err_free;

	return 0;

err_free:
	input_free_device(wdata->extension.input);
	wdata->extension.input = NULL;
	return ret;
}

static void wiimod_classic_remove(const struct wiimod_ops *ops,
				  struct wiimote_data *wdata)
{
	if (!wdata->extension.input)
		return;

	input_unregister_device(wdata->extension.input);
	wdata->extension.input = NULL;
}

static const struct wiimod_ops wiimod_classic = {
	.flags = 0,
	.arg = 0,
	.probe = wiimod_classic_probe,
	.remove = wiimod_classic_remove,
	.in_ext = wiimod_classic_in_ext,
};

 

static void wiimod_bboard_in_keys(struct wiimote_data *wdata, const __u8 *keys)
{
	input_report_key(wdata->extension.input, BTN_A,
			 !!(keys[1] & 0x08));
	input_sync(wdata->extension.input);
}

static void wiimod_bboard_in_ext(struct wiimote_data *wdata,
				 const __u8 *ext)
{
	__s32 val[4], tmp, div;
	unsigned int i;
	struct wiimote_state *s = &wdata->state;

	 

	val[0] = ext[0];
	val[0] <<= 8;
	val[0] |= ext[1];

	val[1] = ext[2];
	val[1] <<= 8;
	val[1] |= ext[3];

	val[2] = ext[4];
	val[2] <<= 8;
	val[2] |= ext[5];

	val[3] = ext[6];
	val[3] <<= 8;
	val[3] |= ext[7];

	 
	for (i = 0; i < 4; i++) {
		if (val[i] <= s->calib_bboard[i][0]) {
			tmp = 0;
		} else if (val[i] < s->calib_bboard[i][1]) {
			tmp = val[i] - s->calib_bboard[i][0];
			tmp *= 1700;
			div = s->calib_bboard[i][1] - s->calib_bboard[i][0];
			tmp /= div ? div : 1;
		} else {
			tmp = val[i] - s->calib_bboard[i][1];
			tmp *= 1700;
			div = s->calib_bboard[i][2] - s->calib_bboard[i][1];
			tmp /= div ? div : 1;
			tmp += 1700;
		}
		val[i] = tmp;
	}

	input_report_abs(wdata->extension.input, ABS_HAT0X, val[0]);
	input_report_abs(wdata->extension.input, ABS_HAT0Y, val[1]);
	input_report_abs(wdata->extension.input, ABS_HAT1X, val[2]);
	input_report_abs(wdata->extension.input, ABS_HAT1Y, val[3]);
	input_sync(wdata->extension.input);
}

static int wiimod_bboard_open(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags |= WIIPROTO_FLAG_EXT_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return 0;
}

static void wiimod_bboard_close(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags &= ~WIIPROTO_FLAG_EXT_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);
}

static ssize_t wiimod_bboard_calib_show(struct device *dev,
					struct device_attribute *attr,
					char *out)
{
	struct wiimote_data *wdata = dev_to_wii(dev);
	int i, j, ret;
	__u16 val;
	__u8 buf[24], offs;

	ret = wiimote_cmd_acquire(wdata);
	if (ret)
		return ret;

	ret = wiimote_cmd_read(wdata, 0xa40024, buf, 12);
	if (ret != 12) {
		wiimote_cmd_release(wdata);
		return ret < 0 ? ret : -EIO;
	}
	ret = wiimote_cmd_read(wdata, 0xa40024 + 12, buf + 12, 12);
	if (ret != 12) {
		wiimote_cmd_release(wdata);
		return ret < 0 ? ret : -EIO;
	}

	wiimote_cmd_release(wdata);

	spin_lock_irq(&wdata->state.lock);
	offs = 0;
	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 4; ++j) {
			wdata->state.calib_bboard[j][i] = buf[offs];
			wdata->state.calib_bboard[j][i] <<= 8;
			wdata->state.calib_bboard[j][i] |= buf[offs + 1];
			offs += 2;
		}
	}
	spin_unlock_irq(&wdata->state.lock);

	ret = 0;
	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 4; ++j) {
			val = wdata->state.calib_bboard[j][i];
			if (i == 2 && j == 3)
				ret += sprintf(&out[ret], "%04x\n", val);
			else
				ret += sprintf(&out[ret], "%04x:", val);
		}
	}

	return ret;
}

static DEVICE_ATTR(bboard_calib, S_IRUGO, wiimod_bboard_calib_show, NULL);

static int wiimod_bboard_probe(const struct wiimod_ops *ops,
			       struct wiimote_data *wdata)
{
	int ret, i, j;
	__u8 buf[24], offs;

	wiimote_cmd_acquire_noint(wdata);

	ret = wiimote_cmd_read(wdata, 0xa40024, buf, 12);
	if (ret != 12) {
		wiimote_cmd_release(wdata);
		return ret < 0 ? ret : -EIO;
	}
	ret = wiimote_cmd_read(wdata, 0xa40024 + 12, buf + 12, 12);
	if (ret != 12) {
		wiimote_cmd_release(wdata);
		return ret < 0 ? ret : -EIO;
	}

	wiimote_cmd_release(wdata);

	offs = 0;
	for (i = 0; i < 3; ++i) {
		for (j = 0; j < 4; ++j) {
			wdata->state.calib_bboard[j][i] = buf[offs];
			wdata->state.calib_bboard[j][i] <<= 8;
			wdata->state.calib_bboard[j][i] |= buf[offs + 1];
			offs += 2;
		}
	}

	wdata->extension.input = input_allocate_device();
	if (!wdata->extension.input)
		return -ENOMEM;

	ret = device_create_file(&wdata->hdev->dev,
				 &dev_attr_bboard_calib);
	if (ret) {
		hid_err(wdata->hdev, "cannot create sysfs attribute\n");
		goto err_free;
	}

	input_set_drvdata(wdata->extension.input, wdata);
	wdata->extension.input->open = wiimod_bboard_open;
	wdata->extension.input->close = wiimod_bboard_close;
	wdata->extension.input->dev.parent = &wdata->hdev->dev;
	wdata->extension.input->id.bustype = wdata->hdev->bus;
	wdata->extension.input->id.vendor = wdata->hdev->vendor;
	wdata->extension.input->id.product = wdata->hdev->product;
	wdata->extension.input->id.version = wdata->hdev->version;
	wdata->extension.input->name = WIIMOTE_NAME " Balance Board";

	set_bit(EV_KEY, wdata->extension.input->evbit);
	set_bit(BTN_A, wdata->extension.input->keybit);

	set_bit(EV_ABS, wdata->extension.input->evbit);
	set_bit(ABS_HAT0X, wdata->extension.input->absbit);
	set_bit(ABS_HAT0Y, wdata->extension.input->absbit);
	set_bit(ABS_HAT1X, wdata->extension.input->absbit);
	set_bit(ABS_HAT1Y, wdata->extension.input->absbit);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT0X, 0, 65535, 2, 4);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT0Y, 0, 65535, 2, 4);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT1X, 0, 65535, 2, 4);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT1Y, 0, 65535, 2, 4);

	ret = input_register_device(wdata->extension.input);
	if (ret)
		goto err_file;

	return 0;

err_file:
	device_remove_file(&wdata->hdev->dev,
			   &dev_attr_bboard_calib);
err_free:
	input_free_device(wdata->extension.input);
	wdata->extension.input = NULL;
	return ret;
}

static void wiimod_bboard_remove(const struct wiimod_ops *ops,
				 struct wiimote_data *wdata)
{
	if (!wdata->extension.input)
		return;

	input_unregister_device(wdata->extension.input);
	wdata->extension.input = NULL;
	device_remove_file(&wdata->hdev->dev,
			   &dev_attr_bboard_calib);
}

static const struct wiimod_ops wiimod_bboard = {
	.flags = WIIMOD_FLAG_EXT8,
	.arg = 0,
	.probe = wiimod_bboard_probe,
	.remove = wiimod_bboard_remove,
	.in_keys = wiimod_bboard_in_keys,
	.in_ext = wiimod_bboard_in_ext,
};

 

enum wiimod_pro_keys {
	WIIMOD_PRO_KEY_A,
	WIIMOD_PRO_KEY_B,
	WIIMOD_PRO_KEY_X,
	WIIMOD_PRO_KEY_Y,
	WIIMOD_PRO_KEY_PLUS,
	WIIMOD_PRO_KEY_MINUS,
	WIIMOD_PRO_KEY_HOME,
	WIIMOD_PRO_KEY_LEFT,
	WIIMOD_PRO_KEY_RIGHT,
	WIIMOD_PRO_KEY_UP,
	WIIMOD_PRO_KEY_DOWN,
	WIIMOD_PRO_KEY_TL,
	WIIMOD_PRO_KEY_TR,
	WIIMOD_PRO_KEY_ZL,
	WIIMOD_PRO_KEY_ZR,
	WIIMOD_PRO_KEY_THUMBL,
	WIIMOD_PRO_KEY_THUMBR,
	WIIMOD_PRO_KEY_NUM,
};

static const __u16 wiimod_pro_map[] = {
	BTN_EAST,	 
	BTN_SOUTH,	 
	BTN_NORTH,	 
	BTN_WEST,	 
	BTN_START,	 
	BTN_SELECT,	 
	BTN_MODE,	 
	BTN_DPAD_LEFT,	 
	BTN_DPAD_RIGHT,	 
	BTN_DPAD_UP,	 
	BTN_DPAD_DOWN,	 
	BTN_TL,		 
	BTN_TR,		 
	BTN_TL2,	 
	BTN_TR2,	 
	BTN_THUMBL,	 
	BTN_THUMBR,	 
};

static void wiimod_pro_in_ext(struct wiimote_data *wdata, const __u8 *ext)
{
	__s16 rx, ry, lx, ly;

	 

	lx = (ext[0] & 0xff) | ((ext[1] & 0x0f) << 8);
	rx = (ext[2] & 0xff) | ((ext[3] & 0x0f) << 8);
	ly = (ext[4] & 0xff) | ((ext[5] & 0x0f) << 8);
	ry = (ext[6] & 0xff) | ((ext[7] & 0x0f) << 8);

	 
	lx -= 0x800;
	ly = 0x800 - ly;
	rx -= 0x800;
	ry = 0x800 - ry;

	 
	if (!(wdata->state.flags & WIIPROTO_FLAG_PRO_CALIB_DONE)) {
		wdata->state.flags |= WIIPROTO_FLAG_PRO_CALIB_DONE;
		if (abs(lx) < 500)
			wdata->state.calib_pro_sticks[0] = -lx;
		if (abs(ly) < 500)
			wdata->state.calib_pro_sticks[1] = -ly;
		if (abs(rx) < 500)
			wdata->state.calib_pro_sticks[2] = -rx;
		if (abs(ry) < 500)
			wdata->state.calib_pro_sticks[3] = -ry;
	}

	 
	lx += wdata->state.calib_pro_sticks[0];
	ly += wdata->state.calib_pro_sticks[1];
	rx += wdata->state.calib_pro_sticks[2];
	ry += wdata->state.calib_pro_sticks[3];

	input_report_abs(wdata->extension.input, ABS_X, lx);
	input_report_abs(wdata->extension.input, ABS_Y, ly);
	input_report_abs(wdata->extension.input, ABS_RX, rx);
	input_report_abs(wdata->extension.input, ABS_RY, ry);

	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_RIGHT],
			 !(ext[8] & 0x80));
	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_DOWN],
			 !(ext[8] & 0x40));
	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_TL],
			 !(ext[8] & 0x20));
	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_MINUS],
			 !(ext[8] & 0x10));
	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_HOME],
			 !(ext[8] & 0x08));
	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_PLUS],
			 !(ext[8] & 0x04));
	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_TR],
			 !(ext[8] & 0x02));

	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_ZL],
			 !(ext[9] & 0x80));
	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_B],
			 !(ext[9] & 0x40));
	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_Y],
			 !(ext[9] & 0x20));
	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_A],
			 !(ext[9] & 0x10));
	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_X],
			 !(ext[9] & 0x08));
	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_ZR],
			 !(ext[9] & 0x04));
	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_LEFT],
			 !(ext[9] & 0x02));
	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_UP],
			 !(ext[9] & 0x01));

	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_THUMBL],
			 !(ext[10] & 0x02));
	input_report_key(wdata->extension.input,
			 wiimod_pro_map[WIIMOD_PRO_KEY_THUMBR],
			 !(ext[10] & 0x01));

	input_sync(wdata->extension.input);
}

static int wiimod_pro_open(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags |= WIIPROTO_FLAG_EXT_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return 0;
}

static void wiimod_pro_close(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags &= ~WIIPROTO_FLAG_EXT_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);
}

static int wiimod_pro_play(struct input_dev *dev, void *data,
			   struct ff_effect *eff)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	__u8 value;

	 

	if (eff->u.rumble.strong_magnitude || eff->u.rumble.weak_magnitude)
		value = 1;
	else
		value = 0;

	 
	wdata->state.cache_rumble = value;
	schedule_work(&wdata->rumble_worker);

	return 0;
}

static ssize_t wiimod_pro_calib_show(struct device *dev,
				     struct device_attribute *attr,
				     char *out)
{
	struct wiimote_data *wdata = dev_to_wii(dev);
	int r;

	r = 0;
	r += sprintf(&out[r], "%+06hd:", wdata->state.calib_pro_sticks[0]);
	r += sprintf(&out[r], "%+06hd ", wdata->state.calib_pro_sticks[1]);
	r += sprintf(&out[r], "%+06hd:", wdata->state.calib_pro_sticks[2]);
	r += sprintf(&out[r], "%+06hd\n", wdata->state.calib_pro_sticks[3]);

	return r;
}

static ssize_t wiimod_pro_calib_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct wiimote_data *wdata = dev_to_wii(dev);
	int r;
	s16 x1, y1, x2, y2;

	if (!strncmp(buf, "scan\n", 5)) {
		spin_lock_irq(&wdata->state.lock);
		wdata->state.flags &= ~WIIPROTO_FLAG_PRO_CALIB_DONE;
		spin_unlock_irq(&wdata->state.lock);
	} else {
		r = sscanf(buf, "%hd:%hd %hd:%hd", &x1, &y1, &x2, &y2);
		if (r != 4)
			return -EINVAL;

		spin_lock_irq(&wdata->state.lock);
		wdata->state.flags |= WIIPROTO_FLAG_PRO_CALIB_DONE;
		spin_unlock_irq(&wdata->state.lock);

		wdata->state.calib_pro_sticks[0] = x1;
		wdata->state.calib_pro_sticks[1] = y1;
		wdata->state.calib_pro_sticks[2] = x2;
		wdata->state.calib_pro_sticks[3] = y2;
	}

	return strnlen(buf, PAGE_SIZE);
}

static DEVICE_ATTR(pro_calib, S_IRUGO|S_IWUSR|S_IWGRP, wiimod_pro_calib_show,
		   wiimod_pro_calib_store);

static int wiimod_pro_probe(const struct wiimod_ops *ops,
			    struct wiimote_data *wdata)
{
	int ret, i;
	unsigned long flags;

	INIT_WORK(&wdata->rumble_worker, wiimod_rumble_worker);
	wdata->state.calib_pro_sticks[0] = 0;
	wdata->state.calib_pro_sticks[1] = 0;
	wdata->state.calib_pro_sticks[2] = 0;
	wdata->state.calib_pro_sticks[3] = 0;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags &= ~WIIPROTO_FLAG_PRO_CALIB_DONE;
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	wdata->extension.input = input_allocate_device();
	if (!wdata->extension.input)
		return -ENOMEM;

	set_bit(FF_RUMBLE, wdata->extension.input->ffbit);
	input_set_drvdata(wdata->extension.input, wdata);

	if (input_ff_create_memless(wdata->extension.input, NULL,
				    wiimod_pro_play)) {
		ret = -ENOMEM;
		goto err_free;
	}

	ret = device_create_file(&wdata->hdev->dev,
				 &dev_attr_pro_calib);
	if (ret) {
		hid_err(wdata->hdev, "cannot create sysfs attribute\n");
		goto err_free;
	}

	wdata->extension.input->open = wiimod_pro_open;
	wdata->extension.input->close = wiimod_pro_close;
	wdata->extension.input->dev.parent = &wdata->hdev->dev;
	wdata->extension.input->id.bustype = wdata->hdev->bus;
	wdata->extension.input->id.vendor = wdata->hdev->vendor;
	wdata->extension.input->id.product = wdata->hdev->product;
	wdata->extension.input->id.version = wdata->hdev->version;
	wdata->extension.input->name = WIIMOTE_NAME " Pro Controller";

	set_bit(EV_KEY, wdata->extension.input->evbit);
	for (i = 0; i < WIIMOD_PRO_KEY_NUM; ++i)
		set_bit(wiimod_pro_map[i],
			wdata->extension.input->keybit);

	set_bit(EV_ABS, wdata->extension.input->evbit);
	set_bit(ABS_X, wdata->extension.input->absbit);
	set_bit(ABS_Y, wdata->extension.input->absbit);
	set_bit(ABS_RX, wdata->extension.input->absbit);
	set_bit(ABS_RY, wdata->extension.input->absbit);
	input_set_abs_params(wdata->extension.input,
			     ABS_X, -0x400, 0x400, 4, 100);
	input_set_abs_params(wdata->extension.input,
			     ABS_Y, -0x400, 0x400, 4, 100);
	input_set_abs_params(wdata->extension.input,
			     ABS_RX, -0x400, 0x400, 4, 100);
	input_set_abs_params(wdata->extension.input,
			     ABS_RY, -0x400, 0x400, 4, 100);

	ret = input_register_device(wdata->extension.input);
	if (ret)
		goto err_file;

	return 0;

err_file:
	device_remove_file(&wdata->hdev->dev,
			   &dev_attr_pro_calib);
err_free:
	input_free_device(wdata->extension.input);
	wdata->extension.input = NULL;
	return ret;
}

static void wiimod_pro_remove(const struct wiimod_ops *ops,
			      struct wiimote_data *wdata)
{
	unsigned long flags;

	if (!wdata->extension.input)
		return;

	input_unregister_device(wdata->extension.input);
	wdata->extension.input = NULL;
	cancel_work_sync(&wdata->rumble_worker);
	device_remove_file(&wdata->hdev->dev,
			   &dev_attr_pro_calib);

	spin_lock_irqsave(&wdata->state.lock, flags);
	wiiproto_req_rumble(wdata, 0);
	spin_unlock_irqrestore(&wdata->state.lock, flags);
}

static const struct wiimod_ops wiimod_pro = {
	.flags = WIIMOD_FLAG_EXT16,
	.arg = 0,
	.probe = wiimod_pro_probe,
	.remove = wiimod_pro_remove,
	.in_ext = wiimod_pro_in_ext,
};

 

static inline void wiimod_drums_report_pressure(struct wiimote_data *wdata,
						__u8 none, __u8 which,
						__u8 pressure, __u8 onoff,
						__u8 *store, __u16 code,
						__u8 which_code)
{
	static const __u8 default_pressure = 3;

	if (!none && which == which_code) {
		*store = pressure;
		input_report_abs(wdata->extension.input, code, *store);
	} else if (onoff != !!*store) {
		*store = onoff ? default_pressure : 0;
		input_report_abs(wdata->extension.input, code, *store);
	}
}

static void wiimod_drums_in_ext(struct wiimote_data *wdata, const __u8 *ext)
{
	__u8 pressure, which, none, hhp, sx, sy;
	__u8 o, r, y, g, b, bass, bm, bp;

	 

	pressure = 7 - (ext[3] >> 5);
	which = (ext[2] >> 1) & 0x1f;
	none = !!(ext[2] & 0x40);
	hhp = !(ext[2] & 0x80);
	sx = ext[0] & 0x3f;
	sy = ext[1] & 0x3f;
	o = !(ext[5] & 0x80);
	r = !(ext[5] & 0x40);
	y = !(ext[5] & 0x20);
	g = !(ext[5] & 0x10);
	b = !(ext[5] & 0x08);
	bass = !(ext[5] & 0x04);
	bm = !(ext[4] & 0x10);
	bp = !(ext[4] & 0x04);

	if (wdata->state.flags & WIIPROTO_FLAG_MP_ACTIVE) {
		sx &= 0x3e;
		sy &= 0x3e;
	}

	wiimod_drums_report_pressure(wdata, none, which, pressure,
				     o, &wdata->state.pressure_drums[0],
				     ABS_HAT2Y, 0x0e);
	wiimod_drums_report_pressure(wdata, none, which, pressure,
				     r, &wdata->state.pressure_drums[1],
				     ABS_HAT0X, 0x19);
	wiimod_drums_report_pressure(wdata, none, which, pressure,
				     y, &wdata->state.pressure_drums[2],
				     ABS_HAT2X, 0x11);
	wiimod_drums_report_pressure(wdata, none, which, pressure,
				     g, &wdata->state.pressure_drums[3],
				     ABS_HAT1X, 0x12);
	wiimod_drums_report_pressure(wdata, none, which, pressure,
				     b, &wdata->state.pressure_drums[4],
				     ABS_HAT0Y, 0x0f);

	 
	wiimod_drums_report_pressure(wdata, none, hhp ? 0xff : which, pressure,
				     bass, &wdata->state.pressure_drums[5],
				     ABS_HAT3X, 0x1b);
	 
	wiimod_drums_report_pressure(wdata, none, hhp ? which : 0xff, pressure,
				     0, &wdata->state.pressure_drums[6],
				     ABS_HAT3Y, 0x0e);

	input_report_abs(wdata->extension.input, ABS_X, sx - 0x20);
	input_report_abs(wdata->extension.input, ABS_Y, sy - 0x20);

	input_report_key(wdata->extension.input, BTN_START, bp);
	input_report_key(wdata->extension.input, BTN_SELECT, bm);

	input_sync(wdata->extension.input);
}

static int wiimod_drums_open(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags |= WIIPROTO_FLAG_EXT_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return 0;
}

static void wiimod_drums_close(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags &= ~WIIPROTO_FLAG_EXT_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);
}

static int wiimod_drums_probe(const struct wiimod_ops *ops,
			      struct wiimote_data *wdata)
{
	int ret;

	wdata->extension.input = input_allocate_device();
	if (!wdata->extension.input)
		return -ENOMEM;

	input_set_drvdata(wdata->extension.input, wdata);
	wdata->extension.input->open = wiimod_drums_open;
	wdata->extension.input->close = wiimod_drums_close;
	wdata->extension.input->dev.parent = &wdata->hdev->dev;
	wdata->extension.input->id.bustype = wdata->hdev->bus;
	wdata->extension.input->id.vendor = wdata->hdev->vendor;
	wdata->extension.input->id.product = wdata->hdev->product;
	wdata->extension.input->id.version = wdata->hdev->version;
	wdata->extension.input->name = WIIMOTE_NAME " Drums";

	set_bit(EV_KEY, wdata->extension.input->evbit);
	set_bit(BTN_START, wdata->extension.input->keybit);
	set_bit(BTN_SELECT, wdata->extension.input->keybit);

	set_bit(EV_ABS, wdata->extension.input->evbit);
	set_bit(ABS_X, wdata->extension.input->absbit);
	set_bit(ABS_Y, wdata->extension.input->absbit);
	set_bit(ABS_HAT0X, wdata->extension.input->absbit);
	set_bit(ABS_HAT0Y, wdata->extension.input->absbit);
	set_bit(ABS_HAT1X, wdata->extension.input->absbit);
	set_bit(ABS_HAT2X, wdata->extension.input->absbit);
	set_bit(ABS_HAT2Y, wdata->extension.input->absbit);
	set_bit(ABS_HAT3X, wdata->extension.input->absbit);
	set_bit(ABS_HAT3Y, wdata->extension.input->absbit);
	input_set_abs_params(wdata->extension.input,
			     ABS_X, -32, 31, 1, 1);
	input_set_abs_params(wdata->extension.input,
			     ABS_Y, -32, 31, 1, 1);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT0X, 0, 7, 0, 0);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT0Y, 0, 7, 0, 0);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT1X, 0, 7, 0, 0);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT2X, 0, 7, 0, 0);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT2Y, 0, 7, 0, 0);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT3X, 0, 7, 0, 0);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT3Y, 0, 7, 0, 0);

	ret = input_register_device(wdata->extension.input);
	if (ret)
		goto err_free;

	return 0;

err_free:
	input_free_device(wdata->extension.input);
	wdata->extension.input = NULL;
	return ret;
}

static void wiimod_drums_remove(const struct wiimod_ops *ops,
				struct wiimote_data *wdata)
{
	if (!wdata->extension.input)
		return;

	input_unregister_device(wdata->extension.input);
	wdata->extension.input = NULL;
}

static const struct wiimod_ops wiimod_drums = {
	.flags = 0,
	.arg = 0,
	.probe = wiimod_drums_probe,
	.remove = wiimod_drums_remove,
	.in_ext = wiimod_drums_in_ext,
};

 

enum wiimod_guitar_keys {
	WIIMOD_GUITAR_KEY_G,
	WIIMOD_GUITAR_KEY_R,
	WIIMOD_GUITAR_KEY_Y,
	WIIMOD_GUITAR_KEY_B,
	WIIMOD_GUITAR_KEY_O,
	WIIMOD_GUITAR_KEY_UP,
	WIIMOD_GUITAR_KEY_DOWN,
	WIIMOD_GUITAR_KEY_PLUS,
	WIIMOD_GUITAR_KEY_MINUS,
	WIIMOD_GUITAR_KEY_NUM,
};

static const __u16 wiimod_guitar_map[] = {
	BTN_1,			 
	BTN_2,			 
	BTN_3,			 
	BTN_4,			 
	BTN_5,			 
	BTN_DPAD_UP,		 
	BTN_DPAD_DOWN,		 
	BTN_START,		 
	BTN_SELECT,		 
};

static void wiimod_guitar_in_ext(struct wiimote_data *wdata, const __u8 *ext)
{
	__u8 sx, sy, tb, wb, bd, bm, bp, bo, br, bb, bg, by, bu;

	 

	sx = ext[0] & 0x3f;
	sy = ext[1] & 0x3f;
	tb = ext[2] & 0x1f;
	wb = ext[3] & 0x1f;
	bd = !(ext[4] & 0x40);
	bm = !(ext[4] & 0x10);
	bp = !(ext[4] & 0x04);
	bo = !(ext[5] & 0x80);
	br = !(ext[5] & 0x40);
	bb = !(ext[5] & 0x20);
	bg = !(ext[5] & 0x10);
	by = !(ext[5] & 0x08);
	bu = !(ext[5] & 0x01);

	if (wdata->state.flags & WIIPROTO_FLAG_MP_ACTIVE) {
		bu = !(ext[0] & 0x01);
		sx &= 0x3e;
		sy &= 0x3e;
	}

	input_report_abs(wdata->extension.input, ABS_X, sx - 0x20);
	input_report_abs(wdata->extension.input, ABS_Y, sy - 0x20);
	input_report_abs(wdata->extension.input, ABS_HAT0X, tb);
	input_report_abs(wdata->extension.input, ABS_HAT1X, wb - 0x10);

	input_report_key(wdata->extension.input,
			 wiimod_guitar_map[WIIMOD_GUITAR_KEY_G],
			 bg);
	input_report_key(wdata->extension.input,
			 wiimod_guitar_map[WIIMOD_GUITAR_KEY_R],
			 br);
	input_report_key(wdata->extension.input,
			 wiimod_guitar_map[WIIMOD_GUITAR_KEY_Y],
			 by);
	input_report_key(wdata->extension.input,
			 wiimod_guitar_map[WIIMOD_GUITAR_KEY_B],
			 bb);
	input_report_key(wdata->extension.input,
			 wiimod_guitar_map[WIIMOD_GUITAR_KEY_O],
			 bo);
	input_report_key(wdata->extension.input,
			 wiimod_guitar_map[WIIMOD_GUITAR_KEY_UP],
			 bu);
	input_report_key(wdata->extension.input,
			 wiimod_guitar_map[WIIMOD_GUITAR_KEY_DOWN],
			 bd);
	input_report_key(wdata->extension.input,
			 wiimod_guitar_map[WIIMOD_GUITAR_KEY_PLUS],
			 bp);
	input_report_key(wdata->extension.input,
			 wiimod_guitar_map[WIIMOD_GUITAR_KEY_MINUS],
			 bm);

	input_sync(wdata->extension.input);
}

static int wiimod_guitar_open(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags |= WIIPROTO_FLAG_EXT_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return 0;
}

static void wiimod_guitar_close(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags &= ~WIIPROTO_FLAG_EXT_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);
}

static int wiimod_guitar_probe(const struct wiimod_ops *ops,
			       struct wiimote_data *wdata)
{
	int ret, i;

	wdata->extension.input = input_allocate_device();
	if (!wdata->extension.input)
		return -ENOMEM;

	input_set_drvdata(wdata->extension.input, wdata);
	wdata->extension.input->open = wiimod_guitar_open;
	wdata->extension.input->close = wiimod_guitar_close;
	wdata->extension.input->dev.parent = &wdata->hdev->dev;
	wdata->extension.input->id.bustype = wdata->hdev->bus;
	wdata->extension.input->id.vendor = wdata->hdev->vendor;
	wdata->extension.input->id.product = wdata->hdev->product;
	wdata->extension.input->id.version = wdata->hdev->version;
	wdata->extension.input->name = WIIMOTE_NAME " Guitar";

	set_bit(EV_KEY, wdata->extension.input->evbit);
	for (i = 0; i < WIIMOD_GUITAR_KEY_NUM; ++i)
		set_bit(wiimod_guitar_map[i],
			wdata->extension.input->keybit);

	set_bit(EV_ABS, wdata->extension.input->evbit);
	set_bit(ABS_X, wdata->extension.input->absbit);
	set_bit(ABS_Y, wdata->extension.input->absbit);
	set_bit(ABS_HAT0X, wdata->extension.input->absbit);
	set_bit(ABS_HAT1X, wdata->extension.input->absbit);
	input_set_abs_params(wdata->extension.input,
			     ABS_X, -32, 31, 1, 1);
	input_set_abs_params(wdata->extension.input,
			     ABS_Y, -32, 31, 1, 1);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT0X, 0, 0x1f, 1, 1);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT1X, 0, 0x0f, 1, 1);

	ret = input_register_device(wdata->extension.input);
	if (ret)
		goto err_free;

	return 0;

err_free:
	input_free_device(wdata->extension.input);
	wdata->extension.input = NULL;
	return ret;
}

static void wiimod_guitar_remove(const struct wiimod_ops *ops,
				 struct wiimote_data *wdata)
{
	if (!wdata->extension.input)
		return;

	input_unregister_device(wdata->extension.input);
	wdata->extension.input = NULL;
}

static const struct wiimod_ops wiimod_guitar = {
	.flags = 0,
	.arg = 0,
	.probe = wiimod_guitar_probe,
	.remove = wiimod_guitar_remove,
	.in_ext = wiimod_guitar_in_ext,
};

 

enum wiimod_turntable_keys {
	WIIMOD_TURNTABLE_KEY_G_RIGHT,
	WIIMOD_TURNTABLE_KEY_R_RIGHT,
	WIIMOD_TURNTABLE_KEY_B_RIGHT,
	WIIMOD_TURNTABLE_KEY_G_LEFT,
	WIIMOD_TURNTABLE_KEY_R_LEFT,
	WIIMOD_TURNTABLE_KEY_B_LEFT,
	WIIMOD_TURNTABLE_KEY_EUPHORIA,
	WIIMOD_TURNTABLE_KEY_PLUS,
	WIIMOD_TURNTABLE_KEY_MINUS,
	WIIMOD_TURNTABLE_KEY_NUM
};

static const __u16 wiimod_turntable_map[] = {
	BTN_1,			 
	BTN_2,			 
	BTN_3,			 
	BTN_4,			 
	BTN_5,			 
	BTN_6,			 
	BTN_7,			 
	BTN_START,		 
	BTN_SELECT,		 
};

static void wiimod_turntable_in_ext(struct wiimote_data *wdata, const __u8 *ext)
{
	__u8 be, cs, sx, sy, ed, rtt, rbg, rbr, rbb, ltt, lbg, lbr, lbb, bp, bm;
	 
	
	be = !(ext[5] & 0x10); 
	cs = ((ext[2] & 0x1e));
	sx = ext[0] & 0x3f;
	sy = ext[1] & 0x3f;
	ed = (ext[3] & 0xe0) >> 5;
	rtt = ((ext[2] & 0x01) << 5 | (ext[0] & 0xc0) >> 3 | (ext[1] & 0xc0) >> 5 | ( ext[2] & 0x80 ) >> 7);
	ltt = ((ext[4] & 0x01) << 5 | (ext[3] & 0x1f));
	rbg = !(ext[5] & 0x20);
	rbr = !(ext[4] & 0x02);
	rbb = !(ext[5] & 0x04);
	lbg = !(ext[5] & 0x08);
	lbb = !(ext[5] & 0x80);
	lbr = !(ext[4] & 0x20);
	bm =  !(ext[4] & 0x10);
	bp =  !(ext[4] & 0x04);

	if (wdata->state.flags & WIIPROTO_FLAG_MP_ACTIVE) {
		ltt = (ext[4] & 0x01) << 5;
		sx &= 0x3e;
		sy &= 0x3e;
	}

	input_report_abs(wdata->extension.input, ABS_X, sx);
	input_report_abs(wdata->extension.input, ABS_Y, sy);
	input_report_abs(wdata->extension.input, ABS_HAT0X, rtt);
	input_report_abs(wdata->extension.input, ABS_HAT1X, ltt);
	input_report_abs(wdata->extension.input, ABS_HAT2X, cs);
	input_report_abs(wdata->extension.input, ABS_HAT3X, ed);
	input_report_key(wdata->extension.input, 
					wiimod_turntable_map[WIIMOD_TURNTABLE_KEY_G_RIGHT], 
					rbg);
	input_report_key(wdata->extension.input,
					wiimod_turntable_map[WIIMOD_TURNTABLE_KEY_R_RIGHT],
					rbr);
	input_report_key(wdata->extension.input, 
					wiimod_turntable_map[WIIMOD_TURNTABLE_KEY_B_RIGHT], 
					rbb);
	input_report_key(wdata->extension.input, 
					wiimod_turntable_map[WIIMOD_TURNTABLE_KEY_G_LEFT], 
					lbg);
	input_report_key(wdata->extension.input, 
					wiimod_turntable_map[WIIMOD_TURNTABLE_KEY_R_LEFT], 
					lbr);
	input_report_key(wdata->extension.input, 
					wiimod_turntable_map[WIIMOD_TURNTABLE_KEY_B_LEFT], 
					lbb);
	input_report_key(wdata->extension.input, 
					wiimod_turntable_map[WIIMOD_TURNTABLE_KEY_EUPHORIA], 
					be);
	input_report_key(wdata->extension.input, 
					wiimod_turntable_map[WIIMOD_TURNTABLE_KEY_PLUS], 
					bp);
	input_report_key(wdata->extension.input, 
					wiimod_turntable_map[WIIMOD_TURNTABLE_KEY_MINUS], 
					bm);

	input_sync(wdata->extension.input);
}

static int wiimod_turntable_open(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags |= WIIPROTO_FLAG_EXT_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return 0;
}

static void wiimod_turntable_close(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags &= ~WIIPROTO_FLAG_EXT_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	spin_unlock_irqrestore(&wdata->state.lock, flags);
}

static int wiimod_turntable_probe(const struct wiimod_ops *ops,
			       struct wiimote_data *wdata)
{
 	int ret, i;

	wdata->extension.input = input_allocate_device();
	if (!wdata->extension.input)
		return -ENOMEM;

	input_set_drvdata(wdata->extension.input, wdata);
	wdata->extension.input->open = wiimod_turntable_open;
	wdata->extension.input->close = wiimod_turntable_close;
	wdata->extension.input->dev.parent = &wdata->hdev->dev;
	wdata->extension.input->id.bustype = wdata->hdev->bus;
	wdata->extension.input->id.vendor = wdata->hdev->vendor;
	wdata->extension.input->id.product = wdata->hdev->product;
	wdata->extension.input->id.version = wdata->hdev->version;
	wdata->extension.input->name = WIIMOTE_NAME " Turntable";

	set_bit(EV_KEY, wdata->extension.input->evbit);
	for (i = 0; i < WIIMOD_TURNTABLE_KEY_NUM; ++i)
		set_bit(wiimod_turntable_map[i],
			wdata->extension.input->keybit);

	set_bit(EV_ABS, wdata->extension.input->evbit);
	set_bit(ABS_X, wdata->extension.input->absbit);
	set_bit(ABS_Y, wdata->extension.input->absbit);
	set_bit(ABS_HAT0X, wdata->extension.input->absbit);
	set_bit(ABS_HAT1X, wdata->extension.input->absbit);
	set_bit(ABS_HAT2X, wdata->extension.input->absbit);
	set_bit(ABS_HAT3X, wdata->extension.input->absbit);
	input_set_abs_params(wdata->extension.input,
			     ABS_X, 0, 63, 1, 0);
	input_set_abs_params(wdata->extension.input,
			     ABS_Y, 63, 0, 1, 0);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT0X, -8, 8, 0, 0);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT1X, -8, 8, 0, 0);
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT2X, 0, 31, 1, 1);	
	input_set_abs_params(wdata->extension.input,
			     ABS_HAT3X, 0, 7, 0, 0);	 
	ret = input_register_device(wdata->extension.input);
	if (ret)
		goto err_free;

	return 0;

err_free:
	input_free_device(wdata->extension.input);
	wdata->extension.input = NULL;
	return ret;
}

static void wiimod_turntable_remove(const struct wiimod_ops *ops,
				 struct wiimote_data *wdata)
{
	if (!wdata->extension.input)
		return;

	input_unregister_device(wdata->extension.input);
	wdata->extension.input = NULL;
}

static const struct wiimod_ops wiimod_turntable = {
	.flags = 0,
	.arg = 0,
	.probe = wiimod_turntable_probe,
	.remove = wiimod_turntable_remove,
	.in_ext = wiimod_turntable_in_ext,
};

 

static int wiimod_builtin_mp_probe(const struct wiimod_ops *ops,
				   struct wiimote_data *wdata)
{
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags |= WIIPROTO_FLAG_BUILTIN_MP;
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return 0;
}

static void wiimod_builtin_mp_remove(const struct wiimod_ops *ops,
				     struct wiimote_data *wdata)
{
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags |= WIIPROTO_FLAG_BUILTIN_MP;
	spin_unlock_irqrestore(&wdata->state.lock, flags);
}

static const struct wiimod_ops wiimod_builtin_mp = {
	.flags = 0,
	.arg = 0,
	.probe = wiimod_builtin_mp_probe,
	.remove = wiimod_builtin_mp_remove,
};

 

static int wiimod_no_mp_probe(const struct wiimod_ops *ops,
			      struct wiimote_data *wdata)
{
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags |= WIIPROTO_FLAG_NO_MP;
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return 0;
}

static void wiimod_no_mp_remove(const struct wiimod_ops *ops,
				struct wiimote_data *wdata)
{
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags |= WIIPROTO_FLAG_NO_MP;
	spin_unlock_irqrestore(&wdata->state.lock, flags);
}

static const struct wiimod_ops wiimod_no_mp = {
	.flags = 0,
	.arg = 0,
	.probe = wiimod_no_mp_probe,
	.remove = wiimod_no_mp_remove,
};

 

static void wiimod_mp_in_mp(struct wiimote_data *wdata, const __u8 *ext)
{
	__s32 x, y, z;

	 

	x = ext[0];
	y = ext[1];
	z = ext[2];

	x |= (((__u16)ext[3]) << 6) & 0xff00;
	y |= (((__u16)ext[4]) << 6) & 0xff00;
	z |= (((__u16)ext[5]) << 6) & 0xff00;

	x -= 8192;
	y -= 8192;
	z -= 8192;

	if (!(ext[3] & 0x02))
		x = (x * 2000 * 9) / 440;
	else
		x *= 9;
	if (!(ext[4] & 0x02))
		y = (y * 2000 * 9) / 440;
	else
		y *= 9;
	if (!(ext[3] & 0x01))
		z = (z * 2000 * 9) / 440;
	else
		z *= 9;

	input_report_abs(wdata->mp, ABS_RX, x);
	input_report_abs(wdata->mp, ABS_RY, y);
	input_report_abs(wdata->mp, ABS_RZ, z);
	input_sync(wdata->mp);
}

static int wiimod_mp_open(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags |= WIIPROTO_FLAG_MP_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	__wiimote_schedule(wdata);
	spin_unlock_irqrestore(&wdata->state.lock, flags);

	return 0;
}

static void wiimod_mp_close(struct input_dev *dev)
{
	struct wiimote_data *wdata = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&wdata->state.lock, flags);
	wdata->state.flags &= ~WIIPROTO_FLAG_MP_USED;
	wiiproto_req_drm(wdata, WIIPROTO_REQ_NULL);
	__wiimote_schedule(wdata);
	spin_unlock_irqrestore(&wdata->state.lock, flags);
}

static int wiimod_mp_probe(const struct wiimod_ops *ops,
			   struct wiimote_data *wdata)
{
	int ret;

	wdata->mp = input_allocate_device();
	if (!wdata->mp)
		return -ENOMEM;

	input_set_drvdata(wdata->mp, wdata);
	wdata->mp->open = wiimod_mp_open;
	wdata->mp->close = wiimod_mp_close;
	wdata->mp->dev.parent = &wdata->hdev->dev;
	wdata->mp->id.bustype = wdata->hdev->bus;
	wdata->mp->id.vendor = wdata->hdev->vendor;
	wdata->mp->id.product = wdata->hdev->product;
	wdata->mp->id.version = wdata->hdev->version;
	wdata->mp->name = WIIMOTE_NAME " Motion Plus";

	set_bit(EV_ABS, wdata->mp->evbit);
	set_bit(ABS_RX, wdata->mp->absbit);
	set_bit(ABS_RY, wdata->mp->absbit);
	set_bit(ABS_RZ, wdata->mp->absbit);
	input_set_abs_params(wdata->mp,
			     ABS_RX, -16000, 16000, 4, 8);
	input_set_abs_params(wdata->mp,
			     ABS_RY, -16000, 16000, 4, 8);
	input_set_abs_params(wdata->mp,
			     ABS_RZ, -16000, 16000, 4, 8);

	ret = input_register_device(wdata->mp);
	if (ret)
		goto err_free;

	return 0;

err_free:
	input_free_device(wdata->mp);
	wdata->mp = NULL;
	return ret;
}

static void wiimod_mp_remove(const struct wiimod_ops *ops,
			     struct wiimote_data *wdata)
{
	if (!wdata->mp)
		return;

	input_unregister_device(wdata->mp);
	wdata->mp = NULL;
}

const struct wiimod_ops wiimod_mp = {
	.flags = 0,
	.arg = 0,
	.probe = wiimod_mp_probe,
	.remove = wiimod_mp_remove,
	.in_mp = wiimod_mp_in_mp,
};

 

static const struct wiimod_ops wiimod_dummy;

const struct wiimod_ops *wiimod_table[WIIMOD_NUM] = {
	[WIIMOD_KEYS] = &wiimod_keys,
	[WIIMOD_RUMBLE] = &wiimod_rumble,
	[WIIMOD_BATTERY] = &wiimod_battery,
	[WIIMOD_LED1] = &wiimod_leds[0],
	[WIIMOD_LED2] = &wiimod_leds[1],
	[WIIMOD_LED3] = &wiimod_leds[2],
	[WIIMOD_LED4] = &wiimod_leds[3],
	[WIIMOD_ACCEL] = &wiimod_accel,
	[WIIMOD_IR] = &wiimod_ir,
	[WIIMOD_BUILTIN_MP] = &wiimod_builtin_mp,
	[WIIMOD_NO_MP] = &wiimod_no_mp,
};

const struct wiimod_ops *wiimod_ext_table[WIIMOTE_EXT_NUM] = {
	[WIIMOTE_EXT_NONE] = &wiimod_dummy,
	[WIIMOTE_EXT_UNKNOWN] = &wiimod_dummy,
	[WIIMOTE_EXT_NUNCHUK] = &wiimod_nunchuk,
	[WIIMOTE_EXT_CLASSIC_CONTROLLER] = &wiimod_classic,
	[WIIMOTE_EXT_BALANCE_BOARD] = &wiimod_bboard,
	[WIIMOTE_EXT_PRO_CONTROLLER] = &wiimod_pro,
	[WIIMOTE_EXT_DRUMS] = &wiimod_drums,
	[WIIMOTE_EXT_GUITAR] = &wiimod_guitar,
	[WIIMOTE_EXT_TURNTABLE] = &wiimod_turntable,
};
