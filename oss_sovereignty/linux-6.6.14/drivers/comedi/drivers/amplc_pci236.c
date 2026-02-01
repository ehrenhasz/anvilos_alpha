
 
 

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/comedi/comedi_pci.h>

#include "amplc_pc236.h"
#include "plx9052.h"

 
#define PCI236_INTR_DISABLE	(PLX9052_INTCSR_LI1POL |	\
				 PLX9052_INTCSR_LI2POL |	\
				 PLX9052_INTCSR_LI1SEL |	\
				 PLX9052_INTCSR_LI1CLRINT)

 
#define PCI236_INTR_ENABLE	(PLX9052_INTCSR_LI1ENAB |	\
				 PLX9052_INTCSR_LI1POL |	\
				 PLX9052_INTCSR_LI2POL |	\
				 PLX9052_INTCSR_PCIENAB |	\
				 PLX9052_INTCSR_LI1SEL |	\
				 PLX9052_INTCSR_LI1CLRINT)

static void pci236_intr_update_cb(struct comedi_device *dev, bool enable)
{
	struct pc236_private *devpriv = dev->private;

	 
	outl(enable ? PCI236_INTR_ENABLE : PCI236_INTR_DISABLE,
	     devpriv->lcr_iobase + PLX9052_INTCSR);
}

static bool pci236_intr_chk_clr_cb(struct comedi_device *dev)
{
	struct pc236_private *devpriv = dev->private;

	 
	if (!(inl(devpriv->lcr_iobase + PLX9052_INTCSR) &
	      PLX9052_INTCSR_LI1STAT))
		return false;
	 
	pci236_intr_update_cb(dev, devpriv->enable_irq);
	return true;
}

static const struct pc236_board pc236_pci_board = {
	.name = "pci236",
	.intr_update_cb = pci236_intr_update_cb,
	.intr_chk_clr_cb = pci236_intr_chk_clr_cb,
};

static int pci236_auto_attach(struct comedi_device *dev,
			      unsigned long context_unused)
{
	struct pci_dev *pci_dev = comedi_to_pci_dev(dev);
	struct pc236_private *devpriv;
	unsigned long iobase;
	int ret;

	dev_info(dev->class_dev, "amplc_pci236: attach pci %s\n",
		 pci_name(pci_dev));

	devpriv = comedi_alloc_devpriv(dev, sizeof(*devpriv));
	if (!devpriv)
		return -ENOMEM;

	dev->board_ptr = &pc236_pci_board;
	dev->board_name = pc236_pci_board.name;
	ret = comedi_pci_enable(dev);
	if (ret)
		return ret;

	devpriv->lcr_iobase = pci_resource_start(pci_dev, 1);
	iobase = pci_resource_start(pci_dev, 2);
	return amplc_pc236_common_attach(dev, iobase, pci_dev->irq,
					 IRQF_SHARED);
}

static struct comedi_driver amplc_pci236_driver = {
	.driver_name = "amplc_pci236",
	.module = THIS_MODULE,
	.auto_attach = pci236_auto_attach,
	.detach = comedi_pci_detach,
};

static const struct pci_device_id pci236_pci_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMPLICON, 0x0009) },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, pci236_pci_table);

static int amplc_pci236_pci_probe(struct pci_dev *dev,
				  const struct pci_device_id *id)
{
	return comedi_pci_auto_config(dev, &amplc_pci236_driver,
				      id->driver_data);
}

static struct pci_driver amplc_pci236_pci_driver = {
	.name		= "amplc_pci236",
	.id_table	= pci236_pci_table,
	.probe		= &amplc_pci236_pci_probe,
	.remove		= comedi_pci_auto_unconfig,
};

module_comedi_pci_driver(amplc_pci236_driver, amplc_pci236_pci_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi driver for Amplicon PCI236 DIO boards");
MODULE_LICENSE("GPL");
