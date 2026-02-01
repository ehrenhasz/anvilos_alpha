
 

#include <linux/slab.h>

#include "amdtp-am824.h"

#define CIP_FMT_AM		0x10

 
#define AMDTP_FDF_AM824		0x00

 
#define MIDI_BYTES_PER_SECOND	3093

 
#define MAX_MIDI_RX_BLOCKS	8

struct amdtp_am824 {
	struct snd_rawmidi_substream *midi[AM824_MAX_CHANNELS_FOR_MIDI * 8];
	int midi_fifo_limit;
	int midi_fifo_used[AM824_MAX_CHANNELS_FOR_MIDI * 8];
	unsigned int pcm_channels;
	unsigned int midi_ports;

	u8 pcm_positions[AM824_MAX_CHANNELS_FOR_PCM];
	u8 midi_position;
};

 
int amdtp_am824_set_parameters(struct amdtp_stream *s, unsigned int rate,
			       unsigned int pcm_channels,
			       unsigned int midi_ports,
			       bool double_pcm_frames)
{
	struct amdtp_am824 *p = s->protocol;
	unsigned int midi_channels;
	unsigned int pcm_frame_multiplier;
	int i, err;

	if (amdtp_stream_running(s))
		return -EINVAL;

	if (pcm_channels > AM824_MAX_CHANNELS_FOR_PCM)
		return -EINVAL;

	midi_channels = DIV_ROUND_UP(midi_ports, 8);
	if (midi_channels > AM824_MAX_CHANNELS_FOR_MIDI)
		return -EINVAL;

	if (WARN_ON(amdtp_stream_running(s)) ||
	    WARN_ON(pcm_channels > AM824_MAX_CHANNELS_FOR_PCM) ||
	    WARN_ON(midi_channels > AM824_MAX_CHANNELS_FOR_MIDI))
		return -EINVAL;

	 
	if (double_pcm_frames)
		pcm_frame_multiplier = 2;
	else
		pcm_frame_multiplier = 1;

	err = amdtp_stream_set_parameters(s, rate, pcm_channels + midi_channels,
					  pcm_frame_multiplier);
	if (err < 0)
		return err;

	if (s->direction == AMDTP_OUT_STREAM)
		s->ctx_data.rx.fdf = AMDTP_FDF_AM824 | s->sfc;

	p->pcm_channels = pcm_channels;
	p->midi_ports = midi_ports;

	 
	for (i = 0; i < pcm_channels; i++)
		p->pcm_positions[i] = i;
	p->midi_position = p->pcm_channels;

	 
	p->midi_fifo_limit = rate - MIDI_BYTES_PER_SECOND * s->syt_interval + 1;

	return 0;
}
EXPORT_SYMBOL_GPL(amdtp_am824_set_parameters);

 
void amdtp_am824_set_pcm_position(struct amdtp_stream *s, unsigned int index,
				 unsigned int position)
{
	struct amdtp_am824 *p = s->protocol;

	if (index < p->pcm_channels)
		p->pcm_positions[index] = position;
}
EXPORT_SYMBOL_GPL(amdtp_am824_set_pcm_position);

 
void amdtp_am824_set_midi_position(struct amdtp_stream *s,
				   unsigned int position)
{
	struct amdtp_am824 *p = s->protocol;

	p->midi_position = position;
}
EXPORT_SYMBOL_GPL(amdtp_am824_set_midi_position);

static void write_pcm_s32(struct amdtp_stream *s, struct snd_pcm_substream *pcm,
			  __be32 *buffer, unsigned int frames,
			  unsigned int pcm_frames)
{
	struct amdtp_am824 *p = s->protocol;
	unsigned int channels = p->pcm_channels;
	struct snd_pcm_runtime *runtime = pcm->runtime;
	unsigned int pcm_buffer_pointer;
	int remaining_frames;
	const u32 *src;
	int i, c;

	pcm_buffer_pointer = s->pcm_buffer_pointer + pcm_frames;
	pcm_buffer_pointer %= runtime->buffer_size;

	src = (void *)runtime->dma_area +
				frames_to_bytes(runtime, pcm_buffer_pointer);
	remaining_frames = runtime->buffer_size - pcm_buffer_pointer;

	for (i = 0; i < frames; ++i) {
		for (c = 0; c < channels; ++c) {
			buffer[p->pcm_positions[c]] =
					cpu_to_be32((*src >> 8) | 0x40000000);
			src++;
		}
		buffer += s->data_block_quadlets;
		if (--remaining_frames == 0)
			src = (void *)runtime->dma_area;
	}
}

static void read_pcm_s32(struct amdtp_stream *s, struct snd_pcm_substream *pcm,
			 __be32 *buffer, unsigned int frames,
			 unsigned int pcm_frames)
{
	struct amdtp_am824 *p = s->protocol;
	unsigned int channels = p->pcm_channels;
	struct snd_pcm_runtime *runtime = pcm->runtime;
	unsigned int pcm_buffer_pointer;
	int remaining_frames;
	u32 *dst;
	int i, c;

	pcm_buffer_pointer = s->pcm_buffer_pointer + pcm_frames;
	pcm_buffer_pointer %= runtime->buffer_size;

	dst  = (void *)runtime->dma_area +
				frames_to_bytes(runtime, pcm_buffer_pointer);
	remaining_frames = runtime->buffer_size - pcm_buffer_pointer;

	for (i = 0; i < frames; ++i) {
		for (c = 0; c < channels; ++c) {
			*dst = be32_to_cpu(buffer[p->pcm_positions[c]]) << 8;
			dst++;
		}
		buffer += s->data_block_quadlets;
		if (--remaining_frames == 0)
			dst = (void *)runtime->dma_area;
	}
}

static void write_pcm_silence(struct amdtp_stream *s,
			      __be32 *buffer, unsigned int frames)
{
	struct amdtp_am824 *p = s->protocol;
	unsigned int i, c, channels = p->pcm_channels;

	for (i = 0; i < frames; ++i) {
		for (c = 0; c < channels; ++c)
			buffer[p->pcm_positions[c]] = cpu_to_be32(0x40000000);
		buffer += s->data_block_quadlets;
	}
}

 
int amdtp_am824_add_pcm_hw_constraints(struct amdtp_stream *s,
				       struct snd_pcm_runtime *runtime)
{
	int err;

	err = amdtp_stream_add_pcm_hw_constraints(s, runtime);
	if (err < 0)
		return err;

	 
	return snd_pcm_hw_constraint_msbits(runtime, 0, 32, 24);
}
EXPORT_SYMBOL_GPL(amdtp_am824_add_pcm_hw_constraints);

 
void amdtp_am824_midi_trigger(struct amdtp_stream *s, unsigned int port,
			      struct snd_rawmidi_substream *midi)
{
	struct amdtp_am824 *p = s->protocol;

	if (port < p->midi_ports)
		WRITE_ONCE(p->midi[port], midi);
}
EXPORT_SYMBOL_GPL(amdtp_am824_midi_trigger);

 
static bool midi_ratelimit_per_packet(struct amdtp_stream *s, unsigned int port)
{
	struct amdtp_am824 *p = s->protocol;
	int used;

	used = p->midi_fifo_used[port];
	if (used == 0)  
		return true;

	used -= MIDI_BYTES_PER_SECOND * s->syt_interval;
	used = max(used, 0);
	p->midi_fifo_used[port] = used;

	return used < p->midi_fifo_limit;
}

static void midi_rate_use_one_byte(struct amdtp_stream *s, unsigned int port)
{
	struct amdtp_am824 *p = s->protocol;

	p->midi_fifo_used[port] += amdtp_rate_table[s->sfc];
}

static void write_midi_messages(struct amdtp_stream *s, __be32 *buffer,
			unsigned int frames, unsigned int data_block_counter)
{
	struct amdtp_am824 *p = s->protocol;
	unsigned int f, port;
	u8 *b;

	for (f = 0; f < frames; f++) {
		b = (u8 *)&buffer[p->midi_position];

		port = (data_block_counter + f) % 8;
		if (f < MAX_MIDI_RX_BLOCKS &&
		    midi_ratelimit_per_packet(s, port) &&
		    p->midi[port] != NULL &&
		    snd_rawmidi_transmit(p->midi[port], &b[1], 1) == 1) {
			midi_rate_use_one_byte(s, port);
			b[0] = 0x81;
		} else {
			b[0] = 0x80;
			b[1] = 0;
		}
		b[2] = 0;
		b[3] = 0;

		buffer += s->data_block_quadlets;
	}
}

static void read_midi_messages(struct amdtp_stream *s, __be32 *buffer,
			unsigned int frames, unsigned int data_block_counter)
{
	struct amdtp_am824 *p = s->protocol;
	int len;
	u8 *b;
	int f;

	for (f = 0; f < frames; f++) {
		unsigned int port = f;

		if (!(s->flags & CIP_UNALIGHED_DBC))
			port += data_block_counter;
		port %= 8;
		b = (u8 *)&buffer[p->midi_position];

		len = b[0] - 0x80;
		if ((1 <= len) &&  (len <= 3) && (p->midi[port]))
			snd_rawmidi_receive(p->midi[port], b + 1, len);

		buffer += s->data_block_quadlets;
	}
}

static void process_it_ctx_payloads(struct amdtp_stream *s, const struct pkt_desc *desc,
				    unsigned int count, struct snd_pcm_substream *pcm)
{
	struct amdtp_am824 *p = s->protocol;
	unsigned int pcm_frames = 0;
	int i;

	for (i = 0; i < count; ++i) {
		__be32 *buf = desc->ctx_payload;
		unsigned int data_blocks = desc->data_blocks;

		if (pcm) {
			write_pcm_s32(s, pcm, buf, data_blocks, pcm_frames);
			pcm_frames += data_blocks * s->pcm_frame_multiplier;
		} else {
			write_pcm_silence(s, buf, data_blocks);
		}

		if (p->midi_ports) {
			write_midi_messages(s, buf, data_blocks,
					    desc->data_block_counter);
		}

		desc = amdtp_stream_next_packet_desc(s, desc);
	}
}

static void process_ir_ctx_payloads(struct amdtp_stream *s, const struct pkt_desc *desc,
				    unsigned int count, struct snd_pcm_substream *pcm)
{
	struct amdtp_am824 *p = s->protocol;
	unsigned int pcm_frames = 0;
	int i;

	for (i = 0; i < count; ++i) {
		__be32 *buf = desc->ctx_payload;
		unsigned int data_blocks = desc->data_blocks;

		if (pcm) {
			read_pcm_s32(s, pcm, buf, data_blocks, pcm_frames);
			pcm_frames += data_blocks * s->pcm_frame_multiplier;
		}

		if (p->midi_ports) {
			read_midi_messages(s, buf, data_blocks,
					   desc->data_block_counter);
		}

		desc = amdtp_stream_next_packet_desc(s, desc);
	}
}

 
int amdtp_am824_init(struct amdtp_stream *s, struct fw_unit *unit,
		     enum amdtp_stream_direction dir, unsigned int flags)
{
	amdtp_stream_process_ctx_payloads_t process_ctx_payloads;

	if (dir == AMDTP_IN_STREAM)
		process_ctx_payloads = process_ir_ctx_payloads;
	else
		process_ctx_payloads = process_it_ctx_payloads;

	return amdtp_stream_init(s, unit, dir, flags, CIP_FMT_AM,
			process_ctx_payloads, sizeof(struct amdtp_am824));
}
EXPORT_SYMBOL_GPL(amdtp_am824_init);
