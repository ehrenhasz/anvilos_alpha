
 

 

#include <linux/module.h>
#include <linux/comedi/comedidev.h>

#include "amplc_dio200.h"

 
static const struct dio200_board dio200_isa_boards[] = {
	{
		.name		= "pc212e",
		.n_subdevs	= 6,
		.sdtype		= {
			sd_8255, sd_8254, sd_8254, sd_8254, sd_8254, sd_intr
		},
		.sdinfo		= { 0x00, 0x08, 0x0c, 0x10, 0x14, 0x3f },
		.has_int_sce	= true,
		.has_clk_gat_sce = true,
	}, {
		.name		= "pc214e",
		.n_subdevs	= 4,
		.sdtype		= {
			sd_8255, sd_8255, sd_8254, sd_intr
		},
		.sdinfo		= { 0x00, 0x08, 0x10, 0x01 },
	}, {
		.name		= "pc215e",
		.n_subdevs	= 5,
		.sdtype		= {
			sd_8255, sd_8255, sd_8254, sd_8254, sd_intr
		},
		.sdinfo		= { 0x00, 0x08, 0x10, 0x14, 0x3f },
		.has_int_sce	= true,
		.has_clk_gat_sce = true,
	}, {
		.name		= "pc218e",
		.n_subdevs	= 7,
		.sdtype		= {
			sd_8254, sd_8254, sd_8255, sd_8254, sd_8254, sd_intr
		},
		.sdinfo		= { 0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, 0x3f },
		.has_int_sce	= true,
		.has_clk_gat_sce = true,
	}, {
		.name		= "pc272e",
		.n_subdevs	= 4,
		.sdtype		= {
			sd_8255, sd_8255, sd_8255, sd_intr
		},
		.sdinfo		= { 0x00, 0x08, 0x10, 0x3f },
		.has_int_sce = true,
	},
};

static int dio200_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	int ret;

	ret = comedi_request_region(dev, it->options[0], 0x20);
	if (ret)
		return ret;

	return amplc_dio200_common_attach(dev, it->options[1], 0);
}

static struct comedi_driver amplc_dio200_driver = {
	.driver_name	= "amplc_dio200",
	.module		= THIS_MODULE,
	.attach		= dio200_attach,
	.detach		= comedi_legacy_detach,
	.board_name	= &dio200_isa_boards[0].name,
	.offset		= sizeof(struct dio200_board),
	.num_names	= ARRAY_SIZE(dio200_isa_boards),
};
module_comedi_driver(amplc_dio200_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi driver for Amplicon 200 Series ISA DIO boards");
MODULE_LICENSE("GPL");
