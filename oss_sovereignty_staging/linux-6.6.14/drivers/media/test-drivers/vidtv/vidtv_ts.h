 
 

#ifndef VIDTV_TS_H
#define VIDTV_TS_H

#include <linux/types.h>

#define TS_SYNC_BYTE 0x47
#define TS_PACKET_LEN 188
#define TS_PAYLOAD_LEN 184
#define TS_NULL_PACKET_PID 0x1fff
#define TS_CC_MAX_VAL 0x0f  
#define TS_LAST_VALID_PID 8191
#define TS_FILL_BYTE 0xff  

struct vidtv_mpeg_ts_adaption {
	u8 length;
	struct {
		u8 extension:1;
		u8 private_data:1;
		u8 splicing_point:1;
		u8 OPCR:1;
		u8 PCR:1;
		u8 priority:1;
		u8 random_access:1;
		u8 discontinued:1;
	} __packed;
	u8 data[];
} __packed;

struct vidtv_mpeg_ts {
	u8 sync_byte;
	__be16 bitfield;  
	struct {
		u8 continuity_counter:4;
		u8 payload:1;
		u8 adaptation_field:1;
		u8 scrambling:2;
	} __packed;
} __packed;

 
struct pcr_write_args {
	void *dest_buf;
	u32 dest_offset;
	u16 pid;
	u32 buf_sz;
	u8 *continuity_counter;
	u64 pcr;
};

 
struct null_packet_write_args {
	void *dest_buf;
	u32 dest_offset;
	u32 buf_sz;
	u8 *continuity_counter;
};

 
void vidtv_ts_inc_cc(u8 *continuity_counter);

 
u32 vidtv_ts_null_write_into(struct null_packet_write_args args);

 
u32 vidtv_ts_pcr_write_into(struct pcr_write_args args);

#endif 
