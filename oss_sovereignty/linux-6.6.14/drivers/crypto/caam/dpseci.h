 
 
#ifndef _DPSECI_H_
#define _DPSECI_H_

 

struct fsl_mc_io;

 

 
#define DPSECI_MAX_QUEUE_NUM		16

 
#define DPSECI_ALL_QUEUES	(u8)(-1)

int dpseci_open(struct fsl_mc_io *mc_io, u32 cmd_flags, int dpseci_id,
		u16 *token);

int dpseci_close(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token);

 
#define DPSECI_OPT_HAS_CG		0x000020

 
struct dpseci_cfg {
	u32 options;
	u8 num_tx_queues;
	u8 num_rx_queues;
	u8 priorities[DPSECI_MAX_QUEUE_NUM];
};

int dpseci_enable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token);

int dpseci_disable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token);

int dpseci_reset(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token);

int dpseci_is_enabled(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		      int *en);

 
struct dpseci_attr {
	int id;
	u8 num_tx_queues;
	u8 num_rx_queues;
	u32 options;
};

int dpseci_get_attributes(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			  struct dpseci_attr *attr);

 
enum dpseci_dest {
	DPSECI_DEST_NONE = 0,
	DPSECI_DEST_DPIO,
	DPSECI_DEST_DPCON
};

 
struct dpseci_dest_cfg {
	enum dpseci_dest dest_type;
	int dest_id;
	u8 priority;
};

 

 
#define DPSECI_QUEUE_OPT_USER_CTX		0x00000001

 
#define DPSECI_QUEUE_OPT_DEST			0x00000002

 
#define DPSECI_QUEUE_OPT_ORDER_PRESERVATION	0x00000004

 
struct dpseci_rx_queue_cfg {
	u32 options;
	int order_preservation_en;
	u64 user_ctx;
	struct dpseci_dest_cfg dest_cfg;
};

int dpseci_set_rx_queue(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			u8 queue, const struct dpseci_rx_queue_cfg *cfg);

 
struct dpseci_rx_queue_attr {
	u64 user_ctx;
	int order_preservation_en;
	struct dpseci_dest_cfg dest_cfg;
	u32 fqid;
};

int dpseci_get_rx_queue(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			u8 queue, struct dpseci_rx_queue_attr *attr);

 
struct dpseci_tx_queue_attr {
	u32 fqid;
	u8 priority;
};

int dpseci_get_tx_queue(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			u8 queue, struct dpseci_tx_queue_attr *attr);

 
struct dpseci_sec_attr {
	u16 ip_id;
	u8 major_rev;
	u8 minor_rev;
	u8 era;
	u8 deco_num;
	u8 zuc_auth_acc_num;
	u8 zuc_enc_acc_num;
	u8 snow_f8_acc_num;
	u8 snow_f9_acc_num;
	u8 crc_acc_num;
	u8 pk_acc_num;
	u8 kasumi_acc_num;
	u8 rng_acc_num;
	u8 md_acc_num;
	u8 arc4_acc_num;
	u8 des_acc_num;
	u8 aes_acc_num;
	u8 ccha_acc_num;
	u8 ptha_acc_num;
};

int dpseci_get_sec_attr(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			struct dpseci_sec_attr *attr);

int dpseci_get_api_version(struct fsl_mc_io *mc_io, u32 cmd_flags,
			   u16 *major_ver, u16 *minor_ver);

 
enum dpseci_congestion_unit {
	DPSECI_CONGESTION_UNIT_BYTES = 0,
	DPSECI_CONGESTION_UNIT_FRAMES
};

 
#define DPSECI_CGN_MODE_WRITE_MEM_ON_ENTER		0x00000001

 
#define DPSECI_CGN_MODE_WRITE_MEM_ON_EXIT		0x00000002

 
#define DPSECI_CGN_MODE_COHERENT_WRITE			0x00000004

 
#define DPSECI_CGN_MODE_NOTIFY_DEST_ON_ENTER		0x00000008

 
#define DPSECI_CGN_MODE_NOTIFY_DEST_ON_EXIT		0x00000010

 
#define DPSECI_CGN_MODE_INTR_COALESCING_DISABLED	0x00000020

 
struct dpseci_congestion_notification_cfg {
	enum dpseci_congestion_unit units;
	u32 threshold_entry;
	u32 threshold_exit;
	u64 message_ctx;
	u64 message_iova;
	struct dpseci_dest_cfg dest_cfg;
	u16 notification_mode;
};

int dpseci_set_congestion_notification(struct fsl_mc_io *mc_io, u32 cmd_flags,
	u16 token, const struct dpseci_congestion_notification_cfg *cfg);

int dpseci_get_congestion_notification(struct fsl_mc_io *mc_io, u32 cmd_flags,
	u16 token, struct dpseci_congestion_notification_cfg *cfg);

#endif  
