
 

 

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/comedi/comedi_pci.h>

 
#define PCI1720_AO_LSB_REG(x)		(0x00 + ((x) * 2))
#define PCI1720_AO_MSB_REG(x)		(0x01 + ((x) * 2))
#define PCI1720_AO_RANGE_REG		0x08
#define PCI1720_AO_RANGE(c, r)		(((r) & 0x3) << ((c) * 2))
#define PCI1720_AO_RANGE_MASK(c)	PCI1720_AO_RANGE((c), 0x3)
#define PCI1720_SYNC_REG		0x09
#define PCI1720_SYNC_CTRL_REG		0x0f
#define PCI1720_SYNC_CTRL_SC0		BIT(0)
#define PCI1720_BOARDID_REG		0x14

static const struct comedi_lrange pci1720_ao_range = {
	4, {
		UNI_RANGE(5),
		UNI_RANGE(10),
		BIP_RANGE(5),
		BIP_RANGE(10)
	}
};

static int pci1720_ao_insn_write(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn,
				 unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int range = CR_RANGE(insn->chanspec);
	unsigned int val;
	int i;

	 
	val = inb(dev->iobase + PCI1720_AO_RANGE_REG);
	val &= ~PCI1720_AO_RANGE_MASK(chan);
	val |= PCI1720_AO_RANGE(chan, range);
	outb(val, dev->iobase + PCI1720_AO_RANGE_REG);

	val = s->readback[chan];
	for (i = 0; i < insn->n; i++) {
		val = data[i];

		outb(val & 0xff, dev->iobase + PCI1720_AO_LSB_REG(chan));
		outb((val >> 8) & 0xff, dev->iobase + PCI1720_AO_MSB_REG(chan));

		 
		usleep_range(2, 100);
	}

	s->readback[chan] = val;

	return insn->n;
}

static int pci1720_di_insn_bits(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn,
				unsigned int *data)
{
	data[1] = inb(dev->iobase + PCI1720_BOARDID_REG);

	return insn->n;
}

static int pci1720_auto_attach(struct comedi_device *dev,
			       unsigned long context)
{
	struct pci_dev *pcidev = comedi_to_pci_dev(dev);
	struct comedi_subdevice *s;
	int ret;

	ret = comedi_pci_enable(dev);
	if (ret)
		return ret;
	dev->iobase = pci_resource_start(pcidev, 2);

	ret = comedi_alloc_subdevices(dev, 2);
	if (ret)
		return ret;

	 
	s = &dev->subdevices[0];
	s->type		= COMEDI_SUBD_AO;
	s->subdev_flags	= SDF_WRITABLE;
	s->n_chan	= 4;
	s->maxdata	= 0x0fff;
	s->range_table	= &pci1720_ao_range;
	s->insn_write	= pci1720_ao_insn_write;

	ret = comedi_alloc_subdev_readback(s);
	if (ret)
		return ret;

	 
	s = &dev->subdevices[1];
	s->type		= COMEDI_SUBD_DI;
	s->subdev_flags	= SDF_READABLE;
	s->n_chan	= 4;
	s->maxdata	= 1;
	s->range_table	= &range_digital;
	s->insn_bits	= pci1720_di_insn_bits;

	 
	outb(0, dev->iobase + PCI1720_SYNC_CTRL_REG);

	return 0;
}

static struct comedi_driver adv_pci1720_driver = {
	.driver_name	= "adv_pci1720",
	.module		= THIS_MODULE,
	.auto_attach	= pci1720_auto_attach,
	.detach		= comedi_pci_detach,
};

static int adv_pci1720_pci_probe(struct pci_dev *dev,
				 const struct pci_device_id *id)
{
	return comedi_pci_auto_config(dev, &adv_pci1720_driver,
				      id->driver_data);
}

static const struct pci_device_id adv_pci1720_pci_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1720) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, adv_pci1720_pci_table);

static struct pci_driver adv_pci1720_pci_driver = {
	.name		= "adv_pci1720",
	.id_table	= adv_pci1720_pci_table,
	.probe		= adv_pci1720_pci_probe,
	.remove		= comedi_pci_auto_unconfig,
};
module_comedi_pci_driver(adv_pci1720_driver, adv_pci1720_pci_driver);

MODULE_AUTHOR("H Hartley Sweeten <hsweeten@visionengravers.com>");
MODULE_DESCRIPTION("Comedi driver for Advantech PCI-1720 Analog Output board");
MODULE_LICENSE("GPL");
