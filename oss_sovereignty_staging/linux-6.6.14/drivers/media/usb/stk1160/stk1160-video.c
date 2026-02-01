
 

#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>

#include "stk1160.h"

static unsigned int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "enable debug messages");

static inline void print_err_status(struct stk1160 *dev,
				     int packet, int status)
{
	char *errmsg = "Unknown";

	switch (status) {
	case -ENOENT:
		errmsg = "unlinked synchronously";
		break;
	case -ECONNRESET:
		errmsg = "unlinked asynchronously";
		break;
	case -ENOSR:
		errmsg = "Buffer error (overrun)";
		break;
	case -EPIPE:
		errmsg = "Stalled (device not responding)";
		break;
	case -EOVERFLOW:
		errmsg = "Babble (bad cable?)";
		break;
	case -EPROTO:
		errmsg = "Bit-stuff error (bad cable?)";
		break;
	case -EILSEQ:
		errmsg = "CRC/Timeout (could be anything)";
		break;
	case -ETIME:
		errmsg = "Device does not respond";
		break;
	}

	if (packet < 0)
		printk_ratelimited(KERN_WARNING "URB status %d [%s].\n",
				status, errmsg);
	else
		printk_ratelimited(KERN_INFO "URB packet %d, status %d [%s].\n",
			       packet, status, errmsg);
}

static inline
struct stk1160_buffer *stk1160_next_buffer(struct stk1160 *dev)
{
	struct stk1160_buffer *buf = NULL;
	unsigned long flags = 0;

	 
	WARN_ON(dev->isoc_ctl.buf);

	spin_lock_irqsave(&dev->buf_lock, flags);
	if (!list_empty(&dev->avail_bufs)) {
		buf = list_first_entry(&dev->avail_bufs,
				struct stk1160_buffer, list);
		list_del(&buf->list);
	}
	spin_unlock_irqrestore(&dev->buf_lock, flags);

	return buf;
}

static inline
void stk1160_buffer_done(struct stk1160 *dev)
{
	struct stk1160_buffer *buf = dev->isoc_ctl.buf;

	buf->vb.sequence = dev->sequence++;
	buf->vb.field = V4L2_FIELD_INTERLACED;
	buf->vb.vb2_buf.timestamp = ktime_get_ns();

	vb2_set_plane_payload(&buf->vb.vb2_buf, 0, buf->bytesused);
	vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);

	dev->isoc_ctl.buf = NULL;
}

static inline
void stk1160_copy_video(struct stk1160 *dev, u8 *src, int len)
{
	int linesdone, lineoff, lencopy;
	int bytesperline = dev->width * 2;
	struct stk1160_buffer *buf = dev->isoc_ctl.buf;
	u8 *dst = buf->mem;
	int remain;

	 

	len -= 4;
	src += 4;

	remain = len;

	linesdone = buf->pos / bytesperline;
	lineoff = buf->pos % bytesperline;  

	if (!buf->odd)
		dst += bytesperline;

	 
	dst += linesdone * bytesperline * 2 + lineoff;

	 
	if (remain < (bytesperline - lineoff))
		lencopy = remain;
	else
		lencopy = bytesperline - lineoff;

	 
	if (lencopy > buf->bytesused - buf->length) {
		lencopy = buf->bytesused - buf->length;
		remain = lencopy;
	}

	 
	if (lencopy == 0 || remain == 0)
		return;

	 
	if (lencopy < 0) {
		stk1160_dbg("copy skipped: negative lencopy\n");
		return;
	}

	if ((unsigned long)dst + lencopy >
		(unsigned long)buf->mem + buf->length) {
		printk_ratelimited(KERN_WARNING "stk1160: buffer overflow detected\n");
		return;
	}

	memcpy(dst, src, lencopy);

	buf->bytesused += lencopy;
	buf->pos += lencopy;
	remain -= lencopy;

	 
	while (remain > 0) {

		dst += lencopy + bytesperline;
		src += lencopy;

		 
		if (remain < bytesperline)
			lencopy = remain;
		else
			lencopy = bytesperline;

		 
		if (lencopy > buf->bytesused - buf->length) {
			lencopy = buf->bytesused - buf->length;
			remain = lencopy;
		}

		 
		if (lencopy == 0 || remain == 0)
			return;

		if (lencopy < 0) {
			printk_ratelimited(KERN_WARNING "stk1160: negative lencopy detected\n");
			return;
		}

		if ((unsigned long)dst + lencopy >
			(unsigned long)buf->mem + buf->length) {
			printk_ratelimited(KERN_WARNING "stk1160: buffer overflow detected\n");
			return;
		}

		memcpy(dst, src, lencopy);
		remain -= lencopy;

		buf->bytesused += lencopy;
		buf->pos += lencopy;
	}
}

 
static void stk1160_process_isoc(struct stk1160 *dev, struct urb *urb)
{
	int i, len, status;
	u8 *p;

	if (!dev) {
		stk1160_warn("%s called with null device\n", __func__);
		return;
	}

	if (urb->status < 0) {
		 
		print_err_status(dev, -1, urb->status);
		return;
	}

	for (i = 0; i < urb->number_of_packets; i++) {
		status = urb->iso_frame_desc[i].status;
		if (status < 0) {
			print_err_status(dev, i, status);
			continue;
		}

		 
		p = urb->transfer_buffer + urb->iso_frame_desc[i].offset;
		len = urb->iso_frame_desc[i].actual_length;

		 
		if (len <= 4)
			continue;

		 
		if (p[0] == 0xc0) {

			 
			if (dev->isoc_ctl.buf != NULL)
				stk1160_buffer_done(dev);

			dev->isoc_ctl.buf = stk1160_next_buffer(dev);
			if (dev->isoc_ctl.buf == NULL)
				return;
		}

		 
		if (dev->isoc_ctl.buf == NULL)
			continue;

		if (p[0] == 0xc0 || p[0] == 0x80) {

			 
			dev->isoc_ctl.buf->odd = *p & 0x40;
			dev->isoc_ctl.buf->pos = 0;
			continue;
		}

		stk1160_copy_video(dev, p, len);
	}
}


 
static void stk1160_isoc_irq(struct urb *urb)
{
	int i, rc;
	struct stk1160_urb *stk_urb = urb->context;
	struct stk1160 *dev = stk_urb->dev;
	struct device *dma_dev = stk1160_get_dmadev(dev);

	switch (urb->status) {
	case 0:
		break;
	case -ECONNRESET:    
	case -ENOENT:
	case -ESHUTDOWN:
		 
		return;
	default:
		stk1160_err("urb error! status %d\n", urb->status);
		return;
	}

	invalidate_kernel_vmap_range(stk_urb->transfer_buffer,
				     urb->transfer_buffer_length);
	dma_sync_sgtable_for_cpu(dma_dev, stk_urb->sgt, DMA_FROM_DEVICE);

	stk1160_process_isoc(dev, urb);

	 
	for (i = 0; i < urb->number_of_packets; i++) {
		urb->iso_frame_desc[i].status = 0;
		urb->iso_frame_desc[i].actual_length = 0;
	}

	dma_sync_sgtable_for_device(dma_dev, stk_urb->sgt, DMA_FROM_DEVICE);
	rc = usb_submit_urb(urb, GFP_ATOMIC);
	if (rc)
		stk1160_err("urb re-submit failed (%d)\n", rc);
}

 
void stk1160_cancel_isoc(struct stk1160 *dev)
{
	int i, num_bufs = dev->isoc_ctl.num_bufs;

	 
	if (!num_bufs)
		return;

	stk1160_dbg("killing %d urbs...\n", num_bufs);

	for (i = 0; i < num_bufs; i++) {

		 
		usb_kill_urb(dev->isoc_ctl.urb_ctl[i].urb);
	}

	stk1160_dbg("all urbs killed\n");
}

static void stk_free_urb(struct stk1160 *dev, struct stk1160_urb *stk_urb)
{
	struct device *dma_dev = stk1160_get_dmadev(dev);

	dma_vunmap_noncontiguous(dma_dev, stk_urb->transfer_buffer);
	dma_free_noncontiguous(dma_dev, stk_urb->urb->transfer_buffer_length,
			       stk_urb->sgt, DMA_FROM_DEVICE);
	usb_free_urb(stk_urb->urb);

	stk_urb->transfer_buffer = NULL;
	stk_urb->sgt = NULL;
	stk_urb->urb = NULL;
	stk_urb->dev = NULL;
	stk_urb->dma = 0;
}

 
void stk1160_free_isoc(struct stk1160 *dev)
{
	int i, num_bufs = dev->isoc_ctl.num_bufs;

	stk1160_dbg("freeing %d urb buffers...\n", num_bufs);

	for (i = 0; i < num_bufs; i++)
		stk_free_urb(dev, &dev->isoc_ctl.urb_ctl[i]);

	dev->isoc_ctl.num_bufs = 0;

	stk1160_dbg("all urb buffers freed\n");
}

 
void stk1160_uninit_isoc(struct stk1160 *dev)
{
	stk1160_cancel_isoc(dev);
	stk1160_free_isoc(dev);
}

static int stk1160_fill_urb(struct stk1160 *dev, struct stk1160_urb *stk_urb,
			    int sb_size, int max_packets)
{
	struct device *dma_dev = stk1160_get_dmadev(dev);

	stk_urb->urb = usb_alloc_urb(max_packets, GFP_KERNEL);
	if (!stk_urb->urb)
		return -ENOMEM;
	stk_urb->sgt = dma_alloc_noncontiguous(dma_dev, sb_size,
					       DMA_FROM_DEVICE, GFP_KERNEL, 0);

	 
	if (!stk_urb->sgt)
		goto free_urb;

	stk_urb->transfer_buffer = dma_vmap_noncontiguous(dma_dev, sb_size,
							  stk_urb->sgt);
	if (!stk_urb->transfer_buffer)
		goto free_sgt;

	stk_urb->dma = stk_urb->sgt->sgl->dma_address;
	stk_urb->dev = dev;
	return 0;
free_sgt:
	dma_free_noncontiguous(dma_dev, sb_size, stk_urb->sgt, DMA_FROM_DEVICE);
	stk_urb->sgt = NULL;
free_urb:
	usb_free_urb(stk_urb->urb);
	stk_urb->urb = NULL;

	return 0;
}
 
int stk1160_alloc_isoc(struct stk1160 *dev)
{
	struct urb *urb;
	int i, j, k, sb_size, max_packets, num_bufs;
	int ret;

	 
	if (dev->isoc_ctl.num_bufs)
		stk1160_uninit_isoc(dev);

	stk1160_dbg("allocating urbs...\n");

	num_bufs = STK1160_NUM_BUFS;
	max_packets = STK1160_NUM_PACKETS;
	sb_size = max_packets * dev->max_pkt_size;

	dev->isoc_ctl.buf = NULL;
	dev->isoc_ctl.max_pkt_size = dev->max_pkt_size;

	 
	for (i = 0; i < num_bufs; i++) {

		ret = stk1160_fill_urb(dev, &dev->isoc_ctl.urb_ctl[i],
				       sb_size, max_packets);
		if (ret)
			goto free_i_bufs;

		urb = dev->isoc_ctl.urb_ctl[i].urb;

		if (!urb) {
			 
			if (i < STK1160_MIN_BUFS)
				goto free_i_bufs;
			goto nomore_tx_bufs;
		}
		memset(dev->isoc_ctl.urb_ctl[i].transfer_buffer, 0, sb_size);

		 
		urb->dev = dev->udev;
		urb->pipe = usb_rcvisocpipe(dev->udev, STK1160_EP_VIDEO);
		urb->transfer_buffer = dev->isoc_ctl.urb_ctl[i].transfer_buffer;
		urb->transfer_buffer_length = sb_size;
		urb->complete = stk1160_isoc_irq;
		urb->context = &dev->isoc_ctl.urb_ctl[i];
		urb->interval = 1;
		urb->start_frame = 0;
		urb->number_of_packets = max_packets;
		urb->transfer_flags = URB_ISO_ASAP | URB_NO_TRANSFER_DMA_MAP;
		urb->transfer_dma = dev->isoc_ctl.urb_ctl[i].dma;

		k = 0;
		for (j = 0; j < max_packets; j++) {
			urb->iso_frame_desc[j].offset = k;
			urb->iso_frame_desc[j].length =
					dev->isoc_ctl.max_pkt_size;
			k += dev->isoc_ctl.max_pkt_size;
		}
	}

	stk1160_dbg("%d urbs allocated\n", num_bufs);

	 
	dev->isoc_ctl.num_bufs = num_bufs;

	return 0;

nomore_tx_bufs:
	 

	stk1160_warn("%d urbs allocated. Trying to continue...\n", i);

	dev->isoc_ctl.num_bufs = i;

	return 0;

free_i_bufs:
	 
	dev->isoc_ctl.num_bufs = i;
	stk1160_free_isoc(dev);
	return -ENOMEM;
}

