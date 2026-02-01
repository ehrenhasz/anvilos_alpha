
 

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/video.h>
#include <asm/unaligned.h>

#include <media/v4l2-dev.h>

#include "uvc.h"
#include "uvc_queue.h"
#include "uvc_video.h"

 

static int
uvc_video_encode_header(struct uvc_video *video, struct uvc_buffer *buf,
		u8 *data, int len)
{
	struct uvc_device *uvc = container_of(video, struct uvc_device, video);
	struct usb_composite_dev *cdev = uvc->func.config->cdev;
	struct timespec64 ts = ns_to_timespec64(buf->buf.vb2_buf.timestamp);
	int pos = 2;

	data[1] = UVC_STREAM_EOH | video->fid;

	if (video->queue.buf_used == 0 && ts.tv_sec) {
		 
		u32 pts = ((u64)ts.tv_sec * USEC_PER_SEC + ts.tv_nsec / NSEC_PER_USEC) * 48;

		data[1] |= UVC_STREAM_PTS;
		put_unaligned_le32(pts, &data[pos]);
		pos += 4;
	}

	if (cdev->gadget->ops->get_frame) {
		u32 sof, stc;

		sof = usb_gadget_frame_number(cdev->gadget);
		ktime_get_ts64(&ts);
		stc = ((u64)ts.tv_sec * USEC_PER_SEC + ts.tv_nsec / NSEC_PER_USEC) * 48;

		data[1] |= UVC_STREAM_SCR;
		put_unaligned_le32(stc, &data[pos]);
		put_unaligned_le16(sof, &data[pos+4]);
		pos += 6;
	}

	data[0] = pos;

	if (buf->bytesused - video->queue.buf_used <= len - pos)
		data[1] |= UVC_STREAM_EOF;

	return pos;
}

static int
uvc_video_encode_data(struct uvc_video *video, struct uvc_buffer *buf,
		u8 *data, int len)
{
	struct uvc_video_queue *queue = &video->queue;
	unsigned int nbytes;
	void *mem;

	 
	mem = buf->mem + queue->buf_used;
	nbytes = min((unsigned int)len, buf->bytesused - queue->buf_used);

	memcpy(data, mem, nbytes);
	queue->buf_used += nbytes;

	return nbytes;
}

static void
uvc_video_encode_bulk(struct usb_request *req, struct uvc_video *video,
		struct uvc_buffer *buf)
{
	void *mem = req->buf;
	struct uvc_request *ureq = req->context;
	int len = video->req_size;
	int ret;

	 
	if (video->payload_size == 0) {
		ret = uvc_video_encode_header(video, buf, mem, len);
		video->payload_size += ret;
		mem += ret;
		len -= ret;
	}

	 
	len = min((int)(video->max_payload_size - video->payload_size), len);
	ret = uvc_video_encode_data(video, buf, mem, len);

	video->payload_size += ret;
	len -= ret;

	req->length = video->req_size - len;
	req->zero = video->payload_size == video->max_payload_size;

	if (buf->bytesused == video->queue.buf_used) {
		video->queue.buf_used = 0;
		buf->state = UVC_BUF_STATE_DONE;
		list_del(&buf->queue);
		video->fid ^= UVC_STREAM_FID;
		ureq->last_buf = buf;

		video->payload_size = 0;
	}

	if (video->payload_size == video->max_payload_size ||
	    video->queue.flags & UVC_QUEUE_DROP_INCOMPLETE ||
	    buf->bytesused == video->queue.buf_used)
		video->payload_size = 0;
}

static void
uvc_video_encode_isoc_sg(struct usb_request *req, struct uvc_video *video,
		struct uvc_buffer *buf)
{
	unsigned int pending = buf->bytesused - video->queue.buf_used;
	struct uvc_request *ureq = req->context;
	struct scatterlist *sg, *iter;
	unsigned int len = video->req_size;
	unsigned int sg_left, part = 0;
	unsigned int i;
	int header_len;

	sg = ureq->sgt.sgl;
	sg_init_table(sg, ureq->sgt.nents);

	 
	header_len = uvc_video_encode_header(video, buf, ureq->header,
				      video->req_size);
	sg_set_buf(sg, ureq->header, header_len);
	len -= header_len;

	if (pending <= len)
		len = pending;

	req->length = (len == pending) ?
		len + header_len : video->req_size;

	 
	sg = sg_next(sg);

	for_each_sg(sg, iter, ureq->sgt.nents - 1, i) {
		if (!len || !buf->sg || !buf->sg->length)
			break;

		sg_left = buf->sg->length - buf->offset;
		part = min_t(unsigned int, len, sg_left);

		sg_set_page(iter, sg_page(buf->sg), part, buf->offset);

		if (part == sg_left) {
			buf->offset = 0;
			buf->sg = sg_next(buf->sg);
		} else {
			buf->offset += part;
		}
		len -= part;
	}

	 
	req->buf = NULL;
	req->sg	= ureq->sgt.sgl;
	req->num_sgs = i + 1;

	req->length -= len;
	video->queue.buf_used += req->length - header_len;

	if (buf->bytesused == video->queue.buf_used || !buf->sg ||
			video->queue.flags & UVC_QUEUE_DROP_INCOMPLETE) {
		video->queue.buf_used = 0;
		buf->state = UVC_BUF_STATE_DONE;
		buf->offset = 0;
		list_del(&buf->queue);
		video->fid ^= UVC_STREAM_FID;
		ureq->last_buf = buf;
	}
}

static void
uvc_video_encode_isoc(struct usb_request *req, struct uvc_video *video,
		struct uvc_buffer *buf)
{
	void *mem = req->buf;
	struct uvc_request *ureq = req->context;
	int len = video->req_size;
	int ret;

	 
	ret = uvc_video_encode_header(video, buf, mem, len);
	mem += ret;
	len -= ret;

	 
	ret = uvc_video_encode_data(video, buf, mem, len);
	len -= ret;

	req->length = video->req_size - len;

	if (buf->bytesused == video->queue.buf_used ||
			video->queue.flags & UVC_QUEUE_DROP_INCOMPLETE) {
		video->queue.buf_used = 0;
		buf->state = UVC_BUF_STATE_DONE;
		list_del(&buf->queue);
		video->fid ^= UVC_STREAM_FID;
		ureq->last_buf = buf;
	}
}

 

static int uvcg_video_ep_queue(struct uvc_video *video, struct usb_request *req)
{
	int ret;

	ret = usb_ep_queue(video->ep, req, GFP_ATOMIC);
	if (ret < 0) {
		uvcg_err(&video->uvc->func, "Failed to queue request (%d).\n",
			 ret);

		 
		if (video->ep->desc) {
			 
			if (usb_endpoint_xfer_bulk(video->ep->desc))
				usb_ep_set_halt(video->ep);
		}
	}

	return ret;
}

static void
uvc_video_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct uvc_request *ureq = req->context;
	struct uvc_video *video = ureq->video;
	struct uvc_video_queue *queue = &video->queue;
	struct uvc_device *uvc = video->uvc;
	unsigned long flags;

	switch (req->status) {
	case 0:
		break;

	case -EXDEV:
		uvcg_dbg(&video->uvc->func, "VS request missed xfer.\n");
		queue->flags |= UVC_QUEUE_DROP_INCOMPLETE;
		break;

	case -ESHUTDOWN:	 
		uvcg_dbg(&video->uvc->func, "VS request cancelled.\n");
		uvcg_queue_cancel(queue, 1);
		break;

	default:
		uvcg_warn(&video->uvc->func,
			  "VS request completed with status %d.\n",
			  req->status);
		uvcg_queue_cancel(queue, 0);
	}

	if (ureq->last_buf) {
		uvcg_complete_buffer(&video->queue, ureq->last_buf);
		ureq->last_buf = NULL;
	}

	spin_lock_irqsave(&video->req_lock, flags);
	list_add_tail(&req->list, &video->req_free);
	spin_unlock_irqrestore(&video->req_lock, flags);

	if (uvc->state == UVC_STATE_STREAMING)
		queue_work(video->async_wq, &video->pump);
}

static int
uvc_video_free_requests(struct uvc_video *video)
{
	unsigned int i;

	if (video->ureq) {
		for (i = 0; i < video->uvc_num_requests; ++i) {
			sg_free_table(&video->ureq[i].sgt);

			if (video->ureq[i].req) {
				usb_ep_free_request(video->ep, video->ureq[i].req);
				video->ureq[i].req = NULL;
			}

			if (video->ureq[i].req_buffer) {
				kfree(video->ureq[i].req_buffer);
				video->ureq[i].req_buffer = NULL;
			}
		}

		kfree(video->ureq);
		video->ureq = NULL;
	}

	INIT_LIST_HEAD(&video->req_free);
	video->req_size = 0;
	return 0;
}

static int
uvc_video_alloc_requests(struct uvc_video *video)
{
	unsigned int req_size;
	unsigned int i;
	int ret = -ENOMEM;

	BUG_ON(video->req_size);

	req_size = video->ep->maxpacket
		 * max_t(unsigned int, video->ep->maxburst, 1)
		 * (video->ep->mult);

	video->ureq = kcalloc(video->uvc_num_requests, sizeof(struct uvc_request), GFP_KERNEL);
	if (video->ureq == NULL)
		return -ENOMEM;

	for (i = 0; i < video->uvc_num_requests; ++i) {
		video->ureq[i].req_buffer = kmalloc(req_size, GFP_KERNEL);
		if (video->ureq[i].req_buffer == NULL)
			goto error;

		video->ureq[i].req = usb_ep_alloc_request(video->ep, GFP_KERNEL);
		if (video->ureq[i].req == NULL)
			goto error;

		video->ureq[i].req->buf = video->ureq[i].req_buffer;
		video->ureq[i].req->length = 0;
		video->ureq[i].req->complete = uvc_video_complete;
		video->ureq[i].req->context = &video->ureq[i];
		video->ureq[i].video = video;
		video->ureq[i].last_buf = NULL;

		list_add_tail(&video->ureq[i].req->list, &video->req_free);
		 
		sg_alloc_table(&video->ureq[i].sgt,
			       DIV_ROUND_UP(req_size - UVCG_REQUEST_HEADER_LEN,
					    PAGE_SIZE) + 2, GFP_KERNEL);
	}

	video->req_size = req_size;

	return 0;

error:
	uvc_video_free_requests(video);
	return ret;
}

 

 
static void uvcg_video_pump(struct work_struct *work)
{
	struct uvc_video *video = container_of(work, struct uvc_video, pump);
	struct uvc_video_queue *queue = &video->queue;
	 
	bool is_bulk = video->max_payload_size;
	struct usb_request *req = NULL;
	struct uvc_buffer *buf;
	unsigned long flags;
	bool buf_done;
	int ret;

	while (video->ep->enabled) {
		 
		spin_lock_irqsave(&video->req_lock, flags);
		if (list_empty(&video->req_free)) {
			spin_unlock_irqrestore(&video->req_lock, flags);
			return;
		}
		req = list_first_entry(&video->req_free, struct usb_request,
					list);
		list_del(&req->list);
		spin_unlock_irqrestore(&video->req_lock, flags);

		 
		spin_lock_irqsave(&queue->irqlock, flags);
		buf = uvcg_queue_head(queue);

		if (buf != NULL) {
			video->encode(req, video, buf);
			buf_done = buf->state == UVC_BUF_STATE_DONE;
		} else if (!(queue->flags & UVC_QUEUE_DISCONNECTED) && !is_bulk) {
			 
			req->length = 0;
			buf_done = false;
		} else {
			 
			spin_unlock_irqrestore(&queue->irqlock, flags);
			break;
		}

		 
		if (list_empty(&video->req_free) || buf_done ||
		    !(video->req_int_count %
		       DIV_ROUND_UP(video->uvc_num_requests, 4))) {
			video->req_int_count = 0;
			req->no_interrupt = 0;
		} else {
			req->no_interrupt = 1;
		}

		 
		ret = uvcg_video_ep_queue(video, req);
		spin_unlock_irqrestore(&queue->irqlock, flags);

		if (ret < 0) {
			uvcg_queue_cancel(queue, 0);
			break;
		}

		 
		req = NULL;
		video->req_int_count++;
	}

	if (!req)
		return;

	spin_lock_irqsave(&video->req_lock, flags);
	list_add_tail(&req->list, &video->req_free);
	spin_unlock_irqrestore(&video->req_lock, flags);
	return;
}

 
int uvcg_video_enable(struct uvc_video *video, int enable)
{
	unsigned int i;
	int ret;

	if (video->ep == NULL) {
		uvcg_info(&video->uvc->func,
			  "Video enable failed, device is uninitialized.\n");
		return -ENODEV;
	}

	if (!enable) {
		cancel_work_sync(&video->pump);
		uvcg_queue_cancel(&video->queue, 0);

		for (i = 0; i < video->uvc_num_requests; ++i)
			if (video->ureq && video->ureq[i].req)
				usb_ep_dequeue(video->ep, video->ureq[i].req);

		uvc_video_free_requests(video);
		uvcg_queue_enable(&video->queue, 0);
		return 0;
	}

	if ((ret = uvcg_queue_enable(&video->queue, 1)) < 0)
		return ret;

	if ((ret = uvc_video_alloc_requests(video)) < 0)
		return ret;

	if (video->max_payload_size) {
		video->encode = uvc_video_encode_bulk;
		video->payload_size = 0;
	} else
		video->encode = video->queue.use_sg ?
			uvc_video_encode_isoc_sg : uvc_video_encode_isoc;

	video->req_int_count = 0;

	queue_work(video->async_wq, &video->pump);

	return ret;
}

 
int uvcg_video_init(struct uvc_video *video, struct uvc_device *uvc)
{
	INIT_LIST_HEAD(&video->req_free);
	spin_lock_init(&video->req_lock);
	INIT_WORK(&video->pump, uvcg_video_pump);

	 
	video->async_wq = alloc_workqueue("uvcgadget", WQ_UNBOUND | WQ_HIGHPRI, 0);
	if (!video->async_wq)
		return -EINVAL;

	video->uvc = uvc;
	video->fcc = V4L2_PIX_FMT_YUYV;
	video->bpp = 16;
	video->width = 320;
	video->height = 240;
	video->imagesize = 320 * 240 * 2;

	 
	uvcg_queue_init(&video->queue, uvc->v4l2_dev.dev->parent,
			V4L2_BUF_TYPE_VIDEO_OUTPUT, &video->mutex);
	return 0;
}
