
 

 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/timer.h>
#include <linux/io.h>
#include <linux/pnp.h>
#include <linux/comedi/comedidev.h>

 
#define C6XDIGIO_DATA_REG	0x00
#define C6XDIGIO_DATA_CHAN(x)	(((x) + 1) << 4)
#define C6XDIGIO_DATA_PWM	BIT(5)
#define C6XDIGIO_DATA_ENCODER	BIT(6)
#define C6XDIGIO_STATUS_REG	0x01
#define C6XDIGIO_CTRL_REG	0x02

#define C6XDIGIO_TIME_OUT 20

static int c6xdigio_chk_status(struct comedi_device *dev, unsigned long context)
{
	unsigned int status;
	int timeout = 0;

	do {
		status = inb(dev->iobase + C6XDIGIO_STATUS_REG);
		if ((status & 0x80) != context)
			return 0;
		timeout++;
	} while  (timeout < C6XDIGIO_TIME_OUT);

	return -EBUSY;
}

static int c6xdigio_write_data(struct comedi_device *dev,
			       unsigned int val, unsigned int status)
{
	outb_p(val, dev->iobase + C6XDIGIO_DATA_REG);
	return c6xdigio_chk_status(dev, status);
}

static int c6xdigio_get_encoder_bits(struct comedi_device *dev,
				     unsigned int *bits,
				     unsigned int cmd,
				     unsigned int status)
{
	unsigned int val;

	val = inb(dev->iobase + C6XDIGIO_STATUS_REG);
	val >>= 3;
	val &= 0x07;

	*bits = val;

	return c6xdigio_write_data(dev, cmd, status);
}

static void c6xdigio_pwm_write(struct comedi_device *dev,
			       unsigned int chan, unsigned int val)
{
	unsigned int cmd = C6XDIGIO_DATA_PWM | C6XDIGIO_DATA_CHAN(chan);
	unsigned int bits;

	if (val > 498)
		val = 498;
	if (val < 2)
		val = 2;

	bits = (val >> 0) & 0x03;
	c6xdigio_write_data(dev, cmd | bits | (0 << 2), 0x00);
	bits = (val >> 2) & 0x03;
	c6xdigio_write_data(dev, cmd | bits | (1 << 2), 0x80);
	bits = (val >> 4) & 0x03;
	c6xdigio_write_data(dev, cmd | bits | (0 << 2), 0x00);
	bits = (val >> 6) & 0x03;
	c6xdigio_write_data(dev, cmd | bits | (1 << 2), 0x80);
	bits = (val >> 8) & 0x03;
	c6xdigio_write_data(dev, cmd | bits | (0 << 2), 0x00);

	c6xdigio_write_data(dev, 0x00, 0x80);
}

static int c6xdigio_encoder_read(struct comedi_device *dev,
				 unsigned int chan)
{
	unsigned int cmd = C6XDIGIO_DATA_ENCODER | C6XDIGIO_DATA_CHAN(chan);
	unsigned int val = 0;
	unsigned int bits;

	c6xdigio_write_data(dev, cmd, 0x00);

	c6xdigio_get_encoder_bits(dev, &bits, cmd | (1 << 2), 0x80);
	val |= (bits << 0);

	c6xdigio_get_encoder_bits(dev, &bits, cmd | (0 << 2), 0x00);
	val |= (bits << 3);

	c6xdigio_get_encoder_bits(dev, &bits, cmd | (1 << 2), 0x80);
	val |= (bits << 6);

	c6xdigio_get_encoder_bits(dev, &bits, cmd | (0 << 2), 0x00);
	val |= (bits << 9);

	c6xdigio_get_encoder_bits(dev, &bits, cmd | (1 << 2), 0x80);
	val |= (bits << 12);

	c6xdigio_get_encoder_bits(dev, &bits, cmd | (0 << 2), 0x00);
	val |= (bits << 15);

	c6xdigio_get_encoder_bits(dev, &bits, cmd | (1 << 2), 0x80);
	val |= (bits << 18);

	c6xdigio_get_encoder_bits(dev, &bits, cmd | (0 << 2), 0x00);
	val |= (bits << 21);

	c6xdigio_write_data(dev, 0x00, 0x80);

	return val;
}

static int c6xdigio_pwm_insn_write(struct comedi_device *dev,
				   struct comedi_subdevice *s,
				   struct comedi_insn *insn,
				   unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int val = (s->state >> (16 * chan)) & 0xffff;
	int i;

	for (i = 0; i < insn->n; i++) {
		val = data[i];
		c6xdigio_pwm_write(dev, chan, val);
	}

	 
	s->state &= (0xffff << (16 * chan));
	s->state |= (val << (16 * chan));

	return insn->n;
}

static int c6xdigio_pwm_insn_read(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn,
				  unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int val;
	int i;

	val = (s->state >> (16 * chan)) & 0xffff;

	for (i = 0; i < insn->n; i++)
		data[i] = val;

	return insn->n;
}

static int c6xdigio_encoder_insn_read(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int val;
	int i;

	for (i = 0; i < insn->n; i++) {
		val = c6xdigio_encoder_read(dev, chan);

		 
		data[i] = comedi_offset_munge(s, val);
	}

	return insn->n;
}

static void c6xdigio_init(struct comedi_device *dev)
{
	 
	c6xdigio_write_data(dev, 0x70, 0x00);
	c6xdigio_write_data(dev, 0x74, 0x80);
	c6xdigio_write_data(dev, 0x70, 0x00);
	c6xdigio_write_data(dev, 0x00, 0x80);

	 
	c6xdigio_write_data(dev, 0x68, 0x00);
	c6xdigio_write_data(dev, 0x6c, 0x80);
	c6xdigio_write_data(dev, 0x68, 0x00);
	c6xdigio_write_data(dev, 0x00, 0x80);
}

static const struct pnp_device_id c6xdigio_pnp_tbl[] = {
	 
	{.id = "PNP0400", .driver_data = 0},
	 
	{.id = "PNP0401", .driver_data = 0},
	{}
};

static struct pnp_driver c6xdigio_pnp_driver = {
	.name = "c6xdigio",
	.id_table = c6xdigio_pnp_tbl,
};

static int c6xdigio_attach(struct comedi_device *dev,
			   struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	int ret;

	ret = comedi_request_region(dev, it->options[0], 0x03);
	if (ret)
		return ret;

	ret = comedi_alloc_subdevices(dev, 2);
	if (ret)
		return ret;

	 
	pnp_register_driver(&c6xdigio_pnp_driver);

	s = &dev->subdevices[0];
	 
	s->type		= COMEDI_SUBD_PWM;
	s->subdev_flags	= SDF_WRITABLE;
	s->n_chan	= 2;
	s->maxdata	= 500;
	s->range_table	= &range_unknown;
	s->insn_write	= c6xdigio_pwm_insn_write;
	s->insn_read	= c6xdigio_pwm_insn_read;

	s = &dev->subdevices[1];
	 
	s->type		= COMEDI_SUBD_COUNTER;
	s->subdev_flags	= SDF_READABLE | SDF_LSAMPL;
	s->n_chan	= 2;
	s->maxdata	= 0xffffff;
	s->range_table	= &range_unknown;
	s->insn_read	= c6xdigio_encoder_insn_read;

	 
	 
	c6xdigio_init(dev);

	return 0;
}

static void c6xdigio_detach(struct comedi_device *dev)
{
	comedi_legacy_detach(dev);
	pnp_unregister_driver(&c6xdigio_pnp_driver);
}

static struct comedi_driver c6xdigio_driver = {
	.driver_name	= "c6xdigio",
	.module		= THIS_MODULE,
	.attach		= c6xdigio_attach,
	.detach		= c6xdigio_detach,
};
module_comedi_driver(c6xdigio_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi driver for the C6x_DIGIO DSP daughter card");
MODULE_LICENSE("GPL");
