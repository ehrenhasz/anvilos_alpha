#ifndef SOUND_FIREWIRE_AMDTP_H_INCLUDED
#define SOUND_FIREWIRE_AMDTP_H_INCLUDED
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <sound/asound.h>
#include "packets-buffer.h"
enum cip_flags {
	CIP_NONBLOCKING		= 0x00,
	CIP_BLOCKING		= 0x01,
	CIP_EMPTY_WITH_TAG0	= 0x02,
	CIP_DBC_IS_END_EVENT	= 0x04,
	CIP_WRONG_DBS		= 0x08,
	CIP_SKIP_DBC_ZERO_CHECK	= 0x10,
	CIP_EMPTY_HAS_WRONG_DBC	= 0x20,
	CIP_JUMBO_PAYLOAD	= 0x40,
	CIP_HEADER_WITHOUT_EOH	= 0x80,
	CIP_NO_HEADER		= 0x100,
	CIP_UNALIGHED_DBC	= 0x200,
	CIP_UNAWARE_SYT		= 0x400,
};
enum cip_sfc {
	CIP_SFC_32000  = 0,
	CIP_SFC_44100  = 1,
	CIP_SFC_48000  = 2,
	CIP_SFC_88200  = 3,
	CIP_SFC_96000  = 4,
	CIP_SFC_176400 = 5,
	CIP_SFC_192000 = 6,
	CIP_SFC_COUNT
};
struct fw_unit;
struct fw_iso_context;
struct snd_pcm_substream;
struct snd_pcm_runtime;
enum amdtp_stream_direction {
	AMDTP_OUT_STREAM = 0,
	AMDTP_IN_STREAM
};
struct pkt_desc {
	u32 cycle;
	u32 syt;
	unsigned int data_blocks;
	unsigned int data_block_counter;
	__be32 *ctx_payload;
	struct list_head link;
};
struct amdtp_stream;
typedef void (*amdtp_stream_process_ctx_payloads_t)(struct amdtp_stream *s,
						    const struct pkt_desc *desc,
						    unsigned int count,
						    struct snd_pcm_substream *pcm);
struct amdtp_domain;
struct amdtp_stream {
	struct fw_unit *unit;
	unsigned int flags;
	enum amdtp_stream_direction direction;
	struct mutex mutex;
	struct fw_iso_context *context;
	struct iso_packets_buffer buffer;
	unsigned int queue_size;
	int packet_index;
	struct pkt_desc *packet_descs;
	struct list_head packet_descs_list;
	struct pkt_desc *packet_descs_cursor;
	int tag;
	union {
		struct {
			unsigned int ctx_header_size;
			unsigned int max_ctx_payload_length;
			unsigned int dbc_interval;
			bool event_starts;
			struct {
				struct seq_desc *descs;
				unsigned int size;
				unsigned int pos;
			} cache;
		} tx;
		struct {
			unsigned int fdf;
			unsigned int event_count;
			struct {
				struct seq_desc *descs;
				unsigned int size;
				unsigned int pos;
			} seq;
			unsigned int data_block_state;
			unsigned int syt_offset_state;
			unsigned int last_syt_offset;
			struct amdtp_stream *replay_target;
			unsigned int cache_pos;
		} rx;
	} ctx_data;
	unsigned int source_node_id_field;
	unsigned int data_block_quadlets;
	unsigned int data_block_counter;
	unsigned int sph;
	unsigned int fmt;
	unsigned int transfer_delay;
	enum cip_sfc sfc;
	unsigned int syt_interval;
	struct snd_pcm_substream *pcm;
	snd_pcm_uframes_t pcm_buffer_pointer;
	unsigned int pcm_period_pointer;
	unsigned int pcm_frame_multiplier;
	bool ready_processing;
	wait_queue_head_t ready_wait;
	unsigned int next_cycle;
	void *protocol;
	amdtp_stream_process_ctx_payloads_t process_ctx_payloads;
	int channel;
	int speed;
	struct list_head list;
	struct amdtp_domain *domain;
};
int amdtp_stream_init(struct amdtp_stream *s, struct fw_unit *unit,
		      enum amdtp_stream_direction dir, unsigned int flags,
		      unsigned int fmt,
		      amdtp_stream_process_ctx_payloads_t process_ctx_payloads,
		      unsigned int protocol_size);
void amdtp_stream_destroy(struct amdtp_stream *s);
int amdtp_stream_set_parameters(struct amdtp_stream *s, unsigned int rate,
				unsigned int data_block_quadlets, unsigned int pcm_frame_multiplier);
unsigned int amdtp_stream_get_max_payload(struct amdtp_stream *s);
void amdtp_stream_update(struct amdtp_stream *s);
int amdtp_stream_add_pcm_hw_constraints(struct amdtp_stream *s,
					struct snd_pcm_runtime *runtime);
void amdtp_stream_pcm_prepare(struct amdtp_stream *s);
void amdtp_stream_pcm_abort(struct amdtp_stream *s);
extern const unsigned int amdtp_syt_intervals[CIP_SFC_COUNT];
extern const unsigned int amdtp_rate_table[CIP_SFC_COUNT];
static inline bool amdtp_stream_running(struct amdtp_stream *s)
{
	return !IS_ERR(s->context);
}
static inline bool amdtp_streaming_error(struct amdtp_stream *s)
{
	return s->packet_index < 0;
}
static inline bool amdtp_stream_pcm_running(struct amdtp_stream *s)
{
	return !!s->pcm;
}
static inline void amdtp_stream_pcm_trigger(struct amdtp_stream *s,
					    struct snd_pcm_substream *pcm)
{
	WRITE_ONCE(s->pcm, pcm);
}
#define amdtp_stream_next_packet_desc(s, desc) \
	list_next_entry_circular(desc, &s->packet_descs_list, link)
static inline bool cip_sfc_is_base_44100(enum cip_sfc sfc)
{
	return sfc & 1;
}
struct seq_desc {
	unsigned int syt_offset;
	unsigned int data_blocks;
};
struct amdtp_domain {
	struct list_head streams;
	unsigned int events_per_period;
	unsigned int events_per_buffer;
	struct amdtp_stream *irq_target;
	struct {
		unsigned int tx_init_skip;
		unsigned int tx_start;
		unsigned int rx_start;
	} processing_cycle;
	struct {
		bool enable:1;
		bool on_the_fly:1;
	} replay;
};
int amdtp_domain_init(struct amdtp_domain *d);
void amdtp_domain_destroy(struct amdtp_domain *d);
int amdtp_domain_add_stream(struct amdtp_domain *d, struct amdtp_stream *s,
			    int channel, int speed);
int amdtp_domain_start(struct amdtp_domain *d, unsigned int tx_init_skip_cycles, bool replay_seq,
		       bool replay_on_the_fly);
void amdtp_domain_stop(struct amdtp_domain *d);
static inline int amdtp_domain_set_events_per_period(struct amdtp_domain *d,
						unsigned int events_per_period,
						unsigned int events_per_buffer)
{
	d->events_per_period = events_per_period;
	d->events_per_buffer = events_per_buffer;
	return 0;
}
unsigned long amdtp_domain_stream_pcm_pointer(struct amdtp_domain *d,
					      struct amdtp_stream *s);
int amdtp_domain_stream_pcm_ack(struct amdtp_domain *d, struct amdtp_stream *s);
static inline bool amdtp_domain_wait_ready(struct amdtp_domain *d, unsigned int timeout_ms)
{
	struct amdtp_stream *s;
	list_for_each_entry(s, &d->streams, list) {
		unsigned int j = msecs_to_jiffies(timeout_ms);
		if (wait_event_interruptible_timeout(s->ready_wait, s->ready_processing, j) <= 0)
			return false;
	}
	return true;
}
#endif
