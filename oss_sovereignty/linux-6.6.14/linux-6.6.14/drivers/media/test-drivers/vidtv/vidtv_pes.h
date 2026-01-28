#ifndef VIDTV_PES_H
#define VIDTV_PES_H
#include <linux/types.h>
#include "vidtv_common.h"
#define PES_MAX_LEN 65536  
#define PES_START_CODE_PREFIX 0x001  
struct vidtv_pes_optional_pts {
	u8 pts1;
	__be16 pts2;
	__be16 pts3;
} __packed;
struct vidtv_pes_optional_pts_dts {
	u8 pts1;
	__be16 pts2;
	__be16 pts3;
	u8 dts1;
	__be16 dts2;
	__be16 dts3;
} __packed;
struct vidtv_pes_optional {
	__be16 bitfield;
	u8 length;
} __packed;
struct vidtv_mpeg_pes {
	__be32 bitfield;  
	__be16 length;
	struct vidtv_pes_optional optional[];
} __packed;
struct pes_header_write_args {
	void *dest_buf;
	u32 dest_offset;
	u32 dest_buf_sz;
	u32 encoder_id;
	bool send_pts;
	u64 pts;
	bool send_dts;
	u64 dts;
	u16 stream_id;
	u32 n_pes_h_s_bytes;
	u32 access_unit_len;
};
struct pes_ts_header_write_args {
	void *dest_buf;
	u32 dest_offset;
	u32 dest_buf_sz;
	u16 pid;
	u8 *continuity_counter;
	bool wrote_pes_header;
	u32 n_stuffing_bytes;
	u64 pcr;
};
struct pes_write_args {
	void *dest_buf;
	void *from;
	u32 access_unit_len;
	u32 dest_offset;
	u32 dest_buf_sz;
	u16 pid;
	u32 encoder_id;
	u8 *continuity_counter;
	u16 stream_id;
	bool send_pts;
	u64 pts;
	bool send_dts;
	u64 dts;
	u32 n_pes_h_s_bytes;
	u64 pcr;
};
u32 vidtv_pes_write_into(struct pes_write_args *args);
#endif  
