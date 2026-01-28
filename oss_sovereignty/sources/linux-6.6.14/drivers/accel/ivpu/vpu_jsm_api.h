



#ifndef VPU_JSM_API_H
#define VPU_JSM_API_H


#define VPU_JSM_API_VER_MAJOR 3


#define VPU_JSM_API_VER_MINOR 0


#define VPU_JSM_API_VER_PATCH 1


#define VPU_JSM_API_VER_INDEX 4


#define VPU_HWS_NUM_PRIORITY_BANDS 4


#define VPU_MAX_ENGINE_RESET_IMPACTED_CONTEXTS 3


#pragma pack(push, 1)


#define VPU_ENGINE_COMPUTE 0
#define VPU_ENGINE_COPY	   1
#define VPU_ENGINE_NB	   2


#define VPU_JSM_STATUS_SUCCESS				 0x0U
#define VPU_JSM_STATUS_PARSING_ERR			 0x1U
#define VPU_JSM_STATUS_PROCESSING_ERR			 0x2U
#define VPU_JSM_STATUS_PREEMPTED			 0x3U
#define VPU_JSM_STATUS_ABORTED				 0x4U
#define VPU_JSM_STATUS_USER_CTX_VIOL_ERR		 0x5U
#define VPU_JSM_STATUS_GLOBAL_CTX_VIOL_ERR		 0x6U
#define VPU_JSM_STATUS_MVNCI_WRONG_INPUT_FORMAT		 0x7U
#define VPU_JSM_STATUS_MVNCI_UNSUPPORTED_NETWORK_ELEMENT 0x8U
#define VPU_JSM_STATUS_MVNCI_INVALID_HANDLE		 0x9U
#define VPU_JSM_STATUS_MVNCI_OUT_OF_RESOURCES		 0xAU
#define VPU_JSM_STATUS_MVNCI_NOT_IMPLEMENTED		 0xBU
#define VPU_JSM_STATUS_MVNCI_INTERNAL_ERROR		 0xCU

#define VPU_JSM_STATUS_PREEMPTED_MID_INFERENCE		 0xDU


#define VPU_IPC_CHAN_ASYNC_CMD 0
#define VPU_IPC_CHAN_GEN_CMD   10
#define VPU_IPC_CHAN_JOB_RET   11


#define VPU_JOB_FLAGS_NULL_SUBMISSION_MASK 0x00000001


#define VPU_JOB_RESERVED_BYTES	     16

#define VPU_JOB_QUEUE_RESERVED_BYTES 52


#define VPU_TRACE_ENTITY_NAME_MAX_LEN 32


#define VPU_DYNDBG_CMD_MAX_LEN 96


struct vpu_job_queue_entry {
	u64 batch_buf_addr; 
	u32 job_id;	  
	u32 flags; 
	u64 root_page_table_addr; 
	u64 root_page_table_update_counter; 
	u64 preemption_buffer_address; 
	u64 preemption_buffer_size; 
	u8 reserved_0[VPU_JOB_RESERVED_BYTES];
};


struct vpu_job_queue_header {
	u32 engine_idx;
	u32 head;
	u32 tail;
	u8 reserved_0[VPU_JOB_QUEUE_RESERVED_BYTES];
};


struct vpu_job_queue {
	struct vpu_job_queue_header header;
	struct vpu_job_queue_entry job[];
};


enum vpu_trace_entity_type {
	
	VPU_TRACE_ENTITY_TYPE_DESTINATION = 1,
	
	VPU_TRACE_ENTITY_TYPE_HW_COMPONENT = 2,
};


enum vpu_ipc_msg_type {
	VPU_JSM_MSG_UNKNOWN = 0xFFFFFFFF,
	
	VPU_JSM_MSG_ASYNC_CMD = 0x1100,
	VPU_JSM_MSG_ENGINE_RESET = VPU_JSM_MSG_ASYNC_CMD,
	VPU_JSM_MSG_ENGINE_PREEMPT = 0x1101,
	VPU_JSM_MSG_REGISTER_DB = 0x1102,
	VPU_JSM_MSG_UNREGISTER_DB = 0x1103,
	VPU_JSM_MSG_QUERY_ENGINE_HB = 0x1104,
	VPU_JSM_MSG_GET_POWER_LEVEL_COUNT = 0x1105,
	VPU_JSM_MSG_GET_POWER_LEVEL = 0x1106,
	VPU_JSM_MSG_SET_POWER_LEVEL = 0x1107,
	
	VPU_JSM_MSG_METRIC_STREAMER_OPEN = 0x1108,
	
	VPU_JSM_MSG_METRIC_STREAMER_CLOSE = 0x1109,
	
	VPU_JSM_MSG_TRACE_SET_CONFIG = 0x110a,
	
	VPU_JSM_MSG_TRACE_GET_CONFIG = 0x110b,
	
	VPU_JSM_MSG_TRACE_GET_CAPABILITY = 0x110c,
	
	VPU_JSM_MSG_TRACE_GET_NAME = 0x110d,
	
	VPU_JSM_MSG_SSID_RELEASE = 0x110e,
	
	VPU_JSM_MSG_METRIC_STREAMER_START = 0x110f,
	
	VPU_JSM_MSG_METRIC_STREAMER_STOP = 0x1110,
	
	VPU_JSM_MSG_METRIC_STREAMER_UPDATE = 0x1111,
	
	VPU_JSM_MSG_METRIC_STREAMER_INFO = 0x1112,
	
	VPU_JSM_MSG_SET_PRIORITY_BAND_SETUP = 0x1113,
	
	VPU_JSM_MSG_CREATE_CMD_QUEUE = 0x1114,
	
	VPU_JSM_MSG_DESTROY_CMD_QUEUE = 0x1115,
	
	VPU_JSM_MSG_SET_CONTEXT_SCHED_PROPERTIES = 0x1116,
	
	VPU_JSM_MSG_HWS_REGISTER_DB = 0x1117,
	
	VPU_JSM_MSG_GENERAL_CMD = 0x1200,
	VPU_JSM_MSG_BLOB_DEINIT = VPU_JSM_MSG_GENERAL_CMD,
	
	VPU_JSM_MSG_DYNDBG_CONTROL = 0x1201,
	
	VPU_JSM_MSG_JOB_DONE = 0x2100,
	
	VPU_JSM_MSG_ASYNC_CMD_DONE = 0x2200,
	VPU_JSM_MSG_ENGINE_RESET_DONE = VPU_JSM_MSG_ASYNC_CMD_DONE,
	VPU_JSM_MSG_ENGINE_PREEMPT_DONE = 0x2201,
	VPU_JSM_MSG_REGISTER_DB_DONE = 0x2202,
	VPU_JSM_MSG_UNREGISTER_DB_DONE = 0x2203,
	VPU_JSM_MSG_QUERY_ENGINE_HB_DONE = 0x2204,
	VPU_JSM_MSG_GET_POWER_LEVEL_COUNT_DONE = 0x2205,
	VPU_JSM_MSG_GET_POWER_LEVEL_DONE = 0x2206,
	VPU_JSM_MSG_SET_POWER_LEVEL_DONE = 0x2207,
	
	VPU_JSM_MSG_METRIC_STREAMER_OPEN_DONE = 0x2208,
	
	VPU_JSM_MSG_METRIC_STREAMER_CLOSE_DONE = 0x2209,
	
	VPU_JSM_MSG_TRACE_SET_CONFIG_RSP = 0x220a,
	
	VPU_JSM_MSG_TRACE_GET_CONFIG_RSP = 0x220b,
	
	VPU_JSM_MSG_TRACE_GET_CAPABILITY_RSP = 0x220c,
	
	VPU_JSM_MSG_TRACE_GET_NAME_RSP = 0x220d,
	
	VPU_JSM_MSG_SSID_RELEASE_DONE = 0x220e,
	
	VPU_JSM_MSG_METRIC_STREAMER_START_DONE = 0x220f,
	
	VPU_JSM_MSG_METRIC_STREAMER_STOP_DONE = 0x2210,
	
	VPU_JSM_MSG_METRIC_STREAMER_UPDATE_DONE = 0x2211,
	
	VPU_JSM_MSG_METRIC_STREAMER_INFO_DONE = 0x2212,
	
	VPU_JSM_MSG_METRIC_STREAMER_NOTIFICATION = 0x2213,
	
	VPU_JSM_MSG_SET_PRIORITY_BAND_SETUP_RSP = 0x2214,
	
	VPU_JSM_MSG_CREATE_CMD_QUEUE_RSP = 0x2215,
	
	VPU_JSM_MSG_DESTROY_CMD_QUEUE_RSP = 0x2216,
	
	VPU_JSM_MSG_SET_CONTEXT_SCHED_PROPERTIES_RSP = 0x2217,
	
	VPU_JSM_MSG_GENERAL_CMD_DONE = 0x2300,
	VPU_JSM_MSG_BLOB_DEINIT_DONE = VPU_JSM_MSG_GENERAL_CMD_DONE,
	
	VPU_JSM_MSG_DYNDBG_CONTROL_RSP = 0x2301,
};

enum vpu_ipc_msg_status { VPU_JSM_MSG_FREE, VPU_JSM_MSG_ALLOCATED };


struct vpu_ipc_msg_payload_engine_reset {
	
	u32 engine_idx;
	
	u32 reserved_0;
};

struct vpu_ipc_msg_payload_engine_preempt {
	
	u32 engine_idx;
	
	u32 preempt_id;
};


struct vpu_ipc_msg_payload_register_db {
	
	u32 db_idx;
	
	u32 reserved_0;
	
	u64 jobq_base;
	
	u32 jobq_size;
	
	u32 host_ssid;
};


struct vpu_ipc_msg_payload_unregister_db {
	
	u32 db_idx;
	
	u32 reserved_0;
};

struct vpu_ipc_msg_payload_query_engine_hb {
	
	u32 engine_idx;
	
	u32 reserved_0;
};

struct vpu_ipc_msg_payload_power_level {
	
	u32 power_level;
	
	u32 reserved_0;
};

struct vpu_ipc_msg_payload_ssid_release {
	
	u32 host_ssid;
	
	u32 reserved_0;
};


struct vpu_jsm_metric_streamer_start {
	
	u64 metric_group_mask;
	
	u64 sampling_rate;
	
	u32 notify_sample_count;
	u32 reserved_0;
	
	u64 buffer_addr;
	u64 buffer_size;
	
	u64 next_buffer_addr;
	u64 next_buffer_size;
};


struct vpu_jsm_metric_streamer_stop {
	
	u64 metric_group_mask;
};


struct vpu_jsm_metric_streamer_update {
	
	u64 metric_group_mask;
	
	u64 buffer_addr;
	u64 buffer_size;
	
	u64 next_buffer_addr;
	u64 next_buffer_size;
};

struct vpu_ipc_msg_payload_blob_deinit {
	
	u64 blob_id;
};

struct vpu_ipc_msg_payload_job_done {
	
	u32 engine_idx;
	
	u32 db_idx;
	
	u32 job_id;
	
	u32 job_status;
	
	u32 host_ssid;
	
	u32 reserved_0;
	
	u64 cmdq_id;
};

struct vpu_jsm_engine_reset_context {
	
	u32 host_ssid;
	
	u32 reserved_0;
	
	u64 cmdq_id;
	
	u64 flags;
};

struct vpu_ipc_msg_payload_engine_reset_done {
	
	u32 engine_idx;
	
	u32 num_impacted_contexts;
	
	struct vpu_jsm_engine_reset_context
		impacted_contexts[VPU_MAX_ENGINE_RESET_IMPACTED_CONTEXTS];
};

struct vpu_ipc_msg_payload_engine_preempt_done {
	
	u32 engine_idx;
	
	u32 preempt_id;
};


struct vpu_ipc_msg_payload_register_db_done {
	
	u32 db_idx;
	
	u32 reserved_0;
};


struct vpu_ipc_msg_payload_unregister_db_done {
	
	u32 db_idx;
	
	u32 reserved_0;
};

struct vpu_ipc_msg_payload_query_engine_hb_done {
	
	u32 engine_idx;
	
	u32 reserved_0;
	
	u64 heartbeat;
};

struct vpu_ipc_msg_payload_get_power_level_count_done {
	
	u32 power_level_count;
	
	u32 reserved_0;
	
	u8 power_limit[16];
};

struct vpu_ipc_msg_payload_blob_deinit_done {
	
	u64 blob_id;
};


struct vpu_ipc_msg_payload_hws_priority_band_setup {
	
	u32 grace_period[VPU_HWS_NUM_PRIORITY_BANDS];
	
	u64 process_quantum[VPU_HWS_NUM_PRIORITY_BANDS];
	
	u64 process_grace_period[VPU_HWS_NUM_PRIORITY_BANDS];
	
	u32 normal_band_percentage;
	
	u32 reserved_0;
};


struct vpu_ipc_msg_payload_hws_create_cmdq {
	
	u64 process_id;
	
	u32 host_ssid;
	
	u32 reserved;
	
	u64 cmdq_id;
	
	u64 cmdq_base;
	
	u32 cmdq_size;
	
	u32 reserved_0;
};


struct vpu_ipc_msg_payload_hws_create_cmdq_rsp {
	
	u64 process_id;
	
	u32 host_ssid;
	
	u32 reserved;
	
	u64 cmdq_id;
};


struct vpu_ipc_msg_payload_hws_destroy_cmdq {
	
	u32 host_ssid;
	
	u32 reserved;
	
	u64 cmdq_id;
};


struct vpu_ipc_msg_payload_hws_set_context_sched_properties {
	
	u32 host_ssid;
	
	u32 reserved_0;
	
	u64 cmdq_id;
	
	u32 priority_band;
	
	u32 realtime_priority_level;
	
	u32 in_process_priority;
	
	u32 reserved_1;
	
	u64 context_quantum;
	
	u64 grace_period_same_priority;
	
	u64 grace_period_lower_priority;
};


struct vpu_jsm_hws_register_db {
	
	u32 db_id;
	
	u32 host_ssid;
	
	u64 cmdq_id;
	
	u64 cmdq_base;
	
	u64 cmdq_size;
};


struct vpu_ipc_msg_payload_trace_config {
	
	u32 trace_level;
	
	u32 trace_destination_mask;
	
	u64 trace_hw_component_mask;
	u64 reserved_0; 
};


struct vpu_ipc_msg_payload_trace_capability_rsp {
	u32 trace_destination_mask; 
	u32 reserved_0;
	u64 trace_hw_component_mask; 
	u64 reserved_1; 
};


struct vpu_ipc_msg_payload_trace_get_name {
	
	u32 entity_type;
	u32 reserved_0;
	
	u64 entity_id;
};


struct vpu_ipc_msg_payload_trace_get_name_rsp {
	
	u32 entity_type;
	u32 reserved_0;
	
	u64 entity_id;
	
	u64 reserved_1;
	
	char entity_name[VPU_TRACE_ENTITY_NAME_MAX_LEN];
};


struct vpu_jsm_metric_streamer_done {
	
	u64 metric_group_mask;
	
	u32 sample_size;
	u32 reserved_0;
	
	u32 samples_collected;
	
	u32 samples_dropped;
	
	u64 buffer_addr;
	
	u64 bytes_written;
};


struct vpu_jsm_metric_group_descriptor {
	
	u32 next_metric_group_info_offset;
	
	u32 next_metric_counter_info_offset;
	
	u32 group_id;
	
	u32 num_counters;
	
	u32 metric_group_data_size;
	
	u32 domain;
	
	u32 name_string_size;
	
	u32 description_string_size;
	u64 reserved_0;
	
};


struct vpu_jsm_metric_counter_descriptor {
	
	u32 next_metric_counter_info_offset;
	
	u32 metric_data_offset;
	
	u32 metric_data_size;
	
	u32 tier;
	
	u32 metric_type;
	
	u32 metric_value_type;
	
	u32 name_string_size;
	
	u32 description_string_size;
	
	u32 component_string_size;
	
	u32 units_string_size;
	u64 reserved_0;
	
};


struct vpu_ipc_msg_payload_dyndbg_control {
	
	char dyndbg_cmd[VPU_DYNDBG_CMD_MAX_LEN];
};


union vpu_ipc_msg_payload {
	struct vpu_ipc_msg_payload_engine_reset engine_reset;
	struct vpu_ipc_msg_payload_engine_preempt engine_preempt;
	struct vpu_ipc_msg_payload_register_db register_db;
	struct vpu_ipc_msg_payload_unregister_db unregister_db;
	struct vpu_ipc_msg_payload_query_engine_hb query_engine_hb;
	struct vpu_ipc_msg_payload_power_level power_level;
	struct vpu_jsm_metric_streamer_start metric_streamer_start;
	struct vpu_jsm_metric_streamer_stop metric_streamer_stop;
	struct vpu_jsm_metric_streamer_update metric_streamer_update;
	struct vpu_ipc_msg_payload_blob_deinit blob_deinit;
	struct vpu_ipc_msg_payload_ssid_release ssid_release;
	struct vpu_jsm_hws_register_db hws_register_db;
	struct vpu_ipc_msg_payload_job_done job_done;
	struct vpu_ipc_msg_payload_engine_reset_done engine_reset_done;
	struct vpu_ipc_msg_payload_engine_preempt_done engine_preempt_done;
	struct vpu_ipc_msg_payload_register_db_done register_db_done;
	struct vpu_ipc_msg_payload_unregister_db_done unregister_db_done;
	struct vpu_ipc_msg_payload_query_engine_hb_done query_engine_hb_done;
	struct vpu_ipc_msg_payload_get_power_level_count_done get_power_level_count_done;
	struct vpu_jsm_metric_streamer_done metric_streamer_done;
	struct vpu_ipc_msg_payload_blob_deinit_done blob_deinit_done;
	struct vpu_ipc_msg_payload_trace_config trace_config;
	struct vpu_ipc_msg_payload_trace_capability_rsp trace_capability;
	struct vpu_ipc_msg_payload_trace_get_name trace_get_name;
	struct vpu_ipc_msg_payload_trace_get_name_rsp trace_get_name_rsp;
	struct vpu_ipc_msg_payload_dyndbg_control dyndbg_control;
	struct vpu_ipc_msg_payload_hws_priority_band_setup hws_priority_band_setup;
	struct vpu_ipc_msg_payload_hws_create_cmdq hws_create_cmdq;
	struct vpu_ipc_msg_payload_hws_create_cmdq_rsp hws_create_cmdq_rsp;
	struct vpu_ipc_msg_payload_hws_destroy_cmdq hws_destroy_cmdq;
	struct vpu_ipc_msg_payload_hws_set_context_sched_properties
		hws_set_context_sched_properties;
};


struct vpu_jsm_msg {
	
	u64 reserved_0;
	
	u32 type;
	
	u32 status;
	
	u32 request_id;
	
	u32 result;
	u64 reserved_1;
	
	union vpu_ipc_msg_payload payload;
};

#pragma pack(pop)

#endif


