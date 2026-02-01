 
 

#ifndef __HGSMI_DEFS_H__
#define __HGSMI_DEFS_H__

 
#define HGSMI_BUFFER_HEADER_F_SEQ_MASK     0x03
 
#define HGSMI_BUFFER_HEADER_F_SEQ_SINGLE   0x00
 
#define HGSMI_BUFFER_HEADER_F_SEQ_START    0x01
 
#define HGSMI_BUFFER_HEADER_F_SEQ_CONTINUE 0x02
 
#define HGSMI_BUFFER_HEADER_F_SEQ_END      0x03

 
struct hgsmi_buffer_header {
	u32 data_size;		 
	u8 flags;		 
	u8 channel;		 
	u16 channel_info;	 

	union {
		 
		u8 header_data[8];

		 
		struct {
			u32 reserved1;	 
			u32 reserved2;	 
		} buffer;

		 
		struct {
			 
			u32 sequence_number;
			 
			u32 sequence_size;
		} sequence_start;

		 
		struct {
			 
			u32 sequence_number;
			 
			u32 sequence_offset;
		} sequence_continue;
	} u;
} __packed;

 
struct hgsmi_buffer_tail {
	 
	u32 reserved;
	 
	u32 checksum;
} __packed;

 
#define HGSMI_NUMBER_OF_CHANNELS 0x100

#endif
