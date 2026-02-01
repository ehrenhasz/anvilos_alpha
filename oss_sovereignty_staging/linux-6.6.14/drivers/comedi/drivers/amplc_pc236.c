
 
 

#include <linux/module.h>
#include <linux/comedi/comedidev.h>

#include "amplc_pc236.h"

static int pc236_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct pc236_private *devpriv;
	int ret;

	devpriv = comedi_alloc_devpriv(dev, sizeof(*devpriv));
	if (!devpriv)
		return -ENOMEM;

	ret = comedi_request_region(dev, it->options[0], 0x4);
	if (ret)
		return ret;

	return amplc_pc236_common_attach(dev, dev->iobase, it->options[1], 0);
}

static const struct pc236_board pc236_boards[] = {
	{
		.name = "pc36at",
	},
};

static struct comedi_driver amplc_pc236_driver = {
	.driver_name = "amplc_pc236",
	.module = THIS_MODULE,
	.attach = pc236_attach,
	.detach = comedi_legacy_detach,
	.board_name = &pc236_boards[0].name,
	.offset = sizeof(struct pc236_board),
	.num_names = ARRAY_SIZE(pc236_boards),
};

module_comedi_driver(amplc_pc236_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi driver for Amplicon PC36AT DIO boards");
MODULE_LICENSE("GPL");
