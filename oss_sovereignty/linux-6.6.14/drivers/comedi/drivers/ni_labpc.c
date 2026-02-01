
 

 

#include <linux/module.h>
#include <linux/comedi/comedidev.h>

#include "ni_labpc.h"
#include "ni_labpc_isadma.h"

static const struct labpc_boardinfo labpc_boards[] = {
	{
		.name			= "lab-pc-1200",
		.ai_speed		= 10000,
		.ai_scan_up		= 1,
		.has_ao			= 1,
		.is_labpc1200		= 1,
	}, {
		.name			= "lab-pc-1200ai",
		.ai_speed		= 10000,
		.ai_scan_up		= 1,
		.is_labpc1200		= 1,
	}, {
		.name			= "lab-pc+",
		.ai_speed		= 12000,
		.has_ao			= 1,
	},
};

static int labpc_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	unsigned int irq = it->options[1];
	unsigned int dma_chan = it->options[2];
	int ret;

	ret = comedi_request_region(dev, it->options[0], 0x20);
	if (ret)
		return ret;

	ret = labpc_common_attach(dev, irq, 0);
	if (ret)
		return ret;

	if (dev->irq)
		labpc_init_dma_chan(dev, dma_chan);

	return 0;
}

static void labpc_detach(struct comedi_device *dev)
{
	labpc_free_dma_chan(dev);
	labpc_common_detach(dev);
	comedi_legacy_detach(dev);
}

static struct comedi_driver labpc_driver = {
	.driver_name	= "ni_labpc",
	.module		= THIS_MODULE,
	.attach		= labpc_attach,
	.detach		= labpc_detach,
	.num_names	= ARRAY_SIZE(labpc_boards),
	.board_name	= &labpc_boards[0].name,
	.offset		= sizeof(struct labpc_boardinfo),
};
module_comedi_driver(labpc_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi driver for NI Lab-PC ISA boards");
MODULE_LICENSE("GPL");
