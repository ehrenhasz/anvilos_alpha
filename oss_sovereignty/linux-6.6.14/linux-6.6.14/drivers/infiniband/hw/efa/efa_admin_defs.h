#ifndef _EFA_ADMIN_H_
#define _EFA_ADMIN_H_
enum efa_admin_aq_completion_status {
	EFA_ADMIN_SUCCESS                           = 0,
	EFA_ADMIN_RESOURCE_ALLOCATION_FAILURE       = 1,
	EFA_ADMIN_BAD_OPCODE                        = 2,
	EFA_ADMIN_UNSUPPORTED_OPCODE                = 3,
	EFA_ADMIN_MALFORMED_REQUEST                 = 4,
	EFA_ADMIN_ILLEGAL_PARAMETER                 = 5,
	EFA_ADMIN_UNKNOWN_ERROR                     = 6,
	EFA_ADMIN_RESOURCE_BUSY                     = 7,
};
struct efa_admin_aq_common_desc {
	u16 command_id;
	u8 opcode;
	u8 flags;
};
struct efa_admin_ctrl_buff_info {
	u32 length;
	struct efa_common_mem_addr address;
};
struct efa_admin_aq_entry {
	struct efa_admin_aq_common_desc aq_common_descriptor;
	union {
		u32 inline_data_w1[3];
		struct efa_admin_ctrl_buff_info control_buffer;
	} u;
	u32 inline_data_w4[12];
};
struct efa_admin_acq_common_desc {
	u16 command;
	u8 status;
	u8 flags;
	u16 extended_status;
	u16 sq_head_indx;
};
struct efa_admin_acq_entry {
	struct efa_admin_acq_common_desc acq_common_descriptor;
	u32 response_specific_data[14];
};
struct efa_admin_aenq_common_desc {
	u16 group;
	u16 syndrom;
	u8 flags;
	u8 reserved1[3];
	u32 timestamp_low;
	u32 timestamp_high;
};
struct efa_admin_aenq_entry {
	struct efa_admin_aenq_common_desc aenq_common_desc;
	u32 inline_data_w4[12];
};
enum efa_admin_eqe_event_type {
	EFA_ADMIN_EQE_EVENT_TYPE_COMPLETION         = 0,
};
struct efa_admin_comp_event {
	u16 cqn;
	u16 reserved;
	u32 reserved2;
};
struct efa_admin_eqe {
	u32 common;
	u32 reserved;
	union {
		u32 event_data[2];
		struct efa_admin_comp_event comp_event;
	} u;
};
#define EFA_ADMIN_AQ_COMMON_DESC_COMMAND_ID_MASK            GENMASK(11, 0)
#define EFA_ADMIN_AQ_COMMON_DESC_PHASE_MASK                 BIT(0)
#define EFA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_MASK             BIT(1)
#define EFA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_MASK    BIT(2)
#define EFA_ADMIN_ACQ_COMMON_DESC_COMMAND_ID_MASK           GENMASK(11, 0)
#define EFA_ADMIN_ACQ_COMMON_DESC_PHASE_MASK                BIT(0)
#define EFA_ADMIN_AENQ_COMMON_DESC_PHASE_MASK               BIT(0)
#define EFA_ADMIN_EQE_PHASE_MASK                            BIT(0)
#define EFA_ADMIN_EQE_EVENT_TYPE_MASK                       GENMASK(8, 1)
#endif  
