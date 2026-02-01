
 

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include "hid-ids.h"

MODULE_AUTHOR("James McKenzie");
MODULE_AUTHOR("Benjamin Tissoires <benjamin.tissoires@redhat.com>");
MODULE_DESCRIPTION("HID Apple IR remote controls");
MODULE_LICENSE("GPL");

#define KEY_MASK		0x0F
#define TWO_PACKETS_MASK	0x40

 

 

 

 

 

static const unsigned short appleir_key_table[] = {
	KEY_RESERVED,
	KEY_MENU,
	KEY_PLAYPAUSE,
	KEY_FORWARD,
	KEY_BACK,
	KEY_VOLUMEUP,
	KEY_VOLUMEDOWN,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_RESERVED,
	KEY_ENTER,
	KEY_PLAYPAUSE,
	KEY_RESERVED,
};

struct appleir {
	struct input_dev *input_dev;
	struct hid_device *hid;
	unsigned short keymap[ARRAY_SIZE(appleir_key_table)];
	struct timer_list key_up_timer;	 
	spinlock_t lock;		 
	int current_key;		 
	int prev_key_idx;		 
};

static int get_key(int data)
{
	 
	int key = (data >> 1) & KEY_MASK;

	if ((data & TWO_PACKETS_MASK))
		 
		key = -key;

	return key;
}

static void key_up(struct hid_device *hid, struct appleir *appleir, int key)
{
	input_report_key(appleir->input_dev, key, 0);
	input_sync(appleir->input_dev);
}

static void key_down(struct hid_device *hid, struct appleir *appleir, int key)
{
	input_report_key(appleir->input_dev, key, 1);
	input_sync(appleir->input_dev);
}

static void battery_flat(struct appleir *appleir)
{
	dev_err(&appleir->input_dev->dev, "possible flat battery?\n");
}

static void key_up_tick(struct timer_list *t)
{
	struct appleir *appleir = from_timer(appleir, t, key_up_timer);
	struct hid_device *hid = appleir->hid;
	unsigned long flags;

	spin_lock_irqsave(&appleir->lock, flags);
	if (appleir->current_key) {
		key_up(hid, appleir, appleir->current_key);
		appleir->current_key = 0;
	}
	spin_unlock_irqrestore(&appleir->lock, flags);
}

static int appleir_raw_event(struct hid_device *hid, struct hid_report *report,
	 u8 *data, int len)
{
	struct appleir *appleir = hid_get_drvdata(hid);
	static const u8 keydown[] = { 0x25, 0x87, 0xee };
	static const u8 keyrepeat[] = { 0x26, };
	static const u8 flatbattery[] = { 0x25, 0x87, 0xe0 };
	unsigned long flags;

	if (len != 5)
		goto out;

	if (!memcmp(data, keydown, sizeof(keydown))) {
		int index;

		spin_lock_irqsave(&appleir->lock, flags);
		 
		if (appleir->current_key)
			key_up(hid, appleir, appleir->current_key);

		 
		if (appleir->prev_key_idx > 0)
			index = appleir->prev_key_idx;
		else
			index = get_key(data[4]);

		if (index >= 0) {
			appleir->current_key = appleir->keymap[index];

			key_down(hid, appleir, appleir->current_key);
			 
			mod_timer(&appleir->key_up_timer, jiffies + HZ / 8);
			appleir->prev_key_idx = 0;
		} else
			 
			appleir->prev_key_idx = -index;
		spin_unlock_irqrestore(&appleir->lock, flags);
		goto out;
	}

	appleir->prev_key_idx = 0;

	if (!memcmp(data, keyrepeat, sizeof(keyrepeat))) {
		key_down(hid, appleir, appleir->current_key);
		 
		mod_timer(&appleir->key_up_timer, jiffies + HZ / 8);
		goto out;
	}

	if (!memcmp(data, flatbattery, sizeof(flatbattery))) {
		battery_flat(appleir);
		 
	}

out:
	 
	return 0;
}

static int appleir_input_configured(struct hid_device *hid,
		struct hid_input *hidinput)
{
	struct input_dev *input_dev = hidinput->input;
	struct appleir *appleir = hid_get_drvdata(hid);
	int i;

	appleir->input_dev = input_dev;

	input_dev->keycode = appleir->keymap;
	input_dev->keycodesize = sizeof(unsigned short);
	input_dev->keycodemax = ARRAY_SIZE(appleir->keymap);

	input_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_REP);

	memcpy(appleir->keymap, appleir_key_table, sizeof(appleir->keymap));
	for (i = 0; i < ARRAY_SIZE(appleir_key_table); i++)
		set_bit(appleir->keymap[i], input_dev->keybit);
	clear_bit(KEY_RESERVED, input_dev->keybit);

	return 0;
}

static int appleir_input_mapping(struct hid_device *hid,
		struct hid_input *hi, struct hid_field *field,
		struct hid_usage *usage, unsigned long **bit, int *max)
{
	return -1;
}

static int appleir_probe(struct hid_device *hid, const struct hid_device_id *id)
{
	int ret;
	struct appleir *appleir;

	appleir = devm_kzalloc(&hid->dev, sizeof(struct appleir), GFP_KERNEL);
	if (!appleir)
		return -ENOMEM;

	appleir->hid = hid;

	 
	hid->quirks |= HID_QUIRK_HIDINPUT_FORCE;

	spin_lock_init(&appleir->lock);
	timer_setup(&appleir->key_up_timer, key_up_tick, 0);

	hid_set_drvdata(hid, appleir);

	ret = hid_parse(hid);
	if (ret) {
		hid_err(hid, "parse failed\n");
		goto fail;
	}

	ret = hid_hw_start(hid, HID_CONNECT_DEFAULT | HID_CONNECT_HIDDEV_FORCE);
	if (ret) {
		hid_err(hid, "hw start failed\n");
		goto fail;
	}

	return 0;
fail:
	devm_kfree(&hid->dev, appleir);
	return ret;
}

static void appleir_remove(struct hid_device *hid)
{
	struct appleir *appleir = hid_get_drvdata(hid);
	hid_hw_stop(hid);
	del_timer_sync(&appleir->key_up_timer);
}

static const struct hid_device_id appleir_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_APPLE, USB_DEVICE_ID_APPLE_IRCONTROL) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_APPLE, USB_DEVICE_ID_APPLE_IRCONTROL2) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_APPLE, USB_DEVICE_ID_APPLE_IRCONTROL3) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_APPLE, USB_DEVICE_ID_APPLE_IRCONTROL4) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_APPLE, USB_DEVICE_ID_APPLE_IRCONTROL5) },
	{ }
};
MODULE_DEVICE_TABLE(hid, appleir_devices);

static struct hid_driver appleir_driver = {
	.name = "appleir",
	.id_table = appleir_devices,
	.raw_event = appleir_raw_event,
	.input_configured = appleir_input_configured,
	.probe = appleir_probe,
	.remove = appleir_remove,
	.input_mapping = appleir_input_mapping,
};
module_hid_driver(appleir_driver);
