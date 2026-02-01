
 

#include <linux/module.h>
#include <linux/comedi/comedi_usb.h>

 
struct usb_interface *comedi_to_usb_interface(struct comedi_device *dev)
{
	return dev->hw_dev ? to_usb_interface(dev->hw_dev) : NULL;
}
EXPORT_SYMBOL_GPL(comedi_to_usb_interface);

 
struct usb_device *comedi_to_usb_dev(struct comedi_device *dev)
{
	struct usb_interface *intf = comedi_to_usb_interface(dev);

	return intf ? interface_to_usbdev(intf) : NULL;
}
EXPORT_SYMBOL_GPL(comedi_to_usb_dev);

 
int comedi_usb_auto_config(struct usb_interface *intf,
			   struct comedi_driver *driver,
			   unsigned long context)
{
	return comedi_auto_config(&intf->dev, driver, context);
}
EXPORT_SYMBOL_GPL(comedi_usb_auto_config);

 
void comedi_usb_auto_unconfig(struct usb_interface *intf)
{
	comedi_auto_unconfig(&intf->dev);
}
EXPORT_SYMBOL_GPL(comedi_usb_auto_unconfig);

 
int comedi_usb_driver_register(struct comedi_driver *comedi_driver,
			       struct usb_driver *usb_driver)
{
	int ret;

	ret = comedi_driver_register(comedi_driver);
	if (ret < 0)
		return ret;

	ret = usb_register(usb_driver);
	if (ret < 0) {
		comedi_driver_unregister(comedi_driver);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(comedi_usb_driver_register);

 
void comedi_usb_driver_unregister(struct comedi_driver *comedi_driver,
				  struct usb_driver *usb_driver)
{
	usb_deregister(usb_driver);
	comedi_driver_unregister(comedi_driver);
}
EXPORT_SYMBOL_GPL(comedi_usb_driver_unregister);

static int __init comedi_usb_init(void)
{
	return 0;
}
module_init(comedi_usb_init);

static void __exit comedi_usb_exit(void)
{
}
module_exit(comedi_usb_exit);

MODULE_AUTHOR("https://www.comedi.org");
MODULE_DESCRIPTION("Comedi USB interface module");
MODULE_LICENSE("GPL");
