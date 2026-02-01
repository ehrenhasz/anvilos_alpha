
 

 

#include <linux/completion.h>
#include <linux/errno.h>
#include <linux/refcount.h>
#include <linux/slab.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>

#include <uapi/linux/virtio_ids.h>
#include <uapi/linux/virtio_scmi.h>

#include "common.h"

#define VIRTIO_MAX_RX_TIMEOUT_MS	60000
#define VIRTIO_SCMI_MAX_MSG_SIZE 128  
#define VIRTIO_SCMI_MAX_PDU_SIZE \
	(VIRTIO_SCMI_MAX_MSG_SIZE + SCMI_MSG_MAX_PROT_OVERHEAD)
#define DESCRIPTORS_PER_TX_MSG 2

 
struct scmi_vio_channel {
	struct virtqueue *vqueue;
	struct scmi_chan_info *cinfo;
	 
	spinlock_t free_lock;
	struct list_head free_list;
	 
	spinlock_t pending_lock;
	struct list_head pending_cmds_list;
	struct work_struct deferred_tx_work;
	struct workqueue_struct *deferred_tx_wq;
	bool is_rx;
	unsigned int max_msg;
	 
	spinlock_t lock;
	struct completion *shutdown_done;
	refcount_t users;
};

enum poll_states {
	VIO_MSG_NOT_POLLED,
	VIO_MSG_POLL_TIMEOUT,
	VIO_MSG_POLLING,
	VIO_MSG_POLL_DONE,
};

 
struct scmi_vio_msg {
	struct scmi_msg_payld *request;
	struct scmi_msg_payld *input;
	struct list_head list;
	unsigned int rx_len;
	unsigned int poll_idx;
	enum poll_states poll_status;
	 
	spinlock_t poll_lock;
	refcount_t users;
};

 
static struct virtio_device *scmi_vdev;

static void scmi_vio_channel_ready(struct scmi_vio_channel *vioch,
				   struct scmi_chan_info *cinfo)
{
	unsigned long flags;

	spin_lock_irqsave(&vioch->lock, flags);
	cinfo->transport_info = vioch;
	 
	vioch->cinfo = cinfo;
	spin_unlock_irqrestore(&vioch->lock, flags);

	refcount_set(&vioch->users, 1);
}

static inline bool scmi_vio_channel_acquire(struct scmi_vio_channel *vioch)
{
	return refcount_inc_not_zero(&vioch->users);
}

static inline void scmi_vio_channel_release(struct scmi_vio_channel *vioch)
{
	if (refcount_dec_and_test(&vioch->users)) {
		unsigned long flags;

		spin_lock_irqsave(&vioch->lock, flags);
		if (vioch->shutdown_done) {
			vioch->cinfo = NULL;
			complete(vioch->shutdown_done);
		}
		spin_unlock_irqrestore(&vioch->lock, flags);
	}
}

static void scmi_vio_channel_cleanup_sync(struct scmi_vio_channel *vioch)
{
	unsigned long flags;
	DECLARE_COMPLETION_ONSTACK(vioch_shutdown_done);

	 
	spin_lock_irqsave(&vioch->lock, flags);
	if (!vioch->cinfo || vioch->shutdown_done) {
		spin_unlock_irqrestore(&vioch->lock, flags);
		return;
	}

	vioch->shutdown_done = &vioch_shutdown_done;
	if (!vioch->is_rx && vioch->deferred_tx_wq)
		 
		vioch->deferred_tx_wq = NULL;
	spin_unlock_irqrestore(&vioch->lock, flags);

	scmi_vio_channel_release(vioch);

	 
	wait_for_completion(vioch->shutdown_done);
}

 
static struct scmi_vio_msg *
scmi_virtio_get_free_msg(struct scmi_vio_channel *vioch)
{
	unsigned long flags;
	struct scmi_vio_msg *msg;

	spin_lock_irqsave(&vioch->free_lock, flags);
	if (list_empty(&vioch->free_list)) {
		spin_unlock_irqrestore(&vioch->free_lock, flags);
		return NULL;
	}

	msg = list_first_entry(&vioch->free_list, typeof(*msg), list);
	list_del_init(&msg->list);
	spin_unlock_irqrestore(&vioch->free_lock, flags);

	 
	msg->poll_status = VIO_MSG_NOT_POLLED;
	refcount_set(&msg->users, 1);

	return msg;
}

static inline bool scmi_vio_msg_acquire(struct scmi_vio_msg *msg)
{
	return refcount_inc_not_zero(&msg->users);
}

 
static inline bool scmi_vio_msg_release(struct scmi_vio_channel *vioch,
					struct scmi_vio_msg *msg)
{
	bool ret;

	ret = refcount_dec_and_test(&msg->users);
	if (ret) {
		unsigned long flags;

		spin_lock_irqsave(&vioch->free_lock, flags);
		list_add_tail(&msg->list, &vioch->free_list);
		spin_unlock_irqrestore(&vioch->free_lock, flags);
	}

	return ret;
}

static bool scmi_vio_have_vq_rx(struct virtio_device *vdev)
{
	return virtio_has_feature(vdev, VIRTIO_SCMI_F_P2A_CHANNELS);
}

static int scmi_vio_feed_vq_rx(struct scmi_vio_channel *vioch,
			       struct scmi_vio_msg *msg)
{
	struct scatterlist sg_in;
	int rc;
	unsigned long flags;
	struct device *dev = &vioch->vqueue->vdev->dev;

	sg_init_one(&sg_in, msg->input, VIRTIO_SCMI_MAX_PDU_SIZE);

	spin_lock_irqsave(&vioch->lock, flags);

	rc = virtqueue_add_inbuf(vioch->vqueue, &sg_in, 1, msg, GFP_ATOMIC);
	if (rc)
		dev_err(dev, "failed to add to RX virtqueue (%d)\n", rc);
	else
		virtqueue_kick(vioch->vqueue);

	spin_unlock_irqrestore(&vioch->lock, flags);

	return rc;
}

 
static void scmi_finalize_message(struct scmi_vio_channel *vioch,
				  struct scmi_vio_msg *msg)
{
	if (vioch->is_rx)
		scmi_vio_feed_vq_rx(vioch, msg);
	else
		scmi_vio_msg_release(vioch, msg);
}

static void scmi_vio_complete_cb(struct virtqueue *vqueue)
{
	unsigned long flags;
	unsigned int length;
	struct scmi_vio_channel *vioch;
	struct scmi_vio_msg *msg;
	bool cb_enabled = true;

	if (WARN_ON_ONCE(!vqueue->vdev->priv))
		return;
	vioch = &((struct scmi_vio_channel *)vqueue->vdev->priv)[vqueue->index];

	for (;;) {
		if (!scmi_vio_channel_acquire(vioch))
			return;

		spin_lock_irqsave(&vioch->lock, flags);
		if (cb_enabled) {
			virtqueue_disable_cb(vqueue);
			cb_enabled = false;
		}

		msg = virtqueue_get_buf(vqueue, &length);
		if (!msg) {
			if (virtqueue_enable_cb(vqueue)) {
				spin_unlock_irqrestore(&vioch->lock, flags);
				scmi_vio_channel_release(vioch);
				return;
			}
			cb_enabled = true;
		}
		spin_unlock_irqrestore(&vioch->lock, flags);

		if (msg) {
			msg->rx_len = length;
			scmi_rx_callback(vioch->cinfo,
					 msg_read_header(msg->input), msg);

			scmi_finalize_message(vioch, msg);
		}

		 
		scmi_vio_channel_release(vioch);
	}
}

static void scmi_vio_deferred_tx_worker(struct work_struct *work)
{
	unsigned long flags;
	struct scmi_vio_channel *vioch;
	struct scmi_vio_msg *msg, *tmp;

	vioch = container_of(work, struct scmi_vio_channel, deferred_tx_work);

	if (!scmi_vio_channel_acquire(vioch))
		return;

	 
	spin_lock_irqsave(&vioch->pending_lock, flags);

	 
	list_for_each_entry_safe(msg, tmp, &vioch->pending_cmds_list, list) {
		list_del(&msg->list);

		 
		if (msg->poll_status == VIO_MSG_NOT_POLLED)
			scmi_rx_callback(vioch->cinfo,
					 msg_read_header(msg->input), msg);

		 
		scmi_vio_msg_release(vioch, msg);
	}

	spin_unlock_irqrestore(&vioch->pending_lock, flags);

	 
	scmi_vio_complete_cb(vioch->vqueue);

	scmi_vio_channel_release(vioch);
}

static const char *const scmi_vio_vqueue_names[] = { "tx", "rx" };

static vq_callback_t *scmi_vio_complete_callbacks[] = {
	scmi_vio_complete_cb,
	scmi_vio_complete_cb
};

static unsigned int virtio_get_max_msg(struct scmi_chan_info *base_cinfo)
{
	struct scmi_vio_channel *vioch = base_cinfo->transport_info;

	return vioch->max_msg;
}

static int virtio_link_supplier(struct device *dev)
{
	if (!scmi_vdev) {
		dev_notice(dev,
			   "Deferring probe after not finding a bound scmi-virtio device\n");
		return -EPROBE_DEFER;
	}

	if (!device_link_add(dev, &scmi_vdev->dev,
			     DL_FLAG_AUTOREMOVE_CONSUMER)) {
		dev_err(dev, "Adding link to supplier virtio device failed\n");
		return -ECANCELED;
	}

	return 0;
}

static bool virtio_chan_available(struct device_node *of_node, int idx)
{
	struct scmi_vio_channel *channels, *vioch = NULL;

	if (WARN_ON_ONCE(!scmi_vdev))
		return false;

	channels = (struct scmi_vio_channel *)scmi_vdev->priv;

	switch (idx) {
	case VIRTIO_SCMI_VQ_TX:
		vioch = &channels[VIRTIO_SCMI_VQ_TX];
		break;
	case VIRTIO_SCMI_VQ_RX:
		if (scmi_vio_have_vq_rx(scmi_vdev))
			vioch = &channels[VIRTIO_SCMI_VQ_RX];
		break;
	default:
		return false;
	}

	return vioch && !vioch->cinfo;
}

static void scmi_destroy_tx_workqueue(void *deferred_tx_wq)
{
	destroy_workqueue(deferred_tx_wq);
}

static int virtio_chan_setup(struct scmi_chan_info *cinfo, struct device *dev,
			     bool tx)
{
	struct scmi_vio_channel *vioch;
	int index = tx ? VIRTIO_SCMI_VQ_TX : VIRTIO_SCMI_VQ_RX;
	int i;

	if (!scmi_vdev)
		return -EPROBE_DEFER;

	vioch = &((struct scmi_vio_channel *)scmi_vdev->priv)[index];

	 
	if (tx && !vioch->deferred_tx_wq) {
		int ret;

		vioch->deferred_tx_wq =
			alloc_workqueue(dev_name(&scmi_vdev->dev),
					WQ_UNBOUND | WQ_FREEZABLE | WQ_SYSFS,
					0);
		if (!vioch->deferred_tx_wq)
			return -ENOMEM;

		ret = devm_add_action_or_reset(dev, scmi_destroy_tx_workqueue,
					       vioch->deferred_tx_wq);
		if (ret)
			return ret;

		INIT_WORK(&vioch->deferred_tx_work,
			  scmi_vio_deferred_tx_worker);
	}

	for (i = 0; i < vioch->max_msg; i++) {
		struct scmi_vio_msg *msg;

		msg = devm_kzalloc(dev, sizeof(*msg), GFP_KERNEL);
		if (!msg)
			return -ENOMEM;

		if (tx) {
			msg->request = devm_kzalloc(dev,
						    VIRTIO_SCMI_MAX_PDU_SIZE,
						    GFP_KERNEL);
			if (!msg->request)
				return -ENOMEM;
			spin_lock_init(&msg->poll_lock);
			refcount_set(&msg->users, 1);
		}

		msg->input = devm_kzalloc(dev, VIRTIO_SCMI_MAX_PDU_SIZE,
					  GFP_KERNEL);
		if (!msg->input)
			return -ENOMEM;

		scmi_finalize_message(vioch, msg);
	}

	scmi_vio_channel_ready(vioch, cinfo);

	return 0;
}

static int virtio_chan_free(int id, void *p, void *data)
{
	struct scmi_chan_info *cinfo = p;
	struct scmi_vio_channel *vioch = cinfo->transport_info;

	 
	virtio_break_device(vioch->vqueue->vdev);
	scmi_vio_channel_cleanup_sync(vioch);

	return 0;
}

static int virtio_send_message(struct scmi_chan_info *cinfo,
			       struct scmi_xfer *xfer)
{
	struct scmi_vio_channel *vioch = cinfo->transport_info;
	struct scatterlist sg_out;
	struct scatterlist sg_in;
	struct scatterlist *sgs[DESCRIPTORS_PER_TX_MSG] = { &sg_out, &sg_in };
	unsigned long flags;
	int rc;
	struct scmi_vio_msg *msg;

	if (!scmi_vio_channel_acquire(vioch))
		return -EINVAL;

	msg = scmi_virtio_get_free_msg(vioch);
	if (!msg) {
		scmi_vio_channel_release(vioch);
		return -EBUSY;
	}

	msg_tx_prepare(msg->request, xfer);

	sg_init_one(&sg_out, msg->request, msg_command_size(xfer));
	sg_init_one(&sg_in, msg->input, msg_response_size(xfer));

	spin_lock_irqsave(&vioch->lock, flags);

	 
	if (xfer->hdr.poll_completion) {
		msg->poll_idx = virtqueue_enable_cb_prepare(vioch->vqueue);
		 
		msg->poll_status = VIO_MSG_POLLING;
		scmi_vio_msg_acquire(msg);
		 
		smp_store_mb(xfer->priv, msg);
	}

	rc = virtqueue_add_sgs(vioch->vqueue, sgs, 1, 1, msg, GFP_ATOMIC);
	if (rc)
		dev_err(vioch->cinfo->dev,
			"failed to add to TX virtqueue (%d)\n", rc);
	else
		virtqueue_kick(vioch->vqueue);

	spin_unlock_irqrestore(&vioch->lock, flags);

	if (rc) {
		 
		smp_store_mb(xfer->priv, NULL);
		if (xfer->hdr.poll_completion)
			scmi_vio_msg_release(vioch, msg);
		scmi_vio_msg_release(vioch, msg);
	}

	scmi_vio_channel_release(vioch);

	return rc;
}

static void virtio_fetch_response(struct scmi_chan_info *cinfo,
				  struct scmi_xfer *xfer)
{
	struct scmi_vio_msg *msg = xfer->priv;

	if (msg)
		msg_fetch_response(msg->input, msg->rx_len, xfer);
}

static void virtio_fetch_notification(struct scmi_chan_info *cinfo,
				      size_t max_len, struct scmi_xfer *xfer)
{
	struct scmi_vio_msg *msg = xfer->priv;

	if (msg)
		msg_fetch_notification(msg->input, msg->rx_len, max_len, xfer);
}

 
static void virtio_mark_txdone(struct scmi_chan_info *cinfo, int ret,
			       struct scmi_xfer *xfer)
{
	unsigned long flags;
	struct scmi_vio_channel *vioch = cinfo->transport_info;
	struct scmi_vio_msg *msg = xfer->priv;

	if (!msg || !scmi_vio_channel_acquire(vioch))
		return;

	 
	smp_store_mb(xfer->priv, NULL);

	 
	if (!xfer->hdr.poll_completion || scmi_vio_msg_release(vioch, msg)) {
		scmi_vio_channel_release(vioch);
		return;
	}

	spin_lock_irqsave(&msg->poll_lock, flags);
	 
	if (ret != -ETIMEDOUT || msg->poll_status == VIO_MSG_POLL_DONE)
		scmi_vio_msg_release(vioch, msg);
	else if (msg->poll_status == VIO_MSG_POLLING)
		msg->poll_status = VIO_MSG_POLL_TIMEOUT;
	spin_unlock_irqrestore(&msg->poll_lock, flags);

	scmi_vio_channel_release(vioch);
}

 
static bool virtio_poll_done(struct scmi_chan_info *cinfo,
			     struct scmi_xfer *xfer)
{
	bool pending, found = false;
	unsigned int length, any_prefetched = 0;
	unsigned long flags;
	struct scmi_vio_msg *next_msg, *msg = xfer->priv;
	struct scmi_vio_channel *vioch = cinfo->transport_info;

	if (!msg)
		return true;

	 
	if (msg->poll_status == VIO_MSG_POLL_DONE)
		return true;

	if (!scmi_vio_channel_acquire(vioch))
		return true;

	 
	pending = virtqueue_poll(vioch->vqueue, msg->poll_idx);
	if (!pending) {
		scmi_vio_channel_release(vioch);
		return false;
	}

	spin_lock_irqsave(&vioch->lock, flags);
	virtqueue_disable_cb(vioch->vqueue);

	 
	while ((next_msg = virtqueue_get_buf(vioch->vqueue, &length))) {
		bool next_msg_done = false;

		 
		spin_lock(&next_msg->poll_lock);
		if (next_msg->poll_status == VIO_MSG_POLLING) {
			next_msg->poll_status = VIO_MSG_POLL_DONE;
			next_msg_done = true;
		}
		spin_unlock(&next_msg->poll_lock);

		next_msg->rx_len = length;
		 
		if (next_msg == msg) {
			found = true;
			break;
		} else if (next_msg_done) {
			 
			continue;
		}

		 
		spin_lock(&next_msg->poll_lock);
		if (next_msg->poll_status == VIO_MSG_NOT_POLLED ||
		    next_msg->poll_status == VIO_MSG_POLL_TIMEOUT) {
			spin_unlock(&next_msg->poll_lock);

			any_prefetched++;
			spin_lock(&vioch->pending_lock);
			list_add_tail(&next_msg->list,
				      &vioch->pending_cmds_list);
			spin_unlock(&vioch->pending_lock);
		} else {
			spin_unlock(&next_msg->poll_lock);
		}
	}

	 
	if (found) {
		pending = !virtqueue_enable_cb(vioch->vqueue);
	} else {
		msg->poll_idx = virtqueue_enable_cb_prepare(vioch->vqueue);
		pending = virtqueue_poll(vioch->vqueue, msg->poll_idx);
	}

	if (vioch->deferred_tx_wq && (any_prefetched || pending))
		queue_work(vioch->deferred_tx_wq, &vioch->deferred_tx_work);

	spin_unlock_irqrestore(&vioch->lock, flags);

	scmi_vio_channel_release(vioch);

	return found;
}

static const struct scmi_transport_ops scmi_virtio_ops = {
	.link_supplier = virtio_link_supplier,
	.chan_available = virtio_chan_available,
	.chan_setup = virtio_chan_setup,
	.chan_free = virtio_chan_free,
	.get_max_msg = virtio_get_max_msg,
	.send_message = virtio_send_message,
	.fetch_response = virtio_fetch_response,
	.fetch_notification = virtio_fetch_notification,
	.mark_txdone = virtio_mark_txdone,
	.poll_done = virtio_poll_done,
};

static int scmi_vio_probe(struct virtio_device *vdev)
{
	struct device *dev = &vdev->dev;
	struct scmi_vio_channel *channels;
	bool have_vq_rx;
	int vq_cnt;
	int i;
	int ret;
	struct virtqueue *vqs[VIRTIO_SCMI_VQ_MAX_CNT];

	 
	if (scmi_vdev) {
		dev_err(dev,
			"One SCMI Virtio device was already initialized: only one allowed.\n");
		return -EBUSY;
	}

	have_vq_rx = scmi_vio_have_vq_rx(vdev);
	vq_cnt = have_vq_rx ? VIRTIO_SCMI_VQ_MAX_CNT : 1;

	channels = devm_kcalloc(dev, vq_cnt, sizeof(*channels), GFP_KERNEL);
	if (!channels)
		return -ENOMEM;

	if (have_vq_rx)
		channels[VIRTIO_SCMI_VQ_RX].is_rx = true;

	ret = virtio_find_vqs(vdev, vq_cnt, vqs, scmi_vio_complete_callbacks,
			      scmi_vio_vqueue_names, NULL);
	if (ret) {
		dev_err(dev, "Failed to get %d virtqueue(s)\n", vq_cnt);
		return ret;
	}

	for (i = 0; i < vq_cnt; i++) {
		unsigned int sz;

		spin_lock_init(&channels[i].lock);
		spin_lock_init(&channels[i].free_lock);
		INIT_LIST_HEAD(&channels[i].free_list);
		spin_lock_init(&channels[i].pending_lock);
		INIT_LIST_HEAD(&channels[i].pending_cmds_list);
		channels[i].vqueue = vqs[i];

		sz = virtqueue_get_vring_size(channels[i].vqueue);
		 
		if (!channels[i].is_rx)
			sz /= DESCRIPTORS_PER_TX_MSG;

		if (sz > MSG_TOKEN_MAX) {
			dev_info(dev,
				 "%s virtqueue could hold %d messages. Only %ld allowed to be pending.\n",
				 channels[i].is_rx ? "rx" : "tx",
				 sz, MSG_TOKEN_MAX);
			sz = MSG_TOKEN_MAX;
		}
		channels[i].max_msg = sz;
	}

	vdev->priv = channels;
	 
	smp_store_mb(scmi_vdev, vdev);

	return 0;
}

static void scmi_vio_remove(struct virtio_device *vdev)
{
	 
	virtio_reset_device(vdev);
	vdev->config->del_vqs(vdev);
	 
	smp_store_mb(scmi_vdev, NULL);
}

static int scmi_vio_validate(struct virtio_device *vdev)
{
#ifdef CONFIG_ARM_SCMI_TRANSPORT_VIRTIO_VERSION1_COMPLIANCE
	if (!virtio_has_feature(vdev, VIRTIO_F_VERSION_1)) {
		dev_err(&vdev->dev,
			"device does not comply with spec version 1.x\n");
		return -EINVAL;
	}
#endif
	return 0;
}

static unsigned int features[] = {
	VIRTIO_SCMI_F_P2A_CHANNELS,
};

static const struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_SCMI, VIRTIO_DEV_ANY_ID },
	{ 0 }
};

static struct virtio_driver virtio_scmi_driver = {
	.driver.name = "scmi-virtio",
	.driver.owner = THIS_MODULE,
	.feature_table = features,
	.feature_table_size = ARRAY_SIZE(features),
	.id_table = id_table,
	.probe = scmi_vio_probe,
	.remove = scmi_vio_remove,
	.validate = scmi_vio_validate,
};

static int __init virtio_scmi_init(void)
{
	return register_virtio_driver(&virtio_scmi_driver);
}

static void virtio_scmi_exit(void)
{
	unregister_virtio_driver(&virtio_scmi_driver);
}

const struct scmi_desc scmi_virtio_desc = {
	.transport_init = virtio_scmi_init,
	.transport_exit = virtio_scmi_exit,
	.ops = &scmi_virtio_ops,
	 
	.max_rx_timeout_ms = VIRTIO_MAX_RX_TIMEOUT_MS,
	.max_msg = 0,  
	.max_msg_size = VIRTIO_SCMI_MAX_MSG_SIZE,
	.atomic_enabled = IS_ENABLED(CONFIG_ARM_SCMI_TRANSPORT_VIRTIO_ATOMIC_ENABLE),
};
