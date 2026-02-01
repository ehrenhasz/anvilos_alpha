
 

 

#include <linux/module.h>
#include <linux/comedi/comedidev.h>
#include <linux/comedi/comedi_8254.h>

 
#define ATAO_DIO_REG		0x00
#define ATAO_CFG2_REG		0x02
#define ATAO_CFG2_CALLD_NOP	(0 << 14)
#define ATAO_CFG2_CALLD(x)	((((x) >> 3) + 1) << 14)
#define ATAO_CFG2_FFRTEN	BIT(13)
#define ATAO_CFG2_DACS(x)	(1 << (((x) / 2) + 8))
#define ATAO_CFG2_LDAC(x)	(1 << (((x) / 2) + 3))
#define ATAO_CFG2_PROMEN	BIT(2)
#define ATAO_CFG2_SCLK		BIT(1)
#define ATAO_CFG2_SDATA		BIT(0)
#define ATAO_CFG3_REG		0x04
#define ATAO_CFG3_DMAMODE	BIT(6)
#define ATAO_CFG3_CLKOUT	BIT(5)
#define ATAO_CFG3_RCLKEN	BIT(4)
#define ATAO_CFG3_DOUTEN2	BIT(3)
#define ATAO_CFG3_DOUTEN1	BIT(2)
#define ATAO_CFG3_EN2_5V	BIT(1)
#define ATAO_CFG3_SCANEN	BIT(0)
#define ATAO_82C53_BASE		0x06
#define ATAO_CFG1_REG		0x0a
#define ATAO_CFG1_EXTINT2EN	BIT(15)
#define ATAO_CFG1_EXTINT1EN	BIT(14)
#define ATAO_CFG1_CNTINT2EN	BIT(13)
#define ATAO_CFG1_CNTINT1EN	BIT(12)
#define ATAO_CFG1_TCINTEN	BIT(11)
#define ATAO_CFG1_CNT1SRC	BIT(10)
#define ATAO_CFG1_CNT2SRC	BIT(9)
#define ATAO_CFG1_FIFOEN	BIT(8)
#define ATAO_CFG1_GRP2WR	BIT(7)
#define ATAO_CFG1_EXTUPDEN	BIT(6)
#define ATAO_CFG1_DMARQ		BIT(5)
#define ATAO_CFG1_DMAEN		BIT(4)
#define ATAO_CFG1_CH(x)		(((x) & 0xf) << 0)
#define ATAO_STATUS_REG		0x0a
#define ATAO_STATUS_FH		BIT(6)
#define ATAO_STATUS_FE		BIT(5)
#define ATAO_STATUS_FF		BIT(4)
#define ATAO_STATUS_INT2	BIT(3)
#define ATAO_STATUS_INT1	BIT(2)
#define ATAO_STATUS_TCINT	BIT(1)
#define ATAO_STATUS_PROMOUT	BIT(0)
#define ATAO_FIFO_WRITE_REG	0x0c
#define ATAO_FIFO_CLEAR_REG	0x0c
#define ATAO_AO_REG(x)		(0x0c + ((x) * 2))

 
#define ATAO_2_DMATCCLR_REG	0x00
#define ATAO_2_INT1CLR_REG	0x02
#define ATAO_2_INT2CLR_REG	0x04
#define ATAO_2_RTSISHFT_REG	0x06
#define ATAO_2_RTSISHFT_RSI	BIT(0)
#define ATAO_2_RTSISTRB_REG	0x07

struct atao_board {
	const char *name;
	int n_ao_chans;
};

static const struct atao_board atao_boards[] = {
	{
		.name		= "at-ao-6",
		.n_ao_chans	= 6,
	}, {
		.name		= "at-ao-10",
		.n_ao_chans	= 10,
	},
};

struct atao_private {
	unsigned short cfg1;
	unsigned short cfg3;

	 
	unsigned char caldac[21];
};

static void atao_select_reg_group(struct comedi_device *dev, int group)
{
	struct atao_private *devpriv = dev->private;

	if (group)
		devpriv->cfg1 |= ATAO_CFG1_GRP2WR;
	else
		devpriv->cfg1 &= ~ATAO_CFG1_GRP2WR;
	outw(devpriv->cfg1, dev->iobase + ATAO_CFG1_REG);
}

static int atao_ao_insn_write(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn,
			      unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int val = s->readback[chan];
	int i;

	if (chan == 0)
		atao_select_reg_group(dev, 1);

	for (i = 0; i < insn->n; i++) {
		val = data[i];

		 
		outw(comedi_offset_munge(s, val),
		     dev->iobase + ATAO_AO_REG(chan));
	}
	s->readback[chan] = val;

	if (chan == 0)
		atao_select_reg_group(dev, 0);

	return insn->n;
}

static int atao_dio_insn_bits(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn,
			      unsigned int *data)
{
	if (comedi_dio_update_state(s, data))
		outw(s->state, dev->iobase + ATAO_DIO_REG);

	data[1] = inw(dev->iobase + ATAO_DIO_REG);

	return insn->n;
}

static int atao_dio_insn_config(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn,
				unsigned int *data)
{
	struct atao_private *devpriv = dev->private;
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int mask;
	int ret;

	if (chan < 4)
		mask = 0x0f;
	else
		mask = 0xf0;

	ret = comedi_dio_insn_config(dev, s, insn, data, mask);
	if (ret)
		return ret;

	if (s->io_bits & 0x0f)
		devpriv->cfg3 |= ATAO_CFG3_DOUTEN1;
	else
		devpriv->cfg3 &= ~ATAO_CFG3_DOUTEN1;
	if (s->io_bits & 0xf0)
		devpriv->cfg3 |= ATAO_CFG3_DOUTEN2;
	else
		devpriv->cfg3 &= ~ATAO_CFG3_DOUTEN2;

	outw(devpriv->cfg3, dev->iobase + ATAO_CFG3_REG);

	return insn->n;
}

 
static int atao_calib_insn_write(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn,
				 unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);

	if (insn->n) {
		unsigned int val = data[insn->n - 1];
		unsigned int bitstring = ((chan & 0x7) << 8) | val;
		unsigned int bits;
		int bit;

		 
		 
		for (bit = BIT(10); bit; bit >>= 1) {
			bits = (bit & bitstring) ? ATAO_CFG2_SDATA : 0;

			outw(bits, dev->iobase + ATAO_CFG2_REG);
			outw(bits | ATAO_CFG2_SCLK,
			     dev->iobase + ATAO_CFG2_REG);
		}

		 
		outw(ATAO_CFG2_CALLD(chan), dev->iobase + ATAO_CFG2_REG);
		outw(ATAO_CFG2_CALLD_NOP, dev->iobase + ATAO_CFG2_REG);

		s->readback[chan] = val;
	}

	return insn->n;
}

static void atao_reset(struct comedi_device *dev)
{
	struct atao_private *devpriv = dev->private;

	 

	devpriv->cfg1 = 0;
	outw(devpriv->cfg1, dev->iobase + ATAO_CFG1_REG);

	 
	comedi_8254_set_mode(dev->pacer, 0, I8254_MODE4 | I8254_BINARY);
	comedi_8254_set_mode(dev->pacer, 1, I8254_MODE4 | I8254_BINARY);
	comedi_8254_write(dev->pacer, 0, 0x0003);

	outw(ATAO_CFG2_CALLD_NOP, dev->iobase + ATAO_CFG2_REG);

	devpriv->cfg3 = 0;
	outw(devpriv->cfg3, dev->iobase + ATAO_CFG3_REG);

	inw(dev->iobase + ATAO_FIFO_CLEAR_REG);

	atao_select_reg_group(dev, 1);
	outw(0, dev->iobase + ATAO_2_INT1CLR_REG);
	outw(0, dev->iobase + ATAO_2_INT2CLR_REG);
	outw(0, dev->iobase + ATAO_2_DMATCCLR_REG);
	atao_select_reg_group(dev, 0);
}

static int atao_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	const struct atao_board *board = dev->board_ptr;
	struct atao_private *devpriv;
	struct comedi_subdevice *s;
	int ret;

	ret = comedi_request_region(dev, it->options[0], 0x20);
	if (ret)
		return ret;

	devpriv = comedi_alloc_devpriv(dev, sizeof(*devpriv));
	if (!devpriv)
		return -ENOMEM;

	dev->pacer = comedi_8254_init(dev->iobase + ATAO_82C53_BASE,
				      0, I8254_IO8, 0);
	if (!dev->pacer)
		return -ENOMEM;

	ret = comedi_alloc_subdevices(dev, 4);
	if (ret)
		return ret;

	 
	s = &dev->subdevices[0];
	s->type		= COMEDI_SUBD_AO;
	s->subdev_flags	= SDF_WRITABLE;
	s->n_chan	= board->n_ao_chans;
	s->maxdata	= 0x0fff;
	s->range_table	= it->options[3] ? &range_unipolar10 : &range_bipolar10;
	s->insn_write	= atao_ao_insn_write;

	ret = comedi_alloc_subdev_readback(s);
	if (ret)
		return ret;

	 
	s = &dev->subdevices[1];
	s->type		= COMEDI_SUBD_DIO;
	s->subdev_flags	= SDF_READABLE | SDF_WRITABLE;
	s->n_chan	= 8;
	s->maxdata	= 1;
	s->range_table	= &range_digital;
	s->insn_bits	= atao_dio_insn_bits;
	s->insn_config	= atao_dio_insn_config;

	 
	s = &dev->subdevices[2];
	s->type		= COMEDI_SUBD_CALIB;
	s->subdev_flags	= SDF_WRITABLE | SDF_INTERNAL;
	s->n_chan	= (board->n_ao_chans * 2) + 1;
	s->maxdata	= 0xff;
	s->insn_write	= atao_calib_insn_write;

	ret = comedi_alloc_subdev_readback(s);
	if (ret)
		return ret;

	 
	s = &dev->subdevices[3];
	s->type		= COMEDI_SUBD_UNUSED;

	atao_reset(dev);

	return 0;
}

static struct comedi_driver ni_at_ao_driver = {
	.driver_name	= "ni_at_ao",
	.module		= THIS_MODULE,
	.attach		= atao_attach,
	.detach		= comedi_legacy_detach,
	.board_name	= &atao_boards[0].name,
	.offset		= sizeof(struct atao_board),
	.num_names	= ARRAY_SIZE(atao_boards),
};
module_comedi_driver(ni_at_ao_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi driver for NI AT-AO-6/10 boards");
MODULE_LICENSE("GPL");
