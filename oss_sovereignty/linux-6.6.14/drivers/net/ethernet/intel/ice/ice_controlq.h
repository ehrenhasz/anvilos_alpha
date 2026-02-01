 
 

#ifndef _ICE_CONTROLQ_H_
#define _ICE_CONTROLQ_H_

#include "ice_adminq_cmd.h"

 
#define ICE_AQ_MAX_BUF_LEN 4096
#define ICE_MBXQ_MAX_BUF_LEN 4096
#define ICE_SBQ_MAX_BUF_LEN 512

#define ICE_CTL_Q_DESC(R, i) \
	(&(((struct ice_aq_desc *)((R).desc_buf.va))[i]))

#define ICE_CTL_Q_DESC_UNUSED(R) \
	((u16)((((R)->next_to_clean > (R)->next_to_use) ? 0 : (R)->count) + \
	       (R)->next_to_clean - (R)->next_to_use - 1))

 
#define EXP_FW_API_VER_BRANCH		0x00
#define EXP_FW_API_VER_MAJOR		0x01
#define EXP_FW_API_VER_MINOR		0x05

 
enum ice_ctl_q {
	ICE_CTL_Q_UNKNOWN = 0,
	ICE_CTL_Q_ADMIN,
	ICE_CTL_Q_MAILBOX,
	ICE_CTL_Q_SB,
};

 
#define ICE_CTL_Q_SQ_CMD_TIMEOUT	HZ     
#define ICE_CTL_Q_ADMIN_INIT_TIMEOUT	10     
#define ICE_CTL_Q_ADMIN_INIT_MSEC	100    

struct ice_ctl_q_ring {
	void *dma_head;			 
	struct ice_dma_mem desc_buf;	 
	void *cmd_buf;			 

	union {
		struct ice_dma_mem *sq_bi;
		struct ice_dma_mem *rq_bi;
	} r;

	u16 count;		 

	 
	u16 next_to_use;
	u16 next_to_clean;

	 
	u32 head;
	u32 tail;
	u32 len;
	u32 bah;
	u32 bal;
	u32 len_mask;
	u32 len_ena_mask;
	u32 len_crit_mask;
	u32 head_mask;
};

 
struct ice_sq_cd {
	struct ice_aq_desc *wb_desc;
};

#define ICE_CTL_Q_DETAILS(R, i) (&(((struct ice_sq_cd *)((R).cmd_buf))[i]))

 
struct ice_rq_event_info {
	struct ice_aq_desc desc;
	u16 msg_len;
	u16 buf_len;
	u8 *msg_buf;
};

 
struct ice_ctl_q_info {
	enum ice_ctl_q qtype;
	struct ice_ctl_q_ring rq;	 
	struct ice_ctl_q_ring sq;	 
	u16 num_rq_entries;		 
	u16 num_sq_entries;		 
	u16 rq_buf_size;		 
	u16 sq_buf_size;		 
	enum ice_aq_err sq_last_status;	 
	struct mutex sq_lock;		 
	struct mutex rq_lock;		 
};

#endif  
