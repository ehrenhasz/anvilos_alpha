
 

#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/rpmsg.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/mailbox_client.h>

#include "rpmsg_internal.h"
#include "qcom_glink_native.h"

#define GLINK_NAME_SIZE		32
#define GLINK_VERSION_1		1

#define RPM_GLINK_CID_MIN	1
#define RPM_GLINK_CID_MAX	65536

struct glink_msg {
	__le16 cmd;
	__le16 param1;
	__le32 param2;
	u8 data[];
} __packed;

 
struct glink_defer_cmd {
	struct list_head node;

	struct glink_msg msg;
	u8 data[];
};

 
struct glink_core_rx_intent {
	void *data;
	u32 id;
	size_t size;
	bool reuse;
	bool in_use;
	u32 offset;

	struct list_head node;
};

 
struct qcom_glink {
	struct device *dev;

	struct qcom_glink_pipe *rx_pipe;
	struct qcom_glink_pipe *tx_pipe;

	struct work_struct rx_work;
	spinlock_t rx_lock;
	struct list_head rx_queue;

	spinlock_t tx_lock;

	spinlock_t idr_lock;
	struct idr lcids;
	struct idr rcids;
	unsigned long features;

	bool intentless;
	wait_queue_head_t tx_avail_notify;
	bool sent_read_notify;

	bool abort_tx;
};

enum {
	GLINK_STATE_CLOSED,
	GLINK_STATE_OPENING,
	GLINK_STATE_OPEN,
	GLINK_STATE_CLOSING,
};

 
struct glink_channel {
	struct rpmsg_endpoint ept;

	struct rpmsg_device *rpdev;
	struct qcom_glink *glink;

	struct kref refcount;

	spinlock_t recv_lock;

	char *name;
	unsigned int lcid;
	unsigned int rcid;

	spinlock_t intent_lock;
	struct idr liids;
	struct idr riids;
	struct work_struct intent_work;
	struct list_head done_intents;

	struct glink_core_rx_intent *buf;
	int buf_offset;
	int buf_size;

	struct completion open_ack;
	struct completion open_req;

	struct mutex intent_req_lock;
	int intent_req_result;
	bool intent_received;
	wait_queue_head_t intent_req_wq;
};

#define to_glink_channel(_ept) container_of(_ept, struct glink_channel, ept)

static const struct rpmsg_endpoint_ops glink_endpoint_ops;

#define GLINK_CMD_VERSION		0
#define GLINK_CMD_VERSION_ACK		1
#define GLINK_CMD_OPEN			2
#define GLINK_CMD_CLOSE			3
#define GLINK_CMD_OPEN_ACK		4
#define GLINK_CMD_INTENT		5
#define GLINK_CMD_RX_DONE		6
#define GLINK_CMD_RX_INTENT_REQ		7
#define GLINK_CMD_RX_INTENT_REQ_ACK	8
#define GLINK_CMD_TX_DATA		9
#define GLINK_CMD_CLOSE_ACK		11
#define GLINK_CMD_TX_DATA_CONT		12
#define GLINK_CMD_READ_NOTIF		13
#define GLINK_CMD_RX_DONE_W_REUSE	14
#define GLINK_CMD_SIGNALS		15

#define GLINK_FEATURE_INTENTLESS	BIT(1)

#define NATIVE_DTR_SIG			NATIVE_DSR_SIG
#define NATIVE_DSR_SIG			BIT(31)
#define NATIVE_RTS_SIG			NATIVE_CTS_SIG
#define NATIVE_CTS_SIG			BIT(30)

static void qcom_glink_rx_done_work(struct work_struct *work);

static struct glink_channel *qcom_glink_alloc_channel(struct qcom_glink *glink,
						      const char *name)
{
	struct glink_channel *channel;

	channel = kzalloc(sizeof(*channel), GFP_KERNEL);
	if (!channel)
		return ERR_PTR(-ENOMEM);

	 
	spin_lock_init(&channel->recv_lock);
	spin_lock_init(&channel->intent_lock);
	mutex_init(&channel->intent_req_lock);

	channel->glink = glink;
	channel->name = kstrdup(name, GFP_KERNEL);
	if (!channel->name) {
		kfree(channel);
		return ERR_PTR(-ENOMEM);
	}

	init_completion(&channel->open_req);
	init_completion(&channel->open_ack);
	init_waitqueue_head(&channel->intent_req_wq);

	INIT_LIST_HEAD(&channel->done_intents);
	INIT_WORK(&channel->intent_work, qcom_glink_rx_done_work);

	idr_init(&channel->liids);
	idr_init(&channel->riids);
	kref_init(&channel->refcount);

	return channel;
}

static void qcom_glink_channel_release(struct kref *ref)
{
	struct glink_channel *channel = container_of(ref, struct glink_channel,
						     refcount);
	struct glink_core_rx_intent *intent;
	struct glink_core_rx_intent *tmp;
	unsigned long flags;
	int iid;

	 
	cancel_work_sync(&channel->intent_work);

	spin_lock_irqsave(&channel->intent_lock, flags);
	 
	list_for_each_entry_safe(intent, tmp, &channel->done_intents, node) {
		if (!intent->reuse) {
			kfree(intent->data);
			kfree(intent);
		}
	}

	idr_for_each_entry(&channel->liids, tmp, iid) {
		kfree(tmp->data);
		kfree(tmp);
	}
	idr_destroy(&channel->liids);

	idr_for_each_entry(&channel->riids, tmp, iid)
		kfree(tmp);
	idr_destroy(&channel->riids);
	spin_unlock_irqrestore(&channel->intent_lock, flags);

	kfree(channel->name);
	kfree(channel);
}

static size_t qcom_glink_rx_avail(struct qcom_glink *glink)
{
	return glink->rx_pipe->avail(glink->rx_pipe);
}

static void qcom_glink_rx_peek(struct qcom_glink *glink,
			       void *data, unsigned int offset, size_t count)
{
	glink->rx_pipe->peek(glink->rx_pipe, data, offset, count);
}

static void qcom_glink_rx_advance(struct qcom_glink *glink, size_t count)
{
	glink->rx_pipe->advance(glink->rx_pipe, count);
}

static size_t qcom_glink_tx_avail(struct qcom_glink *glink)
{
	return glink->tx_pipe->avail(glink->tx_pipe);
}

static void qcom_glink_tx_write(struct qcom_glink *glink,
				const void *hdr, size_t hlen,
				const void *data, size_t dlen)
{
	glink->tx_pipe->write(glink->tx_pipe, hdr, hlen, data, dlen);
}

static void qcom_glink_tx_kick(struct qcom_glink *glink)
{
	glink->tx_pipe->kick(glink->tx_pipe);
}

static void qcom_glink_send_read_notify(struct qcom_glink *glink)
{
	struct glink_msg msg;

	msg.cmd = cpu_to_le16(GLINK_CMD_READ_NOTIF);
	msg.param1 = 0;
	msg.param2 = 0;

	qcom_glink_tx_write(glink, &msg, sizeof(msg), NULL, 0);

	qcom_glink_tx_kick(glink);
}

static int qcom_glink_tx(struct qcom_glink *glink,
			 const void *hdr, size_t hlen,
			 const void *data, size_t dlen, bool wait)
{
	unsigned int tlen = hlen + dlen;
	unsigned long flags;
	int ret = 0;

	 
	if (tlen >= glink->tx_pipe->length)
		return -EINVAL;

	spin_lock_irqsave(&glink->tx_lock, flags);

	if (glink->abort_tx) {
		ret = -EIO;
		goto out;
	}

	while (qcom_glink_tx_avail(glink) < tlen) {
		if (!wait) {
			ret = -EAGAIN;
			goto out;
		}

		if (glink->abort_tx) {
			ret = -EIO;
			goto out;
		}

		if (!glink->sent_read_notify) {
			glink->sent_read_notify = true;
			qcom_glink_send_read_notify(glink);
		}

		 
		spin_unlock_irqrestore(&glink->tx_lock, flags);

		wait_event_timeout(glink->tx_avail_notify,
				   qcom_glink_tx_avail(glink) >= tlen, 10 * HZ);

		spin_lock_irqsave(&glink->tx_lock, flags);

		if (qcom_glink_tx_avail(glink) >= tlen)
			glink->sent_read_notify = false;
	}

	qcom_glink_tx_write(glink, hdr, hlen, data, dlen);
	qcom_glink_tx_kick(glink);

out:
	spin_unlock_irqrestore(&glink->tx_lock, flags);

	return ret;
}

static int qcom_glink_send_version(struct qcom_glink *glink)
{
	struct glink_msg msg;

	msg.cmd = cpu_to_le16(GLINK_CMD_VERSION);
	msg.param1 = cpu_to_le16(GLINK_VERSION_1);
	msg.param2 = cpu_to_le32(glink->features);

	return qcom_glink_tx(glink, &msg, sizeof(msg), NULL, 0, true);
}

static void qcom_glink_send_version_ack(struct qcom_glink *glink)
{
	struct glink_msg msg;

	msg.cmd = cpu_to_le16(GLINK_CMD_VERSION_ACK);
	msg.param1 = cpu_to_le16(GLINK_VERSION_1);
	msg.param2 = cpu_to_le32(glink->features);

	qcom_glink_tx(glink, &msg, sizeof(msg), NULL, 0, true);
}

static void qcom_glink_send_open_ack(struct qcom_glink *glink,
				     struct glink_channel *channel)
{
	struct glink_msg msg;

	msg.cmd = cpu_to_le16(GLINK_CMD_OPEN_ACK);
	msg.param1 = cpu_to_le16(channel->rcid);
	msg.param2 = cpu_to_le32(0);

	qcom_glink_tx(glink, &msg, sizeof(msg), NULL, 0, true);
}

static void qcom_glink_handle_intent_req_ack(struct qcom_glink *glink,
					     unsigned int cid, bool granted)
{
	struct glink_channel *channel;
	unsigned long flags;

	spin_lock_irqsave(&glink->idr_lock, flags);
	channel = idr_find(&glink->rcids, cid);
	spin_unlock_irqrestore(&glink->idr_lock, flags);
	if (!channel) {
		dev_err(glink->dev, "unable to find channel\n");
		return;
	}

	WRITE_ONCE(channel->intent_req_result, granted);
	wake_up_all(&channel->intent_req_wq);
}

static void qcom_glink_intent_req_abort(struct glink_channel *channel)
{
	WRITE_ONCE(channel->intent_req_result, 0);
	wake_up_all(&channel->intent_req_wq);
}

 
static int qcom_glink_send_open_req(struct qcom_glink *glink,
				    struct glink_channel *channel)
{
	struct {
		struct glink_msg msg;
		u8 name[GLINK_NAME_SIZE];
	} __packed req;
	int name_len = strlen(channel->name) + 1;
	int req_len = ALIGN(sizeof(req.msg) + name_len, 8);
	int ret;
	unsigned long flags;

	kref_get(&channel->refcount);

	spin_lock_irqsave(&glink->idr_lock, flags);
	ret = idr_alloc_cyclic(&glink->lcids, channel,
			       RPM_GLINK_CID_MIN, RPM_GLINK_CID_MAX,
			       GFP_ATOMIC);
	spin_unlock_irqrestore(&glink->idr_lock, flags);
	if (ret < 0)
		return ret;

	channel->lcid = ret;

	req.msg.cmd = cpu_to_le16(GLINK_CMD_OPEN);
	req.msg.param1 = cpu_to_le16(channel->lcid);
	req.msg.param2 = cpu_to_le32(name_len);
	strcpy(req.name, channel->name);

	ret = qcom_glink_tx(glink, &req, req_len, NULL, 0, true);
	if (ret)
		goto remove_idr;

	return 0;

remove_idr:
	spin_lock_irqsave(&glink->idr_lock, flags);
	idr_remove(&glink->lcids, channel->lcid);
	channel->lcid = 0;
	spin_unlock_irqrestore(&glink->idr_lock, flags);

	return ret;
}

static void qcom_glink_send_close_req(struct qcom_glink *glink,
				      struct glink_channel *channel)
{
	struct glink_msg req;

	req.cmd = cpu_to_le16(GLINK_CMD_CLOSE);
	req.param1 = cpu_to_le16(channel->lcid);
	req.param2 = 0;

	qcom_glink_tx(glink, &req, sizeof(req), NULL, 0, true);
}

static void qcom_glink_send_close_ack(struct qcom_glink *glink,
				      unsigned int rcid)
{
	struct glink_msg req;

	req.cmd = cpu_to_le16(GLINK_CMD_CLOSE_ACK);
	req.param1 = cpu_to_le16(rcid);
	req.param2 = 0;

	qcom_glink_tx(glink, &req, sizeof(req), NULL, 0, true);
}

static void qcom_glink_rx_done_work(struct work_struct *work)
{
	struct glink_channel *channel = container_of(work, struct glink_channel,
						     intent_work);
	struct qcom_glink *glink = channel->glink;
	struct glink_core_rx_intent *intent, *tmp;
	struct {
		u16 id;
		u16 lcid;
		u32 liid;
	} __packed cmd;

	unsigned int cid = channel->lcid;
	unsigned int iid;
	bool reuse;
	unsigned long flags;

	spin_lock_irqsave(&channel->intent_lock, flags);
	list_for_each_entry_safe(intent, tmp, &channel->done_intents, node) {
		list_del(&intent->node);
		spin_unlock_irqrestore(&channel->intent_lock, flags);
		iid = intent->id;
		reuse = intent->reuse;

		cmd.id = reuse ? GLINK_CMD_RX_DONE_W_REUSE : GLINK_CMD_RX_DONE;
		cmd.lcid = cid;
		cmd.liid = iid;

		qcom_glink_tx(glink, &cmd, sizeof(cmd), NULL, 0, true);
		if (!reuse) {
			kfree(intent->data);
			kfree(intent);
		}
		spin_lock_irqsave(&channel->intent_lock, flags);
	}
	spin_unlock_irqrestore(&channel->intent_lock, flags);
}

static void qcom_glink_rx_done(struct qcom_glink *glink,
			       struct glink_channel *channel,
			       struct glink_core_rx_intent *intent)
{
	 
	if (glink->intentless) {
		kfree(intent->data);
		kfree(intent);
		return;
	}

	 
	if (!intent->reuse) {
		spin_lock(&channel->intent_lock);
		idr_remove(&channel->liids, intent->id);
		spin_unlock(&channel->intent_lock);
	}

	 
	spin_lock(&channel->intent_lock);
	list_add_tail(&intent->node, &channel->done_intents);
	spin_unlock(&channel->intent_lock);

	schedule_work(&channel->intent_work);
}

 
static void qcom_glink_receive_version(struct qcom_glink *glink,
				       u32 version,
				       u32 features)
{
	switch (version) {
	case 0:
		break;
	case GLINK_VERSION_1:
		glink->features &= features;
		fallthrough;
	default:
		qcom_glink_send_version_ack(glink);
		break;
	}
}

 
static void qcom_glink_receive_version_ack(struct qcom_glink *glink,
					   u32 version,
					   u32 features)
{
	switch (version) {
	case 0:
		 
		break;
	case GLINK_VERSION_1:
		if (features == glink->features)
			break;

		glink->features &= features;
		fallthrough;
	default:
		qcom_glink_send_version(glink);
		break;
	}
}

 
static int qcom_glink_send_intent_req_ack(struct qcom_glink *glink,
					  struct glink_channel *channel,
					  bool granted)
{
	struct glink_msg msg;

	msg.cmd = cpu_to_le16(GLINK_CMD_RX_INTENT_REQ_ACK);
	msg.param1 = cpu_to_le16(channel->lcid);
	msg.param2 = cpu_to_le32(granted);

	qcom_glink_tx(glink, &msg, sizeof(msg), NULL, 0, true);

	return 0;
}

 
static int qcom_glink_advertise_intent(struct qcom_glink *glink,
				       struct glink_channel *channel,
				       struct glink_core_rx_intent *intent)
{
	struct command {
		__le16 id;
		__le16 lcid;
		__le32 count;
		__le32 size;
		__le32 liid;
	} __packed;
	struct command cmd;

	cmd.id = cpu_to_le16(GLINK_CMD_INTENT);
	cmd.lcid = cpu_to_le16(channel->lcid);
	cmd.count = cpu_to_le32(1);
	cmd.size = cpu_to_le32(intent->size);
	cmd.liid = cpu_to_le32(intent->id);

	qcom_glink_tx(glink, &cmd, sizeof(cmd), NULL, 0, true);

	return 0;
}

static struct glink_core_rx_intent *
qcom_glink_alloc_intent(struct qcom_glink *glink,
			struct glink_channel *channel,
			size_t size,
			bool reuseable)
{
	struct glink_core_rx_intent *intent;
	int ret;
	unsigned long flags;

	intent = kzalloc(sizeof(*intent), GFP_KERNEL);
	if (!intent)
		return NULL;

	intent->data = kzalloc(size, GFP_KERNEL);
	if (!intent->data)
		goto free_intent;

	spin_lock_irqsave(&channel->intent_lock, flags);
	ret = idr_alloc_cyclic(&channel->liids, intent, 1, -1, GFP_ATOMIC);
	if (ret < 0) {
		spin_unlock_irqrestore(&channel->intent_lock, flags);
		goto free_data;
	}
	spin_unlock_irqrestore(&channel->intent_lock, flags);

	intent->id = ret;
	intent->size = size;
	intent->reuse = reuseable;

	return intent;

free_data:
	kfree(intent->data);
free_intent:
	kfree(intent);
	return NULL;
}

static void qcom_glink_handle_rx_done(struct qcom_glink *glink,
				      u32 cid, uint32_t iid,
				      bool reuse)
{
	struct glink_core_rx_intent *intent;
	struct glink_channel *channel;
	unsigned long flags;

	spin_lock_irqsave(&glink->idr_lock, flags);
	channel = idr_find(&glink->rcids, cid);
	spin_unlock_irqrestore(&glink->idr_lock, flags);
	if (!channel) {
		dev_err(glink->dev, "invalid channel id received\n");
		return;
	}

	spin_lock_irqsave(&channel->intent_lock, flags);
	intent = idr_find(&channel->riids, iid);

	if (!intent) {
		spin_unlock_irqrestore(&channel->intent_lock, flags);
		dev_err(glink->dev, "invalid intent id received\n");
		return;
	}

	intent->in_use = false;

	if (!reuse) {
		idr_remove(&channel->riids, intent->id);
		kfree(intent);
	}
	spin_unlock_irqrestore(&channel->intent_lock, flags);

	if (reuse) {
		WRITE_ONCE(channel->intent_received, true);
		wake_up_all(&channel->intent_req_wq);
	}
}

 
static void qcom_glink_handle_intent_req(struct qcom_glink *glink,
					 u32 cid, size_t size)
{
	struct glink_core_rx_intent *intent;
	struct glink_channel *channel;
	unsigned long flags;

	spin_lock_irqsave(&glink->idr_lock, flags);
	channel = idr_find(&glink->rcids, cid);
	spin_unlock_irqrestore(&glink->idr_lock, flags);

	if (!channel) {
		pr_err("%s channel not found for cid %d\n", __func__, cid);
		return;
	}

	intent = qcom_glink_alloc_intent(glink, channel, size, false);
	if (intent)
		qcom_glink_advertise_intent(glink, channel, intent);

	qcom_glink_send_intent_req_ack(glink, channel, !!intent);
}

static int qcom_glink_rx_defer(struct qcom_glink *glink, size_t extra)
{
	struct glink_defer_cmd *dcmd;

	extra = ALIGN(extra, 8);

	if (qcom_glink_rx_avail(glink) < sizeof(struct glink_msg) + extra) {
		dev_dbg(glink->dev, "Insufficient data in rx fifo");
		return -ENXIO;
	}

	dcmd = kzalloc(struct_size(dcmd, data, extra), GFP_ATOMIC);
	if (!dcmd)
		return -ENOMEM;

	INIT_LIST_HEAD(&dcmd->node);

	qcom_glink_rx_peek(glink, &dcmd->msg, 0, sizeof(dcmd->msg) + extra);

	spin_lock(&glink->rx_lock);
	list_add_tail(&dcmd->node, &glink->rx_queue);
	spin_unlock(&glink->rx_lock);

	schedule_work(&glink->rx_work);
	qcom_glink_rx_advance(glink, sizeof(dcmd->msg) + extra);

	return 0;
}

static int qcom_glink_rx_data(struct qcom_glink *glink, size_t avail)
{
	struct glink_core_rx_intent *intent;
	struct glink_channel *channel;
	struct {
		struct glink_msg msg;
		__le32 chunk_size;
		__le32 left_size;
	} __packed hdr;
	unsigned int chunk_size;
	unsigned int left_size;
	unsigned int rcid;
	unsigned int liid;
	int ret = 0;
	unsigned long flags;

	if (avail < sizeof(hdr)) {
		dev_dbg(glink->dev, "Not enough data in fifo\n");
		return -EAGAIN;
	}

	qcom_glink_rx_peek(glink, &hdr, 0, sizeof(hdr));
	chunk_size = le32_to_cpu(hdr.chunk_size);
	left_size = le32_to_cpu(hdr.left_size);

	if (avail < sizeof(hdr) + chunk_size) {
		dev_dbg(glink->dev, "Payload not yet in fifo\n");
		return -EAGAIN;
	}

	rcid = le16_to_cpu(hdr.msg.param1);
	spin_lock_irqsave(&glink->idr_lock, flags);
	channel = idr_find(&glink->rcids, rcid);
	spin_unlock_irqrestore(&glink->idr_lock, flags);
	if (!channel) {
		dev_dbg(glink->dev, "Data on non-existing channel\n");

		 
		goto advance_rx;
	}

	if (glink->intentless) {
		 
		if (!channel->buf) {
			intent = kzalloc(sizeof(*intent), GFP_ATOMIC);
			if (!intent)
				return -ENOMEM;

			intent->data = kmalloc(chunk_size + left_size,
					       GFP_ATOMIC);
			if (!intent->data) {
				kfree(intent);
				return -ENOMEM;
			}

			intent->id = 0xbabababa;
			intent->size = chunk_size + left_size;
			intent->offset = 0;

			channel->buf = intent;
		} else {
			intent = channel->buf;
		}
	} else {
		liid = le32_to_cpu(hdr.msg.param2);

		spin_lock_irqsave(&channel->intent_lock, flags);
		intent = idr_find(&channel->liids, liid);
		spin_unlock_irqrestore(&channel->intent_lock, flags);

		if (!intent) {
			dev_err(glink->dev,
				"no intent found for channel %s intent %d",
				channel->name, liid);
			ret = -ENOENT;
			goto advance_rx;
		}
	}

	if (intent->size - intent->offset < chunk_size) {
		dev_err(glink->dev, "Insufficient space in intent\n");

		 
		goto advance_rx;
	}

	qcom_glink_rx_peek(glink, intent->data + intent->offset,
			   sizeof(hdr), chunk_size);
	intent->offset += chunk_size;

	 
	if (!left_size) {
		spin_lock(&channel->recv_lock);
		if (channel->ept.cb) {
			channel->ept.cb(channel->ept.rpdev,
					intent->data,
					intent->offset,
					channel->ept.priv,
					RPMSG_ADDR_ANY);
		}
		spin_unlock(&channel->recv_lock);

		intent->offset = 0;
		channel->buf = NULL;

		qcom_glink_rx_done(glink, channel, intent);
	}

advance_rx:
	qcom_glink_rx_advance(glink, ALIGN(sizeof(hdr) + chunk_size, 8));

	return ret;
}

static void qcom_glink_handle_intent(struct qcom_glink *glink,
				     unsigned int cid,
				     unsigned int count,
				     size_t avail)
{
	struct glink_core_rx_intent *intent;
	struct glink_channel *channel;
	struct intent_pair {
		__le32 size;
		__le32 iid;
	};

	struct {
		struct glink_msg msg;
		struct intent_pair intents[];
	} __packed * msg;

	const size_t msglen = struct_size(msg, intents, count);
	int ret;
	int i;
	unsigned long flags;

	if (avail < msglen) {
		dev_dbg(glink->dev, "Not enough data in fifo\n");
		return;
	}

	spin_lock_irqsave(&glink->idr_lock, flags);
	channel = idr_find(&glink->rcids, cid);
	spin_unlock_irqrestore(&glink->idr_lock, flags);
	if (!channel) {
		dev_err(glink->dev, "intents for non-existing channel\n");
		qcom_glink_rx_advance(glink, ALIGN(msglen, 8));
		return;
	}

	msg = kmalloc(msglen, GFP_ATOMIC);
	if (!msg)
		return;

	qcom_glink_rx_peek(glink, msg, 0, msglen);

	for (i = 0; i < count; ++i) {
		intent = kzalloc(sizeof(*intent), GFP_ATOMIC);
		if (!intent)
			break;

		intent->id = le32_to_cpu(msg->intents[i].iid);
		intent->size = le32_to_cpu(msg->intents[i].size);

		spin_lock_irqsave(&channel->intent_lock, flags);
		ret = idr_alloc(&channel->riids, intent,
				intent->id, intent->id + 1, GFP_ATOMIC);
		spin_unlock_irqrestore(&channel->intent_lock, flags);

		if (ret < 0)
			dev_err(glink->dev, "failed to store remote intent\n");
	}

	WRITE_ONCE(channel->intent_received, true);
	wake_up_all(&channel->intent_req_wq);

	kfree(msg);
	qcom_glink_rx_advance(glink, ALIGN(msglen, 8));
}

static int qcom_glink_rx_open_ack(struct qcom_glink *glink, unsigned int lcid)
{
	struct glink_channel *channel;

	spin_lock(&glink->idr_lock);
	channel = idr_find(&glink->lcids, lcid);
	spin_unlock(&glink->idr_lock);
	if (!channel) {
		dev_err(glink->dev, "Invalid open ack packet\n");
		return -EINVAL;
	}

	complete_all(&channel->open_ack);

	return 0;
}

 
static int qcom_glink_set_flow_control(struct rpmsg_endpoint *ept, bool pause, u32 dst)
{
	struct glink_channel *channel = to_glink_channel(ept);
	struct qcom_glink *glink = channel->glink;
	struct glink_msg msg;
	u32 sigs = 0;

	if (pause)
		sigs |= NATIVE_DTR_SIG | NATIVE_RTS_SIG;

	msg.cmd = cpu_to_le16(GLINK_CMD_SIGNALS);
	msg.param1 = cpu_to_le16(channel->lcid);
	msg.param2 = cpu_to_le32(sigs);

	return qcom_glink_tx(glink, &msg, sizeof(msg), NULL, 0, true);
}

static void qcom_glink_handle_signals(struct qcom_glink *glink,
				      unsigned int rcid, unsigned int sigs)
{
	struct glink_channel *channel;
	unsigned long flags;
	bool enable;

	spin_lock_irqsave(&glink->idr_lock, flags);
	channel = idr_find(&glink->rcids, rcid);
	spin_unlock_irqrestore(&glink->idr_lock, flags);
	if (!channel) {
		dev_err(glink->dev, "signal for non-existing channel\n");
		return;
	}

	enable = sigs & NATIVE_DSR_SIG || sigs & NATIVE_CTS_SIG;

	if (channel->ept.flow_cb)
		channel->ept.flow_cb(channel->ept.rpdev, channel->ept.priv, enable);
}

void qcom_glink_native_rx(struct qcom_glink *glink)
{
	struct glink_msg msg;
	unsigned int param1;
	unsigned int param2;
	unsigned int avail;
	unsigned int cmd;
	int ret = 0;

	 
	wake_up_all(&glink->tx_avail_notify);

	for (;;) {
		avail = qcom_glink_rx_avail(glink);
		if (avail < sizeof(msg))
			break;

		qcom_glink_rx_peek(glink, &msg, 0, sizeof(msg));

		cmd = le16_to_cpu(msg.cmd);
		param1 = le16_to_cpu(msg.param1);
		param2 = le32_to_cpu(msg.param2);

		switch (cmd) {
		case GLINK_CMD_VERSION:
		case GLINK_CMD_VERSION_ACK:
		case GLINK_CMD_CLOSE:
		case GLINK_CMD_CLOSE_ACK:
		case GLINK_CMD_RX_INTENT_REQ:
			ret = qcom_glink_rx_defer(glink, 0);
			break;
		case GLINK_CMD_OPEN_ACK:
			ret = qcom_glink_rx_open_ack(glink, param1);
			qcom_glink_rx_advance(glink, ALIGN(sizeof(msg), 8));
			break;
		case GLINK_CMD_OPEN:
			ret = qcom_glink_rx_defer(glink, param2);
			break;
		case GLINK_CMD_TX_DATA:
		case GLINK_CMD_TX_DATA_CONT:
			ret = qcom_glink_rx_data(glink, avail);
			break;
		case GLINK_CMD_READ_NOTIF:
			qcom_glink_rx_advance(glink, ALIGN(sizeof(msg), 8));
			qcom_glink_tx_kick(glink);
			break;
		case GLINK_CMD_INTENT:
			qcom_glink_handle_intent(glink, param1, param2, avail);
			break;
		case GLINK_CMD_RX_DONE:
			qcom_glink_handle_rx_done(glink, param1, param2, false);
			qcom_glink_rx_advance(glink, ALIGN(sizeof(msg), 8));
			break;
		case GLINK_CMD_RX_DONE_W_REUSE:
			qcom_glink_handle_rx_done(glink, param1, param2, true);
			qcom_glink_rx_advance(glink, ALIGN(sizeof(msg), 8));
			break;
		case GLINK_CMD_RX_INTENT_REQ_ACK:
			qcom_glink_handle_intent_req_ack(glink, param1, param2);
			qcom_glink_rx_advance(glink, ALIGN(sizeof(msg), 8));
			break;
		case GLINK_CMD_SIGNALS:
			qcom_glink_handle_signals(glink, param1, param2);
			qcom_glink_rx_advance(glink, ALIGN(sizeof(msg), 8));
			break;
		default:
			dev_err(glink->dev, "unhandled rx cmd: %d\n", cmd);
			ret = -EINVAL;
			break;
		}

		if (ret)
			break;
	}
}
EXPORT_SYMBOL(qcom_glink_native_rx);

 
static struct glink_channel *qcom_glink_create_local(struct qcom_glink *glink,
						     const char *name)
{
	struct glink_channel *channel;
	int ret;
	unsigned long flags;

	channel = qcom_glink_alloc_channel(glink, name);
	if (IS_ERR(channel))
		return ERR_CAST(channel);

	ret = qcom_glink_send_open_req(glink, channel);
	if (ret)
		goto release_channel;

	ret = wait_for_completion_timeout(&channel->open_ack, 5 * HZ);
	if (!ret)
		goto err_timeout;

	ret = wait_for_completion_timeout(&channel->open_req, 5 * HZ);
	if (!ret)
		goto err_timeout;

	qcom_glink_send_open_ack(glink, channel);

	return channel;

err_timeout:
	 
	spin_lock_irqsave(&glink->idr_lock, flags);
	idr_remove(&glink->lcids, channel->lcid);
	spin_unlock_irqrestore(&glink->idr_lock, flags);

release_channel:
	 
	kref_put(&channel->refcount, qcom_glink_channel_release);
	 
	kref_put(&channel->refcount, qcom_glink_channel_release);

	return ERR_PTR(-ETIMEDOUT);
}

 
static int qcom_glink_create_remote(struct qcom_glink *glink,
				    struct glink_channel *channel)
{
	int ret;

	qcom_glink_send_open_ack(glink, channel);

	ret = qcom_glink_send_open_req(glink, channel);
	if (ret)
		goto close_link;

	ret = wait_for_completion_timeout(&channel->open_ack, 5 * HZ);
	if (!ret) {
		ret = -ETIMEDOUT;
		goto close_link;
	}

	return 0;

close_link:
	 
	qcom_glink_send_close_req(glink, channel);

	return ret;
}

static struct rpmsg_endpoint *qcom_glink_create_ept(struct rpmsg_device *rpdev,
						    rpmsg_rx_cb_t cb,
						    void *priv,
						    struct rpmsg_channel_info
									chinfo)
{
	struct glink_channel *parent = to_glink_channel(rpdev->ept);
	struct glink_channel *channel;
	struct qcom_glink *glink = parent->glink;
	struct rpmsg_endpoint *ept;
	const char *name = chinfo.name;
	int cid;
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&glink->idr_lock, flags);
	idr_for_each_entry(&glink->rcids, channel, cid) {
		if (!strcmp(channel->name, name))
			break;
	}
	spin_unlock_irqrestore(&glink->idr_lock, flags);

	if (!channel) {
		channel = qcom_glink_create_local(glink, name);
		if (IS_ERR(channel))
			return NULL;
	} else {
		ret = qcom_glink_create_remote(glink, channel);
		if (ret)
			return NULL;
	}

	ept = &channel->ept;
	ept->rpdev = rpdev;
	ept->cb = cb;
	ept->priv = priv;
	ept->ops = &glink_endpoint_ops;

	return ept;
}

static int qcom_glink_announce_create(struct rpmsg_device *rpdev)
{
	struct glink_channel *channel = to_glink_channel(rpdev->ept);
	struct device_node *np = rpdev->dev.of_node;
	struct qcom_glink *glink = channel->glink;
	struct glink_core_rx_intent *intent;
	const struct property *prop = NULL;
	__be32 defaults[] = { cpu_to_be32(SZ_1K), cpu_to_be32(5) };
	int num_intents;
	int num_groups = 1;
	__be32 *val = defaults;
	int size;

	if (glink->intentless || !completion_done(&channel->open_ack))
		return 0;

	prop = of_find_property(np, "qcom,intents", NULL);
	if (prop) {
		val = prop->value;
		num_groups = prop->length / sizeof(u32) / 2;
	}

	 
	while (num_groups--) {
		size = be32_to_cpup(val++);
		num_intents = be32_to_cpup(val++);
		while (num_intents--) {
			intent = qcom_glink_alloc_intent(glink, channel, size,
							 true);
			if (!intent)
				break;

			qcom_glink_advertise_intent(glink, channel, intent);
		}
	}
	return 0;
}

static void qcom_glink_destroy_ept(struct rpmsg_endpoint *ept)
{
	struct glink_channel *channel = to_glink_channel(ept);
	struct qcom_glink *glink = channel->glink;
	unsigned long flags;

	spin_lock_irqsave(&channel->recv_lock, flags);
	channel->ept.cb = NULL;
	spin_unlock_irqrestore(&channel->recv_lock, flags);

	 
	channel->rpdev = NULL;

	qcom_glink_send_close_req(glink, channel);
}

static int qcom_glink_request_intent(struct qcom_glink *glink,
				     struct glink_channel *channel,
				     size_t size)
{
	struct {
		u16 id;
		u16 cid;
		u32 size;
	} __packed cmd;

	int ret;

	mutex_lock(&channel->intent_req_lock);

	WRITE_ONCE(channel->intent_req_result, -1);
	WRITE_ONCE(channel->intent_received, false);

	cmd.id = GLINK_CMD_RX_INTENT_REQ;
	cmd.cid = channel->lcid;
	cmd.size = size;

	ret = qcom_glink_tx(glink, &cmd, sizeof(cmd), NULL, 0, true);
	if (ret)
		goto unlock;

	ret = wait_event_timeout(channel->intent_req_wq,
				 READ_ONCE(channel->intent_req_result) >= 0 &&
				 READ_ONCE(channel->intent_received),
				 10 * HZ);
	if (!ret) {
		dev_err(glink->dev, "intent request timed out\n");
		ret = -ETIMEDOUT;
	} else {
		ret = READ_ONCE(channel->intent_req_result) ? 0 : -ECANCELED;
	}

unlock:
	mutex_unlock(&channel->intent_req_lock);
	return ret;
}

static int __qcom_glink_send(struct glink_channel *channel,
			     void *data, int len, bool wait)
{
	struct qcom_glink *glink = channel->glink;
	struct glink_core_rx_intent *intent = NULL;
	struct glink_core_rx_intent *tmp;
	int iid = 0;
	struct {
		struct glink_msg msg;
		__le32 chunk_size;
		__le32 left_size;
	} __packed req;
	int ret;
	unsigned long flags;
	int chunk_size = len;
	size_t offset = 0;

	if (!glink->intentless) {
		while (!intent) {
			spin_lock_irqsave(&channel->intent_lock, flags);
			idr_for_each_entry(&channel->riids, tmp, iid) {
				if (tmp->size >= len && !tmp->in_use) {
					if (!intent)
						intent = tmp;
					else if (intent->size > tmp->size)
						intent = tmp;
					if (intent->size == len)
						break;
				}
			}
			if (intent)
				intent->in_use = true;
			spin_unlock_irqrestore(&channel->intent_lock, flags);

			 
			if (intent)
				break;

			if (!wait)
				return -EBUSY;

			ret = qcom_glink_request_intent(glink, channel, len);
			if (ret < 0)
				return ret;
		}

		iid = intent->id;
	}

	while (offset < len) {
		chunk_size = len - offset;
		if (chunk_size > SZ_8K && wait)
			chunk_size = SZ_8K;

		req.msg.cmd = cpu_to_le16(offset == 0 ? GLINK_CMD_TX_DATA : GLINK_CMD_TX_DATA_CONT);
		req.msg.param1 = cpu_to_le16(channel->lcid);
		req.msg.param2 = cpu_to_le32(iid);
		req.chunk_size = cpu_to_le32(chunk_size);
		req.left_size = cpu_to_le32(len - offset - chunk_size);

		ret = qcom_glink_tx(glink, &req, sizeof(req), data + offset, chunk_size, wait);
		if (ret) {
			 
			if (intent)
				intent->in_use = false;
			return ret;
		}

		offset += chunk_size;
	}

	return 0;
}

static int qcom_glink_send(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct glink_channel *channel = to_glink_channel(ept);

	return __qcom_glink_send(channel, data, len, true);
}

static int qcom_glink_trysend(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct glink_channel *channel = to_glink_channel(ept);

	return __qcom_glink_send(channel, data, len, false);
}

static int qcom_glink_sendto(struct rpmsg_endpoint *ept, void *data, int len, u32 dst)
{
	struct glink_channel *channel = to_glink_channel(ept);

	return __qcom_glink_send(channel, data, len, true);
}

static int qcom_glink_trysendto(struct rpmsg_endpoint *ept, void *data, int len, u32 dst)
{
	struct glink_channel *channel = to_glink_channel(ept);

	return __qcom_glink_send(channel, data, len, false);
}

 
static struct device_node *qcom_glink_match_channel(struct device_node *node,
						    const char *channel)
{
	struct device_node *child;
	const char *name;
	const char *key;
	int ret;

	for_each_available_child_of_node(node, child) {
		key = "qcom,glink-channels";
		ret = of_property_read_string(child, key, &name);
		if (ret)
			continue;

		if (strcmp(name, channel) == 0)
			return child;
	}

	return NULL;
}

static const struct rpmsg_device_ops glink_device_ops = {
	.create_ept = qcom_glink_create_ept,
	.announce_create = qcom_glink_announce_create,
};

static const struct rpmsg_endpoint_ops glink_endpoint_ops = {
	.destroy_ept = qcom_glink_destroy_ept,
	.send = qcom_glink_send,
	.sendto = qcom_glink_sendto,
	.trysend = qcom_glink_trysend,
	.trysendto = qcom_glink_trysendto,
	.set_flow_control = qcom_glink_set_flow_control,
};

static void qcom_glink_rpdev_release(struct device *dev)
{
	struct rpmsg_device *rpdev = to_rpmsg_device(dev);

	kfree(rpdev->driver_override);
	kfree(rpdev);
}

static int qcom_glink_rx_open(struct qcom_glink *glink, unsigned int rcid,
			      char *name)
{
	struct glink_channel *channel;
	struct rpmsg_device *rpdev;
	bool create_device = false;
	struct device_node *node;
	int lcid;
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&glink->idr_lock, flags);
	idr_for_each_entry(&glink->lcids, channel, lcid) {
		if (!strcmp(channel->name, name))
			break;
	}
	spin_unlock_irqrestore(&glink->idr_lock, flags);

	if (!channel) {
		channel = qcom_glink_alloc_channel(glink, name);
		if (IS_ERR(channel))
			return PTR_ERR(channel);

		 
		create_device = true;
	}

	spin_lock_irqsave(&glink->idr_lock, flags);
	ret = idr_alloc(&glink->rcids, channel, rcid, rcid + 1, GFP_ATOMIC);
	if (ret < 0) {
		dev_err(glink->dev, "Unable to insert channel into rcid list\n");
		spin_unlock_irqrestore(&glink->idr_lock, flags);
		goto free_channel;
	}
	channel->rcid = ret;
	spin_unlock_irqrestore(&glink->idr_lock, flags);

	complete_all(&channel->open_req);

	if (create_device) {
		rpdev = kzalloc(sizeof(*rpdev), GFP_KERNEL);
		if (!rpdev) {
			ret = -ENOMEM;
			goto rcid_remove;
		}

		rpdev->ept = &channel->ept;
		strscpy_pad(rpdev->id.name, name, RPMSG_NAME_SIZE);
		rpdev->src = RPMSG_ADDR_ANY;
		rpdev->dst = RPMSG_ADDR_ANY;
		rpdev->ops = &glink_device_ops;

		node = qcom_glink_match_channel(glink->dev->of_node, name);
		rpdev->dev.of_node = node;
		rpdev->dev.parent = glink->dev;
		rpdev->dev.release = qcom_glink_rpdev_release;

		ret = rpmsg_register_device(rpdev);
		if (ret)
			goto rcid_remove;

		channel->rpdev = rpdev;
	}

	return 0;

rcid_remove:
	spin_lock_irqsave(&glink->idr_lock, flags);
	idr_remove(&glink->rcids, channel->rcid);
	channel->rcid = 0;
	spin_unlock_irqrestore(&glink->idr_lock, flags);
free_channel:
	 
	if (create_device)
		kref_put(&channel->refcount, qcom_glink_channel_release);

	return ret;
}

static void qcom_glink_rx_close(struct qcom_glink *glink, unsigned int rcid)
{
	struct rpmsg_channel_info chinfo;
	struct glink_channel *channel;
	unsigned long flags;

	spin_lock_irqsave(&glink->idr_lock, flags);
	channel = idr_find(&glink->rcids, rcid);
	spin_unlock_irqrestore(&glink->idr_lock, flags);
	if (WARN(!channel, "close request on unknown channel\n"))
		return;

	 
	cancel_work_sync(&channel->intent_work);

	if (channel->rpdev) {
		strscpy_pad(chinfo.name, channel->name, sizeof(chinfo.name));
		chinfo.src = RPMSG_ADDR_ANY;
		chinfo.dst = RPMSG_ADDR_ANY;

		rpmsg_unregister_device(glink->dev, &chinfo);
	}
	channel->rpdev = NULL;

	qcom_glink_send_close_ack(glink, channel->rcid);

	spin_lock_irqsave(&glink->idr_lock, flags);
	idr_remove(&glink->rcids, channel->rcid);
	channel->rcid = 0;
	spin_unlock_irqrestore(&glink->idr_lock, flags);

	kref_put(&channel->refcount, qcom_glink_channel_release);
}

static void qcom_glink_rx_close_ack(struct qcom_glink *glink, unsigned int lcid)
{
	struct rpmsg_channel_info chinfo;
	struct glink_channel *channel;
	unsigned long flags;

	 
	wake_up_all(&glink->tx_avail_notify);

	spin_lock_irqsave(&glink->idr_lock, flags);
	channel = idr_find(&glink->lcids, lcid);
	if (WARN(!channel, "close ack on unknown channel\n")) {
		spin_unlock_irqrestore(&glink->idr_lock, flags);
		return;
	}

	idr_remove(&glink->lcids, channel->lcid);
	channel->lcid = 0;
	spin_unlock_irqrestore(&glink->idr_lock, flags);

	 
	if (channel->rpdev) {
		strscpy(chinfo.name, channel->name, sizeof(chinfo.name));
		chinfo.src = RPMSG_ADDR_ANY;
		chinfo.dst = RPMSG_ADDR_ANY;

		rpmsg_unregister_device(glink->dev, &chinfo);
	}
	channel->rpdev = NULL;

	kref_put(&channel->refcount, qcom_glink_channel_release);
}

static void qcom_glink_work(struct work_struct *work)
{
	struct qcom_glink *glink = container_of(work, struct qcom_glink,
						rx_work);
	struct glink_defer_cmd *dcmd;
	struct glink_msg *msg;
	unsigned long flags;
	unsigned int param1;
	unsigned int param2;
	unsigned int cmd;

	for (;;) {
		spin_lock_irqsave(&glink->rx_lock, flags);
		if (list_empty(&glink->rx_queue)) {
			spin_unlock_irqrestore(&glink->rx_lock, flags);
			break;
		}
		dcmd = list_first_entry(&glink->rx_queue,
					struct glink_defer_cmd, node);
		list_del(&dcmd->node);
		spin_unlock_irqrestore(&glink->rx_lock, flags);

		msg = &dcmd->msg;
		cmd = le16_to_cpu(msg->cmd);
		param1 = le16_to_cpu(msg->param1);
		param2 = le32_to_cpu(msg->param2);

		switch (cmd) {
		case GLINK_CMD_VERSION:
			qcom_glink_receive_version(glink, param1, param2);
			break;
		case GLINK_CMD_VERSION_ACK:
			qcom_glink_receive_version_ack(glink, param1, param2);
			break;
		case GLINK_CMD_OPEN:
			qcom_glink_rx_open(glink, param1, msg->data);
			break;
		case GLINK_CMD_CLOSE:
			qcom_glink_rx_close(glink, param1);
			break;
		case GLINK_CMD_CLOSE_ACK:
			qcom_glink_rx_close_ack(glink, param1);
			break;
		case GLINK_CMD_RX_INTENT_REQ:
			qcom_glink_handle_intent_req(glink, param1, param2);
			break;
		default:
			WARN(1, "Unknown defer object %d\n", cmd);
			break;
		}

		kfree(dcmd);
	}
}

static void qcom_glink_cancel_rx_work(struct qcom_glink *glink)
{
	struct glink_defer_cmd *dcmd;
	struct glink_defer_cmd *tmp;

	 
	cancel_work_sync(&glink->rx_work);

	list_for_each_entry_safe(dcmd, tmp, &glink->rx_queue, node)
		kfree(dcmd);
}

static ssize_t rpmsg_name_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int ret = 0;
	const char *name;

	ret = of_property_read_string(dev->of_node, "label", &name);
	if (ret < 0)
		name = dev->of_node->name;

	return sysfs_emit(buf, "%s\n", name);
}
static DEVICE_ATTR_RO(rpmsg_name);

static struct attribute *qcom_glink_attrs[] = {
	&dev_attr_rpmsg_name.attr,
	NULL
};
ATTRIBUTE_GROUPS(qcom_glink);

static void qcom_glink_device_release(struct device *dev)
{
	struct rpmsg_device *rpdev = to_rpmsg_device(dev);
	struct glink_channel *channel = to_glink_channel(rpdev->ept);

	 
	kref_put(&channel->refcount, qcom_glink_channel_release);
	kfree(rpdev->driver_override);
	kfree(rpdev);
}

static int qcom_glink_create_chrdev(struct qcom_glink *glink)
{
	struct rpmsg_device *rpdev;
	struct glink_channel *channel;

	rpdev = kzalloc(sizeof(*rpdev), GFP_KERNEL);
	if (!rpdev)
		return -ENOMEM;

	channel = qcom_glink_alloc_channel(glink, "rpmsg_chrdev");
	if (IS_ERR(channel)) {
		kfree(rpdev);
		return PTR_ERR(channel);
	}
	channel->rpdev = rpdev;

	rpdev->ept = &channel->ept;
	rpdev->ops = &glink_device_ops;
	rpdev->dev.parent = glink->dev;
	rpdev->dev.release = qcom_glink_device_release;

	return rpmsg_ctrldev_register_device(rpdev);
}

struct qcom_glink *qcom_glink_native_probe(struct device *dev,
					   unsigned long features,
					   struct qcom_glink_pipe *rx,
					   struct qcom_glink_pipe *tx,
					   bool intentless)
{
	int ret;
	struct qcom_glink *glink;

	glink = devm_kzalloc(dev, sizeof(*glink), GFP_KERNEL);
	if (!glink)
		return ERR_PTR(-ENOMEM);

	glink->dev = dev;
	glink->tx_pipe = tx;
	glink->rx_pipe = rx;

	glink->features = features;
	glink->intentless = intentless;

	spin_lock_init(&glink->tx_lock);
	spin_lock_init(&glink->rx_lock);
	INIT_LIST_HEAD(&glink->rx_queue);
	INIT_WORK(&glink->rx_work, qcom_glink_work);
	init_waitqueue_head(&glink->tx_avail_notify);

	spin_lock_init(&glink->idr_lock);
	idr_init(&glink->lcids);
	idr_init(&glink->rcids);

	glink->dev->groups = qcom_glink_groups;

	ret = device_add_groups(dev, qcom_glink_groups);
	if (ret)
		dev_err(dev, "failed to add groups\n");

	ret = qcom_glink_send_version(glink);
	if (ret)
		return ERR_PTR(ret);

	ret = qcom_glink_create_chrdev(glink);
	if (ret)
		dev_err(glink->dev, "failed to register chrdev\n");

	return glink;
}
EXPORT_SYMBOL_GPL(qcom_glink_native_probe);

static int qcom_glink_remove_device(struct device *dev, void *data)
{
	device_unregister(dev);

	return 0;
}

void qcom_glink_native_remove(struct qcom_glink *glink)
{
	struct glink_channel *channel;
	unsigned long flags;
	int cid;
	int ret;

	qcom_glink_cancel_rx_work(glink);

	 
	spin_lock_irqsave(&glink->tx_lock, flags);
	glink->abort_tx = true;
	wake_up_all(&glink->tx_avail_notify);
	spin_unlock_irqrestore(&glink->tx_lock, flags);

	 
	spin_lock_irqsave(&glink->idr_lock, flags);
	idr_for_each_entry(&glink->lcids, channel, cid)
		qcom_glink_intent_req_abort(channel);
	spin_unlock_irqrestore(&glink->idr_lock, flags);

	ret = device_for_each_child(glink->dev, NULL, qcom_glink_remove_device);
	if (ret)
		dev_warn(glink->dev, "Can't remove GLINK devices: %d\n", ret);

	 
	idr_for_each_entry(&glink->lcids, channel, cid)
		kref_put(&channel->refcount, qcom_glink_channel_release);

	 
	idr_for_each_entry(&glink->rcids, channel, cid)
		kref_put(&channel->refcount, qcom_glink_channel_release);

	idr_destroy(&glink->lcids);
	idr_destroy(&glink->rcids);
}
EXPORT_SYMBOL_GPL(qcom_glink_native_remove);

MODULE_DESCRIPTION("Qualcomm GLINK driver");
MODULE_LICENSE("GPL v2");
