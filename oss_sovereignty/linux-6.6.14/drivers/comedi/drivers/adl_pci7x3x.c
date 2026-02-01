
 

 

#include <linux/module.h>
#include <linux/comedi/comedi_pci.h>

#include "plx9052.h"

 
#define PCI7X3X_DIO_REG		0x0000	 
#define PCI743X_DIO_REG		0x0004

#define ADL_PT_CLRIRQ		0x0040	 

#define LINTI1_EN_ACT_IDI0 (PLX9052_INTCSR_LI1ENAB | PLX9052_INTCSR_LI1STAT)
#define LINTI2_EN_ACT_IDI1 (PLX9052_INTCSR_LI2ENAB | PLX9052_INTCSR_LI2STAT)
#define EN_PCI_LINT2H_LINT1H	\
	(PLX9052_INTCSR_PCIENAB | PLX9052_INTCSR_LI2POL | PLX9052_INTCSR_LI1POL)

enum adl_pci7x3x_boardid {
	BOARD_PCI7230,
	BOARD_PCI7233,
	BOARD_PCI7234,
	BOARD_PCI7432,
	BOARD_PCI7433,
	BOARD_PCI7434,
};

struct adl_pci7x3x_boardinfo {
	const char *name;
	int nsubdevs;
	int di_nchan;
	int do_nchan;
	int irq_nchan;
};

static const struct adl_pci7x3x_boardinfo adl_pci7x3x_boards[] = {
	[BOARD_PCI7230] = {
		.name		= "adl_pci7230",
		.nsubdevs	= 4,   
		.di_nchan	= 16,
		.do_nchan	= 16,
		.irq_nchan	= 2,
	},
	[BOARD_PCI7233] = {
		.name		= "adl_pci7233",
		.nsubdevs	= 1,
		.di_nchan	= 32,
	},
	[BOARD_PCI7234] = {
		.name		= "adl_pci7234",
		.nsubdevs	= 1,
		.do_nchan	= 32,
	},
	[BOARD_PCI7432] = {
		.name		= "adl_pci7432",
		.nsubdevs	= 2,
		.di_nchan	= 32,
		.do_nchan	= 32,
	},
	[BOARD_PCI7433] = {
		.name		= "adl_pci7433",
		.nsubdevs	= 2,
		.di_nchan	= 64,
	},
	[BOARD_PCI7434] = {
		.name		= "adl_pci7434",
		.nsubdevs	= 2,
		.do_nchan	= 64,
	}
};

struct adl_pci7x3x_dev_private_data {
	unsigned long lcr_io_base;
	unsigned int int_ctrl;
};

struct adl_pci7x3x_sd_private_data {
	spinlock_t subd_slock;		 
	unsigned long port_offset;
	short int cmd_running;
};

static void process_irq(struct comedi_device *dev, unsigned int subdev,
			unsigned short intcsr)
{
	struct comedi_subdevice *s = &dev->subdevices[subdev];
	struct adl_pci7x3x_sd_private_data *sd_priv = s->private;
	unsigned long reg = sd_priv->port_offset;
	struct comedi_async *async_p = s->async;

	if (async_p) {
		unsigned short val = inw(dev->iobase + reg);

		spin_lock(&sd_priv->subd_slock);
		if (sd_priv->cmd_running)
			comedi_buf_write_samples(s, &val, 1);
		spin_unlock(&sd_priv->subd_slock);
		comedi_handle_events(dev, s);
	}
}

static irqreturn_t adl_pci7x3x_interrupt(int irq, void *p_device)
{
	struct comedi_device *dev = p_device;
	struct adl_pci7x3x_dev_private_data *dev_private = dev->private;
	unsigned long cpu_flags;
	unsigned int intcsr;
	bool li1stat, li2stat;

	if (!dev->attached) {
		 
		 
		return IRQ_NONE;
	}

	 
	spin_lock_irqsave(&dev->spinlock, cpu_flags);
	intcsr = inl(dev_private->lcr_io_base + PLX9052_INTCSR);
	li1stat = (intcsr & LINTI1_EN_ACT_IDI0) == LINTI1_EN_ACT_IDI0;
	li2stat = (intcsr & LINTI2_EN_ACT_IDI1) == LINTI2_EN_ACT_IDI1;
	if (li1stat || li2stat) {
		 
		 
		outb(0x00, dev->iobase + ADL_PT_CLRIRQ);
	}
	spin_unlock_irqrestore(&dev->spinlock, cpu_flags);

	 

	if (li1stat)	 
		process_irq(dev, 2, intcsr);

	if (li2stat)	 
		process_irq(dev, 3, intcsr);

	return IRQ_RETVAL(li1stat || li2stat);
}

static int adl_pci7x3x_asy_cmdtest(struct comedi_device *dev,
				   struct comedi_subdevice *s,
				   struct comedi_cmd *cmd)
{
	int err = 0;

	 

	err |= comedi_check_trigger_src(&cmd->start_src, TRIG_NOW);
	err |= comedi_check_trigger_src(&cmd->scan_begin_src, TRIG_EXT);
	err |= comedi_check_trigger_src(&cmd->convert_src, TRIG_FOLLOW);
	err |= comedi_check_trigger_src(&cmd->scan_end_src, TRIG_COUNT);
	err |= comedi_check_trigger_src(&cmd->stop_src, TRIG_NONE);

	if (err)
		return 1;

	 
	 

	 

	err |= comedi_check_trigger_arg_is(&cmd->start_arg, 0);
	err |= comedi_check_trigger_arg_is(&cmd->scan_begin_arg, 0);
	err |= comedi_check_trigger_arg_is(&cmd->convert_arg, 0);
	err |= comedi_check_trigger_arg_is(&cmd->scan_end_arg,
					   cmd->chanlist_len);
	err |= comedi_check_trigger_arg_is(&cmd->stop_arg, 0);

	if (err)
		return 3;

	 

	 

	return 0;
}

static int adl_pci7x3x_asy_cmd(struct comedi_device *dev,
			       struct comedi_subdevice *s)
{
	struct adl_pci7x3x_dev_private_data *dev_private = dev->private;
	struct adl_pci7x3x_sd_private_data *sd_priv = s->private;
	unsigned long cpu_flags;
	unsigned int int_enab;

	if (s->index == 2) {
		 
		int_enab = PLX9052_INTCSR_LI1ENAB;
	} else {
		 
		int_enab = PLX9052_INTCSR_LI2ENAB;
	}

	spin_lock_irqsave(&dev->spinlock, cpu_flags);
	dev_private->int_ctrl |= int_enab;
	outl(dev_private->int_ctrl, dev_private->lcr_io_base + PLX9052_INTCSR);
	spin_unlock_irqrestore(&dev->spinlock, cpu_flags);

	spin_lock_irqsave(&sd_priv->subd_slock, cpu_flags);
	sd_priv->cmd_running = 1;
	spin_unlock_irqrestore(&sd_priv->subd_slock, cpu_flags);

	return 0;
}

static int adl_pci7x3x_asy_cancel(struct comedi_device *dev,
				  struct comedi_subdevice *s)
{
	struct adl_pci7x3x_dev_private_data *dev_private = dev->private;
	struct adl_pci7x3x_sd_private_data *sd_priv = s->private;
	unsigned long cpu_flags;
	unsigned int int_enab;

	spin_lock_irqsave(&sd_priv->subd_slock, cpu_flags);
	sd_priv->cmd_running = 0;
	spin_unlock_irqrestore(&sd_priv->subd_slock, cpu_flags);
	 
	if (s->index == 2)
		int_enab = PLX9052_INTCSR_LI1ENAB;
	else
		int_enab = PLX9052_INTCSR_LI2ENAB;
	spin_lock_irqsave(&dev->spinlock, cpu_flags);
	dev_private->int_ctrl &= ~int_enab;
	outl(dev_private->int_ctrl, dev_private->lcr_io_base + PLX9052_INTCSR);
	spin_unlock_irqrestore(&dev->spinlock, cpu_flags);

	return 0;
}

 
static int adl_pci7x3x_dirq_insn_bits(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data)
{
	struct adl_pci7x3x_sd_private_data *sd_priv = s->private;
	unsigned long reg = (unsigned long)sd_priv->port_offset;

	data[1] = inl(dev->iobase + reg);

	return insn->n;
}

static int adl_pci7x3x_do_insn_bits(struct comedi_device *dev,
				    struct comedi_subdevice *s,
				    struct comedi_insn *insn,
				    unsigned int *data)
{
	unsigned long reg = (unsigned long)s->private;

	if (comedi_dio_update_state(s, data)) {
		unsigned int val = s->state;

		if (s->n_chan == 16) {
			 
			val |= val << 16;
		}
		outl(val, dev->iobase + reg);
	}

	data[1] = s->state;

	return insn->n;
}

static int adl_pci7x3x_di_insn_bits(struct comedi_device *dev,
				    struct comedi_subdevice *s,
				    struct comedi_insn *insn,
				    unsigned int *data)
{
	unsigned long reg = (unsigned long)s->private;

	data[1] = inl(dev->iobase + reg);

	return insn->n;
}

static int adl_pci7x3x_reset(struct comedi_device *dev)
{
	struct adl_pci7x3x_dev_private_data *dev_private = dev->private;

	 
	dev_private->int_ctrl = 0x00;   
	outl(dev_private->int_ctrl, dev_private->lcr_io_base + PLX9052_INTCSR);

	return 0;
}

static int adl_pci7x3x_auto_attach(struct comedi_device *dev,
				   unsigned long context)
{
	struct pci_dev *pcidev = comedi_to_pci_dev(dev);
	const struct adl_pci7x3x_boardinfo *board = NULL;
	struct comedi_subdevice *s;
	struct adl_pci7x3x_dev_private_data *dev_private;
	int subdev;
	int nchan;
	int ret;
	int ic;

	if (context < ARRAY_SIZE(adl_pci7x3x_boards))
		board = &adl_pci7x3x_boards[context];
	if (!board)
		return -ENODEV;
	dev->board_ptr = board;
	dev->board_name = board->name;

	dev_private = comedi_alloc_devpriv(dev, sizeof(*dev_private));
	if (!dev_private)
		return -ENOMEM;

	ret = comedi_pci_enable(dev);
	if (ret)
		return ret;
	dev->iobase = pci_resource_start(pcidev, 2);
	dev_private->lcr_io_base = pci_resource_start(pcidev, 1);

	adl_pci7x3x_reset(dev);

	if (board->irq_nchan) {
		 
		outb(0x00, dev->iobase + ADL_PT_CLRIRQ);

		if (pcidev->irq) {
			ret = request_irq(pcidev->irq, adl_pci7x3x_interrupt,
					  IRQF_SHARED, dev->board_name, dev);
			if (ret == 0) {
				dev->irq = pcidev->irq;
				 
				dev_private->int_ctrl = EN_PCI_LINT2H_LINT1H;
				outl(dev_private->int_ctrl,
				     dev_private->lcr_io_base + PLX9052_INTCSR);
			}
		}
	}

	ret = comedi_alloc_subdevices(dev, board->nsubdevs);
	if (ret)
		return ret;

	subdev = 0;

	if (board->di_nchan) {
		nchan = min(board->di_nchan, 32);

		s = &dev->subdevices[subdev];
		 
		s->type		= COMEDI_SUBD_DI;
		s->subdev_flags	= SDF_READABLE;
		s->n_chan	= nchan;
		s->maxdata	= 1;
		s->insn_bits	= adl_pci7x3x_di_insn_bits;
		s->range_table	= &range_digital;

		s->private	= (void *)PCI7X3X_DIO_REG;

		subdev++;

		nchan = board->di_nchan - nchan;
		if (nchan) {
			s = &dev->subdevices[subdev];
			 
			s->type		= COMEDI_SUBD_DI;
			s->subdev_flags	= SDF_READABLE;
			s->n_chan	= nchan;
			s->maxdata	= 1;
			s->insn_bits	= adl_pci7x3x_di_insn_bits;
			s->range_table	= &range_digital;

			s->private	= (void *)PCI743X_DIO_REG;

			subdev++;
		}
	}

	if (board->do_nchan) {
		nchan = min(board->do_nchan, 32);

		s = &dev->subdevices[subdev];
		 
		s->type		= COMEDI_SUBD_DO;
		s->subdev_flags	= SDF_WRITABLE;
		s->n_chan	= nchan;
		s->maxdata	= 1;
		s->insn_bits	= adl_pci7x3x_do_insn_bits;
		s->range_table	= &range_digital;

		s->private	= (void *)PCI7X3X_DIO_REG;

		subdev++;

		nchan = board->do_nchan - nchan;
		if (nchan) {
			s = &dev->subdevices[subdev];
			 
			s->type		= COMEDI_SUBD_DO;
			s->subdev_flags	= SDF_WRITABLE;
			s->n_chan	= nchan;
			s->maxdata	= 1;
			s->insn_bits	= adl_pci7x3x_do_insn_bits;
			s->range_table	= &range_digital;

			s->private	= (void *)PCI743X_DIO_REG;

			subdev++;
		}
	}

	for (ic = 0; ic < board->irq_nchan; ++ic) {
		struct adl_pci7x3x_sd_private_data *sd_priv;

		nchan = 1;

		s = &dev->subdevices[subdev];
		 
		s->type		= COMEDI_SUBD_DI;
		s->subdev_flags	= SDF_READABLE;
		s->n_chan	= nchan;
		s->maxdata	= 1;
		s->insn_bits	= adl_pci7x3x_dirq_insn_bits;
		s->range_table	= &range_digital;

		sd_priv = comedi_alloc_spriv(s, sizeof(*sd_priv));
		if (!sd_priv)
			return -ENOMEM;

		spin_lock_init(&sd_priv->subd_slock);
		sd_priv->port_offset = PCI7X3X_DIO_REG;
		sd_priv->cmd_running = 0;

		if (dev->irq) {
			dev->read_subdev = s;
			s->type		= COMEDI_SUBD_DI;
			s->subdev_flags	= SDF_READABLE | SDF_CMD_READ;
			s->len_chanlist	= 1;
			s->do_cmdtest	= adl_pci7x3x_asy_cmdtest;
			s->do_cmd	= adl_pci7x3x_asy_cmd;
			s->cancel	= adl_pci7x3x_asy_cancel;
		}

		subdev++;
	}

	return 0;
}

static void adl_pci7x3x_detach(struct comedi_device *dev)
{
	if (dev->iobase)
		adl_pci7x3x_reset(dev);
	comedi_pci_detach(dev);
}

static struct comedi_driver adl_pci7x3x_driver = {
	.driver_name	= "adl_pci7x3x",
	.module		= THIS_MODULE,
	.auto_attach	= adl_pci7x3x_auto_attach,
	.detach		= adl_pci7x3x_detach,
};

static int adl_pci7x3x_pci_probe(struct pci_dev *dev,
				 const struct pci_device_id *id)
{
	return comedi_pci_auto_config(dev, &adl_pci7x3x_driver,
				      id->driver_data);
}

static const struct pci_device_id adl_pci7x3x_pci_table[] = {
	{ PCI_VDEVICE(ADLINK, 0x7230), BOARD_PCI7230 },
	{ PCI_VDEVICE(ADLINK, 0x7233), BOARD_PCI7233 },
	{ PCI_VDEVICE(ADLINK, 0x7234), BOARD_PCI7234 },
	{ PCI_VDEVICE(ADLINK, 0x7432), BOARD_PCI7432 },
	{ PCI_VDEVICE(ADLINK, 0x7433), BOARD_PCI7433 },
	{ PCI_VDEVICE(ADLINK, 0x7434), BOARD_PCI7434 },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, adl_pci7x3x_pci_table);

static struct pci_driver adl_pci7x3x_pci_driver = {
	.name		= "adl_pci7x3x",
	.id_table	= adl_pci7x3x_pci_table,
	.probe		= adl_pci7x3x_pci_probe,
	.remove		= comedi_pci_auto_unconfig,
};
module_comedi_pci_driver(adl_pci7x3x_driver, adl_pci7x3x_pci_driver);

MODULE_DESCRIPTION("ADLINK PCI-723x/743x Isolated Digital I/O boards");
MODULE_AUTHOR("H Hartley Sweeten <hsweeten@visionengravers.com>");
MODULE_LICENSE("GPL");
