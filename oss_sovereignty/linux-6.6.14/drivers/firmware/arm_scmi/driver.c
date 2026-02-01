
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/bitmap.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/idr.h>
#include <linux/io.h>
#include <linux/io-64-nonatomic-hi-lo.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/hashtable.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/processor.h>
#include <linux/refcount.h>
#include <linux/slab.h>

#include "common.h"
#include "notify.h"

#include "raw_mode.h"

#define CREATE_TRACE_POINTS
#include <trace/events/scmi.h>

static DEFINE_IDA(scmi_id);

static DEFINE_IDR(scmi_protocols);
static DEFINE_SPINLOCK(protocol_lock);

 
static LIST_HEAD(scmi_list);
 
static DEFINE_MUTEX(scmi_list_mutex);
 
static atomic_t transfer_last_id;

static struct dentry *scmi_top_dentry;

 
struct scmi_xfers_info {
	unsigned long *xfer_alloc_table;
	spinlock_t xfer_lock;
	int max_msg;
	struct hlist_head free_xfers;
	DECLARE_HASHTABLE(pending_xfers, SCMI_PENDING_XFERS_HT_ORDER_SZ);
};

 
struct scmi_protocol_instance {
	const struct scmi_handle	*handle;
	const struct scmi_protocol	*proto;
	void				*gid;
	refcount_t			users;
	void				*priv;
	struct scmi_protocol_handle	ph;
};

#define ph_to_pi(h)	container_of(h, struct scmi_protocol_instance, ph)

 
struct scmi_debug_info {
	struct dentry *top_dentry;
	const char *name;
	const char *type;
	bool is_atomic;
};

 
struct scmi_info {
	int id;
	struct device *dev;
	const struct scmi_desc *desc;
	struct scmi_revision_info version;
	struct scmi_handle handle;
	struct scmi_xfers_info tx_minfo;
	struct scmi_xfers_info rx_minfo;
	struct idr tx_idr;
	struct idr rx_idr;
	struct idr protocols;
	 
	struct mutex protocols_mtx;
	u8 *protocols_imp;
	struct idr active_protocols;
	unsigned int atomic_threshold;
	void *notify_priv;
	struct list_head node;
	int users;
	struct notifier_block bus_nb;
	struct notifier_block dev_req_nb;
	 
	struct mutex devreq_mtx;
	struct scmi_debug_info *dbg;
	void *raw;
};

#define handle_to_scmi_info(h)	container_of(h, struct scmi_info, handle)
#define bus_nb_to_scmi_info(nb)	container_of(nb, struct scmi_info, bus_nb)
#define req_nb_to_scmi_info(nb)	container_of(nb, struct scmi_info, dev_req_nb)

static const struct scmi_protocol *scmi_protocol_get(int protocol_id)
{
	const struct scmi_protocol *proto;

	proto = idr_find(&scmi_protocols, protocol_id);
	if (!proto || !try_module_get(proto->owner)) {
		pr_warn("SCMI Protocol 0x%x not found!\n", protocol_id);
		return NULL;
	}

	pr_debug("Found SCMI Protocol 0x%x\n", protocol_id);

	return proto;
}

static void scmi_protocol_put(int protocol_id)
{
	const struct scmi_protocol *proto;

	proto = idr_find(&scmi_protocols, protocol_id);
	if (proto)
		module_put(proto->owner);
}

int scmi_protocol_register(const struct scmi_protocol *proto)
{
	int ret;

	if (!proto) {
		pr_err("invalid protocol\n");
		return -EINVAL;
	}

	if (!proto->instance_init) {
		pr_err("missing init for protocol 0x%x\n", proto->id);
		return -EINVAL;
	}

	spin_lock(&protocol_lock);
	ret = idr_alloc(&scmi_protocols, (void *)proto,
			proto->id, proto->id + 1, GFP_ATOMIC);
	spin_unlock(&protocol_lock);
	if (ret != proto->id) {
		pr_err("unable to allocate SCMI idr slot for 0x%x - err %d\n",
		       proto->id, ret);
		return ret;
	}

	pr_debug("Registered SCMI Protocol 0x%x\n", proto->id);

	return 0;
}
EXPORT_SYMBOL_GPL(scmi_protocol_register);

void scmi_protocol_unregister(const struct scmi_protocol *proto)
{
	spin_lock(&protocol_lock);
	idr_remove(&scmi_protocols, proto->id);
	spin_unlock(&protocol_lock);

	pr_debug("Unregistered SCMI Protocol 0x%x\n", proto->id);
}
EXPORT_SYMBOL_GPL(scmi_protocol_unregister);

 
static void scmi_create_protocol_devices(struct device_node *np,
					 struct scmi_info *info,
					 int prot_id, const char *name)
{
	struct scmi_device *sdev;

	mutex_lock(&info->devreq_mtx);
	sdev = scmi_device_create(np, info->dev, prot_id, name);
	if (name && !sdev)
		dev_err(info->dev,
			"failed to create device for protocol 0x%X (%s)\n",
			prot_id, name);
	mutex_unlock(&info->devreq_mtx);
}

static void scmi_destroy_protocol_devices(struct scmi_info *info,
					  int prot_id, const char *name)
{
	mutex_lock(&info->devreq_mtx);
	scmi_device_destroy(info->dev, prot_id, name);
	mutex_unlock(&info->devreq_mtx);
}

void scmi_notification_instance_data_set(const struct scmi_handle *handle,
					 void *priv)
{
	struct scmi_info *info = handle_to_scmi_info(handle);

	info->notify_priv = priv;
	 
	smp_wmb();
}

void *scmi_notification_instance_data_get(const struct scmi_handle *handle)
{
	struct scmi_info *info = handle_to_scmi_info(handle);

	 
	smp_rmb();
	return info->notify_priv;
}

 
static int scmi_xfer_token_set(struct scmi_xfers_info *minfo,
			       struct scmi_xfer *xfer)
{
	unsigned long xfer_id, next_token;

	 
	next_token = (xfer->transfer_id & (MSG_TOKEN_MAX - 1));

	 
	xfer_id = find_next_zero_bit(minfo->xfer_alloc_table,
				     MSG_TOKEN_MAX, next_token);
	if (xfer_id == MSG_TOKEN_MAX) {
		 
		xfer_id = find_next_zero_bit(minfo->xfer_alloc_table,
					     MSG_TOKEN_MAX, 0);
		 
		if (WARN_ON_ONCE(xfer_id == MSG_TOKEN_MAX))
			return -ENOMEM;
	}

	 
	if (xfer_id != next_token)
		atomic_add((int)(xfer_id - next_token), &transfer_last_id);

	xfer->hdr.seq = (u16)xfer_id;

	return 0;
}

 
static inline void scmi_xfer_token_clear(struct scmi_xfers_info *minfo,
					 struct scmi_xfer *xfer)
{
	clear_bit(xfer->hdr.seq, minfo->xfer_alloc_table);
}

 
static inline void
scmi_xfer_inflight_register_unlocked(struct scmi_xfer *xfer,
				     struct scmi_xfers_info *minfo)
{
	 
	set_bit(xfer->hdr.seq, minfo->xfer_alloc_table);
	hash_add(minfo->pending_xfers, &xfer->node, xfer->hdr.seq);
	xfer->pending = true;
}

 
static int scmi_xfer_inflight_register(struct scmi_xfer *xfer,
				       struct scmi_xfers_info *minfo)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&minfo->xfer_lock, flags);
	if (!test_bit(xfer->hdr.seq, minfo->xfer_alloc_table))
		scmi_xfer_inflight_register_unlocked(xfer, minfo);
	else
		ret = -EBUSY;
	spin_unlock_irqrestore(&minfo->xfer_lock, flags);

	return ret;
}

 
int scmi_xfer_raw_inflight_register(const struct scmi_handle *handle,
				    struct scmi_xfer *xfer)
{
	struct scmi_info *info = handle_to_scmi_info(handle);

	return scmi_xfer_inflight_register(xfer, &info->tx_minfo);
}

 
static inline int scmi_xfer_pending_set(struct scmi_xfer *xfer,
					struct scmi_xfers_info *minfo)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&minfo->xfer_lock, flags);
	 
	ret = scmi_xfer_token_set(minfo, xfer);
	if (!ret)
		scmi_xfer_inflight_register_unlocked(xfer, minfo);
	spin_unlock_irqrestore(&minfo->xfer_lock, flags);

	return ret;
}

 
static struct scmi_xfer *scmi_xfer_get(const struct scmi_handle *handle,
				       struct scmi_xfers_info *minfo)
{
	unsigned long flags;
	struct scmi_xfer *xfer;

	spin_lock_irqsave(&minfo->xfer_lock, flags);
	if (hlist_empty(&minfo->free_xfers)) {
		spin_unlock_irqrestore(&minfo->xfer_lock, flags);
		return ERR_PTR(-ENOMEM);
	}

	 
	xfer = hlist_entry(minfo->free_xfers.first, struct scmi_xfer, node);
	hlist_del_init(&xfer->node);

	 
	xfer->transfer_id = atomic_inc_return(&transfer_last_id);

	refcount_set(&xfer->users, 1);
	atomic_set(&xfer->busy, SCMI_XFER_FREE);
	spin_unlock_irqrestore(&minfo->xfer_lock, flags);

	return xfer;
}

 
struct scmi_xfer *scmi_xfer_raw_get(const struct scmi_handle *handle)
{
	struct scmi_xfer *xfer;
	struct scmi_info *info = handle_to_scmi_info(handle);

	xfer = scmi_xfer_get(handle, &info->tx_minfo);
	if (!IS_ERR(xfer))
		xfer->flags |= SCMI_XFER_FLAG_IS_RAW;

	return xfer;
}

 
struct scmi_chan_info *
scmi_xfer_raw_channel_get(const struct scmi_handle *handle, u8 protocol_id)
{
	struct scmi_chan_info *cinfo;
	struct scmi_info *info = handle_to_scmi_info(handle);

	cinfo = idr_find(&info->tx_idr, protocol_id);
	if (!cinfo) {
		if (protocol_id == SCMI_PROTOCOL_BASE)
			return ERR_PTR(-EINVAL);
		 
		cinfo = idr_find(&info->tx_idr, SCMI_PROTOCOL_BASE);
		if (!cinfo)
			return ERR_PTR(-EINVAL);
		dev_warn_once(handle->dev,
			      "Using Base channel for protocol 0x%X\n",
			      protocol_id);
	}

	return cinfo;
}

 
static void
__scmi_xfer_put(struct scmi_xfers_info *minfo, struct scmi_xfer *xfer)
{
	unsigned long flags;

	spin_lock_irqsave(&minfo->xfer_lock, flags);
	if (refcount_dec_and_test(&xfer->users)) {
		if (xfer->pending) {
			scmi_xfer_token_clear(minfo, xfer);
			hash_del(&xfer->node);
			xfer->pending = false;
		}
		hlist_add_head(&xfer->node, &minfo->free_xfers);
	}
	spin_unlock_irqrestore(&minfo->xfer_lock, flags);
}

 
void scmi_xfer_raw_put(const struct scmi_handle *handle, struct scmi_xfer *xfer)
{
	struct scmi_info *info = handle_to_scmi_info(handle);

	xfer->flags &= ~SCMI_XFER_FLAG_IS_RAW;
	xfer->flags &= ~SCMI_XFER_FLAG_CHAN_SET;
	return __scmi_xfer_put(&info->tx_minfo, xfer);
}

 
static struct scmi_xfer *
scmi_xfer_lookup_unlocked(struct scmi_xfers_info *minfo, u16 xfer_id)
{
	struct scmi_xfer *xfer = NULL;

	if (test_bit(xfer_id, minfo->xfer_alloc_table))
		xfer = XFER_FIND(minfo->pending_xfers, xfer_id);

	return xfer ?: ERR_PTR(-EINVAL);
}

 
static inline int scmi_msg_response_validate(struct scmi_chan_info *cinfo,
					     u8 msg_type,
					     struct scmi_xfer *xfer)
{
	 
	if (msg_type == MSG_TYPE_DELAYED_RESP && !xfer->async_done) {
		dev_err(cinfo->dev,
			"Delayed Response for %d not expected! Buggy F/W ?\n",
			xfer->hdr.seq);
		return -EINVAL;
	}

	switch (xfer->state) {
	case SCMI_XFER_SENT_OK:
		if (msg_type == MSG_TYPE_DELAYED_RESP) {
			 
			xfer->hdr.status = SCMI_SUCCESS;
			xfer->state = SCMI_XFER_RESP_OK;
			complete(&xfer->done);
			dev_warn(cinfo->dev,
				 "Received valid OoO Delayed Response for %d\n",
				 xfer->hdr.seq);
		}
		break;
	case SCMI_XFER_RESP_OK:
		if (msg_type != MSG_TYPE_DELAYED_RESP)
			return -EINVAL;
		break;
	case SCMI_XFER_DRESP_OK:
		 
		return -EINVAL;
	}

	return 0;
}

 
static inline void scmi_xfer_state_update(struct scmi_xfer *xfer, u8 msg_type)
{
	xfer->hdr.type = msg_type;

	 
	if (xfer->hdr.type == MSG_TYPE_COMMAND)
		xfer->state = SCMI_XFER_RESP_OK;
	else
		xfer->state = SCMI_XFER_DRESP_OK;
}

static bool scmi_xfer_acquired(struct scmi_xfer *xfer)
{
	int ret;

	ret = atomic_cmpxchg(&xfer->busy, SCMI_XFER_FREE, SCMI_XFER_BUSY);

	return ret == SCMI_XFER_FREE;
}

 
static inline struct scmi_xfer *
scmi_xfer_command_acquire(struct scmi_chan_info *cinfo, u32 msg_hdr)
{
	int ret;
	unsigned long flags;
	struct scmi_xfer *xfer;
	struct scmi_info *info = handle_to_scmi_info(cinfo->handle);
	struct scmi_xfers_info *minfo = &info->tx_minfo;
	u8 msg_type = MSG_XTRACT_TYPE(msg_hdr);
	u16 xfer_id = MSG_XTRACT_TOKEN(msg_hdr);

	 
	spin_lock_irqsave(&minfo->xfer_lock, flags);
	xfer = scmi_xfer_lookup_unlocked(minfo, xfer_id);
	if (IS_ERR(xfer)) {
		dev_err(cinfo->dev,
			"Message for %d type %d is not expected!\n",
			xfer_id, msg_type);
		spin_unlock_irqrestore(&minfo->xfer_lock, flags);
		return xfer;
	}
	refcount_inc(&xfer->users);
	spin_unlock_irqrestore(&minfo->xfer_lock, flags);

	spin_lock_irqsave(&xfer->lock, flags);
	ret = scmi_msg_response_validate(cinfo, msg_type, xfer);
	 
	if (!ret) {
		spin_until_cond(scmi_xfer_acquired(xfer));
		scmi_xfer_state_update(xfer, msg_type);
	}
	spin_unlock_irqrestore(&xfer->lock, flags);

	if (ret) {
		dev_err(cinfo->dev,
			"Invalid message type:%d for %d - HDR:0x%X  state:%d\n",
			msg_type, xfer_id, msg_hdr, xfer->state);
		 
		__scmi_xfer_put(minfo, xfer);
		xfer = ERR_PTR(-EINVAL);
	}

	return xfer;
}

static inline void scmi_xfer_command_release(struct scmi_info *info,
					     struct scmi_xfer *xfer)
{
	atomic_set(&xfer->busy, SCMI_XFER_FREE);
	__scmi_xfer_put(&info->tx_minfo, xfer);
}

static inline void scmi_clear_channel(struct scmi_info *info,
				      struct scmi_chan_info *cinfo)
{
	if (info->desc->ops->clear_channel)
		info->desc->ops->clear_channel(cinfo);
}

static void scmi_handle_notification(struct scmi_chan_info *cinfo,
				     u32 msg_hdr, void *priv)
{
	struct scmi_xfer *xfer;
	struct device *dev = cinfo->dev;
	struct scmi_info *info = handle_to_scmi_info(cinfo->handle);
	struct scmi_xfers_info *minfo = &info->rx_minfo;
	ktime_t ts;

	ts = ktime_get_boottime();
	xfer = scmi_xfer_get(cinfo->handle, minfo);
	if (IS_ERR(xfer)) {
		dev_err(dev, "failed to get free message slot (%ld)\n",
			PTR_ERR(xfer));
		scmi_clear_channel(info, cinfo);
		return;
	}

	unpack_scmi_header(msg_hdr, &xfer->hdr);
	if (priv)
		 
		smp_store_mb(xfer->priv, priv);
	info->desc->ops->fetch_notification(cinfo, info->desc->max_msg_size,
					    xfer);

	trace_scmi_msg_dump(info->id, cinfo->id, xfer->hdr.protocol_id,
			    xfer->hdr.id, "NOTI", xfer->hdr.seq,
			    xfer->hdr.status, xfer->rx.buf, xfer->rx.len);

	scmi_notify(cinfo->handle, xfer->hdr.protocol_id,
		    xfer->hdr.id, xfer->rx.buf, xfer->rx.len, ts);

	trace_scmi_rx_done(xfer->transfer_id, xfer->hdr.id,
			   xfer->hdr.protocol_id, xfer->hdr.seq,
			   MSG_TYPE_NOTIFICATION);

	if (IS_ENABLED(CONFIG_ARM_SCMI_RAW_MODE_SUPPORT)) {
		xfer->hdr.seq = MSG_XTRACT_TOKEN(msg_hdr);
		scmi_raw_message_report(info->raw, xfer, SCMI_RAW_NOTIF_QUEUE,
					cinfo->id);
	}

	__scmi_xfer_put(minfo, xfer);

	scmi_clear_channel(info, cinfo);
}

static void scmi_handle_response(struct scmi_chan_info *cinfo,
				 u32 msg_hdr, void *priv)
{
	struct scmi_xfer *xfer;
	struct scmi_info *info = handle_to_scmi_info(cinfo->handle);

	xfer = scmi_xfer_command_acquire(cinfo, msg_hdr);
	if (IS_ERR(xfer)) {
		if (IS_ENABLED(CONFIG_ARM_SCMI_RAW_MODE_SUPPORT))
			scmi_raw_error_report(info->raw, cinfo, msg_hdr, priv);

		if (MSG_XTRACT_TYPE(msg_hdr) == MSG_TYPE_DELAYED_RESP)
			scmi_clear_channel(info, cinfo);
		return;
	}

	 
	if (xfer->hdr.type == MSG_TYPE_DELAYED_RESP)
		xfer->rx.len = info->desc->max_msg_size;

	if (priv)
		 
		smp_store_mb(xfer->priv, priv);
	info->desc->ops->fetch_response(cinfo, xfer);

	trace_scmi_msg_dump(info->id, cinfo->id, xfer->hdr.protocol_id,
			    xfer->hdr.id,
			    xfer->hdr.type == MSG_TYPE_DELAYED_RESP ?
			    (!SCMI_XFER_IS_RAW(xfer) ? "DLYD" : "dlyd") :
			    (!SCMI_XFER_IS_RAW(xfer) ? "RESP" : "resp"),
			    xfer->hdr.seq, xfer->hdr.status,
			    xfer->rx.buf, xfer->rx.len);

	trace_scmi_rx_done(xfer->transfer_id, xfer->hdr.id,
			   xfer->hdr.protocol_id, xfer->hdr.seq,
			   xfer->hdr.type);

	if (xfer->hdr.type == MSG_TYPE_DELAYED_RESP) {
		scmi_clear_channel(info, cinfo);
		complete(xfer->async_done);
	} else {
		complete(&xfer->done);
	}

	if (IS_ENABLED(CONFIG_ARM_SCMI_RAW_MODE_SUPPORT)) {
		 
		if (!xfer->hdr.poll_completion)
			scmi_raw_message_report(info->raw, xfer,
						SCMI_RAW_REPLY_QUEUE,
						cinfo->id);
	}

	scmi_xfer_command_release(info, xfer);
}

 
void scmi_rx_callback(struct scmi_chan_info *cinfo, u32 msg_hdr, void *priv)
{
	u8 msg_type = MSG_XTRACT_TYPE(msg_hdr);

	switch (msg_type) {
	case MSG_TYPE_NOTIFICATION:
		scmi_handle_notification(cinfo, msg_hdr, priv);
		break;
	case MSG_TYPE_COMMAND:
	case MSG_TYPE_DELAYED_RESP:
		scmi_handle_response(cinfo, msg_hdr, priv);
		break;
	default:
		WARN_ONCE(1, "received unknown msg_type:%d\n", msg_type);
		break;
	}
}

 
static void xfer_put(const struct scmi_protocol_handle *ph,
		     struct scmi_xfer *xfer)
{
	const struct scmi_protocol_instance *pi = ph_to_pi(ph);
	struct scmi_info *info = handle_to_scmi_info(pi->handle);

	__scmi_xfer_put(&info->tx_minfo, xfer);
}

static bool scmi_xfer_done_no_timeout(struct scmi_chan_info *cinfo,
				      struct scmi_xfer *xfer, ktime_t stop)
{
	struct scmi_info *info = handle_to_scmi_info(cinfo->handle);

	 
	return info->desc->ops->poll_done(cinfo, xfer) ||
	       try_wait_for_completion(&xfer->done) ||
	       ktime_after(ktime_get(), stop);
}

static int scmi_wait_for_reply(struct device *dev, const struct scmi_desc *desc,
			       struct scmi_chan_info *cinfo,
			       struct scmi_xfer *xfer, unsigned int timeout_ms)
{
	int ret = 0;

	if (xfer->hdr.poll_completion) {
		 
		if (!desc->sync_cmds_completed_on_ret) {
			 
			ktime_t stop = ktime_add_ms(ktime_get(), timeout_ms);

			spin_until_cond(scmi_xfer_done_no_timeout(cinfo,
								  xfer, stop));
			if (ktime_after(ktime_get(), stop)) {
				dev_err(dev,
					"timed out in resp(caller: %pS) - polling\n",
					(void *)_RET_IP_);
				ret = -ETIMEDOUT;
			}
		}

		if (!ret) {
			unsigned long flags;
			struct scmi_info *info =
				handle_to_scmi_info(cinfo->handle);

			 
			spin_lock_irqsave(&xfer->lock, flags);
			if (xfer->state == SCMI_XFER_SENT_OK) {
				desc->ops->fetch_response(cinfo, xfer);
				xfer->state = SCMI_XFER_RESP_OK;
			}
			spin_unlock_irqrestore(&xfer->lock, flags);

			 
			trace_scmi_msg_dump(info->id, cinfo->id,
					    xfer->hdr.protocol_id, xfer->hdr.id,
					    !SCMI_XFER_IS_RAW(xfer) ?
					    "RESP" : "resp",
					    xfer->hdr.seq, xfer->hdr.status,
					    xfer->rx.buf, xfer->rx.len);

			if (IS_ENABLED(CONFIG_ARM_SCMI_RAW_MODE_SUPPORT)) {
				struct scmi_info *info =
					handle_to_scmi_info(cinfo->handle);

				scmi_raw_message_report(info->raw, xfer,
							SCMI_RAW_REPLY_QUEUE,
							cinfo->id);
			}
		}
	} else {
		 
		if (!wait_for_completion_timeout(&xfer->done,
						 msecs_to_jiffies(timeout_ms))) {
			dev_err(dev, "timed out in resp(caller: %pS)\n",
				(void *)_RET_IP_);
			ret = -ETIMEDOUT;
		}
	}

	return ret;
}

 
static int scmi_wait_for_message_response(struct scmi_chan_info *cinfo,
					  struct scmi_xfer *xfer)
{
	struct scmi_info *info = handle_to_scmi_info(cinfo->handle);
	struct device *dev = info->dev;

	trace_scmi_xfer_response_wait(xfer->transfer_id, xfer->hdr.id,
				      xfer->hdr.protocol_id, xfer->hdr.seq,
				      info->desc->max_rx_timeout_ms,
				      xfer->hdr.poll_completion);

	return scmi_wait_for_reply(dev, info->desc, cinfo, xfer,
				   info->desc->max_rx_timeout_ms);
}

 
int scmi_xfer_raw_wait_for_message_response(struct scmi_chan_info *cinfo,
					    struct scmi_xfer *xfer,
					    unsigned int timeout_ms)
{
	int ret;
	struct scmi_info *info = handle_to_scmi_info(cinfo->handle);
	struct device *dev = info->dev;

	ret = scmi_wait_for_reply(dev, info->desc, cinfo, xfer, timeout_ms);
	if (ret)
		dev_dbg(dev, "timed out in RAW response - HDR:%08X\n",
			pack_scmi_header(&xfer->hdr));

	return ret;
}

 
static int do_xfer(const struct scmi_protocol_handle *ph,
		   struct scmi_xfer *xfer)
{
	int ret;
	const struct scmi_protocol_instance *pi = ph_to_pi(ph);
	struct scmi_info *info = handle_to_scmi_info(pi->handle);
	struct device *dev = info->dev;
	struct scmi_chan_info *cinfo;

	 
	if (xfer->hdr.poll_completion &&
	    !is_transport_polling_capable(info->desc)) {
		dev_warn_once(dev,
			      "Polling mode is not supported by transport.\n");
		return -EINVAL;
	}

	cinfo = idr_find(&info->tx_idr, pi->proto->id);
	if (unlikely(!cinfo))
		return -EINVAL;

	 
	if (is_polling_enabled(cinfo, info->desc))
		xfer->hdr.poll_completion = true;

	 
	xfer->hdr.protocol_id = pi->proto->id;
	reinit_completion(&xfer->done);

	trace_scmi_xfer_begin(xfer->transfer_id, xfer->hdr.id,
			      xfer->hdr.protocol_id, xfer->hdr.seq,
			      xfer->hdr.poll_completion);

	 
	xfer->hdr.status = SCMI_SUCCESS;
	xfer->state = SCMI_XFER_SENT_OK;
	 
	smp_mb();

	ret = info->desc->ops->send_message(cinfo, xfer);
	if (ret < 0) {
		dev_dbg(dev, "Failed to send message %d\n", ret);
		return ret;
	}

	trace_scmi_msg_dump(info->id, cinfo->id, xfer->hdr.protocol_id,
			    xfer->hdr.id, "CMND", xfer->hdr.seq,
			    xfer->hdr.status, xfer->tx.buf, xfer->tx.len);

	ret = scmi_wait_for_message_response(cinfo, xfer);
	if (!ret && xfer->hdr.status)
		ret = scmi_to_linux_errno(xfer->hdr.status);

	if (info->desc->ops->mark_txdone)
		info->desc->ops->mark_txdone(cinfo, ret, xfer);

	trace_scmi_xfer_end(xfer->transfer_id, xfer->hdr.id,
			    xfer->hdr.protocol_id, xfer->hdr.seq, ret);

	return ret;
}

static void reset_rx_to_maxsz(const struct scmi_protocol_handle *ph,
			      struct scmi_xfer *xfer)
{
	const struct scmi_protocol_instance *pi = ph_to_pi(ph);
	struct scmi_info *info = handle_to_scmi_info(pi->handle);

	xfer->rx.len = info->desc->max_msg_size;
}

 
static int do_xfer_with_response(const struct scmi_protocol_handle *ph,
				 struct scmi_xfer *xfer)
{
	int ret, timeout = msecs_to_jiffies(SCMI_MAX_RESPONSE_TIMEOUT);
	DECLARE_COMPLETION_ONSTACK(async_response);

	xfer->async_done = &async_response;

	 
	WARN_ON_ONCE(xfer->hdr.poll_completion);

	ret = do_xfer(ph, xfer);
	if (!ret) {
		if (!wait_for_completion_timeout(xfer->async_done, timeout)) {
			dev_err(ph->dev,
				"timed out in delayed resp(caller: %pS)\n",
				(void *)_RET_IP_);
			ret = -ETIMEDOUT;
		} else if (xfer->hdr.status) {
			ret = scmi_to_linux_errno(xfer->hdr.status);
		}
	}

	xfer->async_done = NULL;
	return ret;
}

 
static int xfer_get_init(const struct scmi_protocol_handle *ph,
			 u8 msg_id, size_t tx_size, size_t rx_size,
			 struct scmi_xfer **p)
{
	int ret;
	struct scmi_xfer *xfer;
	const struct scmi_protocol_instance *pi = ph_to_pi(ph);
	struct scmi_info *info = handle_to_scmi_info(pi->handle);
	struct scmi_xfers_info *minfo = &info->tx_minfo;
	struct device *dev = info->dev;

	 
	if (rx_size > info->desc->max_msg_size ||
	    tx_size > info->desc->max_msg_size)
		return -ERANGE;

	xfer = scmi_xfer_get(pi->handle, minfo);
	if (IS_ERR(xfer)) {
		ret = PTR_ERR(xfer);
		dev_err(dev, "failed to get free message slot(%d)\n", ret);
		return ret;
	}

	 
	ret = scmi_xfer_pending_set(xfer, minfo);
	if (ret) {
		dev_err(pi->handle->dev,
			"Failed to get monotonic token %d\n", ret);
		__scmi_xfer_put(minfo, xfer);
		return ret;
	}

	xfer->tx.len = tx_size;
	xfer->rx.len = rx_size ? : info->desc->max_msg_size;
	xfer->hdr.type = MSG_TYPE_COMMAND;
	xfer->hdr.id = msg_id;
	xfer->hdr.poll_completion = false;

	*p = xfer;

	return 0;
}

 
static int version_get(const struct scmi_protocol_handle *ph, u32 *version)
{
	int ret;
	__le32 *rev_info;
	struct scmi_xfer *t;

	ret = xfer_get_init(ph, PROTOCOL_VERSION, 0, sizeof(*version), &t);
	if (ret)
		return ret;

	ret = do_xfer(ph, t);
	if (!ret) {
		rev_info = t->rx.buf;
		*version = le32_to_cpu(*rev_info);
	}

	xfer_put(ph, t);
	return ret;
}

 
static int scmi_set_protocol_priv(const struct scmi_protocol_handle *ph,
				  void *priv)
{
	struct scmi_protocol_instance *pi = ph_to_pi(ph);

	pi->priv = priv;

	return 0;
}

 
static void *scmi_get_protocol_priv(const struct scmi_protocol_handle *ph)
{
	const struct scmi_protocol_instance *pi = ph_to_pi(ph);

	return pi->priv;
}

static const struct scmi_xfer_ops xfer_ops = {
	.version_get = version_get,
	.xfer_get_init = xfer_get_init,
	.reset_rx_to_maxsz = reset_rx_to_maxsz,
	.do_xfer = do_xfer,
	.do_xfer_with_response = do_xfer_with_response,
	.xfer_put = xfer_put,
};

struct scmi_msg_resp_domain_name_get {
	__le32 flags;
	u8 name[SCMI_MAX_STR_SIZE];
};

 
static int scmi_common_extended_name_get(const struct scmi_protocol_handle *ph,
					 u8 cmd_id, u32 res_id, char *name,
					 size_t len)
{
	int ret;
	struct scmi_xfer *t;
	struct scmi_msg_resp_domain_name_get *resp;

	ret = ph->xops->xfer_get_init(ph, cmd_id, sizeof(res_id),
				      sizeof(*resp), &t);
	if (ret)
		goto out;

	put_unaligned_le32(res_id, t->tx.buf);
	resp = t->rx.buf;

	ret = ph->xops->do_xfer(ph, t);
	if (!ret)
		strscpy(name, resp->name, len);

	ph->xops->xfer_put(ph, t);
out:
	if (ret)
		dev_warn(ph->dev,
			 "Failed to get extended name - id:%u (ret:%d). Using %s\n",
			 res_id, ret, name);
	return ret;
}

 
struct scmi_iterator {
	void *msg;
	void *resp;
	struct scmi_xfer *t;
	const struct scmi_protocol_handle *ph;
	struct scmi_iterator_ops *ops;
	struct scmi_iterator_state state;
	void *priv;
};

static void *scmi_iterator_init(const struct scmi_protocol_handle *ph,
				struct scmi_iterator_ops *ops,
				unsigned int max_resources, u8 msg_id,
				size_t tx_size, void *priv)
{
	int ret;
	struct scmi_iterator *i;

	i = devm_kzalloc(ph->dev, sizeof(*i), GFP_KERNEL);
	if (!i)
		return ERR_PTR(-ENOMEM);

	i->ph = ph;
	i->ops = ops;
	i->priv = priv;

	ret = ph->xops->xfer_get_init(ph, msg_id, tx_size, 0, &i->t);
	if (ret) {
		devm_kfree(ph->dev, i);
		return ERR_PTR(ret);
	}

	i->state.max_resources = max_resources;
	i->msg = i->t->tx.buf;
	i->resp = i->t->rx.buf;

	return i;
}

static int scmi_iterator_run(void *iter)
{
	int ret = -EINVAL;
	struct scmi_iterator_ops *iops;
	const struct scmi_protocol_handle *ph;
	struct scmi_iterator_state *st;
	struct scmi_iterator *i = iter;

	if (!i || !i->ops || !i->ph)
		return ret;

	iops = i->ops;
	ph = i->ph;
	st = &i->state;

	do {
		iops->prepare_message(i->msg, st->desc_index, i->priv);
		ret = ph->xops->do_xfer(ph, i->t);
		if (ret)
			break;

		st->rx_len = i->t->rx.len;
		ret = iops->update_state(st, i->resp, i->priv);
		if (ret)
			break;

		if (st->num_returned > st->max_resources - st->desc_index) {
			dev_err(ph->dev,
				"No. of resources can't exceed %d\n",
				st->max_resources);
			ret = -EINVAL;
			break;
		}

		for (st->loop_idx = 0; st->loop_idx < st->num_returned;
		     st->loop_idx++) {
			ret = iops->process_response(ph, i->resp, st, i->priv);
			if (ret)
				goto out;
		}

		st->desc_index += st->num_returned;
		ph->xops->reset_rx_to_maxsz(ph, i->t);
		 
	} while (st->num_returned && st->num_remaining);

out:
	 
	ph->xops->xfer_put(ph, i->t);
	devm_kfree(ph->dev, i);

	return ret;
}

struct scmi_msg_get_fc_info {
	__le32 domain;
	__le32 message_id;
};

struct scmi_msg_resp_desc_fc {
	__le32 attr;
#define SUPPORTS_DOORBELL(x)		((x) & BIT(0))
#define DOORBELL_REG_WIDTH(x)		FIELD_GET(GENMASK(2, 1), (x))
	__le32 rate_limit;
	__le32 chan_addr_low;
	__le32 chan_addr_high;
	__le32 chan_size;
	__le32 db_addr_low;
	__le32 db_addr_high;
	__le32 db_set_lmask;
	__le32 db_set_hmask;
	__le32 db_preserve_lmask;
	__le32 db_preserve_hmask;
};

static void
scmi_common_fastchannel_init(const struct scmi_protocol_handle *ph,
			     u8 describe_id, u32 message_id, u32 valid_size,
			     u32 domain, void __iomem **p_addr,
			     struct scmi_fc_db_info **p_db)
{
	int ret;
	u32 flags;
	u64 phys_addr;
	u8 size;
	void __iomem *addr;
	struct scmi_xfer *t;
	struct scmi_fc_db_info *db = NULL;
	struct scmi_msg_get_fc_info *info;
	struct scmi_msg_resp_desc_fc *resp;
	const struct scmi_protocol_instance *pi = ph_to_pi(ph);

	if (!p_addr) {
		ret = -EINVAL;
		goto err_out;
	}

	ret = ph->xops->xfer_get_init(ph, describe_id,
				      sizeof(*info), sizeof(*resp), &t);
	if (ret)
		goto err_out;

	info = t->tx.buf;
	info->domain = cpu_to_le32(domain);
	info->message_id = cpu_to_le32(message_id);

	 
	ret = ph->xops->do_xfer(ph, t);
	if (ret)
		goto err_xfer;

	resp = t->rx.buf;
	flags = le32_to_cpu(resp->attr);
	size = le32_to_cpu(resp->chan_size);
	if (size != valid_size) {
		ret = -EINVAL;
		goto err_xfer;
	}

	phys_addr = le32_to_cpu(resp->chan_addr_low);
	phys_addr |= (u64)le32_to_cpu(resp->chan_addr_high) << 32;
	addr = devm_ioremap(ph->dev, phys_addr, size);
	if (!addr) {
		ret = -EADDRNOTAVAIL;
		goto err_xfer;
	}

	*p_addr = addr;

	if (p_db && SUPPORTS_DOORBELL(flags)) {
		db = devm_kzalloc(ph->dev, sizeof(*db), GFP_KERNEL);
		if (!db) {
			ret = -ENOMEM;
			goto err_db;
		}

		size = 1 << DOORBELL_REG_WIDTH(flags);
		phys_addr = le32_to_cpu(resp->db_addr_low);
		phys_addr |= (u64)le32_to_cpu(resp->db_addr_high) << 32;
		addr = devm_ioremap(ph->dev, phys_addr, size);
		if (!addr) {
			ret = -EADDRNOTAVAIL;
			goto err_db_mem;
		}

		db->addr = addr;
		db->width = size;
		db->set = le32_to_cpu(resp->db_set_lmask);
		db->set |= (u64)le32_to_cpu(resp->db_set_hmask) << 32;
		db->mask = le32_to_cpu(resp->db_preserve_lmask);
		db->mask |= (u64)le32_to_cpu(resp->db_preserve_hmask) << 32;

		*p_db = db;
	}

	ph->xops->xfer_put(ph, t);

	dev_dbg(ph->dev,
		"Using valid FC for protocol %X [MSG_ID:%u / RES_ID:%u]\n",
		pi->proto->id, message_id, domain);

	return;

err_db_mem:
	devm_kfree(ph->dev, db);

err_db:
	*p_addr = NULL;

err_xfer:
	ph->xops->xfer_put(ph, t);

err_out:
	dev_warn(ph->dev,
		 "Failed to get FC for protocol %X [MSG_ID:%u / RES_ID:%u] - ret:%d. Using regular messaging.\n",
		 pi->proto->id, message_id, domain, ret);
}

#define SCMI_PROTO_FC_RING_DB(w)			\
do {							\
	u##w val = 0;					\
							\
	if (db->mask)					\
		val = ioread##w(db->addr) & db->mask;	\
	iowrite##w((u##w)db->set | val, db->addr);	\
} while (0)

static void scmi_common_fastchannel_db_ring(struct scmi_fc_db_info *db)
{
	if (!db || !db->addr)
		return;

	if (db->width == 1)
		SCMI_PROTO_FC_RING_DB(8);
	else if (db->width == 2)
		SCMI_PROTO_FC_RING_DB(16);
	else if (db->width == 4)
		SCMI_PROTO_FC_RING_DB(32);
	else  
#ifdef CONFIG_64BIT
		SCMI_PROTO_FC_RING_DB(64);
#else
	{
		u64 val = 0;

		if (db->mask)
			val = ioread64_hi_lo(db->addr) & db->mask;
		iowrite64_hi_lo(db->set | val, db->addr);
	}
#endif
}

static const struct scmi_proto_helpers_ops helpers_ops = {
	.extended_name_get = scmi_common_extended_name_get,
	.iter_response_init = scmi_iterator_init,
	.iter_response_run = scmi_iterator_run,
	.fastchannel_init = scmi_common_fastchannel_init,
	.fastchannel_db_ring = scmi_common_fastchannel_db_ring,
};

 
struct scmi_revision_info *
scmi_revision_area_get(const struct scmi_protocol_handle *ph)
{
	const struct scmi_protocol_instance *pi = ph_to_pi(ph);

	return pi->handle->version;
}

 
static struct scmi_protocol_instance *
scmi_alloc_init_protocol_instance(struct scmi_info *info,
				  const struct scmi_protocol *proto)
{
	int ret = -ENOMEM;
	void *gid;
	struct scmi_protocol_instance *pi;
	const struct scmi_handle *handle = &info->handle;

	 
	gid = devres_open_group(handle->dev, NULL, GFP_KERNEL);
	if (!gid) {
		scmi_protocol_put(proto->id);
		goto out;
	}

	pi = devm_kzalloc(handle->dev, sizeof(*pi), GFP_KERNEL);
	if (!pi)
		goto clean;

	pi->gid = gid;
	pi->proto = proto;
	pi->handle = handle;
	pi->ph.dev = handle->dev;
	pi->ph.xops = &xfer_ops;
	pi->ph.hops = &helpers_ops;
	pi->ph.set_priv = scmi_set_protocol_priv;
	pi->ph.get_priv = scmi_get_protocol_priv;
	refcount_set(&pi->users, 1);
	 
	ret = pi->proto->instance_init(&pi->ph);
	if (ret)
		goto clean;

	ret = idr_alloc(&info->protocols, pi, proto->id, proto->id + 1,
			GFP_KERNEL);
	if (ret != proto->id)
		goto clean;

	 
	if (pi->proto->events) {
		ret = scmi_register_protocol_events(handle, pi->proto->id,
						    &pi->ph,
						    pi->proto->events);
		if (ret)
			dev_warn(handle->dev,
				 "Protocol:%X - Events Registration Failed - err:%d\n",
				 pi->proto->id, ret);
	}

	devres_close_group(handle->dev, pi->gid);
	dev_dbg(handle->dev, "Initialized protocol: 0x%X\n", pi->proto->id);

	return pi;

clean:
	 
	scmi_protocol_put(proto->id);
	devres_release_group(handle->dev, gid);
out:
	return ERR_PTR(ret);
}

 
static struct scmi_protocol_instance * __must_check
scmi_get_protocol_instance(const struct scmi_handle *handle, u8 protocol_id)
{
	struct scmi_protocol_instance *pi;
	struct scmi_info *info = handle_to_scmi_info(handle);

	mutex_lock(&info->protocols_mtx);
	pi = idr_find(&info->protocols, protocol_id);

	if (pi) {
		refcount_inc(&pi->users);
	} else {
		const struct scmi_protocol *proto;

		 
		proto = scmi_protocol_get(protocol_id);
		if (proto)
			pi = scmi_alloc_init_protocol_instance(info, proto);
		else
			pi = ERR_PTR(-EPROBE_DEFER);
	}
	mutex_unlock(&info->protocols_mtx);

	return pi;
}

 
int scmi_protocol_acquire(const struct scmi_handle *handle, u8 protocol_id)
{
	return PTR_ERR_OR_ZERO(scmi_get_protocol_instance(handle, protocol_id));
}

 
void scmi_protocol_release(const struct scmi_handle *handle, u8 protocol_id)
{
	struct scmi_info *info = handle_to_scmi_info(handle);
	struct scmi_protocol_instance *pi;

	mutex_lock(&info->protocols_mtx);
	pi = idr_find(&info->protocols, protocol_id);
	if (WARN_ON(!pi))
		goto out;

	if (refcount_dec_and_test(&pi->users)) {
		void *gid = pi->gid;

		if (pi->proto->events)
			scmi_deregister_protocol_events(handle, protocol_id);

		if (pi->proto->instance_deinit)
			pi->proto->instance_deinit(&pi->ph);

		idr_remove(&info->protocols, protocol_id);

		scmi_protocol_put(protocol_id);

		devres_release_group(handle->dev, gid);
		dev_dbg(handle->dev, "De-Initialized protocol: 0x%X\n",
			protocol_id);
	}

out:
	mutex_unlock(&info->protocols_mtx);
}

void scmi_setup_protocol_implemented(const struct scmi_protocol_handle *ph,
				     u8 *prot_imp)
{
	const struct scmi_protocol_instance *pi = ph_to_pi(ph);
	struct scmi_info *info = handle_to_scmi_info(pi->handle);

	info->protocols_imp = prot_imp;
}

static bool
scmi_is_protocol_implemented(const struct scmi_handle *handle, u8 prot_id)
{
	int i;
	struct scmi_info *info = handle_to_scmi_info(handle);
	struct scmi_revision_info *rev = handle->version;

	if (!info->protocols_imp)
		return false;

	for (i = 0; i < rev->num_protocols; i++)
		if (info->protocols_imp[i] == prot_id)
			return true;
	return false;
}

struct scmi_protocol_devres {
	const struct scmi_handle *handle;
	u8 protocol_id;
};

static void scmi_devm_release_protocol(struct device *dev, void *res)
{
	struct scmi_protocol_devres *dres = res;

	scmi_protocol_release(dres->handle, dres->protocol_id);
}

static struct scmi_protocol_instance __must_check *
scmi_devres_protocol_instance_get(struct scmi_device *sdev, u8 protocol_id)
{
	struct scmi_protocol_instance *pi;
	struct scmi_protocol_devres *dres;

	dres = devres_alloc(scmi_devm_release_protocol,
			    sizeof(*dres), GFP_KERNEL);
	if (!dres)
		return ERR_PTR(-ENOMEM);

	pi = scmi_get_protocol_instance(sdev->handle, protocol_id);
	if (IS_ERR(pi)) {
		devres_free(dres);
		return pi;
	}

	dres->handle = sdev->handle;
	dres->protocol_id = protocol_id;
	devres_add(&sdev->dev, dres);

	return pi;
}

 
static const void __must_check *
scmi_devm_protocol_get(struct scmi_device *sdev, u8 protocol_id,
		       struct scmi_protocol_handle **ph)
{
	struct scmi_protocol_instance *pi;

	if (!ph)
		return ERR_PTR(-EINVAL);

	pi = scmi_devres_protocol_instance_get(sdev, protocol_id);
	if (IS_ERR(pi))
		return pi;

	*ph = &pi->ph;

	return pi->proto->ops;
}

 
static int __must_check scmi_devm_protocol_acquire(struct scmi_device *sdev,
						   u8 protocol_id)
{
	struct scmi_protocol_instance *pi;

	pi = scmi_devres_protocol_instance_get(sdev, protocol_id);
	if (IS_ERR(pi))
		return PTR_ERR(pi);

	return 0;
}

static int scmi_devm_protocol_match(struct device *dev, void *res, void *data)
{
	struct scmi_protocol_devres *dres = res;

	if (WARN_ON(!dres || !data))
		return 0;

	return dres->protocol_id == *((u8 *)data);
}

 
static void scmi_devm_protocol_put(struct scmi_device *sdev, u8 protocol_id)
{
	int ret;

	ret = devres_release(&sdev->dev, scmi_devm_release_protocol,
			     scmi_devm_protocol_match, &protocol_id);
	WARN_ON(ret);
}

 
static bool scmi_is_transport_atomic(const struct scmi_handle *handle,
				     unsigned int *atomic_threshold)
{
	bool ret;
	struct scmi_info *info = handle_to_scmi_info(handle);

	ret = info->desc->atomic_enabled &&
		is_transport_polling_capable(info->desc);
	if (ret && atomic_threshold)
		*atomic_threshold = info->atomic_threshold;

	return ret;
}

 
static struct scmi_handle *scmi_handle_get(struct device *dev)
{
	struct list_head *p;
	struct scmi_info *info;
	struct scmi_handle *handle = NULL;

	mutex_lock(&scmi_list_mutex);
	list_for_each(p, &scmi_list) {
		info = list_entry(p, struct scmi_info, node);
		if (dev->parent == info->dev) {
			info->users++;
			handle = &info->handle;
			break;
		}
	}
	mutex_unlock(&scmi_list_mutex);

	return handle;
}

 
static int scmi_handle_put(const struct scmi_handle *handle)
{
	struct scmi_info *info;

	if (!handle)
		return -EINVAL;

	info = handle_to_scmi_info(handle);
	mutex_lock(&scmi_list_mutex);
	if (!WARN_ON(!info->users))
		info->users--;
	mutex_unlock(&scmi_list_mutex);

	return 0;
}

static void scmi_device_link_add(struct device *consumer,
				 struct device *supplier)
{
	struct device_link *link;

	link = device_link_add(consumer, supplier, DL_FLAG_AUTOREMOVE_CONSUMER);

	WARN_ON(!link);
}

static void scmi_set_handle(struct scmi_device *scmi_dev)
{
	scmi_dev->handle = scmi_handle_get(&scmi_dev->dev);
	if (scmi_dev->handle)
		scmi_device_link_add(&scmi_dev->dev, scmi_dev->handle->dev);
}

static int __scmi_xfer_info_init(struct scmi_info *sinfo,
				 struct scmi_xfers_info *info)
{
	int i;
	struct scmi_xfer *xfer;
	struct device *dev = sinfo->dev;
	const struct scmi_desc *desc = sinfo->desc;

	 
	if (WARN_ON(!info->max_msg || info->max_msg > MSG_TOKEN_MAX)) {
		dev_err(dev,
			"Invalid maximum messages %d, not in range [1 - %lu]\n",
			info->max_msg, MSG_TOKEN_MAX);
		return -EINVAL;
	}

	hash_init(info->pending_xfers);

	 
	info->xfer_alloc_table = devm_bitmap_zalloc(dev, MSG_TOKEN_MAX,
						    GFP_KERNEL);
	if (!info->xfer_alloc_table)
		return -ENOMEM;

	 
	INIT_HLIST_HEAD(&info->free_xfers);
	for (i = 0; i < info->max_msg; i++) {
		xfer = devm_kzalloc(dev, sizeof(*xfer), GFP_KERNEL);
		if (!xfer)
			return -ENOMEM;

		xfer->rx.buf = devm_kcalloc(dev, sizeof(u8), desc->max_msg_size,
					    GFP_KERNEL);
		if (!xfer->rx.buf)
			return -ENOMEM;

		xfer->tx.buf = xfer->rx.buf;
		init_completion(&xfer->done);
		spin_lock_init(&xfer->lock);

		 
		hlist_add_head(&xfer->node, &info->free_xfers);
	}

	spin_lock_init(&info->xfer_lock);

	return 0;
}

static int scmi_channels_max_msg_configure(struct scmi_info *sinfo)
{
	const struct scmi_desc *desc = sinfo->desc;

	if (!desc->ops->get_max_msg) {
		sinfo->tx_minfo.max_msg = desc->max_msg;
		sinfo->rx_minfo.max_msg = desc->max_msg;
	} else {
		struct scmi_chan_info *base_cinfo;

		base_cinfo = idr_find(&sinfo->tx_idr, SCMI_PROTOCOL_BASE);
		if (!base_cinfo)
			return -EINVAL;
		sinfo->tx_minfo.max_msg = desc->ops->get_max_msg(base_cinfo);

		 
		base_cinfo = idr_find(&sinfo->rx_idr, SCMI_PROTOCOL_BASE);
		if (base_cinfo)
			sinfo->rx_minfo.max_msg =
				desc->ops->get_max_msg(base_cinfo);
	}

	return 0;
}

static int scmi_xfer_info_init(struct scmi_info *sinfo)
{
	int ret;

	ret = scmi_channels_max_msg_configure(sinfo);
	if (ret)
		return ret;

	ret = __scmi_xfer_info_init(sinfo, &sinfo->tx_minfo);
	if (!ret && !idr_is_empty(&sinfo->rx_idr))
		ret = __scmi_xfer_info_init(sinfo, &sinfo->rx_minfo);

	return ret;
}

static int scmi_chan_setup(struct scmi_info *info, struct device_node *of_node,
			   int prot_id, bool tx)
{
	int ret, idx;
	char name[32];
	struct scmi_chan_info *cinfo;
	struct idr *idr;
	struct scmi_device *tdev = NULL;

	 
	idx = tx ? 0 : 1;
	idr = tx ? &info->tx_idr : &info->rx_idr;

	if (!info->desc->ops->chan_available(of_node, idx)) {
		cinfo = idr_find(idr, SCMI_PROTOCOL_BASE);
		if (unlikely(!cinfo))  
			return -EINVAL;
		goto idr_alloc;
	}

	cinfo = devm_kzalloc(info->dev, sizeof(*cinfo), GFP_KERNEL);
	if (!cinfo)
		return -ENOMEM;

	cinfo->rx_timeout_ms = info->desc->max_rx_timeout_ms;

	 
	snprintf(name, 32, "__scmi_transport_device_%s_%02X",
		 idx ? "rx" : "tx", prot_id);
	 
	tdev = scmi_device_create(of_node, info->dev, prot_id, name);
	if (!tdev) {
		dev_err(info->dev,
			"failed to create transport device (%s)\n", name);
		devm_kfree(info->dev, cinfo);
		return -EINVAL;
	}
	of_node_get(of_node);

	cinfo->id = prot_id;
	cinfo->dev = &tdev->dev;
	ret = info->desc->ops->chan_setup(cinfo, info->dev, tx);
	if (ret) {
		of_node_put(of_node);
		scmi_device_destroy(info->dev, prot_id, name);
		devm_kfree(info->dev, cinfo);
		return ret;
	}

	if (tx && is_polling_required(cinfo, info->desc)) {
		if (is_transport_polling_capable(info->desc))
			dev_info(&tdev->dev,
				 "Enabled polling mode TX channel - prot_id:%d\n",
				 prot_id);
		else
			dev_warn(&tdev->dev,
				 "Polling mode NOT supported by transport.\n");
	}

idr_alloc:
	ret = idr_alloc(idr, cinfo, prot_id, prot_id + 1, GFP_KERNEL);
	if (ret != prot_id) {
		dev_err(info->dev,
			"unable to allocate SCMI idr slot err %d\n", ret);
		 
		if (tdev) {
			of_node_put(of_node);
			scmi_device_destroy(info->dev, prot_id, name);
			devm_kfree(info->dev, cinfo);
		}
		return ret;
	}

	cinfo->handle = &info->handle;
	return 0;
}

static inline int
scmi_txrx_setup(struct scmi_info *info, struct device_node *of_node,
		int prot_id)
{
	int ret = scmi_chan_setup(info, of_node, prot_id, true);

	if (!ret) {
		 
		ret = scmi_chan_setup(info, of_node, prot_id, false);
		if (ret && ret != -ENOMEM)
			ret = 0;
	}

	return ret;
}

 
static int scmi_channels_setup(struct scmi_info *info)
{
	int ret;
	struct device_node *child, *top_np = info->dev->of_node;

	 
	ret = scmi_txrx_setup(info, top_np, SCMI_PROTOCOL_BASE);
	if (ret)
		return ret;

	for_each_available_child_of_node(top_np, child) {
		u32 prot_id;

		if (of_property_read_u32(child, "reg", &prot_id))
			continue;

		if (!FIELD_FIT(MSG_PROTOCOL_ID_MASK, prot_id))
			dev_err(info->dev,
				"Out of range protocol %d\n", prot_id);

		ret = scmi_txrx_setup(info, child, prot_id);
		if (ret) {
			of_node_put(child);
			return ret;
		}
	}

	return 0;
}

static int scmi_chan_destroy(int id, void *p, void *idr)
{
	struct scmi_chan_info *cinfo = p;

	if (cinfo->dev) {
		struct scmi_info *info = handle_to_scmi_info(cinfo->handle);
		struct scmi_device *sdev = to_scmi_dev(cinfo->dev);

		of_node_put(cinfo->dev->of_node);
		scmi_device_destroy(info->dev, id, sdev->name);
		cinfo->dev = NULL;
	}

	idr_remove(idr, id);

	return 0;
}

static void scmi_cleanup_channels(struct scmi_info *info, struct idr *idr)
{
	 
	idr_for_each(idr, info->desc->ops->chan_free, idr);

	 
	idr_for_each(idr, scmi_chan_destroy, idr);

	idr_destroy(idr);
}

static void scmi_cleanup_txrx_channels(struct scmi_info *info)
{
	scmi_cleanup_channels(info, &info->tx_idr);

	scmi_cleanup_channels(info, &info->rx_idr);
}

static int scmi_bus_notifier(struct notifier_block *nb,
			     unsigned long action, void *data)
{
	struct scmi_info *info = bus_nb_to_scmi_info(nb);
	struct scmi_device *sdev = to_scmi_dev(data);

	 
	if (!strncmp(sdev->name, "__scmi_transport_device", 23) ||
	    sdev->dev.parent != info->dev)
		return NOTIFY_DONE;

	switch (action) {
	case BUS_NOTIFY_BIND_DRIVER:
		 
		scmi_set_handle(sdev);
		break;
	case BUS_NOTIFY_UNBOUND_DRIVER:
		scmi_handle_put(sdev->handle);
		sdev->handle = NULL;
		break;
	default:
		return NOTIFY_DONE;
	}

	dev_dbg(info->dev, "Device %s (%s) is now %s\n", dev_name(&sdev->dev),
		sdev->name, action == BUS_NOTIFY_BIND_DRIVER ?
		"about to be BOUND." : "UNBOUND.");

	return NOTIFY_OK;
}

static int scmi_device_request_notifier(struct notifier_block *nb,
					unsigned long action, void *data)
{
	struct device_node *np;
	struct scmi_device_id *id_table = data;
	struct scmi_info *info = req_nb_to_scmi_info(nb);

	np = idr_find(&info->active_protocols, id_table->protocol_id);
	if (!np)
		return NOTIFY_DONE;

	dev_dbg(info->dev, "%sRequested device (%s) for protocol 0x%x\n",
		action == SCMI_BUS_NOTIFY_DEVICE_REQUEST ? "" : "UN-",
		id_table->name, id_table->protocol_id);

	switch (action) {
	case SCMI_BUS_NOTIFY_DEVICE_REQUEST:
		scmi_create_protocol_devices(np, info, id_table->protocol_id,
					     id_table->name);
		break;
	case SCMI_BUS_NOTIFY_DEVICE_UNREQUEST:
		scmi_destroy_protocol_devices(info, id_table->protocol_id,
					      id_table->name);
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static void scmi_debugfs_common_cleanup(void *d)
{
	struct scmi_debug_info *dbg = d;

	if (!dbg)
		return;

	debugfs_remove_recursive(dbg->top_dentry);
	kfree(dbg->name);
	kfree(dbg->type);
}

static struct scmi_debug_info *scmi_debugfs_common_setup(struct scmi_info *info)
{
	char top_dir[16];
	struct dentry *trans, *top_dentry;
	struct scmi_debug_info *dbg;
	const char *c_ptr = NULL;

	dbg = devm_kzalloc(info->dev, sizeof(*dbg), GFP_KERNEL);
	if (!dbg)
		return NULL;

	dbg->name = kstrdup(of_node_full_name(info->dev->of_node), GFP_KERNEL);
	if (!dbg->name) {
		devm_kfree(info->dev, dbg);
		return NULL;
	}

	of_property_read_string(info->dev->of_node, "compatible", &c_ptr);
	dbg->type = kstrdup(c_ptr, GFP_KERNEL);
	if (!dbg->type) {
		kfree(dbg->name);
		devm_kfree(info->dev, dbg);
		return NULL;
	}

	snprintf(top_dir, 16, "%d", info->id);
	top_dentry = debugfs_create_dir(top_dir, scmi_top_dentry);
	trans = debugfs_create_dir("transport", top_dentry);

	dbg->is_atomic = info->desc->atomic_enabled &&
				is_transport_polling_capable(info->desc);

	debugfs_create_str("instance_name", 0400, top_dentry,
			   (char **)&dbg->name);

	debugfs_create_u32("atomic_threshold_us", 0400, top_dentry,
			   &info->atomic_threshold);

	debugfs_create_str("type", 0400, trans, (char **)&dbg->type);

	debugfs_create_bool("is_atomic", 0400, trans, &dbg->is_atomic);

	debugfs_create_u32("max_rx_timeout_ms", 0400, trans,
			   (u32 *)&info->desc->max_rx_timeout_ms);

	debugfs_create_u32("max_msg_size", 0400, trans,
			   (u32 *)&info->desc->max_msg_size);

	debugfs_create_u32("tx_max_msg", 0400, trans,
			   (u32 *)&info->tx_minfo.max_msg);

	debugfs_create_u32("rx_max_msg", 0400, trans,
			   (u32 *)&info->rx_minfo.max_msg);

	dbg->top_dentry = top_dentry;

	if (devm_add_action_or_reset(info->dev,
				     scmi_debugfs_common_cleanup, dbg)) {
		scmi_debugfs_common_cleanup(dbg);
		return NULL;
	}

	return dbg;
}

static int scmi_debugfs_raw_mode_setup(struct scmi_info *info)
{
	int id, num_chans = 0, ret = 0;
	struct scmi_chan_info *cinfo;
	u8 channels[SCMI_MAX_CHANNELS] = {};
	DECLARE_BITMAP(protos, SCMI_MAX_CHANNELS) = {};

	if (!info->dbg)
		return -EINVAL;

	 
	idr_for_each_entry(&info->tx_idr, cinfo, id) {
		 
		if (num_chans >= SCMI_MAX_CHANNELS || !cinfo) {
			dev_warn(info->dev,
				 "SCMI RAW - Error enumerating channels\n");
			break;
		}

		if (!test_bit(cinfo->id, protos)) {
			channels[num_chans++] = cinfo->id;
			set_bit(cinfo->id, protos);
		}
	}

	info->raw = scmi_raw_mode_init(&info->handle, info->dbg->top_dentry,
				       info->id, channels, num_chans,
				       info->desc, info->tx_minfo.max_msg);
	if (IS_ERR(info->raw)) {
		dev_err(info->dev, "Failed to initialize SCMI RAW Mode !\n");
		ret = PTR_ERR(info->raw);
		info->raw = NULL;
	}

	return ret;
}

static int scmi_probe(struct platform_device *pdev)
{
	int ret;
	struct scmi_handle *handle;
	const struct scmi_desc *desc;
	struct scmi_info *info;
	bool coex = IS_ENABLED(CONFIG_ARM_SCMI_RAW_MODE_SUPPORT_COEX);
	struct device *dev = &pdev->dev;
	struct device_node *child, *np = dev->of_node;

	desc = of_device_get_match_data(dev);
	if (!desc)
		return -EINVAL;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->id = ida_alloc_min(&scmi_id, 0, GFP_KERNEL);
	if (info->id < 0)
		return info->id;

	info->dev = dev;
	info->desc = desc;
	info->bus_nb.notifier_call = scmi_bus_notifier;
	info->dev_req_nb.notifier_call = scmi_device_request_notifier;
	INIT_LIST_HEAD(&info->node);
	idr_init(&info->protocols);
	mutex_init(&info->protocols_mtx);
	idr_init(&info->active_protocols);
	mutex_init(&info->devreq_mtx);

	platform_set_drvdata(pdev, info);
	idr_init(&info->tx_idr);
	idr_init(&info->rx_idr);

	handle = &info->handle;
	handle->dev = info->dev;
	handle->version = &info->version;
	handle->devm_protocol_acquire = scmi_devm_protocol_acquire;
	handle->devm_protocol_get = scmi_devm_protocol_get;
	handle->devm_protocol_put = scmi_devm_protocol_put;

	 
	if (!of_property_read_u32(np, "atomic-threshold-us",
				  &info->atomic_threshold))
		dev_info(dev,
			 "SCMI System wide atomic threshold set to %d us\n",
			 info->atomic_threshold);
	handle->is_transport_atomic = scmi_is_transport_atomic;

	if (desc->ops->link_supplier) {
		ret = desc->ops->link_supplier(dev);
		if (ret)
			goto clear_ida;
	}

	 
	ret = scmi_channels_setup(info);
	if (ret)
		goto clear_ida;

	ret = bus_register_notifier(&scmi_bus_type, &info->bus_nb);
	if (ret)
		goto clear_txrx_setup;

	ret = blocking_notifier_chain_register(&scmi_requested_devices_nh,
					       &info->dev_req_nb);
	if (ret)
		goto clear_bus_notifier;

	ret = scmi_xfer_info_init(info);
	if (ret)
		goto clear_dev_req_notifier;

	if (scmi_top_dentry) {
		info->dbg = scmi_debugfs_common_setup(info);
		if (!info->dbg)
			dev_warn(dev, "Failed to setup SCMI debugfs.\n");

		if (IS_ENABLED(CONFIG_ARM_SCMI_RAW_MODE_SUPPORT)) {
			ret = scmi_debugfs_raw_mode_setup(info);
			if (!coex) {
				if (ret)
					goto clear_dev_req_notifier;

				 
				return 0;
			}

			 
			dev_info(dev, "SCMI RAW Mode COEX enabled !\n");
		}
	}

	if (scmi_notification_init(handle))
		dev_err(dev, "SCMI Notifications NOT available.\n");

	if (info->desc->atomic_enabled &&
	    !is_transport_polling_capable(info->desc))
		dev_err(dev,
			"Transport is not polling capable. Atomic mode not supported.\n");

	 
	ret = scmi_protocol_acquire(handle, SCMI_PROTOCOL_BASE);
	if (ret) {
		dev_err(dev, "unable to communicate with SCMI\n");
		if (coex)
			return 0;
		goto notification_exit;
	}

	mutex_lock(&scmi_list_mutex);
	list_add_tail(&info->node, &scmi_list);
	mutex_unlock(&scmi_list_mutex);

	for_each_available_child_of_node(np, child) {
		u32 prot_id;

		if (of_property_read_u32(child, "reg", &prot_id))
			continue;

		if (!FIELD_FIT(MSG_PROTOCOL_ID_MASK, prot_id))
			dev_err(dev, "Out of range protocol %d\n", prot_id);

		if (!scmi_is_protocol_implemented(handle, prot_id)) {
			dev_err(dev, "SCMI protocol %d not implemented\n",
				prot_id);
			continue;
		}

		 
		ret = idr_alloc(&info->active_protocols, child,
				prot_id, prot_id + 1, GFP_KERNEL);
		if (ret != prot_id) {
			dev_err(dev, "SCMI protocol %d already activated. Skip\n",
				prot_id);
			continue;
		}

		of_node_get(child);
		scmi_create_protocol_devices(child, info, prot_id, NULL);
	}

	return 0;

notification_exit:
	if (IS_ENABLED(CONFIG_ARM_SCMI_RAW_MODE_SUPPORT))
		scmi_raw_mode_cleanup(info->raw);
	scmi_notification_exit(&info->handle);
clear_dev_req_notifier:
	blocking_notifier_chain_unregister(&scmi_requested_devices_nh,
					   &info->dev_req_nb);
clear_bus_notifier:
	bus_unregister_notifier(&scmi_bus_type, &info->bus_nb);
clear_txrx_setup:
	scmi_cleanup_txrx_channels(info);
clear_ida:
	ida_free(&scmi_id, info->id);
	return ret;
}

static int scmi_remove(struct platform_device *pdev)
{
	int id;
	struct scmi_info *info = platform_get_drvdata(pdev);
	struct device_node *child;

	if (IS_ENABLED(CONFIG_ARM_SCMI_RAW_MODE_SUPPORT))
		scmi_raw_mode_cleanup(info->raw);

	mutex_lock(&scmi_list_mutex);
	if (info->users)
		dev_warn(&pdev->dev,
			 "Still active SCMI users will be forcibly unbound.\n");
	list_del(&info->node);
	mutex_unlock(&scmi_list_mutex);

	scmi_notification_exit(&info->handle);

	mutex_lock(&info->protocols_mtx);
	idr_destroy(&info->protocols);
	mutex_unlock(&info->protocols_mtx);

	idr_for_each_entry(&info->active_protocols, child, id)
		of_node_put(child);
	idr_destroy(&info->active_protocols);

	blocking_notifier_chain_unregister(&scmi_requested_devices_nh,
					   &info->dev_req_nb);
	bus_unregister_notifier(&scmi_bus_type, &info->bus_nb);

	 
	scmi_cleanup_txrx_channels(info);

	ida_free(&scmi_id, info->id);

	return 0;
}

static ssize_t protocol_version_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct scmi_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%u.%u\n", info->version.major_ver,
		       info->version.minor_ver);
}
static DEVICE_ATTR_RO(protocol_version);

static ssize_t firmware_version_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct scmi_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "0x%x\n", info->version.impl_ver);
}
static DEVICE_ATTR_RO(firmware_version);

static ssize_t vendor_id_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct scmi_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", info->version.vendor_id);
}
static DEVICE_ATTR_RO(vendor_id);

static ssize_t sub_vendor_id_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct scmi_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", info->version.sub_vendor_id);
}
static DEVICE_ATTR_RO(sub_vendor_id);

static struct attribute *versions_attrs[] = {
	&dev_attr_firmware_version.attr,
	&dev_attr_protocol_version.attr,
	&dev_attr_vendor_id.attr,
	&dev_attr_sub_vendor_id.attr,
	NULL,
};
ATTRIBUTE_GROUPS(versions);

 
static const struct of_device_id scmi_of_match[] = {
#ifdef CONFIG_ARM_SCMI_TRANSPORT_MAILBOX
	{ .compatible = "arm,scmi", .data = &scmi_mailbox_desc },
#endif
#ifdef CONFIG_ARM_SCMI_TRANSPORT_OPTEE
	{ .compatible = "linaro,scmi-optee", .data = &scmi_optee_desc },
#endif
#ifdef CONFIG_ARM_SCMI_TRANSPORT_SMC
	{ .compatible = "arm,scmi-smc", .data = &scmi_smc_desc},
	{ .compatible = "arm,scmi-smc-param", .data = &scmi_smc_desc},
#endif
#ifdef CONFIG_ARM_SCMI_TRANSPORT_VIRTIO
	{ .compatible = "arm,scmi-virtio", .data = &scmi_virtio_desc},
#endif
	{   },
};

MODULE_DEVICE_TABLE(of, scmi_of_match);

static struct platform_driver scmi_driver = {
	.driver = {
		   .name = "arm-scmi",
		   .suppress_bind_attrs = true,
		   .of_match_table = scmi_of_match,
		   .dev_groups = versions_groups,
		   },
	.probe = scmi_probe,
	.remove = scmi_remove,
};

 
static inline int __scmi_transports_setup(bool init)
{
	int ret = 0;
	const struct of_device_id *trans;

	for (trans = scmi_of_match; trans->data; trans++) {
		const struct scmi_desc *tdesc = trans->data;

		if ((init && !tdesc->transport_init) ||
		    (!init && !tdesc->transport_exit))
			continue;

		if (init)
			ret = tdesc->transport_init();
		else
			tdesc->transport_exit();

		if (ret) {
			pr_err("SCMI transport %s FAILED initialization!\n",
			       trans->compatible);
			break;
		}
	}

	return ret;
}

static int __init scmi_transports_init(void)
{
	return __scmi_transports_setup(true);
}

static void __exit scmi_transports_exit(void)
{
	__scmi_transports_setup(false);
}

static struct dentry *scmi_debugfs_init(void)
{
	struct dentry *d;

	d = debugfs_create_dir("scmi", NULL);
	if (IS_ERR(d)) {
		pr_err("Could NOT create SCMI top dentry.\n");
		return NULL;
	}

	return d;
}

static int __init scmi_driver_init(void)
{
	int ret;

	 
	if (WARN_ON(!IS_ENABLED(CONFIG_ARM_SCMI_HAVE_TRANSPORT)))
		return -EINVAL;

	 
	ret = scmi_transports_init();
	if (ret)
		return ret;

	if (IS_ENABLED(CONFIG_ARM_SCMI_NEED_DEBUGFS))
		scmi_top_dentry = scmi_debugfs_init();

	scmi_base_register();

	scmi_clock_register();
	scmi_perf_register();
	scmi_power_register();
	scmi_reset_register();
	scmi_sensors_register();
	scmi_voltage_register();
	scmi_system_register();
	scmi_powercap_register();

	return platform_driver_register(&scmi_driver);
}
module_init(scmi_driver_init);

static void __exit scmi_driver_exit(void)
{
	scmi_base_unregister();

	scmi_clock_unregister();
	scmi_perf_unregister();
	scmi_power_unregister();
	scmi_reset_unregister();
	scmi_sensors_unregister();
	scmi_voltage_unregister();
	scmi_system_unregister();
	scmi_powercap_unregister();

	scmi_transports_exit();

	platform_driver_unregister(&scmi_driver);

	debugfs_remove_recursive(scmi_top_dentry);
}
module_exit(scmi_driver_exit);

MODULE_ALIAS("platform:arm-scmi");
MODULE_AUTHOR("Sudeep Holla <sudeep.holla@arm.com>");
MODULE_DESCRIPTION("ARM SCMI protocol driver");
MODULE_LICENSE("GPL v2");
