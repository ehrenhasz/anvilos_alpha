
 

#include <linux/acpi.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dmi.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/sysfs.h>
#include <linux/wmi.h>
#include <acpi/battery.h>

 
#define HWMI_METHOD_GUID "ABBC0F5B-8EA1-11D1-A000-C90629100000"
#define HWMI_EVENT_GUID "ABBC0F5C-8EA1-11D1-A000-C90629100000"

 
#define WMI0_EXPENSIVE_GUID "39142400-C6A3-40fa-BADB-8A2652834100"
#define WMI0_EVENT_GUID "59142400-C6A3-40fa-BADB-8A2652834100"

 

enum {
	BATTERY_THRESH_GET		= 0x00001103,  
	BATTERY_THRESH_SET		= 0x00001003,  
	FN_LOCK_GET			= 0x00000604,  
	FN_LOCK_SET			= 0x00000704,  
	MICMUTE_LED_SET			= 0x00000b04,  
};

union hwmi_arg {
	u64 cmd;
	u8 args[8];
};

struct quirk_entry {
	bool battery_reset;
	bool ec_micmute;
	bool report_brightness;
};

static struct quirk_entry *quirks;

struct huawei_wmi_debug {
	struct dentry *root;
	u64 arg;
};

struct huawei_wmi {
	bool battery_available;
	bool fn_lock_available;

	struct huawei_wmi_debug debug;
	struct led_classdev cdev;
	struct device *dev;

	struct mutex wmi_lock;
};

static struct huawei_wmi *huawei_wmi;

static const struct key_entry huawei_wmi_keymap[] = {
	{ KE_KEY,    0x281, { KEY_BRIGHTNESSDOWN } },
	{ KE_KEY,    0x282, { KEY_BRIGHTNESSUP } },
	{ KE_KEY,    0x284, { KEY_MUTE } },
	{ KE_KEY,    0x285, { KEY_VOLUMEDOWN } },
	{ KE_KEY,    0x286, { KEY_VOLUMEUP } },
	{ KE_KEY,    0x287, { KEY_MICMUTE } },
	{ KE_KEY,    0x289, { KEY_WLAN } },
	
	{ KE_KEY,    0x28a, { KEY_CONFIG } },
	
	{ KE_IGNORE, 0x293, { KEY_KBDILLUMTOGGLE } },
	{ KE_IGNORE, 0x294, { KEY_KBDILLUMUP } },
	{ KE_IGNORE, 0x295, { KEY_KBDILLUMUP } },
	
	{ KE_KEY,    0x2c1, { KEY_RESERVED } },
	{ KE_END,	 0 }
};

static int battery_reset = -1;
static int report_brightness = -1;

module_param(battery_reset, bint, 0444);
MODULE_PARM_DESC(battery_reset,
		"Reset battery charge values to (0-0) before disabling it using (0-100)");
module_param(report_brightness, bint, 0444);
MODULE_PARM_DESC(report_brightness,
		"Report brightness keys.");

 

static int __init dmi_matched(const struct dmi_system_id *dmi)
{
	quirks = dmi->driver_data;
	return 1;
}

static struct quirk_entry quirk_unknown = {
};

static struct quirk_entry quirk_battery_reset = {
	.battery_reset = true,
};

static struct quirk_entry quirk_matebook_x = {
	.ec_micmute = true,
	.report_brightness = true,
};

static const struct dmi_system_id huawei_quirks[] = {
	{
		.callback = dmi_matched,
		.ident = "Huawei MACH-WX9",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HUAWEI"),
			DMI_MATCH(DMI_PRODUCT_NAME, "MACH-WX9"),
		},
		.driver_data = &quirk_battery_reset
	},
	{
		.callback = dmi_matched,
		.ident = "Huawei MateBook X",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HUAWEI"),
			DMI_MATCH(DMI_PRODUCT_NAME, "HUAWEI MateBook X")
		},
		.driver_data = &quirk_matebook_x
	},
	{  }
};

 

static int huawei_wmi_call(struct huawei_wmi *huawei,
			   struct acpi_buffer *in, struct acpi_buffer *out)
{
	acpi_status status;

	mutex_lock(&huawei->wmi_lock);
	status = wmi_evaluate_method(HWMI_METHOD_GUID, 0, 1, in, out);
	mutex_unlock(&huawei->wmi_lock);
	if (ACPI_FAILURE(status)) {
		dev_err(huawei->dev, "Failed to evaluate wmi method\n");
		return -ENODEV;
	}

	return 0;
}

 
static int huawei_wmi_cmd(u64 arg, u8 *buf, size_t buflen)
{
	struct huawei_wmi *huawei = huawei_wmi;
	struct acpi_buffer out = { ACPI_ALLOCATE_BUFFER, NULL };
	struct acpi_buffer in;
	union acpi_object *obj;
	size_t len;
	int err, i;

	in.length = sizeof(arg);
	in.pointer = &arg;

	 
	for (i = 0; i < 2; i++) {
		err = huawei_wmi_call(huawei, &in, &out);
		if (err)
			goto fail_cmd;

		obj = out.pointer;
		if (!obj) {
			err = -EIO;
			goto fail_cmd;
		}

		switch (obj->type) {
		 
		case ACPI_TYPE_BUFFER:
			if (obj->buffer.length == 0x104) {
				
				obj->buffer.pointer += 4;
				len = 0x100;
			} else {
				dev_err(huawei->dev, "Bad buffer length, got %d\n", obj->buffer.length);
				err = -EIO;
				goto fail_cmd;
			}

			break;
		 
		case ACPI_TYPE_PACKAGE:
			if (obj->package.count != 2) {
				dev_err(huawei->dev, "Bad package count, got %d\n", obj->package.count);
				err = -EIO;
				goto fail_cmd;
			}

			obj = &obj->package.elements[1];
			if (obj->type != ACPI_TYPE_BUFFER) {
				dev_err(huawei->dev, "Bad package element type, got %d\n", obj->type);
				err = -EIO;
				goto fail_cmd;
			}
			len = obj->buffer.length;

			break;
		 
		default:
			dev_err(huawei->dev, "Unexpected obj type, got: %d\n", obj->type);
			err = -EIO;
			goto fail_cmd;
		}

		if (!*obj->buffer.pointer)
			break;
	}

	err = (*obj->buffer.pointer) ? -ENODEV : 0;

	if (buf) {
		len = min(buflen, len);
		memcpy(buf, obj->buffer.pointer, len);
	}

fail_cmd:
	kfree(out.pointer);
	return err;
}

 

static int huawei_wmi_micmute_led_set(struct led_classdev *led_cdev,
		enum led_brightness brightness)
{
	 
	if (quirks && quirks->ec_micmute) {
		char *acpi_method;
		acpi_handle handle;
		acpi_status status;
		union acpi_object args[3];
		struct acpi_object_list arg_list = {
			.pointer = args,
			.count = ARRAY_SIZE(args),
		};

		handle = ec_get_handle();
		if (!handle)
			return -ENODEV;

		args[0].type = args[1].type = args[2].type = ACPI_TYPE_INTEGER;
		args[1].integer.value = 0x04;

		if (acpi_has_method(handle, "SPIN")) {
			acpi_method = "SPIN";
			args[0].integer.value = 0;
			args[2].integer.value = brightness ? 1 : 0;
		} else if (acpi_has_method(handle, "WPIN")) {
			acpi_method = "WPIN";
			args[0].integer.value = 1;
			args[2].integer.value = brightness ? 0 : 1;
		} else {
			return -ENODEV;
		}

		status = acpi_evaluate_object(handle, acpi_method, &arg_list, NULL);
		if (ACPI_FAILURE(status))
			return -ENODEV;

		return 0;
	} else {
		union hwmi_arg arg;

		arg.cmd = MICMUTE_LED_SET;
		arg.args[2] = brightness;

		return huawei_wmi_cmd(arg.cmd, NULL, 0);
	}
}

static void huawei_wmi_leds_setup(struct device *dev)
{
	struct huawei_wmi *huawei = dev_get_drvdata(dev);

	huawei->cdev.name = "platform::micmute";
	huawei->cdev.max_brightness = 1;
	huawei->cdev.brightness_set_blocking = &huawei_wmi_micmute_led_set;
	huawei->cdev.default_trigger = "audio-micmute";
	huawei->cdev.brightness = ledtrig_audio_get(LED_AUDIO_MICMUTE);
	huawei->cdev.dev = dev;
	huawei->cdev.flags = LED_CORE_SUSPENDRESUME;

	devm_led_classdev_register(dev, &huawei->cdev);
}

 

static int huawei_wmi_battery_get(int *start, int *end)
{
	u8 ret[0x100];
	int err, i;

	err = huawei_wmi_cmd(BATTERY_THRESH_GET, ret, sizeof(ret));
	if (err)
		return err;

	 
	i = ARRAY_SIZE(ret) - 1;
	do {
		if (start)
			*start = ret[i-1];
		if (end)
			*end = ret[i];
	} while (i > 2 && !ret[i--]);

	return 0;
}

static int huawei_wmi_battery_set(int start, int end)
{
	union hwmi_arg arg;
	int err;

	if (start < 0 || end < 0 || start > 100 || end > 100)
		return -EINVAL;

	arg.cmd = BATTERY_THRESH_SET;
	arg.args[2] = start;
	arg.args[3] = end;

	 
	if (quirks && quirks->battery_reset && start == 0 && end == 100) {
		err = huawei_wmi_battery_set(0, 0);
		if (err)
			return err;

		msleep(1000);
	}

	err = huawei_wmi_cmd(arg.cmd, NULL, 0);

	return err;
}

static ssize_t charge_control_start_threshold_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int err, start;

	err = huawei_wmi_battery_get(&start, NULL);
	if (err)
		return err;

	return sprintf(buf, "%d\n", start);
}

static ssize_t charge_control_end_threshold_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int err, end;

	err = huawei_wmi_battery_get(NULL, &end);
	if (err)
		return err;

	return sprintf(buf, "%d\n", end);
}

static ssize_t charge_control_thresholds_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int err, start, end;

	err = huawei_wmi_battery_get(&start, &end);
	if (err)
		return err;

	return sprintf(buf, "%d %d\n", start, end);
}

static ssize_t charge_control_start_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int err, start, end;

	err = huawei_wmi_battery_get(NULL, &end);
	if (err)
		return err;

	if (sscanf(buf, "%d", &start) != 1)
		return -EINVAL;

	err = huawei_wmi_battery_set(start, end);
	if (err)
		return err;

	return size;
}

static ssize_t charge_control_end_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int err, start, end;

	err = huawei_wmi_battery_get(&start, NULL);
	if (err)
		return err;

	if (sscanf(buf, "%d", &end) != 1)
		return -EINVAL;

	err = huawei_wmi_battery_set(start, end);
	if (err)
		return err;

	return size;
}

static ssize_t charge_control_thresholds_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int err, start, end;

	if (sscanf(buf, "%d %d", &start, &end) != 2)
		return -EINVAL;

	err = huawei_wmi_battery_set(start, end);
	if (err)
		return err;

	return size;
}

static DEVICE_ATTR_RW(charge_control_start_threshold);
static DEVICE_ATTR_RW(charge_control_end_threshold);
static DEVICE_ATTR_RW(charge_control_thresholds);

static int huawei_wmi_battery_add(struct power_supply *battery, struct acpi_battery_hook *hook)
{
	int err = 0;

	err = device_create_file(&battery->dev, &dev_attr_charge_control_start_threshold);
	if (err)
		return err;

	err = device_create_file(&battery->dev, &dev_attr_charge_control_end_threshold);
	if (err)
		device_remove_file(&battery->dev, &dev_attr_charge_control_start_threshold);

	return err;
}

static int huawei_wmi_battery_remove(struct power_supply *battery, struct acpi_battery_hook *hook)
{
	device_remove_file(&battery->dev, &dev_attr_charge_control_start_threshold);
	device_remove_file(&battery->dev, &dev_attr_charge_control_end_threshold);

	return 0;
}

static struct acpi_battery_hook huawei_wmi_battery_hook = {
	.add_battery = huawei_wmi_battery_add,
	.remove_battery = huawei_wmi_battery_remove,
	.name = "Huawei Battery Extension"
};

static void huawei_wmi_battery_setup(struct device *dev)
{
	struct huawei_wmi *huawei = dev_get_drvdata(dev);

	huawei->battery_available = true;
	if (huawei_wmi_battery_get(NULL, NULL)) {
		huawei->battery_available = false;
		return;
	}

	battery_hook_register(&huawei_wmi_battery_hook);
	device_create_file(dev, &dev_attr_charge_control_thresholds);
}

static void huawei_wmi_battery_exit(struct device *dev)
{
	struct huawei_wmi *huawei = dev_get_drvdata(dev);

	if (huawei->battery_available) {
		battery_hook_unregister(&huawei_wmi_battery_hook);
		device_remove_file(dev, &dev_attr_charge_control_thresholds);
	}
}

 

static int huawei_wmi_fn_lock_get(int *on)
{
	u8 ret[0x100] = { 0 };
	int err, i;

	err = huawei_wmi_cmd(FN_LOCK_GET, ret, 0x100);
	if (err)
		return err;

	 
	i = 1;
	do {
		if (on)
			*on = ret[i] - 1; 
	} while (i < 0xff && !ret[i++]);

	return 0;
}

static int huawei_wmi_fn_lock_set(int on)
{
	union hwmi_arg arg;

	arg.cmd = FN_LOCK_SET;
	arg.args[2] = on + 1; 

	return huawei_wmi_cmd(arg.cmd, NULL, 0);
}

static ssize_t fn_lock_state_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int err, on;

	err = huawei_wmi_fn_lock_get(&on);
	if (err)
		return err;

	return sprintf(buf, "%d\n", on);
}

static ssize_t fn_lock_state_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int on, err;

	if (kstrtoint(buf, 10, &on) ||
			on < 0 || on > 1)
		return -EINVAL;

	err = huawei_wmi_fn_lock_set(on);
	if (err)
		return err;

	return size;
}

static DEVICE_ATTR_RW(fn_lock_state);

static void huawei_wmi_fn_lock_setup(struct device *dev)
{
	struct huawei_wmi *huawei = dev_get_drvdata(dev);

	huawei->fn_lock_available = true;
	if (huawei_wmi_fn_lock_get(NULL)) {
		huawei->fn_lock_available = false;
		return;
	}

	device_create_file(dev, &dev_attr_fn_lock_state);
}

static void huawei_wmi_fn_lock_exit(struct device *dev)
{
	struct huawei_wmi *huawei = dev_get_drvdata(dev);

	if (huawei->fn_lock_available)
		device_remove_file(dev, &dev_attr_fn_lock_state);
}

 

static void huawei_wmi_debugfs_call_dump(struct seq_file *m, void *data,
		union acpi_object *obj)
{
	struct huawei_wmi *huawei = m->private;
	int i;

	switch (obj->type) {
	case ACPI_TYPE_INTEGER:
		seq_printf(m, "0x%llx", obj->integer.value);
		break;
	case ACPI_TYPE_STRING:
		seq_printf(m, "\"%.*s\"", obj->string.length, obj->string.pointer);
		break;
	case ACPI_TYPE_BUFFER:
		seq_puts(m, "{");
		for (i = 0; i < obj->buffer.length; i++) {
			seq_printf(m, "0x%02x", obj->buffer.pointer[i]);
			if (i < obj->buffer.length - 1)
				seq_puts(m, ",");
		}
		seq_puts(m, "}");
		break;
	case ACPI_TYPE_PACKAGE:
		seq_puts(m, "[");
		for (i = 0; i < obj->package.count; i++) {
			huawei_wmi_debugfs_call_dump(m, huawei, &obj->package.elements[i]);
			if (i < obj->package.count - 1)
				seq_puts(m, ",");
		}
		seq_puts(m, "]");
		break;
	default:
		dev_err(huawei->dev, "Unexpected obj type, got %d\n", obj->type);
		return;
	}
}

static int huawei_wmi_debugfs_call_show(struct seq_file *m, void *data)
{
	struct huawei_wmi *huawei = m->private;
	struct acpi_buffer out = { ACPI_ALLOCATE_BUFFER, NULL };
	struct acpi_buffer in;
	union acpi_object *obj;
	int err;

	in.length = sizeof(u64);
	in.pointer = &huawei->debug.arg;

	err = huawei_wmi_call(huawei, &in, &out);
	if (err)
		return err;

	obj = out.pointer;
	if (!obj) {
		err = -EIO;
		goto fail_debugfs_call;
	}

	huawei_wmi_debugfs_call_dump(m, huawei, obj);

fail_debugfs_call:
	kfree(out.pointer);
	return err;
}

DEFINE_SHOW_ATTRIBUTE(huawei_wmi_debugfs_call);

static void huawei_wmi_debugfs_setup(struct device *dev)
{
	struct huawei_wmi *huawei = dev_get_drvdata(dev);

	huawei->debug.root = debugfs_create_dir("huawei-wmi", NULL);

	debugfs_create_x64("arg", 0644, huawei->debug.root,
		&huawei->debug.arg);
	debugfs_create_file("call", 0400,
		huawei->debug.root, huawei, &huawei_wmi_debugfs_call_fops);
}

static void huawei_wmi_debugfs_exit(struct device *dev)
{
	struct huawei_wmi *huawei = dev_get_drvdata(dev);

	debugfs_remove_recursive(huawei->debug.root);
}

 

static void huawei_wmi_process_key(struct input_dev *idev, int code)
{
	const struct key_entry *key;

	 
	if (code == 0x80) {
		struct acpi_buffer response = { ACPI_ALLOCATE_BUFFER, NULL };
		union acpi_object *obj;
		acpi_status status;

		status = wmi_query_block(WMI0_EXPENSIVE_GUID, 0, &response);
		if (ACPI_FAILURE(status))
			return;

		obj = (union acpi_object *)response.pointer;
		if (obj && obj->type == ACPI_TYPE_INTEGER)
			code = obj->integer.value;

		kfree(response.pointer);
	}

	key = sparse_keymap_entry_from_scancode(idev, code);
	if (!key) {
		dev_info(&idev->dev, "Unknown key pressed, code: 0x%04x\n", code);
		return;
	}

	if (quirks && !quirks->report_brightness &&
			(key->sw.code == KEY_BRIGHTNESSDOWN ||
			key->sw.code == KEY_BRIGHTNESSUP))
		return;

	sparse_keymap_report_entry(idev, key, 1, true);
}

static void huawei_wmi_input_notify(u32 value, void *context)
{
	struct input_dev *idev = (struct input_dev *)context;
	struct acpi_buffer response = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	acpi_status status;

	status = wmi_get_event_data(value, &response);
	if (ACPI_FAILURE(status)) {
		dev_err(&idev->dev, "Unable to get event data\n");
		return;
	}

	obj = (union acpi_object *)response.pointer;
	if (obj && obj->type == ACPI_TYPE_INTEGER)
		huawei_wmi_process_key(idev, obj->integer.value);
	else
		dev_err(&idev->dev, "Bad response type\n");

	kfree(response.pointer);
}

static int huawei_wmi_input_setup(struct device *dev, const char *guid)
{
	struct input_dev *idev;
	acpi_status status;
	int err;

	idev = devm_input_allocate_device(dev);
	if (!idev)
		return -ENOMEM;

	idev->name = "Huawei WMI hotkeys";
	idev->phys = "wmi/input0";
	idev->id.bustype = BUS_HOST;
	idev->dev.parent = dev;

	err = sparse_keymap_setup(idev, huawei_wmi_keymap, NULL);
	if (err)
		return err;

	err = input_register_device(idev);
	if (err)
		return err;

	status = wmi_install_notify_handler(guid, huawei_wmi_input_notify, idev);
	if (ACPI_FAILURE(status))
		return -EIO;

	return 0;
}

static void huawei_wmi_input_exit(struct device *dev, const char *guid)
{
	wmi_remove_notify_handler(guid);
}

 

static const struct wmi_device_id huawei_wmi_events_id_table[] = {
	{ .guid_string = WMI0_EVENT_GUID },
	{ .guid_string = HWMI_EVENT_GUID },
	{  }
};

static int huawei_wmi_probe(struct platform_device *pdev)
{
	const struct wmi_device_id *guid = huawei_wmi_events_id_table;
	int err;

	platform_set_drvdata(pdev, huawei_wmi);
	huawei_wmi->dev = &pdev->dev;

	while (*guid->guid_string) {
		if (wmi_has_guid(guid->guid_string)) {
			err = huawei_wmi_input_setup(&pdev->dev, guid->guid_string);
			if (err) {
				dev_err(&pdev->dev, "Failed to setup input on %s\n", guid->guid_string);
				return err;
			}
		}

		guid++;
	}

	if (wmi_has_guid(HWMI_METHOD_GUID)) {
		mutex_init(&huawei_wmi->wmi_lock);

		huawei_wmi_leds_setup(&pdev->dev);
		huawei_wmi_fn_lock_setup(&pdev->dev);
		huawei_wmi_battery_setup(&pdev->dev);
		huawei_wmi_debugfs_setup(&pdev->dev);
	}

	return 0;
}

static void huawei_wmi_remove(struct platform_device *pdev)
{
	const struct wmi_device_id *guid = huawei_wmi_events_id_table;

	while (*guid->guid_string) {
		if (wmi_has_guid(guid->guid_string))
			huawei_wmi_input_exit(&pdev->dev, guid->guid_string);

		guid++;
	}

	if (wmi_has_guid(HWMI_METHOD_GUID)) {
		huawei_wmi_debugfs_exit(&pdev->dev);
		huawei_wmi_battery_exit(&pdev->dev);
		huawei_wmi_fn_lock_exit(&pdev->dev);
	}
}

static struct platform_driver huawei_wmi_driver = {
	.driver = {
		.name = "huawei-wmi",
	},
	.probe = huawei_wmi_probe,
	.remove_new = huawei_wmi_remove,
};

static __init int huawei_wmi_init(void)
{
	struct platform_device *pdev;
	int err;

	huawei_wmi = kzalloc(sizeof(struct huawei_wmi), GFP_KERNEL);
	if (!huawei_wmi)
		return -ENOMEM;

	quirks = &quirk_unknown;
	dmi_check_system(huawei_quirks);
	if (battery_reset != -1)
		quirks->battery_reset = battery_reset;
	if (report_brightness != -1)
		quirks->report_brightness = report_brightness;

	err = platform_driver_register(&huawei_wmi_driver);
	if (err)
		goto pdrv_err;

	pdev = platform_device_register_simple("huawei-wmi", PLATFORM_DEVID_NONE, NULL, 0);
	if (IS_ERR(pdev)) {
		err = PTR_ERR(pdev);
		goto pdev_err;
	}

	return 0;

pdev_err:
	platform_driver_unregister(&huawei_wmi_driver);
pdrv_err:
	kfree(huawei_wmi);
	return err;
}

static __exit void huawei_wmi_exit(void)
{
	struct platform_device *pdev = to_platform_device(huawei_wmi->dev);

	platform_device_unregister(pdev);
	platform_driver_unregister(&huawei_wmi_driver);

	kfree(huawei_wmi);
}

module_init(huawei_wmi_init);
module_exit(huawei_wmi_exit);

MODULE_ALIAS("wmi:"HWMI_METHOD_GUID);
MODULE_DEVICE_TABLE(wmi, huawei_wmi_events_id_table);
MODULE_AUTHOR("Ayman Bagabas <ayman.bagabas@gmail.com>");
MODULE_DESCRIPTION("Huawei WMI laptop extras driver");
MODULE_LICENSE("GPL v2");
