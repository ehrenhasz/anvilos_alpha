
 
 

#include <linux/bitmap.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/xarray.h>

#include "common.h"

#include "raw_mode.h"

#include <trace/events/scmi.h>

#define SCMI_XFER_RAW_MAX_RETRIES	10

 
struct scmi_raw_queue {
	struct list_head free_bufs;
	 
	spinlock_t free_bufs_lock;
	struct list_head msg_q;
	 
	spinlock_t msg_q_lock;
	wait_queue_head_t wq;
};

 
struct scmi_raw_mode_info {
	unsigned int id;
	const struct scmi_handle *handle;
	const struct scmi_desc *desc;
	int tx_max_msg;
	struct scmi_raw_queue *q[SCMI_RAW_MAX_QUEUE];
	struct xarray chans_q;
	struct list_head free_waiters;
	 
	struct mutex free_mtx;
	struct list_head active_waiters;
	 
	struct mutex active_mtx;
	struct work_struct waiters_work;
	struct workqueue_struct	*wait_wq;
	struct dentry *dentry;
	void *gid;
};

 
struct scmi_xfer_raw_waiter {
	unsigned long start_jiffies;
	struct scmi_chan_info *cinfo;
	struct scmi_xfer *xfer;
	struct completion async_response;
	struct list_head node;
};

 
struct scmi_raw_buffer {
	size_t max_len;
	struct scmi_msg msg;
	struct list_head node;
};

 
struct scmi_dbg_raw_data {
	u8 chan_id;
	struct scmi_raw_mode_info *raw;
	struct scmi_msg tx;
	size_t tx_size;
	size_t tx_req_size;
	struct scmi_msg rx;
	size_t rx_size;
};

static struct scmi_raw_queue *
scmi_raw_queue_select(struct scmi_raw_mode_info *raw, unsigned int idx,
		      unsigned int chan_id)
{
	if (!chan_id)
		return raw->q[idx];

	return xa_load(&raw->chans_q, chan_id);
}

static struct scmi_raw_buffer *scmi_raw_buffer_get(struct scmi_raw_queue *q)
{
	unsigned long flags;
	struct scmi_raw_buffer *rb = NULL;
	struct list_head *head = &q->free_bufs;

	spin_lock_irqsave(&q->free_bufs_lock, flags);
	if (!list_empty(head)) {
		rb = list_first_entry(head, struct scmi_raw_buffer, node);
		list_del_init(&rb->node);
	}
	spin_unlock_irqrestore(&q->free_bufs_lock, flags);

	return rb;
}

static void scmi_raw_buffer_put(struct scmi_raw_queue *q,
				struct scmi_raw_buffer *rb)
{
	unsigned long flags;

	 
	rb->msg.len = rb->max_len;

	spin_lock_irqsave(&q->free_bufs_lock, flags);
	list_add_tail(&rb->node, &q->free_bufs);
	spin_unlock_irqrestore(&q->free_bufs_lock, flags);
}

static void scmi_raw_buffer_enqueue(struct scmi_raw_queue *q,
				    struct scmi_raw_buffer *rb)
{
	unsigned long flags;

	spin_lock_irqsave(&q->msg_q_lock, flags);
	list_add_tail(&rb->node, &q->msg_q);
	spin_unlock_irqrestore(&q->msg_q_lock, flags);

	wake_up_interruptible(&q->wq);
}

static struct scmi_raw_buffer*
scmi_raw_buffer_dequeue_unlocked(struct scmi_raw_queue *q)
{
	struct scmi_raw_buffer *rb = NULL;

	if (!list_empty(&q->msg_q)) {
		rb = list_first_entry(&q->msg_q, struct scmi_raw_buffer, node);
		list_del_init(&rb->node);
	}

	return rb;
}

static struct scmi_raw_buffer *scmi_raw_buffer_dequeue(struct scmi_raw_queue *q)
{
	unsigned long flags;
	struct scmi_raw_buffer *rb;

	spin_lock_irqsave(&q->msg_q_lock, flags);
	rb = scmi_raw_buffer_dequeue_unlocked(q);
	spin_unlock_irqrestore(&q->msg_q_lock, flags);

	return rb;
}

static void scmi_raw_buffer_queue_flush(struct scmi_raw_queue *q)
{
	struct scmi_raw_buffer *rb;

	do {
		rb = scmi_raw_buffer_dequeue(q);
		if (rb)
			scmi_raw_buffer_put(q, rb);
	} while (rb);
}

static struct scmi_xfer_raw_waiter *
scmi_xfer_raw_waiter_get(struct scmi_raw_mode_info *raw, struct scmi_xfer *xfer,
			 struct scmi_chan_info *cinfo, bool async)
{
	struct scmi_xfer_raw_waiter *rw = NULL;

	mutex_lock(&raw->free_mtx);
	if (!list_empty(&raw->free_waiters)) {
		rw = list_first_entry(&raw->free_waiters,
				      struct scmi_xfer_raw_waiter, node);
		list_del_init(&rw->node);

		if (async) {
			reinit_completion(&rw->async_response);
			xfer->async_done = &rw->async_response;
		}

		rw->cinfo = cinfo;
		rw->xfer = xfer;
	}
	mutex_unlock(&raw->free_mtx);

	return rw;
}

static void scmi_xfer_raw_waiter_put(struct scmi_raw_mode_info *raw,
				     struct scmi_xfer_raw_waiter *rw)
{
	if (rw->xfer) {
		rw->xfer->async_done = NULL;
		rw->xfer = NULL;
	}

	mutex_lock(&raw->free_mtx);
	list_add_tail(&rw->node, &raw->free_waiters);
	mutex_unlock(&raw->free_mtx);
}

static void scmi_xfer_raw_waiter_enqueue(struct scmi_raw_mode_info *raw,
					 struct scmi_xfer_raw_waiter *rw)
{
	 
	rw->start_jiffies = jiffies;

	trace_scmi_xfer_response_wait(rw->xfer->transfer_id, rw->xfer->hdr.id,
				      rw->xfer->hdr.protocol_id,
				      rw->xfer->hdr.seq,
				      raw->desc->max_rx_timeout_ms,
				      rw->xfer->hdr.poll_completion);

	mutex_lock(&raw->active_mtx);
	list_add_tail(&rw->node, &raw->active_waiters);
	mutex_unlock(&raw->active_mtx);

	 
	queue_work(raw->wait_wq, &raw->waiters_work);
}

static struct scmi_xfer_raw_waiter *
scmi_xfer_raw_waiter_dequeue(struct scmi_raw_mode_info *raw)
{
	struct scmi_xfer_raw_waiter *rw = NULL;

	mutex_lock(&raw->active_mtx);
	if (!list_empty(&raw->active_waiters)) {
		rw = list_first_entry(&raw->active_waiters,
				      struct scmi_xfer_raw_waiter, node);
		list_del_init(&rw->node);
	}
	mutex_unlock(&raw->active_mtx);

	return rw;
}

 
static void scmi_xfer_raw_worker(struct work_struct *work)
{
	struct scmi_raw_mode_info *raw;
	struct device *dev;
	unsigned long max_tmo;

	raw = container_of(work, struct scmi_raw_mode_info, waiters_work);
	dev = raw->handle->dev;
	max_tmo = msecs_to_jiffies(raw->desc->max_rx_timeout_ms);

	do {
		int ret = 0;
		unsigned int timeout_ms;
		unsigned long aging;
		struct scmi_xfer *xfer;
		struct scmi_xfer_raw_waiter *rw;
		struct scmi_chan_info *cinfo;

		rw = scmi_xfer_raw_waiter_dequeue(raw);
		if (!rw)
			return;

		cinfo = rw->cinfo;
		xfer = rw->xfer;
		 
		aging = jiffies - rw->start_jiffies;
		timeout_ms = max_tmo > aging ?
			jiffies_to_msecs(max_tmo - aging) : 1;

		ret = scmi_xfer_raw_wait_for_message_response(cinfo, xfer,
							      timeout_ms);
		if (!ret && xfer->hdr.status)
			ret = scmi_to_linux_errno(xfer->hdr.status);

		if (raw->desc->ops->mark_txdone)
			raw->desc->ops->mark_txdone(rw->cinfo, ret, xfer);

		trace_scmi_xfer_end(xfer->transfer_id, xfer->hdr.id,
				    xfer->hdr.protocol_id, xfer->hdr.seq, ret);

		 
		if (!ret && xfer->async_done) {
			unsigned long tmo = msecs_to_jiffies(SCMI_MAX_RESPONSE_TIMEOUT);

			if (!wait_for_completion_timeout(xfer->async_done, tmo))
				dev_err(dev,
					"timed out in RAW delayed resp - HDR:%08X\n",
					pack_scmi_header(&xfer->hdr));
		}

		 
		scmi_xfer_raw_put(raw->handle, xfer);
		scmi_xfer_raw_waiter_put(raw, rw);
	} while (1);
}

static void scmi_xfer_raw_reset(struct scmi_raw_mode_info *raw)
{
	int i;

	dev_info(raw->handle->dev, "Resetting SCMI Raw stack.\n");

	for (i = 0; i < SCMI_RAW_MAX_QUEUE; i++)
		scmi_raw_buffer_queue_flush(raw->q[i]);
}

 
static int scmi_xfer_raw_get_init(struct scmi_raw_mode_info *raw, void *buf,
				  size_t len, struct scmi_xfer **p)
{
	u32 msg_hdr;
	size_t tx_size;
	struct scmi_xfer *xfer;
	int ret, retry = SCMI_XFER_RAW_MAX_RETRIES;
	struct device *dev = raw->handle->dev;

	if (!buf || len < sizeof(u32))
		return -EINVAL;

	tx_size = len - sizeof(u32);
	 
	if (tx_size > raw->desc->max_msg_size)
		return -ERANGE;

	xfer = scmi_xfer_raw_get(raw->handle);
	if (IS_ERR(xfer)) {
		dev_warn(dev, "RAW - Cannot get a free RAW xfer !\n");
		return PTR_ERR(xfer);
	}

	 
	msg_hdr = le32_to_cpu(*((__le32 *)buf));
	unpack_scmi_header(msg_hdr, &xfer->hdr);
	xfer->hdr.seq = (u16)MSG_XTRACT_TOKEN(msg_hdr);
	 
	xfer->hdr.poll_completion = false;
	xfer->hdr.status = SCMI_SUCCESS;
	xfer->tx.len = tx_size;
	xfer->rx.len = raw->desc->max_msg_size;
	 
	memset(xfer->tx.buf, 0x00, raw->desc->max_msg_size);
	if (xfer->tx.len)
		memcpy(xfer->tx.buf, (u8 *)buf + sizeof(msg_hdr), xfer->tx.len);
	*p = xfer;

	 
	do {
		ret = scmi_xfer_raw_inflight_register(raw->handle, xfer);
		if (ret) {
			dev_dbg(dev,
				"...retrying[%d] inflight registration\n",
				retry);
			msleep(raw->desc->max_rx_timeout_ms /
			       SCMI_XFER_RAW_MAX_RETRIES);
		}
	} while (ret && --retry);

	if (ret) {
		dev_warn(dev,
			 "RAW - Could NOT register xfer %d in-flight HDR:0x%08X\n",
			 xfer->hdr.seq, msg_hdr);
		scmi_xfer_raw_put(raw->handle, xfer);
	}

	return ret;
}

 
static int scmi_do_xfer_raw_start(struct scmi_raw_mode_info *raw,
				  struct scmi_xfer *xfer, u8 chan_id,
				  bool async)
{
	int ret;
	struct scmi_chan_info *cinfo;
	struct scmi_xfer_raw_waiter *rw;
	struct device *dev = raw->handle->dev;

	if (!chan_id)
		chan_id = xfer->hdr.protocol_id;
	else
		xfer->flags |= SCMI_XFER_FLAG_CHAN_SET;

	cinfo = scmi_xfer_raw_channel_get(raw->handle, chan_id);
	if (IS_ERR(cinfo))
		return PTR_ERR(cinfo);

	rw = scmi_xfer_raw_waiter_get(raw, xfer, cinfo, async);
	if (!rw) {
		dev_warn(dev, "RAW - Cannot get a free waiter !\n");
		return -ENOMEM;
	}

	 
	if (is_polling_enabled(cinfo, raw->desc))
		xfer->hdr.poll_completion = true;

	reinit_completion(&xfer->done);
	 
	smp_store_mb(xfer->state, SCMI_XFER_SENT_OK);

	trace_scmi_xfer_begin(xfer->transfer_id, xfer->hdr.id,
			      xfer->hdr.protocol_id, xfer->hdr.seq,
			      xfer->hdr.poll_completion);

	ret = raw->desc->ops->send_message(rw->cinfo, xfer);
	if (ret) {
		dev_err(dev, "Failed to send RAW message %d\n", ret);
		scmi_xfer_raw_waiter_put(raw, rw);
		return ret;
	}

	trace_scmi_msg_dump(raw->id, cinfo->id, xfer->hdr.protocol_id,
			    xfer->hdr.id, "cmnd", xfer->hdr.seq,
			    xfer->hdr.status,
			    xfer->tx.buf, xfer->tx.len);

	scmi_xfer_raw_waiter_enqueue(raw, rw);

	return ret;
}

 
static int scmi_raw_message_send(struct scmi_raw_mode_info *raw,
				 void *buf, size_t len, u8 chan_id, bool async)
{
	int ret;
	struct scmi_xfer *xfer;

	ret = scmi_xfer_raw_get_init(raw, buf, len, &xfer);
	if (ret)
		return ret;

	ret = scmi_do_xfer_raw_start(raw, xfer, chan_id, async);
	if (ret)
		scmi_xfer_raw_put(raw->handle, xfer);

	return ret;
}

static struct scmi_raw_buffer *
scmi_raw_message_dequeue(struct scmi_raw_queue *q, bool o_nonblock)
{
	unsigned long flags;
	struct scmi_raw_buffer *rb;

	spin_lock_irqsave(&q->msg_q_lock, flags);
	while (list_empty(&q->msg_q)) {
		spin_unlock_irqrestore(&q->msg_q_lock, flags);

		if (o_nonblock)
			return ERR_PTR(-EAGAIN);

		if (wait_event_interruptible(q->wq, !list_empty(&q->msg_q)))
			return ERR_PTR(-ERESTARTSYS);

		spin_lock_irqsave(&q->msg_q_lock, flags);
	}

	rb = scmi_raw_buffer_dequeue_unlocked(q);

	spin_unlock_irqrestore(&q->msg_q_lock, flags);

	return rb;
}

 
static int scmi_raw_message_receive(struct scmi_raw_mode_info *raw,
				    void *buf, size_t len, size_t *size,
				    unsigned int idx, unsigned int chan_id,
				    bool o_nonblock)
{
	int ret = 0;
	struct scmi_raw_buffer *rb;
	struct scmi_raw_queue *q;

	q = scmi_raw_queue_select(raw, idx, chan_id);
	if (!q)
		return -ENODEV;

	rb = scmi_raw_message_dequeue(q, o_nonblock);
	if (IS_ERR(rb)) {
		dev_dbg(raw->handle->dev, "RAW - No message available!\n");
		return PTR_ERR(rb);
	}

	if (rb->msg.len <= len) {
		memcpy(buf, rb->msg.buf, rb->msg.len);
		*size = rb->msg.len;
	} else {
		ret = -ENOSPC;
	}

	scmi_raw_buffer_put(q, rb);

	return ret;
}

 

static ssize_t scmi_dbg_raw_mode_common_read(struct file *filp,
					     char __user *buf,
					     size_t count, loff_t *ppos,
					     unsigned int idx)
{
	ssize_t cnt;
	struct scmi_dbg_raw_data *rd = filp->private_data;

	if (!rd->rx_size) {
		int ret;

		ret = scmi_raw_message_receive(rd->raw, rd->rx.buf, rd->rx.len,
					       &rd->rx_size, idx, rd->chan_id,
					       filp->f_flags & O_NONBLOCK);
		if (ret) {
			rd->rx_size = 0;
			return ret;
		}

		 
		*ppos = 0;
	} else if (*ppos == rd->rx_size) {
		 
		rd->rx_size = 0;
		return 0;
	}

	cnt = simple_read_from_buffer(buf, count, ppos,
				      rd->rx.buf, rd->rx_size);

	return cnt;
}

static ssize_t scmi_dbg_raw_mode_common_write(struct file *filp,
					      const char __user *buf,
					      size_t count, loff_t *ppos,
					      bool async)
{
	int ret;
	struct scmi_dbg_raw_data *rd = filp->private_data;

	if (count > rd->tx.len - rd->tx_size)
		return -ENOSPC;

	 
	if (!rd->tx_size)
		rd->tx_req_size = count;

	 
	if (rd->tx_size < rd->tx_req_size) {
		ssize_t cnt;

		cnt = simple_write_to_buffer(rd->tx.buf, rd->tx.len, ppos,
					     buf, count);
		if (cnt < 0)
			return cnt;

		rd->tx_size += cnt;
		if (cnt < count)
			return cnt;
	}

	ret = scmi_raw_message_send(rd->raw, rd->tx.buf, rd->tx_size,
				    rd->chan_id, async);

	 
	rd->tx_size = 0;
	*ppos = 0;

	return ret ?: count;
}

static __poll_t scmi_test_dbg_raw_common_poll(struct file *filp,
					      struct poll_table_struct *wait,
					      unsigned int idx)
{
	unsigned long flags;
	struct scmi_dbg_raw_data *rd = filp->private_data;
	struct scmi_raw_queue *q;
	__poll_t mask = 0;

	q = scmi_raw_queue_select(rd->raw, idx, rd->chan_id);
	if (!q)
		return mask;

	poll_wait(filp, &q->wq, wait);

	spin_lock_irqsave(&q->msg_q_lock, flags);
	if (!list_empty(&q->msg_q))
		mask = EPOLLIN | EPOLLRDNORM;
	spin_unlock_irqrestore(&q->msg_q_lock, flags);

	return mask;
}

static ssize_t scmi_dbg_raw_mode_message_read(struct file *filp,
					      char __user *buf,
					      size_t count, loff_t *ppos)
{
	return scmi_dbg_raw_mode_common_read(filp, buf, count, ppos,
					     SCMI_RAW_REPLY_QUEUE);
}

static ssize_t scmi_dbg_raw_mode_message_write(struct file *filp,
					       const char __user *buf,
					       size_t count, loff_t *ppos)
{
	return scmi_dbg_raw_mode_common_write(filp, buf, count, ppos, false);
}

static __poll_t scmi_dbg_raw_mode_message_poll(struct file *filp,
					       struct poll_table_struct *wait)
{
	return scmi_test_dbg_raw_common_poll(filp, wait, SCMI_RAW_REPLY_QUEUE);
}

static int scmi_dbg_raw_mode_open(struct inode *inode, struct file *filp)
{
	u8 id;
	struct scmi_raw_mode_info *raw;
	struct scmi_dbg_raw_data *rd;
	const char *id_str = filp->f_path.dentry->d_parent->d_name.name;

	if (!inode->i_private)
		return -ENODEV;

	raw = inode->i_private;
	rd = kzalloc(sizeof(*rd), GFP_KERNEL);
	if (!rd)
		return -ENOMEM;

	rd->rx.len = raw->desc->max_msg_size + sizeof(u32);
	rd->rx.buf = kzalloc(rd->rx.len, GFP_KERNEL);
	if (!rd->rx.buf) {
		kfree(rd);
		return -ENOMEM;
	}

	rd->tx.len = raw->desc->max_msg_size + sizeof(u32);
	rd->tx.buf = kzalloc(rd->tx.len, GFP_KERNEL);
	if (!rd->tx.buf) {
		kfree(rd->rx.buf);
		kfree(rd);
		return -ENOMEM;
	}

	 
	if (!kstrtou8(id_str, 16, &id))
		rd->chan_id = id;

	rd->raw = raw;
	filp->private_data = rd;

	return 0;
}

static int scmi_dbg_raw_mode_release(struct inode *inode, struct file *filp)
{
	struct scmi_dbg_raw_data *rd = filp->private_data;

	kfree(rd->rx.buf);
	kfree(rd->tx.buf);
	kfree(rd);

	return 0;
}

static ssize_t scmi_dbg_raw_mode_reset_write(struct file *filp,
					     const char __user *buf,
					     size_t count, loff_t *ppos)
{
	struct scmi_dbg_raw_data *rd = filp->private_data;

	scmi_xfer_raw_reset(rd->raw);

	return count;
}

static const struct file_operations scmi_dbg_raw_mode_reset_fops = {
	.open = scmi_dbg_raw_mode_open,
	.release = scmi_dbg_raw_mode_release,
	.write = scmi_dbg_raw_mode_reset_write,
	.owner = THIS_MODULE,
};

static const struct file_operations scmi_dbg_raw_mode_message_fops = {
	.open = scmi_dbg_raw_mode_open,
	.release = scmi_dbg_raw_mode_release,
	.read = scmi_dbg_raw_mode_message_read,
	.write = scmi_dbg_raw_mode_message_write,
	.poll = scmi_dbg_raw_mode_message_poll,
	.owner = THIS_MODULE,
};

static ssize_t scmi_dbg_raw_mode_message_async_write(struct file *filp,
						     const char __user *buf,
						     size_t count, loff_t *ppos)
{
	return scmi_dbg_raw_mode_common_write(filp, buf, count, ppos, true);
}

static const struct file_operations scmi_dbg_raw_mode_message_async_fops = {
	.open = scmi_dbg_raw_mode_open,
	.release = scmi_dbg_raw_mode_release,
	.read = scmi_dbg_raw_mode_message_read,
	.write = scmi_dbg_raw_mode_message_async_write,
	.poll = scmi_dbg_raw_mode_message_poll,
	.owner = THIS_MODULE,
};

static ssize_t scmi_test_dbg_raw_mode_notif_read(struct file *filp,
						 char __user *buf,
						 size_t count, loff_t *ppos)
{
	return scmi_dbg_raw_mode_common_read(filp, buf, count, ppos,
					     SCMI_RAW_NOTIF_QUEUE);
}

static __poll_t
scmi_test_dbg_raw_mode_notif_poll(struct file *filp,
				  struct poll_table_struct *wait)
{
	return scmi_test_dbg_raw_common_poll(filp, wait, SCMI_RAW_NOTIF_QUEUE);
}

static const struct file_operations scmi_dbg_raw_mode_notification_fops = {
	.open = scmi_dbg_raw_mode_open,
	.release = scmi_dbg_raw_mode_release,
	.read = scmi_test_dbg_raw_mode_notif_read,
	.poll = scmi_test_dbg_raw_mode_notif_poll,
	.owner = THIS_MODULE,
};

static ssize_t scmi_test_dbg_raw_mode_errors_read(struct file *filp,
						  char __user *buf,
						  size_t count, loff_t *ppos)
{
	return scmi_dbg_raw_mode_common_read(filp, buf, count, ppos,
					     SCMI_RAW_ERRS_QUEUE);
}

static __poll_t
scmi_test_dbg_raw_mode_errors_poll(struct file *filp,
				   struct poll_table_struct *wait)
{
	return scmi_test_dbg_raw_common_poll(filp, wait, SCMI_RAW_ERRS_QUEUE);
}

static const struct file_operations scmi_dbg_raw_mode_errors_fops = {
	.open = scmi_dbg_raw_mode_open,
	.release = scmi_dbg_raw_mode_release,
	.read = scmi_test_dbg_raw_mode_errors_read,
	.poll = scmi_test_dbg_raw_mode_errors_poll,
	.owner = THIS_MODULE,
};

static struct scmi_raw_queue *
scmi_raw_queue_init(struct scmi_raw_mode_info *raw)
{
	int i;
	struct scmi_raw_buffer *rb;
	struct device *dev = raw->handle->dev;
	struct scmi_raw_queue *q;

	q = devm_kzalloc(dev, sizeof(*q), GFP_KERNEL);
	if (!q)
		return ERR_PTR(-ENOMEM);

	rb = devm_kcalloc(dev, raw->tx_max_msg, sizeof(*rb), GFP_KERNEL);
	if (!rb)
		return ERR_PTR(-ENOMEM);

	spin_lock_init(&q->free_bufs_lock);
	INIT_LIST_HEAD(&q->free_bufs);
	for (i = 0; i < raw->tx_max_msg; i++, rb++) {
		rb->max_len = raw->desc->max_msg_size + sizeof(u32);
		rb->msg.buf = devm_kzalloc(dev, rb->max_len, GFP_KERNEL);
		if (!rb->msg.buf)
			return ERR_PTR(-ENOMEM);
		scmi_raw_buffer_put(q, rb);
	}

	spin_lock_init(&q->msg_q_lock);
	INIT_LIST_HEAD(&q->msg_q);
	init_waitqueue_head(&q->wq);

	return q;
}

static int scmi_xfer_raw_worker_init(struct scmi_raw_mode_info *raw)
{
	int i;
	struct scmi_xfer_raw_waiter *rw;
	struct device *dev = raw->handle->dev;

	rw = devm_kcalloc(dev, raw->tx_max_msg, sizeof(*rw), GFP_KERNEL);
	if (!rw)
		return -ENOMEM;

	raw->wait_wq = alloc_workqueue("scmi-raw-wait-wq-%d",
				       WQ_UNBOUND | WQ_FREEZABLE |
				       WQ_HIGHPRI | WQ_SYSFS, 0, raw->id);
	if (!raw->wait_wq)
		return -ENOMEM;

	mutex_init(&raw->free_mtx);
	INIT_LIST_HEAD(&raw->free_waiters);
	mutex_init(&raw->active_mtx);
	INIT_LIST_HEAD(&raw->active_waiters);

	for (i = 0; i < raw->tx_max_msg; i++, rw++) {
		init_completion(&rw->async_response);
		scmi_xfer_raw_waiter_put(raw, rw);
	}
	INIT_WORK(&raw->waiters_work, scmi_xfer_raw_worker);

	return 0;
}

static int scmi_raw_mode_setup(struct scmi_raw_mode_info *raw,
			       u8 *channels, int num_chans)
{
	int ret, idx;
	void *gid;
	struct device *dev = raw->handle->dev;

	gid = devres_open_group(dev, NULL, GFP_KERNEL);
	if (!gid)
		return -ENOMEM;

	for (idx = 0; idx < SCMI_RAW_MAX_QUEUE; idx++) {
		raw->q[idx] = scmi_raw_queue_init(raw);
		if (IS_ERR(raw->q[idx])) {
			ret = PTR_ERR(raw->q[idx]);
			goto err;
		}
	}

	xa_init(&raw->chans_q);
	if (num_chans > 1) {
		int i;

		for (i = 0; i < num_chans; i++) {
			void *xret;
			struct scmi_raw_queue *q;

			q = scmi_raw_queue_init(raw);
			if (IS_ERR(q)) {
				ret = PTR_ERR(q);
				goto err_xa;
			}

			xret = xa_store(&raw->chans_q, channels[i], q,
					GFP_KERNEL);
			if (xa_err(xret)) {
				dev_err(dev,
					"Fail to allocate Raw queue 0x%02X\n",
					channels[i]);
				ret = xa_err(xret);
				goto err_xa;
			}
		}
	}

	ret = scmi_xfer_raw_worker_init(raw);
	if (ret)
		goto err_xa;

	devres_close_group(dev, gid);
	raw->gid = gid;

	return 0;

err_xa:
	xa_destroy(&raw->chans_q);
err:
	devres_release_group(dev, gid);
	return ret;
}

 
void *scmi_raw_mode_init(const struct scmi_handle *handle,
			 struct dentry *top_dentry, int instance_id,
			 u8 *channels, int num_chans,
			 const struct scmi_desc *desc, int tx_max_msg)
{
	int ret;
	struct scmi_raw_mode_info *raw;
	struct device *dev;

	if (!handle || !desc)
		return ERR_PTR(-EINVAL);

	dev = handle->dev;
	raw = devm_kzalloc(dev, sizeof(*raw), GFP_KERNEL);
	if (!raw)
		return ERR_PTR(-ENOMEM);

	raw->handle = handle;
	raw->desc = desc;
	raw->tx_max_msg = tx_max_msg;
	raw->id = instance_id;

	ret = scmi_raw_mode_setup(raw, channels, num_chans);
	if (ret) {
		devm_kfree(dev, raw);
		return ERR_PTR(ret);
	}

	raw->dentry = debugfs_create_dir("raw", top_dentry);

	debugfs_create_file("reset", 0200, raw->dentry, raw,
			    &scmi_dbg_raw_mode_reset_fops);

	debugfs_create_file("message", 0600, raw->dentry, raw,
			    &scmi_dbg_raw_mode_message_fops);

	debugfs_create_file("message_async", 0600, raw->dentry, raw,
			    &scmi_dbg_raw_mode_message_async_fops);

	debugfs_create_file("notification", 0400, raw->dentry, raw,
			    &scmi_dbg_raw_mode_notification_fops);

	debugfs_create_file("errors", 0400, raw->dentry, raw,
			    &scmi_dbg_raw_mode_errors_fops);

	 
	if (num_chans > 1) {
		int i;
		struct dentry *top_chans;

		top_chans = debugfs_create_dir("channels", raw->dentry);

		for (i = 0; i < num_chans; i++) {
			char cdir[8];
			struct dentry *chd;

			snprintf(cdir, 8, "0x%02X", channels[i]);
			chd = debugfs_create_dir(cdir, top_chans);

			debugfs_create_file("message", 0600, chd, raw,
					    &scmi_dbg_raw_mode_message_fops);

			debugfs_create_file("message_async", 0600, chd, raw,
					    &scmi_dbg_raw_mode_message_async_fops);
		}
	}

	dev_info(dev, "SCMI RAW Mode initialized for instance %d\n", raw->id);

	return raw;
}

 
void scmi_raw_mode_cleanup(void *r)
{
	struct scmi_raw_mode_info *raw = r;

	if (!raw)
		return;

	debugfs_remove_recursive(raw->dentry);

	cancel_work_sync(&raw->waiters_work);
	destroy_workqueue(raw->wait_wq);
	xa_destroy(&raw->chans_q);
}

static int scmi_xfer_raw_collect(void *msg, size_t *msg_len,
				 struct scmi_xfer *xfer)
{
	__le32 *m;
	size_t msg_size;

	if (!xfer || !msg || !msg_len)
		return -EINVAL;

	 
	msg_size = xfer->rx.len + sizeof(u32);
	 
	if (xfer->hdr.type != MSG_TYPE_NOTIFICATION)
		msg_size += sizeof(u32);

	if (msg_size > *msg_len)
		return -ENOSPC;

	m = msg;
	*m = cpu_to_le32(pack_scmi_header(&xfer->hdr));
	if (xfer->hdr.type != MSG_TYPE_NOTIFICATION)
		*++m = cpu_to_le32(xfer->hdr.status);

	memcpy(++m, xfer->rx.buf, xfer->rx.len);

	*msg_len = msg_size;

	return 0;
}

 
void scmi_raw_message_report(void *r, struct scmi_xfer *xfer,
			     unsigned int idx, unsigned int chan_id)
{
	int ret;
	unsigned long flags;
	struct scmi_raw_buffer *rb;
	struct device *dev;
	struct scmi_raw_queue *q;
	struct scmi_raw_mode_info *raw = r;

	if (!raw || (idx == SCMI_RAW_REPLY_QUEUE && !SCMI_XFER_IS_RAW(xfer)))
		return;

	dev = raw->handle->dev;
	q = scmi_raw_queue_select(raw, idx,
				  SCMI_XFER_IS_CHAN_SET(xfer) ? chan_id : 0);

	 
	spin_lock_irqsave(&q->msg_q_lock, flags);
	rb = scmi_raw_buffer_get(q);
	if (!rb) {
		 
		if (idx == SCMI_RAW_REPLY_QUEUE) {
			spin_unlock_irqrestore(&q->msg_q_lock, flags);
			dev_warn(dev,
				 "RAW[%d] - Buffers exhausted. Dropping report.\n",
				 idx);
			return;
		}

		 
		rb = scmi_raw_buffer_dequeue_unlocked(q);
		if (WARN_ON(!rb)) {
			spin_unlock_irqrestore(&q->msg_q_lock, flags);
			return;
		}

		 
		rb->msg.len = rb->max_len;

		dev_warn_once(dev,
			      "RAW[%d] - Buffers exhausted. Re-using oldest.\n",
			      idx);
	}
	spin_unlock_irqrestore(&q->msg_q_lock, flags);

	ret = scmi_xfer_raw_collect(rb->msg.buf, &rb->msg.len, xfer);
	if (ret) {
		dev_warn(dev, "RAW - Cannot collect xfer into buffer !\n");
		scmi_raw_buffer_put(q, rb);
		return;
	}

	scmi_raw_buffer_enqueue(q, rb);
}

static void scmi_xfer_raw_fill(struct scmi_raw_mode_info *raw,
			       struct scmi_chan_info *cinfo,
			       struct scmi_xfer *xfer, u32 msg_hdr)
{
	 
	unpack_scmi_header(msg_hdr, &xfer->hdr);
	xfer->hdr.seq = MSG_XTRACT_TOKEN(msg_hdr);

	memset(xfer->rx.buf, 0x00, xfer->rx.len);

	raw->desc->ops->fetch_response(cinfo, xfer);
}

 
void scmi_raw_error_report(void *r, struct scmi_chan_info *cinfo,
			   u32 msg_hdr, void *priv)
{
	struct scmi_xfer xfer;
	struct scmi_raw_mode_info *raw = r;

	if (!raw)
		return;

	xfer.rx.len = raw->desc->max_msg_size;
	xfer.rx.buf = kzalloc(xfer.rx.len, GFP_ATOMIC);
	if (!xfer.rx.buf) {
		dev_info(raw->handle->dev,
			 "Cannot report Raw error for HDR:0x%X - ENOMEM\n",
			 msg_hdr);
		return;
	}

	 
	if (priv)
		 
		smp_store_mb(xfer.priv, priv);

	scmi_xfer_raw_fill(raw, cinfo, &xfer, msg_hdr);
	scmi_raw_message_report(raw, &xfer, SCMI_RAW_ERRS_QUEUE, 0);

	kfree(xfer.rx.buf);
}
