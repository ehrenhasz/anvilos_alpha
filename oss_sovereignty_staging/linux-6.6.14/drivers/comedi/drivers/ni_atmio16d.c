
 

 

 

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/comedi/comedidev.h>
#include <linux/comedi/comedi_8255.h>

 
#define COM_REG_1	0x00	 
#define STAT_REG	0x00	 
#define COM_REG_2	0x02	 
 
#define START_CONVERT_REG	0x08	 
#define START_DAQ_REG		0x0A	 
#define AD_CLEAR_REG		0x0C	 
#define EXT_STROBE_REG		0x0E	 
 
#define DAC0_REG		0x10	 
#define DAC1_REG		0x12	 
#define INT2CLR_REG		0x14	 
 
#define MUX_CNTR_REG		0x04	 
#define MUX_GAIN_REG		0x06	 
#define AD_FIFO_REG		0x16	 
#define DMA_TC_INT_CLR_REG	0x16	 
 
#define AM9513A_DATA_REG	0x18	 
#define AM9513A_COM_REG		0x1A	 
#define AM9513A_STAT_REG	0x1A	 
 
#define MIO_16_DIG_IN_REG	0x1C	 
#define MIO_16_DIG_OUT_REG	0x1C	 
 
#define RTSI_SW_SHIFT_REG	0x1E	 
#define RTSI_SW_STROBE_REG	0x1F	 
 
#define DIO_24_PORTA_REG	0x00	 
#define DIO_24_PORTB_REG	0x01	 
#define DIO_24_PORTC_REG	0x02	 
#define DIO_24_CNFG_REG		0x03	 

 
#define COMREG1_2SCADC		0x0001
#define COMREG1_1632CNT		0x0002
#define COMREG1_SCANEN		0x0008
#define COMREG1_DAQEN		0x0010
#define COMREG1_DMAEN		0x0020
#define COMREG1_CONVINTEN	0x0080
#define COMREG2_SCN2		0x0010
#define COMREG2_INTEN		0x0080
#define COMREG2_DOUTEN0		0x0100
#define COMREG2_DOUTEN1		0x0200
 
#define STAT_AD_OVERRUN		0x0100
#define STAT_AD_OVERFLOW	0x0200
#define STAT_AD_DAQPROG		0x0800
#define STAT_AD_CONVAVAIL	0x2000
#define STAT_AD_DAQSTOPINT	0x4000
 
#define CLOCK_1_MHZ		0x8B25
#define CLOCK_100_KHZ	0x8C25
#define CLOCK_10_KHZ	0x8D25
#define CLOCK_1_KHZ		0x8E25
#define CLOCK_100_HZ	0x8F25

struct atmio16_board_t {
	const char *name;
	int has_8255;
};

 
static const struct comedi_lrange range_atmio16d_ai_10_bipolar = {
	4, {
		BIP_RANGE(10),
		BIP_RANGE(1),
		BIP_RANGE(0.1),
		BIP_RANGE(0.02)
	}
};

static const struct comedi_lrange range_atmio16d_ai_5_bipolar = {
	4, {
		BIP_RANGE(5),
		BIP_RANGE(0.5),
		BIP_RANGE(0.05),
		BIP_RANGE(0.01)
	}
};

static const struct comedi_lrange range_atmio16d_ai_unipolar = {
	4, {
		UNI_RANGE(10),
		UNI_RANGE(1),
		UNI_RANGE(0.1),
		UNI_RANGE(0.02)
	}
};

 
struct atmio16d_private {
	enum { adc_diff, adc_singleended } adc_mux;
	enum { adc_bipolar10, adc_bipolar5, adc_unipolar10 } adc_range;
	enum { adc_2comp, adc_straight } adc_coding;
	enum { dac_bipolar, dac_unipolar } dac0_range, dac1_range;
	enum { dac_internal, dac_external } dac0_reference, dac1_reference;
	enum { dac_2comp, dac_straight } dac0_coding, dac1_coding;
	const struct comedi_lrange *ao_range_type_list[2];
	unsigned int com_reg_1_state;  
	unsigned int com_reg_2_state;  
};

static void reset_counters(struct comedi_device *dev)
{
	 
	outw(0xFFC2, dev->iobase + AM9513A_COM_REG);
	outw(0xFF02, dev->iobase + AM9513A_COM_REG);
	outw(0x4, dev->iobase + AM9513A_DATA_REG);
	outw(0xFF0A, dev->iobase + AM9513A_COM_REG);
	outw(0x3, dev->iobase + AM9513A_DATA_REG);
	outw(0xFF42, dev->iobase + AM9513A_COM_REG);
	outw(0xFF42, dev->iobase + AM9513A_COM_REG);
	 
	outw(0xFFC4, dev->iobase + AM9513A_COM_REG);
	outw(0xFF03, dev->iobase + AM9513A_COM_REG);
	outw(0x4, dev->iobase + AM9513A_DATA_REG);
	outw(0xFF0B, dev->iobase + AM9513A_COM_REG);
	outw(0x3, dev->iobase + AM9513A_DATA_REG);
	outw(0xFF44, dev->iobase + AM9513A_COM_REG);
	outw(0xFF44, dev->iobase + AM9513A_COM_REG);
	 
	outw(0xFFC8, dev->iobase + AM9513A_COM_REG);
	outw(0xFF04, dev->iobase + AM9513A_COM_REG);
	outw(0x4, dev->iobase + AM9513A_DATA_REG);
	outw(0xFF0C, dev->iobase + AM9513A_COM_REG);
	outw(0x3, dev->iobase + AM9513A_DATA_REG);
	outw(0xFF48, dev->iobase + AM9513A_COM_REG);
	outw(0xFF48, dev->iobase + AM9513A_COM_REG);
	 
	outw(0xFFD0, dev->iobase + AM9513A_COM_REG);
	outw(0xFF05, dev->iobase + AM9513A_COM_REG);
	outw(0x4, dev->iobase + AM9513A_DATA_REG);
	outw(0xFF0D, dev->iobase + AM9513A_COM_REG);
	outw(0x3, dev->iobase + AM9513A_DATA_REG);
	outw(0xFF50, dev->iobase + AM9513A_COM_REG);
	outw(0xFF50, dev->iobase + AM9513A_COM_REG);

	outw(0, dev->iobase + AD_CLEAR_REG);
}

static void reset_atmio16d(struct comedi_device *dev)
{
	struct atmio16d_private *devpriv = dev->private;
	int i;

	 
	outw(0, dev->iobase + COM_REG_1);
	outw(0, dev->iobase + COM_REG_2);
	outw(0, dev->iobase + MUX_GAIN_REG);
	 
	outw(0xFFFF, dev->iobase + AM9513A_COM_REG);
	outw(0xFFEF, dev->iobase + AM9513A_COM_REG);
	outw(0xFF17, dev->iobase + AM9513A_COM_REG);
	outw(0xF000, dev->iobase + AM9513A_DATA_REG);
	for (i = 1; i <= 5; ++i) {
		outw(0xFF00 + i, dev->iobase + AM9513A_COM_REG);
		outw(0x0004, dev->iobase + AM9513A_DATA_REG);
		outw(0xFF08 + i, dev->iobase + AM9513A_COM_REG);
		outw(0x3, dev->iobase + AM9513A_DATA_REG);
	}
	outw(0xFF5F, dev->iobase + AM9513A_COM_REG);
	 
	outw(0, dev->iobase + AD_CLEAR_REG);
	outw(0, dev->iobase + INT2CLR_REG);
	 
	devpriv->com_reg_1_state |= 1;
	outw(devpriv->com_reg_1_state, dev->iobase + COM_REG_1);
	devpriv->adc_coding = adc_straight;
	 
	outw(2048, dev->iobase + DAC0_REG);
	outw(2048, dev->iobase + DAC1_REG);
}

static irqreturn_t atmio16d_interrupt(int irq, void *d)
{
	struct comedi_device *dev = d;
	struct comedi_subdevice *s = dev->read_subdev;
	unsigned short val;

	val = inw(dev->iobase + AD_FIFO_REG);
	comedi_buf_write_samples(s, &val, 1);
	comedi_handle_events(dev, s);

	return IRQ_HANDLED;
}

static int atmio16d_ai_cmdtest(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_cmd *cmd)
{
	int err = 0;

	 

	err |= comedi_check_trigger_src(&cmd->start_src, TRIG_NOW);
	err |= comedi_check_trigger_src(&cmd->scan_begin_src,
					TRIG_FOLLOW | TRIG_TIMER);
	err |= comedi_check_trigger_src(&cmd->convert_src, TRIG_TIMER);
	err |= comedi_check_trigger_src(&cmd->scan_end_src, TRIG_COUNT);
	err |= comedi_check_trigger_src(&cmd->stop_src, TRIG_COUNT | TRIG_NONE);

	if (err)
		return 1;

	 

	err |= comedi_check_trigger_is_unique(cmd->scan_begin_src);
	err |= comedi_check_trigger_is_unique(cmd->stop_src);

	 

	if (err)
		return 2;

	 

	err |= comedi_check_trigger_arg_is(&cmd->start_arg, 0);

	if (cmd->scan_begin_src == TRIG_FOLLOW) {
		 
		err |= comedi_check_trigger_arg_is(&cmd->scan_begin_arg, 0);
	}

	err |= comedi_check_trigger_arg_min(&cmd->convert_arg, 10000);
	err |= comedi_check_trigger_arg_is(&cmd->scan_end_arg,
					   cmd->chanlist_len);

	if (cmd->stop_src == TRIG_COUNT)
		err |= comedi_check_trigger_arg_min(&cmd->stop_arg, 1);
	else	 
		err |= comedi_check_trigger_arg_is(&cmd->stop_arg, 0);

	if (err)
		return 3;

	return 0;
}

static int atmio16d_ai_cmd(struct comedi_device *dev,
			   struct comedi_subdevice *s)
{
	struct atmio16d_private *devpriv = dev->private;
	struct comedi_cmd *cmd = &s->async->cmd;
	unsigned int timer, base_clock;
	unsigned int sample_count, tmp, chan, gain;
	int i;

	 

	reset_counters(dev);

	 
	if (cmd->chanlist_len < 2) {
		devpriv->com_reg_1_state &= ~COMREG1_SCANEN;
		outw(devpriv->com_reg_1_state, dev->iobase + COM_REG_1);
	} else {
		devpriv->com_reg_1_state |= COMREG1_SCANEN;
		devpriv->com_reg_2_state |= COMREG2_SCN2;
		outw(devpriv->com_reg_1_state, dev->iobase + COM_REG_1);
		outw(devpriv->com_reg_2_state, dev->iobase + COM_REG_2);
	}

	 
	for (i = 0; i < cmd->chanlist_len; ++i) {
		chan = CR_CHAN(cmd->chanlist[i]);
		gain = CR_RANGE(cmd->chanlist[i]);
		outw(i, dev->iobase + MUX_CNTR_REG);
		tmp = chan | (gain << 6);
		if (i == cmd->scan_end_arg - 1)
			tmp |= 0x0010;	 
		outw(tmp, dev->iobase + MUX_GAIN_REG);
	}

	 
	if (cmd->convert_arg < 65536000) {
		base_clock = CLOCK_1_MHZ;
		timer = cmd->convert_arg / 1000;
	} else if (cmd->convert_arg < 655360000) {
		base_clock = CLOCK_100_KHZ;
		timer = cmd->convert_arg / 10000;
	} else   {
		base_clock = CLOCK_10_KHZ;
		timer = cmd->convert_arg / 100000;
	}
	outw(0xFF03, dev->iobase + AM9513A_COM_REG);
	outw(base_clock, dev->iobase + AM9513A_DATA_REG);
	outw(0xFF0B, dev->iobase + AM9513A_COM_REG);
	outw(0x2, dev->iobase + AM9513A_DATA_REG);
	outw(0xFF44, dev->iobase + AM9513A_COM_REG);
	outw(0xFFF3, dev->iobase + AM9513A_COM_REG);
	outw(timer, dev->iobase + AM9513A_DATA_REG);
	outw(0xFF24, dev->iobase + AM9513A_COM_REG);

	 
	 
	sample_count = cmd->stop_arg * cmd->scan_end_arg;
	outw(0xFF04, dev->iobase + AM9513A_COM_REG);
	outw(0x1025, dev->iobase + AM9513A_DATA_REG);
	outw(0xFF0C, dev->iobase + AM9513A_COM_REG);
	if (sample_count < 65536) {
		 
		outw(sample_count, dev->iobase + AM9513A_DATA_REG);
		outw(0xFF48, dev->iobase + AM9513A_COM_REG);
		outw(0xFFF4, dev->iobase + AM9513A_COM_REG);
		outw(0xFF28, dev->iobase + AM9513A_COM_REG);
		devpriv->com_reg_1_state &= ~COMREG1_1632CNT;
		outw(devpriv->com_reg_1_state, dev->iobase + COM_REG_1);
	} else {
		 

		tmp = sample_count & 0xFFFF;
		if (tmp)
			outw(tmp - 1, dev->iobase + AM9513A_DATA_REG);
		else
			outw(0xFFFF, dev->iobase + AM9513A_DATA_REG);

		outw(0xFF48, dev->iobase + AM9513A_COM_REG);
		outw(0, dev->iobase + AM9513A_DATA_REG);
		outw(0xFF28, dev->iobase + AM9513A_COM_REG);
		outw(0xFF05, dev->iobase + AM9513A_COM_REG);
		outw(0x25, dev->iobase + AM9513A_DATA_REG);
		outw(0xFF0D, dev->iobase + AM9513A_COM_REG);
		tmp = sample_count & 0xFFFF;
		if ((tmp == 0) || (tmp == 1)) {
			outw((sample_count >> 16) & 0xFFFF,
			     dev->iobase + AM9513A_DATA_REG);
		} else {
			outw(((sample_count >> 16) & 0xFFFF) + 1,
			     dev->iobase + AM9513A_DATA_REG);
		}
		outw(0xFF70, dev->iobase + AM9513A_COM_REG);
		devpriv->com_reg_1_state |= COMREG1_1632CNT;
		outw(devpriv->com_reg_1_state, dev->iobase + COM_REG_1);
	}

	 
	if (cmd->chanlist_len > 1) {
		if (cmd->scan_begin_arg < 65536000) {
			base_clock = CLOCK_1_MHZ;
			timer = cmd->scan_begin_arg / 1000;
		} else if (cmd->scan_begin_arg < 655360000) {
			base_clock = CLOCK_100_KHZ;
			timer = cmd->scan_begin_arg / 10000;
		} else   {
			base_clock = CLOCK_10_KHZ;
			timer = cmd->scan_begin_arg / 100000;
		}
		outw(0xFF02, dev->iobase + AM9513A_COM_REG);
		outw(base_clock, dev->iobase + AM9513A_DATA_REG);
		outw(0xFF0A, dev->iobase + AM9513A_COM_REG);
		outw(0x2, dev->iobase + AM9513A_DATA_REG);
		outw(0xFF42, dev->iobase + AM9513A_COM_REG);
		outw(0xFFF2, dev->iobase + AM9513A_COM_REG);
		outw(timer, dev->iobase + AM9513A_DATA_REG);
		outw(0xFF22, dev->iobase + AM9513A_COM_REG);
	}

	 
	outw(0, dev->iobase + AD_CLEAR_REG);
	outw(0, dev->iobase + MUX_CNTR_REG);
	outw(0, dev->iobase + INT2CLR_REG);
	 
	devpriv->com_reg_1_state |= COMREG1_DAQEN;
	outw(devpriv->com_reg_1_state, dev->iobase + COM_REG_1);
	 
	devpriv->com_reg_1_state |= COMREG1_CONVINTEN;
	devpriv->com_reg_2_state |= COMREG2_INTEN;
	outw(devpriv->com_reg_1_state, dev->iobase + COM_REG_1);
	outw(devpriv->com_reg_2_state, dev->iobase + COM_REG_2);
	 
	outw(0, dev->iobase + START_DAQ_REG);

	return 0;
}

 
static int atmio16d_ai_cancel(struct comedi_device *dev,
			      struct comedi_subdevice *s)
{
	reset_atmio16d(dev);

	return 0;
}

static int atmio16d_ai_eoc(struct comedi_device *dev,
			   struct comedi_subdevice *s,
			   struct comedi_insn *insn,
			   unsigned long context)
{
	unsigned int status;

	status = inw(dev->iobase + STAT_REG);
	if (status & STAT_AD_CONVAVAIL)
		return 0;
	if (status & STAT_AD_OVERFLOW) {
		outw(0, dev->iobase + AD_CLEAR_REG);
		return -EOVERFLOW;
	}
	return -EBUSY;
}

static int atmio16d_ai_insn_read(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn, unsigned int *data)
{
	struct atmio16d_private *devpriv = dev->private;
	int i;
	int chan;
	int gain;
	int ret;

	chan = CR_CHAN(insn->chanspec);
	gain = CR_RANGE(insn->chanspec);

	 
	 
	 
	 

	 
	outw(chan | (gain << 6), dev->iobase + MUX_GAIN_REG);

	for (i = 0; i < insn->n; i++) {
		 
		outw(0, dev->iobase + START_CONVERT_REG);

		 
		ret = comedi_timeout(dev, s, insn, atmio16d_ai_eoc, 0);
		if (ret)
			return ret;

		 
		data[i] = inw(dev->iobase + AD_FIFO_REG);
		 
		if (devpriv->adc_coding == adc_2comp)
			data[i] ^= 0x800;
	}

	return i;
}

static int atmio16d_ao_insn_write(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn,
				  unsigned int *data)
{
	struct atmio16d_private *devpriv = dev->private;
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int reg = (chan) ? DAC1_REG : DAC0_REG;
	bool munge = false;
	int i;

	if (chan == 0 && devpriv->dac0_coding == dac_2comp)
		munge = true;
	if (chan == 1 && devpriv->dac1_coding == dac_2comp)
		munge = true;

	for (i = 0; i < insn->n; i++) {
		unsigned int val = data[i];

		s->readback[chan] = val;

		if (munge)
			val ^= 0x800;

		outw(val, dev->iobase + reg);
	}

	return insn->n;
}

static int atmio16d_dio_insn_bits(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn,
				  unsigned int *data)
{
	if (comedi_dio_update_state(s, data))
		outw(s->state, dev->iobase + MIO_16_DIG_OUT_REG);

	data[1] = inw(dev->iobase + MIO_16_DIG_IN_REG);

	return insn->n;
}

static int atmio16d_dio_insn_config(struct comedi_device *dev,
				    struct comedi_subdevice *s,
				    struct comedi_insn *insn,
				    unsigned int *data)
{
	struct atmio16d_private *devpriv = dev->private;
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

	devpriv->com_reg_2_state &= ~(COMREG2_DOUTEN0 | COMREG2_DOUTEN1);
	if (s->io_bits & 0x0f)
		devpriv->com_reg_2_state |= COMREG2_DOUTEN0;
	if (s->io_bits & 0xf0)
		devpriv->com_reg_2_state |= COMREG2_DOUTEN1;
	outw(devpriv->com_reg_2_state, dev->iobase + COM_REG_2);

	return insn->n;
}

static int atmio16d_attach(struct comedi_device *dev,
			   struct comedi_devconfig *it)
{
	const struct atmio16_board_t *board = dev->board_ptr;
	struct atmio16d_private *devpriv;
	struct comedi_subdevice *s;
	int ret;

	ret = comedi_request_region(dev, it->options[0], 0x20);
	if (ret)
		return ret;

	ret = comedi_alloc_subdevices(dev, 4);
	if (ret)
		return ret;

	devpriv = comedi_alloc_devpriv(dev, sizeof(*devpriv));
	if (!devpriv)
		return -ENOMEM;

	 
	reset_atmio16d(dev);

	if (it->options[1]) {
		ret = request_irq(it->options[1], atmio16d_interrupt, 0,
				  dev->board_name, dev);
		if (ret == 0)
			dev->irq = it->options[1];
	}

	 
	devpriv->adc_mux = it->options[5];
	devpriv->adc_range = it->options[6];

	devpriv->dac0_range = it->options[7];
	devpriv->dac0_reference = it->options[8];
	devpriv->dac0_coding = it->options[9];
	devpriv->dac1_range = it->options[10];
	devpriv->dac1_reference = it->options[11];
	devpriv->dac1_coding = it->options[12];

	 
	s = &dev->subdevices[0];
	 
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags = SDF_READABLE | SDF_GROUND;
	s->n_chan = (devpriv->adc_mux ? 16 : 8);
	s->insn_read = atmio16d_ai_insn_read;
	s->maxdata = 0xfff;	 
	switch (devpriv->adc_range) {
	case adc_bipolar10:
		s->range_table = &range_atmio16d_ai_10_bipolar;
		break;
	case adc_bipolar5:
		s->range_table = &range_atmio16d_ai_5_bipolar;
		break;
	case adc_unipolar10:
		s->range_table = &range_atmio16d_ai_unipolar;
		break;
	}
	if (dev->irq) {
		dev->read_subdev = s;
		s->subdev_flags |= SDF_CMD_READ;
		s->len_chanlist = 16;
		s->do_cmdtest = atmio16d_ai_cmdtest;
		s->do_cmd = atmio16d_ai_cmd;
		s->cancel = atmio16d_ai_cancel;
	}

	 
	s = &dev->subdevices[1];
	s->type = COMEDI_SUBD_AO;
	s->subdev_flags = SDF_WRITABLE;
	s->n_chan = 2;
	s->maxdata = 0xfff;	 
	s->range_table_list = devpriv->ao_range_type_list;
	switch (devpriv->dac0_range) {
	case dac_bipolar:
		devpriv->ao_range_type_list[0] = &range_bipolar10;
		break;
	case dac_unipolar:
		devpriv->ao_range_type_list[0] = &range_unipolar10;
		break;
	}
	switch (devpriv->dac1_range) {
	case dac_bipolar:
		devpriv->ao_range_type_list[1] = &range_bipolar10;
		break;
	case dac_unipolar:
		devpriv->ao_range_type_list[1] = &range_unipolar10;
		break;
	}
	s->insn_write = atmio16d_ao_insn_write;

	ret = comedi_alloc_subdev_readback(s);
	if (ret)
		return ret;

	 
	s = &dev->subdevices[2];
	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_WRITABLE | SDF_READABLE;
	s->n_chan = 8;
	s->insn_bits = atmio16d_dio_insn_bits;
	s->insn_config = atmio16d_dio_insn_config;
	s->maxdata = 1;
	s->range_table = &range_digital;

	 
	s = &dev->subdevices[3];
	if (board->has_8255) {
		ret = subdev_8255_init(dev, s, NULL, 0x00);
		if (ret)
			return ret;
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

 
#if 0
	s = &dev->subdevices[4];
	 
	s->type = COMEDI_SUBD_TIMER;
	s->n_chan = 0;
	s->maxdata = 0
#endif

	return 0;
}

static void atmio16d_detach(struct comedi_device *dev)
{
	reset_atmio16d(dev);
	comedi_legacy_detach(dev);
}

static const struct atmio16_board_t atmio16_boards[] = {
	{
		.name		= "atmio16",
		.has_8255	= 0,
	}, {
		.name		= "atmio16d",
		.has_8255	= 1,
	},
};

static struct comedi_driver atmio16d_driver = {
	.driver_name	= "atmio16",
	.module		= THIS_MODULE,
	.attach		= atmio16d_attach,
	.detach		= atmio16d_detach,
	.board_name	= &atmio16_boards[0].name,
	.num_names	= ARRAY_SIZE(atmio16_boards),
	.offset		= sizeof(struct atmio16_board_t),
};
module_comedi_driver(atmio16d_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
