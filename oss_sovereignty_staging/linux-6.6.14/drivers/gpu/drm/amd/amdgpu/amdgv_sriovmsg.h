 

#ifndef AMDGV_SRIOV_MSG__H_
#define AMDGV_SRIOV_MSG__H_

 
#define AMD_SRIOV_MSG_VBIOS_OFFSET	     0
#define AMD_SRIOV_MSG_VBIOS_SIZE_KB	     64
#define AMD_SRIOV_MSG_DATAEXCHANGE_OFFSET_KB AMD_SRIOV_MSG_VBIOS_SIZE_KB
#define AMD_SRIOV_MSG_DATAEXCHANGE_SIZE_KB   4

 
#define AMD_SRIOV_MSG_SIZE_KB                   1
#define AMD_SRIOV_MSG_PF2VF_OFFSET_KB           AMD_SRIOV_MSG_DATAEXCHANGE_OFFSET_KB
#define AMD_SRIOV_MSG_VF2PF_OFFSET_KB           (AMD_SRIOV_MSG_PF2VF_OFFSET_KB + AMD_SRIOV_MSG_SIZE_KB)
#define AMD_SRIOV_MSG_BAD_PAGE_OFFSET_KB        (AMD_SRIOV_MSG_VF2PF_OFFSET_KB + AMD_SRIOV_MSG_SIZE_KB)

 
#define AMD_SRIOV_MSG_FW_VRAM_PF2VF_VER 2
#define AMD_SRIOV_MSG_FW_VRAM_VF2PF_VER 3

#define AMD_SRIOV_MSG_RESERVE_UCODE 24

#define AMD_SRIOV_MSG_RESERVE_VCN_INST 4

enum amd_sriov_ucode_engine_id {
	AMD_SRIOV_UCODE_ID_VCE = 0,
	AMD_SRIOV_UCODE_ID_UVD,
	AMD_SRIOV_UCODE_ID_MC,
	AMD_SRIOV_UCODE_ID_ME,
	AMD_SRIOV_UCODE_ID_PFP,
	AMD_SRIOV_UCODE_ID_CE,
	AMD_SRIOV_UCODE_ID_RLC,
	AMD_SRIOV_UCODE_ID_RLC_SRLC,
	AMD_SRIOV_UCODE_ID_RLC_SRLG,
	AMD_SRIOV_UCODE_ID_RLC_SRLS,
	AMD_SRIOV_UCODE_ID_MEC,
	AMD_SRIOV_UCODE_ID_MEC2,
	AMD_SRIOV_UCODE_ID_SOS,
	AMD_SRIOV_UCODE_ID_ASD,
	AMD_SRIOV_UCODE_ID_TA_RAS,
	AMD_SRIOV_UCODE_ID_TA_XGMI,
	AMD_SRIOV_UCODE_ID_SMC,
	AMD_SRIOV_UCODE_ID_SDMA,
	AMD_SRIOV_UCODE_ID_SDMA2,
	AMD_SRIOV_UCODE_ID_VCN,
	AMD_SRIOV_UCODE_ID_DMCU,
	AMD_SRIOV_UCODE_ID__MAX
};

#pragma pack(push, 1) 

union amd_sriov_msg_feature_flags {
	struct {
		uint32_t error_log_collect : 1;
		uint32_t host_load_ucodes  : 1;
		uint32_t host_flr_vramlost : 1;
		uint32_t mm_bw_management  : 1;
		uint32_t pp_one_vf_mode	   : 1;
		uint32_t reg_indirect_acc  : 1;
		uint32_t av1_support       : 1;
		uint32_t reserved	   : 25;
	} flags;
	uint32_t all;
};

union amd_sriov_reg_access_flags {
	struct {
		uint32_t vf_reg_access_ih 	 : 1;
		uint32_t vf_reg_access_mmhub : 1;
		uint32_t vf_reg_access_gc 	 : 1;
		uint32_t reserved	         : 29;
	} flags;
	uint32_t all;
};

union amd_sriov_msg_os_info {
	struct {
		uint32_t windows  : 1;
		uint32_t reserved : 31;
	} info;
	uint32_t all;
};

struct amd_sriov_msg_uuid_info {
	union {
		struct {
			uint32_t did	: 16;
			uint32_t fcn	: 8;
			uint32_t asic_7 : 8;
		};
		uint32_t time_low;
	};

	struct {
		uint32_t time_mid  : 16;
		uint32_t time_high : 12;
		uint32_t version   : 4;
	};

	struct {
		struct {
			uint8_t clk_seq_hi : 6;
			uint8_t variant    : 2;
		};
		union {
			uint8_t clk_seq_low;
			uint8_t asic_6;
		};
		uint16_t asic_4;
	};

	uint32_t asic_0;
};

struct amd_sriov_msg_pf2vf_info_header {
	 
	uint32_t size;
	 
	uint32_t version;
	 
	uint32_t reserved[2];
};

#define AMD_SRIOV_MSG_PF2VF_INFO_FILLED_SIZE (48)
struct amd_sriov_msg_pf2vf_info {
	 
	struct amd_sriov_msg_pf2vf_info_header header;
	 
	uint32_t checksum;
	 
	union amd_sriov_msg_feature_flags feature_flags;
	 
	uint32_t hevc_enc_max_mb_per_second;
	 
	uint32_t hevc_enc_max_mb_per_frame;
	 
	uint32_t avc_enc_max_mb_per_second;
	 
	uint32_t avc_enc_max_mb_per_frame;
	 
	uint64_t mecfw_offset;
	 
	uint32_t mecfw_size;
	 
	uint64_t uvdfw_offset;
	 
	uint32_t uvdfw_size;
	 
	uint64_t vcefw_offset;
	 
	uint32_t vcefw_size;
	 
	uint32_t bp_block_offset_low;
	uint32_t bp_block_offset_high;
	 
	uint32_t bp_block_size;
	 
	uint32_t vf2pf_update_interval_ms;
	 
	uint64_t uuid;
	uint32_t fcn_idx;
	 
	union amd_sriov_reg_access_flags reg_access_flags;
	 
	struct {
		uint32_t decode_max_dimension_pixels;
		uint32_t decode_max_frame_pixels;
		uint32_t encode_max_dimension_pixels;
		uint32_t encode_max_frame_pixels;
	} mm_bw_management[AMD_SRIOV_MSG_RESERVE_VCN_INST];
	 
	struct amd_sriov_msg_uuid_info uuid_info;
	 
	uint32_t pcie_atomic_ops_support_flags;
	 
	uint32_t reserved[256 - AMD_SRIOV_MSG_PF2VF_INFO_FILLED_SIZE];
};

struct amd_sriov_msg_vf2pf_info_header {
	 
	uint32_t size;
	 
	uint32_t version;
	 
	uint32_t reserved[2];
};

#define AMD_SRIOV_MSG_VF2PF_INFO_FILLED_SIZE (70)
struct amd_sriov_msg_vf2pf_info {
	 
	struct amd_sriov_msg_vf2pf_info_header header;
	uint32_t checksum;
	 
	uint8_t driver_version[64];
	 
	uint32_t driver_cert;
	 
	union amd_sriov_msg_os_info os_info;
	 
	uint32_t fb_usage;
	 
	uint32_t gfx_usage;
	 
	uint32_t gfx_health;
	 
	uint32_t compute_usage;
	 
	uint32_t compute_health;
	 
	uint32_t avc_enc_usage;
	 
	uint32_t avc_enc_health;
	 
	uint32_t hevc_enc_usage;
	 
	uint32_t hevc_enc_health;
	 
	uint32_t encode_usage;
	uint32_t decode_usage;
	 
	uint32_t pf2vf_version_required;
	 
	uint32_t fb_vis_usage;
	uint32_t fb_vis_size;
	uint32_t fb_size;
	 
	struct {
		uint8_t id;
		uint32_t version;
	} ucode_info[AMD_SRIOV_MSG_RESERVE_UCODE];
	uint64_t dummy_page_addr;

	 
	uint32_t reserved[256 - AMD_SRIOV_MSG_VF2PF_INFO_FILLED_SIZE];
};

 
enum amd_sriov_mailbox_request_message {
	MB_REQ_MSG_REQ_GPU_INIT_ACCESS = 1,
	MB_REQ_MSG_REL_GPU_INIT_ACCESS,
	MB_REQ_MSG_REQ_GPU_FINI_ACCESS,
	MB_REQ_MSG_REL_GPU_FINI_ACCESS,
	MB_REQ_MSG_REQ_GPU_RESET_ACCESS,
	MB_REQ_MSG_REQ_GPU_INIT_DATA,

	MB_REQ_MSG_LOG_VF_ERROR = 200,
};

 
enum amd_sriov_mailbox_response_message {
	MB_RES_MSG_CLR_MSG_BUF = 0,
	MB_RES_MSG_READY_TO_ACCESS_GPU = 1,
	MB_RES_MSG_FLR_NOTIFICATION,
	MB_RES_MSG_FLR_NOTIFICATION_COMPLETION,
	MB_RES_MSG_SUCCESS,
	MB_RES_MSG_FAIL,
	MB_RES_MSG_QUERY_ALIVE,
	MB_RES_MSG_GPU_INIT_DATA_READY,

	MB_RES_MSG_TEXT_MESSAGE = 255
};

 
enum amd_sriov_gpu_init_data_version {
	GPU_INIT_DATA_READY_V1 = 1,
};

#pragma pack(pop) 

 
unsigned int amd_sriov_msg_checksum(void *obj, unsigned long obj_size, unsigned int key,
				    unsigned int checksum);

 
#ifdef __linux__
#define stringification(s)  _stringification(s)
#define _stringification(s) #s

_Static_assert(
	sizeof(struct amd_sriov_msg_vf2pf_info) == AMD_SRIOV_MSG_SIZE_KB << 10,
	"amd_sriov_msg_vf2pf_info must be " stringification(AMD_SRIOV_MSG_SIZE_KB) " KB");

_Static_assert(
	sizeof(struct amd_sriov_msg_pf2vf_info) == AMD_SRIOV_MSG_SIZE_KB << 10,
	"amd_sriov_msg_pf2vf_info must be " stringification(AMD_SRIOV_MSG_SIZE_KB) " KB");

_Static_assert(AMD_SRIOV_MSG_RESERVE_UCODE % 4 == 0,
	       "AMD_SRIOV_MSG_RESERVE_UCODE must be multiple of 4");

_Static_assert(AMD_SRIOV_MSG_RESERVE_UCODE > AMD_SRIOV_UCODE_ID__MAX,
	       "AMD_SRIOV_MSG_RESERVE_UCODE must be bigger than AMD_SRIOV_UCODE_ID__MAX");

#undef _stringification
#undef stringification
#endif

#endif  
