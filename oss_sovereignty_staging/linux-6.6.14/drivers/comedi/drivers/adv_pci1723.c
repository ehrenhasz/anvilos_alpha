
 

 

#include <linux/module.h>
#include <linux/comedi/comedi_pci.h>

 
#define PCI1723_AO_REG(x)		(0x00 + ((x) * 2))
#define PCI1723_BOARD_ID_REG		0x10
#define PCI1723_BOARD_ID_MASK		(0xf << 0)
#define PCI1723_SYNC_CTRL_REG		0x12
#define PCI1723_SYNC_CTRL(x)		(((x) & 0x1) << 0)
#define PCI1723_SYNC_CTRL_ASYNC		PCI1723_SYNC_CTRL(0)
#define PCI1723_SYNC_CTRL_SYNC		PCI1723_SYNC_CTRL(1)
#define PCI1723_CTRL_REG		0x14
#define PCI1723_CTRL_BUSY		BIT(15)
#define PCI1723_CTRL_INIT		BIT(14)
#define PCI1723_CTRL_SELF		BIT(8)
#define PCI1723_CTRL_IDX(x)		(((x) & 0x3) << 6)
#define PCI1723_CTRL_RANGE(x)		(((x) & 0x3) << 4)
#define PCI1723_CTRL_SEL(x)		(((x) & 0x1) << 3)
#define PCI1723_CTRL_GAIN		PCI1723_CTRL_SEL(0)
#define PCI1723_CTRL_OFFSET		PCI1723_CTRL_SEL(1)
#define PCI1723_CTRL_CHAN(x)		(((x) & 0x7) << 0)
#define PCI1723_CALIB_CTRL_REG		0x16
#define PCI1723_CALIB_CTRL_CS		BIT(2)
#define PCI1723_CALIB_CTRL_DAT		BIT(1)
#define PCI1723_CALIB_CTRL_CLK		BIT(0)
#define PCI1723_CALIB_STROBE_REG	0x18
#define PCI1723_DIO_CTRL_REG		0x1a
#define PCI1723_DIO_CTRL_HDIO		BIT(1)
#define PCI1723_DIO_CTRL_LDIO		BIT(0)
#define PCI1723_DIO_DATA_REG		0x1c
#define PCI1723_CALIB_DATA_REG		0x1e
#define PCI1723_SYNC_STROBE_REG		0x20
#define PCI1723_RESET_AO_STROBE_REG	0x22
#define PCI1723_RESET_CALIB_STROBE_REG	0x24
#define PCI1723_RANGE_STROBE_REG	0x26
#define PCI1723_VREF_REG		0x28
#define PCI1723_VREF(x)			(((x) & 0x3) << 0)
#define PCI1723_VREF_NEG10V		PCI1723_VREF(0)
#define PCI1723_VREF_0V			PCI1723_VREF(1)
#define PCI1723_VREF_POS10V		PCI1723_VREF(3)

static int pci1723_ao_insn_write(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn,
				 unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	int i;

	for (i = 0; i < insn->n; i++) {
		unsigned int val = data[i];

		outw(val, dev->iobase + PCI1723_AO_REG(chan));
		s->readback[chan] = val;
	}

	return insn->n;
}

static int pci1723_dio_insn_config(struct comedi_device *dev,
				   struct comedi_subdevice *s,
				   struct comedi_insn *insn,
				   unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int mask = (chan < 8) ? 0x00ff : 0xff00;
	unsigned short mode = 0x0000;		 
	int ret;

	ret = comedi_dio_insn_config(dev, s, insn, data, mask);
	if (ret)
		return ret;

	if (!(s->io_bits & 0x00ff))
		mode |= PCI1723_DIO_CTRL_LDIO;	 
	if (!(s->io_bits & 0xff00))
		mode |= PCI1723_DIO_CTRL_HDIO;	 
	outw(mode, dev->iobase + PCI1723_DIO_CTRL_REG);

	return insn->n;
}

static int pci1723_dio_insn_bits(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn,
				 unsigned int *data)
{
	if (comedi_dio_update_state(s, data))
		outw(s->state, dev->iobase + PCI1723_DIO_DATA_REG);

	data[1] = inw(dev->iobase + PCI1723_DIO_DATA_REG);

	return insn->n;
}

static int pci1723_auto_attach(struct comedi_device *dev,
			       unsigned long context_unused)
{
	struct pci_dev *pcidev = comedi_to_pci_dev(dev);
	struct comedi_subdevice *s;
	unsigned int val;
	int ret;
	int i;

	ret = comedi_pci_enable(dev);
	if (ret)
		return ret;
	dev->iobase = pci_resource_start(pcidev, 2);

	ret = comedi_alloc_subdevices(dev, 2);
	if (ret)
		return ret;

	s = &dev->subdevices[0];
	s->type		= COMEDI_SUBD_AO;
	s->subdev_flags	= SDF_WRITABLE | SDF_GROUND | SDF_COMMON;
	s->n_chan	= 8;
	s->maxdata	= 0xffff;
	s->range_table	= &range_bipolar10;
	s->insn_write	= pci1723_ao_insn_write;

	ret = comedi_alloc_subdev_readback(s);
	if (ret)
		return ret;

	 
	outw(PCI1723_SYNC_CTRL_SYNC, dev->iobase + PCI1723_SYNC_CTRL_REG);
	for (i = 0; i < s->n_chan; i++) {
		outw(PCI1723_CTRL_RANGE(0) | PCI1723_CTRL_CHAN(i),
		     PCI1723_CTRL_REG);
		outw(0, dev->iobase + PCI1723_RANGE_STROBE_REG);

		outw(0x8000, dev->iobase + PCI1723_AO_REG(i));
		s->readback[i] = 0x8000;
	}
	outw(0, dev->iobase + PCI1723_SYNC_STROBE_REG);

	 
	outw(PCI1723_SYNC_CTRL_ASYNC, dev->iobase + PCI1723_SYNC_CTRL_REG);

	s = &dev->subdevices[1];
	s->type		= COMEDI_SUBD_DIO;
	s->subdev_flags	= SDF_READABLE | SDF_WRITABLE;
	s->n_chan	= 16;
	s->maxdata	= 1;
	s->range_table	= &range_digital;
	s->insn_config	= pci1723_dio_insn_config;
	s->insn_bits	= pci1723_dio_insn_bits;

	 
	val = inw(dev->iobase + PCI1723_DIO_CTRL_REG);
	if (!(val & PCI1723_DIO_CTRL_LDIO))
		s->io_bits |= 0x00ff;	 
	if (!(val & PCI1723_DIO_CTRL_HDIO))
		s->io_bits |= 0xff00;	 
	s->state = inw(dev->iobase + PCI1723_DIO_DATA_REG);

	return 0;
}

static struct comedi_driver adv_pci1723_driver = {
	.driver_name	= "adv_pci1723",
	.module		= THIS_MODULE,
	.auto_attach	= pci1723_auto_attach,
	.detach		= comedi_pci_detach,
};

static int adv_pci1723_pci_probe(struct pci_dev *dev,
				 const struct pci_device_id *id)
{
	return comedi_pci_auto_config(dev, &adv_pci1723_driver,
				      id->driver_data);
}

static const struct pci_device_id adv_pci1723_pci_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1723) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, adv_pci1723_pci_table);

static struct pci_driver adv_pci1723_pci_driver = {
	.name		= "adv_pci1723",
	.id_table	= adv_pci1723_pci_table,
	.probe		= adv_pci1723_pci_probe,
	.remove		= comedi_pci_auto_unconfig,
};
module_comedi_pci_driver(adv_pci1723_driver, adv_pci1723_pci_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Advantech PCI-1723 Comedi driver");
MODULE_LICENSE("GPL");
