#ifndef BCM_VK_MSG_H
#define BCM_VK_MSG_H
#include <uapi/linux/misc/bcm_vk.h>
#include "bcm_vk_sg.h"
struct bcm_vk_msgq {
	u16 type;	 
	u16 num;	 
	u32 start;	 
	u32 rd_idx;  
	u32 wr_idx;  
	u32 size;	 
	u32 nxt;	 
#define DB_SHIFT 16
	u32 db_offset;  
	u32 rsvd;
};
struct bcm_vk_sync_qinfo {
	void __iomem *q_start;
	u32 q_size;
	u32 q_mask;
	u32 q_low;
	u32 q_db_offset;
};
#define VK_MSGQ_MAX_NR 4  
struct vk_msg_blk {
	u8 function_id;
#define VK_FID_TRANS_BUF	5
#define VK_FID_SHUTDOWN		8
#define VK_FID_INIT		9
	u8 size;  
	u16 trans_id;  
	u32 context_id;
#define VK_NEW_CTX		0
	u32 cmd;
#define VK_CMD_PLANES_MASK	0x000f  
#define VK_CMD_UPLOAD		0x0400  
#define VK_CMD_DOWNLOAD		0x0500  
#define VK_CMD_MASK		0x0f00  
	u32 arg;
};
#define VK_MSGQ_BLK_SIZE   (sizeof(struct vk_msg_blk))
#define VK_MSGQ_BLK_SZ_SHIFT 4
#define VK_SIMPLEX_MSG_ID 0
struct bcm_vk_ctx {
	struct list_head node;  
	unsigned int idx;
	bool in_use;
	pid_t pid;
	u32 hash_idx;
	u32 q_num;  
	struct miscdevice *miscdev;
	atomic_t pend_cnt;  
	atomic_t dma_cnt;  
	wait_queue_head_t rd_wq;
};
struct bcm_vk_ht_entry {
	struct list_head head;
};
#define VK_DMA_MAX_ADDRS 4  
struct bcm_vk_wkent {
	struct list_head node;  
	struct bcm_vk_ctx *ctx;
	struct bcm_vk_dma dma[VK_DMA_MAX_ADDRS];
	u32 to_h_blks;  
	struct vk_msg_blk *to_h_msg;
	u32 usr_msg_id;
	u32 to_v_blks;
	u32 seq_num;
	struct vk_msg_blk to_v_msg[];
};
struct bcm_vk_qs_cnts {
	u32 cnt;  
	u32 acc_sum;
	u32 max_occ;  
	u32 max_abs;  
};
struct bcm_vk_msg_chan {
	u32 q_nr;
	struct mutex msgq_mutex;
	struct bcm_vk_msgq __iomem *msgq[VK_MSGQ_MAX_NR];
	spinlock_t pendq_lock;
	struct list_head pendq[VK_MSGQ_MAX_NR];
	struct bcm_vk_sync_qinfo sync_qinfo[VK_MSGQ_MAX_NR];
};
#define VK_MSGQ_PER_CHAN_MAX	3
#define VK_MSGQ_NUM_DEFAULT	(VK_MSGQ_PER_CHAN_MAX - 1)
#define VK_CMPT_CTX_MAX		(32 * 5)
#define VK_PID_HT_SHIFT_BIT	7  
#define VK_PID_HT_SZ		BIT(VK_PID_HT_SHIFT_BIT)
#define VK_BAR0_SEG_SIZE	(4 * SZ_1K)  
#define VK_SHUTDOWN_PID		1
#define VK_SHUTDOWN_GRACEFUL	2
#endif
