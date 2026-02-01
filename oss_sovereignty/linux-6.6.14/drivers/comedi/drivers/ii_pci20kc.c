
 

 

#include <linux/module.h>
#include <linux/io.h>
#include <linux/comedi/comedidev.h>

 
#define II20K_SIZE			0x400
#define II20K_MOD_OFFSET		0x100
#define II20K_ID_REG			0x00
#define II20K_ID_MOD1_EMPTY		BIT(7)
#define II20K_ID_MOD2_EMPTY		BIT(6)
#define II20K_ID_MOD3_EMPTY		BIT(5)
#define II20K_ID_MASK			0x1f
#define II20K_ID_PCI20001C_1A		0x1b	 
#define II20K_ID_PCI20001C_2A		0x1d	 
#define II20K_MOD_STATUS_REG		0x40
#define II20K_MOD_STATUS_IRQ_MOD1	BIT(7)
#define II20K_MOD_STATUS_IRQ_MOD2	BIT(6)
#define II20K_MOD_STATUS_IRQ_MOD3	BIT(5)
#define II20K_DIO0_REG			0x80
#define II20K_DIO1_REG			0x81
#define II20K_DIR_ENA_REG		0x82
#define II20K_DIR_DIO3_OUT		BIT(7)
#define II20K_DIR_DIO2_OUT		BIT(6)
#define II20K_BUF_DISAB_DIO3		BIT(5)
#define II20K_BUF_DISAB_DIO2		BIT(4)
#define II20K_DIR_DIO1_OUT		BIT(3)
#define II20K_DIR_DIO0_OUT		BIT(2)
#define II20K_BUF_DISAB_DIO1		BIT(1)
#define II20K_BUF_DISAB_DIO0		BIT(0)
#define II20K_CTRL01_REG		0x83
#define II20K_CTRL01_SET		BIT(7)
#define II20K_CTRL01_DIO0_IN		BIT(4)
#define II20K_CTRL01_DIO1_IN		BIT(1)
#define II20K_DIO2_REG			0xc0
#define II20K_DIO3_REG			0xc1
#define II20K_CTRL23_REG		0xc3
#define II20K_CTRL23_SET		BIT(7)
#define II20K_CTRL23_DIO2_IN		BIT(4)
#define II20K_CTRL23_DIO3_IN		BIT(1)

#define II20K_ID_PCI20006M_1		0xe2	 
#define II20K_ID_PCI20006M_2		0xe3	 
#define II20K_AO_STRB_REG(x)		(0x0b + ((x) * 0x08))
#define II20K_AO_LSB_REG(x)		(0x0d + ((x) * 0x08))
#define II20K_AO_MSB_REG(x)		(0x0e + ((x) * 0x08))
#define II20K_AO_STRB_BOTH_REG		0x1b

#define II20K_ID_PCI20341M_1		0x77	 
#define II20K_AI_STATUS_CMD_REG		0x01
#define II20K_AI_STATUS_CMD_BUSY	BIT(7)
#define II20K_AI_STATUS_CMD_HW_ENA	BIT(1)
#define II20K_AI_STATUS_CMD_EXT_START	BIT(0)
#define II20K_AI_LSB_REG		0x02
#define II20K_AI_MSB_REG		0x03
#define II20K_AI_PACER_RESET_REG	0x04
#define II20K_AI_16BIT_DATA_REG		0x06
#define II20K_AI_CONF_REG		0x10
#define II20K_AI_CONF_ENA		BIT(2)
#define II20K_AI_OPT_REG		0x11
#define II20K_AI_OPT_TRIG_ENA		BIT(5)
#define II20K_AI_OPT_TRIG_INV		BIT(4)
#define II20K_AI_OPT_TIMEBASE(x)	(((x) & 0x3) << 1)
#define II20K_AI_OPT_BURST_MODE		BIT(0)
#define II20K_AI_STATUS_REG		0x12
#define II20K_AI_STATUS_INT		BIT(7)
#define II20K_AI_STATUS_TRIG		BIT(6)
#define II20K_AI_STATUS_TRIG_ENA	BIT(5)
#define II20K_AI_STATUS_PACER_ERR	BIT(2)
#define II20K_AI_STATUS_DATA_ERR	BIT(1)
#define II20K_AI_STATUS_SET_TIME_ERR	BIT(0)
#define II20K_AI_LAST_CHAN_ADDR_REG	0x13
#define II20K_AI_CUR_ADDR_REG		0x14
#define II20K_AI_SET_TIME_REG		0x15
#define II20K_AI_DELAY_LSB_REG		0x16
#define II20K_AI_DELAY_MSB_REG		0x17
#define II20K_AI_CHAN_ADV_REG		0x18
#define II20K_AI_CHAN_RESET_REG		0x19
#define II20K_AI_START_TRIG_REG		0x1a
#define II20K_AI_COUNT_RESET_REG	0x1b
#define II20K_AI_CHANLIST_REG		0x80
#define II20K_AI_CHANLIST_ONBOARD_ONLY	BIT(5)
#define II20K_AI_CHANLIST_GAIN(x)	(((x) & 0x3) << 3)
#define II20K_AI_CHANLIST_MUX_ENA	BIT(2)
#define II20K_AI_CHANLIST_CHAN(x)	(((x) & 0x3) << 0)
#define II20K_AI_CHANLIST_LEN		0x80

 
static const struct comedi_lrange ii20k_ao_ranges = {
	3, {
		BIP_RANGE(5),	 
		UNI_RANGE(10),	 
		BIP_RANGE(10)	 
	}
};

static const struct comedi_lrange ii20k_ai_ranges = {
	4, {
		BIP_RANGE(5),		 
		BIP_RANGE(0.5),		 
		BIP_RANGE(0.05),	 
		BIP_RANGE(0.025)	 
	},
};

static void __iomem *ii20k_module_iobase(struct comedi_device *dev,
					 struct comedi_subdevice *s)
{
	return dev->mmio + (s->index + 1) * II20K_MOD_OFFSET;
}

static int ii20k_ao_insn_write(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn,
			       unsigned int *data)
{
	void __iomem *iobase = ii20k_module_iobase(dev, s);
	unsigned int chan = CR_CHAN(insn->chanspec);
	int i;

	for (i = 0; i < insn->n; i++) {
		unsigned int val = data[i];

		s->readback[chan] = val;

		 
		val = comedi_offset_munge(s, val);

		writeb(val & 0xff, iobase + II20K_AO_LSB_REG(chan));
		writeb((val >> 8) & 0xff, iobase + II20K_AO_MSB_REG(chan));
		writeb(0x00, iobase + II20K_AO_STRB_REG(chan));
	}

	return insn->n;
}

static int ii20k_ai_eoc(struct comedi_device *dev,
			struct comedi_subdevice *s,
			struct comedi_insn *insn,
			unsigned long context)
{
	void __iomem *iobase = ii20k_module_iobase(dev, s);
	unsigned char status;

	status = readb(iobase + II20K_AI_STATUS_REG);
	if ((status & II20K_AI_STATUS_INT) == 0)
		return 0;
	return -EBUSY;
}

static void ii20k_ai_setup(struct comedi_device *dev,
			   struct comedi_subdevice *s,
			   unsigned int chanspec)
{
	void __iomem *iobase = ii20k_module_iobase(dev, s);
	unsigned int chan = CR_CHAN(chanspec);
	unsigned int range = CR_RANGE(chanspec);
	unsigned char val;

	 
	writeb(II20K_AI_CONF_ENA, iobase + II20K_AI_CONF_REG);

	 
	writeb(0, iobase + II20K_AI_STATUS_CMD_REG);

	 
	val = (range < 3) ? II20K_AI_OPT_TIMEBASE(0) : II20K_AI_OPT_TIMEBASE(2);
	writeb(val, iobase + II20K_AI_OPT_REG);

	 
	val = (range < 2) ? 0x58 : (range < 3) ? 0x93 : 0x99;
	writeb(val, iobase + II20K_AI_SET_TIME_REG);

	 
	writeb(1, iobase + II20K_AI_LAST_CHAN_ADDR_REG);

	 
	val = II20K_AI_CHANLIST_ONBOARD_ONLY |
	      II20K_AI_CHANLIST_MUX_ENA |
	      II20K_AI_CHANLIST_GAIN(range) |
	      II20K_AI_CHANLIST_CHAN(chan);
	writeb(val, iobase + II20K_AI_CHANLIST_REG);

	 
	writeb(0, iobase + II20K_AI_COUNT_RESET_REG);

	 
	writeb(0, iobase + II20K_AI_CHAN_RESET_REG);
}

static int ii20k_ai_insn_read(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn,
			      unsigned int *data)
{
	void __iomem *iobase = ii20k_module_iobase(dev, s);
	int ret;
	int i;

	ii20k_ai_setup(dev, s, insn->chanspec);

	for (i = 0; i < insn->n; i++) {
		unsigned int val;

		 
		readb(iobase + II20K_AI_PACER_RESET_REG);

		ret = comedi_timeout(dev, s, insn, ii20k_ai_eoc, 0);
		if (ret)
			return ret;

		val = readb(iobase + II20K_AI_LSB_REG);
		val |= (readb(iobase + II20K_AI_MSB_REG) << 8);

		 
		data[i] = comedi_offset_munge(s, val);
	}

	return insn->n;
}

static void ii20k_dio_config(struct comedi_device *dev,
			     struct comedi_subdevice *s)
{
	unsigned char ctrl01 = 0;
	unsigned char ctrl23 = 0;
	unsigned char dir_ena = 0;

	 
	if (s->io_bits & 0x000000ff) {
		 
		ctrl01 &= ~II20K_CTRL01_DIO0_IN;
		dir_ena &= ~II20K_BUF_DISAB_DIO0;
		dir_ena |= II20K_DIR_DIO0_OUT;
	} else {
		 
		ctrl01 |= II20K_CTRL01_DIO0_IN;
		dir_ena &= ~II20K_DIR_DIO0_OUT;
	}

	 
	if (s->io_bits & 0x0000ff00) {
		 
		ctrl01 &= ~II20K_CTRL01_DIO1_IN;
		dir_ena &= ~II20K_BUF_DISAB_DIO1;
		dir_ena |= II20K_DIR_DIO1_OUT;
	} else {
		 
		ctrl01 |= II20K_CTRL01_DIO1_IN;
		dir_ena &= ~II20K_DIR_DIO1_OUT;
	}

	 
	if (s->io_bits & 0x00ff0000) {
		 
		ctrl23 &= ~II20K_CTRL23_DIO2_IN;
		dir_ena &= ~II20K_BUF_DISAB_DIO2;
		dir_ena |= II20K_DIR_DIO2_OUT;
	} else {
		 
		ctrl23 |= II20K_CTRL23_DIO2_IN;
		dir_ena &= ~II20K_DIR_DIO2_OUT;
	}

	 
	if (s->io_bits & 0xff000000) {
		 
		ctrl23 &= ~II20K_CTRL23_DIO3_IN;
		dir_ena &= ~II20K_BUF_DISAB_DIO3;
		dir_ena |= II20K_DIR_DIO3_OUT;
	} else {
		 
		ctrl23 |= II20K_CTRL23_DIO3_IN;
		dir_ena &= ~II20K_DIR_DIO3_OUT;
	}

	ctrl23 |= II20K_CTRL01_SET;
	ctrl23 |= II20K_CTRL23_SET;

	 
	writeb(ctrl01, dev->mmio + II20K_CTRL01_REG);
	writeb(ctrl23, dev->mmio + II20K_CTRL23_REG);
	writeb(dir_ena, dev->mmio + II20K_DIR_ENA_REG);
}

static int ii20k_dio_insn_config(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn,
				 unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int mask;
	int ret;

	if (chan < 8)
		mask = 0x000000ff;
	else if (chan < 16)
		mask = 0x0000ff00;
	else if (chan < 24)
		mask = 0x00ff0000;
	else
		mask = 0xff000000;

	ret = comedi_dio_insn_config(dev, s, insn, data, mask);
	if (ret)
		return ret;

	ii20k_dio_config(dev, s);

	return insn->n;
}

static int ii20k_dio_insn_bits(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn,
			       unsigned int *data)
{
	unsigned int mask;

	mask = comedi_dio_update_state(s, data);
	if (mask) {
		if (mask & 0x000000ff)
			writeb((s->state >> 0) & 0xff,
			       dev->mmio + II20K_DIO0_REG);
		if (mask & 0x0000ff00)
			writeb((s->state >> 8) & 0xff,
			       dev->mmio + II20K_DIO1_REG);
		if (mask & 0x00ff0000)
			writeb((s->state >> 16) & 0xff,
			       dev->mmio + II20K_DIO2_REG);
		if (mask & 0xff000000)
			writeb((s->state >> 24) & 0xff,
			       dev->mmio + II20K_DIO3_REG);
	}

	data[1] = readb(dev->mmio + II20K_DIO0_REG);
	data[1] |= readb(dev->mmio + II20K_DIO1_REG) << 8;
	data[1] |= readb(dev->mmio + II20K_DIO2_REG) << 16;
	data[1] |= readb(dev->mmio + II20K_DIO3_REG) << 24;

	return insn->n;
}

static int ii20k_init_module(struct comedi_device *dev,
			     struct comedi_subdevice *s)
{
	void __iomem *iobase = ii20k_module_iobase(dev, s);
	unsigned char id;
	int ret;

	id = readb(iobase + II20K_ID_REG);
	switch (id) {
	case II20K_ID_PCI20006M_1:
	case II20K_ID_PCI20006M_2:
		 
		s->type		= COMEDI_SUBD_AO;
		s->subdev_flags	= SDF_WRITABLE;
		s->n_chan	= (id == II20K_ID_PCI20006M_2) ? 2 : 1;
		s->maxdata	= 0xffff;
		s->range_table	= &ii20k_ao_ranges;
		s->insn_write	= ii20k_ao_insn_write;

		ret = comedi_alloc_subdev_readback(s);
		if (ret)
			return ret;
		break;
	case II20K_ID_PCI20341M_1:
		 
		s->type		= COMEDI_SUBD_AI;
		s->subdev_flags	= SDF_READABLE | SDF_DIFF;
		s->n_chan	= 4;
		s->maxdata	= 0xffff;
		s->range_table	= &ii20k_ai_ranges;
		s->insn_read	= ii20k_ai_insn_read;
		break;
	default:
		s->type = COMEDI_SUBD_UNUSED;
		break;
	}

	return 0;
}

static int ii20k_attach(struct comedi_device *dev,
			struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	unsigned int membase;
	unsigned char id;
	bool has_dio;
	int ret;

	membase = it->options[0];
	if (!membase || (membase & ~(0x100000 - II20K_SIZE))) {
		dev_warn(dev->class_dev,
			 "%s: invalid memory address specified\n",
			 dev->board_name);
		return -EINVAL;
	}

	if (!request_mem_region(membase, II20K_SIZE, dev->board_name)) {
		dev_warn(dev->class_dev, "%s: I/O mem conflict (%#x,%u)\n",
			 dev->board_name, membase, II20K_SIZE);
		return -EIO;
	}
	dev->iobase = membase;	 

	dev->mmio = ioremap(membase, II20K_SIZE);
	if (!dev->mmio)
		return -ENOMEM;

	id = readb(dev->mmio + II20K_ID_REG);
	switch (id & II20K_ID_MASK) {
	case II20K_ID_PCI20001C_1A:
		has_dio = false;
		break;
	case II20K_ID_PCI20001C_2A:
		has_dio = true;
		break;
	default:
		return -ENODEV;
	}

	ret = comedi_alloc_subdevices(dev, 4);
	if (ret)
		return ret;

	s = &dev->subdevices[0];
	if (id & II20K_ID_MOD1_EMPTY) {
		s->type = COMEDI_SUBD_UNUSED;
	} else {
		ret = ii20k_init_module(dev, s);
		if (ret)
			return ret;
	}

	s = &dev->subdevices[1];
	if (id & II20K_ID_MOD2_EMPTY) {
		s->type = COMEDI_SUBD_UNUSED;
	} else {
		ret = ii20k_init_module(dev, s);
		if (ret)
			return ret;
	}

	s = &dev->subdevices[2];
	if (id & II20K_ID_MOD3_EMPTY) {
		s->type = COMEDI_SUBD_UNUSED;
	} else {
		ret = ii20k_init_module(dev, s);
		if (ret)
			return ret;
	}

	 
	s = &dev->subdevices[3];
	if (has_dio) {
		s->type		= COMEDI_SUBD_DIO;
		s->subdev_flags	= SDF_READABLE | SDF_WRITABLE;
		s->n_chan	= 32;
		s->maxdata	= 1;
		s->range_table	= &range_digital;
		s->insn_bits	= ii20k_dio_insn_bits;
		s->insn_config	= ii20k_dio_insn_config;

		 
		ii20k_dio_config(dev, s);
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	return 0;
}

static void ii20k_detach(struct comedi_device *dev)
{
	if (dev->mmio)
		iounmap(dev->mmio);
	if (dev->iobase)	 
		release_mem_region(dev->iobase, II20K_SIZE);
}

static struct comedi_driver ii20k_driver = {
	.driver_name	= "ii_pci20kc",
	.module		= THIS_MODULE,
	.attach		= ii20k_attach,
	.detach		= ii20k_detach,
};
module_comedi_driver(ii20k_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi driver for Intelligent Instruments PCI-20001C");
MODULE_LICENSE("GPL");
