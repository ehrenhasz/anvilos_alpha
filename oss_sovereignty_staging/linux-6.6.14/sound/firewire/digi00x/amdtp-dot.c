
 

#include <sound/pcm.h>
#include "digi00x.h"

#define CIP_FMT_AM		0x10

 
#define AMDTP_FDF_AM824		0x00

 
#define MIDI_BYTES_PER_SECOND	3093

 
#define MAX_MIDI_RX_BLOCKS	8

 
#define MAX_MIDI_PORTS		3

 
struct dot_state {
	u8 carry;
	u8 idx;
	unsigned int off;
};

struct amdtp_dot {
	unsigned int pcm_channels;
	struct dot_state state;

	struct snd_rawmidi_substream *midi[MAX_MIDI_PORTS];
	int midi_fifo_used[MAX_MIDI_PORTS];
	int midi_fifo_limit;
};

 
#define BYTE_PER_SAMPLE (4)
#define MAGIC_DOT_BYTE (2)
#define MAGIC_BYTE_OFF(x) (((x) * BYTE_PER_SAMPLE) + MAGIC_DOT_BYTE)
static u8 dot_scrt(const u8 idx, const unsigned int off)
{
	 
	static const u8 len[16] = {0, 1, 3, 5, 7, 9, 11, 13, 14,
				   12, 10, 8, 6, 4, 2, 0};

	 
	static const u8 nib[15] = {0x8, 0x7, 0x9, 0x6, 0xa, 0x5, 0xb, 0x4,
				   0xc, 0x3, 0xd, 0x2, 0xe, 0x1, 0xf};

	 
	static const u8 hir[15] = {0x0, 0x6, 0xf, 0x8, 0x7, 0x5, 0x3, 0x4,
				   0xc, 0xd, 0xe, 0x1, 0x2, 0xb, 0xa};

	 
	static const u8 hio[16] = {0, 11, 12, 6, 7, 5, 1, 4,
				   3, 0x00, 14, 13, 8, 9, 10, 2};

	const u8 ln = idx & 0xf;
	const u8 hn = (idx >> 4) & 0xf;
	const u8 hr = (hn == 0x9) ? 0x9 : hir[(hio[hn] + off) % 15];

	if (len[ln] < off)
		return 0x00;

	return ((nib[14 + off - len[ln]]) | (hr << 4));
}

static void dot_encode_step(struct dot_state *state, __be32 *const buffer)
{
	u8 * const data = (u8 *) buffer;

	if (data[MAGIC_DOT_BYTE] != 0x00) {
		state->off = 0;
		state->idx = data[MAGIC_DOT_BYTE] ^ state->carry;
	}
	data[MAGIC_DOT_BYTE] ^= state->carry;
	state->carry = dot_scrt(state->idx, ++(state->off));
}

int amdtp_dot_set_parameters(struct amdtp_stream *s, unsigned int rate,
			     unsigned int pcm_channels)
{
	struct amdtp_dot *p = s->protocol;
	int err;

	if (amdtp_stream_running(s))
		return -EBUSY;

	 
	err = amdtp_stream_set_parameters(s, rate, pcm_channels + 1, 1);
	if (err < 0)
		return err;

	s->ctx_data.rx.fdf = AMDTP_FDF_AM824 | s->sfc;

	p->pcm_channels = pcm_channels;

	 
	p->midi_fifo_limit = rate - MIDI_BYTES_PER_SECOND * s->syt_interval + 1;

	return 0;
}

static void write_pcm_s32(struct amdtp_stream *s, struct snd_pcm_substream *pcm,
			  __be32 *buffer, unsigned int frames,
			  unsigned int pcm_frames)
{
	struct amdtp_dot *p = s->protocol;
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

	buffer++;
	for (i = 0; i < frames; ++i) {
		for (c = 0; c < channels; ++c) {
			buffer[c] = cpu_to_be32((*src >> 8) | 0x40000000);
			dot_encode_step(&p->state, &buffer[c]);
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
	struct amdtp_dot *p = s->protocol;
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

	buffer++;
	for (i = 0; i < frames; ++i) {
		for (c = 0; c < channels; ++c) {
			*dst = be32_to_cpu(buffer[c]) << 8;
			dst++;
		}
		buffer += s->data_block_quadlets;
		if (--remaining_frames == 0)
			dst = (void *)runtime->dma_area;
	}
}

static void write_pcm_silence(struct amdtp_stream *s, __be32 *buffer,
			      unsigned int data_blocks)
{
	struct amdtp_dot *p = s->protocol;
	unsigned int channels, i, c;

	channels = p->pcm_channels;

	buffer++;
	for (i = 0; i < data_blocks; ++i) {
		for (c = 0; c < channels; ++c)
			buffer[c] = cpu_to_be32(0x40000000);
		buffer += s->data_block_quadlets;
	}
}

static bool midi_ratelimit_per_packet(struct amdtp_stream *s, unsigned int port)
{
	struct amdtp_dot *p = s->protocol;
	int used;

	used = p->midi_fifo_used[port];
	if (used == 0)
		return true;

	used -= MIDI_BYTES_PER_SECOND * s->syt_interval;
	used = max(used, 0);
	p->midi_fifo_used[port] = used;

	return used < p->midi_fifo_limit;
}

static inline void midi_use_bytes(struct amdtp_stream *s,
				  unsigned int port, unsigned int count)
{
	struct amdtp_dot *p = s->protocol;

	p->midi_fifo_used[port] += amdtp_rate_table[s->sfc] * count;
}

static void write_midi_messages(struct amdtp_stream *s, __be32 *buffer,
		unsigned int data_blocks, unsigned int data_block_counter)
{
	struct amdtp_dot *p = s->protocol;
	unsigned int f, port;
	int len;
	u8 *b;

	for (f = 0; f < data_blocks; f++) {
		port = (data_block_counter + f) % 8;
		b = (u8 *)&buffer[0];

		len = 0;
		if (port < MAX_MIDI_PORTS &&
		    midi_ratelimit_per_packet(s, port) &&
		    p->midi[port] != NULL)
			len = snd_rawmidi_transmit(p->midi[port], b + 1, 2);

		if (len > 0) {
			 
			if (port == 2)
				b[3] = 0xe0;
			else if (port == 1)
				b[3] = 0x20;
			else
				b[3] = 0x00;
			b[3] |= len;
			midi_use_bytes(s, port, len);
		} else {
			b[1] = 0;
			b[2] = 0;
			b[3] = 0;
		}
		b[0] = 0x80;

		buffer += s->data_block_quadlets;
	}
}

static void read_midi_messages(struct amdtp_stream *s, __be32 *buffer,
			       unsigned int data_blocks)
{
	struct amdtp_dot *p = s->protocol;
	unsigned int f, port, len;
	u8 *b;

	for (f = 0; f < data_blocks; f++) {
		b = (u8 *)&buffer[0];

		len = b[3] & 0x0f;
		if (len > 0) {
			 
			if (b[3] >> 4 > 0)
				port = 2;
			else
				port = 0;

			if (port < MAX_MIDI_PORTS && p->midi[port])
				snd_rawmidi_receive(p->midi[port], b + 1, len);
		}

		buffer += s->data_block_quadlets;
	}
}

int amdtp_dot_add_pcm_hw_constraints(struct amdtp_stream *s,
				     struct snd_pcm_runtime *runtime)
{
	int err;

	 
	err = snd_pcm_hw_constraint_msbits(runtime, 0, 32, 24);
	if (err < 0)
		return err;

	return amdtp_stream_add_pcm_hw_constraints(s, runtime);
}

void amdtp_dot_midi_trigger(struct amdtp_stream *s, unsigned int port,
			  struct snd_rawmidi_substream *midi)
{
	struct amdtp_dot *p = s->protocol;

	if (port < MAX_MIDI_PORTS)
		WRITE_ONCE(p->midi[port], midi);
}

static void process_ir_ctx_payloads(struct amdtp_stream *s, const struct pkt_desc *desc,
				    unsigned int count, struct snd_pcm_substream *pcm)
{
	unsigned int pcm_frames = 0;
	int i;

	for (i = 0; i < count; ++i) {
		__be32 *buf = desc->ctx_payload;
		unsigned int data_blocks = desc->data_blocks;

		if (pcm) {
			read_pcm_s32(s, pcm, buf, data_blocks, pcm_frames);
			pcm_frames += data_blocks;
		}

		read_midi_messages(s, buf, data_blocks);

		desc = amdtp_stream_next_packet_desc(s, desc);
	}
}

static void process_it_ctx_payloads(struct amdtp_stream *s, const struct pkt_desc *desc,
				    unsigned int count, struct snd_pcm_substream *pcm)
{
	unsigned int pcm_frames = 0;
	int i;

	for (i = 0; i < count; ++i) {
		__be32 *buf = desc->ctx_payload;
		unsigned int data_blocks = desc->data_blocks;

		if (pcm) {
			write_pcm_s32(s, pcm, buf, data_blocks, pcm_frames);
			pcm_frames += data_blocks;
		} else {
			write_pcm_silence(s, buf, data_blocks);
		}

		write_midi_messages(s, buf, data_blocks,
				    desc->data_block_counter);

		desc = amdtp_stream_next_packet_desc(s, desc);
	}
}

int amdtp_dot_init(struct amdtp_stream *s, struct fw_unit *unit,
		 enum amdtp_stream_direction dir)
{
	amdtp_stream_process_ctx_payloads_t process_ctx_payloads;
	unsigned int flags = CIP_NONBLOCKING | CIP_UNAWARE_SYT;

	
	if (dir == AMDTP_IN_STREAM)
		process_ctx_payloads = process_ir_ctx_payloads;
	else
		process_ctx_payloads = process_it_ctx_payloads;

	return amdtp_stream_init(s, unit, dir, flags, CIP_FMT_AM,
				process_ctx_payloads, sizeof(struct amdtp_dot));
}

void amdtp_dot_reset(struct amdtp_stream *s)
{
	struct amdtp_dot *p = s->protocol;

	p->state.carry = 0x00;
	p->state.idx = 0x00;
	p->state.off = 0;
}
