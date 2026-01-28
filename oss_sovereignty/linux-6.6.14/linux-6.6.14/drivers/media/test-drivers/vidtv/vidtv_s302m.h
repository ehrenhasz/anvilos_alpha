#ifndef VIDTV_S302M_H
#define VIDTV_S302M_H
#include <linux/types.h>
#include "vidtv_encoder.h"
#define VIDTV_S302M_BUF_SZ 65024
#define VIDTV_S302M_FORMAT_IDENTIFIER 0x42535344
struct vidtv_s302m_ctx {
	struct vidtv_encoder *enc;
	u32 frame_index;
	u32 au_count;
	int last_duration;
	unsigned int note_offset;
	enum musical_notes last_tone;
};
struct vidtv_smpte_s302m_es {
	__be32 bitfield;
} __packed;
struct vidtv_s302m_frame_16 {
	u8 data[5];
} __packed;
struct vidtv_s302m_encoder_init_args {
	char *name;
	void *src_buf;
	u32 src_buf_sz;
	u16 es_pid;
	struct vidtv_encoder *sync;
	void (*last_sample_cb)(u32 sample_no);
	struct vidtv_encoder *head;
};
struct vidtv_encoder
*vidtv_s302m_encoder_init(struct vidtv_s302m_encoder_init_args args);
void vidtv_s302m_encoder_destroy(struct vidtv_encoder *encoder);
#endif  
