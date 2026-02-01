
 

 

#include <linux/module.h>
#include <linux/comedi/comedi_pci.h>

 
#define PCI1760_OMB_REG(x)		(0x0c + (x))
#define PCI1760_IMB_REG(x)		(0x1c + (x))
#define PCI1760_INTCSR_REG(x)		(0x38 + (x))
#define PCI1760_INTCSR1_IRQ_ENA		BIT(5)
#define PCI1760_INTCSR2_OMB_IRQ		BIT(0)
#define PCI1760_INTCSR2_IMB_IRQ		BIT(1)
#define PCI1760_INTCSR2_IRQ_STATUS	BIT(6)
#define PCI1760_INTCSR2_IRQ_ASSERTED	BIT(7)

 
#define PCI1760_CMD_CLR_IMB2		0x00	 
#define PCI1760_CMD_SET_DO		0x01	 
#define PCI1760_CMD_GET_DO		0x02	 
#define PCI1760_CMD_GET_STATUS		0x07	 
#define PCI1760_CMD_GET_FW_VER		0x0e	 
#define PCI1760_CMD_GET_HW_VER		0x0f	 
#define PCI1760_CMD_SET_PWM_HI(x)	(0x10 + (x) * 2)  
#define PCI1760_CMD_SET_PWM_LO(x)	(0x11 + (x) * 2)  
#define PCI1760_CMD_SET_PWM_CNT(x)	(0x14 + (x))  
#define PCI1760_CMD_ENA_PWM		0x1f	 
#define PCI1760_CMD_ENA_FILT		0x20	 
#define PCI1760_CMD_ENA_PAT_MATCH	0x21	 
#define PCI1760_CMD_SET_PAT_MATCH	0x22	 
#define PCI1760_CMD_ENA_RISE_EDGE	0x23	 
#define PCI1760_CMD_ENA_FALL_EDGE	0x24	 
#define PCI1760_CMD_ENA_CNT		0x28	 
#define PCI1760_CMD_RST_CNT		0x29	 
#define PCI1760_CMD_ENA_CNT_OFLOW	0x2a	 
#define PCI1760_CMD_ENA_CNT_MATCH	0x2b	 
#define PCI1760_CMD_SET_CNT_EDGE	0x2c	 
#define PCI1760_CMD_GET_CNT		0x2f	 
#define PCI1760_CMD_SET_HI_SAMP(x)	(0x30 + (x))  
#define PCI1760_CMD_SET_LO_SAMP(x)	(0x38 + (x))  
#define PCI1760_CMD_SET_CNT(x)		(0x40 + (x))  
#define PCI1760_CMD_SET_CNT_MATCH(x)	(0x48 + (x))  
#define PCI1760_CMD_GET_INT_FLAGS	0x60	 
#define PCI1760_CMD_GET_INT_FLAGS_MATCH	BIT(0)
#define PCI1760_CMD_GET_INT_FLAGS_COS	BIT(1)
#define PCI1760_CMD_GET_INT_FLAGS_OFLOW	BIT(2)
#define PCI1760_CMD_GET_OS		0x61	 
#define PCI1760_CMD_GET_CNT_STATUS	0x62	 

#define PCI1760_CMD_TIMEOUT		250	 
#define PCI1760_CMD_RETRIES		3	 

#define PCI1760_PWM_TIMEBASE		100000	 

static int pci1760_send_cmd(struct comedi_device *dev,
			    unsigned char cmd, unsigned short val)
{
	unsigned long timeout;

	 
	outb(val & 0xff, dev->iobase + PCI1760_OMB_REG(0));
	outb((val >> 8) & 0xff, dev->iobase + PCI1760_OMB_REG(1));
	outb(cmd, dev->iobase + PCI1760_OMB_REG(2));
	outb(0, dev->iobase + PCI1760_OMB_REG(3));

	 
	timeout = jiffies + usecs_to_jiffies(PCI1760_CMD_TIMEOUT);
	do {
		if (inb(dev->iobase + PCI1760_IMB_REG(2)) == cmd) {
			 
			return inb(dev->iobase + PCI1760_IMB_REG(0)) |
			       (inb(dev->iobase + PCI1760_IMB_REG(1)) << 8);
		}
		cpu_relax();
	} while (time_before(jiffies, timeout));

	return -EBUSY;
}

static int pci1760_cmd(struct comedi_device *dev,
		       unsigned char cmd, unsigned short val)
{
	int repeats;
	int ret;

	 
	if (inb(dev->iobase + PCI1760_IMB_REG(2)) == cmd) {
		ret = pci1760_send_cmd(dev, PCI1760_CMD_CLR_IMB2, 0);
		if (ret < 0) {
			 
			ret = pci1760_send_cmd(dev, PCI1760_CMD_CLR_IMB2, 0);
			if (ret < 0)
				return -ETIMEDOUT;
		}
	}

	 
	for (repeats = 0; repeats < PCI1760_CMD_RETRIES; repeats++) {
		ret = pci1760_send_cmd(dev, cmd, val);
		if (ret >= 0)
			return ret;
	}

	 
	return -ETIMEDOUT;
}

static int pci1760_di_insn_bits(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn,
				unsigned int *data)
{
	data[1] = inb(dev->iobase + PCI1760_IMB_REG(3));

	return insn->n;
}

static int pci1760_do_insn_bits(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn,
				unsigned int *data)
{
	int ret;

	if (comedi_dio_update_state(s, data)) {
		ret = pci1760_cmd(dev, PCI1760_CMD_SET_DO, s->state);
		if (ret < 0)
			return ret;
	}

	data[1] = s->state;

	return insn->n;
}

static int pci1760_pwm_ns_to_div(unsigned int flags, unsigned int ns)
{
	unsigned int divisor;

	switch (flags) {
	case CMDF_ROUND_NEAREST:
		divisor = DIV_ROUND_CLOSEST(ns, PCI1760_PWM_TIMEBASE);
		break;
	case CMDF_ROUND_UP:
		divisor = DIV_ROUND_UP(ns, PCI1760_PWM_TIMEBASE);
		break;
	case CMDF_ROUND_DOWN:
		divisor = ns / PCI1760_PWM_TIMEBASE;
		break;
	default:
		return -EINVAL;
	}

	if (divisor < 1)
		divisor = 1;
	if (divisor > 0xffff)
		divisor = 0xffff;

	return divisor;
}

static int pci1760_pwm_enable(struct comedi_device *dev,
			      unsigned int chan, bool enable)
{
	int ret;

	ret = pci1760_cmd(dev, PCI1760_CMD_GET_STATUS, PCI1760_CMD_ENA_PWM);
	if (ret < 0)
		return ret;

	if (enable)
		ret |= BIT(chan);
	else
		ret &= ~BIT(chan);

	return pci1760_cmd(dev, PCI1760_CMD_ENA_PWM, ret);
}

static int pci1760_pwm_insn_config(struct comedi_device *dev,
				   struct comedi_subdevice *s,
				   struct comedi_insn *insn,
				   unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	int hi_div;
	int lo_div;
	int ret;

	switch (data[0]) {
	case INSN_CONFIG_ARM:
		ret = pci1760_pwm_enable(dev, chan, false);
		if (ret < 0)
			return ret;

		if (data[1] > 0xffff)
			return -EINVAL;
		ret = pci1760_cmd(dev, PCI1760_CMD_SET_PWM_CNT(chan), data[1]);
		if (ret < 0)
			return ret;

		ret = pci1760_pwm_enable(dev, chan, true);
		if (ret < 0)
			return ret;
		break;
	case INSN_CONFIG_DISARM:
		ret = pci1760_pwm_enable(dev, chan, false);
		if (ret < 0)
			return ret;
		break;
	case INSN_CONFIG_PWM_OUTPUT:
		ret = pci1760_pwm_enable(dev, chan, false);
		if (ret < 0)
			return ret;

		hi_div = pci1760_pwm_ns_to_div(data[1], data[2]);
		lo_div = pci1760_pwm_ns_to_div(data[3], data[4]);
		if (hi_div < 0 || lo_div < 0)
			return -EINVAL;
		if ((hi_div * PCI1760_PWM_TIMEBASE) != data[2] ||
		    (lo_div * PCI1760_PWM_TIMEBASE) != data[4]) {
			data[2] = hi_div * PCI1760_PWM_TIMEBASE;
			data[4] = lo_div * PCI1760_PWM_TIMEBASE;
			return -EAGAIN;
		}
		ret = pci1760_cmd(dev, PCI1760_CMD_SET_PWM_HI(chan), hi_div);
		if (ret < 0)
			return ret;
		ret = pci1760_cmd(dev, PCI1760_CMD_SET_PWM_LO(chan), lo_div);
		if (ret < 0)
			return ret;
		break;
	case INSN_CONFIG_GET_PWM_OUTPUT:
		hi_div = pci1760_cmd(dev, PCI1760_CMD_GET_STATUS,
				     PCI1760_CMD_SET_PWM_HI(chan));
		lo_div = pci1760_cmd(dev, PCI1760_CMD_GET_STATUS,
				     PCI1760_CMD_SET_PWM_LO(chan));
		if (hi_div < 0 || lo_div < 0)
			return -ETIMEDOUT;

		data[1] = hi_div * PCI1760_PWM_TIMEBASE;
		data[2] = lo_div * PCI1760_PWM_TIMEBASE;
		break;
	case INSN_CONFIG_GET_PWM_STATUS:
		ret = pci1760_cmd(dev, PCI1760_CMD_GET_STATUS,
				  PCI1760_CMD_ENA_PWM);
		if (ret < 0)
			return ret;

		data[1] = (ret & BIT(chan)) ? 1 : 0;
		break;
	default:
		return -EINVAL;
	}

	return insn->n;
}

static void pci1760_reset(struct comedi_device *dev)
{
	int i;

	 
	outb(0, dev->iobase + PCI1760_INTCSR_REG(0));
	outb(0, dev->iobase + PCI1760_INTCSR_REG(1));
	outb(0, dev->iobase + PCI1760_INTCSR_REG(3));

	 
	pci1760_cmd(dev, PCI1760_CMD_ENA_CNT, 0);

	 
	pci1760_cmd(dev, PCI1760_CMD_ENA_CNT_OFLOW, 0);

	 
	pci1760_cmd(dev, PCI1760_CMD_ENA_CNT_MATCH, 0);

	 
	for (i = 0; i < 8; i++) {
		pci1760_cmd(dev, PCI1760_CMD_SET_CNT_MATCH(i), 0x8000);
		pci1760_cmd(dev, PCI1760_CMD_SET_CNT(i), 0x0000);
	}

	 
	pci1760_cmd(dev, PCI1760_CMD_RST_CNT, 0xff);

	 
	pci1760_cmd(dev, PCI1760_CMD_SET_CNT_EDGE, 0);

	 
	pci1760_cmd(dev, PCI1760_CMD_ENA_FILT, 0);

	 
	pci1760_cmd(dev, PCI1760_CMD_ENA_PAT_MATCH, 0);

	 
	pci1760_cmd(dev, PCI1760_CMD_SET_PAT_MATCH, 0);
}

static int pci1760_auto_attach(struct comedi_device *dev,
			       unsigned long context)
{
	struct pci_dev *pcidev = comedi_to_pci_dev(dev);
	struct comedi_subdevice *s;
	int ret;

	ret = comedi_pci_enable(dev);
	if (ret)
		return ret;
	dev->iobase = pci_resource_start(pcidev, 0);

	pci1760_reset(dev);

	ret = comedi_alloc_subdevices(dev, 4);
	if (ret)
		return ret;

	 
	s = &dev->subdevices[0];
	s->type		= COMEDI_SUBD_DI;
	s->subdev_flags	= SDF_READABLE;
	s->n_chan	= 8;
	s->maxdata	= 1;
	s->range_table	= &range_digital;
	s->insn_bits	= pci1760_di_insn_bits;

	 
	s = &dev->subdevices[1];
	s->type		= COMEDI_SUBD_DO;
	s->subdev_flags	= SDF_WRITABLE;
	s->n_chan	= 8;
	s->maxdata	= 1;
	s->range_table	= &range_digital;
	s->insn_bits	= pci1760_do_insn_bits;

	 
	ret = pci1760_cmd(dev, PCI1760_CMD_GET_DO, 0);
	if (ret < 0)
		return ret;
	s->state	= ret;

	 
	s = &dev->subdevices[2];
	s->type		= COMEDI_SUBD_PWM;
	s->subdev_flags	= SDF_PWM_COUNTER;
	s->n_chan	= 2;
	s->insn_config	= pci1760_pwm_insn_config;

	 
	s = &dev->subdevices[3];
	s->type		= COMEDI_SUBD_UNUSED;

	return 0;
}

static struct comedi_driver pci1760_driver = {
	.driver_name	= "adv_pci1760",
	.module		= THIS_MODULE,
	.auto_attach	= pci1760_auto_attach,
	.detach		= comedi_pci_detach,
};

static int pci1760_pci_probe(struct pci_dev *dev,
			     const struct pci_device_id *id)
{
	return comedi_pci_auto_config(dev, &pci1760_driver, id->driver_data);
}

static const struct pci_device_id pci1760_pci_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1760) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, pci1760_pci_table);

static struct pci_driver pci1760_pci_driver = {
	.name		= "adv_pci1760",
	.id_table	= pci1760_pci_table,
	.probe		= pci1760_pci_probe,
	.remove		= comedi_pci_auto_unconfig,
};
module_comedi_pci_driver(pci1760_driver, pci1760_pci_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi driver for Advantech PCI-1760");
MODULE_LICENSE("GPL");
