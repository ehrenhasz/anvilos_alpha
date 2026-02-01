
 
 

 

#include <linux/module.h>
#include <linux/comedi/comedi_pci.h>
#include <linux/comedi/comedi_8255.h>

 
#define PCI_ID_PCIM_DDA06_16		0x0053

 
#define PCIMDDA_DA_CHAN(x)		(0x00 + (x) * 2)
#define PCIMDDA_8255_BASE_REG		0x0c

static int cb_pcimdda_ao_insn_write(struct comedi_device *dev,
				    struct comedi_subdevice *s,
				    struct comedi_insn *insn,
				    unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned long offset = dev->iobase + PCIMDDA_DA_CHAN(chan);
	unsigned int val = s->readback[chan];
	int i;

	for (i = 0; i < insn->n; i++) {
		val = data[i];

		 
		outb(val & 0x00ff, offset);
		outb((val >> 8) & 0x00ff, offset + 1);
	}
	s->readback[chan] = val;

	return insn->n;
}

static int cb_pcimdda_ao_insn_read(struct comedi_device *dev,
				   struct comedi_subdevice *s,
				   struct comedi_insn *insn,
				   unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);

	 
	inw(dev->iobase + PCIMDDA_DA_CHAN(chan));

	return comedi_readback_insn_read(dev, s, insn, data);
}

static int cb_pcimdda_auto_attach(struct comedi_device *dev,
				  unsigned long context_unused)
{
	struct pci_dev *pcidev = comedi_to_pci_dev(dev);
	struct comedi_subdevice *s;
	int ret;

	ret = comedi_pci_enable(dev);
	if (ret)
		return ret;
	dev->iobase = pci_resource_start(pcidev, 3);

	ret = comedi_alloc_subdevices(dev, 2);
	if (ret)
		return ret;

	s = &dev->subdevices[0];
	 
	s->type		= COMEDI_SUBD_AO;
	s->subdev_flags	= SDF_WRITABLE | SDF_READABLE;
	s->n_chan	= 6;
	s->maxdata	= 0xffff;
	s->range_table	= &range_bipolar5;
	s->insn_write	= cb_pcimdda_ao_insn_write;
	s->insn_read	= cb_pcimdda_ao_insn_read;

	ret = comedi_alloc_subdev_readback(s);
	if (ret)
		return ret;

	s = &dev->subdevices[1];
	 
	return subdev_8255_init(dev, s, NULL, PCIMDDA_8255_BASE_REG);
}

static struct comedi_driver cb_pcimdda_driver = {
	.driver_name	= "cb_pcimdda",
	.module		= THIS_MODULE,
	.auto_attach	= cb_pcimdda_auto_attach,
	.detach		= comedi_pci_detach,
};

static int cb_pcimdda_pci_probe(struct pci_dev *dev,
				const struct pci_device_id *id)
{
	return comedi_pci_auto_config(dev, &cb_pcimdda_driver,
				      id->driver_data);
}

static const struct pci_device_id cb_pcimdda_pci_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_CB, PCI_ID_PCIM_DDA06_16) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, cb_pcimdda_pci_table);

static struct pci_driver cb_pcimdda_driver_pci_driver = {
	.name		= "cb_pcimdda",
	.id_table	= cb_pcimdda_pci_table,
	.probe		= cb_pcimdda_pci_probe,
	.remove		= comedi_pci_auto_unconfig,
};
module_comedi_pci_driver(cb_pcimdda_driver, cb_pcimdda_driver_pci_driver);

MODULE_AUTHOR("Calin A. Culianu <calin@rtlab.org>");
MODULE_DESCRIPTION("Comedi low-level driver for the Computerboards PCIM-DDA series.  Currently only supports PCIM-DDA06-16 (which also happens to be the only board in this series. :) ) ");
MODULE_LICENSE("GPL");
