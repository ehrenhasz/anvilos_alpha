 
 

#ifndef __H_IBMVSCSI_TGT
#define __H_IBMVSCSI_TGT

#include <linux/interrupt.h>
#include "libsrp.h"

#define SYS_ID_NAME_LEN		64
#define PARTITION_NAMELEN	96
#define IBMVSCSIS_NAMELEN       32

#define MSG_HI  0
#define MSG_LOW 1

#define MAX_CMD_Q_PAGES       4
#define CRQ_PER_PAGE          (PAGE_SIZE / sizeof(struct viosrp_crq))
 
#define DEFAULT_CMD_Q_SIZE    CRQ_PER_PAGE
#define MAX_CMD_Q_SIZE        (DEFAULT_CMD_Q_SIZE * MAX_CMD_Q_PAGES)

#define SRP_VIOLATION           0x102   

 
#define SUPPORTED_FORMATS  ((SRP_DATA_DESC_DIRECT << 1) | \
			    (SRP_DATA_DESC_INDIRECT << 1))

#define SCSI_LUN_ADDR_METHOD_FLAT	1

struct dma_window {
	u32 liobn;	 
	u64 tce_base;	 
	u64 tce_size;	 
};

struct target_dds {
	u64 unit_id;                 
#define NUM_DMA_WINDOWS 2
#define LOCAL  0
#define REMOTE 1
	struct dma_window  window[NUM_DMA_WINDOWS];

	 
	uint partition_num;
	char partition_name[PARTITION_NAMELEN];
};

#define MAX_NUM_PORTS        1
#define MAX_H_COPY_RDMA      (128 * 1024)

#define MAX_EYE   64

 
#define ADAPT_SUCCESS            0L
 
#define ERROR                   -40L

struct format_code {
	u8 reserved;
	u8 buffers;
};

struct client_info {
#define SRP_VERSION "16.a"
	char srp_version[8];
	 
	char partition_name[PARTITION_NAMELEN];
	 
	u32 partition_number;
	 
	u32 mad_version;
	u32 os_type;
};

 
#define SECONDS_TO_CONSIDER_FAILED 30
 
#define WAIT_SECONDS 1
#define WAIT_NANO_SECONDS 5000
#define MAX_TIMER_POPS ((1000000 / WAIT_NANO_SECONDS) * \
			SECONDS_TO_CONSIDER_FAILED)
 
struct timer_cb {
	struct hrtimer timer;
	 
	int timer_pops;
	 
	bool started;
};

struct cmd_queue {
	 
	struct viosrp_crq *base_addr;
	dma_addr_t crq_token;
	 
	uint mask;
	 
	uint index;
	int size;
};

#define SCSOLNT_RESP_SHIFT	1
#define UCSOLNT_RESP_SHIFT	2

#define SCSOLNT         BIT(SCSOLNT_RESP_SHIFT)
#define UCSOLNT         BIT(UCSOLNT_RESP_SHIFT)

enum cmd_type {
	SCSI_CDB	= 0x01,
	TASK_MANAGEMENT	= 0x02,
	 
	ADAPTER_MAD	= 0x04,
	UNSET_TYPE	= 0x08,
};

struct iu_rsp {
	u8 format;
	u8 sol_not;
	u16 len;
	 
	u64 tag;
};

struct ibmvscsis_cmd {
	struct list_head list;
	 
	struct se_cmd se_cmd;
	struct iu_entry *iue;
	struct iu_rsp rsp;
	struct work_struct work;
	struct scsi_info *adapter;
	struct ibmvscsis_cmd *abort_cmd;
	 
	unsigned char sense_buf[TRANSPORT_SENSE_BUFFER];
	u64 init_time;
#define CMD_FAST_FAIL	BIT(0)
#define DELAY_SEND	BIT(1)
	u32 flags;
	char type;
};

struct ibmvscsis_nexus {
	struct se_session *se_sess;
};

struct ibmvscsis_tport {
	 
	u8 tport_proto_id;
	 
	char tport_name[IBMVSCSIS_NAMELEN];
	 
	struct se_wwn tport_wwn;
	 
	struct se_portal_group se_tpg;
	 
	u16 tport_tpgt;
	 
	struct ibmvscsis_nexus *ibmv_nexus;
	bool enabled;
	bool releasing;
};

struct scsi_info {
	struct list_head list;
	char eye[MAX_EYE];

	 
	struct list_head waiting_rsp;
#define NO_QUEUE                    0x00
#define WAIT_ENABLED                0X01
#define WAIT_CONNECTION             0x04
	 
#define CONNECTED                   0x08
	 
#define SRP_PROCESSING              0x10
	 
#define UNCONFIGURING               0x20
	 
#define WAIT_IDLE                   0x40
	 
#define ERR_DISCONNECT              0x80
	 
#define ERR_DISCONNECT_RECONNECT    0x100
	 
#define ERR_DISCONNECTED            0x200
	 
#define UNDEFINED                   0x400
	u16  state;
	int fast_fail;
	struct target_dds dds;
	char *cmd_pool;
	 
	struct list_head free_cmd;
	 
	struct list_head schedule_q;
	 
	struct list_head active_q;
	caddr_t *map_buf;
	 
	dma_addr_t map_ioba;
	 
	int request_limit;
	 
	int credit;
	 
	int debit;

	 
#define PROCESSING_MAD                0x00002
	 
#define WAIT_FOR_IDLE		      0x00004
	 
#define CRQ_CLOSED                    0x00010
	 
#define CLIENT_FAILED                 0x00040
	 
#define TRANS_EVENT                   0x00080
	 
#define RESPONSE_Q_DOWN               0x00100
	 
#define SCHEDULE_DISCONNECT           0x00400
	 
#define DISCONNECT_SCHEDULED          0x00800
	 
#define CFG_SLEEPING                  0x01000
	 
#define PREP_FOR_SUSPEND_ENABLED      0x02000
	 
#define PREP_FOR_SUSPEND_PENDING      0x04000
	 
#define PREP_FOR_SUSPEND_ABORTED      0x08000
	 
#define PREP_FOR_SUSPEND_OVERWRITE    0x10000
	u32 flags;
	 
	spinlock_t intr_lock;
	 
	struct cmd_queue cmd_q;
	 
	u64  empty_iu_id;
	 
	u64  empty_iu_tag;
	uint new_state;
	uint resume_state;
	 
	struct timer_cb rsp_q_timer;
	 
	struct client_info client_data;
	 
	u32 client_cap;
	 
	u16  phyp_acr_state;
	u32 phyp_acr_flags;

	struct workqueue_struct *work_q;
	struct completion wait_idle;
	struct completion unconfig;
	struct device dev;
	struct vio_dev *dma_dev;
	struct srp_target target;
	struct ibmvscsis_tport tport;
	struct tasklet_struct work_task;
	struct work_struct proc_work;
};

 
#define IS_DISCONNECTING (UNCONFIGURING | ERR_DISCONNECT_RECONNECT | \
			  ERR_DISCONNECT)

 
#define DONT_PROCESS_STATE (IS_DISCONNECTING | UNDEFINED | \
			    ERR_DISCONNECTED  | WAIT_IDLE)

 
#define BLOCK (DISCONNECT_SCHEDULED)

 
#define TARGET_STOP(VSCSI) (long)(((VSCSI)->state & DONT_PROCESS_STATE) | \
				  ((VSCSI)->flags & BLOCK))

#define PREP_FOR_SUSPEND_FLAGS  (PREP_FOR_SUSPEND_ENABLED | \
				 PREP_FOR_SUSPEND_PENDING | \
				 PREP_FOR_SUSPEND_ABORTED | \
				 PREP_FOR_SUSPEND_OVERWRITE)

 
#define PRESERVE_FLAG_FIELDS (PREP_FOR_SUSPEND_FLAGS)

#define vio_iu(IUE) ((union viosrp_iu *)((IUE)->sbuf->buf))

#define READ_CMD(cdb)	(((cdb)[0] & 0x1F) == 8)
#define WRITE_CMD(cdb)	(((cdb)[0] & 0x1F) == 0xA)

#ifndef H_GET_PARTNER_INFO
#define H_GET_PARTNER_INFO              0x0000000000000008LL
#endif
#ifndef H_ENABLE_PREPARE_FOR_SUSPEND
#define H_ENABLE_PREPARE_FOR_SUSPEND    0x000000000000001DLL
#endif
#ifndef H_READY_FOR_SUSPEND
#define H_READY_FOR_SUSPEND             0x000000000000001ELL
#endif


#define h_copy_rdma(l, sa, sb, da, db) \
		plpar_hcall_norets(H_COPY_RDMA, l, sa, sb, da, db)
#define h_vioctl(u, o, a, u1, u2, u3, u4) \
		plpar_hcall_norets(H_VIOCTL, u, o, a, u1, u2)
#define h_reg_crq(ua, tok, sz) \
		plpar_hcall_norets(H_REG_CRQ, ua, tok, sz)
#define h_free_crq(ua) \
		plpar_hcall_norets(H_FREE_CRQ, ua)
#define h_send_crq(ua, d1, d2) \
		plpar_hcall_norets(H_SEND_CRQ, ua, d1, d2)

#endif
