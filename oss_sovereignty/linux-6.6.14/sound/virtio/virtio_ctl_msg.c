
 
#include <linux/moduleparam.h>
#include <linux/virtio_config.h>

#include "virtio_card.h"

 
struct virtio_snd_msg {
	struct scatterlist sg_request;
	struct scatterlist sg_response;
	struct list_head list;
	struct completion notify;
	refcount_t ref_count;
};

 
void virtsnd_ctl_msg_ref(struct virtio_snd_msg *msg)
{
	refcount_inc(&msg->ref_count);
}

 
void virtsnd_ctl_msg_unref(struct virtio_snd_msg *msg)
{
	if (refcount_dec_and_test(&msg->ref_count))
		kfree(msg);
}

 
void *virtsnd_ctl_msg_request(struct virtio_snd_msg *msg)
{
	return sg_virt(&msg->sg_request);
}

 
void *virtsnd_ctl_msg_response(struct virtio_snd_msg *msg)
{
	return sg_virt(&msg->sg_response);
}

 
struct virtio_snd_msg *virtsnd_ctl_msg_alloc(size_t request_size,
					     size_t response_size, gfp_t gfp)
{
	struct virtio_snd_msg *msg;

	if (!request_size || !response_size)
		return NULL;

	msg = kzalloc(sizeof(*msg) + request_size + response_size, gfp);
	if (!msg)
		return NULL;

	sg_init_one(&msg->sg_request, (u8 *)msg + sizeof(*msg), request_size);
	sg_init_one(&msg->sg_response, (u8 *)msg + sizeof(*msg) + request_size,
		    response_size);

	INIT_LIST_HEAD(&msg->list);
	init_completion(&msg->notify);
	 
	refcount_set(&msg->ref_count, 1);

	return msg;
}

 
int virtsnd_ctl_msg_send(struct virtio_snd *snd, struct virtio_snd_msg *msg,
			 struct scatterlist *out_sgs,
			 struct scatterlist *in_sgs, bool nowait)
{
	struct virtio_device *vdev = snd->vdev;
	struct virtio_snd_queue *queue = virtsnd_control_queue(snd);
	unsigned int js = msecs_to_jiffies(virtsnd_msg_timeout_ms);
	struct virtio_snd_hdr *request = virtsnd_ctl_msg_request(msg);
	struct virtio_snd_hdr *response = virtsnd_ctl_msg_response(msg);
	unsigned int nouts = 0;
	unsigned int nins = 0;
	struct scatterlist *psgs[4];
	bool notify = false;
	unsigned long flags;
	int rc;

	virtsnd_ctl_msg_ref(msg);

	 
	response->code = cpu_to_le32(VIRTIO_SND_S_IO_ERR);

	psgs[nouts++] = &msg->sg_request;
	if (out_sgs)
		psgs[nouts++] = out_sgs;

	psgs[nouts + nins++] = &msg->sg_response;
	if (in_sgs)
		psgs[nouts + nins++] = in_sgs;

	spin_lock_irqsave(&queue->lock, flags);
	rc = virtqueue_add_sgs(queue->vqueue, psgs, nouts, nins, msg,
			       GFP_ATOMIC);
	if (!rc) {
		notify = virtqueue_kick_prepare(queue->vqueue);

		list_add_tail(&msg->list, &snd->ctl_msgs);
	}
	spin_unlock_irqrestore(&queue->lock, flags);

	if (rc) {
		dev_err(&vdev->dev, "failed to send control message (0x%08x)\n",
			le32_to_cpu(request->code));

		 
		virtsnd_ctl_msg_unref(msg);

		goto on_exit;
	}

	if (notify)
		virtqueue_notify(queue->vqueue);

	if (nowait)
		goto on_exit;

	rc = wait_for_completion_interruptible_timeout(&msg->notify, js);
	if (rc <= 0) {
		if (!rc) {
			dev_err(&vdev->dev,
				"control message (0x%08x) timeout\n",
				le32_to_cpu(request->code));
			rc = -ETIMEDOUT;
		}

		goto on_exit;
	}

	switch (le32_to_cpu(response->code)) {
	case VIRTIO_SND_S_OK:
		rc = 0;
		break;
	case VIRTIO_SND_S_NOT_SUPP:
		rc = -EOPNOTSUPP;
		break;
	case VIRTIO_SND_S_IO_ERR:
		rc = -EIO;
		break;
	default:
		rc = -EINVAL;
		break;
	}

on_exit:
	virtsnd_ctl_msg_unref(msg);

	return rc;
}

 
void virtsnd_ctl_msg_complete(struct virtio_snd_msg *msg)
{
	list_del(&msg->list);
	complete(&msg->notify);

	virtsnd_ctl_msg_unref(msg);
}

 
void virtsnd_ctl_msg_cancel_all(struct virtio_snd *snd)
{
	struct virtio_snd_queue *queue = virtsnd_control_queue(snd);
	unsigned long flags;

	spin_lock_irqsave(&queue->lock, flags);
	while (!list_empty(&snd->ctl_msgs)) {
		struct virtio_snd_msg *msg =
			list_first_entry(&snd->ctl_msgs, struct virtio_snd_msg,
					 list);

		virtsnd_ctl_msg_complete(msg);
	}
	spin_unlock_irqrestore(&queue->lock, flags);
}

 
int virtsnd_ctl_query_info(struct virtio_snd *snd, int command, int start_id,
			   int count, size_t size, void *info)
{
	struct virtio_snd_msg *msg;
	struct virtio_snd_query_info *query;
	struct scatterlist sg;

	msg = virtsnd_ctl_msg_alloc(sizeof(*query),
				    sizeof(struct virtio_snd_hdr), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	query = virtsnd_ctl_msg_request(msg);
	query->hdr.code = cpu_to_le32(command);
	query->start_id = cpu_to_le32(start_id);
	query->count = cpu_to_le32(count);
	query->size = cpu_to_le32(size);

	sg_init_one(&sg, info, count * size);

	return virtsnd_ctl_msg_send(snd, msg, NULL, &sg, false);
}

 
void virtsnd_ctl_notify_cb(struct virtqueue *vqueue)
{
	struct virtio_snd *snd = vqueue->vdev->priv;
	struct virtio_snd_queue *queue = virtsnd_control_queue(snd);
	struct virtio_snd_msg *msg;
	u32 length;
	unsigned long flags;

	spin_lock_irqsave(&queue->lock, flags);
	do {
		virtqueue_disable_cb(vqueue);
		while ((msg = virtqueue_get_buf(vqueue, &length)))
			virtsnd_ctl_msg_complete(msg);
		if (unlikely(virtqueue_is_broken(vqueue)))
			break;
	} while (!virtqueue_enable_cb(vqueue));
	spin_unlock_irqrestore(&queue->lock, flags);
}
