
 

 

#include <linux/module.h>
#include <linux/comedi/comedidev.h>
#include <linux/comedi/comedi_8255.h>

struct pcl724_board {
	const char *name;
	unsigned int io_range;
	unsigned int can_have96:1;
	unsigned int is_pet48:1;
	int numofports;
};

static const struct pcl724_board boardtypes[] = {
	{
		.name		= "pcl724",
		.io_range	= 0x04,
		.numofports	= 1,	 
	}, {
		.name		= "pcl722",
		.io_range	= 0x20,
		.can_have96	= 1,
		.numofports	= 6,	 
	}, {
		.name		= "pcl731",
		.io_range	= 0x08,
		.numofports	= 2,	 
	}, {
		.name		= "acl7122",
		.io_range	= 0x20,
		.can_have96	= 1,
		.numofports	= 6,	 
	}, {
		.name		= "acl7124",
		.io_range	= 0x04,
		.numofports	= 1,	 
	}, {
		.name		= "pet48dio",
		.io_range	= 0x02,
		.is_pet48	= 1,
		.numofports	= 2,	 
	}, {
		.name		= "pcmio48",
		.io_range	= 0x08,
		.numofports	= 2,	 
	}, {
		.name		= "onyx-mm-dio",
		.io_range	= 0x10,
		.numofports	= 2,	 
	},
};

static int pcl724_8255mapped_io(struct comedi_device *dev,
				int dir, int port, int data,
				unsigned long iobase)
{
	int movport = I8255_SIZE * (iobase >> 12);

	iobase &= 0x0fff;

	outb(port + movport, iobase);
	if (dir) {
		outb(data, iobase + 1);
		return 0;
	}
	return inb(iobase + 1);
}

static int pcl724_attach(struct comedi_device *dev,
			 struct comedi_devconfig *it)
{
	const struct pcl724_board *board = dev->board_ptr;
	struct comedi_subdevice *s;
	unsigned long iobase;
	unsigned int iorange;
	int n_subdevices;
	int ret;
	int i;

	iorange = board->io_range;
	n_subdevices = board->numofports;

	 
	if (board->can_have96 &&
	    (it->options[2] == 1 || it->options[2] == 96)) {
		iorange = 0x10;
		n_subdevices = 4;
	}

	ret = comedi_request_region(dev, it->options[0], iorange);
	if (ret)
		return ret;

	ret = comedi_alloc_subdevices(dev, n_subdevices);
	if (ret)
		return ret;

	for (i = 0; i < dev->n_subdevices; i++) {
		s = &dev->subdevices[i];
		if (board->is_pet48) {
			iobase = dev->iobase + (i * 0x1000);
			ret = subdev_8255_init(dev, s, pcl724_8255mapped_io,
					       iobase);
		} else {
			ret = subdev_8255_init(dev, s, NULL, i * I8255_SIZE);
		}
		if (ret)
			return ret;
	}

	return 0;
}

static struct comedi_driver pcl724_driver = {
	.driver_name	= "pcl724",
	.module		= THIS_MODULE,
	.attach		= pcl724_attach,
	.detach		= comedi_legacy_detach,
	.board_name	= &boardtypes[0].name,
	.num_names	= ARRAY_SIZE(boardtypes),
	.offset		= sizeof(struct pcl724_board),
};
module_comedi_driver(pcl724_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi driver for 8255 based ISA and PC/104 DIO boards");
MODULE_LICENSE("GPL");
