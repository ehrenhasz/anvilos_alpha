 
 
#ifndef VIRTIO_SND_PCM_H
#define VIRTIO_SND_PCM_H

#include <linux/atomic.h>
#include <linux/virtio_config.h>
#include <sound/pcm.h>

struct virtio_pcm;
struct virtio_pcm_msg;

 
struct virtio_pcm_substream {
	struct virtio_snd *snd;
	u32 nid;
	u32 sid;
	u32 direction;
	u32 features;
	struct snd_pcm_substream *substream;
	struct snd_pcm_hardware hw;
	struct work_struct elapsed_period;
	spinlock_t lock;
	size_t buffer_bytes;
	size_t hw_ptr;
	bool xfer_enabled;
	bool xfer_xrun;
	bool stopped;
	bool suspended;
	struct virtio_pcm_msg **msgs;
	unsigned int nmsgs;
	int msg_last_enqueued;
	unsigned int msg_count;
	wait_queue_head_t msg_empty;
};

 
struct virtio_pcm_stream {
	struct virtio_pcm_substream **substreams;
	u32 nsubstreams;
	struct snd_pcm_chmap_elem *chmaps;
	u32 nchmaps;
};

 
struct virtio_pcm {
	struct list_head list;
	u32 nid;
	struct snd_pcm *pcm;
	struct virtio_pcm_stream streams[SNDRV_PCM_STREAM_LAST + 1];
};

extern const struct snd_pcm_ops virtsnd_pcm_ops;

int virtsnd_pcm_validate(struct virtio_device *vdev);

int virtsnd_pcm_parse_cfg(struct virtio_snd *snd);

int virtsnd_pcm_build_devs(struct virtio_snd *snd);

void virtsnd_pcm_event(struct virtio_snd *snd, struct virtio_snd_event *event);

void virtsnd_pcm_tx_notify_cb(struct virtqueue *vqueue);

void virtsnd_pcm_rx_notify_cb(struct virtqueue *vqueue);

struct virtio_pcm *virtsnd_pcm_find(struct virtio_snd *snd, u32 nid);

struct virtio_pcm *virtsnd_pcm_find_or_create(struct virtio_snd *snd, u32 nid);

struct virtio_snd_msg *
virtsnd_pcm_ctl_msg_alloc(struct virtio_pcm_substream *vss,
			  unsigned int command, gfp_t gfp);

int virtsnd_pcm_msg_alloc(struct virtio_pcm_substream *vss,
			  unsigned int periods, unsigned int period_bytes);

void virtsnd_pcm_msg_free(struct virtio_pcm_substream *vss);

int virtsnd_pcm_msg_send(struct virtio_pcm_substream *vss);

unsigned int virtsnd_pcm_msg_pending_num(struct virtio_pcm_substream *vss);

#endif  
