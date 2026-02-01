
 

 

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/comedi/comedi_pci.h>

#define ICP_MULTI_ADC_CSR	0x00	 
#define ICP_MULTI_ADC_CSR_ST	BIT(0)	 
#define ICP_MULTI_ADC_CSR_BSY	BIT(0)	 
#define ICP_MULTI_ADC_CSR_BI	BIT(4)	 
#define ICP_MULTI_ADC_CSR_RA	BIT(5)	 
#define ICP_MULTI_ADC_CSR_DI	BIT(6)	 
#define ICP_MULTI_ADC_CSR_DI_CHAN(x) (((x) & 0x7) << 9)
#define ICP_MULTI_ADC_CSR_SE_CHAN(x) (((x) & 0xf) << 8)
#define ICP_MULTI_AI		2	 
#define ICP_MULTI_DAC_CSR	0x04	 
#define ICP_MULTI_DAC_CSR_ST	BIT(0)	 
#define ICP_MULTI_DAC_CSR_BSY	BIT(0)	 
#define ICP_MULTI_DAC_CSR_BI	BIT(4)	 
#define ICP_MULTI_DAC_CSR_RA	BIT(5)	 
#define ICP_MULTI_DAC_CSR_CHAN(x) (((x) & 0x3) << 8)
#define ICP_MULTI_AO		6	 
#define ICP_MULTI_DI		8	 
#define ICP_MULTI_DO		0x0A	 
#define ICP_MULTI_INT_EN	0x0c	 
#define ICP_MULTI_INT_STAT	0x0e	 
#define ICP_MULTI_INT_ADC_RDY	BIT(0)	 
#define ICP_MULTI_INT_DAC_RDY	BIT(1)	 
#define ICP_MULTI_INT_DOUT_ERR	BIT(2)	 
#define ICP_MULTI_INT_DIN_STAT	BIT(3)	 
#define ICP_MULTI_INT_CIE0	BIT(4)	 
#define ICP_MULTI_INT_CIE1	BIT(5)	 
#define ICP_MULTI_INT_CIE2	BIT(6)	 
#define ICP_MULTI_INT_CIE3	BIT(7)	 
#define ICP_MULTI_INT_MASK	0xff	 
#define ICP_MULTI_CNTR0		0x10	 
#define ICP_MULTI_CNTR1		0x12	 
#define ICP_MULTI_CNTR2		0x14	 
#define ICP_MULTI_CNTR3		0x16	 

 
static const struct comedi_lrange icp_multi_ranges = {
	4, {
		UNI_RANGE(5),
		UNI_RANGE(10),
		BIP_RANGE(5),
		BIP_RANGE(10)
	}
};

static const char range_codes_analog[] = { 0x00, 0x20, 0x10, 0x30 };

static int icp_multi_ai_eoc(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    struct comedi_insn *insn,
			    unsigned long context)
{
	unsigned int status;

	status = readw(dev->mmio + ICP_MULTI_ADC_CSR);
	if ((status & ICP_MULTI_ADC_CSR_BSY) == 0)
		return 0;
	return -EBUSY;
}

static int icp_multi_ai_insn_read(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn,
				  unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int range = CR_RANGE(insn->chanspec);
	unsigned int aref = CR_AREF(insn->chanspec);
	unsigned int adc_csr;
	int ret = 0;
	int n;

	 
	if (aref == AREF_DIFF) {
		adc_csr = ICP_MULTI_ADC_CSR_DI_CHAN(chan) |
			  ICP_MULTI_ADC_CSR_DI;
	} else {
		adc_csr = ICP_MULTI_ADC_CSR_SE_CHAN(chan);
	}
	adc_csr |= range_codes_analog[range];
	writew(adc_csr, dev->mmio + ICP_MULTI_ADC_CSR);

	for (n = 0; n < insn->n; n++) {
		 
		writew(adc_csr | ICP_MULTI_ADC_CSR_ST,
		       dev->mmio + ICP_MULTI_ADC_CSR);

		udelay(1);

		 
		ret = comedi_timeout(dev, s, insn, icp_multi_ai_eoc, 0);
		if (ret)
			break;

		data[n] = (readw(dev->mmio + ICP_MULTI_AI) >> 4) & 0x0fff;
	}

	return ret ? ret : n;
}

static int icp_multi_ao_ready(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn,
			      unsigned long context)
{
	unsigned int status;

	status = readw(dev->mmio + ICP_MULTI_DAC_CSR);
	if ((status & ICP_MULTI_DAC_CSR_BSY) == 0)
		return 0;
	return -EBUSY;
}

static int icp_multi_ao_insn_write(struct comedi_device *dev,
				   struct comedi_subdevice *s,
				   struct comedi_insn *insn,
				   unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int range = CR_RANGE(insn->chanspec);
	unsigned int dac_csr;
	int i;

	 
	dac_csr = ICP_MULTI_DAC_CSR_CHAN(chan);
	dac_csr |= range_codes_analog[range];
	writew(dac_csr, dev->mmio + ICP_MULTI_DAC_CSR);

	for (i = 0; i < insn->n; i++) {
		unsigned int val = data[i];
		int ret;

		 
		ret = comedi_timeout(dev, s, insn, icp_multi_ao_ready, 0);
		if (ret)
			return ret;

		writew(val, dev->mmio + ICP_MULTI_AO);

		 
		writew(dac_csr | ICP_MULTI_DAC_CSR_ST,
		       dev->mmio + ICP_MULTI_DAC_CSR);

		s->readback[chan] = val;
	}

	return insn->n;
}

static int icp_multi_di_insn_bits(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn,
				  unsigned int *data)
{
	data[1] = readw(dev->mmio + ICP_MULTI_DI);

	return insn->n;
}

static int icp_multi_do_insn_bits(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn,
				  unsigned int *data)
{
	if (comedi_dio_update_state(s, data))
		writew(s->state, dev->mmio + ICP_MULTI_DO);

	data[1] = s->state;

	return insn->n;
}

static int icp_multi_reset(struct comedi_device *dev)
{
	int i;

	 
	writew(0, dev->mmio + ICP_MULTI_INT_EN);
	writew(ICP_MULTI_INT_MASK, dev->mmio + ICP_MULTI_INT_STAT);

	 
	for (i = 0; i < 4; i++) {
		unsigned int dac_csr = ICP_MULTI_DAC_CSR_CHAN(i);

		 
		writew(dac_csr, dev->mmio + ICP_MULTI_DAC_CSR);

		 
		writew(0, dev->mmio + ICP_MULTI_AO);

		 
		writew(dac_csr | ICP_MULTI_DAC_CSR_ST,
		       dev->mmio + ICP_MULTI_DAC_CSR);
		udelay(1);
	}

	 
	writew(0, dev->mmio + ICP_MULTI_DO);

	return 0;
}

static int icp_multi_auto_attach(struct comedi_device *dev,
				 unsigned long context_unused)
{
	struct pci_dev *pcidev = comedi_to_pci_dev(dev);
	struct comedi_subdevice *s;
	int ret;

	ret = comedi_pci_enable(dev);
	if (ret)
		return ret;

	dev->mmio = pci_ioremap_bar(pcidev, 2);
	if (!dev->mmio)
		return -ENOMEM;

	ret = comedi_alloc_subdevices(dev, 4);
	if (ret)
		return ret;

	icp_multi_reset(dev);

	 
	s = &dev->subdevices[0];
	s->type		= COMEDI_SUBD_AI;
	s->subdev_flags	= SDF_READABLE | SDF_COMMON | SDF_GROUND | SDF_DIFF;
	s->n_chan	= 16;
	s->maxdata	= 0x0fff;
	s->range_table	= &icp_multi_ranges;
	s->insn_read	= icp_multi_ai_insn_read;

	 
	s = &dev->subdevices[1];
	s->type		= COMEDI_SUBD_AO;
	s->subdev_flags	= SDF_WRITABLE | SDF_GROUND | SDF_COMMON;
	s->n_chan	= 4;
	s->maxdata	= 0x0fff;
	s->range_table	= &icp_multi_ranges;
	s->insn_write	= icp_multi_ao_insn_write;

	ret = comedi_alloc_subdev_readback(s);
	if (ret)
		return ret;

	 
	s = &dev->subdevices[2];
	s->type		= COMEDI_SUBD_DI;
	s->subdev_flags	= SDF_READABLE;
	s->n_chan	= 16;
	s->maxdata	= 1;
	s->range_table	= &range_digital;
	s->insn_bits	= icp_multi_di_insn_bits;

	 
	s = &dev->subdevices[3];
	s->type		= COMEDI_SUBD_DO;
	s->subdev_flags	= SDF_WRITABLE;
	s->n_chan	= 8;
	s->maxdata	= 1;
	s->range_table	= &range_digital;
	s->insn_bits	= icp_multi_do_insn_bits;

	return 0;
}

static struct comedi_driver icp_multi_driver = {
	.driver_name	= "icp_multi",
	.module		= THIS_MODULE,
	.auto_attach	= icp_multi_auto_attach,
	.detach		= comedi_pci_detach,
};

static int icp_multi_pci_probe(struct pci_dev *dev,
			       const struct pci_device_id *id)
{
	return comedi_pci_auto_config(dev, &icp_multi_driver, id->driver_data);
}

static const struct pci_device_id icp_multi_pci_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_ICP, 0x8000) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, icp_multi_pci_table);

static struct pci_driver icp_multi_pci_driver = {
	.name		= "icp_multi",
	.id_table	= icp_multi_pci_table,
	.probe		= icp_multi_pci_probe,
	.remove		= comedi_pci_auto_unconfig,
};
module_comedi_pci_driver(icp_multi_driver, icp_multi_pci_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi driver for Inova ICP_MULTI board");
MODULE_LICENSE("GPL");
