#ifndef __SST_MFLD_DSP_H__
#define __SST_MFLD_DSP_H__
#define SST_MAX_BIN_BYTES 1024
#define MAX_DBG_RW_BYTES 80
#define MAX_NUM_SCATTER_BUFFERS 8
#define MAX_LOOP_BACK_DWORDS 8
#define SST_MAILBOX_SIZE 0x0400
#define SST_MAILBOX_SEND 0x0000
#define SST_TIME_STAMP 0x1800
#define SST_TIME_STAMP_MRFLD 0x800
#define SST_RESERVED_OFFSET 0x1A00
#define SST_SCU_LPE_MAILBOX 0x1000
#define SST_LPE_SCU_MAILBOX 0x1400
#define SST_SCU_LPE_LOG_BUF (SST_SCU_LPE_MAILBOX+16)
#define PROCESS_MSG 0x80
#define IPC_IA_PREP_LIB_DNLD 0x01
#define IPC_IA_LIB_DNLD_CMPLT 0x02
#define IPC_IA_GET_FW_VERSION 0x04
#define IPC_IA_GET_FW_BUILD_INF 0x05
#define IPC_IA_GET_FW_INFO 0x06
#define IPC_IA_GET_FW_CTXT 0x07
#define IPC_IA_SET_FW_CTXT 0x08
#define IPC_IA_PREPARE_SHUTDOWN 0x31
#define IPC_PREP_D3 0x10
#define IPC_IA_SET_CODEC_PARAMS 0x10
#define IPC_IA_GET_CODEC_PARAMS 0x11
#define IPC_IA_SET_PPP_PARAMS 0x12
#define IPC_IA_GET_PPP_PARAMS 0x13
#define IPC_SST_PERIOD_ELAPSED_MRFLD 0xA
#define IPC_IA_ALG_PARAMS 0x1A
#define IPC_IA_TUNING_PARAMS 0x1B
#define IPC_IA_SET_RUNTIME_PARAMS 0x1C
#define IPC_IA_SET_PARAMS 0x1
#define IPC_IA_GET_PARAMS 0x2
#define IPC_EFFECTS_CREATE 0xE
#define IPC_EFFECTS_DESTROY 0xF
#define IPC_IA_ALLOC_STREAM_MRFLD 0x2
#define IPC_IA_ALLOC_STREAM 0x20  
#define IPC_IA_FREE_STREAM_MRFLD 0x03
#define IPC_IA_FREE_STREAM 0x21  
#define IPC_IA_SET_STREAM_PARAMS 0x22
#define IPC_IA_SET_STREAM_PARAMS_MRFLD 0x12
#define IPC_IA_GET_STREAM_PARAMS 0x23
#define IPC_IA_PAUSE_STREAM 0x24
#define IPC_IA_PAUSE_STREAM_MRFLD 0x4
#define IPC_IA_RESUME_STREAM 0x25
#define IPC_IA_RESUME_STREAM_MRFLD 0x5
#define IPC_IA_DROP_STREAM 0x26
#define IPC_IA_DROP_STREAM_MRFLD 0x07
#define IPC_IA_DRAIN_STREAM 0x27  
#define IPC_IA_DRAIN_STREAM_MRFLD 0x8
#define IPC_IA_CONTROL_ROUTING 0x29
#define IPC_IA_VTSV_UPDATE_MODULES 0x20
#define IPC_IA_VTSV_DETECTED 0x21
#define IPC_IA_START_STREAM_MRFLD 0X06
#define IPC_IA_START_STREAM 0x30  
#define IPC_IA_SET_GAIN_MRFLD 0x21
#define IPC_IA_DBG_MEM_READ 0x40
#define IPC_IA_DBG_MEM_WRITE 0x41
#define IPC_IA_DBG_LOOP_BACK 0x42
#define IPC_IA_DBG_LOG_ENABLE 0x45
#define IPC_IA_DBG_SET_PROBE_PARAMS 0x47
#define IPC_IA_FW_INIT_CMPLT 0x81
#define IPC_IA_FW_INIT_CMPLT_MRFLD 0x01
#define IPC_IA_FW_ASYNC_ERR_MRFLD 0x11
#define IPC_SST_FRAGMENT_ELPASED 0x90  
#define IPC_SST_BUF_UNDER_RUN 0x92  
#define IPC_SST_BUF_OVER_RUN 0x93  
#define IPC_SST_DRAIN_END 0x94  
#define IPC_SST_CHNGE_SSP_PARAMS 0x95  
#define IPC_SST_STREAM_PROCESS_FATAL_ERR 0x96 
#define IPC_SST_PERIOD_ELAPSED 0x97  
#define IPC_SST_ERROR_EVENT 0x99  
#define IPC_SC_DDR_LINK_UP 0xC0
#define IPC_SC_DDR_LINK_DOWN 0xC1
#define IPC_SC_SET_LPECLK_REQ 0xC2
#define IPC_SC_SSP_BIT_BANG 0xC3
#define IPC_IA_MEM_ALLOC_FAIL 0xE0
#define IPC_IA_PROC_ERR 0xE1  
#define IPC_IA_PRINT_STRING 0xF0
#define IPC_IA_BUF_UNDER_RUN_MRFLD 0x0B
#define SST_ASYNC_DRV_ID 0
enum ackData {
	IPC_ACK_SUCCESS = 0,
	IPC_ACK_FAILURE,
};
enum ipc_ia_msg_id {
	IPC_CMD = 1,		 
	IPC_SET_PARAMS = 2, 
	IPC_GET_PARAMS = 3,	 
	IPC_INVALID = 0xFF,	 
};
enum sst_codec_types {
	SST_CODEC_TYPE_UNKNOWN = 0,
	SST_CODEC_TYPE_PCM,	 
	SST_CODEC_TYPE_MP3,
	SST_CODEC_TYPE_MP24,
	SST_CODEC_TYPE_AAC,
	SST_CODEC_TYPE_AACP,
	SST_CODEC_TYPE_eAACP,
};
enum stream_type {
	SST_STREAM_TYPE_NONE = 0,
	SST_STREAM_TYPE_MUSIC = 1,
};
enum sst_error_codes {
	SST_SUCCESS = 0,         
	SST_ERR_INVALID_STREAM_ID = 1,
	SST_ERR_INVALID_MSG_ID = 2,
	SST_ERR_INVALID_STREAM_OP = 3,
	SST_ERR_INVALID_PARAMS = 4,
	SST_ERR_INVALID_CODEC = 5,
	SST_ERR_INVALID_MEDIA_TYPE = 6,
	SST_ERR_STREAM_ERR = 7,
	SST_ERR_STREAM_IN_USE = 15,
};
struct ipc_dsp_hdr {
	u16 mod_index_id:8;		 
	u16 pipe_id:8;	 
	u16 mod_id;		 
	u16 cmd_id;		 
	u16 length;		 
} __packed;
union ipc_header_high {
	struct {
		u32  msg_id:8;	     
		u32  task_id:4;	     
		u32  drv_id:4;     
		u32  rsvd1:8;	     
		u32  result:4;	     
		u32  res_rqd:1;	     
		u32  large:1;	     
		u32  done:1;	     
		u32  busy:1;	     
	} part;
	u32 full;
} __packed;
union ipc_header_mrfld {
	struct {
		u32 header_low_payload;
		union ipc_header_high header_high;
	} p;
	u64 full;
} __packed;
union ipc_header {
	struct {
		u32  msg_id:8;  
		u32  str_id:5;
		u32  large:1;	 
		u32  reserved:2;	 
		u32  data:14;	 
		u32  done:1;  
		u32  busy:1;  
	} part;
	u32 full;
} __packed;
struct sst_fw_build_info {
	unsigned char  date[16];  
	unsigned char  time[16];  
} __packed;
struct snd_sst_fw_version {
	u8 build;	 
	u8 minor;	 
	u8 major;	 
	u8 type;	 
};
struct ipc_header_fw_init {
	struct snd_sst_fw_version fw_version; 
	struct sst_fw_build_info build_info;
	u16 result;	 
	u8 module_id;  
	u8 debug_info;  
} __packed;
struct snd_sst_tstamp {
	u64 ring_buffer_counter;	 
	u64 hardware_counter;	     
	u64 frames_decoded;
	u64 bytes_decoded;
	u64 bytes_copied;
	u32 sampling_frequency;
	u32 channel_peak[8];
} __packed;
struct snd_sst_str_type {
	u8 codec_type;		 
	u8 str_type;		 
	u8 operation;		 
	u8 protected_str;	 
	u8 time_slots;
	u8 reserved;		 
	u16 result;		 
} __packed;
struct module_info {
	u32 lib_version;
	u32 lib_type; 
	u32 media_type;
	u8  lib_name[12];
	u32 lib_caps;
	unsigned char  b_date[16];  
	unsigned char  b_time[16];  
} __packed;
struct lib_slot_info {
	u8  slot_num;  
	u8  reserved1;
	u16 reserved2;
	u32 iram_size;  
	u32 dram_size;  
	u32 iram_offset;  
	u32 dram_offset;  
} __packed;
struct snd_ppp_mixer_params {
	__u32			type;  
	__u32			size;
	__u32			input_stream_bitmap;  
} __packed;
struct snd_sst_lib_download {
	struct module_info lib_info;  
	struct lib_slot_info slot_info;  
	u32 mod_entry_pt;
};
struct snd_sst_lib_download_info {
	struct snd_sst_lib_download dload_lib;
	u16 result;	 
	u8 pvt_id;  
	u8 reserved;   
};
struct snd_pcm_params {
	u8 num_chan;	 
	u8 pcm_wd_sz;	 
	u8 use_offload_path;	 
	u8 reserved2;
	u32 sfreq;     
	u8 channel_map[8];
} __packed;
struct snd_mp3_params {
	u8  num_chan;	 
	u8  pcm_wd_sz;  
	u8  crc_check;  
	u8  reserved1;  
	u16 reserved2;	 
} __packed;
#define AAC_BIT_STREAM_ADTS		0
#define AAC_BIT_STREAM_ADIF		1
#define AAC_BIT_STREAM_RAW		2
struct snd_aac_params {
	u8 num_chan;  
	u8 pcm_wd_sz;  
	u8 bdownsample;  
	u8 bs_format;  
	u16  reser2;
	u32 externalsr;  
	u8 sbr_signalling; 
	u8 reser1;
	u16  reser3;
} __packed;
struct snd_wma_params {
	u8  num_chan;	 
	u8  pcm_wd_sz;	 
	u16 reserved1;
	u32 brate;	 
	u32 sfreq;	 
	u32 channel_mask;   
	u16 format_tag;	 
	u16 block_align;	 
	u16 wma_encode_opt; 
	u8 op_align;	 
	u8 reserved;	 
} __packed;
union  snd_sst_codec_params {
	struct snd_pcm_params pcm_params;
	struct snd_mp3_params mp3_params;
	struct snd_aac_params aac_params;
	struct snd_wma_params wma_params;
} __packed;
struct sst_address_info {
	u32 addr;  
	u32 size;  
};
struct snd_sst_alloc_params_ext {
	__u16 sg_count;
	__u16 reserved;
	__u32 frag_size;	 
	struct sst_address_info  ring_buf_info[8];
};
struct snd_sst_stream_params {
	union snd_sst_codec_params uc;
} __packed;
struct snd_sst_params {
	u32 result;
	u32 stream_id;
	u8 codec;
	u8 ops;
	u8 stream_type;
	u8 device_type;
	u8 task;
	struct snd_sst_stream_params sparams;
	struct snd_sst_alloc_params_ext aparams;
};
struct snd_sst_alloc_mrfld {
	u16 codec_type;
	u8 operation;
	u8 sg_count;
	struct sst_address_info ring_buf_info[8];
	u32 frag_size;
	u32 ts;
	struct snd_sst_stream_params codec_params;
} __packed;
struct snd_sst_alloc_params {
	struct snd_sst_str_type str_type;
	struct snd_sst_stream_params stream_params;
	struct snd_sst_alloc_params_ext alloc_params;
} __packed;
struct snd_sst_alloc_response {
	struct snd_sst_str_type str_type;  
	struct snd_sst_lib_download lib_dnld;  
};
struct snd_sst_drop_response {
	u32 result;
	u32 bytes;
};
struct snd_sst_async_msg {
	u32 msg_id;  
	u32 payload[];
};
struct snd_sst_async_err_msg {
	u32 fw_resp;  
	u32 lib_resp;  
} __packed;
struct snd_sst_vol {
	u32	stream_id;
	s32	volume;
	u32	ramp_duration;
	u32	ramp_type;		 
};
struct snd_sst_gain_v2 {
	u16 gain_cell_num;   
	u8 cell_nbr_idx;  
	u8 cell_path_idx;  
	u16 module_id;  
	u16 left_cell_gain;  
	u16 right_cell_gain;  
	u16 gain_time_const;  
} __packed;
struct snd_sst_mute {
	u32	stream_id;
	u32	mute;
};
struct snd_sst_runtime_params {
	u8 type;
	u8 str_id;
	u8 size;
	u8 rsvd;
	void *addr;
} __packed;
enum stream_param_type {
	SST_SET_TIME_SLOT = 0,
	SST_SET_CHANNEL_INFO = 1,
	OTHERS = 2,  
};
struct snd_sst_control_routing {
	u8 control;  
	u8 reserved[3];	 
};
struct ipc_post {
	struct list_head node;
	union ipc_header header;  
	bool is_large;
	bool is_process_reply;
	union ipc_header_mrfld mrfld_header;
	char *mailbox_data;
};
struct snd_sst_ctxt_params {
	u32 address;  
	u32 size;  
};
struct snd_sst_lpe_log_params {
	u8 dbg_type;
	u8 module_id;
	u8 log_level;
	u8 reserved;
} __packed;
enum snd_sst_bytes_type {
	SND_SST_BYTES_SET = 0x1,
	SND_SST_BYTES_GET = 0x2,
};
struct snd_sst_bytes_v2 {
	u8 type;
	u8 ipc_msg;
	u8 block;
	u8 task_id;
	u8 pipe_id;
	u8 rsvd;
	u16 len;
	char bytes[];
};
#define MAX_VTSV_FILES 2
struct snd_sst_vtsv_info {
	struct sst_address_info vfiles[MAX_VTSV_FILES];
} __packed;
#endif  
