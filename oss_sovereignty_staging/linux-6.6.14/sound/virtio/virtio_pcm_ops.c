
 
#include <sound/pcm_params.h>

#include "virtio_card.h"

 

 
struct virtsnd_a2v_format {
	snd_pcm_format_t alsa_bit;
	unsigned int vio_bit;
};

static const struct virtsnd_a2v_format g_a2v_format_map[] = {
	{ SNDRV_PCM_FORMAT_IMA_ADPCM, VIRTIO_SND_PCM_FMT_IMA_ADPCM },
	{ SNDRV_PCM_FORMAT_MU_LAW, VIRTIO_SND_PCM_FMT_MU_LAW },
	{ SNDRV_PCM_FORMAT_A_LAW, VIRTIO_SND_PCM_FMT_A_LAW },
	{ SNDRV_PCM_FORMAT_S8, VIRTIO_SND_PCM_FMT_S8 },
	{ SNDRV_PCM_FORMAT_U8, VIRTIO_SND_PCM_FMT_U8 },
	{ SNDRV_PCM_FORMAT_S16_LE, VIRTIO_SND_PCM_FMT_S16 },
	{ SNDRV_PCM_FORMAT_U16_LE, VIRTIO_SND_PCM_FMT_U16 },
	{ SNDRV_PCM_FORMAT_S18_3LE, VIRTIO_SND_PCM_FMT_S18_3 },
	{ SNDRV_PCM_FORMAT_U18_3LE, VIRTIO_SND_PCM_FMT_U18_3 },
	{ SNDRV_PCM_FORMAT_S20_3LE, VIRTIO_SND_PCM_FMT_S20_3 },
	{ SNDRV_PCM_FORMAT_U20_3LE, VIRTIO_SND_PCM_FMT_U20_3 },
	{ SNDRV_PCM_FORMAT_S24_3LE, VIRTIO_SND_PCM_FMT_S24_3 },
	{ SNDRV_PCM_FORMAT_U24_3LE, VIRTIO_SND_PCM_FMT_U24_3 },
	{ SNDRV_PCM_FORMAT_S20_LE, VIRTIO_SND_PCM_FMT_S20 },
	{ SNDRV_PCM_FORMAT_U20_LE, VIRTIO_SND_PCM_FMT_U20 },
	{ SNDRV_PCM_FORMAT_S24_LE, VIRTIO_SND_PCM_FMT_S24 },
	{ SNDRV_PCM_FORMAT_U24_LE, VIRTIO_SND_PCM_FMT_U24 },
	{ SNDRV_PCM_FORMAT_S32_LE, VIRTIO_SND_PCM_FMT_S32 },
	{ SNDRV_PCM_FORMAT_U32_LE, VIRTIO_SND_PCM_FMT_U32 },
	{ SNDRV_PCM_FORMAT_FLOAT_LE, VIRTIO_SND_PCM_FMT_FLOAT },
	{ SNDRV_PCM_FORMAT_FLOAT64_LE, VIRTIO_SND_PCM_FMT_FLOAT64 },
	{ SNDRV_PCM_FORMAT_DSD_U8, VIRTIO_SND_PCM_FMT_DSD_U8 },
	{ SNDRV_PCM_FORMAT_DSD_U16_LE, VIRTIO_SND_PCM_FMT_DSD_U16 },
	{ SNDRV_PCM_FORMAT_DSD_U32_LE, VIRTIO_SND_PCM_FMT_DSD_U32 },
	{ SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE,
	  VIRTIO_SND_PCM_FMT_IEC958_SUBFRAME }
};

 
struct virtsnd_a2v_rate {
	unsigned int rate;
	unsigned int vio_bit;
};

static const struct virtsnd_a2v_rate g_a2v_rate_map[] = {
	{ 5512, VIRTIO_SND_PCM_RATE_5512 },
	{ 8000, VIRTIO_SND_PCM_RATE_8000 },
	{ 11025, VIRTIO_SND_PCM_RATE_11025 },
	{ 16000, VIRTIO_SND_PCM_RATE_16000 },
	{ 22050, VIRTIO_SND_PCM_RATE_22050 },
	{ 32000, VIRTIO_SND_PCM_RATE_32000 },
	{ 44100, VIRTIO_SND_PCM_RATE_44100 },
	{ 48000, VIRTIO_SND_PCM_RATE_48000 },
	{ 64000, VIRTIO_SND_PCM_RATE_64000 },
	{ 88200, VIRTIO_SND_PCM_RATE_88200 },
	{ 96000, VIRTIO_SND_PCM_RATE_96000 },
	{ 176400, VIRTIO_SND_PCM_RATE_176400 },
	{ 192000, VIRTIO_SND_PCM_RATE_192000 }
};

static int virtsnd_pcm_sync_stop(struct snd_pcm_substream *substream);

 
static int virtsnd_pcm_open(struct snd_pcm_substream *substream)
{
	struct virtio_pcm *vpcm = snd_pcm_substream_chip(substream);
	struct virtio_pcm_stream *vs = &vpcm->streams[substream->stream];
	struct virtio_pcm_substream *vss = vs->substreams[substream->number];

	substream->runtime->hw = vss->hw;
	substream->private_data = vss;

	snd_pcm_hw_constraint_integer(substream->runtime,
				      SNDRV_PCM_HW_PARAM_PERIODS);

	vss->stopped = !!virtsnd_pcm_msg_pending_num(vss);
	vss->suspended = false;

	 
	return virtsnd_pcm_sync_stop(substream);
}

 
static int virtsnd_pcm_close(struct snd_pcm_substream *substream)
{
	return 0;
}

 
static int virtsnd_pcm_dev_set_params(struct virtio_pcm_substream *vss,
				      unsigned int buffer_bytes,
				      unsigned int period_bytes,
				      unsigned int channels,
				      snd_pcm_format_t format,
				      unsigned int rate)
{
	struct virtio_snd_msg *msg;
	struct virtio_snd_pcm_set_params *request;
	unsigned int i;
	int vformat = -1;
	int vrate = -1;

	for (i = 0; i < ARRAY_SIZE(g_a2v_format_map); ++i)
		if (g_a2v_format_map[i].alsa_bit == format) {
			vformat = g_a2v_format_map[i].vio_bit;

			break;
		}

	for (i = 0; i < ARRAY_SIZE(g_a2v_rate_map); ++i)
		if (g_a2v_rate_map[i].rate == rate) {
			vrate = g_a2v_rate_map[i].vio_bit;

			break;
		}

	if (vformat == -1 || vrate == -1)
		return -EINVAL;

	msg = virtsnd_pcm_ctl_msg_alloc(vss, VIRTIO_SND_R_PCM_SET_PARAMS,
					GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	request = virtsnd_ctl_msg_request(msg);
	request->buffer_bytes = cpu_to_le32(buffer_bytes);
	request->period_bytes = cpu_to_le32(period_bytes);
	request->channels = channels;
	request->format = vformat;
	request->rate = vrate;

	if (vss->features & (1U << VIRTIO_SND_PCM_F_MSG_POLLING))
		request->features |=
			cpu_to_le32(1U << VIRTIO_SND_PCM_F_MSG_POLLING);

	if (vss->features & (1U << VIRTIO_SND_PCM_F_EVT_XRUNS))
		request->features |=
			cpu_to_le32(1U << VIRTIO_SND_PCM_F_EVT_XRUNS);

	return virtsnd_ctl_msg_send_sync(vss->snd, msg);
}

 
static int virtsnd_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *hw_params)
{
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	struct virtio_device *vdev = vss->snd->vdev;
	int rc;

	if (virtsnd_pcm_msg_pending_num(vss)) {
		dev_err(&vdev->dev, "SID %u: invalid I/O queue state\n",
			vss->sid);
		return -EBADFD;
	}

	rc = virtsnd_pcm_dev_set_params(vss, params_buffer_bytes(hw_params),
					params_period_bytes(hw_params),
					params_channels(hw_params),
					params_format(hw_params),
					params_rate(hw_params));
	if (rc)
		return rc;

	 
	virtsnd_pcm_msg_free(vss);

	return virtsnd_pcm_msg_alloc(vss, params_periods(hw_params),
				     params_period_bytes(hw_params));
}

 
static int virtsnd_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);

	 
	if (!virtsnd_pcm_msg_pending_num(vss))
		virtsnd_pcm_msg_free(vss);

	return 0;
}

 
static int virtsnd_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	struct virtio_device *vdev = vss->snd->vdev;
	struct virtio_snd_msg *msg;

	if (!vss->suspended) {
		if (virtsnd_pcm_msg_pending_num(vss)) {
			dev_err(&vdev->dev, "SID %u: invalid I/O queue state\n",
				vss->sid);
			return -EBADFD;
		}

		vss->buffer_bytes = snd_pcm_lib_buffer_bytes(substream);
		vss->hw_ptr = 0;
		vss->msg_last_enqueued = -1;
	} else {
		struct snd_pcm_runtime *runtime = substream->runtime;
		unsigned int buffer_bytes = snd_pcm_lib_buffer_bytes(substream);
		unsigned int period_bytes = snd_pcm_lib_period_bytes(substream);
		int rc;

		rc = virtsnd_pcm_dev_set_params(vss, buffer_bytes, period_bytes,
						runtime->channels,
						runtime->format, runtime->rate);
		if (rc)
			return rc;
	}

	vss->xfer_xrun = false;
	vss->suspended = false;
	vss->msg_count = 0;

	msg = virtsnd_pcm_ctl_msg_alloc(vss, VIRTIO_SND_R_PCM_PREPARE,
					GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	return virtsnd_ctl_msg_send_sync(vss->snd, msg);
}

 
static int virtsnd_pcm_trigger(struct snd_pcm_substream *substream, int command)
{
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	struct virtio_snd *snd = vss->snd;
	struct virtio_snd_queue *queue;
	struct virtio_snd_msg *msg;
	unsigned long flags;
	int rc;

	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		queue = virtsnd_pcm_queue(vss);

		spin_lock_irqsave(&queue->lock, flags);
		spin_lock(&vss->lock);
		rc = virtsnd_pcm_msg_send(vss);
		if (!rc)
			vss->xfer_enabled = true;
		spin_unlock(&vss->lock);
		spin_unlock_irqrestore(&queue->lock, flags);
		if (rc)
			return rc;

		msg = virtsnd_pcm_ctl_msg_alloc(vss, VIRTIO_SND_R_PCM_START,
						GFP_KERNEL);
		if (!msg) {
			spin_lock_irqsave(&vss->lock, flags);
			vss->xfer_enabled = false;
			spin_unlock_irqrestore(&vss->lock, flags);

			return -ENOMEM;
		}

		return virtsnd_ctl_msg_send_sync(snd, msg);
	case SNDRV_PCM_TRIGGER_SUSPEND:
		vss->suspended = true;
		fallthrough;
	case SNDRV_PCM_TRIGGER_STOP:
		vss->stopped = true;
		fallthrough;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		spin_lock_irqsave(&vss->lock, flags);
		vss->xfer_enabled = false;
		spin_unlock_irqrestore(&vss->lock, flags);

		msg = virtsnd_pcm_ctl_msg_alloc(vss, VIRTIO_SND_R_PCM_STOP,
						GFP_KERNEL);
		if (!msg)
			return -ENOMEM;

		return virtsnd_ctl_msg_send_sync(snd, msg);
	default:
		return -EINVAL;
	}
}

 
static int virtsnd_pcm_sync_stop(struct snd_pcm_substream *substream)
{
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	struct virtio_snd *snd = vss->snd;
	struct virtio_snd_msg *msg;
	unsigned int js = msecs_to_jiffies(virtsnd_msg_timeout_ms);
	int rc;

	cancel_work_sync(&vss->elapsed_period);

	if (!vss->stopped)
		return 0;

	msg = virtsnd_pcm_ctl_msg_alloc(vss, VIRTIO_SND_R_PCM_RELEASE,
					GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	rc = virtsnd_ctl_msg_send_sync(snd, msg);
	if (rc)
		return rc;

	 
	rc = wait_event_interruptible_timeout(vss->msg_empty,
					      !virtsnd_pcm_msg_pending_num(vss),
					      js);
	if (rc <= 0) {
		dev_warn(&snd->vdev->dev, "SID %u: failed to flush I/O queue\n",
			 vss->sid);

		return !rc ? -ETIMEDOUT : rc;
	}

	vss->stopped = false;

	return 0;
}

 
static snd_pcm_uframes_t
virtsnd_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct virtio_pcm_substream *vss = snd_pcm_substream_chip(substream);
	snd_pcm_uframes_t hw_ptr = SNDRV_PCM_POS_XRUN;
	unsigned long flags;

	spin_lock_irqsave(&vss->lock, flags);
	if (!vss->xfer_xrun)
		hw_ptr = bytes_to_frames(substream->runtime, vss->hw_ptr);
	spin_unlock_irqrestore(&vss->lock, flags);

	return hw_ptr;
}

 
const struct snd_pcm_ops virtsnd_pcm_ops = {
	.open = virtsnd_pcm_open,
	.close = virtsnd_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = virtsnd_pcm_hw_params,
	.hw_free = virtsnd_pcm_hw_free,
	.prepare = virtsnd_pcm_prepare,
	.trigger = virtsnd_pcm_trigger,
	.sync_stop = virtsnd_pcm_sync_stop,
	.pointer = virtsnd_pcm_pointer,
};
