
 

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <sound/hdaudio_ext.h>

MODULE_DESCRIPTION("HDA extended core");
MODULE_LICENSE("GPL v2");

 
int snd_hdac_ext_bus_init(struct hdac_bus *bus, struct device *dev,
			const struct hdac_bus_ops *ops,
			const struct hdac_ext_bus_ops *ext_ops)
{
	int ret;

	ret = snd_hdac_bus_init(bus, dev, ops);
	if (ret < 0)
		return ret;

	bus->ext_ops = ext_ops;
	 
	bus->idx = 0;
	bus->cmd_dma_state = true;

	return 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_ext_bus_init);

 
void snd_hdac_ext_bus_exit(struct hdac_bus *bus)
{
	snd_hdac_bus_exit(bus);
	WARN_ON(!list_empty(&bus->hlink_list));
}
EXPORT_SYMBOL_GPL(snd_hdac_ext_bus_exit);

 
void snd_hdac_ext_bus_device_remove(struct hdac_bus *bus)
{
	struct hdac_device *codec, *__codec;
	 
	list_for_each_entry_safe(codec, __codec, &bus->codec_list, list) {
		snd_hdac_device_unregister(codec);
		put_device(&codec->dev);
	}
}
EXPORT_SYMBOL_GPL(snd_hdac_ext_bus_device_remove);
#define dev_to_hdac(dev) (container_of((dev), \
			struct hdac_device, dev))

static inline struct hdac_driver *get_hdrv(struct device *dev)
{
	struct hdac_driver *hdrv = drv_to_hdac_driver(dev->driver);
	return hdrv;
}

static inline struct hdac_device *get_hdev(struct device *dev)
{
	struct hdac_device *hdev = dev_to_hdac_dev(dev);
	return hdev;
}

static int hda_ext_drv_probe(struct device *dev)
{
	return (get_hdrv(dev))->probe(get_hdev(dev));
}

static int hdac_ext_drv_remove(struct device *dev)
{
	return (get_hdrv(dev))->remove(get_hdev(dev));
}

static void hdac_ext_drv_shutdown(struct device *dev)
{
	return (get_hdrv(dev))->shutdown(get_hdev(dev));
}

 
int snd_hda_ext_driver_register(struct hdac_driver *drv)
{
	drv->type = HDA_DEV_ASOC;
	drv->driver.bus = &snd_hda_bus_type;
	 

	if (drv->probe)
		drv->driver.probe = hda_ext_drv_probe;
	if (drv->remove)
		drv->driver.remove = hdac_ext_drv_remove;
	if (drv->shutdown)
		drv->driver.shutdown = hdac_ext_drv_shutdown;

	return driver_register(&drv->driver);
}
EXPORT_SYMBOL_GPL(snd_hda_ext_driver_register);

 
void snd_hda_ext_driver_unregister(struct hdac_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL_GPL(snd_hda_ext_driver_unregister);
