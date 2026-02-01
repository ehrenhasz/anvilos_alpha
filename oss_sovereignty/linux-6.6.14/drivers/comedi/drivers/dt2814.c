
 
 

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/comedi/comedidev.h>
#include <linux/delay.h>

#define DT2814_CSR 0
#define DT2814_DATA 1

 

#define DT2814_FINISH 0x80
#define DT2814_ERR 0x40
#define DT2814_BUSY 0x20
#define DT2814_ENB 0x10
#define DT2814_CHANMASK 0x0f

#define DT2814_TIMEOUT 10
#define DT2814_MAX_SPEED 100000	 

static int dt2814_ai_notbusy(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn,
			     unsigned long context)
{
	unsigned int status;

	status = inb(dev->iobase + DT2814_CSR);
	if (context)
		*(unsigned int *)context = status;
	if (status & DT2814_BUSY)
		return -EBUSY;
	return 0;
}

static int dt2814_ai_clear(struct comedi_device *dev)
{
	unsigned int status = 0;
	int ret;

	 
	ret = comedi_timeout(dev, NULL, NULL, dt2814_ai_notbusy,
			     (unsigned long)&status);
	if (ret)
		return ret;

	if (status & (DT2814_FINISH | DT2814_ERR)) {
		 
		inb(dev->iobase + DT2814_DATA);
		inb(dev->iobase + DT2814_DATA);
	}
	return 0;
}

static int dt2814_ai_eoc(struct comedi_device *dev,
			 struct comedi_subdevice *s,
			 struct comedi_insn *insn,
			 unsigned long context)
{
	unsigned int status;

	status = inb(dev->iobase + DT2814_CSR);
	if (status & DT2814_FINISH)
		return 0;
	return -EBUSY;
}

static int dt2814_ai_insn_read(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	int n, hi, lo;
	int chan;
	int ret;

	dt2814_ai_clear(dev);	 
	for (n = 0; n < insn->n; n++) {
		chan = CR_CHAN(insn->chanspec);

		outb(chan, dev->iobase + DT2814_CSR);

		ret = comedi_timeout(dev, s, insn, dt2814_ai_eoc, 0);
		if (ret)
			return ret;

		hi = inb(dev->iobase + DT2814_DATA);
		lo = inb(dev->iobase + DT2814_DATA);

		data[n] = (hi << 4) | (lo >> 4);
	}

	return n;
}

static int dt2814_ns_to_timer(unsigned int *ns, unsigned int flags)
{
	int i;
	unsigned int f;

	 

	f = 10000;		 
	for (i = 0; i < 8; i++) {
		if ((2 * (*ns)) < (f * 11))
			break;
		f *= 10;
	}

	*ns = f;

	return i;
}

static int dt2814_ai_cmdtest(struct comedi_device *dev,
			     struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	int err = 0;
	unsigned int arg;

	 

	err |= comedi_check_trigger_src(&cmd->start_src, TRIG_NOW);
	err |= comedi_check_trigger_src(&cmd->scan_begin_src, TRIG_TIMER);
	err |= comedi_check_trigger_src(&cmd->convert_src, TRIG_NOW);
	err |= comedi_check_trigger_src(&cmd->scan_end_src, TRIG_COUNT);
	err |= comedi_check_trigger_src(&cmd->stop_src, TRIG_COUNT | TRIG_NONE);

	if (err)
		return 1;

	 

	err |= comedi_check_trigger_is_unique(cmd->stop_src);

	 

	if (err)
		return 2;

	 

	err |= comedi_check_trigger_arg_is(&cmd->start_arg, 0);

	err |= comedi_check_trigger_arg_max(&cmd->scan_begin_arg, 1000000000);
	err |= comedi_check_trigger_arg_min(&cmd->scan_begin_arg,
					    DT2814_MAX_SPEED);

	err |= comedi_check_trigger_arg_is(&cmd->scan_end_arg,
					   cmd->chanlist_len);

	if (cmd->stop_src == TRIG_COUNT)
		err |= comedi_check_trigger_arg_min(&cmd->stop_arg, 2);
	else	 
		err |= comedi_check_trigger_arg_is(&cmd->stop_arg, 0);

	if (err)
		return 3;

	 

	arg = cmd->scan_begin_arg;
	dt2814_ns_to_timer(&arg, cmd->flags);
	err |= comedi_check_trigger_arg_is(&cmd->scan_begin_arg, arg);

	if (err)
		return 4;

	return 0;
}

static int dt2814_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	int chan;
	int trigvar;

	dt2814_ai_clear(dev);	 
	trigvar = dt2814_ns_to_timer(&cmd->scan_begin_arg, cmd->flags);

	chan = CR_CHAN(cmd->chanlist[0]);

	outb(chan | DT2814_ENB | (trigvar << 5), dev->iobase + DT2814_CSR);

	return 0;
}

static int dt2814_ai_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	unsigned int status;
	unsigned long flags;

	spin_lock_irqsave(&dev->spinlock, flags);
	status = inb(dev->iobase + DT2814_CSR);
	if (status & DT2814_ENB) {
		 
		outb(status & DT2814_CHANMASK, dev->iobase + DT2814_CSR);
	}
	spin_unlock_irqrestore(&dev->spinlock, flags);
	return 0;
}

static irqreturn_t dt2814_interrupt(int irq, void *d)
{
	struct comedi_device *dev = d;
	struct comedi_subdevice *s = dev->read_subdev;
	struct comedi_async *async;
	unsigned int lo, hi;
	unsigned short data;
	unsigned int status;

	if (!dev->attached) {
		dev_err(dev->class_dev, "spurious interrupt\n");
		return IRQ_HANDLED;
	}

	async = s->async;

	spin_lock(&dev->spinlock);

	status = inb(dev->iobase + DT2814_CSR);
	if (!(status & DT2814_ENB)) {
		 
		spin_unlock(&dev->spinlock);
		return IRQ_HANDLED;
	}

	if (!(status & (DT2814_FINISH | DT2814_ERR))) {
		 
		spin_unlock(&dev->spinlock);
		return IRQ_HANDLED;
	}

	 
	hi = inb(dev->iobase + DT2814_DATA);
	lo = inb(dev->iobase + DT2814_DATA);

	data = (hi << 4) | (lo >> 4);

	if (status & DT2814_ERR) {
		async->events |= COMEDI_CB_ERROR;
	} else {
		comedi_buf_write_samples(s, &data, 1);
		if (async->cmd.stop_src == TRIG_COUNT &&
		    async->scans_done >=  async->cmd.stop_arg) {
			async->events |= COMEDI_CB_EOA;
		}
	}
	if (async->events & COMEDI_CB_CANCEL_MASK) {
		 
		outb(status & DT2814_CHANMASK, dev->iobase + DT2814_CSR);
	}

	spin_unlock(&dev->spinlock);

	comedi_handle_events(dev, s);
	return IRQ_HANDLED;
}

static int dt2814_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	int ret;

	ret = comedi_request_region(dev, it->options[0], 0x2);
	if (ret)
		return ret;

	outb(0, dev->iobase + DT2814_CSR);
	if (dt2814_ai_clear(dev)) {
		dev_err(dev->class_dev, "reset error (fatal)\n");
		return -EIO;
	}

	if (it->options[1]) {
		ret = request_irq(it->options[1], dt2814_interrupt, 0,
				  dev->board_name, dev);
		if (ret == 0)
			dev->irq = it->options[1];
	}

	ret = comedi_alloc_subdevices(dev, 1);
	if (ret)
		return ret;

	s = &dev->subdevices[0];
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags = SDF_READABLE | SDF_GROUND;
	s->n_chan = 16;		 
	s->insn_read = dt2814_ai_insn_read;
	s->maxdata = 0xfff;
	s->range_table = &range_unknown;	 
	if (dev->irq) {
		dev->read_subdev = s;
		s->subdev_flags |= SDF_CMD_READ;
		s->len_chanlist = 1;
		s->do_cmd = dt2814_ai_cmd;
		s->do_cmdtest = dt2814_ai_cmdtest;
		s->cancel = dt2814_ai_cancel;
	}

	return 0;
}

static void dt2814_detach(struct comedi_device *dev)
{
	if (dev->irq) {
		 
		dt2814_ai_clear(dev);
	}
	comedi_legacy_detach(dev);
}

static struct comedi_driver dt2814_driver = {
	.driver_name	= "dt2814",
	.module		= THIS_MODULE,
	.attach		= dt2814_attach,
	.detach		= dt2814_detach,
};
module_comedi_driver(dt2814_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
