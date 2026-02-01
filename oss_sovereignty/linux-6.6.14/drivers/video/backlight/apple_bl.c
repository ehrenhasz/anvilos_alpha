
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/acpi.h>
#include <linux/atomic.h>
#include <acpi/video.h>

static struct backlight_device *apple_backlight_device;

struct hw_data {
	 
	unsigned long iostart;
	unsigned long iolen;
	 
	const struct backlight_ops backlight_ops;
	void (*set_brightness)(int);
};

static const struct hw_data *hw_data;

 
static int debug;
module_param_named(debug, debug, int, 0644);
MODULE_PARM_DESC(debug, "Set to one to enable debugging messages.");

 
static void intel_chipset_set_brightness(int intensity)
{
	outb(0x04 | (intensity << 4), 0xb3);
	outb(0xbf, 0xb2);
}

static int intel_chipset_send_intensity(struct backlight_device *bd)
{
	int intensity = bd->props.brightness;

	if (debug)
		pr_debug("setting brightness to %d\n", intensity);

	intel_chipset_set_brightness(intensity);
	return 0;
}

static int intel_chipset_get_intensity(struct backlight_device *bd)
{
	int intensity;

	outb(0x03, 0xb3);
	outb(0xbf, 0xb2);
	intensity = inb(0xb3) >> 4;

	if (debug)
		pr_debug("read brightness of %d\n", intensity);

	return intensity;
}

static const struct hw_data intel_chipset_data = {
	.iostart = 0xb2,
	.iolen = 2,
	.backlight_ops	= {
		.options	= BL_CORE_SUSPENDRESUME,
		.get_brightness	= intel_chipset_get_intensity,
		.update_status	= intel_chipset_send_intensity,
	},
	.set_brightness = intel_chipset_set_brightness,
};

 
static void nvidia_chipset_set_brightness(int intensity)
{
	outb(0x04 | (intensity << 4), 0x52f);
	outb(0xbf, 0x52e);
}

static int nvidia_chipset_send_intensity(struct backlight_device *bd)
{
	int intensity = bd->props.brightness;

	if (debug)
		pr_debug("setting brightness to %d\n", intensity);

	nvidia_chipset_set_brightness(intensity);
	return 0;
}

static int nvidia_chipset_get_intensity(struct backlight_device *bd)
{
	int intensity;

	outb(0x03, 0x52f);
	outb(0xbf, 0x52e);
	intensity = inb(0x52f) >> 4;

	if (debug)
		pr_debug("read brightness of %d\n", intensity);

	return intensity;
}

static const struct hw_data nvidia_chipset_data = {
	.iostart = 0x52e,
	.iolen = 2,
	.backlight_ops		= {
		.options	= BL_CORE_SUSPENDRESUME,
		.get_brightness	= nvidia_chipset_get_intensity,
		.update_status	= nvidia_chipset_send_intensity
	},
	.set_brightness = nvidia_chipset_set_brightness,
};

static int apple_bl_add(struct acpi_device *dev)
{
	struct backlight_properties props;
	struct pci_dev *host;
	int intensity;

	host = pci_get_domain_bus_and_slot(0, 0, 0);

	if (!host) {
		pr_err("unable to find PCI host\n");
		return -ENODEV;
	}

	if (host->vendor == PCI_VENDOR_ID_INTEL)
		hw_data = &intel_chipset_data;
	else if (host->vendor == PCI_VENDOR_ID_NVIDIA)
		hw_data = &nvidia_chipset_data;

	pci_dev_put(host);

	if (!hw_data) {
		pr_err("unknown hardware\n");
		return -ENODEV;
	}

	 

	intensity = hw_data->backlight_ops.get_brightness(NULL);

	if (!intensity) {
		hw_data->set_brightness(1);
		if (!hw_data->backlight_ops.get_brightness(NULL))
			return -ENODEV;

		hw_data->set_brightness(0);
	}

	if (!request_region(hw_data->iostart, hw_data->iolen,
			    "Apple backlight"))
		return -ENXIO;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_PLATFORM;
	props.max_brightness = 15;
	apple_backlight_device = backlight_device_register("apple_backlight",
				  NULL, NULL, &hw_data->backlight_ops, &props);

	if (IS_ERR(apple_backlight_device)) {
		release_region(hw_data->iostart, hw_data->iolen);
		return PTR_ERR(apple_backlight_device);
	}

	apple_backlight_device->props.brightness =
		hw_data->backlight_ops.get_brightness(apple_backlight_device);
	backlight_update_status(apple_backlight_device);

	return 0;
}

static void apple_bl_remove(struct acpi_device *dev)
{
	backlight_device_unregister(apple_backlight_device);

	release_region(hw_data->iostart, hw_data->iolen);
	hw_data = NULL;
}

static const struct acpi_device_id apple_bl_ids[] = {
	{"APP0002", 0},
	{"", 0},
};

static struct acpi_driver apple_bl_driver = {
	.name = "Apple backlight",
	.ids = apple_bl_ids,
	.ops = {
		.add = apple_bl_add,
		.remove = apple_bl_remove,
	},
};

static int __init apple_bl_init(void)
{
	 
	if (acpi_video_get_backlight_type() != acpi_backlight_vendor)
		return -ENODEV;

	return acpi_bus_register_driver(&apple_bl_driver);
}

static void __exit apple_bl_exit(void)
{
	acpi_bus_unregister_driver(&apple_bl_driver);
}

module_init(apple_bl_init);
module_exit(apple_bl_exit);

MODULE_AUTHOR("Matthew Garrett <mjg@redhat.com>");
MODULE_DESCRIPTION("Apple Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(acpi, apple_bl_ids);
MODULE_ALIAS("mbp_nvidia_bl");
