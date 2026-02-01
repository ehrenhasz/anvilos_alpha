
 

 

#include <linux/module.h>
#include <linux/comedi/comedi_pcmcia.h>

#include "ni_labpc.h"

static const struct labpc_boardinfo labpc_cs_boards[] = {
	{
		.name			= "daqcard-1200",
		.ai_speed		= 10000,
		.has_ao			= 1,
		.is_labpc1200		= 1,
	},
};

static int labpc_cs_auto_attach(struct comedi_device *dev,
				unsigned long context)
{
	struct pcmcia_device *link = comedi_to_pcmcia_dev(dev);
	int ret;

	 
	dev->board_ptr = &labpc_cs_boards[0];

	link->config_flags |= CONF_AUTO_SET_IO |
			      CONF_ENABLE_IRQ | CONF_ENABLE_PULSE_IRQ;
	ret = comedi_pcmcia_enable(dev, NULL);
	if (ret)
		return ret;
	dev->iobase = link->resource[0]->start;

	if (!link->irq)
		return -EINVAL;

	return labpc_common_attach(dev, link->irq, IRQF_SHARED);
}

static void labpc_cs_detach(struct comedi_device *dev)
{
	labpc_common_detach(dev);
	comedi_pcmcia_disable(dev);
}

static struct comedi_driver driver_labpc_cs = {
	.driver_name	= "ni_labpc_cs",
	.module		= THIS_MODULE,
	.auto_attach	= labpc_cs_auto_attach,
	.detach		= labpc_cs_detach,
};

static int labpc_cs_attach(struct pcmcia_device *link)
{
	return comedi_pcmcia_auto_config(link, &driver_labpc_cs);
}

static const struct pcmcia_device_id labpc_cs_ids[] = {
	PCMCIA_DEVICE_MANF_CARD(0x010b, 0x0103),	 
	PCMCIA_DEVICE_NULL
};
MODULE_DEVICE_TABLE(pcmcia, labpc_cs_ids);

static struct pcmcia_driver labpc_cs_driver = {
	.name		= "daqcard-1200",
	.owner		= THIS_MODULE,
	.id_table	= labpc_cs_ids,
	.probe		= labpc_cs_attach,
	.remove		= comedi_pcmcia_auto_unconfig,
};
module_comedi_pcmcia_driver(driver_labpc_cs, labpc_cs_driver);

MODULE_DESCRIPTION("Comedi driver for National Instruments Lab-PC");
MODULE_AUTHOR("Frank Mori Hess <fmhess@users.sourceforge.net>");
MODULE_LICENSE("GPL");
