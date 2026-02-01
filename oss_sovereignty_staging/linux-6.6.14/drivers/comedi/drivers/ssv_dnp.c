
 

 

 

#include <linux/module.h>
#include <linux/comedi/comedidev.h>

 
 
 
 
 
 
 

#define CSCIR 0x22		 
#define CSCDR 0x23		 
#define PAMR  0xa5		 
#define PADR  0xa9		 
#define PBMR  0xa4		 
#define PBDR  0xa8		 
#define PCMR  0xa3		 
#define PCDR  0xa7		 

static int dnp_dio_insn_bits(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn,
			     unsigned int *data)
{
	unsigned int mask;
	unsigned int val;

	 

	mask = comedi_dio_update_state(s, data);
	if (mask) {
		outb(PADR, CSCIR);
		outb(s->state & 0xff, CSCDR);

		outb(PBDR, CSCIR);
		outb((s->state >> 8) & 0xff, CSCDR);

		outb(PCDR, CSCIR);
		val = inb(CSCDR) & 0x0f;
		outb(((s->state >> 12) & 0xf0) | val, CSCDR);
	}

	outb(PADR, CSCIR);
	val = inb(CSCDR);
	outb(PBDR, CSCIR);
	val |= (inb(CSCDR) << 8);
	outb(PCDR, CSCIR);
	val |= ((inb(CSCDR) & 0xf0) << 12);

	data[1] = val;

	return insn->n;
}

static int dnp_dio_insn_config(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn,
			       unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int mask;
	unsigned int val;
	int ret;

	ret = comedi_dio_insn_config(dev, s, insn, data, 0);
	if (ret)
		return ret;

	if (chan < 8) {			 
		mask = 1 << chan;
		outb(PAMR, CSCIR);
	} else if (chan < 16) {		 
		mask = 1 << (chan - 8);
		outb(PBMR, CSCIR);
	} else {			 
		 
		mask = 1 << ((chan - 16) * 2);
		outb(PCMR, CSCIR);
	}

	val = inb(CSCDR);
	if (data[0] == COMEDI_OUTPUT)
		val |= mask;
	else
		val &= ~mask;
	outb(val, CSCDR);

	return insn->n;
}

static int dnp_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	int ret;

	 

	ret = comedi_alloc_subdevices(dev, 1);
	if (ret)
		return ret;

	s = &dev->subdevices[0];
	 
	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
	s->n_chan = 20;
	s->maxdata = 1;
	s->range_table = &range_digital;
	s->insn_bits = dnp_dio_insn_bits;
	s->insn_config = dnp_dio_insn_config;

	 
	outb(PAMR, CSCIR);
	outb(0x00, CSCDR);
	outb(PBMR, CSCIR);
	outb(0x00, CSCDR);
	outb(PCMR, CSCIR);
	outb((inb(CSCDR) & 0xAA), CSCDR);

	return 0;
}

static void dnp_detach(struct comedi_device *dev)
{
	outb(PAMR, CSCIR);
	outb(0x00, CSCDR);
	outb(PBMR, CSCIR);
	outb(0x00, CSCDR);
	outb(PCMR, CSCIR);
	outb((inb(CSCDR) & 0xAA), CSCDR);
}

static struct comedi_driver dnp_driver = {
	.driver_name	= "dnp-1486",
	.module		= THIS_MODULE,
	.attach		= dnp_attach,
	.detach		= dnp_detach,
};
module_comedi_driver(dnp_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
