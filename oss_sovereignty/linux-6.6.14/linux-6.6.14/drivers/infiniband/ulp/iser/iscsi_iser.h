#ifndef __ISCSI_ISER_H__
#define __ISCSI_ISER_H__
#include <linux/types.h>
#include <linux/net.h>
#include <linux/printk.h>
#include <scsi/libiscsi.h>
#include <scsi/scsi_transport_iscsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/iser.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/mutex.h>
#include <linux/mempool.h>
#include <linux/uio.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>
#define DRV_NAME	"iser"
#define PFX		DRV_NAME ": "
#define DRV_VER		"1.6"
#define iser_dbg(fmt, arg...)				 \
	do {						 \
		if (unlikely(iser_debug_level > 2))	 \
			printk(KERN_DEBUG PFX "%s: " fmt,\
				__func__ , ## arg);	 \
	} while (0)
#define iser_warn(fmt, arg...)				\
	do {						\
		if (unlikely(iser_debug_level > 0))	\
			pr_warn(PFX "%s: " fmt,		\
				__func__ , ## arg);	\
	} while (0)
#define iser_info(fmt, arg...)				\
	do {						\
		if (unlikely(iser_debug_level > 1))	\
			pr_info(PFX "%s: " fmt,		\
				__func__ , ## arg);	\
	} while (0)
#define iser_err(fmt, arg...) \
	pr_err(PFX "%s: " fmt, __func__ , ## arg)
#define ISER_DEF_MAX_SECTORS		1024
#define ISCSI_ISER_DEF_SG_TABLESIZE                                            \
	((ISER_DEF_MAX_SECTORS * SECTOR_SIZE) >> ilog2(SZ_4K))
#define ISCSI_ISER_MAX_SG_TABLESIZE ((32768 * SECTOR_SIZE) >> ilog2(SZ_4K))
#define ISER_DEF_XMIT_CMDS_DEFAULT		512
#if ISCSI_DEF_XMIT_CMDS_MAX > ISER_DEF_XMIT_CMDS_DEFAULT
	#define ISER_DEF_XMIT_CMDS_MAX		ISCSI_DEF_XMIT_CMDS_MAX
#else
	#define ISER_DEF_XMIT_CMDS_MAX		ISER_DEF_XMIT_CMDS_DEFAULT
#endif
#define ISER_DEF_CMD_PER_LUN		ISER_DEF_XMIT_CMDS_MAX
#define ISER_MAX_RX_MISC_PDUS		4  
#define ISER_MAX_TX_MISC_PDUS		6  
#define ISER_QP_MAX_RECV_DTOS		(ISER_DEF_XMIT_CMDS_MAX)
#define ISER_INFLIGHT_DATAOUTS		8
#define ISER_QP_MAX_REQ_DTOS		(ISER_DEF_XMIT_CMDS_MAX *    \
					(1 + ISER_INFLIGHT_DATAOUTS) + \
					ISER_MAX_TX_MISC_PDUS        + \
					ISER_MAX_RX_MISC_PDUS)
#define ISER_MAX_REG_WR_PER_CMD		5
#define ISER_QP_SIG_MAX_REQ_DTOS	(ISER_DEF_XMIT_CMDS_MAX	*       \
					(1 + ISER_MAX_REG_WR_PER_CMD) + \
					ISER_MAX_TX_MISC_PDUS         + \
					ISER_MAX_RX_MISC_PDUS)
#define ISER_GET_MAX_XMIT_CMDS(send_wr) ((send_wr			\
					 - ISER_MAX_TX_MISC_PDUS	\
					 - ISER_MAX_RX_MISC_PDUS) /	\
					 (1 + ISER_INFLIGHT_DATAOUTS))
#define ISER_HEADERS_LEN	(sizeof(struct iser_ctrl) + sizeof(struct iscsi_hdr))
#define ISER_RECV_DATA_SEG_LEN	128
#define ISER_RX_PAYLOAD_SIZE	(ISER_HEADERS_LEN + ISER_RECV_DATA_SEG_LEN)
#define ISER_RX_LOGIN_SIZE	(ISER_HEADERS_LEN + ISCSI_DEF_MAX_RECV_SEG_LEN)
#define ISER_OBJECT_NAME_SIZE		    64
enum iser_conn_state {
	ISER_CONN_INIT,		    
	ISER_CONN_PENDING,	    
	ISER_CONN_UP,		    
	ISER_CONN_TERMINATING,	    
	ISER_CONN_DOWN,		    
	ISER_CONN_STATES_NUM
};
enum iser_task_status {
	ISER_TASK_STATUS_INIT = 0,
	ISER_TASK_STATUS_STARTED,
	ISER_TASK_STATUS_COMPLETED
};
enum iser_data_dir {
	ISER_DIR_IN = 0,	    
	ISER_DIR_OUT,		    
	ISER_DIRS_NUM
};
struct iser_data_buf {
	struct scatterlist *sg;
	int                size;
	unsigned long      data_len;
	int                dma_nents;
};
struct iser_device;
struct iscsi_iser_task;
struct iscsi_endpoint;
struct iser_reg_resources;
struct iser_mem_reg {
	struct ib_sge sge;
	u32 rkey;
	struct iser_fr_desc *desc;
};
enum iser_desc_type {
	ISCSI_TX_CONTROL ,
	ISCSI_TX_SCSI_COMMAND,
	ISCSI_TX_DATAOUT
};
struct iser_tx_desc {
	struct iser_ctrl             iser_header;
	struct iscsi_hdr             iscsi_header;
	enum   iser_desc_type        type;
	u64		             dma_addr;
	struct ib_sge		     tx_sg[2];
	int                          num_sge;
	struct ib_cqe		     cqe;
	bool			     mapped;
	struct ib_reg_wr	     reg_wr;
	struct ib_send_wr	     send_wr;
	struct ib_send_wr	     inv_wr;
};
#define ISER_RX_PAD_SIZE	(256 - (ISER_RX_PAYLOAD_SIZE + \
				 sizeof(u64) + sizeof(struct ib_sge) + \
				 sizeof(struct ib_cqe)))
struct iser_rx_desc {
	struct iser_ctrl             iser_header;
	struct iscsi_hdr             iscsi_header;
	char		             data[ISER_RECV_DATA_SEG_LEN];
	u64		             dma_addr;
	struct ib_sge		     rx_sg;
	struct ib_cqe		     cqe;
	char		             pad[ISER_RX_PAD_SIZE];
} __packed;
struct iser_login_desc {
	void                         *req;
	void                         *rsp;
	u64                          req_dma;
	u64                          rsp_dma;
	struct ib_sge                sge;
	struct ib_cqe		     cqe;
} __packed;
struct iser_conn;
struct ib_conn;
struct iser_device {
	struct ib_device             *ib_device;
	struct ib_pd	             *pd;
	struct ib_event_handler      event_handler;
	struct list_head             ig_list;
	int                          refcount;
};
struct iser_reg_resources {
	struct ib_mr                     *mr;
	struct ib_mr                     *sig_mr;
};
struct iser_fr_desc {
	struct list_head		  list;
	struct iser_reg_resources	  rsc;
	bool				  sig_protected;
	struct list_head                  all_list;
};
struct iser_fr_pool {
	struct list_head        list;
	spinlock_t              lock;
	int                     size;
	struct list_head        all_list;
};
struct ib_conn {
	struct rdma_cm_id           *cma_id;
	struct ib_qp	            *qp;
	struct ib_cq		    *cq;
	u32			    cq_size;
	struct iser_device          *device;
	struct iser_fr_pool          fr_pool;
	bool			     pi_support;
	struct ib_cqe		     reg_cqe;
};
struct iser_conn {
	struct ib_conn		     ib_conn;
	struct iscsi_conn	     *iscsi_conn;
	struct iscsi_endpoint	     *ep;
	enum iser_conn_state	     state;
	unsigned		     qp_max_recv_dtos;
	u16                          max_cmds;
	char 			     name[ISER_OBJECT_NAME_SIZE];
	struct work_struct	     release_work;
	struct mutex		     state_mutex;
	struct completion	     stop_completion;
	struct completion	     ib_completion;
	struct completion	     up_completion;
	struct list_head	     conn_list;
	struct iser_login_desc       login_desc;
	struct iser_rx_desc	     *rx_descs;
	u32                          num_rx_descs;
	unsigned short               scsi_sg_tablesize;
	unsigned short               pages_per_mr;
	bool			     snd_w_inv;
};
struct iscsi_iser_task {
	struct iser_tx_desc          desc;
	struct iser_conn	     *iser_conn;
	enum iser_task_status 	     status;
	struct scsi_cmnd	     *sc;
	int                          command_sent;
	int                          dir[ISER_DIRS_NUM];
	struct iser_mem_reg          rdma_reg[ISER_DIRS_NUM];
	struct iser_data_buf         data[ISER_DIRS_NUM];
	struct iser_data_buf         prot[ISER_DIRS_NUM];
};
struct iser_global {
	struct mutex      device_list_mutex;
	struct list_head  device_list;
	struct mutex      connlist_mutex;
	struct list_head  connlist;
	struct kmem_cache *desc_cache;
};
extern struct iser_global ig;
extern int iser_debug_level;
extern bool iser_pi_enable;
extern unsigned int iser_max_sectors;
extern bool iser_always_reg;
int iser_send_control(struct iscsi_conn *conn,
		      struct iscsi_task *task);
int iser_send_command(struct iscsi_conn *conn,
		      struct iscsi_task *task);
int iser_send_data_out(struct iscsi_conn *conn,
		       struct iscsi_task *task,
		       struct iscsi_data *hdr);
void iscsi_iser_recv(struct iscsi_conn *conn,
		     struct iscsi_hdr *hdr,
		     char *rx_data,
		     int rx_data_len);
void iser_conn_init(struct iser_conn *iser_conn);
void iser_conn_release(struct iser_conn *iser_conn);
int iser_conn_terminate(struct iser_conn *iser_conn);
void iser_release_work(struct work_struct *work);
void iser_err_comp(struct ib_wc *wc, const char *type);
void iser_login_rsp(struct ib_cq *cq, struct ib_wc *wc);
void iser_task_rsp(struct ib_cq *cq, struct ib_wc *wc);
void iser_cmd_comp(struct ib_cq *cq, struct ib_wc *wc);
void iser_ctrl_comp(struct ib_cq *cq, struct ib_wc *wc);
void iser_dataout_comp(struct ib_cq *cq, struct ib_wc *wc);
void iser_reg_comp(struct ib_cq *cq, struct ib_wc *wc);
void iser_task_rdma_init(struct iscsi_iser_task *task);
void iser_task_rdma_finalize(struct iscsi_iser_task *task);
void iser_free_rx_descriptors(struct iser_conn *iser_conn);
void iser_finalize_rdma_unaligned_sg(struct iscsi_iser_task *iser_task,
				     struct iser_data_buf *mem,
				     enum iser_data_dir cmd_dir);
int iser_reg_mem_fastreg(struct iscsi_iser_task *task,
			 enum iser_data_dir dir,
			 bool all_imm);
void iser_unreg_mem_fastreg(struct iscsi_iser_task *task,
			    enum iser_data_dir dir);
int  iser_connect(struct iser_conn *iser_conn,
		  struct sockaddr *src_addr,
		  struct sockaddr *dst_addr,
		  int non_blocking);
int  iser_post_recvl(struct iser_conn *iser_conn);
int  iser_post_recvm(struct iser_conn *iser_conn,
		     struct iser_rx_desc *rx_desc);
int  iser_post_send(struct ib_conn *ib_conn, struct iser_tx_desc *tx_desc);
int iser_dma_map_task_data(struct iscsi_iser_task *iser_task,
			   enum iser_data_dir iser_dir,
			   enum dma_data_direction dma_dir);
void iser_dma_unmap_task_data(struct iscsi_iser_task *iser_task,
			      enum iser_data_dir iser_dir,
			      enum dma_data_direction dma_dir);
int  iser_initialize_task_headers(struct iscsi_task *task,
			struct iser_tx_desc *tx_desc);
int iser_alloc_rx_descriptors(struct iser_conn *iser_conn,
			      struct iscsi_session *session);
int iser_alloc_fastreg_pool(struct ib_conn *ib_conn,
			    unsigned cmds_max,
			    unsigned int size);
void iser_free_fastreg_pool(struct ib_conn *ib_conn);
u8 iser_check_task_pi_status(struct iscsi_iser_task *iser_task,
			     enum iser_data_dir cmd_dir, sector_t *sector);
static inline struct iser_conn *
to_iser_conn(struct ib_conn *ib_conn)
{
	return container_of(ib_conn, struct iser_conn, ib_conn);
}
static inline struct iser_rx_desc *
iser_rx(struct ib_cqe *cqe)
{
	return container_of(cqe, struct iser_rx_desc, cqe);
}
static inline struct iser_tx_desc *
iser_tx(struct ib_cqe *cqe)
{
	return container_of(cqe, struct iser_tx_desc, cqe);
}
static inline struct iser_login_desc *
iser_login(struct ib_cqe *cqe)
{
	return container_of(cqe, struct iser_login_desc, cqe);
}
#endif
