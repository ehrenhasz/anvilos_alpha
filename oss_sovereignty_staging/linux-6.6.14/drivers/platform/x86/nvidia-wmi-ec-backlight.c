
 

#include <linux/acpi.h>
#include <linux/backlight.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_data/x86/nvidia-wmi-ec-backlight.h>
#include <linux/types.h>
#include <linux/wmi.h>
#include <acpi/video.h>

static bool force;
module_param(force, bool, 0444);
MODULE_PARM_DESC(force, "Force loading (disable acpi_backlight=xxx checks");

 
static int wmi_brightness_notify(struct wmi_device *w, enum wmi_brightness_method id, enum wmi_brightness_mode mode, u32 *val)
{
	struct wmi_brightness_args args = {
		.mode = mode,
		.val = 0,
		.ret = 0,
	};
	struct acpi_buffer buf = { (acpi_size)sizeof(args), &args };
	acpi_status status;

	if (id < WMI_BRIGHTNESS_METHOD_LEVEL ||
	    id >= WMI_BRIGHTNESS_METHOD_MAX ||
	    mode < WMI_BRIGHTNESS_MODE_GET || mode >= WMI_BRIGHTNESS_MODE_MAX)
		return -EINVAL;

	if (mode == WMI_BRIGHTNESS_MODE_SET)
		args.val = *val;

	status = wmidev_evaluate_method(w, 0, id, &buf, &buf);
	if (ACPI_FAILURE(status)) {
		dev_err(&w->dev, "EC backlight control failed: %s\n",
			acpi_format_exception(status));
		return -EIO;
	}

	if (mode != WMI_BRIGHTNESS_MODE_SET)
		*val = args.ret;

	return 0;
}

static int nvidia_wmi_ec_backlight_update_status(struct backlight_device *bd)
{
	struct wmi_device *wdev = bl_get_data(bd);

	return wmi_brightness_notify(wdev, WMI_BRIGHTNESS_METHOD_LEVEL,
	                             WMI_BRIGHTNESS_MODE_SET,
			             &bd->props.brightness);
}

static int nvidia_wmi_ec_backlight_get_brightness(struct backlight_device *bd)
{
	struct wmi_device *wdev = bl_get_data(bd);
	u32 level;
	int ret;

	ret = wmi_brightness_notify(wdev, WMI_BRIGHTNESS_METHOD_LEVEL,
	                            WMI_BRIGHTNESS_MODE_GET, &level);
	if (ret < 0)
		return ret;

	return level;
}

static const struct backlight_ops nvidia_wmi_ec_backlight_ops = {
	.update_status = nvidia_wmi_ec_backlight_update_status,
	.get_brightness = nvidia_wmi_ec_backlight_get_brightness,
};

static int nvidia_wmi_ec_backlight_probe(struct wmi_device *wdev, const void *ctx)
{
	struct backlight_properties props = {};
	struct backlight_device *bdev;
	int ret;

	 
	if (!force && acpi_video_get_backlight_type() != acpi_backlight_nvidia_wmi_ec)
		return -ENODEV;

	 
	props.type = BACKLIGHT_FIRMWARE;

	ret = wmi_brightness_notify(wdev, WMI_BRIGHTNESS_METHOD_LEVEL,
	                           WMI_BRIGHTNESS_MODE_GET_MAX_LEVEL,
	                           &props.max_brightness);
	if (ret)
		return ret;

	ret = wmi_brightness_notify(wdev, WMI_BRIGHTNESS_METHOD_LEVEL,
	                           WMI_BRIGHTNESS_MODE_GET, &props.brightness);
	if (ret)
		return ret;

	bdev = devm_backlight_device_register(&wdev->dev,
	                                      "nvidia_wmi_ec_backlight",
					      &wdev->dev, wdev,
					      &nvidia_wmi_ec_backlight_ops,
					      &props);
	return PTR_ERR_OR_ZERO(bdev);
}

static const struct wmi_device_id nvidia_wmi_ec_backlight_id_table[] = {
	{ .guid_string = WMI_BRIGHTNESS_GUID },
	{ }
};
MODULE_DEVICE_TABLE(wmi, nvidia_wmi_ec_backlight_id_table);

static struct wmi_driver nvidia_wmi_ec_backlight_driver = {
	.driver = {
		.name = "nvidia-wmi-ec-backlight",
	},
	.probe = nvidia_wmi_ec_backlight_probe,
	.id_table = nvidia_wmi_ec_backlight_id_table,
};
module_wmi_driver(nvidia_wmi_ec_backlight_driver);

MODULE_AUTHOR("Daniel Dadap <ddadap@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA WMI EC Backlight driver");
MODULE_LICENSE("GPL");
