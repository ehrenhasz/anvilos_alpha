
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/kdev_t.h>
#include <media/v4l2-ioctl.h>
#include <asm/io.h>
#include "bttvp.h"

 
#define VBI_OFFSET 244

static unsigned int vbibufs = 4;
static unsigned int vbi_debug;

module_param(vbibufs,   int, 0444);
module_param(vbi_debug, int, 0644);
MODULE_PARM_DESC(vbibufs,"number of vbi buffers, range 2-32, default 4");
MODULE_PARM_DESC(vbi_debug,"vbi code debug messages, default is 0 (no)");

#ifdef dprintk
# undef dprintk
#endif
#define dprintk(fmt, ...)						\
do {									\
	if (vbi_debug)							\
		pr_debug("%d: " fmt, btv->c.nr, ##__VA_ARGS__);		\
} while (0)

#define IMAGE_SIZE(fmt) \
	(((fmt)->count[0] + (fmt)->count[1]) * (fmt)->samples_per_line)

 
 

static int queue_setup_vbi(struct vb2_queue *q, unsigned int *num_buffers,
			   unsigned int *num_planes, unsigned int sizes[],
			   struct device *alloc_devs[])
{
	struct bttv *btv = vb2_get_drv_priv(q);
	unsigned int size = IMAGE_SIZE(&btv->vbi_fmt.fmt);

	if (*num_planes)
		return sizes[0] < size ? -EINVAL : 0;
	*num_planes = 1;
	sizes[0] = size;

	return 0;
}

static void buf_queue_vbi(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct vb2_queue *vq = vb->vb2_queue;
	struct bttv *btv = vb2_get_drv_priv(vq);
	struct bttv_buffer *buf = container_of(vbuf, struct bttv_buffer, vbuf);
	unsigned long flags;

	spin_lock_irqsave(&btv->s_lock, flags);
	if (list_empty(&btv->vcapture)) {
		btv->loop_irq = BT848_RISC_VBI;
		if (vb2_is_streaming(&btv->capq))
			btv->loop_irq |= BT848_RISC_VIDEO;
		bttv_set_dma(btv, BT848_CAP_CTL_CAPTURE_VBI_ODD |
			     BT848_CAP_CTL_CAPTURE_VBI_EVEN);
	}
	list_add_tail(&buf->list, &btv->vcapture);
	spin_unlock_irqrestore(&btv->s_lock, flags);
}

static int buf_prepare_vbi(struct vb2_buffer *vb)
{
	int ret = 0;
	struct vb2_queue *vq = vb->vb2_queue;
	struct bttv *btv = vb2_get_drv_priv(vq);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct bttv_buffer *buf = container_of(vbuf, struct bttv_buffer, vbuf);
	unsigned int size = IMAGE_SIZE(&btv->vbi_fmt.fmt);

	if (vb2_plane_size(vb, 0) < size)
		return -EINVAL;
	vb2_set_plane_payload(vb, 0, size);
	buf->vbuf.field = V4L2_FIELD_NONE;
	ret = bttv_buffer_risc_vbi(btv, buf);

	return ret;
}

static void buf_cleanup_vbi(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct bttv_buffer *buf = container_of(vbuf, struct bttv_buffer, vbuf);
	struct vb2_queue *vq = vb->vb2_queue;
	struct bttv *btv = vb2_get_drv_priv(vq);

	btcx_riscmem_free(btv->c.pci, &buf->top);
	btcx_riscmem_free(btv->c.pci, &buf->bottom);
}

static int start_streaming_vbi(struct vb2_queue *q, unsigned int count)
{
	int seqnr = 0;
	struct bttv_buffer *buf;
	struct bttv *btv = vb2_get_drv_priv(q);

	btv->framedrop = 0;
	if (!check_alloc_btres_lock(btv, RESOURCE_VBI)) {
		if (btv->field_count)
			seqnr++;
		while (!list_empty(&btv->vcapture)) {
			buf = list_entry(btv->vcapture.next,
					 struct bttv_buffer, list);
			list_del(&buf->list);
			buf->vbuf.sequence = (btv->field_count >> 1) + seqnr++;
			vb2_buffer_done(&buf->vbuf.vb2_buf,
					VB2_BUF_STATE_QUEUED);
		}
		return -EBUSY;
	}
	if (!vb2_is_streaming(&btv->capq)) {
		init_irqreg(btv);
		btv->field_count = 0;
	}
	return 0;
}

static void stop_streaming_vbi(struct vb2_queue *q)
{
	struct bttv *btv = vb2_get_drv_priv(q);
	unsigned long flags;

	vb2_wait_for_all_buffers(q);
	spin_lock_irqsave(&btv->s_lock, flags);
	free_btres_lock(btv, RESOURCE_VBI);
	if (!vb2_is_streaming(&btv->capq)) {
		 
		btand(~BT848_INT_VSYNC, BT848_INT_MASK);
	}
	spin_unlock_irqrestore(&btv->s_lock, flags);
}

const struct vb2_ops bttv_vbi_qops = {
	.queue_setup    = queue_setup_vbi,
	.buf_queue      = buf_queue_vbi,
	.buf_prepare    = buf_prepare_vbi,
	.buf_cleanup	= buf_cleanup_vbi,
	.start_streaming = start_streaming_vbi,
	.stop_streaming = stop_streaming_vbi,
	.wait_prepare   = vb2_ops_wait_prepare,
	.wait_finish    = vb2_ops_wait_finish,
};

 

static int try_fmt(struct v4l2_vbi_format *f, const struct bttv_tvnorm *tvnorm,
			__s32 crop_start)
{
	__s32 min_start, max_start, max_end, f2_offset;
	unsigned int i;

	 
	min_start = tvnorm->vbistart[0];
	max_start = (crop_start >> 1) - 1;
	max_end = (tvnorm->cropcap.bounds.top
		   + tvnorm->cropcap.bounds.height) >> 1;

	if (min_start > max_start)
		return -EBUSY;

	WARN_ON(max_start >= max_end);

	f->sampling_rate    = tvnorm->Fsc;
	f->samples_per_line = VBI_BPL;
	f->sample_format    = V4L2_PIX_FMT_GREY;
	f->offset           = VBI_OFFSET;

	f2_offset = tvnorm->vbistart[1] - tvnorm->vbistart[0];

	for (i = 0; i < 2; ++i) {
		if (0 == f->count[i]) {
			 
		} else {
			s64 start, count;

			start = clamp(f->start[i], min_start, max_start);
			 
			count = (s64) f->start[i] + f->count[i] - start;
			f->start[i] = start;
			f->count[i] = clamp(count, (s64) 1,
					    max_end - start);
		}

		min_start += f2_offset;
		max_start += f2_offset;
		max_end += f2_offset;
	}

	if (0 == (f->count[0] | f->count[1])) {
		 
		f->start[0] = tvnorm->vbistart[0];
		f->start[1] = tvnorm->vbistart[1];
		f->count[0] = 1;
		f->count[1] = 1;
	}

	f->flags = 0;

	f->reserved[0] = 0;
	f->reserved[1] = 0;

	return 0;
}

int bttv_try_fmt_vbi_cap(struct file *file, void *f, struct v4l2_format *frt)
{
	struct bttv *btv = video_drvdata(file);
	const struct bttv_tvnorm *tvnorm;
	__s32 crop_start;

	mutex_lock(&btv->lock);

	tvnorm = &bttv_tvnorms[btv->tvnorm];
	crop_start = btv->crop_start;

	mutex_unlock(&btv->lock);

	return try_fmt(&frt->fmt.vbi, tvnorm, crop_start);
}


int bttv_s_fmt_vbi_cap(struct file *file, void *f, struct v4l2_format *frt)
{
	struct bttv *btv = video_drvdata(file);
	const struct bttv_tvnorm *tvnorm;
	__s32 start1, end;
	int rc;

	mutex_lock(&btv->lock);

	rc = -EBUSY;
	if (btv->resources & RESOURCE_VBI)
		goto fail;

	tvnorm = &bttv_tvnorms[btv->tvnorm];

	rc = try_fmt(&frt->fmt.vbi, tvnorm, btv->crop_start);
	if (0 != rc)
		goto fail;

	start1 = frt->fmt.vbi.start[1] - tvnorm->vbistart[1] +
		tvnorm->vbistart[0];

	 
	end = max(frt->fmt.vbi.start[0], start1) * 2 + 2;

	btv->vbi_fmt.fmt = frt->fmt.vbi;
	btv->vbi_fmt.tvnorm = tvnorm;
	btv->vbi_fmt.end = end;

	rc = 0;

 fail:
	mutex_unlock(&btv->lock);

	return rc;
}


int bttv_g_fmt_vbi_cap(struct file *file, void *f, struct v4l2_format *frt)
{
	const struct bttv_tvnorm *tvnorm;
	struct bttv *btv = video_drvdata(file);

	frt->fmt.vbi = btv->vbi_fmt.fmt;

	tvnorm = &bttv_tvnorms[btv->tvnorm];

	if (tvnorm != btv->vbi_fmt.tvnorm) {
		__s32 max_end;
		unsigned int i;

		 

		max_end = (tvnorm->cropcap.bounds.top
			   + tvnorm->cropcap.bounds.height) >> 1;

		frt->fmt.vbi.sampling_rate = tvnorm->Fsc;

		for (i = 0; i < 2; ++i) {
			__s32 new_start;

			new_start = frt->fmt.vbi.start[i] + tvnorm->vbistart[i]
				- btv->vbi_fmt.tvnorm->vbistart[i];

			frt->fmt.vbi.start[i] = min(new_start, max_end - 1);
			frt->fmt.vbi.count[i] =
				min((__s32) frt->fmt.vbi.count[i],
					  max_end - frt->fmt.vbi.start[i]);

			max_end += tvnorm->vbistart[1]
				- tvnorm->vbistart[0];
		}
	}
	return 0;
}

void bttv_vbi_fmt_reset(struct bttv_vbi_fmt *f, unsigned int norm)
{
	const struct bttv_tvnorm *tvnorm;
	unsigned int real_samples_per_line;
	unsigned int real_count;

	tvnorm = &bttv_tvnorms[norm];

	f->fmt.sampling_rate    = tvnorm->Fsc;
	f->fmt.samples_per_line = VBI_BPL;
	f->fmt.sample_format    = V4L2_PIX_FMT_GREY;
	f->fmt.offset           = VBI_OFFSET;
	f->fmt.start[0]		= tvnorm->vbistart[0];
	f->fmt.start[1]		= tvnorm->vbistart[1];
	f->fmt.count[0]		= VBI_DEFLINES;
	f->fmt.count[1]		= VBI_DEFLINES;
	f->fmt.flags            = 0;
	f->fmt.reserved[0]      = 0;
	f->fmt.reserved[1]      = 0;

	 
	real_samples_per_line   = 1024 + tvnorm->vbipack * 4;
	real_count              = ((tvnorm->cropcap.defrect.top >> 1)
				   - tvnorm->vbistart[0]);

	WARN_ON(real_samples_per_line > VBI_BPL);
	WARN_ON(real_count > VBI_DEFLINES);

	f->tvnorm               = tvnorm;

	 
	f->end                  = tvnorm->vbistart[0] * 2 + 2;
}
