
 

#include <linux/acpi.h>
#include <linux/backlight.h>
#include <linux/ctype.h>
#include <linux/i8042.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/serio.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <acpi/video.h>

MODULE_AUTHOR("Hiroshi Miura <miura@da-cha.org>");
MODULE_AUTHOR("David Bronaugh <dbronaugh@linuxboxen.org>");
MODULE_AUTHOR("Harald Welte <laforge@gnumonks.org>");
MODULE_AUTHOR("Martin Lucina <mato@kotelna.sk>");
MODULE_AUTHOR("Kenneth Chan <kenneth.t.chan@gmail.com>");
MODULE_DESCRIPTION("ACPI HotKey driver for Panasonic Let's Note laptops");
MODULE_LICENSE("GPL");

#define LOGPREFIX "pcc_acpi: "

 
 
#define METHOD_HKEY_QUERY	"HINF"
#define METHOD_HKEY_SQTY	"SQTY"
#define METHOD_HKEY_SINF	"SINF"
#define METHOD_HKEY_SSET	"SSET"
#define METHOD_ECWR		"\\_SB.ECWR"
#define HKEY_NOTIFY		0x80
#define ECO_MODE_OFF		0x00
#define ECO_MODE_ON		0x80

#define ACPI_PCC_DRIVER_NAME	"Panasonic Laptop Support"
#define ACPI_PCC_DEVICE_NAME	"Hotkey"
#define ACPI_PCC_CLASS		"pcc"

#define ACPI_PCC_INPUT_PHYS	"panasonic/hkey0"

 
enum SINF_BITS { SINF_NUM_BATTERIES = 0,
		 SINF_LCD_TYPE,
		 SINF_AC_MAX_BRIGHT,
		 SINF_AC_MIN_BRIGHT,
		 SINF_AC_CUR_BRIGHT,
		 SINF_DC_MAX_BRIGHT,
		 SINF_DC_MIN_BRIGHT,
		 SINF_DC_CUR_BRIGHT,
		 SINF_MUTE,
		 SINF_RESERVED,
		 SINF_ECO_MODE = 0x0A,
		 SINF_CUR_BRIGHT = 0x0D,
		 SINF_STICKY_KEY = 0x80,
	};
 

static int acpi_pcc_hotkey_add(struct acpi_device *device);
static void acpi_pcc_hotkey_remove(struct acpi_device *device);
static void acpi_pcc_hotkey_notify(struct acpi_device *device, u32 event);

static const struct acpi_device_id pcc_device_ids[] = {
	{ "MAT0012", 0},
	{ "MAT0013", 0},
	{ "MAT0018", 0},
	{ "MAT0019", 0},
	{ "", 0},
};
MODULE_DEVICE_TABLE(acpi, pcc_device_ids);

#ifdef CONFIG_PM_SLEEP
static int acpi_pcc_hotkey_resume(struct device *dev);
#endif
static SIMPLE_DEV_PM_OPS(acpi_pcc_hotkey_pm, NULL, acpi_pcc_hotkey_resume);

static struct acpi_driver acpi_pcc_driver = {
	.name =		ACPI_PCC_DRIVER_NAME,
	.class =	ACPI_PCC_CLASS,
	.ids =		pcc_device_ids,
	.ops =		{
				.add =		acpi_pcc_hotkey_add,
				.remove =	acpi_pcc_hotkey_remove,
				.notify =	acpi_pcc_hotkey_notify,
			},
	.drv.pm =	&acpi_pcc_hotkey_pm,
};

static const struct key_entry panasonic_keymap[] = {
	{ KE_KEY, 0, { KEY_RESERVED } },
	{ KE_KEY, 1, { KEY_BRIGHTNESSDOWN } },
	{ KE_KEY, 2, { KEY_BRIGHTNESSUP } },
	{ KE_KEY, 3, { KEY_DISPLAYTOGGLE } },
	{ KE_KEY, 4, { KEY_MUTE } },
	{ KE_KEY, 5, { KEY_VOLUMEDOWN } },
	{ KE_KEY, 6, { KEY_VOLUMEUP } },
	{ KE_KEY, 7, { KEY_SLEEP } },
	{ KE_KEY, 8, { KEY_PROG1 } },  
	{ KE_KEY, 9, { KEY_BATTERY } },
	{ KE_KEY, 10, { KEY_SUSPEND } },
	{ KE_END, 0 }
};

struct pcc_acpi {
	acpi_handle		handle;
	unsigned long		num_sifr;
	int			sticky_key;
	int			eco_mode;
	int			mute;
	int			ac_brightness;
	int			dc_brightness;
	int			current_brightness;
	u32			*sinf;
	struct acpi_device	*device;
	struct input_dev	*input_dev;
	struct backlight_device	*backlight;
	struct platform_device	*platform;
};

 
static bool panasonic_i8042_filter(unsigned char data, unsigned char str,
				   struct serio *port)
{
	static bool extended;

	if (str & I8042_STR_AUXDATA)
		return false;

	if (data == 0xe0) {
		extended = true;
		return true;
	} else if (extended) {
		extended = false;

		switch (data & 0x7f) {
		case 0x20:  
		case 0x2e:  
		case 0x30:  
			return true;
		default:
			 
			serio_interrupt(port, 0xe0, 0);
			return false;
		}
	}

	return false;
}

 
static int acpi_pcc_write_sset(struct pcc_acpi *pcc, int func, int val)
{
	union acpi_object in_objs[] = {
		{ .integer.type  = ACPI_TYPE_INTEGER,
		  .integer.value = func, },
		{ .integer.type  = ACPI_TYPE_INTEGER,
		  .integer.value = val, },
	};
	struct acpi_object_list params = {
		.count   = ARRAY_SIZE(in_objs),
		.pointer = in_objs,
	};
	acpi_status status = AE_OK;

	status = acpi_evaluate_object(pcc->handle, METHOD_HKEY_SSET,
				      &params, NULL);

	return (status == AE_OK) ? 0 : -EIO;
}

static inline int acpi_pcc_get_sqty(struct acpi_device *device)
{
	unsigned long long s;
	acpi_status status;

	status = acpi_evaluate_integer(device->handle, METHOD_HKEY_SQTY,
				       NULL, &s);
	if (ACPI_SUCCESS(status))
		return s;
	else {
		pr_err("evaluation error HKEY.SQTY\n");
		return -EINVAL;
	}
}

static int acpi_pcc_retrieve_biosdata(struct pcc_acpi *pcc)
{
	acpi_status status;
	struct acpi_buffer buffer = {ACPI_ALLOCATE_BUFFER, NULL};
	union acpi_object *hkey = NULL;
	int i;

	status = acpi_evaluate_object(pcc->handle, METHOD_HKEY_SINF, NULL,
				      &buffer);
	if (ACPI_FAILURE(status)) {
		pr_err("evaluation error HKEY.SINF\n");
		return 0;
	}

	hkey = buffer.pointer;
	if (!hkey || (hkey->type != ACPI_TYPE_PACKAGE)) {
		pr_err("Invalid HKEY.SINF\n");
		status = AE_ERROR;
		goto end;
	}

	if (pcc->num_sifr < hkey->package.count) {
		pr_err("SQTY reports bad SINF length\n");
		status = AE_ERROR;
		goto end;
	}

	for (i = 0; i < hkey->package.count; i++) {
		union acpi_object *element = &(hkey->package.elements[i]);
		if (likely(element->type == ACPI_TYPE_INTEGER)) {
			pcc->sinf[i] = element->integer.value;
		} else
			pr_err("Invalid HKEY.SINF data\n");
	}
	pcc->sinf[hkey->package.count] = -1;

end:
	kfree(buffer.pointer);
	return status == AE_OK;
}

 

 

static int bl_get(struct backlight_device *bd)
{
	struct pcc_acpi *pcc = bl_get_data(bd);

	if (!acpi_pcc_retrieve_biosdata(pcc))
		return -EIO;

	return pcc->sinf[SINF_AC_CUR_BRIGHT];
}

static int bl_set_status(struct backlight_device *bd)
{
	struct pcc_acpi *pcc = bl_get_data(bd);
	int bright = bd->props.brightness;
	int rc;

	if (!acpi_pcc_retrieve_biosdata(pcc))
		return -EIO;

	if (bright < pcc->sinf[SINF_AC_MIN_BRIGHT])
		bright = pcc->sinf[SINF_AC_MIN_BRIGHT];

	if (bright < pcc->sinf[SINF_DC_MIN_BRIGHT])
		bright = pcc->sinf[SINF_DC_MIN_BRIGHT];

	if (bright < pcc->sinf[SINF_AC_MIN_BRIGHT] ||
	    bright > pcc->sinf[SINF_AC_MAX_BRIGHT])
		return -EINVAL;

	rc = acpi_pcc_write_sset(pcc, SINF_AC_CUR_BRIGHT, bright);
	if (rc < 0)
		return rc;

	return acpi_pcc_write_sset(pcc, SINF_DC_CUR_BRIGHT, bright);
}

static const struct backlight_ops pcc_backlight_ops = {
	.get_brightness	= bl_get,
	.update_status	= bl_set_status,
};


 

static acpi_status check_optd_present(void)
{
	acpi_status status = AE_OK;
	acpi_handle handle;

	status = acpi_get_handle(NULL, "\\_SB.STAT", &handle);
	if (ACPI_FAILURE(status))
		goto out;
	status = acpi_get_handle(NULL, "\\_SB.FBAY", &handle);
	if (ACPI_FAILURE(status))
		goto out;
	status = acpi_get_handle(NULL, "\\_SB.CDDI", &handle);
	if (ACPI_FAILURE(status))
		goto out;

out:
	return status;
}

 

static int get_optd_power_state(void)
{
	acpi_status status;
	unsigned long long state;
	int result;

	status = acpi_evaluate_integer(NULL, "\\_SB.STAT", NULL, &state);
	if (ACPI_FAILURE(status)) {
		pr_err("evaluation error _SB.STAT\n");
		result = -EIO;
		goto out;
	}
	switch (state) {
	case 0:  
		result = 0;
		break;
	case 0x0f:  
		result = 1;
		break;
	default:
		result = -EIO;
		break;
	}

out:
	return result;
}

 

static int set_optd_power_state(int new_state)
{
	int result;
	acpi_status status;

	result = get_optd_power_state();
	if (result < 0)
		goto out;
	if (new_state == result)
		goto out;

	switch (new_state) {
	case 0:  
		 
		status = acpi_evaluate_object(NULL, "\\_SB.CDDR", NULL, NULL);
		if (ACPI_FAILURE(status)) {
			pr_err("evaluation error _SB.CDDR\n");
			result = -EIO;
		}
		break;
	case 1:  
		status = acpi_evaluate_object(NULL, "\\_SB.FBAY", NULL, NULL);
		if (ACPI_FAILURE(status)) {
			pr_err("evaluation error _SB.FBAY\n");
			result = -EIO;
		}
		break;
	default:
		result = -EINVAL;
		break;
	}

out:
	return result;
}


 

static ssize_t numbatt_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);

	if (!acpi_pcc_retrieve_biosdata(pcc))
		return -EIO;

	return sysfs_emit(buf, "%u\n", pcc->sinf[SINF_NUM_BATTERIES]);
}

static ssize_t lcdtype_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);

	if (!acpi_pcc_retrieve_biosdata(pcc))
		return -EIO;

	return sysfs_emit(buf, "%u\n", pcc->sinf[SINF_LCD_TYPE]);
}

static ssize_t mute_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);

	if (!acpi_pcc_retrieve_biosdata(pcc))
		return -EIO;

	return sysfs_emit(buf, "%u\n", pcc->sinf[SINF_MUTE]);
}

static ssize_t mute_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);
	int err, val;

	err = kstrtoint(buf, 0, &val);
	if (err)
		return err;
	if (val == 0 || val == 1) {
		acpi_pcc_write_sset(pcc, SINF_MUTE, val);
		pcc->mute = val;
	}

	return count;
}

static ssize_t sticky_key_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);

	if (!acpi_pcc_retrieve_biosdata(pcc))
		return -EIO;

	return sysfs_emit(buf, "%u\n", pcc->sticky_key);
}

static ssize_t sticky_key_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);
	int err, val;

	err = kstrtoint(buf, 0, &val);
	if (err)
		return err;
	if (val == 0 || val == 1) {
		acpi_pcc_write_sset(pcc, SINF_STICKY_KEY, val);
		pcc->sticky_key = val;
	}

	return count;
}

static ssize_t eco_mode_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);
	int result;

	if (!acpi_pcc_retrieve_biosdata(pcc))
		return -EIO;

	switch (pcc->sinf[SINF_ECO_MODE]) {
	case (ECO_MODE_OFF + 3):
		result = 0;
		break;
	case (ECO_MODE_ON + 3):
		result = 1;
		break;
	default:
		result = -EIO;
		break;
	}
	return sysfs_emit(buf, "%u\n", result);
}

static ssize_t eco_mode_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);
	int err, state;

	union acpi_object param[2];
	struct acpi_object_list input;
	acpi_status status;

	param[0].type = ACPI_TYPE_INTEGER;
	param[0].integer.value = 0x15;
	param[1].type = ACPI_TYPE_INTEGER;
	input.count = 2;
	input.pointer = param;

	err = kstrtoint(buf, 0, &state);
	if (err)
		return err;

	switch (state) {
	case 0:
		param[1].integer.value = ECO_MODE_OFF;
		pcc->sinf[SINF_ECO_MODE] = 0;
		pcc->eco_mode = 0;
		break;
	case 1:
		param[1].integer.value = ECO_MODE_ON;
		pcc->sinf[SINF_ECO_MODE] = 1;
		pcc->eco_mode = 1;
		break;
	default:
		 
		return count;
	}

	status = acpi_evaluate_object(NULL, METHOD_ECWR,
				       &input, NULL);
	if (ACPI_FAILURE(status)) {
		pr_err("%s evaluation failed\n", METHOD_ECWR);
		return -EINVAL;
	}

	return count;
}

static ssize_t ac_brightness_show(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);

	if (!acpi_pcc_retrieve_biosdata(pcc))
		return -EIO;

	return sysfs_emit(buf, "%u\n", pcc->sinf[SINF_AC_CUR_BRIGHT]);
}

static ssize_t ac_brightness_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);
	int err, val;

	err = kstrtoint(buf, 0, &val);
	if (err)
		return err;
	if (val >= 0 && val <= 255) {
		acpi_pcc_write_sset(pcc, SINF_AC_CUR_BRIGHT, val);
		pcc->ac_brightness = val;
	}

	return count;
}

static ssize_t dc_brightness_show(struct device *dev, struct device_attribute *attr,
				  char *buf)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);

	if (!acpi_pcc_retrieve_biosdata(pcc))
		return -EIO;

	return sysfs_emit(buf, "%u\n", pcc->sinf[SINF_DC_CUR_BRIGHT]);
}

static ssize_t dc_brightness_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);
	int err, val;

	err = kstrtoint(buf, 0, &val);
	if (err)
		return err;
	if (val >= 0 && val <= 255) {
		acpi_pcc_write_sset(pcc, SINF_DC_CUR_BRIGHT, val);
		pcc->dc_brightness = val;
	}

	return count;
}

static ssize_t current_brightness_show(struct device *dev, struct device_attribute *attr,
				       char *buf)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);

	if (!acpi_pcc_retrieve_biosdata(pcc))
		return -EIO;

	return sysfs_emit(buf, "%u\n", pcc->sinf[SINF_CUR_BRIGHT]);
}

static ssize_t current_brightness_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);
	int err, val;

	err = kstrtoint(buf, 0, &val);
	if (err)
		return err;

	if (val >= 0 && val <= 255) {
		err = acpi_pcc_write_sset(pcc, SINF_CUR_BRIGHT, val);
		pcc->current_brightness = val;
	}

	return count;
}

static ssize_t cdpower_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return sysfs_emit(buf, "%d\n", get_optd_power_state());
}

static ssize_t cdpower_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int err, val;

	err = kstrtoint(buf, 10, &val);
	if (err)
		return err;
	set_optd_power_state(val);
	return count;
}

static DEVICE_ATTR_RO(numbatt);
static DEVICE_ATTR_RO(lcdtype);
static DEVICE_ATTR_RW(mute);
static DEVICE_ATTR_RW(sticky_key);
static DEVICE_ATTR_RW(eco_mode);
static DEVICE_ATTR_RW(ac_brightness);
static DEVICE_ATTR_RW(dc_brightness);
static DEVICE_ATTR_RW(current_brightness);
static DEVICE_ATTR_RW(cdpower);

static struct attribute *pcc_sysfs_entries[] = {
	&dev_attr_numbatt.attr,
	&dev_attr_lcdtype.attr,
	&dev_attr_mute.attr,
	&dev_attr_sticky_key.attr,
	&dev_attr_eco_mode.attr,
	&dev_attr_ac_brightness.attr,
	&dev_attr_dc_brightness.attr,
	&dev_attr_current_brightness.attr,
	&dev_attr_cdpower.attr,
	NULL,
};

static const struct attribute_group pcc_attr_group = {
	.name	= NULL,		 
	.attrs	= pcc_sysfs_entries,
};


 

static int sleep_keydown_seen;
static void acpi_pcc_generate_keyinput(struct pcc_acpi *pcc)
{
	struct input_dev *hotk_input_dev = pcc->input_dev;
	int rc;
	unsigned long long result;
	unsigned int key;
	unsigned int updown;

	rc = acpi_evaluate_integer(pcc->handle, METHOD_HKEY_QUERY,
				   NULL, &result);
	if (ACPI_FAILURE(rc)) {
		pr_err("error getting hotkey status\n");
		return;
	}

	key = result & 0xf;
	updown = result & 0x80;  

	 
	if (key == 7 || key == 10) {
		if (updown)
			sleep_keydown_seen = 1;
		if (!sleep_keydown_seen)
			sparse_keymap_report_event(hotk_input_dev,
					key, 0x80, false);
	}

	 
	if ((key == 1 || key == 2) && acpi_video_handles_brightness_key_presses())
		return;

	if (!sparse_keymap_report_event(hotk_input_dev, key, updown, false))
		pr_err("Unknown hotkey event: 0x%04llx\n", result);
}

static void acpi_pcc_hotkey_notify(struct acpi_device *device, u32 event)
{
	struct pcc_acpi *pcc = acpi_driver_data(device);

	switch (event) {
	case HKEY_NOTIFY:
		acpi_pcc_generate_keyinput(pcc);
		break;
	default:
		 
		break;
	}
}

static void pcc_optd_notify(acpi_handle handle, u32 event, void *data)
{
	if (event != ACPI_NOTIFY_EJECT_REQUEST)
		return;

	set_optd_power_state(0);
}

static int pcc_register_optd_notifier(struct pcc_acpi *pcc, char *node)
{
	acpi_status status;
	acpi_handle handle;

	status = acpi_get_handle(NULL, node, &handle);

	if (ACPI_SUCCESS(status)) {
		status = acpi_install_notify_handler(handle,
				ACPI_SYSTEM_NOTIFY,
				pcc_optd_notify, pcc);
		if (ACPI_FAILURE(status))
			pr_err("Failed to register notify on %s\n", node);
	} else
		return -ENODEV;

	return 0;
}

static void pcc_unregister_optd_notifier(struct pcc_acpi *pcc, char *node)
{
	acpi_status status = AE_OK;
	acpi_handle handle;

	status = acpi_get_handle(NULL, node, &handle);

	if (ACPI_SUCCESS(status)) {
		status = acpi_remove_notify_handler(handle,
				ACPI_SYSTEM_NOTIFY,
				pcc_optd_notify);
		if (ACPI_FAILURE(status))
			pr_err("Error removing optd notify handler %s\n",
					node);
	}
}

static int acpi_pcc_init_input(struct pcc_acpi *pcc)
{
	struct input_dev *input_dev;
	int error;

	input_dev = input_allocate_device();
	if (!input_dev)
		return -ENOMEM;

	input_dev->name = ACPI_PCC_DRIVER_NAME;
	input_dev->phys = ACPI_PCC_INPUT_PHYS;
	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;

	error = sparse_keymap_setup(input_dev, panasonic_keymap, NULL);
	if (error) {
		pr_err("Unable to setup input device keymap\n");
		goto err_free_dev;
	}

	error = input_register_device(input_dev);
	if (error) {
		pr_err("Unable to register input device\n");
		goto err_free_dev;
	}

	pcc->input_dev = input_dev;
	return 0;

 err_free_dev:
	input_free_device(input_dev);
	return error;
}

 

#ifdef CONFIG_PM_SLEEP
static int acpi_pcc_hotkey_resume(struct device *dev)
{
	struct pcc_acpi *pcc;

	if (!dev)
		return -EINVAL;

	pcc = acpi_driver_data(to_acpi_device(dev));
	if (!pcc)
		return -EINVAL;

	acpi_pcc_write_sset(pcc, SINF_MUTE, pcc->mute);
	acpi_pcc_write_sset(pcc, SINF_ECO_MODE, pcc->eco_mode);
	acpi_pcc_write_sset(pcc, SINF_STICKY_KEY, pcc->sticky_key);
	acpi_pcc_write_sset(pcc, SINF_AC_CUR_BRIGHT, pcc->ac_brightness);
	acpi_pcc_write_sset(pcc, SINF_DC_CUR_BRIGHT, pcc->dc_brightness);
	acpi_pcc_write_sset(pcc, SINF_CUR_BRIGHT, pcc->current_brightness);

	return 0;
}
#endif

static int acpi_pcc_hotkey_add(struct acpi_device *device)
{
	struct backlight_properties props;
	struct pcc_acpi *pcc;
	int num_sifr, result;

	if (!device)
		return -EINVAL;

	num_sifr = acpi_pcc_get_sqty(device);

	if (num_sifr < 0 || num_sifr > 255) {
		pr_err("num_sifr out of range");
		return -ENODEV;
	}

	pcc = kzalloc(sizeof(struct pcc_acpi), GFP_KERNEL);
	if (!pcc) {
		pr_err("Couldn't allocate mem for pcc");
		return -ENOMEM;
	}

	pcc->sinf = kcalloc(num_sifr + 1, sizeof(u32), GFP_KERNEL);
	if (!pcc->sinf) {
		result = -ENOMEM;
		goto out_hotkey;
	}

	pcc->device = device;
	pcc->handle = device->handle;
	pcc->num_sifr = num_sifr;
	device->driver_data = pcc;
	strcpy(acpi_device_name(device), ACPI_PCC_DEVICE_NAME);
	strcpy(acpi_device_class(device), ACPI_PCC_CLASS);

	result = acpi_pcc_init_input(pcc);
	if (result) {
		pr_err("Error installing keyinput handler\n");
		goto out_sinf;
	}

	if (!acpi_pcc_retrieve_biosdata(pcc)) {
		result = -EIO;
		pr_err("Couldn't retrieve BIOS data\n");
		goto out_input;
	}

	if (acpi_video_get_backlight_type() == acpi_backlight_vendor) {
		 
		memset(&props, 0, sizeof(struct backlight_properties));
		props.type = BACKLIGHT_PLATFORM;
		props.max_brightness = pcc->sinf[SINF_AC_MAX_BRIGHT];

		pcc->backlight = backlight_device_register("panasonic", NULL, pcc,
							   &pcc_backlight_ops, &props);
		if (IS_ERR(pcc->backlight)) {
			result = PTR_ERR(pcc->backlight);
			goto out_input;
		}

		 
		pcc->backlight->props.brightness = pcc->sinf[SINF_AC_CUR_BRIGHT];
	}

	 
	acpi_pcc_write_sset(pcc, SINF_STICKY_KEY, 0);
	pcc->sticky_key = 0;

	pcc->eco_mode = pcc->sinf[SINF_ECO_MODE];
	pcc->mute = pcc->sinf[SINF_MUTE];
	pcc->ac_brightness = pcc->sinf[SINF_AC_CUR_BRIGHT];
	pcc->dc_brightness = pcc->sinf[SINF_DC_CUR_BRIGHT];
	pcc->current_brightness = pcc->sinf[SINF_CUR_BRIGHT];

	 
	result = sysfs_create_group(&device->dev.kobj, &pcc_attr_group);
	if (result)
		goto out_backlight;

	 
	if (ACPI_SUCCESS(check_optd_present())) {
		pcc->platform = platform_device_register_simple("panasonic",
			PLATFORM_DEVID_NONE, NULL, 0);
		if (IS_ERR(pcc->platform)) {
			result = PTR_ERR(pcc->platform);
			goto out_backlight;
		}
		result = device_create_file(&pcc->platform->dev,
			&dev_attr_cdpower);
		pcc_register_optd_notifier(pcc, "\\_SB.PCI0.EHCI.ERHB.OPTD");
		if (result)
			goto out_platform;
	} else {
		pcc->platform = NULL;
	}

	i8042_install_filter(panasonic_i8042_filter);
	return 0;

out_platform:
	platform_device_unregister(pcc->platform);
out_backlight:
	backlight_device_unregister(pcc->backlight);
out_input:
	input_unregister_device(pcc->input_dev);
out_sinf:
	kfree(pcc->sinf);
out_hotkey:
	kfree(pcc);

	return result;
}

static void acpi_pcc_hotkey_remove(struct acpi_device *device)
{
	struct pcc_acpi *pcc = acpi_driver_data(device);

	if (!device || !pcc)
		return;

	i8042_remove_filter(panasonic_i8042_filter);

	if (pcc->platform) {
		device_remove_file(&pcc->platform->dev, &dev_attr_cdpower);
		platform_device_unregister(pcc->platform);
	}
	pcc_unregister_optd_notifier(pcc, "\\_SB.PCI0.EHCI.ERHB.OPTD");

	sysfs_remove_group(&device->dev.kobj, &pcc_attr_group);

	backlight_device_unregister(pcc->backlight);

	input_unregister_device(pcc->input_dev);

	kfree(pcc->sinf);
	kfree(pcc);
}

module_acpi_driver(acpi_pcc_driver);
