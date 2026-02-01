
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/acpi.h>
#include <linux/string.h>
#include <linux/dmi.h>
#include <linux/wmi.h>
#include <acpi/video.h>
#include "dell-smbios.h"
#include "dell-wmi-descriptor.h"
#include "dell-wmi-privacy.h"

MODULE_AUTHOR("Matthew Garrett <mjg@redhat.com>");
MODULE_AUTHOR("Pali Rohár <pali@kernel.org>");
MODULE_DESCRIPTION("Dell laptop WMI hotkeys driver");
MODULE_LICENSE("GPL");

#define DELL_EVENT_GUID "9DBB5994-A997-11DA-B012-B622A1EF5492"

static bool wmi_requires_smbios_request;

struct dell_wmi_priv {
	struct input_dev *input_dev;
	struct input_dev *tabletswitch_dev;
	u32 interface_version;
};

static int __init dmi_matched(const struct dmi_system_id *dmi)
{
	wmi_requires_smbios_request = 1;
	return 1;
}

static const struct dmi_system_id dell_wmi_smbios_list[] __initconst = {
	{
		.callback = dmi_matched,
		.ident = "Dell Inspiron M5110",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Inspiron M5110"),
		},
	},
	{
		.callback = dmi_matched,
		.ident = "Dell Vostro V131",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Vostro V131"),
		},
	},
	{ }
};

 
static const struct key_entry dell_wmi_keymap_type_0000[] = {
	{ KE_IGNORE, 0x003a, { KEY_CAPSLOCK } },

	 
	{ KE_KEY,    0xe005, { KEY_BRIGHTNESSDOWN } },
	{ KE_KEY,    0xe006, { KEY_BRIGHTNESSUP } },

	 
	{ KE_KEY,    0xe007, { KEY_BATTERY } },

	 
	{ KE_IGNORE, 0xe008, { KEY_RFKILL } },

	{ KE_KEY,    0xe009, { KEY_EJECTCD } },

	 
	{ KE_KEY,    0xe00b, { KEY_SWITCHVIDEOMODE } },

	 
	{ KE_IGNORE, 0xe00c, { KEY_KBDILLUMTOGGLE } },

	 
	{ KE_IGNORE, 0xe00d, { KEY_RESERVED } },

	 
	{ KE_IGNORE, 0xe00e, { KEY_RESERVED } },

	 
	{ KE_KEY,    0xe011, { KEY_WLAN } },

	 
	{ KE_IGNORE, 0xe013, { KEY_RESERVED } },

	{ KE_IGNORE, 0xe020, { KEY_MUTE } },

	 
	 

	 
	 

	 
	{ KE_KEY,    0xe025, { KEY_PROG4 } },

	 
	{ KE_IGNORE, 0xe026, { KEY_RESERVED } },

	 
	{ KE_KEY,    0xe027, { KEY_DISPLAYTOGGLE } },

	 
	 

	 
	{ KE_KEY,    0xe029, { KEY_PROG4 } },

	 
	 

	 
	 

	 
	 

	{ KE_IGNORE, 0xe02e, { KEY_VOLUMEDOWN } },
	{ KE_IGNORE, 0xe030, { KEY_VOLUMEUP } },
	{ KE_IGNORE, 0xe033, { KEY_KBDILLUMUP } },
	{ KE_IGNORE, 0xe034, { KEY_KBDILLUMDOWN } },
	{ KE_IGNORE, 0xe03a, { KEY_CAPSLOCK } },

	 
	{ KE_IGNORE, 0xe043, { KEY_RESERVED } },

	 
	{ KE_IGNORE, 0xe044, { KEY_RESERVED } },

	 
	{ KE_IGNORE, 0xe045, { KEY_NUMLOCK } },

	 
	{ KE_IGNORE, 0xe046, { KEY_SCROLLLOCK } },

	 
	 

	 
	{ KE_IGNORE, 0xe06e, { KEY_RESERVED } },

	{ KE_IGNORE, 0xe0f7, { KEY_MUTE } },
	{ KE_IGNORE, 0xe0f8, { KEY_VOLUMEDOWN } },
	{ KE_IGNORE, 0xe0f9, { KEY_VOLUMEUP } },
};

struct dell_bios_keymap_entry {
	u16 scancode;
	u16 keycode;
};

struct dell_bios_hotkey_table {
	struct dmi_header header;
	struct dell_bios_keymap_entry keymap[];

};

struct dell_dmi_results {
	int err;
	int keymap_size;
	struct key_entry *keymap;
};

 
static const u16 bios_to_linux_keycode[256] = {
	[0]	= KEY_MEDIA,
	[1]	= KEY_NEXTSONG,
	[2]	= KEY_PLAYPAUSE,
	[3]	= KEY_PREVIOUSSONG,
	[4]	= KEY_STOPCD,
	[5]	= KEY_UNKNOWN,
	[6]	= KEY_UNKNOWN,
	[7]	= KEY_UNKNOWN,
	[8]	= KEY_WWW,
	[9]	= KEY_UNKNOWN,
	[10]	= KEY_VOLUMEDOWN,
	[11]	= KEY_MUTE,
	[12]	= KEY_VOLUMEUP,
	[13]	= KEY_UNKNOWN,
	[14]	= KEY_BATTERY,
	[15]	= KEY_EJECTCD,
	[16]	= KEY_UNKNOWN,
	[17]	= KEY_SLEEP,
	[18]	= KEY_PROG1,
	[19]	= KEY_BRIGHTNESSDOWN,
	[20]	= KEY_BRIGHTNESSUP,
	[21]	= KEY_BRIGHTNESS_AUTO,
	[22]	= KEY_KBDILLUMTOGGLE,
	[23]	= KEY_UNKNOWN,
	[24]	= KEY_SWITCHVIDEOMODE,
	[25]	= KEY_UNKNOWN,
	[26]	= KEY_UNKNOWN,
	[27]	= KEY_SWITCHVIDEOMODE,
	[28]	= KEY_UNKNOWN,
	[29]	= KEY_UNKNOWN,
	[30]	= KEY_PROG2,
	[31]	= KEY_UNKNOWN,
	[32]	= KEY_UNKNOWN,
	[33]	= KEY_UNKNOWN,
	[34]	= KEY_UNKNOWN,
	[35]	= KEY_UNKNOWN,
	[36]	= KEY_UNKNOWN,
	[37]	= KEY_UNKNOWN,
	[38]	= KEY_MICMUTE,
	[255]	= KEY_PROG3,
};

 
static const struct key_entry dell_wmi_keymap_type_0010[] = {
	 
	{ KE_IGNORE, 0x0, { KEY_RESERVED } },

	 
	{ KE_IGNORE, 0x1, { KEY_RESERVED } },

	 
	{ KE_IGNORE, 0x3f, { KEY_RESERVED } },

	 
	{ KE_KEY,    0x57, { KEY_BRIGHTNESSDOWN } },
	{ KE_KEY,    0x58, { KEY_BRIGHTNESSUP } },

	 
	{ KE_KEY, 0x109, { KEY_MUTE} },

	 
	{ KE_KEY, 0x150, { KEY_MICMUTE } },

	 
	{ KE_IGNORE, 0x151, { KEY_RESERVED } },

	 
	{ KE_IGNORE, 0x152, { KEY_KBDILLUMTOGGLE } },

	 
	{ KE_IGNORE, 0x153, { KEY_RFKILL } },

	 
	{ KE_IGNORE, 0x154, { KEY_RESERVED } },

	 
	{ KE_IGNORE, 0x155, { KEY_RESERVED } },

	 
	{ KE_IGNORE, 0x156, { KEY_RESERVED } },
	{ KE_IGNORE, 0x157, { KEY_RESERVED } },

	 
	{ KE_KEY,    0x850, { KEY_PROG1 } },
	{ KE_KEY,    0x851, { KEY_PROG2 } },
	{ KE_KEY,    0x852, { KEY_PROG3 } },

	 
	{ KE_IGNORE, 0xe008, { KEY_RFKILL } },

	 
	{ KE_IGNORE, 0xe035, { KEY_RESERVED } },
};

 
static const struct key_entry dell_wmi_keymap_type_0011[] = {
	 
	{ KE_IGNORE, 0xe070, { KEY_RESERVED } },

	 
	{ KE_IGNORE, 0xfff0, { KEY_RESERVED } },

	 
	{ KE_IGNORE, 0xfff1, { KEY_RESERVED } },

	 
	{ KE_IGNORE, 0xfff2, { KEY_RESERVED } },

	 
	{ KE_IGNORE, 0xfff3, { KEY_RESERVED } },

	 
	{ KE_IGNORE, KBD_LED_OFF_TOKEN,      { KEY_RESERVED } },
	{ KE_IGNORE, KBD_LED_ON_TOKEN,       { KEY_RESERVED } },
	{ KE_IGNORE, KBD_LED_AUTO_TOKEN,     { KEY_RESERVED } },
	{ KE_IGNORE, KBD_LED_AUTO_25_TOKEN,  { KEY_RESERVED } },
	{ KE_IGNORE, KBD_LED_AUTO_50_TOKEN,  { KEY_RESERVED } },
	{ KE_IGNORE, KBD_LED_AUTO_75_TOKEN,  { KEY_RESERVED } },
	{ KE_IGNORE, KBD_LED_AUTO_100_TOKEN, { KEY_RESERVED } },
};

 
static const struct key_entry dell_wmi_keymap_type_0012[] = {
	 
	{ KE_IGNORE, 0x0003, { KEY_RESERVED } },

	 
	{ KE_IGNORE, 0x000d, { KEY_RESERVED } },

	 
	{ KE_IGNORE, 0xe035, { KEY_RESERVED } },
};

static void dell_wmi_switch_event(struct input_dev **subdev,
				  const char *devname,
				  int switchid,
				  int value)
{
	if (!*subdev) {
		struct input_dev *dev = input_allocate_device();

		if (!dev) {
			pr_warn("could not allocate device for %s\n", devname);
			return;
		}
		__set_bit(EV_SW, (dev)->evbit);
		__set_bit(switchid, (dev)->swbit);

		(dev)->name = devname;
		(dev)->id.bustype = BUS_HOST;
		if (input_register_device(dev)) {
			input_free_device(dev);
			pr_warn("could not register device for %s\n", devname);
			return;
		}
		*subdev = dev;
	}

	input_report_switch(*subdev, switchid, value);
	input_sync(*subdev);
}

static int dell_wmi_process_key(struct wmi_device *wdev, int type, int code, u16 *buffer, int remaining)
{
	struct dell_wmi_priv *priv = dev_get_drvdata(&wdev->dev);
	const struct key_entry *key;
	int used = 0;
	int value = 1;

	key = sparse_keymap_entry_from_scancode(priv->input_dev,
						(type << 16) | code);
	if (!key) {
		pr_info("Unknown key with type 0x%04x and code 0x%04x pressed\n",
			type, code);
		return 0;
	}

	pr_debug("Key with type 0x%04x and code 0x%04x pressed\n", type, code);

	 
	if ((key->keycode == KEY_BRIGHTNESSUP ||
	     key->keycode == KEY_BRIGHTNESSDOWN) &&
	    acpi_video_handles_brightness_key_presses())
		return 0;

	if (type == 0x0000 && code == 0xe025 && !wmi_requires_smbios_request)
		return 0;

	if (key->keycode == KEY_KBDILLUMTOGGLE) {
		dell_laptop_call_notifier(
			DELL_LAPTOP_KBD_BACKLIGHT_BRIGHTNESS_CHANGED, NULL);
	} else if (type == 0x0011 && code == 0xe070 && remaining > 0) {
		dell_wmi_switch_event(&priv->tabletswitch_dev,
				      "Dell tablet mode switch",
				      SW_TABLET_MODE, !buffer[0]);
		return 1;
	} else if (type == 0x0012 && code == 0x000d && remaining > 0) {
		value = (buffer[2] == 2);
		used = 1;
	}

	sparse_keymap_report_entry(priv->input_dev, key, value, true);

	return used;
}

static void dell_wmi_notify(struct wmi_device *wdev,
			    union acpi_object *obj)
{
	struct dell_wmi_priv *priv = dev_get_drvdata(&wdev->dev);
	u16 *buffer_entry, *buffer_end;
	acpi_size buffer_size;
	int len, i;

	if (obj->type != ACPI_TYPE_BUFFER) {
		pr_warn("bad response type %x\n", obj->type);
		return;
	}

	pr_debug("Received WMI event (%*ph)\n",
		obj->buffer.length, obj->buffer.pointer);

	buffer_entry = (u16 *)obj->buffer.pointer;
	buffer_size = obj->buffer.length/2;
	buffer_end = buffer_entry + buffer_size;

	 
	if (priv->interface_version == 0 && buffer_entry < buffer_end)
		if (buffer_end > buffer_entry + buffer_entry[0] + 1)
			buffer_end = buffer_entry + buffer_entry[0] + 1;

	while (buffer_entry < buffer_end) {

		len = buffer_entry[0];
		if (len == 0)
			break;

		len++;

		if (buffer_entry + len > buffer_end) {
			pr_warn("Invalid length of WMI event\n");
			break;
		}

		pr_debug("Process buffer (%*ph)\n", len*2, buffer_entry);

		switch (buffer_entry[1]) {
		case 0x0000:  
			if (len > 2)
				dell_wmi_process_key(wdev, buffer_entry[1],
						     buffer_entry[2],
						     buffer_entry + 3,
						     len - 3);
			 
			break;
		case 0x0010:  
		case 0x0011:  
			for (i = 2; i < len; ++i)
				i += dell_wmi_process_key(wdev, buffer_entry[1],
							  buffer_entry[i],
							  buffer_entry + i,
							  len - i - 1);
			break;
		case 0x0012:
			if ((len > 4) && dell_privacy_process_event(buffer_entry[1], buffer_entry[3],
								    buffer_entry[4]))
				 ;
			else if (len > 2)
				dell_wmi_process_key(wdev, buffer_entry[1], buffer_entry[2],
						     buffer_entry + 3, len - 3);
			break;
		default:  
			pr_info("Unknown WMI event type 0x%x\n",
				(int)buffer_entry[1]);
			break;
		}

		buffer_entry += len;

	}

}

static bool have_scancode(u32 scancode, const struct key_entry *keymap, int len)
{
	int i;

	for (i = 0; i < len; i++)
		if (keymap[i].code == scancode)
			return true;

	return false;
}

static void handle_dmi_entry(const struct dmi_header *dm, void *opaque)
{
	struct dell_dmi_results *results = opaque;
	struct dell_bios_hotkey_table *table;
	int hotkey_num, i, pos = 0;
	struct key_entry *keymap;

	if (results->err || results->keymap)
		return;		 

	 
	if (dm->type != 0xb2)
		return;

	table = container_of(dm, struct dell_bios_hotkey_table, header);

	hotkey_num = (table->header.length -
		      sizeof(struct dell_bios_hotkey_table)) /
				sizeof(struct dell_bios_keymap_entry);
	if (hotkey_num < 1) {
		 
		return;
	}

	keymap = kcalloc(hotkey_num, sizeof(struct key_entry), GFP_KERNEL);
	if (!keymap) {
		results->err = -ENOMEM;
		return;
	}

	for (i = 0; i < hotkey_num; i++) {
		const struct dell_bios_keymap_entry *bios_entry =
					&table->keymap[i];

		 
		u16 keycode = (bios_entry->keycode <
			       ARRAY_SIZE(bios_to_linux_keycode)) ?
			bios_to_linux_keycode[bios_entry->keycode] :
			(bios_entry->keycode == 0xffff ? KEY_UNKNOWN : KEY_RESERVED);

		 
		if (keycode == KEY_RESERVED) {
			pr_info("firmware scancode 0x%x maps to unrecognized keycode 0x%x\n",
				bios_entry->scancode, bios_entry->keycode);
			continue;
		}

		if (keycode == KEY_KBDILLUMTOGGLE)
			keymap[pos].type = KE_IGNORE;
		else
			keymap[pos].type = KE_KEY;
		keymap[pos].code = bios_entry->scancode;
		keymap[pos].keycode = keycode;

		pos++;
	}

	results->keymap = keymap;
	results->keymap_size = pos;
}

static int dell_wmi_input_setup(struct wmi_device *wdev)
{
	struct dell_wmi_priv *priv = dev_get_drvdata(&wdev->dev);
	struct dell_dmi_results dmi_results = {};
	struct key_entry *keymap;
	int err, i, pos = 0;

	priv->input_dev = input_allocate_device();
	if (!priv->input_dev)
		return -ENOMEM;

	priv->input_dev->name = "Dell WMI hotkeys";
	priv->input_dev->id.bustype = BUS_HOST;
	priv->input_dev->dev.parent = &wdev->dev;

	if (dmi_walk(handle_dmi_entry, &dmi_results)) {
		 
		pr_warn("no DMI; using the old-style hotkey interface\n");
	}

	if (dmi_results.err) {
		err = dmi_results.err;
		goto err_free_dev;
	}

	keymap = kcalloc(dmi_results.keymap_size +
			 ARRAY_SIZE(dell_wmi_keymap_type_0000) +
			 ARRAY_SIZE(dell_wmi_keymap_type_0010) +
			 ARRAY_SIZE(dell_wmi_keymap_type_0011) +
			 ARRAY_SIZE(dell_wmi_keymap_type_0012) +
			 1,
			 sizeof(struct key_entry), GFP_KERNEL);
	if (!keymap) {
		kfree(dmi_results.keymap);
		err = -ENOMEM;
		goto err_free_dev;
	}

	 
	for (i = 0; i < dmi_results.keymap_size; i++) {
		keymap[pos] = dmi_results.keymap[i];
		keymap[pos].code |= (0x0010 << 16);
		pos++;
	}

	kfree(dmi_results.keymap);

	 
	for (i = 0; i < ARRAY_SIZE(dell_wmi_keymap_type_0010); i++) {
		const struct key_entry *entry = &dell_wmi_keymap_type_0010[i];

		 
		if (dmi_results.keymap_size &&
		    have_scancode(entry->code | (0x0010 << 16),
				  keymap, dmi_results.keymap_size)
		   )
			continue;

		keymap[pos] = *entry;
		keymap[pos].code |= (0x0010 << 16);
		pos++;
	}

	 
	for (i = 0; i < ARRAY_SIZE(dell_wmi_keymap_type_0011); i++) {
		keymap[pos] = dell_wmi_keymap_type_0011[i];
		keymap[pos].code |= (0x0011 << 16);
		pos++;
	}

	 
	for (i = 0; i < ARRAY_SIZE(dell_wmi_keymap_type_0012); i++) {
		keymap[pos] = dell_wmi_keymap_type_0012[i];
		keymap[pos].code |= (0x0012 << 16);
		pos++;
	}

	 
	for (i = 0; i < ARRAY_SIZE(dell_wmi_keymap_type_0000); i++) {
		keymap[pos] = dell_wmi_keymap_type_0000[i];
		pos++;
	}

	keymap[pos].type = KE_END;

	err = sparse_keymap_setup(priv->input_dev, keymap, NULL);
	 
	kfree(keymap);
	if (err)
		goto err_free_dev;

	err = input_register_device(priv->input_dev);
	if (err)
		goto err_free_dev;

	return 0;

 err_free_dev:
	input_free_device(priv->input_dev);
	return err;
}

static void dell_wmi_input_destroy(struct wmi_device *wdev)
{
	struct dell_wmi_priv *priv = dev_get_drvdata(&wdev->dev);

	input_unregister_device(priv->input_dev);
	if (priv->tabletswitch_dev)
		input_unregister_device(priv->tabletswitch_dev);
}

 

static int dell_wmi_events_set_enabled(bool enable)
{
	struct calling_interface_buffer *buffer;
	int ret;

	buffer = kzalloc(sizeof(struct calling_interface_buffer), GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;
	buffer->cmd_class = CLASS_INFO;
	buffer->cmd_select = SELECT_APP_REGISTRATION;
	buffer->input[0] = 0x10000;
	buffer->input[1] = 0x51534554;
	buffer->input[3] = enable;
	ret = dell_smbios_call(buffer);
	if (ret == 0)
		ret = buffer->output[0];
	kfree(buffer);

	return dell_smbios_error(ret);
}

static int dell_wmi_probe(struct wmi_device *wdev, const void *context)
{
	struct dell_wmi_priv *priv;
	int ret;

	ret = dell_wmi_get_descriptor_valid();
	if (ret)
		return ret;

	priv = devm_kzalloc(
		&wdev->dev, sizeof(struct dell_wmi_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	dev_set_drvdata(&wdev->dev, priv);

	if (!dell_wmi_get_interface_version(&priv->interface_version))
		return -EPROBE_DEFER;

	return dell_wmi_input_setup(wdev);
}

static void dell_wmi_remove(struct wmi_device *wdev)
{
	dell_wmi_input_destroy(wdev);
}
static const struct wmi_device_id dell_wmi_id_table[] = {
	{ .guid_string = DELL_EVENT_GUID },
	{ },
};

static struct wmi_driver dell_wmi_driver = {
	.driver = {
		.name = "dell-wmi",
	},
	.id_table = dell_wmi_id_table,
	.probe = dell_wmi_probe,
	.remove = dell_wmi_remove,
	.notify = dell_wmi_notify,
};

static int __init dell_wmi_init(void)
{
	int err;

	dmi_check_system(dell_wmi_smbios_list);

	if (wmi_requires_smbios_request) {
		err = dell_wmi_events_set_enabled(true);
		if (err) {
			pr_err("Failed to enable WMI events\n");
			return err;
		}
	}

	err = dell_privacy_register_driver();
	if (err)
		return err;

	return wmi_driver_register(&dell_wmi_driver);
}
late_initcall(dell_wmi_init);

static void __exit dell_wmi_exit(void)
{
	if (wmi_requires_smbios_request)
		dell_wmi_events_set_enabled(false);

	wmi_driver_unregister(&dell_wmi_driver);
	dell_privacy_unregister_driver();
}
module_exit(dell_wmi_exit);

MODULE_DEVICE_TABLE(wmi, dell_wmi_id_table);
