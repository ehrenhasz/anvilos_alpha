
 

 

#include <linux/module.h>
#include <linux/comedi/comedidev.h>

 
#define MULTIQ3_DI_REG			0x00
#define MULTIQ3_DO_REG			0x00
#define MULTIQ3_AO_REG			0x02
#define MULTIQ3_AI_REG			0x04
#define MULTIQ3_AI_CONV_REG		0x04
#define MULTIQ3_STATUS_REG		0x06
#define MULTIQ3_STATUS_EOC		BIT(3)
#define MULTIQ3_STATUS_EOC_I		BIT(4)
#define MULTIQ3_CTRL_REG		0x06
#define MULTIQ3_CTRL_AO_CHAN(x)		(((x) & 0x7) << 0)
#define MULTIQ3_CTRL_RC(x)		(((x) & 0x3) << 0)
#define MULTIQ3_CTRL_AI_CHAN(x)		(((x) & 0x7) << 3)
#define MULTIQ3_CTRL_E_CHAN(x)		(((x) & 0x7) << 3)
#define MULTIQ3_CTRL_EN			BIT(6)
#define MULTIQ3_CTRL_AZ			BIT(7)
#define MULTIQ3_CTRL_CAL		BIT(8)
#define MULTIQ3_CTRL_SH			BIT(9)
#define MULTIQ3_CTRL_CLK		BIT(10)
#define MULTIQ3_CTRL_LD			(3 << 11)
#define MULTIQ3_CLK_REG			0x08
#define MULTIQ3_ENC_DATA_REG		0x0c
#define MULTIQ3_ENC_CTRL_REG		0x0e

 
#define MULTIQ3_CLOCK_DATA		0x00	 
#define MULTIQ3_CLOCK_SETUP		0x18	 
#define MULTIQ3_INPUT_SETUP		0x41	 
#define MULTIQ3_QUAD_X4			0x38	 
#define MULTIQ3_BP_RESET		0x01	 
#define MULTIQ3_CNTR_RESET		0x02	 
#define MULTIQ3_TRSFRPR_CTR		0x08	 
#define MULTIQ3_TRSFRCNTR_OL		0x10	 
#define MULTIQ3_EFLAG_RESET		0x06	 

static void multiq3_set_ctrl(struct comedi_device *dev, unsigned int bits)
{
	 
	outw(MULTIQ3_CTRL_SH | MULTIQ3_CTRL_CLK | bits,
	     dev->iobase + MULTIQ3_CTRL_REG);
}

static int multiq3_ai_status(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn,
			     unsigned long context)
{
	unsigned int status;

	status = inw(dev->iobase + MULTIQ3_STATUS_REG);
	if (status & context)
		return 0;
	return -EBUSY;
}

static int multiq3_ai_insn_read(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn,
				unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int val;
	int ret;
	int i;

	multiq3_set_ctrl(dev, MULTIQ3_CTRL_EN | MULTIQ3_CTRL_AI_CHAN(chan));

	ret = comedi_timeout(dev, s, insn, multiq3_ai_status,
			     MULTIQ3_STATUS_EOC);
	if (ret)
		return ret;

	for (i = 0; i < insn->n; i++) {
		outw(0, dev->iobase + MULTIQ3_AI_CONV_REG);

		ret = comedi_timeout(dev, s, insn, multiq3_ai_status,
				     MULTIQ3_STATUS_EOC_I);
		if (ret)
			return ret;

		 
		val = inb(dev->iobase + MULTIQ3_AI_REG) << 8;
		val |= inb(dev->iobase + MULTIQ3_AI_REG);
		val &= s->maxdata;

		 
		data[i] = comedi_offset_munge(s, val);
	}

	return insn->n;
}

static int multiq3_ao_insn_write(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn,
				 unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int val = s->readback[chan];
	int i;

	for (i = 0; i < insn->n; i++) {
		val = data[i];
		multiq3_set_ctrl(dev, MULTIQ3_CTRL_LD |
				      MULTIQ3_CTRL_AO_CHAN(chan));
		outw(val, dev->iobase + MULTIQ3_AO_REG);
		multiq3_set_ctrl(dev, 0);
	}
	s->readback[chan] = val;

	return insn->n;
}

static int multiq3_di_insn_bits(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	data[1] = inw(dev->iobase + MULTIQ3_DI_REG);

	return insn->n;
}

static int multiq3_do_insn_bits(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn,
				unsigned int *data)
{
	if (comedi_dio_update_state(s, data))
		outw(s->state, dev->iobase + MULTIQ3_DO_REG);

	data[1] = s->state;

	return insn->n;
}

static int multiq3_encoder_insn_read(struct comedi_device *dev,
				     struct comedi_subdevice *s,
				     struct comedi_insn *insn,
				     unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int val;
	int i;

	for (i = 0; i < insn->n; i++) {
		 
		multiq3_set_ctrl(dev, MULTIQ3_CTRL_EN |
				      MULTIQ3_CTRL_E_CHAN(chan));

		 
		outb(MULTIQ3_BP_RESET, dev->iobase + MULTIQ3_ENC_CTRL_REG);

		 
		outb(MULTIQ3_TRSFRCNTR_OL, dev->iobase + MULTIQ3_ENC_CTRL_REG);

		 
		val = inb(dev->iobase + MULTIQ3_ENC_DATA_REG);
		val |= (inb(dev->iobase + MULTIQ3_ENC_DATA_REG) << 8);
		val |= (inb(dev->iobase + MULTIQ3_ENC_DATA_REG) << 16);

		 
		data[i] = (val + ((s->maxdata + 1) >> 1)) & s->maxdata;
	}

	return insn->n;
}

static void multiq3_encoder_reset(struct comedi_device *dev,
				  unsigned int chan)
{
	multiq3_set_ctrl(dev, MULTIQ3_CTRL_EN | MULTIQ3_CTRL_E_CHAN(chan));
	outb(MULTIQ3_EFLAG_RESET, dev->iobase + MULTIQ3_ENC_CTRL_REG);
	outb(MULTIQ3_BP_RESET, dev->iobase + MULTIQ3_ENC_CTRL_REG);
	outb(MULTIQ3_CLOCK_DATA, dev->iobase + MULTIQ3_ENC_DATA_REG);
	outb(MULTIQ3_CLOCK_SETUP, dev->iobase + MULTIQ3_ENC_CTRL_REG);
	outb(MULTIQ3_INPUT_SETUP, dev->iobase + MULTIQ3_ENC_CTRL_REG);
	outb(MULTIQ3_QUAD_X4, dev->iobase + MULTIQ3_ENC_CTRL_REG);
	outb(MULTIQ3_CNTR_RESET, dev->iobase + MULTIQ3_ENC_CTRL_REG);
}

static int multiq3_encoder_insn_config(struct comedi_device *dev,
				       struct comedi_subdevice *s,
				       struct comedi_insn *insn,
				       unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);

	switch (data[0]) {
	case INSN_CONFIG_RESET:
		multiq3_encoder_reset(dev, chan);
		break;
	default:
		return -EINVAL;
	}

	return insn->n;
}

static int multiq3_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	int ret;
	int i;

	ret = comedi_request_region(dev, it->options[0], 0x10);
	if (ret)
		return ret;

	ret = comedi_alloc_subdevices(dev, 5);
	if (ret)
		return ret;

	 
	s = &dev->subdevices[0];
	s->type		= COMEDI_SUBD_AI;
	s->subdev_flags	= SDF_READABLE | SDF_GROUND;
	s->n_chan	= 8;
	s->maxdata	= 0x1fff;
	s->range_table	= &range_bipolar5;
	s->insn_read	= multiq3_ai_insn_read;

	 
	s = &dev->subdevices[1];
	s->type		= COMEDI_SUBD_AO;
	s->subdev_flags	= SDF_WRITABLE;
	s->n_chan	= 8;
	s->maxdata	= 0x0fff;
	s->range_table	= &range_bipolar5;
	s->insn_write	= multiq3_ao_insn_write;

	ret = comedi_alloc_subdev_readback(s);
	if (ret)
		return ret;

	 
	s = &dev->subdevices[2];
	s->type		= COMEDI_SUBD_DI;
	s->subdev_flags	= SDF_READABLE;
	s->n_chan	= 16;
	s->maxdata	= 1;
	s->range_table	= &range_digital;
	s->insn_bits	= multiq3_di_insn_bits;

	 
	s = &dev->subdevices[3];
	s->type		= COMEDI_SUBD_DO;
	s->subdev_flags	= SDF_WRITABLE;
	s->n_chan	= 16;
	s->maxdata	= 1;
	s->range_table	= &range_digital;
	s->insn_bits	= multiq3_do_insn_bits;

	 
	s = &dev->subdevices[4];
	s->type		= COMEDI_SUBD_COUNTER;
	s->subdev_flags	= SDF_READABLE | SDF_LSAMPL;
	s->n_chan	= it->options[2] * 2;
	s->maxdata	= 0x00ffffff;
	s->range_table	= &range_unknown;
	s->insn_read	= multiq3_encoder_insn_read;
	s->insn_config	= multiq3_encoder_insn_config;

	for (i = 0; i < s->n_chan; i++)
		multiq3_encoder_reset(dev, i);

	return 0;
}

static struct comedi_driver multiq3_driver = {
	.driver_name	= "multiq3",
	.module		= THIS_MODULE,
	.attach		= multiq3_attach,
	.detach		= comedi_legacy_detach,
};
module_comedi_driver(multiq3_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi driver for Quanser Consulting MultiQ-3 board");
MODULE_LICENSE("GPL");
