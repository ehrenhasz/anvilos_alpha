
 

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/comedi/comedidev.h>
#include <linux/comedi/comedi_isadma.h>

#include "ni_labpc.h"
#include "ni_labpc_regs.h"
#include "ni_labpc_isadma.h"

 
#define LABPC_ISADMA_BUFFER_SIZE	0xff00

 
static unsigned int labpc_suggest_transfer_size(struct comedi_device *dev,
						struct comedi_subdevice *s,
						unsigned int maxbytes)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	unsigned int sample_size = comedi_bytes_per_sample(s);
	unsigned int size;
	unsigned int freq;

	if (cmd->convert_src == TRIG_TIMER)
		freq = 1000000000 / cmd->convert_arg;
	else
		 
		freq = 0xffffffff;

	 
	size = (freq / 3) * sample_size;

	 
	if (size > maxbytes)
		size = maxbytes;
	else if (size < sample_size)
		size = sample_size;

	return size;
}

void labpc_setup_dma(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct labpc_private *devpriv = dev->private;
	struct comedi_isadma_desc *desc = &devpriv->dma->desc[0];
	struct comedi_cmd *cmd = &s->async->cmd;
	unsigned int sample_size = comedi_bytes_per_sample(s);

	 
	desc->size = labpc_suggest_transfer_size(dev, s, desc->maxsize);
	if (cmd->stop_src == TRIG_COUNT &&
	    devpriv->count * sample_size < desc->size)
		desc->size = devpriv->count * sample_size;

	comedi_isadma_program(desc);

	 
	devpriv->cmd3 |= (CMD3_DMAEN | CMD3_DMATCINTEN);
}
EXPORT_SYMBOL_GPL(labpc_setup_dma);

void labpc_drain_dma(struct comedi_device *dev)
{
	struct labpc_private *devpriv = dev->private;
	struct comedi_isadma_desc *desc = &devpriv->dma->desc[0];
	struct comedi_subdevice *s = dev->read_subdev;
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	unsigned int max_samples = comedi_bytes_to_samples(s, desc->size);
	unsigned int residue;
	unsigned int nsamples;
	unsigned int leftover;

	 
	residue = comedi_isadma_disable(desc->chan);

	 
	nsamples = max_samples - comedi_bytes_to_samples(s, residue);
	if (cmd->stop_src == TRIG_COUNT) {
		if (devpriv->count <= nsamples) {
			nsamples = devpriv->count;
			leftover = 0;
		} else {
			leftover = devpriv->count - nsamples;
			if (leftover > max_samples)
				leftover = max_samples;
		}
		devpriv->count -= nsamples;
	} else {
		leftover = max_samples;
	}
	desc->size = comedi_samples_to_bytes(s, leftover);

	comedi_buf_write_samples(s, desc->virt_addr, nsamples);
}
EXPORT_SYMBOL_GPL(labpc_drain_dma);

static void handle_isa_dma(struct comedi_device *dev)
{
	struct labpc_private *devpriv = dev->private;
	struct comedi_isadma_desc *desc = &devpriv->dma->desc[0];

	labpc_drain_dma(dev);

	if (desc->size)
		comedi_isadma_program(desc);

	 
	devpriv->write_byte(dev, 0x1, DMATC_CLEAR_REG);
}

void labpc_handle_dma_status(struct comedi_device *dev)
{
	const struct labpc_boardinfo *board = dev->board_ptr;
	struct labpc_private *devpriv = dev->private;

	 
	if (devpriv->stat1 & STAT1_GATA0 ||
	    (board->is_labpc1200 && devpriv->stat2 & STAT2_OUTA1))
		handle_isa_dma(dev);
}
EXPORT_SYMBOL_GPL(labpc_handle_dma_status);

void labpc_init_dma_chan(struct comedi_device *dev, unsigned int dma_chan)
{
	struct labpc_private *devpriv = dev->private;

	 
	if (dma_chan != 1 && dma_chan != 3)
		return;

	 
	devpriv->dma = comedi_isadma_alloc(dev, 1, dma_chan, dma_chan,
					   LABPC_ISADMA_BUFFER_SIZE,
					   COMEDI_ISADMA_READ);
}
EXPORT_SYMBOL_GPL(labpc_init_dma_chan);

void labpc_free_dma_chan(struct comedi_device *dev)
{
	struct labpc_private *devpriv = dev->private;

	if (devpriv)
		comedi_isadma_free(devpriv->dma);
}
EXPORT_SYMBOL_GPL(labpc_free_dma_chan);

static int __init ni_labpc_isadma_init_module(void)
{
	return 0;
}
module_init(ni_labpc_isadma_init_module);

static void __exit ni_labpc_isadma_cleanup_module(void)
{
}
module_exit(ni_labpc_isadma_cleanup_module);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi NI Lab-PC ISA DMA support");
MODULE_LICENSE("GPL");
