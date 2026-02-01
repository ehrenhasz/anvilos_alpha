
 

#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <uapi/linux/usb/audio.h>
#include "usb.h"

static inline const char *plural(int n)
{
	return (n == 1 ? "" : "s");
}

static int is_rndis(struct usb_interface_descriptor *desc)
{
	return desc->bInterfaceClass == USB_CLASS_COMM
		&& desc->bInterfaceSubClass == 2
		&& desc->bInterfaceProtocol == 0xff;
}

static int is_activesync(struct usb_interface_descriptor *desc)
{
	return desc->bInterfaceClass == USB_CLASS_MISC
		&& desc->bInterfaceSubClass == 1
		&& desc->bInterfaceProtocol == 1;
}

static bool is_audio(struct usb_interface_descriptor *desc)
{
	return desc->bInterfaceClass == USB_CLASS_AUDIO;
}

static bool is_uac3_config(struct usb_interface_descriptor *desc)
{
	return desc->bInterfaceProtocol == UAC_VERSION_3;
}

int usb_choose_configuration(struct usb_device *udev)
{
	int i;
	int num_configs;
	int insufficient_power = 0;
	struct usb_host_config *c, *best;

	if (usb_device_is_owned(udev))
		return 0;

	best = NULL;
	c = udev->config;
	num_configs = udev->descriptor.bNumConfigurations;
	for (i = 0; i < num_configs; (i++, c++)) {
		struct usb_interface_descriptor	*desc = NULL;

		 
		if (c->desc.bNumInterfaces > 0)
			desc = &c->intf_cache[0]->altsetting->desc;

		 
#if 0
		 
		if (bus_powered && (c->desc.bmAttributes &
					USB_CONFIG_ATT_SELFPOWER))
			continue;
#endif

		 

		 
		if (usb_get_max_power(udev, c) > udev->bus_mA) {
			insufficient_power++;
			continue;
		}

		 
		if (desc && is_audio(desc)) {
			 
			if (is_uac3_config(desc)) {
				best = c;
				break;
			}

			 
			else if (i == 0)
				best = c;

			 
			continue;
		}

		 
		if (i == 0 && num_configs > 1 && desc &&
				(is_rndis(desc) || is_activesync(desc))) {
#if !defined(CONFIG_USB_NET_RNDIS_HOST) && !defined(CONFIG_USB_NET_RNDIS_HOST_MODULE)
			continue;
#else
			best = c;
#endif
		}

		 
		else if (udev->descriptor.bDeviceClass !=
						USB_CLASS_VENDOR_SPEC &&
				(desc && desc->bInterfaceClass !=
						USB_CLASS_VENDOR_SPEC)) {
			best = c;
			break;
		}

		 
		else if (!best)
			best = c;
	}

	if (insufficient_power > 0)
		dev_info(&udev->dev, "rejected %d configuration%s "
			"due to insufficient available bus power\n",
			insufficient_power, plural(insufficient_power));

	if (best) {
		i = best->desc.bConfigurationValue;
		dev_dbg(&udev->dev,
			"configuration #%d chosen from %d choice%s\n",
			i, num_configs, plural(num_configs));
	} else {
		i = -1;
		dev_warn(&udev->dev,
			"no configuration chosen from %d choice%s\n",
			num_configs, plural(num_configs));
	}
	return i;
}
EXPORT_SYMBOL_GPL(usb_choose_configuration);

static int __check_for_non_generic_match(struct device_driver *drv, void *data)
{
	struct usb_device *udev = data;
	struct usb_device_driver *udrv;

	if (!is_usb_device_driver(drv))
		return 0;
	udrv = to_usb_device_driver(drv);
	if (udrv == &usb_generic_driver)
		return 0;
	return usb_driver_applicable(udev, udrv);
}

static bool usb_generic_driver_match(struct usb_device *udev)
{
	if (udev->use_generic_driver)
		return true;

	 
	if (bus_for_each_drv(&usb_bus_type, NULL, udev, __check_for_non_generic_match))
		return false;

	return true;
}

int usb_generic_driver_probe(struct usb_device *udev)
{
	int err, c;

	 
	if (udev->authorized == 0)
		dev_err(&udev->dev, "Device is not authorized for usage\n");
	else {
		c = usb_choose_configuration(udev);
		if (c >= 0) {
			err = usb_set_configuration(udev, c);
			if (err && err != -ENODEV) {
				dev_err(&udev->dev, "can't set config #%d, error %d\n",
					c, err);
				 
			}
		}
	}
	 
	usb_notify_add_device(udev);

	return 0;
}

void usb_generic_driver_disconnect(struct usb_device *udev)
{
	usb_notify_remove_device(udev);

	 
	if (udev->actconfig)
		usb_set_configuration(udev, -1);
}

#ifdef	CONFIG_PM

int usb_generic_driver_suspend(struct usb_device *udev, pm_message_t msg)
{
	int rc;

	 
	if (!udev->parent)
		rc = hcd_bus_suspend(udev, msg);

	 
	else if ((msg.event == PM_EVENT_FREEZE || msg.event == PM_EVENT_PRETHAW)
		 && (udev->speed < USB_SPEED_SUPER))
		rc = 0;
	else
		rc = usb_port_suspend(udev, msg);

	if (rc == 0)
		usbfs_notify_suspend(udev);
	return rc;
}

int usb_generic_driver_resume(struct usb_device *udev, pm_message_t msg)
{
	int rc;

	 
	if (!udev->parent)
		rc = hcd_bus_resume(udev, msg);
	else
		rc = usb_port_resume(udev, msg);

	if (rc == 0)
		usbfs_notify_resume(udev);
	return rc;
}

#endif	 

struct usb_device_driver usb_generic_driver = {
	.name =	"usb",
	.match = usb_generic_driver_match,
	.probe = usb_generic_driver_probe,
	.disconnect = usb_generic_driver_disconnect,
#ifdef	CONFIG_PM
	.suspend = usb_generic_driver_suspend,
	.resume = usb_generic_driver_resume,
#endif
	.supports_autosuspend = 1,
};
