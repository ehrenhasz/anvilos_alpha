
 

 

#include <linux/module.h>
#include <linux/comedi/comedidev.h>

#include "das08.h"

static const struct das08_board_struct das08_isa_boards[] = {
	{
		 
		.name		= "isa-das08",
		.ai_nbits	= 12,
		.ai_pg		= das08_pg_none,
		.ai_encoding	= das08_encode12,
		.di_nchan	= 3,
		.do_nchan	= 4,
		.i8255_offset	= 8,
		.i8254_offset	= 4,
		.iosize		= 16,		 
	}, {
		 
		.name		= "das08-pgm",
		.ai_nbits	= 12,
		.ai_pg		= das08_pgm,
		.ai_encoding	= das08_encode12,
		.di_nchan	= 3,
		.do_nchan	= 4,
		.i8255_offset	= 0,
		.i8254_offset	= 0x04,
		.iosize		= 16,		 
	}, {
		 
		.name		= "das08-pgh",
		.ai_nbits	= 12,
		.ai_pg		= das08_pgh,
		.ai_encoding	= das08_encode12,
		.di_nchan	= 3,
		.do_nchan	= 4,
		.i8254_offset	= 0x04,
		.iosize		= 16,		 
	}, {
		 
		.name		= "das08-pgl",
		.ai_nbits	= 12,
		.ai_pg		= das08_pgl,
		.ai_encoding	= das08_encode12,
		.di_nchan	= 3,
		.do_nchan	= 4,
		.i8254_offset	= 0x04,
		.iosize		= 16,		 
	}, {
		 
		.name		= "das08-aoh",
		.ai_nbits	= 12,
		.ai_pg		= das08_pgh,
		.ai_encoding	= das08_encode12,
		.ao_nbits	= 12,
		.di_nchan	= 3,
		.do_nchan	= 4,
		.i8255_offset	= 0x0c,
		.i8254_offset	= 0x04,
		.iosize		= 16,		 
	}, {
		 
		.name		= "das08-aol",
		.ai_nbits	= 12,
		.ai_pg		= das08_pgl,
		.ai_encoding	= das08_encode12,
		.ao_nbits	= 12,
		.di_nchan	= 3,
		.do_nchan	= 4,
		.i8255_offset	= 0x0c,
		.i8254_offset	= 0x04,
		.iosize		= 16,		 
	}, {
		 
		.name		= "das08-aom",
		.ai_nbits	= 12,
		.ai_pg		= das08_pgm,
		.ai_encoding	= das08_encode12,
		.ao_nbits	= 12,
		.di_nchan	= 3,
		.do_nchan	= 4,
		.i8255_offset	= 0x0c,
		.i8254_offset	= 0x04,
		.iosize		= 16,		 
	}, {
		 
		.name		= "das08/jr-ao",
		.is_jr		= true,
		.ai_nbits	= 12,
		.ai_pg		= das08_pg_none,
		.ai_encoding	= das08_encode12,
		.ao_nbits	= 12,
		.di_nchan	= 8,
		.do_nchan	= 8,
		.iosize		= 16,		 
	}, {
		 
		.name		= "das08jr-16-ao",
		.is_jr		= true,
		.ai_nbits	= 16,
		.ai_pg		= das08_pg_none,
		.ai_encoding	= das08_encode16,
		.ao_nbits	= 16,
		.di_nchan	= 8,
		.do_nchan	= 8,
		.i8254_offset	= 0x04,
		.iosize		= 16,		 
	}, {
		.name		= "pc104-das08",
		.ai_nbits	= 12,
		.ai_pg		= das08_pg_none,
		.ai_encoding	= das08_encode12,
		.di_nchan	= 3,
		.do_nchan	= 4,
		.i8254_offset	= 4,
		.iosize		= 16,		 
	}, {
		.name		= "das08jr/16",
		.is_jr		= true,
		.ai_nbits	= 16,
		.ai_pg		= das08_pg_none,
		.ai_encoding	= das08_encode16,
		.di_nchan	= 8,
		.do_nchan	= 8,
		.iosize		= 16,		 
	},
};

static int das08_isa_attach(struct comedi_device *dev,
			    struct comedi_devconfig *it)
{
	const struct das08_board_struct *board = dev->board_ptr;
	struct das08_private_struct *devpriv;
	int ret;

	devpriv = comedi_alloc_devpriv(dev, sizeof(*devpriv));
	if (!devpriv)
		return -ENOMEM;

	ret = comedi_request_region(dev, it->options[0], board->iosize);
	if (ret)
		return ret;

	return das08_common_attach(dev, dev->iobase);
}

static struct comedi_driver das08_isa_driver = {
	.driver_name	= "isa-das08",
	.module		= THIS_MODULE,
	.attach		= das08_isa_attach,
	.detach		= comedi_legacy_detach,
	.board_name	= &das08_isa_boards[0].name,
	.num_names	= ARRAY_SIZE(das08_isa_boards),
	.offset		= sizeof(das08_isa_boards[0]),
};
module_comedi_driver(das08_isa_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
