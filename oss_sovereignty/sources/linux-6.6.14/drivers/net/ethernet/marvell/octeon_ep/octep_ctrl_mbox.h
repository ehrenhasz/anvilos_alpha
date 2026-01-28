

 #ifndef __OCTEP_CTRL_MBOX_H__
#define __OCTEP_CTRL_MBOX_H__



#define OCTEP_CTRL_MBOX_MAGIC_NUMBER			0xdeaddeadbeefbeefull


#define OCTEP_CTRL_MBOX_MSG_HDR_FLAG_REQ		BIT(0)

#define OCTEP_CTRL_MBOX_MSG_HDR_FLAG_RESP		BIT(1)

#define OCTEP_CTRL_MBOX_MSG_HDR_FLAG_NOTIFY		BIT(2)

#define OCTEP_CTRL_MBOX_MSG_HDR_FLAG_CUSTOM		BIT(3)

#define OCTEP_CTRL_MBOX_MSG_DESC_MAX			4

enum octep_ctrl_mbox_status {
	OCTEP_CTRL_MBOX_STATUS_INVALID = 0,
	OCTEP_CTRL_MBOX_STATUS_INIT,
	OCTEP_CTRL_MBOX_STATUS_READY,
	OCTEP_CTRL_MBOX_STATUS_UNINIT
};


union octep_ctrl_mbox_msg_hdr {
	u64 words[2];
	struct {
		
		u16 reserved1:15;
		
		u16 is_vf:1;
		
		u16 vf_idx;
		
		u32 sz;
		
		u32 flags;
		
		u16 msg_id;
		u16 reserved2;
	} s;
};


struct octep_ctrl_mbox_msg_buf {
	u32 reserved1;
	u16 reserved2;
	
	u16 sz;
	
	void *msg;
};


struct octep_ctrl_mbox_msg {
	
	union octep_ctrl_mbox_msg_hdr hdr;
	
	int sg_num;
	
	struct octep_ctrl_mbox_msg_buf sg_list[OCTEP_CTRL_MBOX_MSG_DESC_MAX];
};


struct octep_ctrl_mbox_q {
	
	u32 sz;
	
	u8 __iomem *hw_prod;
	
	u8 __iomem *hw_cons;
	
	u8 __iomem *hw_q;
};

struct octep_ctrl_mbox {
	
	u64 version;
	
	u32 barmem_sz;
	
	u8 __iomem *barmem;
	
	struct octep_ctrl_mbox_q h2fq;
	
	struct octep_ctrl_mbox_q f2hq;
	
	struct mutex h2fq_lock;
	
	struct mutex f2hq_lock;
	
	u32 min_fw_version;
	
	u32 max_fw_version;
};


int octep_ctrl_mbox_init(struct octep_ctrl_mbox *mbox);


int octep_ctrl_mbox_send(struct octep_ctrl_mbox *mbox, struct octep_ctrl_mbox_msg *msg);


int octep_ctrl_mbox_recv(struct octep_ctrl_mbox *mbox, struct octep_ctrl_mbox_msg *msg);


int octep_ctrl_mbox_uninit(struct octep_ctrl_mbox *mbox);

#endif 
