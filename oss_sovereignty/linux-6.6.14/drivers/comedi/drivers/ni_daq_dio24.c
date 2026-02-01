
 

 

#include <linux/module.h>
#include <linux/comedi/comedi_pcmcia.h>
#include <linux/comedi/comedi_8255.h>

static int dio24_auto_attach(struct comedi_device *dev,
			     unsigned long context)
{
	struct pcmcia_device *link = comedi_to_pcmcia_dev(dev);
	struct comedi_subdevice *s;
	int ret;

	link->config_flags |= CONF_AUTO_SET_IO;
	ret = comedi_pcmcia_enable(dev, NULL);
	if (ret)
		return ret;
	dev->iobase = link->resource[0]->start;

	ret = comedi_alloc_subdevices(dev, 1);
	if (ret)
		return ret;

	 
	s = &dev->subdevices[0];
	return subdev_8255_init(dev, s, NULL, 0x00);
}

static struct comedi_driver driver_dio24 = {
	.driver_name	= "ni_daq_dio24",
	.module		= THIS_MODULE,
	.auto_attach	= dio24_auto_attach,
	.detach		= comedi_pcmcia_disable,
};

static int dio24_cs_attach(struct pcmcia_device *link)
{
	return comedi_pcmcia_auto_config(link, &driver_dio24);
}

static const struct pcmcia_device_id dio24_cs_ids[] = {
	PCMCIA_DEVICE_MANF_CARD(0x010b, 0x475c),	 
	PCMCIA_DEVICE_NULL
};
MODULE_DEVICE_TABLE(pcmcia, dio24_cs_ids);

static struct pcmcia_driver dio24_cs_driver = {
	.name		= "ni_daq_dio24",
	.owner		= THIS_MODULE,
	.id_table	= dio24_cs_ids,
	.probe		= dio24_cs_attach,
	.remove		= comedi_pcmcia_auto_unconfig,
};
module_comedi_pcmcia_driver(driver_dio24, dio24_cs_driver);

MODULE_AUTHOR("Daniel Vecino Castel <dvecino@able.es>");
MODULE_DESCRIPTION(
	"Comedi driver for National Instruments PCMCIA DAQ-Card DIO-24");
MODULE_LICENSE("GPL");
