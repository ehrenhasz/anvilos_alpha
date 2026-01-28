#ifndef VIRTIO_SND_MSG_H
#define VIRTIO_SND_MSG_H
#include <linux/atomic.h>
#include <linux/virtio.h>
struct virtio_snd;
struct virtio_snd_msg;
void virtsnd_ctl_msg_ref(struct virtio_snd_msg *msg);
void virtsnd_ctl_msg_unref(struct virtio_snd_msg *msg);
void *virtsnd_ctl_msg_request(struct virtio_snd_msg *msg);
void *virtsnd_ctl_msg_response(struct virtio_snd_msg *msg);
struct virtio_snd_msg *virtsnd_ctl_msg_alloc(size_t request_size,
					     size_t response_size, gfp_t gfp);
int virtsnd_ctl_msg_send(struct virtio_snd *snd, struct virtio_snd_msg *msg,
			 struct scatterlist *out_sgs,
			 struct scatterlist *in_sgs, bool nowait);
static inline int virtsnd_ctl_msg_send_sync(struct virtio_snd *snd,
					    struct virtio_snd_msg *msg)
{
	return virtsnd_ctl_msg_send(snd, msg, NULL, NULL, false);
}
static inline int virtsnd_ctl_msg_send_async(struct virtio_snd *snd,
					     struct virtio_snd_msg *msg)
{
	return virtsnd_ctl_msg_send(snd, msg, NULL, NULL, true);
}
void virtsnd_ctl_msg_cancel_all(struct virtio_snd *snd);
void virtsnd_ctl_msg_complete(struct virtio_snd_msg *msg);
int virtsnd_ctl_query_info(struct virtio_snd *snd, int command, int start_id,
			   int count, size_t size, void *info);
void virtsnd_ctl_notify_cb(struct virtqueue *vqueue);
#endif  
