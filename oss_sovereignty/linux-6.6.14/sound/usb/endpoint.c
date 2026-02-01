
 

#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/ratelimit.h>
#include <linux/usb.h>
#include <linux/usb/audio.h>
#include <linux/slab.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include "usbaudio.h"
#include "helper.h"
#include "card.h"
#include "endpoint.h"
#include "pcm.h"
#include "clock.h"
#include "quirks.h"

enum {
	EP_STATE_STOPPED,
	EP_STATE_RUNNING,
	EP_STATE_STOPPING,
};

 
struct snd_usb_iface_ref {
	unsigned char iface;
	bool need_setup;
	int opened;
	int altset;
	struct list_head list;
};

 
struct snd_usb_clock_ref {
	unsigned char clock;
	atomic_t locked;
	int opened;
	int rate;
	bool need_setup;
	struct list_head list;
};

 

 
static inline unsigned get_usb_full_speed_rate(unsigned int rate)
{
	return ((rate << 13) + 62) / 125;
}

 
static inline unsigned get_usb_high_speed_rate(unsigned int rate)
{
	return ((rate << 10) + 62) / 125;
}

 
static void release_urb_ctx(struct snd_urb_ctx *u)
{
	if (u->urb && u->buffer_size)
		usb_free_coherent(u->ep->chip->dev, u->buffer_size,
				  u->urb->transfer_buffer,
				  u->urb->transfer_dma);
	usb_free_urb(u->urb);
	u->urb = NULL;
	u->buffer_size = 0;
}

static const char *usb_error_string(int err)
{
	switch (err) {
	case -ENODEV:
		return "no device";
	case -ENOENT:
		return "endpoint not enabled";
	case -EPIPE:
		return "endpoint stalled";
	case -ENOSPC:
		return "not enough bandwidth";
	case -ESHUTDOWN:
		return "device disabled";
	case -EHOSTUNREACH:
		return "device suspended";
	case -EINVAL:
	case -EAGAIN:
	case -EFBIG:
	case -EMSGSIZE:
		return "internal error";
	default:
		return "unknown error";
	}
}

static inline bool ep_state_running(struct snd_usb_endpoint *ep)
{
	return atomic_read(&ep->state) == EP_STATE_RUNNING;
}

static inline bool ep_state_update(struct snd_usb_endpoint *ep, int old, int new)
{
	return atomic_try_cmpxchg(&ep->state, &old, new);
}

 
int snd_usb_endpoint_implicit_feedback_sink(struct snd_usb_endpoint *ep)
{
	return  ep->implicit_fb_sync && usb_pipeout(ep->pipe);
}

 
static int slave_next_packet_size(struct snd_usb_endpoint *ep,
				  unsigned int avail)
{
	unsigned long flags;
	unsigned int phase;
	int ret;

	if (ep->fill_max)
		return ep->maxframesize;

	spin_lock_irqsave(&ep->lock, flags);
	phase = (ep->phase & 0xffff) + (ep->freqm << ep->datainterval);
	ret = min(phase >> 16, ep->maxframesize);
	if (avail && ret >= avail)
		ret = -EAGAIN;
	else
		ep->phase = phase;
	spin_unlock_irqrestore(&ep->lock, flags);

	return ret;
}

 
static int next_packet_size(struct snd_usb_endpoint *ep, unsigned int avail)
{
	unsigned int sample_accum;
	int ret;

	if (ep->fill_max)
		return ep->maxframesize;

	sample_accum = ep->sample_accum + ep->sample_rem;
	if (sample_accum >= ep->pps) {
		sample_accum -= ep->pps;
		ret = ep->packsize[1];
	} else {
		ret = ep->packsize[0];
	}
	if (avail && ret >= avail)
		ret = -EAGAIN;
	else
		ep->sample_accum = sample_accum;

	return ret;
}

 
int snd_usb_endpoint_next_packet_size(struct snd_usb_endpoint *ep,
				      struct snd_urb_ctx *ctx, int idx,
				      unsigned int avail)
{
	unsigned int packet;

	packet = ctx->packet_size[idx];
	if (packet) {
		if (avail && packet >= avail)
			return -EAGAIN;
		return packet;
	}

	if (ep->sync_source)
		return slave_next_packet_size(ep, avail);
	else
		return next_packet_size(ep, avail);
}

static void call_retire_callback(struct snd_usb_endpoint *ep,
				 struct urb *urb)
{
	struct snd_usb_substream *data_subs;

	data_subs = READ_ONCE(ep->data_subs);
	if (data_subs && ep->retire_data_urb)
		ep->retire_data_urb(data_subs, urb);
}

static void retire_outbound_urb(struct snd_usb_endpoint *ep,
				struct snd_urb_ctx *urb_ctx)
{
	call_retire_callback(ep, urb_ctx->urb);
}

static void snd_usb_handle_sync_urb(struct snd_usb_endpoint *ep,
				    struct snd_usb_endpoint *sender,
				    const struct urb *urb);

static void retire_inbound_urb(struct snd_usb_endpoint *ep,
			       struct snd_urb_ctx *urb_ctx)
{
	struct urb *urb = urb_ctx->urb;
	struct snd_usb_endpoint *sync_sink;

	if (unlikely(ep->skip_packets > 0)) {
		ep->skip_packets--;
		return;
	}

	sync_sink = READ_ONCE(ep->sync_sink);
	if (sync_sink)
		snd_usb_handle_sync_urb(sync_sink, ep, urb);

	call_retire_callback(ep, urb);
}

static inline bool has_tx_length_quirk(struct snd_usb_audio *chip)
{
	return chip->quirk_flags & QUIRK_FLAG_TX_LENGTH;
}

static void prepare_silent_urb(struct snd_usb_endpoint *ep,
			       struct snd_urb_ctx *ctx)
{
	struct urb *urb = ctx->urb;
	unsigned int offs = 0;
	unsigned int extra = 0;
	__le32 packet_length;
	int i;

	 
	if (has_tx_length_quirk(ep->chip))
		extra = sizeof(packet_length);

	for (i = 0; i < ctx->packets; ++i) {
		unsigned int offset;
		unsigned int length;
		int counts;

		counts = snd_usb_endpoint_next_packet_size(ep, ctx, i, 0);
		length = counts * ep->stride;  
		offset = offs * ep->stride + extra * i;
		urb->iso_frame_desc[i].offset = offset;
		urb->iso_frame_desc[i].length = length + extra;
		if (extra) {
			packet_length = cpu_to_le32(length);
			memcpy(urb->transfer_buffer + offset,
			       &packet_length, sizeof(packet_length));
		}
		memset(urb->transfer_buffer + offset + extra,
		       ep->silence_value, length);
		offs += counts;
	}

	urb->number_of_packets = ctx->packets;
	urb->transfer_buffer_length = offs * ep->stride + ctx->packets * extra;
	ctx->queued = 0;
}

 
static int prepare_outbound_urb(struct snd_usb_endpoint *ep,
				struct snd_urb_ctx *ctx,
				bool in_stream_lock)
{
	struct urb *urb = ctx->urb;
	unsigned char *cp = urb->transfer_buffer;
	struct snd_usb_substream *data_subs;

	urb->dev = ep->chip->dev;  

	switch (ep->type) {
	case SND_USB_ENDPOINT_TYPE_DATA:
		data_subs = READ_ONCE(ep->data_subs);
		if (data_subs && ep->prepare_data_urb)
			return ep->prepare_data_urb(data_subs, urb, in_stream_lock);
		 
		prepare_silent_urb(ep, ctx);
		break;

	case SND_USB_ENDPOINT_TYPE_SYNC:
		if (snd_usb_get_speed(ep->chip->dev) >= USB_SPEED_HIGH) {
			 
			urb->iso_frame_desc[0].length = 4;
			urb->iso_frame_desc[0].offset = 0;
			cp[0] = ep->freqn;
			cp[1] = ep->freqn >> 8;
			cp[2] = ep->freqn >> 16;
			cp[3] = ep->freqn >> 24;
		} else {
			 
			urb->iso_frame_desc[0].length = 3;
			urb->iso_frame_desc[0].offset = 0;
			cp[0] = ep->freqn >> 2;
			cp[1] = ep->freqn >> 10;
			cp[2] = ep->freqn >> 18;
		}

		break;
	}
	return 0;
}

 
static int prepare_inbound_urb(struct snd_usb_endpoint *ep,
			       struct snd_urb_ctx *urb_ctx)
{
	int i, offs;
	struct urb *urb = urb_ctx->urb;

	urb->dev = ep->chip->dev;  

	switch (ep->type) {
	case SND_USB_ENDPOINT_TYPE_DATA:
		offs = 0;
		for (i = 0; i < urb_ctx->packets; i++) {
			urb->iso_frame_desc[i].offset = offs;
			urb->iso_frame_desc[i].length = ep->curpacksize;
			offs += ep->curpacksize;
		}

		urb->transfer_buffer_length = offs;
		urb->number_of_packets = urb_ctx->packets;
		break;

	case SND_USB_ENDPOINT_TYPE_SYNC:
		urb->iso_frame_desc[0].length = min(4u, ep->syncmaxsize);
		urb->iso_frame_desc[0].offset = 0;
		break;
	}
	return 0;
}

 
static void notify_xrun(struct snd_usb_endpoint *ep)
{
	struct snd_usb_substream *data_subs;

	data_subs = READ_ONCE(ep->data_subs);
	if (data_subs && data_subs->pcm_substream)
		snd_pcm_stop_xrun(data_subs->pcm_substream);
}

static struct snd_usb_packet_info *
next_packet_fifo_enqueue(struct snd_usb_endpoint *ep)
{
	struct snd_usb_packet_info *p;

	p = ep->next_packet + (ep->next_packet_head + ep->next_packet_queued) %
		ARRAY_SIZE(ep->next_packet);
	ep->next_packet_queued++;
	return p;
}

static struct snd_usb_packet_info *
next_packet_fifo_dequeue(struct snd_usb_endpoint *ep)
{
	struct snd_usb_packet_info *p;

	p = ep->next_packet + ep->next_packet_head;
	ep->next_packet_head++;
	ep->next_packet_head %= ARRAY_SIZE(ep->next_packet);
	ep->next_packet_queued--;
	return p;
}

static void push_back_to_ready_list(struct snd_usb_endpoint *ep,
				    struct snd_urb_ctx *ctx)
{
	unsigned long flags;

	spin_lock_irqsave(&ep->lock, flags);
	list_add_tail(&ctx->ready_list, &ep->ready_playback_urbs);
	spin_unlock_irqrestore(&ep->lock, flags);
}

 
int snd_usb_queue_pending_output_urbs(struct snd_usb_endpoint *ep,
				      bool in_stream_lock)
{
	bool implicit_fb = snd_usb_endpoint_implicit_feedback_sink(ep);

	while (ep_state_running(ep)) {

		unsigned long flags;
		struct snd_usb_packet_info *packet;
		struct snd_urb_ctx *ctx = NULL;
		int err, i;

		spin_lock_irqsave(&ep->lock, flags);
		if ((!implicit_fb || ep->next_packet_queued > 0) &&
		    !list_empty(&ep->ready_playback_urbs)) {
			 
			ctx = list_first_entry(&ep->ready_playback_urbs,
					       struct snd_urb_ctx, ready_list);
			list_del_init(&ctx->ready_list);
			if (implicit_fb)
				packet = next_packet_fifo_dequeue(ep);
		}
		spin_unlock_irqrestore(&ep->lock, flags);

		if (ctx == NULL)
			break;

		 
		if (implicit_fb) {
			for (i = 0; i < packet->packets; i++)
				ctx->packet_size[i] = packet->packet_size[i];
		}

		 
		err = prepare_outbound_urb(ep, ctx, in_stream_lock);
		 
		if (unlikely(!ep_state_running(ep)))
			break;
		if (err < 0) {
			 
			if (err == -EAGAIN) {
				push_back_to_ready_list(ep, ctx);
				break;
			}

			if (!in_stream_lock)
				notify_xrun(ep);
			return -EPIPE;
		}

		if (!atomic_read(&ep->chip->shutdown))
			err = usb_submit_urb(ctx->urb, GFP_ATOMIC);
		else
			err = -ENODEV;
		if (err < 0) {
			if (!atomic_read(&ep->chip->shutdown)) {
				usb_audio_err(ep->chip,
					      "Unable to submit urb #%d: %d at %s\n",
					      ctx->index, err, __func__);
				if (!in_stream_lock)
					notify_xrun(ep);
			}
			return -EPIPE;
		}

		set_bit(ctx->index, &ep->active_mask);
		atomic_inc(&ep->submitted_urbs);
	}

	return 0;
}

 
static void snd_complete_urb(struct urb *urb)
{
	struct snd_urb_ctx *ctx = urb->context;
	struct snd_usb_endpoint *ep = ctx->ep;
	int err;

	if (unlikely(urb->status == -ENOENT ||		 
		     urb->status == -ENODEV ||		 
		     urb->status == -ECONNRESET ||	 
		     urb->status == -ESHUTDOWN))	 
		goto exit_clear;
	 
	if (unlikely(atomic_read(&ep->chip->shutdown)))
		goto exit_clear;

	if (unlikely(!ep_state_running(ep)))
		goto exit_clear;

	if (usb_pipeout(ep->pipe)) {
		retire_outbound_urb(ep, ctx);
		 
		if (unlikely(!ep_state_running(ep)))
			goto exit_clear;

		 
		if (ep->lowlatency_playback ||
		     snd_usb_endpoint_implicit_feedback_sink(ep)) {
			push_back_to_ready_list(ep, ctx);
			clear_bit(ctx->index, &ep->active_mask);
			snd_usb_queue_pending_output_urbs(ep, false);
			atomic_dec(&ep->submitted_urbs);  
			return;
		}

		 
		prepare_outbound_urb(ep, ctx, false);
		 
		if (unlikely(!ep_state_running(ep)))
			goto exit_clear;
	} else {
		retire_inbound_urb(ep, ctx);
		 
		if (unlikely(!ep_state_running(ep)))
			goto exit_clear;

		prepare_inbound_urb(ep, ctx);
	}

	if (!atomic_read(&ep->chip->shutdown))
		err = usb_submit_urb(urb, GFP_ATOMIC);
	else
		err = -ENODEV;
	if (err == 0)
		return;

	if (!atomic_read(&ep->chip->shutdown)) {
		usb_audio_err(ep->chip, "cannot submit urb (err = %d)\n", err);
		notify_xrun(ep);
	}

exit_clear:
	clear_bit(ctx->index, &ep->active_mask);
	atomic_dec(&ep->submitted_urbs);
}

 
static struct snd_usb_iface_ref *
iface_ref_find(struct snd_usb_audio *chip, int iface)
{
	struct snd_usb_iface_ref *ip;

	list_for_each_entry(ip, &chip->iface_ref_list, list)
		if (ip->iface == iface)
			return ip;

	ip = kzalloc(sizeof(*ip), GFP_KERNEL);
	if (!ip)
		return NULL;
	ip->iface = iface;
	list_add_tail(&ip->list, &chip->iface_ref_list);
	return ip;
}

 
static struct snd_usb_clock_ref *
clock_ref_find(struct snd_usb_audio *chip, int clock)
{
	struct snd_usb_clock_ref *ref;

	list_for_each_entry(ref, &chip->clock_ref_list, list)
		if (ref->clock == clock)
			return ref;

	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref)
		return NULL;
	ref->clock = clock;
	atomic_set(&ref->locked, 0);
	list_add_tail(&ref->list, &chip->clock_ref_list);
	return ref;
}

 
struct snd_usb_endpoint *
snd_usb_get_endpoint(struct snd_usb_audio *chip, int ep_num)
{
	struct snd_usb_endpoint *ep;

	list_for_each_entry(ep, &chip->ep_list, list) {
		if (ep->ep_num == ep_num)
			return ep;
	}

	return NULL;
}

#define ep_type_name(type) \
	(type == SND_USB_ENDPOINT_TYPE_DATA ? "data" : "sync")

 
int snd_usb_add_endpoint(struct snd_usb_audio *chip, int ep_num, int type)
{
	struct snd_usb_endpoint *ep;
	bool is_playback;

	ep = snd_usb_get_endpoint(chip, ep_num);
	if (ep)
		return 0;

	usb_audio_dbg(chip, "Creating new %s endpoint #%x\n",
		      ep_type_name(type),
		      ep_num);
	ep = kzalloc(sizeof(*ep), GFP_KERNEL);
	if (!ep)
		return -ENOMEM;

	ep->chip = chip;
	spin_lock_init(&ep->lock);
	ep->type = type;
	ep->ep_num = ep_num;
	INIT_LIST_HEAD(&ep->ready_playback_urbs);
	atomic_set(&ep->submitted_urbs, 0);

	is_playback = ((ep_num & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT);
	ep_num &= USB_ENDPOINT_NUMBER_MASK;
	if (is_playback)
		ep->pipe = usb_sndisocpipe(chip->dev, ep_num);
	else
		ep->pipe = usb_rcvisocpipe(chip->dev, ep_num);

	list_add_tail(&ep->list, &chip->ep_list);
	return 0;
}

 
static void endpoint_set_syncinterval(struct snd_usb_audio *chip,
				      struct snd_usb_endpoint *ep)
{
	struct usb_host_interface *alts;
	struct usb_endpoint_descriptor *desc;

	alts = snd_usb_get_host_interface(chip, ep->iface, ep->altsetting);
	if (!alts)
		return;

	desc = get_endpoint(alts, ep->ep_idx);
	if (desc->bLength >= USB_DT_ENDPOINT_AUDIO_SIZE &&
	    desc->bRefresh >= 1 && desc->bRefresh <= 9)
		ep->syncinterval = desc->bRefresh;
	else if (snd_usb_get_speed(chip->dev) == USB_SPEED_FULL)
		ep->syncinterval = 1;
	else if (desc->bInterval >= 1 && desc->bInterval <= 16)
		ep->syncinterval = desc->bInterval - 1;
	else
		ep->syncinterval = 3;

	ep->syncmaxsize = le16_to_cpu(desc->wMaxPacketSize);
}

static bool endpoint_compatible(struct snd_usb_endpoint *ep,
				const struct audioformat *fp,
				const struct snd_pcm_hw_params *params)
{
	if (!ep->opened)
		return false;
	if (ep->cur_audiofmt != fp)
		return false;
	if (ep->cur_rate != params_rate(params) ||
	    ep->cur_format != params_format(params) ||
	    ep->cur_period_frames != params_period_size(params) ||
	    ep->cur_buffer_periods != params_periods(params))
		return false;
	return true;
}

 
bool snd_usb_endpoint_compatible(struct snd_usb_audio *chip,
				 struct snd_usb_endpoint *ep,
				 const struct audioformat *fp,
				 const struct snd_pcm_hw_params *params)
{
	bool ret;

	mutex_lock(&chip->mutex);
	ret = endpoint_compatible(ep, fp, params);
	mutex_unlock(&chip->mutex);
	return ret;
}

 
struct snd_usb_endpoint *
snd_usb_endpoint_open(struct snd_usb_audio *chip,
		      const struct audioformat *fp,
		      const struct snd_pcm_hw_params *params,
		      bool is_sync_ep,
		      bool fixed_rate)
{
	struct snd_usb_endpoint *ep;
	int ep_num = is_sync_ep ? fp->sync_ep : fp->endpoint;

	mutex_lock(&chip->mutex);
	ep = snd_usb_get_endpoint(chip, ep_num);
	if (!ep) {
		usb_audio_err(chip, "Cannot find EP 0x%x to open\n", ep_num);
		goto unlock;
	}

	if (!ep->opened) {
		if (is_sync_ep) {
			ep->iface = fp->sync_iface;
			ep->altsetting = fp->sync_altsetting;
			ep->ep_idx = fp->sync_ep_idx;
		} else {
			ep->iface = fp->iface;
			ep->altsetting = fp->altsetting;
			ep->ep_idx = fp->ep_idx;
		}
		usb_audio_dbg(chip, "Open EP 0x%x, iface=%d:%d, idx=%d\n",
			      ep_num, ep->iface, ep->altsetting, ep->ep_idx);

		ep->iface_ref = iface_ref_find(chip, ep->iface);
		if (!ep->iface_ref) {
			ep = NULL;
			goto unlock;
		}

		if (fp->protocol != UAC_VERSION_1) {
			ep->clock_ref = clock_ref_find(chip, fp->clock);
			if (!ep->clock_ref) {
				ep = NULL;
				goto unlock;
			}
			ep->clock_ref->opened++;
		}

		ep->cur_audiofmt = fp;
		ep->cur_channels = fp->channels;
		ep->cur_rate = params_rate(params);
		ep->cur_format = params_format(params);
		ep->cur_frame_bytes = snd_pcm_format_physical_width(ep->cur_format) *
			ep->cur_channels / 8;
		ep->cur_period_frames = params_period_size(params);
		ep->cur_period_bytes = ep->cur_period_frames * ep->cur_frame_bytes;
		ep->cur_buffer_periods = params_periods(params);

		if (ep->type == SND_USB_ENDPOINT_TYPE_SYNC)
			endpoint_set_syncinterval(chip, ep);

		ep->implicit_fb_sync = fp->implicit_fb;
		ep->need_setup = true;
		ep->need_prepare = true;
		ep->fixed_rate = fixed_rate;

		usb_audio_dbg(chip, "  channels=%d, rate=%d, format=%s, period_bytes=%d, periods=%d, implicit_fb=%d\n",
			      ep->cur_channels, ep->cur_rate,
			      snd_pcm_format_name(ep->cur_format),
			      ep->cur_period_bytes, ep->cur_buffer_periods,
			      ep->implicit_fb_sync);

	} else {
		if (WARN_ON(!ep->iface_ref)) {
			ep = NULL;
			goto unlock;
		}

		if (!endpoint_compatible(ep, fp, params)) {
			usb_audio_err(chip, "Incompatible EP setup for 0x%x\n",
				      ep_num);
			ep = NULL;
			goto unlock;
		}

		usb_audio_dbg(chip, "Reopened EP 0x%x (count %d)\n",
			      ep_num, ep->opened);
	}

	if (!ep->iface_ref->opened++)
		ep->iface_ref->need_setup = true;

	ep->opened++;

 unlock:
	mutex_unlock(&chip->mutex);
	return ep;
}

 
void snd_usb_endpoint_set_sync(struct snd_usb_audio *chip,
			       struct snd_usb_endpoint *data_ep,
			       struct snd_usb_endpoint *sync_ep)
{
	data_ep->sync_source = sync_ep;
}

 
void snd_usb_endpoint_set_callback(struct snd_usb_endpoint *ep,
				   int (*prepare)(struct snd_usb_substream *subs,
						  struct urb *urb,
						  bool in_stream_lock),
				   void (*retire)(struct snd_usb_substream *subs,
						  struct urb *urb),
				   struct snd_usb_substream *data_subs)
{
	ep->prepare_data_urb = prepare;
	ep->retire_data_urb = retire;
	if (data_subs)
		ep->lowlatency_playback = data_subs->lowlatency_playback;
	else
		ep->lowlatency_playback = false;
	WRITE_ONCE(ep->data_subs, data_subs);
}

static int endpoint_set_interface(struct snd_usb_audio *chip,
				  struct snd_usb_endpoint *ep,
				  bool set)
{
	int altset = set ? ep->altsetting : 0;
	int err;

	if (ep->iface_ref->altset == altset)
		return 0;

	usb_audio_dbg(chip, "Setting usb interface %d:%d for EP 0x%x\n",
		      ep->iface, altset, ep->ep_num);
	err = usb_set_interface(chip->dev, ep->iface, altset);
	if (err < 0) {
		usb_audio_err_ratelimited(
			chip, "%d:%d: usb_set_interface failed (%d)\n",
			ep->iface, altset, err);
		return err;
	}

	if (chip->quirk_flags & QUIRK_FLAG_IFACE_DELAY)
		msleep(50);
	ep->iface_ref->altset = altset;
	return 0;
}

 
void snd_usb_endpoint_close(struct snd_usb_audio *chip,
			    struct snd_usb_endpoint *ep)
{
	mutex_lock(&chip->mutex);
	usb_audio_dbg(chip, "Closing EP 0x%x (count %d)\n",
		      ep->ep_num, ep->opened);

	if (!--ep->iface_ref->opened &&
		!(chip->quirk_flags & QUIRK_FLAG_IFACE_SKIP_CLOSE))
		endpoint_set_interface(chip, ep, false);

	if (!--ep->opened) {
		if (ep->clock_ref) {
			if (!--ep->clock_ref->opened)
				ep->clock_ref->rate = 0;
		}
		ep->iface = 0;
		ep->altsetting = 0;
		ep->cur_audiofmt = NULL;
		ep->cur_rate = 0;
		ep->iface_ref = NULL;
		ep->clock_ref = NULL;
		usb_audio_dbg(chip, "EP 0x%x closed\n", ep->ep_num);
	}
	mutex_unlock(&chip->mutex);
}

 
void snd_usb_endpoint_suspend(struct snd_usb_endpoint *ep)
{
	ep->need_prepare = true;
	if (ep->iface_ref)
		ep->iface_ref->need_setup = true;
	if (ep->clock_ref)
		ep->clock_ref->rate = 0;
}

 
static int wait_clear_urbs(struct snd_usb_endpoint *ep)
{
	unsigned long end_time = jiffies + msecs_to_jiffies(1000);
	int alive;

	if (atomic_read(&ep->state) != EP_STATE_STOPPING)
		return 0;

	do {
		alive = atomic_read(&ep->submitted_urbs);
		if (!alive)
			break;

		schedule_timeout_uninterruptible(1);
	} while (time_before(jiffies, end_time));

	if (alive)
		usb_audio_err(ep->chip,
			"timeout: still %d active urbs on EP #%x\n",
			alive, ep->ep_num);

	if (ep_state_update(ep, EP_STATE_STOPPING, EP_STATE_STOPPED)) {
		ep->sync_sink = NULL;
		snd_usb_endpoint_set_callback(ep, NULL, NULL, NULL);
	}

	return 0;
}

 
void snd_usb_endpoint_sync_pending_stop(struct snd_usb_endpoint *ep)
{
	if (ep)
		wait_clear_urbs(ep);
}

 
static int stop_urbs(struct snd_usb_endpoint *ep, bool force, bool keep_pending)
{
	unsigned int i;
	unsigned long flags;

	if (!force && atomic_read(&ep->running))
		return -EBUSY;

	if (!ep_state_update(ep, EP_STATE_RUNNING, EP_STATE_STOPPING))
		return 0;

	spin_lock_irqsave(&ep->lock, flags);
	INIT_LIST_HEAD(&ep->ready_playback_urbs);
	ep->next_packet_head = 0;
	ep->next_packet_queued = 0;
	spin_unlock_irqrestore(&ep->lock, flags);

	if (keep_pending)
		return 0;

	for (i = 0; i < ep->nurbs; i++) {
		if (test_bit(i, &ep->active_mask)) {
			if (!test_and_set_bit(i, &ep->unlink_mask)) {
				struct urb *u = ep->urb[i].urb;
				usb_unlink_urb(u);
			}
		}
	}

	return 0;
}

 
static int release_urbs(struct snd_usb_endpoint *ep, bool force)
{
	int i, err;

	 
	snd_usb_endpoint_set_callback(ep, NULL, NULL, NULL);

	 
	err = stop_urbs(ep, force, false);
	if (err)
		return err;

	wait_clear_urbs(ep);

	for (i = 0; i < ep->nurbs; i++)
		release_urb_ctx(&ep->urb[i]);

	usb_free_coherent(ep->chip->dev, SYNC_URBS * 4,
			  ep->syncbuf, ep->sync_dma);

	ep->syncbuf = NULL;
	ep->nurbs = 0;
	return 0;
}

 
static int data_ep_set_params(struct snd_usb_endpoint *ep)
{
	struct snd_usb_audio *chip = ep->chip;
	unsigned int maxsize, minsize, packs_per_ms, max_packs_per_urb;
	unsigned int max_packs_per_period, urbs_per_period, urb_packs;
	unsigned int max_urbs, i;
	const struct audioformat *fmt = ep->cur_audiofmt;
	int frame_bits = ep->cur_frame_bytes * 8;
	int tx_length_quirk = (has_tx_length_quirk(chip) &&
			       usb_pipeout(ep->pipe));

	usb_audio_dbg(chip, "Setting params for data EP 0x%x, pipe 0x%x\n",
		      ep->ep_num, ep->pipe);

	if (ep->cur_format == SNDRV_PCM_FORMAT_DSD_U16_LE && fmt->dsd_dop) {
		 
		frame_bits += ep->cur_channels << 3;
	}

	ep->datainterval = fmt->datainterval;
	ep->stride = frame_bits >> 3;

	switch (ep->cur_format) {
	case SNDRV_PCM_FORMAT_U8:
		ep->silence_value = 0x80;
		break;
	case SNDRV_PCM_FORMAT_DSD_U8:
	case SNDRV_PCM_FORMAT_DSD_U16_LE:
	case SNDRV_PCM_FORMAT_DSD_U32_LE:
	case SNDRV_PCM_FORMAT_DSD_U16_BE:
	case SNDRV_PCM_FORMAT_DSD_U32_BE:
		ep->silence_value = 0x69;
		break;
	default:
		ep->silence_value = 0;
	}

	 
	ep->freqmax = ep->freqn + (ep->freqn >> 1);
	 
	maxsize = (((ep->freqmax << ep->datainterval) + 0xffff) >> 16) *
			 (frame_bits >> 3);
	if (tx_length_quirk)
		maxsize += sizeof(__le32);  
	 
	if (ep->maxpacksize && ep->maxpacksize < maxsize) {
		 
		unsigned int data_maxsize = maxsize = ep->maxpacksize;

		if (tx_length_quirk)
			 
			data_maxsize -= sizeof(__le32);
		ep->freqmax = (data_maxsize / (frame_bits >> 3))
				<< (16 - ep->datainterval);
	}

	if (ep->fill_max)
		ep->curpacksize = ep->maxpacksize;
	else
		ep->curpacksize = maxsize;

	if (snd_usb_get_speed(chip->dev) != USB_SPEED_FULL) {
		packs_per_ms = 8 >> ep->datainterval;
		max_packs_per_urb = MAX_PACKS_HS;
	} else {
		packs_per_ms = 1;
		max_packs_per_urb = MAX_PACKS;
	}
	if (ep->sync_source && !ep->implicit_fb_sync)
		max_packs_per_urb = min(max_packs_per_urb,
					1U << ep->sync_source->syncinterval);
	max_packs_per_urb = max(1u, max_packs_per_urb >> ep->datainterval);

	 
	if (usb_pipein(ep->pipe) || ep->implicit_fb_sync) {

		 
		urb_packs = min(max_packs_per_urb, packs_per_ms);
		while (urb_packs > 1 && urb_packs * maxsize >= ep->cur_period_bytes)
			urb_packs >>= 1;
		ep->nurbs = MAX_URBS;

	 
	} else {
		 
		minsize = (ep->freqn >> (16 - ep->datainterval)) *
				(frame_bits >> 3);
		 
		if (ep->sync_source)
			minsize -= minsize >> 3;
		minsize = max(minsize, 1u);

		 
		max_packs_per_period = DIV_ROUND_UP(ep->cur_period_bytes, minsize);

		 
		urbs_per_period = DIV_ROUND_UP(max_packs_per_period,
				max_packs_per_urb);
		 
		urb_packs = DIV_ROUND_UP(max_packs_per_period, urbs_per_period);

		 
		ep->max_urb_frames = DIV_ROUND_UP(ep->cur_period_frames,
						  urbs_per_period);

		 
		max_urbs = min((unsigned) MAX_URBS,
				MAX_QUEUE * packs_per_ms / urb_packs);
		ep->nurbs = min(max_urbs, urbs_per_period * ep->cur_buffer_periods);
	}

	 
	for (i = 0; i < ep->nurbs; i++) {
		struct snd_urb_ctx *u = &ep->urb[i];
		u->index = i;
		u->ep = ep;
		u->packets = urb_packs;
		u->buffer_size = maxsize * u->packets;

		if (fmt->fmt_type == UAC_FORMAT_TYPE_II)
			u->packets++;  
		u->urb = usb_alloc_urb(u->packets, GFP_KERNEL);
		if (!u->urb)
			goto out_of_memory;

		u->urb->transfer_buffer =
			usb_alloc_coherent(chip->dev, u->buffer_size,
					   GFP_KERNEL, &u->urb->transfer_dma);
		if (!u->urb->transfer_buffer)
			goto out_of_memory;
		u->urb->pipe = ep->pipe;
		u->urb->transfer_flags = URB_NO_TRANSFER_DMA_MAP;
		u->urb->interval = 1 << ep->datainterval;
		u->urb->context = u;
		u->urb->complete = snd_complete_urb;
		INIT_LIST_HEAD(&u->ready_list);
	}

	return 0;

out_of_memory:
	release_urbs(ep, false);
	return -ENOMEM;
}

 
static int sync_ep_set_params(struct snd_usb_endpoint *ep)
{
	struct snd_usb_audio *chip = ep->chip;
	int i;

	usb_audio_dbg(chip, "Setting params for sync EP 0x%x, pipe 0x%x\n",
		      ep->ep_num, ep->pipe);

	ep->syncbuf = usb_alloc_coherent(chip->dev, SYNC_URBS * 4,
					 GFP_KERNEL, &ep->sync_dma);
	if (!ep->syncbuf)
		return -ENOMEM;

	ep->nurbs = SYNC_URBS;
	for (i = 0; i < SYNC_URBS; i++) {
		struct snd_urb_ctx *u = &ep->urb[i];
		u->index = i;
		u->ep = ep;
		u->packets = 1;
		u->urb = usb_alloc_urb(1, GFP_KERNEL);
		if (!u->urb)
			goto out_of_memory;
		u->urb->transfer_buffer = ep->syncbuf + i * 4;
		u->urb->transfer_dma = ep->sync_dma + i * 4;
		u->urb->transfer_buffer_length = 4;
		u->urb->pipe = ep->pipe;
		u->urb->transfer_flags = URB_NO_TRANSFER_DMA_MAP;
		u->urb->number_of_packets = 1;
		u->urb->interval = 1 << ep->syncinterval;
		u->urb->context = u;
		u->urb->complete = snd_complete_urb;
	}

	return 0;

out_of_memory:
	release_urbs(ep, false);
	return -ENOMEM;
}

 
static int update_clock_ref_rate(struct snd_usb_audio *chip,
				 struct snd_usb_endpoint *ep)
{
	struct snd_usb_clock_ref *clock = ep->clock_ref;
	int rate = ep->cur_rate;

	if (!clock || clock->rate == rate)
		return rate;
	if (clock->rate) {
		if (atomic_read(&clock->locked))
			return clock->rate;
		if (clock->rate != rate) {
			usb_audio_err(chip, "Mismatched sample rate %d vs %d for EP 0x%x\n",
				      clock->rate, rate, ep->ep_num);
			return clock->rate;
		}
	}
	clock->rate = rate;
	clock->need_setup = true;
	return rate;
}

 
int snd_usb_endpoint_set_params(struct snd_usb_audio *chip,
				struct snd_usb_endpoint *ep)
{
	const struct audioformat *fmt = ep->cur_audiofmt;
	int err = 0;

	mutex_lock(&chip->mutex);
	if (!ep->need_setup)
		goto unlock;

	 
	err = release_urbs(ep, false);
	if (err < 0)
		goto unlock;

	ep->datainterval = fmt->datainterval;
	ep->maxpacksize = fmt->maxpacksize;
	ep->fill_max = !!(fmt->attributes & UAC_EP_CS_ATTR_FILL_MAX);

	if (snd_usb_get_speed(chip->dev) == USB_SPEED_FULL) {
		ep->freqn = get_usb_full_speed_rate(ep->cur_rate);
		ep->pps = 1000 >> ep->datainterval;
	} else {
		ep->freqn = get_usb_high_speed_rate(ep->cur_rate);
		ep->pps = 8000 >> ep->datainterval;
	}

	ep->sample_rem = ep->cur_rate % ep->pps;
	ep->packsize[0] = ep->cur_rate / ep->pps;
	ep->packsize[1] = (ep->cur_rate + (ep->pps - 1)) / ep->pps;

	 
	ep->freqm = ep->freqn;
	ep->freqshift = INT_MIN;

	ep->phase = 0;

	switch (ep->type) {
	case  SND_USB_ENDPOINT_TYPE_DATA:
		err = data_ep_set_params(ep);
		break;
	case  SND_USB_ENDPOINT_TYPE_SYNC:
		err = sync_ep_set_params(ep);
		break;
	default:
		err = -EINVAL;
	}

	usb_audio_dbg(chip, "Set up %d URBS, ret=%d\n", ep->nurbs, err);

	if (err < 0)
		goto unlock;

	 
	ep->maxframesize = ep->maxpacksize / ep->cur_frame_bytes;
	ep->curframesize = ep->curpacksize / ep->cur_frame_bytes;

	err = update_clock_ref_rate(chip, ep);
	if (err >= 0) {
		ep->need_setup = false;
		err = 0;
	}

 unlock:
	mutex_unlock(&chip->mutex);
	return err;
}

static int init_sample_rate(struct snd_usb_audio *chip,
			    struct snd_usb_endpoint *ep)
{
	struct snd_usb_clock_ref *clock = ep->clock_ref;
	int rate, err;

	rate = update_clock_ref_rate(chip, ep);
	if (rate < 0)
		return rate;
	if (clock && !clock->need_setup)
		return 0;

	if (!ep->fixed_rate) {
		err = snd_usb_init_sample_rate(chip, ep->cur_audiofmt, rate);
		if (err < 0) {
			if (clock)
				clock->rate = 0;  
			return err;
		}
	}

	if (clock)
		clock->need_setup = false;
	return 0;
}

 
int snd_usb_endpoint_prepare(struct snd_usb_audio *chip,
			     struct snd_usb_endpoint *ep)
{
	bool iface_first;
	int err = 0;

	mutex_lock(&chip->mutex);
	if (WARN_ON(!ep->iface_ref))
		goto unlock;
	if (!ep->need_prepare)
		goto unlock;

	 
	if (!ep->iface_ref->need_setup) {
		 
		if (ep->cur_audiofmt->protocol == UAC_VERSION_1) {
			err = init_sample_rate(chip, ep);
			if (err < 0)
				goto unlock;
		}
		goto done;
	}

	 
	endpoint_set_interface(chip, ep, false);

	 
	iface_first = ep->cur_audiofmt->protocol == UAC_VERSION_1;
	 
	if (chip->quirk_flags & QUIRK_FLAG_SET_IFACE_FIRST)
		iface_first = true;
	if (iface_first) {
		err = endpoint_set_interface(chip, ep, true);
		if (err < 0)
			goto unlock;
	}

	err = snd_usb_init_pitch(chip, ep->cur_audiofmt);
	if (err < 0)
		goto unlock;

	err = init_sample_rate(chip, ep);
	if (err < 0)
		goto unlock;

	err = snd_usb_select_mode_quirk(chip, ep->cur_audiofmt);
	if (err < 0)
		goto unlock;

	 
	if (!iface_first) {
		err = endpoint_set_interface(chip, ep, true);
		if (err < 0)
			goto unlock;
	}

	ep->iface_ref->need_setup = false;

 done:
	ep->need_prepare = false;
	err = 1;

unlock:
	mutex_unlock(&chip->mutex);
	return err;
}

 
int snd_usb_endpoint_get_clock_rate(struct snd_usb_audio *chip, int clock)
{
	struct snd_usb_clock_ref *ref;
	int rate = 0;

	if (!clock)
		return 0;
	mutex_lock(&chip->mutex);
	list_for_each_entry(ref, &chip->clock_ref_list, list) {
		if (ref->clock == clock) {
			rate = ref->rate;
			break;
		}
	}
	mutex_unlock(&chip->mutex);
	return rate;
}

 
int snd_usb_endpoint_start(struct snd_usb_endpoint *ep)
{
	bool is_playback = usb_pipeout(ep->pipe);
	int err;
	unsigned int i;

	if (atomic_read(&ep->chip->shutdown))
		return -EBADFD;

	if (ep->sync_source)
		WRITE_ONCE(ep->sync_source->sync_sink, ep);

	usb_audio_dbg(ep->chip, "Starting %s EP 0x%x (running %d)\n",
		      ep_type_name(ep->type), ep->ep_num,
		      atomic_read(&ep->running));

	 
	if (atomic_inc_return(&ep->running) != 1)
		return 0;

	if (ep->clock_ref)
		atomic_inc(&ep->clock_ref->locked);

	ep->active_mask = 0;
	ep->unlink_mask = 0;
	ep->phase = 0;
	ep->sample_accum = 0;

	snd_usb_endpoint_start_quirk(ep);

	 

	if (!ep_state_update(ep, EP_STATE_STOPPED, EP_STATE_RUNNING))
		goto __error;

	if (snd_usb_endpoint_implicit_feedback_sink(ep) &&
	    !(ep->chip->quirk_flags & QUIRK_FLAG_PLAYBACK_FIRST)) {
		usb_audio_dbg(ep->chip, "No URB submission due to implicit fb sync\n");
		i = 0;
		goto fill_rest;
	}

	for (i = 0; i < ep->nurbs; i++) {
		struct urb *urb = ep->urb[i].urb;

		if (snd_BUG_ON(!urb))
			goto __error;

		if (is_playback)
			err = prepare_outbound_urb(ep, urb->context, true);
		else
			err = prepare_inbound_urb(ep, urb->context);
		if (err < 0) {
			 
			if (err == -EAGAIN)
				break;
			usb_audio_dbg(ep->chip,
				      "EP 0x%x: failed to prepare urb: %d\n",
				      ep->ep_num, err);
			goto __error;
		}

		if (!atomic_read(&ep->chip->shutdown))
			err = usb_submit_urb(urb, GFP_ATOMIC);
		else
			err = -ENODEV;
		if (err < 0) {
			if (!atomic_read(&ep->chip->shutdown))
				usb_audio_err(ep->chip,
					      "cannot submit urb %d, error %d: %s\n",
					      i, err, usb_error_string(err));
			goto __error;
		}
		set_bit(i, &ep->active_mask);
		atomic_inc(&ep->submitted_urbs);
	}

	if (!i) {
		usb_audio_dbg(ep->chip, "XRUN at starting EP 0x%x\n",
			      ep->ep_num);
		goto __error;
	}

	usb_audio_dbg(ep->chip, "%d URBs submitted for EP 0x%x\n",
		      i, ep->ep_num);

 fill_rest:
	 
	if (is_playback) {
		for (; i < ep->nurbs; i++)
			push_back_to_ready_list(ep, ep->urb + i);
	}

	return 0;

__error:
	snd_usb_endpoint_stop(ep, false);
	return -EPIPE;
}

 
void snd_usb_endpoint_stop(struct snd_usb_endpoint *ep, bool keep_pending)
{
	if (!ep)
		return;

	usb_audio_dbg(ep->chip, "Stopping %s EP 0x%x (running %d)\n",
		      ep_type_name(ep->type), ep->ep_num,
		      atomic_read(&ep->running));

	if (snd_BUG_ON(!atomic_read(&ep->running)))
		return;

	if (!atomic_dec_return(&ep->running)) {
		if (ep->sync_source)
			WRITE_ONCE(ep->sync_source->sync_sink, NULL);
		stop_urbs(ep, false, keep_pending);
		if (ep->clock_ref)
			atomic_dec(&ep->clock_ref->locked);

		if (ep->chip->quirk_flags & QUIRK_FLAG_FORCE_IFACE_RESET &&
		    usb_pipeout(ep->pipe)) {
			ep->need_prepare = true;
			if (ep->iface_ref)
				ep->iface_ref->need_setup = true;
		}
	}
}

 
void snd_usb_endpoint_release(struct snd_usb_endpoint *ep)
{
	release_urbs(ep, true);
}

 
void snd_usb_endpoint_free_all(struct snd_usb_audio *chip)
{
	struct snd_usb_endpoint *ep, *en;
	struct snd_usb_iface_ref *ip, *in;
	struct snd_usb_clock_ref *cp, *cn;

	list_for_each_entry_safe(ep, en, &chip->ep_list, list)
		kfree(ep);

	list_for_each_entry_safe(ip, in, &chip->iface_ref_list, list)
		kfree(ip);

	list_for_each_entry_safe(cp, cn, &chip->clock_ref_list, list)
		kfree(cp);
}

 
static void snd_usb_handle_sync_urb(struct snd_usb_endpoint *ep,
				    struct snd_usb_endpoint *sender,
				    const struct urb *urb)
{
	int shift;
	unsigned int f;
	unsigned long flags;

	snd_BUG_ON(ep == sender);

	 
	if (snd_usb_endpoint_implicit_feedback_sink(ep) &&
	    atomic_read(&ep->running)) {

		 
		int i, bytes = 0;
		struct snd_urb_ctx *in_ctx;
		struct snd_usb_packet_info *out_packet;

		in_ctx = urb->context;

		 
		for (i = 0; i < in_ctx->packets; i++)
			if (urb->iso_frame_desc[i].status == 0)
				bytes += urb->iso_frame_desc[i].actual_length;

		 
		if (bytes == 0)
			return;

		spin_lock_irqsave(&ep->lock, flags);
		if (ep->next_packet_queued >= ARRAY_SIZE(ep->next_packet)) {
			spin_unlock_irqrestore(&ep->lock, flags);
			usb_audio_err(ep->chip,
				      "next package FIFO overflow EP 0x%x\n",
				      ep->ep_num);
			notify_xrun(ep);
			return;
		}

		out_packet = next_packet_fifo_enqueue(ep);

		 

		out_packet->packets = in_ctx->packets;
		for (i = 0; i < in_ctx->packets; i++) {
			if (urb->iso_frame_desc[i].status == 0)
				out_packet->packet_size[i] =
					urb->iso_frame_desc[i].actual_length / sender->stride;
			else
				out_packet->packet_size[i] = 0;
		}

		spin_unlock_irqrestore(&ep->lock, flags);
		snd_usb_queue_pending_output_urbs(ep, false);

		return;
	}

	 

	if (urb->iso_frame_desc[0].status != 0 ||
	    urb->iso_frame_desc[0].actual_length < 3)
		return;

	f = le32_to_cpup(urb->transfer_buffer);
	if (urb->iso_frame_desc[0].actual_length == 3)
		f &= 0x00ffffff;
	else
		f &= 0x0fffffff;

	if (f == 0)
		return;

	if (unlikely(sender->tenor_fb_quirk)) {
		 
		if (f < ep->freqn - 0x8000)
			f += 0xf000;
		else if (f > ep->freqn + 0x8000)
			f -= 0xf000;
	} else if (unlikely(ep->freqshift == INT_MIN)) {
		 
		shift = 0;
		while (f < ep->freqn - ep->freqn / 4) {
			f <<= 1;
			shift++;
		}
		while (f > ep->freqn + ep->freqn / 2) {
			f >>= 1;
			shift--;
		}
		ep->freqshift = shift;
	} else if (ep->freqshift >= 0)
		f <<= ep->freqshift;
	else
		f >>= -ep->freqshift;

	if (likely(f >= ep->freqn - ep->freqn / 8 && f <= ep->freqmax)) {
		 
		spin_lock_irqsave(&ep->lock, flags);
		ep->freqm = f;
		spin_unlock_irqrestore(&ep->lock, flags);
	} else {
		 
		ep->freqshift = INT_MIN;
	}
}

