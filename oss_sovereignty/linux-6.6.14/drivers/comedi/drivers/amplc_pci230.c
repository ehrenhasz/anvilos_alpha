
 

 

 

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/comedi/comedi_pci.h>
#include <linux/comedi/comedi_8255.h>
#include <linux/comedi/comedi_8254.h>

 
#define PCI_DEVICE_ID_PCI230 0x0000
#define PCI_DEVICE_ID_PCI260 0x0006

 
#define PCI230_PPI_X_BASE	0x00	 
#define PCI230_PPI_X_A		0x00	 
#define PCI230_PPI_X_B		0x01	 
#define PCI230_PPI_X_C		0x02	 
#define PCI230_PPI_X_CMD	0x03	 
#define PCI230_Z2_CT_BASE	0x14	 
#define PCI230_ZCLK_SCE		0x1A	 
#define PCI230_ZGAT_SCE		0x1D	 
#define PCI230_INT_SCE		0x1E	 
#define PCI230_INT_STAT		0x1E	 

 
#define PCI230_DACCON		0x00	 
#define PCI230_DACOUT1		0x02	 
#define PCI230_DACOUT2		0x04	 
#define PCI230_ADCDATA		0x08	 
#define PCI230_ADCSWTRIG	0x08	 
#define PCI230_ADCCON		0x0A	 
#define PCI230_ADCEN		0x0C	 
#define PCI230_ADCG		0x0E	 
 
#define PCI230P_ADCTRIG		0x10	 
#define PCI230P_ADCTH		0x12	 
#define PCI230P_ADCFFTH		0x14	 
#define PCI230P_ADCFFLEV	0x16	 
#define PCI230P_ADCPTSC		0x18	 
#define PCI230P_ADCHYST		0x1A	 
#define PCI230P_EXTFUNC		0x1C	 
#define PCI230P_HWVER		0x1E	 
 
#define PCI230P2_DACDATA	0x02	 
#define PCI230P2_DACSWTRIG	0x02	 
#define PCI230P2_DACEN		0x06	 

 
#define PCI230_DAC_OR(x)		(((x) & 0x1) << 0)
#define PCI230_DAC_OR_UNI		PCI230_DAC_OR(0)  
#define PCI230_DAC_OR_BIP		PCI230_DAC_OR(1)  
#define PCI230_DAC_OR_MASK		PCI230_DAC_OR(1)
 
#define PCI230P2_DAC_FIFO_EN		BIT(8)  
 
#define PCI230P2_DAC_TRIG(x)		(((x) & 0x7) << 2)
#define PCI230P2_DAC_TRIG_NONE		PCI230P2_DAC_TRIG(0)  
#define PCI230P2_DAC_TRIG_SW		PCI230P2_DAC_TRIG(1)  
#define PCI230P2_DAC_TRIG_EXTP		PCI230P2_DAC_TRIG(2)  
#define PCI230P2_DAC_TRIG_EXTN		PCI230P2_DAC_TRIG(3)  
#define PCI230P2_DAC_TRIG_Z2CT0		PCI230P2_DAC_TRIG(4)  
#define PCI230P2_DAC_TRIG_Z2CT1		PCI230P2_DAC_TRIG(5)  
#define PCI230P2_DAC_TRIG_Z2CT2		PCI230P2_DAC_TRIG(6)  
#define PCI230P2_DAC_TRIG_MASK		PCI230P2_DAC_TRIG(7)
#define PCI230P2_DAC_FIFO_WRAP		BIT(7)  
#define PCI230P2_DAC_INT_FIFO(x)	(((x) & 7) << 9)
#define PCI230P2_DAC_INT_FIFO_EMPTY	PCI230P2_DAC_INT_FIFO(0)  
#define PCI230P2_DAC_INT_FIFO_NEMPTY	PCI230P2_DAC_INT_FIFO(1)  
#define PCI230P2_DAC_INT_FIFO_NHALF	PCI230P2_DAC_INT_FIFO(2)  
#define PCI230P2_DAC_INT_FIFO_HALF	PCI230P2_DAC_INT_FIFO(3)  
#define PCI230P2_DAC_INT_FIFO_NFULL	PCI230P2_DAC_INT_FIFO(4)  
#define PCI230P2_DAC_INT_FIFO_FULL	PCI230P2_DAC_INT_FIFO(5)  
#define PCI230P2_DAC_INT_FIFO_MASK	PCI230P2_DAC_INT_FIFO(7)

 
#define PCI230_DAC_BUSY			BIT(1)  
 
#define PCI230P2_DAC_FIFO_UNDERRUN_LATCHED	BIT(5)  
#define PCI230P2_DAC_FIFO_EMPTY		BIT(13)  
#define PCI230P2_DAC_FIFO_FULL		BIT(14)  
#define PCI230P2_DAC_FIFO_HALF		BIT(15)  

 
 
#define PCI230P2_DAC_FIFO_UNDERRUN_CLEAR	BIT(5)  
#define PCI230P2_DAC_FIFO_RESET		BIT(12)  

 
#define PCI230P2_DAC_FIFOLEVEL_HALF	512
#define PCI230P2_DAC_FIFOLEVEL_FULL	1024
 
#define PCI230P2_DAC_FIFOROOM_EMPTY		PCI230P2_DAC_FIFOLEVEL_FULL
#define PCI230P2_DAC_FIFOROOM_ONETOHALF		\
	(PCI230P2_DAC_FIFOLEVEL_FULL - PCI230P2_DAC_FIFOLEVEL_HALF)
#define PCI230P2_DAC_FIFOROOM_HALFTOFULL	1
#define PCI230P2_DAC_FIFOROOM_FULL		0

 
#define PCI230_ADC_TRIG(x)		(((x) & 0x7) << 0)
#define PCI230_ADC_TRIG_NONE		PCI230_ADC_TRIG(0)  
#define PCI230_ADC_TRIG_SW		PCI230_ADC_TRIG(1)  
#define PCI230_ADC_TRIG_EXTP		PCI230_ADC_TRIG(2)  
#define PCI230_ADC_TRIG_EXTN		PCI230_ADC_TRIG(3)  
#define PCI230_ADC_TRIG_Z2CT0		PCI230_ADC_TRIG(4)  
#define PCI230_ADC_TRIG_Z2CT1		PCI230_ADC_TRIG(5)  
#define PCI230_ADC_TRIG_Z2CT2		PCI230_ADC_TRIG(6)  
#define PCI230_ADC_TRIG_MASK		PCI230_ADC_TRIG(7)
#define PCI230_ADC_IR(x)		(((x) & 0x1) << 3)
#define PCI230_ADC_IR_UNI		PCI230_ADC_IR(0)  
#define PCI230_ADC_IR_BIP		PCI230_ADC_IR(1)  
#define PCI230_ADC_IR_MASK		PCI230_ADC_IR(1)
#define PCI230_ADC_IM(x)		(((x) & 0x1) << 4)
#define PCI230_ADC_IM_SE		PCI230_ADC_IM(0)  
#define PCI230_ADC_IM_DIF		PCI230_ADC_IM(1)  
#define PCI230_ADC_IM_MASK		PCI230_ADC_IM(1)
#define PCI230_ADC_FIFO_EN		BIT(8)  
#define PCI230_ADC_INT_FIFO(x)		(((x) & 0x7) << 9)
#define PCI230_ADC_INT_FIFO_EMPTY	PCI230_ADC_INT_FIFO(0)  
#define PCI230_ADC_INT_FIFO_NEMPTY	PCI230_ADC_INT_FIFO(1)  
#define PCI230_ADC_INT_FIFO_NHALF	PCI230_ADC_INT_FIFO(2)  
#define PCI230_ADC_INT_FIFO_HALF	PCI230_ADC_INT_FIFO(3)  
#define PCI230_ADC_INT_FIFO_NFULL	PCI230_ADC_INT_FIFO(4)  
#define PCI230_ADC_INT_FIFO_FULL	PCI230_ADC_INT_FIFO(5)  
#define PCI230P_ADC_INT_FIFO_THRESH	PCI230_ADC_INT_FIFO(7)  
#define PCI230_ADC_INT_FIFO_MASK	PCI230_ADC_INT_FIFO(7)

 
#define PCI230_ADC_FIFO_RESET		BIT(12)  
#define PCI230_ADC_GLOB_RESET		BIT(13)  

 
#define PCI230_ADC_BUSY			BIT(15)  
#define PCI230_ADC_FIFO_EMPTY		BIT(12)  
#define PCI230_ADC_FIFO_FULL		BIT(13)  
#define PCI230_ADC_FIFO_HALF		BIT(14)  
#define PCI230_ADC_FIFO_FULL_LATCHED	BIT(5)   

 
#define PCI230_ADC_FIFOLEVEL_HALFFULL	2049	 
#define PCI230_ADC_FIFOLEVEL_FULL	4096	 

 
 
#define PCI230P_EXTFUNC_GAT_EXTTRIG	BIT(0)
 
 
#define PCI230P2_EXTFUNC_DACFIFO	BIT(1)

 
#define CLK_CLK		0	 
#define CLK_10MHZ	1	 
#define CLK_1MHZ	2	 
#define CLK_100KHZ	3	 
#define CLK_10KHZ	4	 
#define CLK_1KHZ	5	 
#define CLK_OUTNM1	6	 
#define CLK_EXT		7	 

static unsigned int pci230_clk_config(unsigned int chan, unsigned int src)
{
	return ((chan & 3) << 3) | (src & 7);
}

 
#define GAT_VCC		0	 
#define GAT_GND		1	 
#define GAT_EXT		2	 
#define GAT_NOUTNM2	3	 

static unsigned int pci230_gat_config(unsigned int chan, unsigned int src)
{
	return ((chan & 3) << 3) | (src & 7);
}

 

 
#define PCI230_INT_DISABLE		0
#define PCI230_INT_PPI_C0		BIT(0)
#define PCI230_INT_PPI_C3		BIT(1)
#define PCI230_INT_ADC			BIT(2)
#define PCI230_INT_ZCLK_CT1		BIT(5)
 
#define PCI230P2_INT_DAC		BIT(4)

 
enum {
	RES_Z2CT0 = BIT(0),	 
	RES_Z2CT1 = BIT(1),	 
	RES_Z2CT2 = BIT(2)	 
};

enum {
	OWNER_AICMD,		 
	OWNER_AOCMD,		 
	NUM_OWNERS		 
};

 

 
#define COMBINE(old, new, mask)	(((old) & ~(mask)) | ((new) & (mask)))

 
#define THISCPU		smp_processor_id()

 

struct pci230_board {
	const char *name;
	unsigned short id;
	unsigned char ai_bits;
	unsigned char ao_bits;
	unsigned char min_hwver;  
	unsigned int have_dio:1;
};

static const struct pci230_board pci230_boards[] = {
	{
		.name		= "pci230+",
		.id		= PCI_DEVICE_ID_PCI230,
		.ai_bits	= 16,
		.ao_bits	= 12,
		.have_dio	= true,
		.min_hwver	= 1,
	},
	{
		.name		= "pci260+",
		.id		= PCI_DEVICE_ID_PCI260,
		.ai_bits	= 16,
		.min_hwver	= 1,
	},
	{
		.name		= "pci230",
		.id		= PCI_DEVICE_ID_PCI230,
		.ai_bits	= 12,
		.ao_bits	= 12,
		.have_dio	= true,
	},
	{
		.name		= "pci260",
		.id		= PCI_DEVICE_ID_PCI260,
		.ai_bits	= 12,
	},
};

struct pci230_private {
	spinlock_t isr_spinlock;	 
	spinlock_t res_spinlock;	 
	spinlock_t ai_stop_spinlock;	 
	spinlock_t ao_stop_spinlock;	 
	unsigned long daqio;		 
	int intr_cpuid;			 
	unsigned short hwver;		 
	unsigned short adccon;		 
	unsigned short daccon;		 
	unsigned short adcfifothresh;	 
	unsigned short adcg;		 
	unsigned char ier;		 
	unsigned char res_owned[NUM_OWNERS];  
	unsigned int intr_running:1;	 
	unsigned int ai_bipolar:1;	 
	unsigned int ao_bipolar:1;	 
	unsigned int ai_cmd_started:1;	 
	unsigned int ao_cmd_started:1;	 
};

 
static const unsigned int pci230_timebase[8] = {
	[CLK_10MHZ]	= I8254_OSC_BASE_10MHZ,
	[CLK_1MHZ]	= I8254_OSC_BASE_1MHZ,
	[CLK_100KHZ]	= I8254_OSC_BASE_100KHZ,
	[CLK_10KHZ]	= I8254_OSC_BASE_10KHZ,
	[CLK_1KHZ]	= I8254_OSC_BASE_1KHZ,
};

 
static const struct comedi_lrange pci230_ai_range = {
	7, {
		BIP_RANGE(10),
		BIP_RANGE(5),
		BIP_RANGE(2.5),
		BIP_RANGE(1.25),
		UNI_RANGE(10),
		UNI_RANGE(5),
		UNI_RANGE(2.5)
	}
};

 
static const unsigned char pci230_ai_gain[7] = { 0, 1, 2, 3, 1, 2, 3 };

 
static const struct comedi_lrange pci230_ao_range = {
	2, {
		UNI_RANGE(10),
		BIP_RANGE(10)
	}
};

static unsigned short pci230_ai_read(struct comedi_device *dev)
{
	const struct pci230_board *board = dev->board_ptr;
	struct pci230_private *devpriv = dev->private;
	unsigned short data;

	 
	data = inw(devpriv->daqio + PCI230_ADCDATA);
	 
	if (devpriv->ai_bipolar)
		data ^= 0x8000;
	data >>= (16 - board->ai_bits);
	return data;
}

static unsigned short pci230_ao_mangle_datum(struct comedi_device *dev,
					     unsigned short datum)
{
	const struct pci230_board *board = dev->board_ptr;
	struct pci230_private *devpriv = dev->private;

	 
	datum <<= (16 - board->ao_bits);
	 
	if (devpriv->ao_bipolar)
		datum ^= 0x8000;
	return datum;
}

static void pci230_ao_write_nofifo(struct comedi_device *dev,
				   unsigned short datum, unsigned int chan)
{
	struct pci230_private *devpriv = dev->private;

	 
	outw(pci230_ao_mangle_datum(dev, datum),
	     devpriv->daqio + ((chan == 0) ? PCI230_DACOUT1 : PCI230_DACOUT2));
}

static void pci230_ao_write_fifo(struct comedi_device *dev,
				 unsigned short datum, unsigned int chan)
{
	struct pci230_private *devpriv = dev->private;

	 
	outw(pci230_ao_mangle_datum(dev, datum),
	     devpriv->daqio + PCI230P2_DACDATA);
}

static bool pci230_claim_shared(struct comedi_device *dev,
				unsigned char res_mask, unsigned int owner)
{
	struct pci230_private *devpriv = dev->private;
	unsigned int o;
	unsigned long irqflags;

	spin_lock_irqsave(&devpriv->res_spinlock, irqflags);
	for (o = 0; o < NUM_OWNERS; o++) {
		if (o == owner)
			continue;
		if (devpriv->res_owned[o] & res_mask) {
			spin_unlock_irqrestore(&devpriv->res_spinlock,
					       irqflags);
			return false;
		}
	}
	devpriv->res_owned[owner] |= res_mask;
	spin_unlock_irqrestore(&devpriv->res_spinlock, irqflags);
	return true;
}

static void pci230_release_shared(struct comedi_device *dev,
				  unsigned char res_mask, unsigned int owner)
{
	struct pci230_private *devpriv = dev->private;
	unsigned long irqflags;

	spin_lock_irqsave(&devpriv->res_spinlock, irqflags);
	devpriv->res_owned[owner] &= ~res_mask;
	spin_unlock_irqrestore(&devpriv->res_spinlock, irqflags);
}

static void pci230_release_all_resources(struct comedi_device *dev,
					 unsigned int owner)
{
	pci230_release_shared(dev, (unsigned char)~0, owner);
}

static unsigned int pci230_divide_ns(u64 ns, unsigned int timebase,
				     unsigned int flags)
{
	u64 div;
	unsigned int rem;

	div = ns;
	rem = do_div(div, timebase);
	switch (flags & CMDF_ROUND_MASK) {
	default:
	case CMDF_ROUND_NEAREST:
		div += DIV_ROUND_CLOSEST(rem, timebase);
		break;
	case CMDF_ROUND_DOWN:
		break;
	case CMDF_ROUND_UP:
		div += DIV_ROUND_UP(rem, timebase);
		break;
	}
	return div > UINT_MAX ? UINT_MAX : (unsigned int)div;
}

 
static unsigned int pci230_choose_clk_count(u64 ns, unsigned int *count,
					    unsigned int flags)
{
	unsigned int clk_src, cnt;

	for (clk_src = CLK_10MHZ;; clk_src++) {
		cnt = pci230_divide_ns(ns, pci230_timebase[clk_src], flags);
		if (cnt <= 65536 || clk_src == CLK_1KHZ)
			break;
	}
	*count = cnt;
	return clk_src;
}

static void pci230_ns_to_single_timer(unsigned int *ns, unsigned int flags)
{
	unsigned int count;
	unsigned int clk_src;

	clk_src = pci230_choose_clk_count(*ns, &count, flags);
	*ns = count * pci230_timebase[clk_src];
}

static void pci230_ct_setup_ns_mode(struct comedi_device *dev, unsigned int ct,
				    unsigned int mode, u64 ns,
				    unsigned int flags)
{
	unsigned int clk_src;
	unsigned int count;

	 
	comedi_8254_set_mode(dev->pacer, ct, mode);
	 
	clk_src = pci230_choose_clk_count(ns, &count, flags);
	 
	outb(pci230_clk_config(ct, clk_src), dev->iobase + PCI230_ZCLK_SCE);
	 
	if (count >= 65536)
		count = 0;

	comedi_8254_write(dev->pacer, ct, count);
}

static void pci230_cancel_ct(struct comedi_device *dev, unsigned int ct)
{
	 
	comedi_8254_set_mode(dev->pacer, ct, I8254_MODE1);
}

static int pci230_ai_eoc(struct comedi_device *dev,
			 struct comedi_subdevice *s,
			 struct comedi_insn *insn,
			 unsigned long context)
{
	struct pci230_private *devpriv = dev->private;
	unsigned int status;

	status = inw(devpriv->daqio + PCI230_ADCCON);
	if ((status & PCI230_ADC_FIFO_EMPTY) == 0)
		return 0;
	return -EBUSY;
}

static int pci230_ai_insn_read(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	struct pci230_private *devpriv = dev->private;
	unsigned int n;
	unsigned int chan, range, aref;
	unsigned int gainshift;
	unsigned short adccon, adcen;
	int ret;

	 
	chan = CR_CHAN(insn->chanspec);
	range = CR_RANGE(insn->chanspec);
	aref = CR_AREF(insn->chanspec);
	if (aref == AREF_DIFF) {
		 
		if (chan >= s->n_chan / 2) {
			dev_dbg(dev->class_dev,
				"%s: differential channel number out of range 0 to %u\n",
				__func__, (s->n_chan / 2) - 1);
			return -EINVAL;
		}
	}

	 
	adccon = PCI230_ADC_TRIG_Z2CT2 | PCI230_ADC_FIFO_EN;
	 
	comedi_8254_set_mode(dev->pacer, 2, I8254_MODE0);
	devpriv->ai_bipolar = comedi_range_is_bipolar(s, range);
	if (aref == AREF_DIFF) {
		 
		gainshift = chan * 2;
		if (devpriv->hwver == 0) {
			 
			adcen = 3 << gainshift;
		} else {
			 
			adcen = 1 << gainshift;
		}
		adccon |= PCI230_ADC_IM_DIF;
	} else {
		 
		adcen = 1 << chan;
		gainshift = chan & ~1;
		adccon |= PCI230_ADC_IM_SE;
	}
	devpriv->adcg = (devpriv->adcg & ~(3 << gainshift)) |
			(pci230_ai_gain[range] << gainshift);
	if (devpriv->ai_bipolar)
		adccon |= PCI230_ADC_IR_BIP;
	else
		adccon |= PCI230_ADC_IR_UNI;

	 
	outw(adcen, devpriv->daqio + PCI230_ADCEN);

	 
	outw(devpriv->adcg, devpriv->daqio + PCI230_ADCG);

	 
	devpriv->adccon = adccon;
	outw(adccon | PCI230_ADC_FIFO_RESET, devpriv->daqio + PCI230_ADCCON);

	 
	for (n = 0; n < insn->n; n++) {
		 
		comedi_8254_set_mode(dev->pacer, 2, I8254_MODE0);
		comedi_8254_set_mode(dev->pacer, 2, I8254_MODE1);

		 
		ret = comedi_timeout(dev, s, insn, pci230_ai_eoc, 0);
		if (ret)
			return ret;

		 
		data[n] = pci230_ai_read(dev);
	}

	 
	return n;
}

static int pci230_ao_insn_write(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn,
				unsigned int *data)
{
	struct pci230_private *devpriv = dev->private;
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int range = CR_RANGE(insn->chanspec);
	unsigned int val = s->readback[chan];
	int i;

	 
	devpriv->ao_bipolar = comedi_range_is_bipolar(s, range);
	outw(range, devpriv->daqio + PCI230_DACCON);

	for (i = 0; i < insn->n; i++) {
		val = data[i];
		pci230_ao_write_nofifo(dev, val, chan);
	}
	s->readback[chan] = val;

	return insn->n;
}

static int pci230_ao_check_chanlist(struct comedi_device *dev,
				    struct comedi_subdevice *s,
				    struct comedi_cmd *cmd)
{
	unsigned int prev_chan = CR_CHAN(cmd->chanlist[0]);
	unsigned int range0 = CR_RANGE(cmd->chanlist[0]);
	int i;

	for (i = 1; i < cmd->chanlist_len; i++) {
		unsigned int chan = CR_CHAN(cmd->chanlist[i]);
		unsigned int range = CR_RANGE(cmd->chanlist[i]);

		if (chan < prev_chan) {
			dev_dbg(dev->class_dev,
				"%s: channel numbers must increase\n",
				__func__);
			return -EINVAL;
		}

		if (range != range0) {
			dev_dbg(dev->class_dev,
				"%s: channels must have the same range\n",
				__func__);
			return -EINVAL;
		}

		prev_chan = chan;
	}

	return 0;
}

static int pci230_ao_cmdtest(struct comedi_device *dev,
			     struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	const struct pci230_board *board = dev->board_ptr;
	struct pci230_private *devpriv = dev->private;
	int err = 0;
	unsigned int tmp;

	 

	err |= comedi_check_trigger_src(&cmd->start_src, TRIG_INT);

	tmp = TRIG_TIMER | TRIG_INT;
	if (board->min_hwver > 0 && devpriv->hwver >= 2) {
		 
		tmp |= TRIG_EXT;
	}
	err |= comedi_check_trigger_src(&cmd->scan_begin_src, tmp);

	err |= comedi_check_trigger_src(&cmd->convert_src, TRIG_NOW);
	err |= comedi_check_trigger_src(&cmd->scan_end_src, TRIG_COUNT);
	err |= comedi_check_trigger_src(&cmd->stop_src, TRIG_COUNT | TRIG_NONE);

	if (err)
		return 1;

	 

	err |= comedi_check_trigger_is_unique(cmd->scan_begin_src);
	err |= comedi_check_trigger_is_unique(cmd->stop_src);

	 

	if (err)
		return 2;

	 

	err |= comedi_check_trigger_arg_is(&cmd->start_arg, 0);

#define MAX_SPEED_AO	8000	 
 
#define MIN_SPEED_AO	4294967295u	 

	switch (cmd->scan_begin_src) {
	case TRIG_TIMER:
		err |= comedi_check_trigger_arg_min(&cmd->scan_begin_arg,
						    MAX_SPEED_AO);
		err |= comedi_check_trigger_arg_max(&cmd->scan_begin_arg,
						    MIN_SPEED_AO);
		break;
	case TRIG_EXT:
		 
		 
		if (cmd->scan_begin_arg & ~CR_FLAGS_MASK) {
			cmd->scan_begin_arg = COMBINE(cmd->scan_begin_arg, 0,
						      ~CR_FLAGS_MASK);
			err |= -EINVAL;
		}
		 
		if (cmd->scan_begin_arg & CR_FLAGS_MASK &
		    ~(CR_EDGE | CR_INVERT)) {
			cmd->scan_begin_arg =
			    COMBINE(cmd->scan_begin_arg, 0,
				    CR_FLAGS_MASK & ~(CR_EDGE | CR_INVERT));
			err |= -EINVAL;
		}
		break;
	default:
		err |= comedi_check_trigger_arg_is(&cmd->scan_begin_arg, 0);
		break;
	}

	err |= comedi_check_trigger_arg_is(&cmd->scan_end_arg,
					   cmd->chanlist_len);

	if (cmd->stop_src == TRIG_COUNT)
		err |= comedi_check_trigger_arg_min(&cmd->stop_arg, 1);
	else	 
		err |= comedi_check_trigger_arg_is(&cmd->stop_arg, 0);

	if (err)
		return 3;

	 

	if (cmd->scan_begin_src == TRIG_TIMER) {
		tmp = cmd->scan_begin_arg;
		pci230_ns_to_single_timer(&cmd->scan_begin_arg, cmd->flags);
		if (tmp != cmd->scan_begin_arg)
			err++;
	}

	if (err)
		return 4;

	 
	if (cmd->chanlist && cmd->chanlist_len > 0)
		err |= pci230_ao_check_chanlist(dev, s, cmd);

	if (err)
		return 5;

	return 0;
}

static void pci230_ao_stop(struct comedi_device *dev,
			   struct comedi_subdevice *s)
{
	struct pci230_private *devpriv = dev->private;
	unsigned long irqflags;
	unsigned char intsrc;
	bool started;
	struct comedi_cmd *cmd;

	spin_lock_irqsave(&devpriv->ao_stop_spinlock, irqflags);
	started = devpriv->ao_cmd_started;
	devpriv->ao_cmd_started = false;
	spin_unlock_irqrestore(&devpriv->ao_stop_spinlock, irqflags);
	if (!started)
		return;
	cmd = &s->async->cmd;
	if (cmd->scan_begin_src == TRIG_TIMER) {
		 
		pci230_cancel_ct(dev, 1);
	}
	 
	if (devpriv->hwver < 2) {
		 
		intsrc = PCI230_INT_ZCLK_CT1;
	} else {
		 
		intsrc = PCI230P2_INT_DAC;
	}
	 
	spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
	devpriv->ier &= ~intsrc;
	while (devpriv->intr_running && devpriv->intr_cpuid != THISCPU) {
		spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);
		spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
	}
	outb(devpriv->ier, dev->iobase + PCI230_INT_SCE);
	spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);
	if (devpriv->hwver >= 2) {
		 
		devpriv->daccon &= PCI230_DAC_OR_MASK;
		outw(devpriv->daccon | PCI230P2_DAC_FIFO_RESET |
		     PCI230P2_DAC_FIFO_UNDERRUN_CLEAR,
		     devpriv->daqio + PCI230_DACCON);
	}
	 
	pci230_release_all_resources(dev, OWNER_AOCMD);
}

static void pci230_handle_ao_nofifo(struct comedi_device *dev,
				    struct comedi_subdevice *s)
{
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	unsigned short data;
	int i;

	if (cmd->stop_src == TRIG_COUNT && async->scans_done >= cmd->stop_arg)
		return;

	for (i = 0; i < cmd->chanlist_len; i++) {
		unsigned int chan = CR_CHAN(cmd->chanlist[i]);

		if (!comedi_buf_read_samples(s, &data, 1)) {
			async->events |= COMEDI_CB_OVERFLOW;
			return;
		}
		pci230_ao_write_nofifo(dev, data, chan);
		s->readback[chan] = data;
	}

	if (cmd->stop_src == TRIG_COUNT && async->scans_done >= cmd->stop_arg)
		async->events |= COMEDI_CB_EOA;
}

 
static bool pci230_handle_ao_fifo(struct comedi_device *dev,
				  struct comedi_subdevice *s)
{
	struct pci230_private *devpriv = dev->private;
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	unsigned int num_scans = comedi_nscans_left(s, 0);
	unsigned int room;
	unsigned short dacstat;
	unsigned int i, n;
	unsigned int events = 0;

	 
	dacstat = inw(devpriv->daqio + PCI230_DACCON);

	if (cmd->stop_src == TRIG_COUNT && num_scans == 0)
		events |= COMEDI_CB_EOA;

	if (events == 0) {
		 
		if (dacstat & PCI230P2_DAC_FIFO_UNDERRUN_LATCHED) {
			dev_err(dev->class_dev, "AO FIFO underrun\n");
			events |= COMEDI_CB_OVERFLOW | COMEDI_CB_ERROR;
		}
		 
		if (num_scans == 0 &&
		    (dacstat & PCI230P2_DAC_FIFO_HALF) == 0) {
			dev_err(dev->class_dev, "AO buffer underrun\n");
			events |= COMEDI_CB_OVERFLOW | COMEDI_CB_ERROR;
		}
	}
	if (events == 0) {
		 
		if (dacstat & PCI230P2_DAC_FIFO_FULL)
			room = PCI230P2_DAC_FIFOROOM_FULL;
		else if (dacstat & PCI230P2_DAC_FIFO_HALF)
			room = PCI230P2_DAC_FIFOROOM_HALFTOFULL;
		else if (dacstat & PCI230P2_DAC_FIFO_EMPTY)
			room = PCI230P2_DAC_FIFOROOM_EMPTY;
		else
			room = PCI230P2_DAC_FIFOROOM_ONETOHALF;
		 
		room /= cmd->chanlist_len;
		 
		if (num_scans > room)
			num_scans = room;
		 
		for (n = 0; n < num_scans; n++) {
			for (i = 0; i < cmd->chanlist_len; i++) {
				unsigned int chan = CR_CHAN(cmd->chanlist[i]);
				unsigned short datum;

				comedi_buf_read_samples(s, &datum, 1);
				pci230_ao_write_fifo(dev, datum, chan);
				s->readback[chan] = datum;
			}
		}

		if (cmd->stop_src == TRIG_COUNT &&
		    async->scans_done >= cmd->stop_arg) {
			 
			devpriv->daccon &= ~PCI230P2_DAC_INT_FIFO_MASK;
			devpriv->daccon |= PCI230P2_DAC_INT_FIFO_EMPTY;
			outw(devpriv->daccon, devpriv->daqio + PCI230_DACCON);
		}
		 
		dacstat = inw(devpriv->daqio + PCI230_DACCON);
		if (dacstat & PCI230P2_DAC_FIFO_UNDERRUN_LATCHED) {
			dev_err(dev->class_dev, "AO FIFO underrun\n");
			events |= COMEDI_CB_OVERFLOW | COMEDI_CB_ERROR;
		}
	}
	async->events |= events;
	return !(async->events & COMEDI_CB_CANCEL_MASK);
}

static int pci230_ao_inttrig_scan_begin(struct comedi_device *dev,
					struct comedi_subdevice *s,
					unsigned int trig_num)
{
	struct pci230_private *devpriv = dev->private;
	unsigned long irqflags;

	if (trig_num)
		return -EINVAL;

	spin_lock_irqsave(&devpriv->ao_stop_spinlock, irqflags);
	if (!devpriv->ao_cmd_started) {
		spin_unlock_irqrestore(&devpriv->ao_stop_spinlock, irqflags);
		return 1;
	}
	 
	if (devpriv->hwver < 2) {
		 
		spin_unlock_irqrestore(&devpriv->ao_stop_spinlock, irqflags);
		pci230_handle_ao_nofifo(dev, s);
		comedi_handle_events(dev, s);
	} else {
		 
		 
		inw(devpriv->daqio + PCI230P2_DACSWTRIG);
		spin_unlock_irqrestore(&devpriv->ao_stop_spinlock, irqflags);
	}
	 
	 
	udelay(8);
	return 1;
}

static void pci230_ao_start(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	struct pci230_private *devpriv = dev->private;
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	unsigned long irqflags;

	devpriv->ao_cmd_started = true;

	if (devpriv->hwver >= 2) {
		 
		unsigned short scantrig;
		bool run;

		 
		run = pci230_handle_ao_fifo(dev, s);
		comedi_handle_events(dev, s);
		if (!run) {
			 
			return;
		}
		 
		switch (cmd->scan_begin_src) {
		case TRIG_TIMER:
			scantrig = PCI230P2_DAC_TRIG_Z2CT1;
			break;
		case TRIG_EXT:
			 
			if ((cmd->scan_begin_arg & CR_INVERT) == 0) {
				 
				scantrig = PCI230P2_DAC_TRIG_EXTP;
			} else {
				 
				scantrig = PCI230P2_DAC_TRIG_EXTN;
			}
			break;
		case TRIG_INT:
			scantrig = PCI230P2_DAC_TRIG_SW;
			break;
		default:
			 
			scantrig = PCI230P2_DAC_TRIG_NONE;
			break;
		}
		devpriv->daccon =
		    (devpriv->daccon & ~PCI230P2_DAC_TRIG_MASK) | scantrig;
		outw(devpriv->daccon, devpriv->daqio + PCI230_DACCON);
	}
	switch (cmd->scan_begin_src) {
	case TRIG_TIMER:
		if (devpriv->hwver < 2) {
			 
			 
			spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
			devpriv->ier |= PCI230_INT_ZCLK_CT1;
			outb(devpriv->ier, dev->iobase + PCI230_INT_SCE);
			spin_unlock_irqrestore(&devpriv->isr_spinlock,
					       irqflags);
		}
		 
		outb(pci230_gat_config(1, GAT_VCC),
		     dev->iobase + PCI230_ZGAT_SCE);
		break;
	case TRIG_INT:
		async->inttrig = pci230_ao_inttrig_scan_begin;
		break;
	}
	if (devpriv->hwver >= 2) {
		 
		spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
		devpriv->ier |= PCI230P2_INT_DAC;
		outb(devpriv->ier, dev->iobase + PCI230_INT_SCE);
		spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);
	}
}

static int pci230_ao_inttrig_start(struct comedi_device *dev,
				   struct comedi_subdevice *s,
				   unsigned int trig_num)
{
	struct comedi_cmd *cmd = &s->async->cmd;

	if (trig_num != cmd->start_src)
		return -EINVAL;

	s->async->inttrig = NULL;
	pci230_ao_start(dev, s);

	return 1;
}

static int pci230_ao_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct pci230_private *devpriv = dev->private;
	unsigned short daccon;
	unsigned int range;

	 
	struct comedi_cmd *cmd = &s->async->cmd;

	if (cmd->scan_begin_src == TRIG_TIMER) {
		 
		if (!pci230_claim_shared(dev, RES_Z2CT1, OWNER_AOCMD))
			return -EBUSY;
	}

	 
	range = CR_RANGE(cmd->chanlist[0]);
	devpriv->ao_bipolar = comedi_range_is_bipolar(s, range);
	daccon = devpriv->ao_bipolar ? PCI230_DAC_OR_BIP : PCI230_DAC_OR_UNI;
	 
	if (devpriv->hwver >= 2) {
		unsigned short dacen;
		unsigned int i;

		dacen = 0;
		for (i = 0; i < cmd->chanlist_len; i++)
			dacen |= 1 << CR_CHAN(cmd->chanlist[i]);

		 
		outw(dacen, devpriv->daqio + PCI230P2_DACEN);
		 
		daccon |= PCI230P2_DAC_FIFO_EN | PCI230P2_DAC_FIFO_RESET |
			  PCI230P2_DAC_FIFO_UNDERRUN_CLEAR |
			  PCI230P2_DAC_TRIG_NONE | PCI230P2_DAC_INT_FIFO_NHALF;
	}

	 
	outw(daccon, devpriv->daqio + PCI230_DACCON);
	 
	devpriv->daccon = daccon & ~(PCI230P2_DAC_FIFO_RESET |
				     PCI230P2_DAC_FIFO_UNDERRUN_CLEAR);

	if (cmd->scan_begin_src == TRIG_TIMER) {
		 
		outb(pci230_gat_config(1, GAT_GND),
		     dev->iobase + PCI230_ZGAT_SCE);
		pci230_ct_setup_ns_mode(dev, 1, I8254_MODE3,
					cmd->scan_begin_arg,
					cmd->flags);
	}

	 
	s->async->inttrig = pci230_ao_inttrig_start;

	return 0;
}

static int pci230_ao_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	pci230_ao_stop(dev, s);
	return 0;
}

static int pci230_ai_check_scan_period(struct comedi_cmd *cmd)
{
	unsigned int min_scan_period, chanlist_len;
	int err = 0;

	chanlist_len = cmd->chanlist_len;
	if (cmd->chanlist_len == 0)
		chanlist_len = 1;

	min_scan_period = chanlist_len * cmd->convert_arg;
	if (min_scan_period < chanlist_len ||
	    min_scan_period < cmd->convert_arg) {
		 
		min_scan_period = UINT_MAX;
		err++;
	}
	if (cmd->scan_begin_arg < min_scan_period) {
		cmd->scan_begin_arg = min_scan_period;
		err++;
	}

	return !err;
}

static int pci230_ai_check_chanlist(struct comedi_device *dev,
				    struct comedi_subdevice *s,
				    struct comedi_cmd *cmd)
{
	struct pci230_private *devpriv = dev->private;
	unsigned int max_diff_chan = (s->n_chan / 2) - 1;
	unsigned int prev_chan = 0;
	unsigned int prev_range = 0;
	unsigned int prev_aref = 0;
	bool prev_bipolar = false;
	unsigned int subseq_len = 0;
	int i;

	for (i = 0; i < cmd->chanlist_len; i++) {
		unsigned int chanspec = cmd->chanlist[i];
		unsigned int chan = CR_CHAN(chanspec);
		unsigned int range = CR_RANGE(chanspec);
		unsigned int aref = CR_AREF(chanspec);
		bool bipolar = comedi_range_is_bipolar(s, range);

		if (aref == AREF_DIFF && chan >= max_diff_chan) {
			dev_dbg(dev->class_dev,
				"%s: differential channel number out of range 0 to %u\n",
				__func__, max_diff_chan);
			return -EINVAL;
		}

		if (i > 0) {
			 
			if (chan <= prev_chan && subseq_len == 0)
				subseq_len = i;

			if (subseq_len > 0 &&
			    cmd->chanlist[i % subseq_len] != chanspec) {
				dev_dbg(dev->class_dev,
					"%s: channel numbers must increase or sequence must repeat exactly\n",
					__func__);
				return -EINVAL;
			}

			if (aref != prev_aref) {
				dev_dbg(dev->class_dev,
					"%s: channel sequence analogue references must be all the same (single-ended or differential)\n",
					__func__);
				return -EINVAL;
			}

			if (bipolar != prev_bipolar) {
				dev_dbg(dev->class_dev,
					"%s: channel sequence ranges must be all bipolar or all unipolar\n",
					__func__);
				return -EINVAL;
			}

			if (aref != AREF_DIFF && range != prev_range &&
			    ((chan ^ prev_chan) & ~1) == 0) {
				dev_dbg(dev->class_dev,
					"%s: single-ended channel pairs must have the same range\n",
					__func__);
				return -EINVAL;
			}
		}
		prev_chan = chan;
		prev_range = range;
		prev_aref = aref;
		prev_bipolar = bipolar;
	}

	if (subseq_len == 0)
		subseq_len = cmd->chanlist_len;

	if (cmd->chanlist_len % subseq_len) {
		dev_dbg(dev->class_dev,
			"%s: sequence must repeat exactly\n", __func__);
		return -EINVAL;
	}

	 
	if (devpriv->hwver > 0 && devpriv->hwver < 4) {
		if (subseq_len > 1 && CR_CHAN(cmd->chanlist[0])) {
			dev_info(dev->class_dev,
				 "amplc_pci230: ai_cmdtest: Buggy PCI230+/260+ h/w version %u requires first channel of multi-channel sequence to be 0 (corrected in h/w version 4)\n",
				 devpriv->hwver);
			return -EINVAL;
		}
	}

	return 0;
}

static int pci230_ai_cmdtest(struct comedi_device *dev,
			     struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	const struct pci230_board *board = dev->board_ptr;
	struct pci230_private *devpriv = dev->private;
	int err = 0;
	unsigned int tmp;

	 

	err |= comedi_check_trigger_src(&cmd->start_src, TRIG_NOW | TRIG_INT);

	tmp = TRIG_FOLLOW | TRIG_TIMER | TRIG_INT;
	if (board->have_dio || board->min_hwver > 0) {
		 
		tmp |= TRIG_EXT;
	}
	err |= comedi_check_trigger_src(&cmd->scan_begin_src, tmp);
	err |= comedi_check_trigger_src(&cmd->convert_src,
					TRIG_TIMER | TRIG_INT | TRIG_EXT);
	err |= comedi_check_trigger_src(&cmd->scan_end_src, TRIG_COUNT);
	err |= comedi_check_trigger_src(&cmd->stop_src, TRIG_COUNT | TRIG_NONE);

	if (err)
		return 1;

	 

	err |= comedi_check_trigger_is_unique(cmd->start_src);
	err |= comedi_check_trigger_is_unique(cmd->scan_begin_src);
	err |= comedi_check_trigger_is_unique(cmd->convert_src);
	err |= comedi_check_trigger_is_unique(cmd->stop_src);

	 

	 
	if (cmd->scan_begin_src != TRIG_FOLLOW &&
	    cmd->convert_src != TRIG_TIMER)
		err |= -EINVAL;

	if (err)
		return 2;

	 

	err |= comedi_check_trigger_arg_is(&cmd->start_arg, 0);

#define MAX_SPEED_AI_SE		3200	 
#define MAX_SPEED_AI_DIFF	8000	 
#define MAX_SPEED_AI_PLUS	4000	 
 
#define MIN_SPEED_AI	4294967295u	 

	if (cmd->convert_src == TRIG_TIMER) {
		unsigned int max_speed_ai;

		if (devpriv->hwver == 0) {
			 
			if (cmd->chanlist && cmd->chanlist_len > 0) {
				 
				if (CR_AREF(cmd->chanlist[0]) == AREF_DIFF)
					max_speed_ai = MAX_SPEED_AI_DIFF;
				else
					max_speed_ai = MAX_SPEED_AI_SE;

			} else {
				 
				max_speed_ai = MAX_SPEED_AI_SE;
			}
		} else {
			 
			max_speed_ai = MAX_SPEED_AI_PLUS;
		}

		err |= comedi_check_trigger_arg_min(&cmd->convert_arg,
						    max_speed_ai);
		err |= comedi_check_trigger_arg_max(&cmd->convert_arg,
						    MIN_SPEED_AI);
	} else if (cmd->convert_src == TRIG_EXT) {
		 
		if (cmd->convert_arg & CR_FLAGS_MASK) {
			 
			if (cmd->convert_arg & ~CR_FLAGS_MASK) {
				cmd->convert_arg = COMBINE(cmd->convert_arg, 0,
							   ~CR_FLAGS_MASK);
				err |= -EINVAL;
			}
			 
			if ((cmd->convert_arg & CR_FLAGS_MASK & ~CR_INVERT) !=
			    CR_EDGE) {
				 
				cmd->convert_arg =
				    COMBINE(cmd->start_arg, CR_EDGE | 0,
					    CR_FLAGS_MASK & ~CR_INVERT);
				err |= -EINVAL;
			}
		} else {
			 
			err |= comedi_check_trigger_arg_max(&cmd->convert_arg,
							    1);
		}
	} else {
		err |= comedi_check_trigger_arg_is(&cmd->convert_arg, 0);
	}

	err |= comedi_check_trigger_arg_is(&cmd->scan_end_arg,
					   cmd->chanlist_len);

	if (cmd->stop_src == TRIG_COUNT)
		err |= comedi_check_trigger_arg_min(&cmd->stop_arg, 1);
	else	 
		err |= comedi_check_trigger_arg_is(&cmd->stop_arg, 0);

	if (cmd->scan_begin_src == TRIG_EXT) {
		 
		if (cmd->scan_begin_arg & ~CR_FLAGS_MASK) {
			cmd->scan_begin_arg = COMBINE(cmd->scan_begin_arg, 0,
						      ~CR_FLAGS_MASK);
			err |= -EINVAL;
		}
		 
		if (cmd->scan_begin_arg & CR_FLAGS_MASK & ~CR_EDGE) {
			cmd->scan_begin_arg = COMBINE(cmd->scan_begin_arg, 0,
						      CR_FLAGS_MASK & ~CR_EDGE);
			err |= -EINVAL;
		}
	} else if (cmd->scan_begin_src == TRIG_TIMER) {
		 
		if (!pci230_ai_check_scan_period(cmd))
			err |= -EINVAL;

	} else {
		err |= comedi_check_trigger_arg_is(&cmd->scan_begin_arg, 0);
	}

	if (err)
		return 3;

	 

	if (cmd->convert_src == TRIG_TIMER) {
		tmp = cmd->convert_arg;
		pci230_ns_to_single_timer(&cmd->convert_arg, cmd->flags);
		if (tmp != cmd->convert_arg)
			err++;
	}

	if (cmd->scan_begin_src == TRIG_TIMER) {
		 
		tmp = cmd->scan_begin_arg;
		pci230_ns_to_single_timer(&cmd->scan_begin_arg, cmd->flags);
		if (!pci230_ai_check_scan_period(cmd)) {
			 
			pci230_ns_to_single_timer(&cmd->scan_begin_arg,
						  CMDF_ROUND_UP);
			pci230_ai_check_scan_period(cmd);
		}
		if (tmp != cmd->scan_begin_arg)
			err++;
	}

	if (err)
		return 4;

	 
	if (cmd->chanlist && cmd->chanlist_len > 0)
		err |= pci230_ai_check_chanlist(dev, s, cmd);

	if (err)
		return 5;

	return 0;
}

static void pci230_ai_update_fifo_trigger_level(struct comedi_device *dev,
						struct comedi_subdevice *s)
{
	struct pci230_private *devpriv = dev->private;
	struct comedi_cmd *cmd = &s->async->cmd;
	unsigned int wake;
	unsigned short triglev;
	unsigned short adccon;

	if (cmd->flags & CMDF_WAKE_EOS)
		wake = cmd->scan_end_arg - s->async->cur_chan;
	else
		wake = comedi_nsamples_left(s, PCI230_ADC_FIFOLEVEL_HALFFULL);

	if (wake >= PCI230_ADC_FIFOLEVEL_HALFFULL) {
		triglev = PCI230_ADC_INT_FIFO_HALF;
	} else if (wake > 1 && devpriv->hwver > 0) {
		 
		if (devpriv->adcfifothresh != wake) {
			devpriv->adcfifothresh = wake;
			outw(wake, devpriv->daqio + PCI230P_ADCFFTH);
		}
		triglev = PCI230P_ADC_INT_FIFO_THRESH;
	} else {
		triglev = PCI230_ADC_INT_FIFO_NEMPTY;
	}
	adccon = (devpriv->adccon & ~PCI230_ADC_INT_FIFO_MASK) | triglev;
	if (adccon != devpriv->adccon) {
		devpriv->adccon = adccon;
		outw(adccon, devpriv->daqio + PCI230_ADCCON);
	}
}

static int pci230_ai_inttrig_convert(struct comedi_device *dev,
				     struct comedi_subdevice *s,
				     unsigned int trig_num)
{
	struct pci230_private *devpriv = dev->private;
	unsigned long irqflags;
	unsigned int delayus;

	if (trig_num)
		return -EINVAL;

	spin_lock_irqsave(&devpriv->ai_stop_spinlock, irqflags);
	if (!devpriv->ai_cmd_started) {
		spin_unlock_irqrestore(&devpriv->ai_stop_spinlock, irqflags);
		return 1;
	}
	 
	comedi_8254_set_mode(dev->pacer, 2, I8254_MODE0);
	comedi_8254_set_mode(dev->pacer, 2, I8254_MODE1);
	 
	if ((devpriv->adccon & PCI230_ADC_IM_MASK) == PCI230_ADC_IM_DIF &&
	    devpriv->hwver == 0) {
		 
		delayus = 8;
	} else {
		 
		delayus = 4;
	}
	spin_unlock_irqrestore(&devpriv->ai_stop_spinlock, irqflags);
	udelay(delayus);
	return 1;
}

static int pci230_ai_inttrig_scan_begin(struct comedi_device *dev,
					struct comedi_subdevice *s,
					unsigned int trig_num)
{
	struct pci230_private *devpriv = dev->private;
	unsigned long irqflags;
	unsigned char zgat;

	if (trig_num)
		return -EINVAL;

	spin_lock_irqsave(&devpriv->ai_stop_spinlock, irqflags);
	if (devpriv->ai_cmd_started) {
		 
		zgat = pci230_gat_config(0, GAT_GND);
		outb(zgat, dev->iobase + PCI230_ZGAT_SCE);
		zgat = pci230_gat_config(0, GAT_VCC);
		outb(zgat, dev->iobase + PCI230_ZGAT_SCE);
	}
	spin_unlock_irqrestore(&devpriv->ai_stop_spinlock, irqflags);

	return 1;
}

static void pci230_ai_stop(struct comedi_device *dev,
			   struct comedi_subdevice *s)
{
	struct pci230_private *devpriv = dev->private;
	unsigned long irqflags;
	struct comedi_cmd *cmd;
	bool started;

	spin_lock_irqsave(&devpriv->ai_stop_spinlock, irqflags);
	started = devpriv->ai_cmd_started;
	devpriv->ai_cmd_started = false;
	spin_unlock_irqrestore(&devpriv->ai_stop_spinlock, irqflags);
	if (!started)
		return;
	cmd = &s->async->cmd;
	if (cmd->convert_src == TRIG_TIMER) {
		 
		pci230_cancel_ct(dev, 2);
	}
	if (cmd->scan_begin_src != TRIG_FOLLOW) {
		 
		pci230_cancel_ct(dev, 0);
	}
	spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
	 
	devpriv->ier &= ~PCI230_INT_ADC;
	while (devpriv->intr_running && devpriv->intr_cpuid != THISCPU) {
		spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);
		spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
	}
	outb(devpriv->ier, dev->iobase + PCI230_INT_SCE);
	spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);
	 
	devpriv->adccon =
	    (devpriv->adccon & (PCI230_ADC_IR_MASK | PCI230_ADC_IM_MASK)) |
	    PCI230_ADC_TRIG_NONE;
	outw(devpriv->adccon | PCI230_ADC_FIFO_RESET,
	     devpriv->daqio + PCI230_ADCCON);
	 
	pci230_release_all_resources(dev, OWNER_AICMD);
}

static void pci230_ai_start(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	struct pci230_private *devpriv = dev->private;
	unsigned long irqflags;
	unsigned short conv;
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;

	devpriv->ai_cmd_started = true;

	 
	spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
	devpriv->ier |= PCI230_INT_ADC;
	outb(devpriv->ier, dev->iobase + PCI230_INT_SCE);
	spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);

	 
	switch (cmd->convert_src) {
	default:
		conv = PCI230_ADC_TRIG_NONE;
		break;
	case TRIG_TIMER:
		 
		conv = PCI230_ADC_TRIG_Z2CT2;
		break;
	case TRIG_EXT:
		if (cmd->convert_arg & CR_EDGE) {
			if ((cmd->convert_arg & CR_INVERT) == 0) {
				 
				conv = PCI230_ADC_TRIG_EXTP;
			} else {
				 
				conv = PCI230_ADC_TRIG_EXTN;
			}
		} else {
			 
			if (cmd->convert_arg) {
				 
				conv = PCI230_ADC_TRIG_EXTP;
			} else {
				 
				conv = PCI230_ADC_TRIG_EXTN;
			}
		}
		break;
	case TRIG_INT:
		 
		conv = PCI230_ADC_TRIG_Z2CT2;
		break;
	}
	devpriv->adccon = (devpriv->adccon & ~PCI230_ADC_TRIG_MASK) | conv;
	outw(devpriv->adccon, devpriv->daqio + PCI230_ADCCON);
	if (cmd->convert_src == TRIG_INT)
		async->inttrig = pci230_ai_inttrig_convert;

	 
	pci230_ai_update_fifo_trigger_level(dev, s);
	if (cmd->convert_src == TRIG_TIMER) {
		 
		unsigned char zgat;

		if (cmd->scan_begin_src != TRIG_FOLLOW) {
			 
			zgat = pci230_gat_config(2, GAT_NOUTNM2);
		} else {
			 
			zgat = pci230_gat_config(2, GAT_VCC);
		}
		outb(zgat, dev->iobase + PCI230_ZGAT_SCE);
		if (cmd->scan_begin_src != TRIG_FOLLOW) {
			 
			switch (cmd->scan_begin_src) {
			default:
				zgat = pci230_gat_config(0, GAT_VCC);
				break;
			case TRIG_EXT:
				 
				zgat = pci230_gat_config(0, GAT_EXT);
				break;
			case TRIG_TIMER:
				 
				zgat = pci230_gat_config(0, GAT_NOUTNM2);
				break;
			case TRIG_INT:
				 
				zgat = pci230_gat_config(0, GAT_VCC);
				break;
			}
			outb(zgat, dev->iobase + PCI230_ZGAT_SCE);
			switch (cmd->scan_begin_src) {
			case TRIG_TIMER:
				 
				zgat = pci230_gat_config(1, GAT_VCC);
				outb(zgat, dev->iobase + PCI230_ZGAT_SCE);
				break;
			case TRIG_INT:
				async->inttrig = pci230_ai_inttrig_scan_begin;
				break;
			}
		}
	} else if (cmd->convert_src != TRIG_INT) {
		 
		pci230_release_shared(dev, RES_Z2CT2, OWNER_AICMD);
	}
}

static int pci230_ai_inttrig_start(struct comedi_device *dev,
				   struct comedi_subdevice *s,
				   unsigned int trig_num)
{
	struct comedi_cmd *cmd = &s->async->cmd;

	if (trig_num != cmd->start_arg)
		return -EINVAL;

	s->async->inttrig = NULL;
	pci230_ai_start(dev, s);

	return 1;
}

static void pci230_handle_ai(struct comedi_device *dev,
			     struct comedi_subdevice *s)
{
	struct pci230_private *devpriv = dev->private;
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	unsigned int status_fifo;
	unsigned int i;
	unsigned int nsamples;
	unsigned int fifoamount;
	unsigned short val;

	 
	nsamples = comedi_nsamples_left(s, PCI230_ADC_FIFOLEVEL_HALFFULL);
	if (nsamples == 0)
		return;

	fifoamount = 0;
	for (i = 0; i < nsamples; i++) {
		if (fifoamount == 0) {
			 
			status_fifo = inw(devpriv->daqio + PCI230_ADCCON);
			if (status_fifo & PCI230_ADC_FIFO_FULL_LATCHED) {
				 
				dev_err(dev->class_dev, "AI FIFO overrun\n");
				async->events |= COMEDI_CB_ERROR;
				break;
			} else if (status_fifo & PCI230_ADC_FIFO_EMPTY) {
				 
				break;
			} else if (status_fifo & PCI230_ADC_FIFO_HALF) {
				 
				fifoamount = PCI230_ADC_FIFOLEVEL_HALFFULL;
			} else if (devpriv->hwver > 0) {
				 
				fifoamount = inw(devpriv->daqio +
						 PCI230P_ADCFFLEV);
				if (fifoamount == 0)
					break;	 
			} else {
				 
				fifoamount = 1;
			}
		}

		val = pci230_ai_read(dev);
		if (!comedi_buf_write_samples(s, &val, 1))
			break;

		fifoamount--;

		if (cmd->stop_src == TRIG_COUNT &&
		    async->scans_done >= cmd->stop_arg) {
			async->events |= COMEDI_CB_EOA;
			break;
		}
	}

	 
	if (!(async->events & COMEDI_CB_CANCEL_MASK))
		pci230_ai_update_fifo_trigger_level(dev, s);
}

static int pci230_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct pci230_private *devpriv = dev->private;
	unsigned int i, chan, range, diff;
	unsigned int res_mask;
	unsigned short adccon, adcen;
	unsigned char zgat;

	 
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;

	 
	res_mask = 0;
	 
	res_mask |= RES_Z2CT2;
	if (cmd->scan_begin_src != TRIG_FOLLOW) {
		 
		res_mask |= RES_Z2CT0;
		if (cmd->scan_begin_src == TRIG_TIMER) {
			 
			res_mask |= RES_Z2CT1;
		}
	}
	 
	if (!pci230_claim_shared(dev, res_mask, OWNER_AICMD))
		return -EBUSY;

	 

	adccon = PCI230_ADC_FIFO_EN;
	adcen = 0;

	if (CR_AREF(cmd->chanlist[0]) == AREF_DIFF) {
		 
		diff = 1;
		adccon |= PCI230_ADC_IM_DIF;
	} else {
		 
		diff = 0;
		adccon |= PCI230_ADC_IM_SE;
	}

	range = CR_RANGE(cmd->chanlist[0]);
	devpriv->ai_bipolar = comedi_range_is_bipolar(s, range);
	if (devpriv->ai_bipolar)
		adccon |= PCI230_ADC_IR_BIP;
	else
		adccon |= PCI230_ADC_IR_UNI;

	for (i = 0; i < cmd->chanlist_len; i++) {
		unsigned int gainshift;

		chan = CR_CHAN(cmd->chanlist[i]);
		range = CR_RANGE(cmd->chanlist[i]);
		if (diff) {
			gainshift = 2 * chan;
			if (devpriv->hwver == 0) {
				 
				adcen |= 3 << gainshift;
			} else {
				 
				adcen |= 1 << gainshift;
			}
		} else {
			gainshift = chan & ~1;
			adcen |= 1 << chan;
		}
		devpriv->adcg = (devpriv->adcg & ~(3 << gainshift)) |
				(pci230_ai_gain[range] << gainshift);
	}

	 
	outw(adcen, devpriv->daqio + PCI230_ADCEN);

	 
	outw(devpriv->adcg, devpriv->daqio + PCI230_ADCG);

	 
	comedi_8254_set_mode(dev->pacer, 2, I8254_MODE1);

	 
	adccon |= PCI230_ADC_INT_FIFO_FULL | PCI230_ADC_TRIG_Z2CT2;

	 
	devpriv->adccon = adccon;
	outw(adccon | PCI230_ADC_FIFO_RESET, devpriv->daqio + PCI230_ADCCON);

	 
	usleep_range(25, 100);

	 
	outw(adccon | PCI230_ADC_FIFO_RESET, devpriv->daqio + PCI230_ADCCON);

	if (cmd->convert_src == TRIG_TIMER) {
		 
		zgat = pci230_gat_config(2, GAT_GND);
		outb(zgat, dev->iobase + PCI230_ZGAT_SCE);
		 
		pci230_ct_setup_ns_mode(dev, 2, I8254_MODE3, cmd->convert_arg,
					cmd->flags);
		if (cmd->scan_begin_src != TRIG_FOLLOW) {
			 
			zgat = pci230_gat_config(0, GAT_VCC);
			outb(zgat, dev->iobase + PCI230_ZGAT_SCE);
			pci230_ct_setup_ns_mode(dev, 0, I8254_MODE1,
						((u64)cmd->convert_arg *
						 cmd->scan_end_arg),
						CMDF_ROUND_UP);
			if (cmd->scan_begin_src == TRIG_TIMER) {
				 
				zgat = pci230_gat_config(1, GAT_GND);
				outb(zgat, dev->iobase + PCI230_ZGAT_SCE);
				pci230_ct_setup_ns_mode(dev, 1, I8254_MODE3,
							cmd->scan_begin_arg,
							cmd->flags);
			}
		}
	}

	if (cmd->start_src == TRIG_INT)
		s->async->inttrig = pci230_ai_inttrig_start;
	else	 
		pci230_ai_start(dev, s);

	return 0;
}

static int pci230_ai_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	pci230_ai_stop(dev, s);
	return 0;
}

 
static irqreturn_t pci230_interrupt(int irq, void *d)
{
	unsigned char status_int, valid_status_int, temp_ier;
	struct comedi_device *dev = d;
	struct pci230_private *devpriv = dev->private;
	struct comedi_subdevice *s_ao = dev->write_subdev;
	struct comedi_subdevice *s_ai = dev->read_subdev;
	unsigned long irqflags;

	 
	status_int = inb(dev->iobase + PCI230_INT_STAT);

	if (status_int == PCI230_INT_DISABLE)
		return IRQ_NONE;

	spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
	valid_status_int = devpriv->ier & status_int;
	 
	temp_ier = devpriv->ier & ~status_int;
	outb(temp_ier, dev->iobase + PCI230_INT_SCE);
	devpriv->intr_running = true;
	devpriv->intr_cpuid = THISCPU;
	spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);

	 

	if (valid_status_int & PCI230_INT_ZCLK_CT1)
		pci230_handle_ao_nofifo(dev, s_ao);

	if (valid_status_int & PCI230P2_INT_DAC)
		pci230_handle_ao_fifo(dev, s_ao);

	if (valid_status_int & PCI230_INT_ADC)
		pci230_handle_ai(dev, s_ai);

	 
	spin_lock_irqsave(&devpriv->isr_spinlock, irqflags);
	if (devpriv->ier != temp_ier)
		outb(devpriv->ier, dev->iobase + PCI230_INT_SCE);
	devpriv->intr_running = false;
	spin_unlock_irqrestore(&devpriv->isr_spinlock, irqflags);

	if (s_ao)
		comedi_handle_events(dev, s_ao);
	comedi_handle_events(dev, s_ai);

	return IRQ_HANDLED;
}

 
static bool pci230_match_pci_board(const struct pci230_board *board,
				   struct pci_dev *pci_dev)
{
	 
	if (board->id != pci_dev->device)
		return false;
	if (board->min_hwver == 0)
		return true;
	 
	if (pci_resource_len(pci_dev, 3) < 32)
		return false;	 
	 
	return true;
}

 
static const struct pci230_board *pci230_find_pci_board(struct pci_dev *pci_dev)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pci230_boards); i++)
		if (pci230_match_pci_board(&pci230_boards[i], pci_dev))
			return &pci230_boards[i];
	return NULL;
}

static int pci230_auto_attach(struct comedi_device *dev,
			      unsigned long context_unused)
{
	struct pci_dev *pci_dev = comedi_to_pci_dev(dev);
	const struct pci230_board *board;
	struct pci230_private *devpriv;
	struct comedi_subdevice *s;
	int rc;

	dev_info(dev->class_dev, "amplc_pci230: attach pci %s\n",
		 pci_name(pci_dev));

	devpriv = comedi_alloc_devpriv(dev, sizeof(*devpriv));
	if (!devpriv)
		return -ENOMEM;

	spin_lock_init(&devpriv->isr_spinlock);
	spin_lock_init(&devpriv->res_spinlock);
	spin_lock_init(&devpriv->ai_stop_spinlock);
	spin_lock_init(&devpriv->ao_stop_spinlock);

	board = pci230_find_pci_board(pci_dev);
	if (!board) {
		dev_err(dev->class_dev,
			"amplc_pci230: BUG! cannot determine board type!\n");
		return -EINVAL;
	}
	dev->board_ptr = board;
	dev->board_name = board->name;

	rc = comedi_pci_enable(dev);
	if (rc)
		return rc;

	 
	dev->iobase = pci_resource_start(pci_dev, 2);
	devpriv->daqio = pci_resource_start(pci_dev, 3);
	dev_dbg(dev->class_dev,
		"%s I/O region 1 0x%04lx I/O region 2 0x%04lx\n",
		dev->board_name, dev->iobase, devpriv->daqio);
	 
	devpriv->daccon = inw(devpriv->daqio + PCI230_DACCON) &
			  PCI230_DAC_OR_MASK;
	 
	if (pci_resource_len(pci_dev, 3) >= 32) {
		unsigned short extfunc = 0;

		devpriv->hwver = inw(devpriv->daqio + PCI230P_HWVER);
		if (devpriv->hwver < board->min_hwver) {
			dev_err(dev->class_dev,
				"%s - bad hardware version - got %u, need %u\n",
				dev->board_name, devpriv->hwver,
				board->min_hwver);
			return -EIO;
		}
		if (devpriv->hwver > 0) {
			if (!board->have_dio) {
				 
				extfunc |= PCI230P_EXTFUNC_GAT_EXTTRIG;
			}
			if (board->ao_bits && devpriv->hwver >= 2) {
				 
				extfunc |= PCI230P2_EXTFUNC_DACFIFO;
			}
		}
		outw(extfunc, devpriv->daqio + PCI230P_EXTFUNC);
		if (extfunc & PCI230P2_EXTFUNC_DACFIFO) {
			 
			outw(devpriv->daccon | PCI230P2_DAC_FIFO_EN |
			     PCI230P2_DAC_FIFO_RESET,
			     devpriv->daqio + PCI230_DACCON);
			 
			outw(0, devpriv->daqio + PCI230P2_DACEN);
			 
			outw(devpriv->daccon, devpriv->daqio + PCI230_DACCON);
		}
	}
	 
	outb(0, dev->iobase + PCI230_INT_SCE);
	 
	devpriv->adcg = 0;
	devpriv->adccon = PCI230_ADC_TRIG_NONE | PCI230_ADC_IM_SE |
			  PCI230_ADC_IR_BIP;
	outw(BIT(0), devpriv->daqio + PCI230_ADCEN);
	outw(devpriv->adcg, devpriv->daqio + PCI230_ADCG);
	outw(devpriv->adccon | PCI230_ADC_FIFO_RESET,
	     devpriv->daqio + PCI230_ADCCON);

	if (pci_dev->irq) {
		rc = request_irq(pci_dev->irq, pci230_interrupt, IRQF_SHARED,
				 dev->board_name, dev);
		if (rc == 0)
			dev->irq = pci_dev->irq;
	}

	dev->pacer = comedi_8254_init(dev->iobase + PCI230_Z2_CT_BASE,
				      0, I8254_IO8, 0);
	if (!dev->pacer)
		return -ENOMEM;

	rc = comedi_alloc_subdevices(dev, 3);
	if (rc)
		return rc;

	s = &dev->subdevices[0];
	 
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags = SDF_READABLE | SDF_DIFF | SDF_GROUND;
	s->n_chan = 16;
	s->maxdata = (1 << board->ai_bits) - 1;
	s->range_table = &pci230_ai_range;
	s->insn_read = pci230_ai_insn_read;
	s->len_chanlist = 256;	 
	if (dev->irq) {
		dev->read_subdev = s;
		s->subdev_flags |= SDF_CMD_READ;
		s->do_cmd = pci230_ai_cmd;
		s->do_cmdtest = pci230_ai_cmdtest;
		s->cancel = pci230_ai_cancel;
	}

	s = &dev->subdevices[1];
	 
	if (board->ao_bits) {
		s->type = COMEDI_SUBD_AO;
		s->subdev_flags = SDF_WRITABLE | SDF_GROUND;
		s->n_chan = 2;
		s->maxdata = (1 << board->ao_bits) - 1;
		s->range_table = &pci230_ao_range;
		s->insn_write = pci230_ao_insn_write;
		s->len_chanlist = 2;
		if (dev->irq) {
			dev->write_subdev = s;
			s->subdev_flags |= SDF_CMD_WRITE;
			s->do_cmd = pci230_ao_cmd;
			s->do_cmdtest = pci230_ao_cmdtest;
			s->cancel = pci230_ao_cancel;
		}

		rc = comedi_alloc_subdev_readback(s);
		if (rc)
			return rc;
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	s = &dev->subdevices[2];
	 
	if (board->have_dio) {
		rc = subdev_8255_init(dev, s, NULL, PCI230_PPI_X_BASE);
		if (rc)
			return rc;
	} else {
		s->type = COMEDI_SUBD_UNUSED;
	}

	return 0;
}

static struct comedi_driver amplc_pci230_driver = {
	.driver_name	= "amplc_pci230",
	.module		= THIS_MODULE,
	.auto_attach	= pci230_auto_attach,
	.detach		= comedi_pci_detach,
};

static int amplc_pci230_pci_probe(struct pci_dev *dev,
				  const struct pci_device_id *id)
{
	return comedi_pci_auto_config(dev, &amplc_pci230_driver,
				      id->driver_data);
}

static const struct pci_device_id amplc_pci230_pci_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMPLICON, PCI_DEVICE_ID_PCI230) },
	{ PCI_DEVICE(PCI_VENDOR_ID_AMPLICON, PCI_DEVICE_ID_PCI260) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, amplc_pci230_pci_table);

static struct pci_driver amplc_pci230_pci_driver = {
	.name		= "amplc_pci230",
	.id_table	= amplc_pci230_pci_table,
	.probe		= amplc_pci230_pci_probe,
	.remove		= comedi_pci_auto_unconfig,
};
module_comedi_pci_driver(amplc_pci230_driver, amplc_pci230_pci_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi driver for Amplicon PCI230(+) and PCI260(+)");
MODULE_LICENSE("GPL");
