
 

 

#include <linux/module.h>
#include <linux/comedi/comedi_pcmcia.h>

#include "das08.h"

static const struct das08_board_struct das08_cs_boards[] = {
	{
		.name		= "pcm-das08",
		.ai_nbits	= 12,
		.ai_pg		= das08_bipolar5,
		.ai_encoding	= das08_pcm_encode12,
		.di_nchan	= 3,
		.do_nchan	= 3,
		.iosize		= 16,
	},
};

static int das08_cs_auto_attach(struct comedi_device *dev,
				unsigned long context)
{
	struct pcmcia_device *link = comedi_to_pcmcia_dev(dev);
	struct das08_private_struct *devpriv;
	unsigned long iobase;
	int ret;

	 
	dev->board_ptr = &das08_cs_boards[0];

	link->config_flags |= CONF_AUTO_SET_IO;
	ret = comedi_pcmcia_enable(dev, NULL);
	if (ret)
		return ret;
	iobase = link->resource[0]->start;

	devpriv = comedi_alloc_devpriv(dev, sizeof(*devpriv));
	if (!devpriv)
		return -ENOMEM;

	return das08_common_attach(dev, iobase);
}

static struct comedi_driver driver_das08_cs = {
	.driver_name	= "das08_cs",
	.module		= THIS_MODULE,
	.auto_attach	= das08_cs_auto_attach,
	.detach		= comedi_pcmcia_disable,
};

static int das08_pcmcia_attach(struct pcmcia_device *link)
{
	return comedi_pcmcia_auto_config(link, &driver_das08_cs);
}

static const struct pcmcia_device_id das08_cs_id_table[] = {
	PCMCIA_DEVICE_MANF_CARD(0x01c5, 0x4001),
	PCMCIA_DEVICE_NULL
};
MODULE_DEVICE_TABLE(pcmcia, das08_cs_id_table);

static struct pcmcia_driver das08_cs_driver = {
	.name		= "pcm-das08",
	.owner		= THIS_MODULE,
	.id_table	= das08_cs_id_table,
	.probe		= das08_pcmcia_attach,
	.remove		= comedi_pcmcia_auto_unconfig,
};
module_comedi_pcmcia_driver(driver_das08_cs, das08_cs_driver);

MODULE_AUTHOR("David A. Schleef <ds@schleef.org>");
MODULE_AUTHOR("Frank Mori Hess <fmhess@users.sourceforge.net>");
MODULE_DESCRIPTION("Comedi driver for ComputerBoards DAS-08 PCMCIA boards");
MODULE_LICENSE("GPL");
