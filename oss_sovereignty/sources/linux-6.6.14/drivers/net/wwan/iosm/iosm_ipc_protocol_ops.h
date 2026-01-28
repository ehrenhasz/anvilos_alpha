

#ifndef IOSM_IPC_PROTOCOL_OPS_H
#define IOSM_IPC_PROTOCOL_OPS_H

#define SIZE_MASK 0x00FFFFFF
#define COMPLETION_STATUS 24
#define RESET_BIT 7


enum ipc_mem_td_cs {
	IPC_MEM_TD_CS_INVALID,
	IPC_MEM_TD_CS_PARTIAL_TRANSFER,
	IPC_MEM_TD_CS_END_TRANSFER,
	IPC_MEM_TD_CS_OVERFLOW,
	IPC_MEM_TD_CS_ABORT,
	IPC_MEM_TD_CS_ERROR,
};


enum ipc_mem_msg_cs {
	IPC_MEM_MSG_CS_INVALID,
	IPC_MEM_MSG_CS_SUCCESS,
	IPC_MEM_MSG_CS_ERROR,
};


struct ipc_msg_prep_args_pipe {
	struct ipc_pipe *pipe;
};


struct ipc_msg_prep_args_sleep {
	unsigned int target;
	unsigned int state;
};


struct ipc_msg_prep_feature_set {
	u8 reset_enable;
};


struct ipc_msg_prep_map {
	unsigned int region_id;
	unsigned long addr;
	size_t size;
};


struct ipc_msg_prep_unmap {
	unsigned int region_id;
};


union ipc_msg_prep_args {
	struct ipc_msg_prep_args_pipe pipe_open;
	struct ipc_msg_prep_args_pipe pipe_close;
	struct ipc_msg_prep_args_sleep sleep;
	struct ipc_msg_prep_feature_set feature_set;
	struct ipc_msg_prep_map map;
	struct ipc_msg_prep_unmap unmap;
};


enum ipc_msg_prep_type {
	IPC_MSG_PREP_SLEEP,
	IPC_MSG_PREP_PIPE_OPEN,
	IPC_MSG_PREP_PIPE_CLOSE,
	IPC_MSG_PREP_FEATURE_SET,
	IPC_MSG_PREP_MAP,
	IPC_MSG_PREP_UNMAP,
};


struct ipc_rsp {
	struct completion completion;
	enum ipc_mem_msg_cs status;
};


enum ipc_mem_msg {
	IPC_MEM_MSG_OPEN_PIPE = 0x01,
	IPC_MEM_MSG_CLOSE_PIPE = 0x02,
	IPC_MEM_MSG_ABORT_PIPE = 0x03,
	IPC_MEM_MSG_SLEEP = 0x04,
	IPC_MEM_MSG_FEATURE_SET = 0xF0,
};


struct ipc_mem_msg_open_pipe {
	__le64 tdr_addr;
	__le16 tdr_entries;
	u8 pipe_nr;
	u8 type_of_message;
	__le32 irq_vector;
	__le32 accumulation_backoff;
	__le32 completion_status;
};


struct ipc_mem_msg_close_pipe {
	__le32 reserved1[2];
	__le16 reserved2;
	u8 pipe_nr;
	u8 type_of_message;
	__le32  reserved3;
	__le32 reserved4;
	__le32 completion_status;
};


struct ipc_mem_msg_abort_pipe {
	__le32  reserved1[2];
	__le16 reserved2;
	u8 pipe_nr;
	u8 type_of_message;
	__le32 reserved3;
	__le32 reserved4;
	__le32 completion_status;
};


struct ipc_mem_msg_host_sleep {
	__le32 reserved1[2];
	u8 target;
	u8 state;
	u8 reserved2;
	u8 type_of_message;
	__le32 reserved3;
	__le32 reserved4;
	__le32 completion_status;
};


struct ipc_mem_msg_feature_set {
	__le32 reserved1[2];
	__le16 reserved2;
	u8 reset_enable;
	u8 type_of_message;
	__le32 reserved3;
	__le32 reserved4;
	__le32 completion_status;
};


struct ipc_mem_msg_common {
	__le32 reserved1[2];
	u8 reserved2[3];
	u8 type_of_message;
	__le32 reserved3;
	__le32 reserved4;
	__le32 completion_status;
};


union ipc_mem_msg_entry {
	struct ipc_mem_msg_open_pipe open_pipe;
	struct ipc_mem_msg_close_pipe close_pipe;
	struct ipc_mem_msg_abort_pipe abort_pipe;
	struct ipc_mem_msg_host_sleep host_sleep;
	struct ipc_mem_msg_feature_set feature_set;
	struct ipc_mem_msg_common common;
};


struct ipc_protocol_td {
	union {
		
		dma_addr_t address;
		struct {
			
			__le32 address;
			
			__le32 desc;
		} __packed shm;
	} buffer;

	
	__le32 scs;

	
	__le32 next;
} __packed;


int ipc_protocol_msg_prep(struct iosm_imem *ipc_imem,
			  enum ipc_msg_prep_type msg_type,
			  union ipc_msg_prep_args *args);


void ipc_protocol_msg_hp_update(struct iosm_imem *ipc_imem);


bool ipc_protocol_msg_process(struct iosm_imem *ipc_imem, int irq);


bool ipc_protocol_ul_td_send(struct iosm_protocol *ipc_protocol,
			     struct ipc_pipe *pipe,
			     struct sk_buff_head *p_ul_list);


struct sk_buff *ipc_protocol_ul_td_process(struct iosm_protocol *ipc_protocol,
					   struct ipc_pipe *pipe);


bool ipc_protocol_dl_td_prepare(struct iosm_protocol *ipc_protocol,
				struct ipc_pipe *pipe);


struct sk_buff *ipc_protocol_dl_td_process(struct iosm_protocol *ipc_protocol,
					   struct ipc_pipe *pipe);


void ipc_protocol_get_head_tail_index(struct iosm_protocol *ipc_protocol,
				      struct ipc_pipe *pipe, u32 *head,
				      u32 *tail);

enum ipc_mem_device_ipc_state ipc_protocol_get_ipc_status(struct iosm_protocol
							  *ipc_protocol);


void ipc_protocol_pipe_cleanup(struct iosm_protocol *ipc_protocol,
			       struct ipc_pipe *pipe);


enum ipc_mem_exec_stage
ipc_protocol_get_ap_exec_stage(struct iosm_protocol *ipc_protocol);


u32 ipc_protocol_pm_dev_get_sleep_notification(struct iosm_protocol
					       *ipc_protocol);
#endif
