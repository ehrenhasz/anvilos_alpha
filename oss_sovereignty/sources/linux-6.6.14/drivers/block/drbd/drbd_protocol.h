
#ifndef __DRBD_PROTOCOL_H
#define __DRBD_PROTOCOL_H

enum drbd_packet {
	
	P_DATA		      = 0x00,
	P_DATA_REPLY	      = 0x01, 
	P_RS_DATA_REPLY	      = 0x02, 
	P_BARRIER	      = 0x03,
	P_BITMAP	      = 0x04,
	P_BECOME_SYNC_TARGET  = 0x05,
	P_BECOME_SYNC_SOURCE  = 0x06,
	P_UNPLUG_REMOTE	      = 0x07, 
	P_DATA_REQUEST	      = 0x08, 
	P_RS_DATA_REQUEST     = 0x09, 
	P_SYNC_PARAM	      = 0x0a,
	P_PROTOCOL	      = 0x0b,
	P_UUIDS		      = 0x0c,
	P_SIZES		      = 0x0d,
	P_STATE		      = 0x0e,
	P_SYNC_UUID	      = 0x0f,
	P_AUTH_CHALLENGE      = 0x10,
	P_AUTH_RESPONSE	      = 0x11,
	P_STATE_CHG_REQ	      = 0x12,

	
	P_PING		      = 0x13,
	P_PING_ACK	      = 0x14,
	P_RECV_ACK	      = 0x15, 
	P_WRITE_ACK	      = 0x16, 
	P_RS_WRITE_ACK	      = 0x17, 
	P_SUPERSEDED	      = 0x18, 
	P_NEG_ACK	      = 0x19, 
	P_NEG_DREPLY	      = 0x1a, 
	P_NEG_RS_DREPLY	      = 0x1b, 
	P_BARRIER_ACK	      = 0x1c,
	P_STATE_CHG_REPLY     = 0x1d,

	

	P_OV_REQUEST	      = 0x1e, 
	P_OV_REPLY	      = 0x1f,
	P_OV_RESULT	      = 0x20, 
	P_CSUM_RS_REQUEST     = 0x21, 
	P_RS_IS_IN_SYNC	      = 0x22, 
	P_SYNC_PARAM89	      = 0x23, 
	P_COMPRESSED_BITMAP   = 0x24, 
	
	
	P_DELAY_PROBE         = 0x27, 
	P_OUT_OF_SYNC         = 0x28, 
	P_RS_CANCEL           = 0x29, 
	P_CONN_ST_CHG_REQ     = 0x2a, 
	P_CONN_ST_CHG_REPLY   = 0x2b, 
	P_RETRY_WRITE	      = 0x2c, 
	P_PROTOCOL_UPDATE     = 0x2d, 
        

	
	P_TRIM                = 0x31,

	
	P_RS_THIN_REQ         = 0x32, 
	P_RS_DEALLOCATED      = 0x33, 

	
	P_WSAME               = 0x34,

	
	P_ZEROES              = 0x36, 

	

	P_MAY_IGNORE	      = 0x100, 
	P_MAX_OPT_CMD	      = 0x101,

	

	P_INITIAL_META	      = 0xfff1, 
	P_INITIAL_DATA	      = 0xfff2, 

	P_CONNECTION_FEATURES = 0xfffe	
};

#ifndef __packed
#define __packed __attribute__((packed))
#endif


struct p_header80 {
	u32	  magic;
	u16	  command;
	u16	  length;	
} __packed;


struct p_header95 {
	u16	  magic;	
	u16	  command;
	u32	  length;
} __packed;

struct p_header100 {
	u32	  magic;
	u16	  volume;
	u16	  command;
	u32	  length;
	u32	  pad;
} __packed;


#define DP_HARDBARRIER	      1 
#define DP_RW_SYNC	      2 
#define DP_MAY_SET_IN_SYNC    4
#define DP_UNPLUG             8 
#define DP_FUA               16 
#define DP_FLUSH             32 
#define DP_DISCARD           64 
#define DP_SEND_RECEIVE_ACK 128 
#define DP_SEND_WRITE_ACK   256 
#define DP_WSAME            512 
#define DP_ZEROES          1024 



struct p_data {
	u64	    sector;    
	u64	    block_id;  
	u32	    seq_num;
	u32	    dp_flags;
} __packed;

struct p_trim {
	struct p_data p_data;
	u32	    size;	
} __packed;

struct p_wsame {
	struct p_data p_data;
	u32           size;     
} __packed;


struct p_block_ack {
	u64	    sector;
	u64	    block_id;
	u32	    blksize;
	u32	    seq_num;
} __packed;

struct p_block_req {
	u64 sector;
	u64 block_id;
	u32 blksize;
	u32 pad;	
} __packed;




#define DRBD_FF_TRIM 1


#define DRBD_FF_THIN_RESYNC 2


#define DRBD_FF_WSAME 4


#define DRBD_FF_WZEROES 8


struct p_connection_features {
	u32 protocol_min;
	u32 feature_flags;
	u32 protocol_max;

	

	u32 _pad;
	u64 reserved[7];
} __packed;

struct p_barrier {
	u32 barrier;	
	u32 pad;	
} __packed;

struct p_barrier_ack {
	u32 barrier;
	u32 set_size;
} __packed;

struct p_rs_param {
	u32 resync_rate;

	      
	char verify_alg[];
} __packed;

struct p_rs_param_89 {
	u32 resync_rate;
	
	char verify_alg[SHARED_SECRET_MAX];
	char csums_alg[SHARED_SECRET_MAX];
} __packed;

struct p_rs_param_95 {
	u32 resync_rate;
	struct_group(algs,
		char verify_alg[SHARED_SECRET_MAX];
		char csums_alg[SHARED_SECRET_MAX];
	);
	u32 c_plan_ahead;
	u32 c_delay_target;
	u32 c_fill_target;
	u32 c_max_rate;
} __packed;

enum drbd_conn_flags {
	CF_DISCARD_MY_DATA = 1,
	CF_DRY_RUN = 2,
};

struct p_protocol {
	u32 protocol;
	u32 after_sb_0p;
	u32 after_sb_1p;
	u32 after_sb_2p;
	u32 conn_flags;
	u32 two_primaries;

	
	char integrity_alg[];

} __packed;

struct p_uuids {
	u64 uuid[UI_EXTENDED_SIZE];
} __packed;

struct p_rs_uuid {
	u64	    uuid;
} __packed;


struct o_qlim {
	
	u32 physical_block_size;

	
	u32 logical_block_size;

	

	
	u32 alignment_offset;
	u32 io_min;
	u32 io_opt;

	

	
	u8 discard_enabled;
	u8 discard_zeroes_data;
	u8 write_same_capable;
	u8 _pad;
} __packed;

struct p_sizes {
	u64	    d_size;  
	u64	    u_size;  
	u64	    c_size;  
	u32	    max_bio_size;  
	u16	    queue_order_type;  
	u16	    dds_flags; 

	
	struct o_qlim qlim[];
} __packed;

struct p_state {
	u32	    state;
} __packed;

struct p_req_state {
	u32	    mask;
	u32	    val;
} __packed;

struct p_req_state_reply {
	u32	    retcode;
} __packed;

struct p_drbd06_param {
	u64	  size;
	u32	  state;
	u32	  blksize;
	u32	  protocol;
	u32	  version;
	u32	  gen_cnt[5];
	u32	  bit_map_gen[5];
} __packed;

struct p_block_desc {
	u64 sector;
	u32 blksize;
	u32 pad;	
} __packed;


enum drbd_bitmap_code {
	
	RLE_VLI_Bits = 2,
};

struct p_compressed_bm {
	
	u8 encoding;

	u8 code[];
} __packed;

struct p_delay_probe93 {
	u32     seq_num; 
	u32     offset;  
} __packed;


#define DRBD_SOCKET_BUFFER_SIZE 4096

#endif  
