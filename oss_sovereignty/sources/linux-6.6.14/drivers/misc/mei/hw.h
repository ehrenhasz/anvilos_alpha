


#ifndef _MEI_HW_TYPES_H_
#define _MEI_HW_TYPES_H_

#include <linux/mei.h>


#define MEI_HW_READY_TIMEOUT        2  
#define MEI_CONNECT_TIMEOUT         3  

#define MEI_CL_CONNECT_TIMEOUT     15  
#define MEI_CL_CONNECT_TIMEOUT_SLOW 30 
#define MEI_CLIENTS_INIT_TIMEOUT   15  

#define MEI_PGI_TIMEOUT             1  
#define MEI_D0I3_TIMEOUT            5  
#define MEI_HBM_TIMEOUT             1  
#define MEI_HBM_TIMEOUT_SLOW        5  

#define MKHI_RCV_TIMEOUT 500 
#define MKHI_RCV_TIMEOUT_SLOW 10000 


#define MEI_FW_PAGE_SIZE 4096UL


#define HBM_MINOR_VERSION                   2
#define HBM_MAJOR_VERSION                   2


#define HBM_MINOR_VERSION_PGI               1
#define HBM_MAJOR_VERSION_PGI               1


#define HBM_MINOR_VERSION_DC               0
#define HBM_MAJOR_VERSION_DC               2


#define HBM_MINOR_VERSION_IE               0
#define HBM_MAJOR_VERSION_IE               2


#define HBM_MINOR_VERSION_DOT              0
#define HBM_MAJOR_VERSION_DOT              2


#define HBM_MINOR_VERSION_EV               0
#define HBM_MAJOR_VERSION_EV               2


#define HBM_MINOR_VERSION_FA               0
#define HBM_MAJOR_VERSION_FA               2


#define HBM_MINOR_VERSION_OS               0
#define HBM_MAJOR_VERSION_OS               2


#define HBM_MINOR_VERSION_DR               1
#define HBM_MAJOR_VERSION_DR               2


#define HBM_MINOR_VERSION_VT               2
#define HBM_MAJOR_VERSION_VT               2


#define HBM_MINOR_VERSION_GSC              2
#define HBM_MAJOR_VERSION_GSC              2


#define HBM_MINOR_VERSION_CAP              2
#define HBM_MAJOR_VERSION_CAP              2


#define HBM_MINOR_VERSION_CD               2
#define HBM_MAJOR_VERSION_CD               2


#define MEI_HBM_CMD_OP_MSK                  0x7f

#define MEI_HBM_CMD_RES_MSK                 0x80


#define HOST_START_REQ_CMD                  0x01
#define HOST_START_RES_CMD                  0x81

#define HOST_STOP_REQ_CMD                   0x02
#define HOST_STOP_RES_CMD                   0x82

#define ME_STOP_REQ_CMD                     0x03

#define HOST_ENUM_REQ_CMD                   0x04
#define HOST_ENUM_RES_CMD                   0x84

#define HOST_CLIENT_PROPERTIES_REQ_CMD      0x05
#define HOST_CLIENT_PROPERTIES_RES_CMD      0x85

#define CLIENT_CONNECT_REQ_CMD              0x06
#define CLIENT_CONNECT_RES_CMD              0x86

#define CLIENT_DISCONNECT_REQ_CMD           0x07
#define CLIENT_DISCONNECT_RES_CMD           0x87

#define MEI_FLOW_CONTROL_CMD                0x08

#define MEI_PG_ISOLATION_ENTRY_REQ_CMD      0x0a
#define MEI_PG_ISOLATION_ENTRY_RES_CMD      0x8a
#define MEI_PG_ISOLATION_EXIT_REQ_CMD       0x0b
#define MEI_PG_ISOLATION_EXIT_RES_CMD       0x8b

#define MEI_HBM_ADD_CLIENT_REQ_CMD          0x0f
#define MEI_HBM_ADD_CLIENT_RES_CMD          0x8f

#define MEI_HBM_NOTIFY_REQ_CMD              0x10
#define MEI_HBM_NOTIFY_RES_CMD              0x90
#define MEI_HBM_NOTIFICATION_CMD            0x11

#define MEI_HBM_DMA_SETUP_REQ_CMD           0x12
#define MEI_HBM_DMA_SETUP_RES_CMD           0x92

#define MEI_HBM_CAPABILITIES_REQ_CMD        0x13
#define MEI_HBM_CAPABILITIES_RES_CMD        0x93

#define MEI_HBM_CLIENT_DMA_MAP_REQ_CMD      0x14
#define MEI_HBM_CLIENT_DMA_MAP_RES_CMD      0x94

#define MEI_HBM_CLIENT_DMA_UNMAP_REQ_CMD    0x15
#define MEI_HBM_CLIENT_DMA_UNMAP_RES_CMD    0x95


enum mei_stop_reason_types {
	DRIVER_STOP_REQUEST = 0x00,
	DEVICE_D1_ENTRY = 0x01,
	DEVICE_D2_ENTRY = 0x02,
	DEVICE_D3_ENTRY = 0x03,
	SYSTEM_S1_ENTRY = 0x04,
	SYSTEM_S2_ENTRY = 0x05,
	SYSTEM_S3_ENTRY = 0x06,
	SYSTEM_S4_ENTRY = 0x07,
	SYSTEM_S5_ENTRY = 0x08
};



enum mei_hbm_status {
	MEI_HBMS_SUCCESS           = 0,
	MEI_HBMS_CLIENT_NOT_FOUND  = 1,
	MEI_HBMS_ALREADY_EXISTS    = 2,
	MEI_HBMS_REJECTED          = 3,
	MEI_HBMS_INVALID_PARAMETER = 4,
	MEI_HBMS_NOT_ALLOWED       = 5,
	MEI_HBMS_ALREADY_STARTED   = 6,
	MEI_HBMS_NOT_STARTED       = 7,

	MEI_HBMS_MAX
};



enum mei_cl_connect_status {
	MEI_CL_CONN_SUCCESS          = MEI_HBMS_SUCCESS,
	MEI_CL_CONN_NOT_FOUND        = MEI_HBMS_CLIENT_NOT_FOUND,
	MEI_CL_CONN_ALREADY_STARTED  = MEI_HBMS_ALREADY_EXISTS,
	MEI_CL_CONN_OUT_OF_RESOURCES = MEI_HBMS_REJECTED,
	MEI_CL_CONN_MESSAGE_SMALL    = MEI_HBMS_INVALID_PARAMETER,
	MEI_CL_CONN_NOT_ALLOWED      = MEI_HBMS_NOT_ALLOWED,
};


enum mei_cl_disconnect_status {
	MEI_CL_DISCONN_SUCCESS = MEI_HBMS_SUCCESS
};


enum mei_ext_hdr_type {
	MEI_EXT_HDR_NONE = 0,
	MEI_EXT_HDR_VTAG = 1,
	MEI_EXT_HDR_GSC = 2,
};


struct mei_ext_hdr {
	u8 type;
	u8 length;
	u8 data[];
} __packed;


struct mei_ext_meta_hdr {
	u8 count;
	u8 size;
	u8 reserved[2];
	u8 hdrs[];
} __packed;


struct mei_ext_hdr_vtag {
	struct mei_ext_hdr hdr;
	u8 vtag;
	u8 reserved;
} __packed;



static inline struct mei_ext_hdr *mei_ext_begin(struct mei_ext_meta_hdr *meta)
{
	return (struct mei_ext_hdr *)meta->hdrs;
}


static inline bool mei_ext_last(struct mei_ext_meta_hdr *meta,
				struct mei_ext_hdr *ext)
{
	return (u8 *)ext >= (u8 *)meta + sizeof(*meta) + (meta->size * 4);
}

struct mei_gsc_sgl {
	u32 low;
	u32 high;
	u32 length;
} __packed;

#define GSC_HECI_MSG_KERNEL 0
#define GSC_HECI_MSG_USER   1

#define GSC_ADDRESS_TYPE_GTT   0
#define GSC_ADDRESS_TYPE_PPGTT 1
#define GSC_ADDRESS_TYPE_PHYSICAL_CONTINUOUS 2 
#define GSC_ADDRESS_TYPE_PHYSICAL_SGL 3


struct mei_ext_hdr_gsc_h2f {
	struct mei_ext_hdr hdr;
	u8                 client_id;
	u8                 addr_type;
	u32                fence_id;
	u8                 input_address_count;
	u8                 output_address_count;
	u8                 reserved[2];
	struct mei_gsc_sgl sgl[];
} __packed;


struct mei_ext_hdr_gsc_f2h {
	struct mei_ext_hdr hdr;
	u8                 client_id;
	u8                 reserved;
	u32                fence_id;
	u32                written;
} __packed;


static inline struct mei_ext_hdr *mei_ext_next(struct mei_ext_hdr *ext)
{
	return (struct mei_ext_hdr *)((u8 *)ext + (ext->length * 4));
}


static inline u32 mei_ext_hdr_len(const struct mei_ext_hdr *ext)
{
	if (!ext)
		return 0;

	return ext->length * sizeof(u32);
}


struct mei_msg_hdr {
	u32 me_addr:8;
	u32 host_addr:8;
	u32 length:9;
	u32 reserved:3;
	u32 extended:1;
	u32 dma_ring:1;
	u32 internal:1;
	u32 msg_complete:1;
	u32 extension[];
} __packed;


#define MEI_MSG_MAX_LEN_MASK GENMASK(9, 0)

struct mei_bus_message {
	u8 hbm_cmd;
	u8 data[];
} __packed;


struct mei_hbm_cl_cmd {
	u8 hbm_cmd;
	u8 me_addr;
	u8 host_addr;
	u8 data;
};

struct hbm_version {
	u8 minor_version;
	u8 major_version;
} __packed;

struct hbm_host_version_request {
	u8 hbm_cmd;
	u8 reserved;
	struct hbm_version host_version;
} __packed;

struct hbm_host_version_response {
	u8 hbm_cmd;
	u8 host_version_supported;
	struct hbm_version me_max_version;
} __packed;

struct hbm_host_stop_request {
	u8 hbm_cmd;
	u8 reason;
	u8 reserved[2];
} __packed;

struct hbm_host_stop_response {
	u8 hbm_cmd;
	u8 reserved[3];
} __packed;

struct hbm_me_stop_request {
	u8 hbm_cmd;
	u8 reason;
	u8 reserved[2];
} __packed;


enum hbm_host_enum_flags {
	MEI_HBM_ENUM_F_ALLOW_ADD = BIT(0),
	MEI_HBM_ENUM_F_IMMEDIATE_ENUM = BIT(1),
};


struct hbm_host_enum_request {
	u8 hbm_cmd;
	u8 flags;
	u8 reserved[2];
} __packed;

struct hbm_host_enum_response {
	u8 hbm_cmd;
	u8 reserved[3];
	u8 valid_addresses[32];
} __packed;


struct mei_client_properties {
	uuid_le protocol_name;
	u8 protocol_version;
	u8 max_number_of_connections;
	u8 fixed_address;
	u8 single_recv_buf:1;
	u8 vt_supported:1;
	u8 reserved:6;
	u32 max_msg_length;
} __packed;

struct hbm_props_request {
	u8 hbm_cmd;
	u8 me_addr;
	u8 reserved[2];
} __packed;

struct hbm_props_response {
	u8 hbm_cmd;
	u8 me_addr;
	u8 status;
	u8 reserved;
	struct mei_client_properties client_properties;
} __packed;


struct hbm_add_client_request {
	u8 hbm_cmd;
	u8 me_addr;
	u8 reserved[2];
	struct mei_client_properties client_properties;
} __packed;


struct hbm_add_client_response {
	u8 hbm_cmd;
	u8 me_addr;
	u8 status;
	u8 reserved;
} __packed;


struct hbm_power_gate {
	u8 hbm_cmd;
	u8 reserved[3];
} __packed;


struct hbm_client_connect_request {
	u8 hbm_cmd;
	u8 me_addr;
	u8 host_addr;
	u8 reserved;
} __packed;


struct hbm_client_connect_response {
	u8 hbm_cmd;
	u8 me_addr;
	u8 host_addr;
	u8 status;
} __packed;


#define MEI_FC_MESSAGE_RESERVED_LENGTH           5

struct hbm_flow_control {
	u8 hbm_cmd;
	u8 me_addr;
	u8 host_addr;
	u8 reserved[MEI_FC_MESSAGE_RESERVED_LENGTH];
} __packed;

#define MEI_HBM_NOTIFICATION_START 1
#define MEI_HBM_NOTIFICATION_STOP  0

struct hbm_notification_request {
	u8 hbm_cmd;
	u8 me_addr;
	u8 host_addr;
	u8 start;
} __packed;


struct hbm_notification_response {
	u8 hbm_cmd;
	u8 me_addr;
	u8 host_addr;
	u8 status;
	u8 start;
	u8 reserved[3];
} __packed;


struct hbm_notification {
	u8 hbm_cmd;
	u8 me_addr;
	u8 host_addr;
	u8 reserved;
} __packed;


struct hbm_dma_mem_dscr {
	u32 addr_hi;
	u32 addr_lo;
	u32 size;
} __packed;

enum {
	DMA_DSCR_HOST = 0,
	DMA_DSCR_DEVICE = 1,
	DMA_DSCR_CTRL = 2,
	DMA_DSCR_NUM,
};


struct hbm_dma_setup_request {
	u8 hbm_cmd;
	u8 reserved[3];
	struct hbm_dma_mem_dscr dma_dscr[DMA_DSCR_NUM];
} __packed;


struct hbm_dma_setup_response {
	u8 hbm_cmd;
	u8 status;
	u8 reserved[2];
} __packed;


struct hbm_dma_ring_ctrl {
	u32 hbuf_wr_idx;
	u32 reserved1;
	u32 hbuf_rd_idx;
	u32 reserved2;
	u32 dbuf_wr_idx;
	u32 reserved3;
	u32 dbuf_rd_idx;
	u32 reserved4;
} __packed;


#define HBM_CAP_VT BIT(0)


#define HBM_CAP_GSC BIT(1)


#define HBM_CAP_CD BIT(2)


struct hbm_capability_request {
	u8 hbm_cmd;
	u8 capability_requested[3];
} __packed;


struct hbm_capability_response {
	u8 hbm_cmd;
	u8 capability_granted[3];
} __packed;


struct hbm_client_dma_map_request {
	u8 hbm_cmd;
	u8 client_buffer_id;
	u8 reserved[2];
	u32 address_lsb;
	u32 address_msb;
	u32 size;
} __packed;


struct hbm_client_dma_unmap_request {
	u8 hbm_cmd;
	u8 status;
	u8 client_buffer_id;
	u8 reserved;
} __packed;


struct hbm_client_dma_response {
	u8 hbm_cmd;
	u8 status;
} __packed;

#endif
