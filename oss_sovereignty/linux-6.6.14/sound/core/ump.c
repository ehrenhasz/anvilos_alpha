
 

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/mm.h>
#include <sound/core.h>
#include <sound/rawmidi.h>
#include <sound/ump.h>
#include <sound/ump_convert.h>

#define ump_err(ump, fmt, args...)	dev_err((ump)->core.dev, fmt, ##args)
#define ump_warn(ump, fmt, args...)	dev_warn((ump)->core.dev, fmt, ##args)
#define ump_info(ump, fmt, args...)	dev_info((ump)->core.dev, fmt, ##args)
#define ump_dbg(ump, fmt, args...)	dev_dbg((ump)->core.dev, fmt, ##args)

static int snd_ump_dev_register(struct snd_rawmidi *rmidi);
static int snd_ump_dev_unregister(struct snd_rawmidi *rmidi);
static long snd_ump_ioctl(struct snd_rawmidi *rmidi, unsigned int cmd,
			  void __user *argp);
static void snd_ump_proc_read(struct snd_info_entry *entry,
			      struct snd_info_buffer *buffer);
static int snd_ump_rawmidi_open(struct snd_rawmidi_substream *substream);
static int snd_ump_rawmidi_close(struct snd_rawmidi_substream *substream);
static void snd_ump_rawmidi_trigger(struct snd_rawmidi_substream *substream,
				    int up);
static void snd_ump_rawmidi_drain(struct snd_rawmidi_substream *substream);

static void ump_handle_stream_msg(struct snd_ump_endpoint *ump,
				  const u32 *buf, int size);
#if IS_ENABLED(CONFIG_SND_UMP_LEGACY_RAWMIDI)
static int process_legacy_output(struct snd_ump_endpoint *ump,
				 u32 *buffer, int count);
static void process_legacy_input(struct snd_ump_endpoint *ump, const u32 *src,
				 int words);
#else
static inline int process_legacy_output(struct snd_ump_endpoint *ump,
					u32 *buffer, int count)
{
	return 0;
}
static inline void process_legacy_input(struct snd_ump_endpoint *ump,
					const u32 *src, int words)
{
}
#endif

static const struct snd_rawmidi_global_ops snd_ump_rawmidi_ops = {
	.dev_register = snd_ump_dev_register,
	.dev_unregister = snd_ump_dev_unregister,
	.ioctl = snd_ump_ioctl,
	.proc_read = snd_ump_proc_read,
};

static const struct snd_rawmidi_ops snd_ump_rawmidi_input_ops = {
	.open = snd_ump_rawmidi_open,
	.close = snd_ump_rawmidi_close,
	.trigger = snd_ump_rawmidi_trigger,
};

static const struct snd_rawmidi_ops snd_ump_rawmidi_output_ops = {
	.open = snd_ump_rawmidi_open,
	.close = snd_ump_rawmidi_close,
	.trigger = snd_ump_rawmidi_trigger,
	.drain = snd_ump_rawmidi_drain,
};

static void snd_ump_endpoint_free(struct snd_rawmidi *rmidi)
{
	struct snd_ump_endpoint *ump = rawmidi_to_ump(rmidi);
	struct snd_ump_block *fb;

	while (!list_empty(&ump->block_list)) {
		fb = list_first_entry(&ump->block_list, struct snd_ump_block,
				      list);
		list_del(&fb->list);
		if (fb->private_free)
			fb->private_free(fb);
		kfree(fb);
	}

	if (ump->private_free)
		ump->private_free(ump);

#if IS_ENABLED(CONFIG_SND_UMP_LEGACY_RAWMIDI)
	kfree(ump->out_cvts);
#endif
}

 
int snd_ump_endpoint_new(struct snd_card *card, char *id, int device,
			 int output, int input,
			 struct snd_ump_endpoint **ump_ret)
{
	unsigned int info_flags = SNDRV_RAWMIDI_INFO_UMP;
	struct snd_ump_endpoint *ump;
	int err;

	if (input)
		info_flags |= SNDRV_RAWMIDI_INFO_INPUT;
	if (output)
		info_flags |= SNDRV_RAWMIDI_INFO_OUTPUT;
	if (input && output)
		info_flags |= SNDRV_RAWMIDI_INFO_DUPLEX;

	ump = kzalloc(sizeof(*ump), GFP_KERNEL);
	if (!ump)
		return -ENOMEM;
	INIT_LIST_HEAD(&ump->block_list);
	mutex_init(&ump->open_mutex);
	init_waitqueue_head(&ump->stream_wait);
#if IS_ENABLED(CONFIG_SND_UMP_LEGACY_RAWMIDI)
	spin_lock_init(&ump->legacy_locks[0]);
	spin_lock_init(&ump->legacy_locks[1]);
#endif
	err = snd_rawmidi_init(&ump->core, card, id, device,
			       output, input, info_flags);
	if (err < 0) {
		snd_rawmidi_free(&ump->core);
		return err;
	}

	ump->info.card = card->number;
	ump->info.device = device;

	ump->core.private_free = snd_ump_endpoint_free;
	ump->core.ops = &snd_ump_rawmidi_ops;
	if (input)
		snd_rawmidi_set_ops(&ump->core, SNDRV_RAWMIDI_STREAM_INPUT,
				    &snd_ump_rawmidi_input_ops);
	if (output)
		snd_rawmidi_set_ops(&ump->core, SNDRV_RAWMIDI_STREAM_OUTPUT,
				    &snd_ump_rawmidi_output_ops);

	ump_dbg(ump, "Created a UMP EP #%d (%s)\n", device, id);
	*ump_ret = ump;
	return 0;
}
EXPORT_SYMBOL_GPL(snd_ump_endpoint_new);

 

#if IS_ENABLED(CONFIG_SND_SEQUENCER)
static void snd_ump_dev_seq_free(struct snd_seq_device *device)
{
	struct snd_ump_endpoint *ump = device->private_data;

	ump->seq_dev = NULL;
}
#endif

static int snd_ump_dev_register(struct snd_rawmidi *rmidi)
{
#if IS_ENABLED(CONFIG_SND_SEQUENCER)
	struct snd_ump_endpoint *ump = rawmidi_to_ump(rmidi);
	int err;

	err = snd_seq_device_new(ump->core.card, ump->core.device,
				 SNDRV_SEQ_DEV_ID_UMP, 0, &ump->seq_dev);
	if (err < 0)
		return err;
	ump->seq_dev->private_data = ump;
	ump->seq_dev->private_free = snd_ump_dev_seq_free;
	snd_device_register(ump->core.card, ump->seq_dev);
#endif
	return 0;
}

static int snd_ump_dev_unregister(struct snd_rawmidi *rmidi)
{
	return 0;
}

static struct snd_ump_block *
snd_ump_get_block(struct snd_ump_endpoint *ump, unsigned char id)
{
	struct snd_ump_block *fb;

	list_for_each_entry(fb, &ump->block_list, list) {
		if (fb->info.block_id == id)
			return fb;
	}
	return NULL;
}

 
static int snd_ump_rawmidi_open(struct snd_rawmidi_substream *substream)
{
	struct snd_ump_endpoint *ump = rawmidi_to_ump(substream->rmidi);
	int dir = substream->stream;
	int err;

	if (ump->substreams[dir])
		return -EBUSY;
	err = ump->ops->open(ump, dir);
	if (err < 0)
		return err;
	ump->substreams[dir] = substream;
	return 0;
}

static int snd_ump_rawmidi_close(struct snd_rawmidi_substream *substream)
{
	struct snd_ump_endpoint *ump = rawmidi_to_ump(substream->rmidi);
	int dir = substream->stream;

	ump->substreams[dir] = NULL;
	ump->ops->close(ump, dir);
	return 0;
}

static void snd_ump_rawmidi_trigger(struct snd_rawmidi_substream *substream,
				    int up)
{
	struct snd_ump_endpoint *ump = rawmidi_to_ump(substream->rmidi);
	int dir = substream->stream;

	ump->ops->trigger(ump, dir, up);
}

static void snd_ump_rawmidi_drain(struct snd_rawmidi_substream *substream)
{
	struct snd_ump_endpoint *ump = rawmidi_to_ump(substream->rmidi);

	if (ump->ops->drain)
		ump->ops->drain(ump, SNDRV_RAWMIDI_STREAM_OUTPUT);
}

 
static unsigned char ump_packet_words[0x10] = {
	1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4
};

 
int snd_ump_receive_ump_val(struct snd_ump_endpoint *ump, u32 val)
{
	int words;

	if (!ump->input_pending)
		ump->input_pending = ump_packet_words[ump_message_type(val)];

	ump->input_buf[ump->input_buf_head++] = val;
	ump->input_pending--;
	if (!ump->input_pending) {
		words = ump->input_buf_head;
		ump->input_buf_head = 0;
		return words;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(snd_ump_receive_ump_val);

 
int snd_ump_receive(struct snd_ump_endpoint *ump, const u32 *buffer, int count)
{
	struct snd_rawmidi_substream *substream;
	const u32 *p = buffer;
	int n, words = count >> 2;

	while (words--) {
		n = snd_ump_receive_ump_val(ump, *p++);
		if (!n)
			continue;
		ump_handle_stream_msg(ump, ump->input_buf, n);
#if IS_ENABLED(CONFIG_SND_SEQUENCER)
		if (ump->seq_ops)
			ump->seq_ops->input_receive(ump, ump->input_buf, n);
#endif
		process_legacy_input(ump, ump->input_buf, n);
	}

	substream = ump->substreams[SNDRV_RAWMIDI_STREAM_INPUT];
	if (!substream)
		return 0;
	return snd_rawmidi_receive(substream, (const char *)buffer, count);
}
EXPORT_SYMBOL_GPL(snd_ump_receive);

 
int snd_ump_transmit(struct snd_ump_endpoint *ump, u32 *buffer, int count)
{
	struct snd_rawmidi_substream *substream =
		ump->substreams[SNDRV_RAWMIDI_STREAM_OUTPUT];
	int err;

	if (!substream)
		return -ENODEV;
	err = snd_rawmidi_transmit(substream, (char *)buffer, count);
	 
	if (err)
		return err;
	return process_legacy_output(ump, buffer, count);
}
EXPORT_SYMBOL_GPL(snd_ump_transmit);

 
int snd_ump_block_new(struct snd_ump_endpoint *ump, unsigned int blk,
		      unsigned int direction, unsigned int first_group,
		      unsigned int num_groups, struct snd_ump_block **blk_ret)
{
	struct snd_ump_block *fb, *p;

	if (blk < 0 || blk >= SNDRV_UMP_MAX_BLOCKS)
		return -EINVAL;

	if (snd_ump_get_block(ump, blk))
		return -EBUSY;

	fb = kzalloc(sizeof(*fb), GFP_KERNEL);
	if (!fb)
		return -ENOMEM;

	fb->ump = ump;
	fb->info.card = ump->info.card;
	fb->info.device = ump->info.device;
	fb->info.block_id = blk;
	if (blk >= ump->info.num_blocks)
		ump->info.num_blocks = blk + 1;
	fb->info.direction = direction;
	fb->info.active = 1;
	fb->info.first_group = first_group;
	fb->info.num_groups = num_groups;
	 
	snprintf(fb->info.name, sizeof(fb->info.name), "Group %d-%d",
		 first_group + 1, first_group + num_groups);

	 
	list_for_each_entry(p, &ump->block_list, list) {
		if (p->info.block_id > blk) {
			list_add_tail(&fb->list, &p->list);
			goto added;
		}
	}
	list_add_tail(&fb->list, &ump->block_list);

 added:
	ump_dbg(ump, "Created a UMP Block #%d (%s)\n", blk, fb->info.name);
	*blk_ret = fb;
	return 0;
}
EXPORT_SYMBOL_GPL(snd_ump_block_new);

static int snd_ump_ioctl_block(struct snd_ump_endpoint *ump,
			       struct snd_ump_block_info __user *argp)
{
	struct snd_ump_block *fb;
	unsigned char id;

	if (get_user(id, &argp->block_id))
		return -EFAULT;
	fb = snd_ump_get_block(ump, id);
	if (!fb)
		return -ENOENT;
	if (copy_to_user(argp, &fb->info, sizeof(fb->info)))
		return -EFAULT;
	return 0;
}

 
static long snd_ump_ioctl(struct snd_rawmidi *rmidi, unsigned int cmd,
			  void __user *argp)
{
	struct snd_ump_endpoint *ump = rawmidi_to_ump(rmidi);

	switch (cmd) {
	case SNDRV_UMP_IOCTL_ENDPOINT_INFO:
		if (copy_to_user(argp, &ump->info, sizeof(ump->info)))
			return -EFAULT;
		return 0;
	case SNDRV_UMP_IOCTL_BLOCK_INFO:
		return snd_ump_ioctl_block(ump, argp);
	default:
		ump_dbg(ump, "rawmidi: unknown command = 0x%x\n", cmd);
		return -ENOTTY;
	}
}

static const char *ump_direction_string(int dir)
{
	switch (dir) {
	case SNDRV_UMP_DIR_INPUT:
		return "input";
	case SNDRV_UMP_DIR_OUTPUT:
		return "output";
	case SNDRV_UMP_DIR_BIDIRECTION:
		return "bidirection";
	default:
		return "unknown";
	}
}

static const char *ump_ui_hint_string(int dir)
{
	switch (dir) {
	case  SNDRV_UMP_BLOCK_UI_HINT_RECEIVER:
		return "receiver";
	case SNDRV_UMP_BLOCK_UI_HINT_SENDER:
		return "sender";
	case SNDRV_UMP_BLOCK_UI_HINT_BOTH:
		return "both";
	default:
		return "unknown";
	}
}

 
static void snd_ump_proc_read(struct snd_info_entry *entry,
			      struct snd_info_buffer *buffer)
{
	struct snd_rawmidi *rmidi = entry->private_data;
	struct snd_ump_endpoint *ump = rawmidi_to_ump(rmidi);
	struct snd_ump_block *fb;

	snd_iprintf(buffer, "EP Name: %s\n", ump->info.name);
	snd_iprintf(buffer, "EP Product ID: %s\n", ump->info.product_id);
	snd_iprintf(buffer, "UMP Version: 0x%04x\n", ump->info.version);
	snd_iprintf(buffer, "Protocol Caps: 0x%08x\n", ump->info.protocol_caps);
	snd_iprintf(buffer, "Protocol: 0x%08x\n", ump->info.protocol);
	if (ump->info.version) {
		snd_iprintf(buffer, "Manufacturer ID: 0x%08x\n",
			    ump->info.manufacturer_id);
		snd_iprintf(buffer, "Family ID: 0x%04x\n", ump->info.family_id);
		snd_iprintf(buffer, "Model ID: 0x%04x\n", ump->info.model_id);
		snd_iprintf(buffer, "SW Revision: 0x%02x%02x%02x%02x\n",
			    ump->info.sw_revision[0],
			    ump->info.sw_revision[1],
			    ump->info.sw_revision[2],
			    ump->info.sw_revision[3]);
	}
	snd_iprintf(buffer, "Static Blocks: %s\n",
		    (ump->info.flags & SNDRV_UMP_EP_INFO_STATIC_BLOCKS) ? "Yes" : "No");
	snd_iprintf(buffer, "Num Blocks: %d\n\n", ump->info.num_blocks);

	list_for_each_entry(fb, &ump->block_list, list) {
		snd_iprintf(buffer, "Block %d (%s)\n", fb->info.block_id,
			    fb->info.name);
		snd_iprintf(buffer, "  Direction: %s\n",
			    ump_direction_string(fb->info.direction));
		snd_iprintf(buffer, "  Active: %s\n",
			    fb->info.active ? "Yes" : "No");
		snd_iprintf(buffer, "  Groups: %d-%d\n",
			    fb->info.first_group + 1,
			    fb->info.first_group + fb->info.num_groups);
		snd_iprintf(buffer, "  Is MIDI1: %s%s\n",
			    (fb->info.flags & SNDRV_UMP_BLOCK_IS_MIDI1) ? "Yes" : "No",
			    (fb->info.flags & SNDRV_UMP_BLOCK_IS_LOWSPEED) ? " (Low Speed)" : "");
		if (ump->info.version) {
			snd_iprintf(buffer, "  MIDI-CI Version: %d\n",
				    fb->info.midi_ci_version);
			snd_iprintf(buffer, "  Sysex8 Streams: %d\n",
				    fb->info.sysex8_streams);
			snd_iprintf(buffer, "  UI Hint: %s\n",
				    ump_ui_hint_string(fb->info.ui_hint));
		}
		snd_iprintf(buffer, "\n");
	}
}

 

 
static int ump_request_open(struct snd_ump_endpoint *ump)
{
	return snd_rawmidi_kernel_open(&ump->core, 0,
				       SNDRV_RAWMIDI_LFLG_OUTPUT,
				       &ump->stream_rfile);
}

static void ump_request_close(struct snd_ump_endpoint *ump)
{
	snd_rawmidi_kernel_release(&ump->stream_rfile);
}

 
static int ump_req_msg(struct snd_ump_endpoint *ump, u32 req1, u32 req2,
		       u32 reply)
{
	u32 buf[4];

	ump_dbg(ump, "%s: request %08x %08x, wait-for %08x\n",
		__func__, req1, req2, reply);
	memset(buf, 0, sizeof(buf));
	buf[0] = req1;
	buf[1] = req2;
	ump->stream_finished = 0;
	ump->stream_wait_for = reply;
	snd_rawmidi_kernel_write(ump->stream_rfile.output,
				 (unsigned char *)&buf, 16);
	wait_event_timeout(ump->stream_wait, ump->stream_finished,
			   msecs_to_jiffies(500));
	if (!READ_ONCE(ump->stream_finished)) {
		ump_dbg(ump, "%s: request timed out\n", __func__);
		return -ETIMEDOUT;
	}
	ump->stream_finished = 0;
	ump_dbg(ump, "%s: reply: %08x %08x %08x %08x\n",
		__func__, buf[0], buf[1], buf[2], buf[3]);
	return 0;
}

 
static int ump_append_string(struct snd_ump_endpoint *ump, char *dest,
			     int maxsize, const u32 *buf, int offset)
{
	unsigned char format;
	int c;

	format = ump_stream_message_format(buf[0]);
	if (format == UMP_STREAM_MSG_FORMAT_SINGLE ||
	    format == UMP_STREAM_MSG_FORMAT_START) {
		c = 0;
	} else {
		c = strlen(dest);
		if (c >= maxsize - 1)
			return 1;
	}

	for (; offset < 16; offset++) {
		dest[c] = buf[offset / 4] >> (3 - (offset % 4)) * 8;
		if (!dest[c])
			break;
		if (++c >= maxsize - 1)
			break;
	}
	dest[c] = 0;
	return (format == UMP_STREAM_MSG_FORMAT_SINGLE ||
		format == UMP_STREAM_MSG_FORMAT_END);
}

 
static int ump_handle_ep_info_msg(struct snd_ump_endpoint *ump,
				  const union snd_ump_stream_msg *buf)
{
	ump->info.version = (buf->ep_info.ump_version_major << 8) |
		buf->ep_info.ump_version_minor;
	ump->info.num_blocks = buf->ep_info.num_function_blocks;
	if (ump->info.num_blocks > SNDRV_UMP_MAX_BLOCKS) {
		ump_info(ump, "Invalid function blocks %d, fallback to 1\n",
			 ump->info.num_blocks);
		ump->info.num_blocks = 1;
	}

	if (buf->ep_info.static_function_block)
		ump->info.flags |= SNDRV_UMP_EP_INFO_STATIC_BLOCKS;

	ump->info.protocol_caps = (buf->ep_info.protocol << 8) |
		buf->ep_info.jrts;

	ump_dbg(ump, "EP info: version=%x, num_blocks=%x, proto_caps=%x\n",
		ump->info.version, ump->info.num_blocks, ump->info.protocol_caps);
	return 1;  
}

 
static int ump_handle_device_info_msg(struct snd_ump_endpoint *ump,
				      const union snd_ump_stream_msg *buf)
{
	ump->info.manufacturer_id = buf->device_info.manufacture_id & 0x7f7f7f;
	ump->info.family_id = (buf->device_info.family_msb << 8) |
		buf->device_info.family_lsb;
	ump->info.model_id = (buf->device_info.model_msb << 8) |
		buf->device_info.model_lsb;
	ump->info.sw_revision[0] = (buf->device_info.sw_revision >> 24) & 0x7f;
	ump->info.sw_revision[1] = (buf->device_info.sw_revision >> 16) & 0x7f;
	ump->info.sw_revision[2] = (buf->device_info.sw_revision >> 8) & 0x7f;
	ump->info.sw_revision[3] = buf->device_info.sw_revision & 0x7f;
	ump_dbg(ump, "EP devinfo: manid=%08x, family=%04x, model=%04x, sw=%02x%02x%02x%02x\n",
		ump->info.manufacturer_id,
		ump->info.family_id,
		ump->info.model_id,
		ump->info.sw_revision[0],
		ump->info.sw_revision[1],
		ump->info.sw_revision[2],
		ump->info.sw_revision[3]);
	return 1;  
}

 
static int ump_handle_ep_name_msg(struct snd_ump_endpoint *ump,
				  const union snd_ump_stream_msg *buf)
{
	return ump_append_string(ump, ump->info.name, sizeof(ump->info.name),
				 buf->raw, 2);
}

 
static int ump_handle_product_id_msg(struct snd_ump_endpoint *ump,
				     const union snd_ump_stream_msg *buf)
{
	return ump_append_string(ump, ump->info.product_id,
				 sizeof(ump->info.product_id),
				 buf->raw, 2);
}

 
static void seq_notify_protocol(struct snd_ump_endpoint *ump)
{
#if IS_ENABLED(CONFIG_SND_SEQUENCER)
	if (ump->seq_ops && ump->seq_ops->switch_protocol)
		ump->seq_ops->switch_protocol(ump);
#endif  
}

 
int snd_ump_switch_protocol(struct snd_ump_endpoint *ump, unsigned int protocol)
{
	protocol &= ump->info.protocol_caps;
	if (protocol == ump->info.protocol)
		return 0;

	ump->info.protocol = protocol;
	ump_dbg(ump, "New protocol = %x (caps = %x)\n",
		protocol, ump->info.protocol_caps);
	seq_notify_protocol(ump);
	return 1;
}
EXPORT_SYMBOL_GPL(snd_ump_switch_protocol);

 
static int ump_handle_stream_cfg_msg(struct snd_ump_endpoint *ump,
				     const union snd_ump_stream_msg *buf)
{
	unsigned int protocol =
		(buf->stream_cfg.protocol << 8) | buf->stream_cfg.jrts;

	snd_ump_switch_protocol(ump, protocol);
	return 1;  
}

 
static void fill_fb_info(struct snd_ump_endpoint *ump,
			 struct snd_ump_block_info *info,
			 const union snd_ump_stream_msg *buf)
{
	info->direction = buf->fb_info.direction;
	info->ui_hint = buf->fb_info.ui_hint;
	info->first_group = buf->fb_info.first_group;
	info->num_groups = buf->fb_info.num_groups;
	info->flags = buf->fb_info.midi_10;
	info->active = buf->fb_info.active;
	info->midi_ci_version = buf->fb_info.midi_ci_version;
	info->sysex8_streams = buf->fb_info.sysex8_streams;

	ump_dbg(ump, "FB %d: dir=%d, active=%d, first_gp=%d, num_gp=%d, midici=%d, sysex8=%d, flags=0x%x\n",
		info->block_id, info->direction, info->active,
		info->first_group, info->num_groups, info->midi_ci_version,
		info->sysex8_streams, info->flags);
}

 
static bool is_fb_info_updated(struct snd_ump_endpoint *ump,
			       struct snd_ump_block *fb,
			       const union snd_ump_stream_msg *buf)
{
	char tmpbuf[offsetof(struct snd_ump_block_info, name)];

	if (ump->info.flags & SNDRV_UMP_EP_INFO_STATIC_BLOCKS) {
		ump_info(ump, "Skipping static FB info update (blk#%d)\n",
			 fb->info.block_id);
		return 0;
	}

	memcpy(tmpbuf, &fb->info, sizeof(tmpbuf));
	fill_fb_info(ump, (struct snd_ump_block_info *)tmpbuf, buf);
	return memcmp(&fb->info, tmpbuf, sizeof(tmpbuf)) != 0;
}

 
static void seq_notify_fb_change(struct snd_ump_endpoint *ump,
				 struct snd_ump_block *fb)
{
#if IS_ENABLED(CONFIG_SND_SEQUENCER)
	if (ump->seq_ops && ump->seq_ops->notify_fb_change)
		ump->seq_ops->notify_fb_change(ump, fb);
#endif
}

 
static int ump_handle_fb_info_msg(struct snd_ump_endpoint *ump,
				  const union snd_ump_stream_msg *buf)
{
	unsigned char blk;
	struct snd_ump_block *fb;

	blk = buf->fb_info.function_block_id;
	fb = snd_ump_get_block(ump, blk);

	 
	if (!fb && ump->parsed) {
		ump_info(ump, "Function Block Info Update for non-existing block %d\n",
			 blk);
		return -ENODEV;
	}

	 
	if (ump->parsed && !is_fb_info_updated(ump, fb, buf))
		return 1;  

	if (fb) {
		fill_fb_info(ump, &fb->info, buf);
		if (ump->parsed)
			seq_notify_fb_change(ump, fb);
	}

	return 1;  
}

 
static int ump_handle_fb_name_msg(struct snd_ump_endpoint *ump,
				  const union snd_ump_stream_msg *buf)
{
	unsigned char blk;
	struct snd_ump_block *fb;
	int ret;

	blk = buf->fb_name.function_block_id;
	fb = snd_ump_get_block(ump, blk);
	if (!fb)
		return -ENODEV;

	ret = ump_append_string(ump, fb->info.name, sizeof(fb->info.name),
				buf->raw, 3);
	 
	if (ret > 0 && ump->parsed)
		seq_notify_fb_change(ump, fb);
	return ret;
}

static int create_block_from_fb_info(struct snd_ump_endpoint *ump, int blk)
{
	struct snd_ump_block *fb;
	unsigned char direction, first_group, num_groups;
	const union snd_ump_stream_msg *buf =
		(const union snd_ump_stream_msg *)ump->input_buf;
	u32 msg;
	int err;

	 
	msg = ump_stream_compose(UMP_STREAM_MSG_STATUS_FB_DISCOVERY, 0) |
		(blk << 8) | UMP_STREAM_MSG_REQUEST_FB_INFO;
	err = ump_req_msg(ump, msg, 0, UMP_STREAM_MSG_STATUS_FB_INFO);
	if (err < 0) {
		ump_dbg(ump, "Unable to get FB info for block %d\n", blk);
		return err;
	}

	 
	if (buf->fb_info.status != UMP_STREAM_MSG_STATUS_FB_INFO) {
		ump_dbg(ump, "Inconsistent input: 0x%x\n", *buf->raw);
		return -EINVAL;
	}

	direction = buf->fb_info.direction;
	first_group = buf->fb_info.first_group;
	num_groups = buf->fb_info.num_groups;

	err = snd_ump_block_new(ump, blk, direction, first_group, num_groups,
				&fb);
	if (err < 0)
		return err;

	fill_fb_info(ump, &fb->info, buf);

	msg = ump_stream_compose(UMP_STREAM_MSG_STATUS_FB_DISCOVERY, 0) |
		(blk << 8) | UMP_STREAM_MSG_REQUEST_FB_NAME;
	err = ump_req_msg(ump, msg, 0, UMP_STREAM_MSG_STATUS_FB_NAME);
	if (err)
		ump_dbg(ump, "Unable to get UMP FB name string #%d\n", blk);

	return 0;
}

 
static void ump_handle_stream_msg(struct snd_ump_endpoint *ump,
				  const u32 *buf, int size)
{
	const union snd_ump_stream_msg *msg;
	unsigned int status;
	int ret;

	 
	if (ump->no_process_stream)
		return;

	BUILD_BUG_ON(sizeof(*msg) != 16);
	ump_dbg(ump, "Stream msg: %08x %08x %08x %08x\n",
		buf[0], buf[1], buf[2], buf[3]);

	if (size != 4 || ump_message_type(*buf) != UMP_MSG_TYPE_STREAM)
		return;

	msg = (const union snd_ump_stream_msg *)buf;
	status = ump_stream_message_status(*buf);
	switch (status) {
	case UMP_STREAM_MSG_STATUS_EP_INFO:
		ret = ump_handle_ep_info_msg(ump, msg);
		break;
	case UMP_STREAM_MSG_STATUS_DEVICE_INFO:
		ret = ump_handle_device_info_msg(ump, msg);
		break;
	case UMP_STREAM_MSG_STATUS_EP_NAME:
		ret = ump_handle_ep_name_msg(ump, msg);
		break;
	case UMP_STREAM_MSG_STATUS_PRODUCT_ID:
		ret = ump_handle_product_id_msg(ump, msg);
		break;
	case UMP_STREAM_MSG_STATUS_STREAM_CFG:
		ret = ump_handle_stream_cfg_msg(ump, msg);
		break;
	case UMP_STREAM_MSG_STATUS_FB_INFO:
		ret = ump_handle_fb_info_msg(ump, msg);
		break;
	case UMP_STREAM_MSG_STATUS_FB_NAME:
		ret = ump_handle_fb_name_msg(ump, msg);
		break;
	default:
		return;
	}

	 
	if (ret > 0 && ump->stream_wait_for == status) {
		WRITE_ONCE(ump->stream_finished, 1);
		wake_up(&ump->stream_wait);
	}
}

 
int snd_ump_parse_endpoint(struct snd_ump_endpoint *ump)
{
	int blk, err;
	u32 msg;

	if (!(ump->core.info_flags & SNDRV_RAWMIDI_INFO_DUPLEX))
		return -ENODEV;

	err = ump_request_open(ump);
	if (err < 0) {
		ump_dbg(ump, "Unable to open rawmidi device: %d\n", err);
		return err;
	}

	 
	msg = ump_stream_compose(UMP_STREAM_MSG_STATUS_EP_DISCOVERY, 0) |
		0x0101;  
	err = ump_req_msg(ump, msg, UMP_STREAM_MSG_REQUEST_EP_INFO,
			  UMP_STREAM_MSG_STATUS_EP_INFO);
	if (err < 0) {
		ump_dbg(ump, "Unable to get UMP EP info\n");
		goto error;
	}

	 
	err = ump_req_msg(ump, msg, UMP_STREAM_MSG_REQUEST_DEVICE_INFO,
			  UMP_STREAM_MSG_STATUS_DEVICE_INFO);
	if (err < 0)
		ump_dbg(ump, "Unable to get UMP EP device info\n");

	 
	err = ump_req_msg(ump, msg, UMP_STREAM_MSG_REQUEST_EP_NAME,
			  UMP_STREAM_MSG_STATUS_EP_NAME);
	if (err < 0)
		ump_dbg(ump, "Unable to get UMP EP name string\n");

	 
	err = ump_req_msg(ump, msg, UMP_STREAM_MSG_REQUEST_PRODUCT_ID,
			  UMP_STREAM_MSG_STATUS_PRODUCT_ID);
	if (err < 0)
		ump_dbg(ump, "Unable to get UMP EP product ID string\n");

	 
	err = ump_req_msg(ump, msg, UMP_STREAM_MSG_REQUEST_STREAM_CFG,
			  UMP_STREAM_MSG_STATUS_STREAM_CFG);
	if (err < 0)
		ump_dbg(ump, "Unable to get UMP EP stream config\n");

	 
	for (blk = 0; blk < ump->info.num_blocks; blk++) {
		err = create_block_from_fb_info(ump, blk);
		if (err < 0)
			continue;
	}

 error:
	ump->parsed = true;
	ump_request_close(ump);
	if (err == -ETIMEDOUT)
		err = -ENODEV;
	return err;
}
EXPORT_SYMBOL_GPL(snd_ump_parse_endpoint);

#if IS_ENABLED(CONFIG_SND_UMP_LEGACY_RAWMIDI)
 
static int snd_ump_legacy_open(struct snd_rawmidi_substream *substream)
{
	struct snd_ump_endpoint *ump = substream->rmidi->private_data;
	int dir = substream->stream;
	int group = ump->legacy_mapping[substream->number];
	int err;

	mutex_lock(&ump->open_mutex);
	if (ump->legacy_substreams[dir][group]) {
		err = -EBUSY;
		goto unlock;
	}
	if (dir == SNDRV_RAWMIDI_STREAM_OUTPUT) {
		if (!ump->legacy_out_opens) {
			err = snd_rawmidi_kernel_open(&ump->core, 0,
						      SNDRV_RAWMIDI_LFLG_OUTPUT |
						      SNDRV_RAWMIDI_LFLG_APPEND,
						      &ump->legacy_out_rfile);
			if (err < 0)
				goto unlock;
		}
		ump->legacy_out_opens++;
		snd_ump_convert_reset(&ump->out_cvts[group]);
	}
	spin_lock_irq(&ump->legacy_locks[dir]);
	ump->legacy_substreams[dir][group] = substream;
	spin_unlock_irq(&ump->legacy_locks[dir]);
 unlock:
	mutex_unlock(&ump->open_mutex);
	return 0;
}

static int snd_ump_legacy_close(struct snd_rawmidi_substream *substream)
{
	struct snd_ump_endpoint *ump = substream->rmidi->private_data;
	int dir = substream->stream;
	int group = ump->legacy_mapping[substream->number];

	mutex_lock(&ump->open_mutex);
	spin_lock_irq(&ump->legacy_locks[dir]);
	ump->legacy_substreams[dir][group] = NULL;
	spin_unlock_irq(&ump->legacy_locks[dir]);
	if (dir == SNDRV_RAWMIDI_STREAM_OUTPUT) {
		if (!--ump->legacy_out_opens)
			snd_rawmidi_kernel_release(&ump->legacy_out_rfile);
	}
	mutex_unlock(&ump->open_mutex);
	return 0;
}

static void snd_ump_legacy_trigger(struct snd_rawmidi_substream *substream,
				   int up)
{
	struct snd_ump_endpoint *ump = substream->rmidi->private_data;
	int dir = substream->stream;

	ump->ops->trigger(ump, dir, up);
}

static void snd_ump_legacy_drain(struct snd_rawmidi_substream *substream)
{
	struct snd_ump_endpoint *ump = substream->rmidi->private_data;

	if (ump->ops->drain)
		ump->ops->drain(ump, SNDRV_RAWMIDI_STREAM_OUTPUT);
}

static int snd_ump_legacy_dev_register(struct snd_rawmidi *rmidi)
{
	 
	return 0;
}

static const struct snd_rawmidi_ops snd_ump_legacy_input_ops = {
	.open = snd_ump_legacy_open,
	.close = snd_ump_legacy_close,
	.trigger = snd_ump_legacy_trigger,
};

static const struct snd_rawmidi_ops snd_ump_legacy_output_ops = {
	.open = snd_ump_legacy_open,
	.close = snd_ump_legacy_close,
	.trigger = snd_ump_legacy_trigger,
	.drain = snd_ump_legacy_drain,
};

static const struct snd_rawmidi_global_ops snd_ump_legacy_ops = {
	.dev_register = snd_ump_legacy_dev_register,
};

static int process_legacy_output(struct snd_ump_endpoint *ump,
				 u32 *buffer, int count)
{
	struct snd_rawmidi_substream *substream;
	struct ump_cvt_to_ump *ctx;
	const int dir = SNDRV_RAWMIDI_STREAM_OUTPUT;
	unsigned char c;
	int group, size = 0;
	unsigned long flags;

	if (!ump->out_cvts || !ump->legacy_out_opens)
		return 0;

	spin_lock_irqsave(&ump->legacy_locks[dir], flags);
	for (group = 0; group < SNDRV_UMP_MAX_GROUPS; group++) {
		substream = ump->legacy_substreams[dir][group];
		if (!substream)
			continue;
		ctx = &ump->out_cvts[group];
		while (!ctx->ump_bytes &&
		       snd_rawmidi_transmit(substream, &c, 1) > 0)
			snd_ump_convert_to_ump(ctx, group, ump->info.protocol, c);
		if (ctx->ump_bytes && ctx->ump_bytes <= count) {
			size = ctx->ump_bytes;
			memcpy(buffer, ctx->ump, size);
			ctx->ump_bytes = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&ump->legacy_locks[dir], flags);
	return size;
}

static void process_legacy_input(struct snd_ump_endpoint *ump, const u32 *src,
				 int words)
{
	struct snd_rawmidi_substream *substream;
	unsigned char buf[16];
	unsigned char group;
	unsigned long flags;
	const int dir = SNDRV_RAWMIDI_STREAM_INPUT;
	int size;

	size = snd_ump_convert_from_ump(src, buf, &group);
	if (size <= 0)
		return;
	spin_lock_irqsave(&ump->legacy_locks[dir], flags);
	substream = ump->legacy_substreams[dir][group];
	if (substream)
		snd_rawmidi_receive(substream, buf, size);
	spin_unlock_irqrestore(&ump->legacy_locks[dir], flags);
}

 
static int fill_legacy_mapping(struct snd_ump_endpoint *ump)
{
	struct snd_ump_block *fb;
	unsigned int group_maps = 0;
	int i, num;

	if (ump->info.flags & SNDRV_UMP_EP_INFO_STATIC_BLOCKS) {
		list_for_each_entry(fb, &ump->block_list, list) {
			for (i = 0; i < fb->info.num_groups; i++)
				group_maps |= 1U << (fb->info.first_group + i);
		}
		if (!group_maps)
			ump_info(ump, "No UMP Group is found in FB\n");
	}

	 
	if (!group_maps)
		group_maps = (1U << SNDRV_UMP_MAX_GROUPS) - 1;

	num = 0;
	for (i = 0; i < SNDRV_UMP_MAX_GROUPS; i++)
		if (group_maps & (1U << i))
			ump->legacy_mapping[num++] = i;

	return num;
}

static void fill_substream_names(struct snd_ump_endpoint *ump,
				 struct snd_rawmidi *rmidi, int dir)
{
	struct snd_rawmidi_substream *s;

	list_for_each_entry(s, &rmidi->streams[dir].substreams, list)
		snprintf(s->name, sizeof(s->name), "Group %d (%.16s)",
			 ump->legacy_mapping[s->number] + 1, ump->info.name);
}

int snd_ump_attach_legacy_rawmidi(struct snd_ump_endpoint *ump,
				  char *id, int device)
{
	struct snd_rawmidi *rmidi;
	bool input, output;
	int err, num;

	ump->out_cvts = kcalloc(SNDRV_UMP_MAX_GROUPS,
				sizeof(*ump->out_cvts), GFP_KERNEL);
	if (!ump->out_cvts)
		return -ENOMEM;

	num = fill_legacy_mapping(ump);

	input = ump->core.info_flags & SNDRV_RAWMIDI_INFO_INPUT;
	output = ump->core.info_flags & SNDRV_RAWMIDI_INFO_OUTPUT;
	err = snd_rawmidi_new(ump->core.card, id, device,
			      output ? num : 0, input ? num : 0,
			      &rmidi);
	if (err < 0) {
		kfree(ump->out_cvts);
		return err;
	}

	if (input)
		snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_INPUT,
				    &snd_ump_legacy_input_ops);
	if (output)
		snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_OUTPUT,
				    &snd_ump_legacy_output_ops);
	snprintf(rmidi->name, sizeof(rmidi->name), "%.68s (MIDI 1.0)",
		 ump->info.name);
	rmidi->info_flags = ump->core.info_flags & ~SNDRV_RAWMIDI_INFO_UMP;
	rmidi->ops = &snd_ump_legacy_ops;
	rmidi->private_data = ump;
	ump->legacy_rmidi = rmidi;
	if (input)
		fill_substream_names(ump, rmidi, SNDRV_RAWMIDI_STREAM_INPUT);
	if (output)
		fill_substream_names(ump, rmidi, SNDRV_RAWMIDI_STREAM_OUTPUT);

	ump_dbg(ump, "Created a legacy rawmidi #%d (%s)\n", device, id);
	return 0;
}
EXPORT_SYMBOL_GPL(snd_ump_attach_legacy_rawmidi);
#endif  

MODULE_DESCRIPTION("Universal MIDI Packet (UMP) Core Driver");
MODULE_LICENSE("GPL");
