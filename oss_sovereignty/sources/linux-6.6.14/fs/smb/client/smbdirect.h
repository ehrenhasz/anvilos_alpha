

#ifndef _SMBDIRECT_H
#define _SMBDIRECT_H

#ifdef CONFIG_CIFS_SMB_DIRECT
#define cifs_rdma_enabled(server)	((server)->rdma)

#include "cifsglob.h"
#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>
#include <linux/mempool.h>

extern int rdma_readwrite_threshold;
extern int smbd_max_frmr_depth;
extern int smbd_keep_alive_interval;
extern int smbd_max_receive_size;
extern int smbd_max_fragmented_recv_size;
extern int smbd_max_send_size;
extern int smbd_send_credit_target;
extern int smbd_receive_credit_max;

enum keep_alive_status {
	KEEP_ALIVE_NONE,
	KEEP_ALIVE_PENDING,
	KEEP_ALIVE_SENT,
};

enum smbd_connection_status {
	SMBD_CREATED,
	SMBD_CONNECTING,
	SMBD_CONNECTED,
	SMBD_NEGOTIATE_FAILED,
	SMBD_DISCONNECTING,
	SMBD_DISCONNECTED,
	SMBD_DESTROYED
};


struct smbd_connection {
	enum smbd_connection_status transport_status;

	
	struct rdma_cm_id *id;
	struct ib_qp_init_attr qp_attr;
	struct ib_pd *pd;
	struct ib_cq *send_cq, *recv_cq;
	struct ib_device_attr dev_attr;
	int ri_rc;
	struct completion ri_done;
	wait_queue_head_t conn_wait;
	wait_queue_head_t disconn_wait;

	struct completion negotiate_completion;
	bool negotiate_done;

	struct work_struct disconnect_work;
	struct work_struct post_send_credits_work;

	spinlock_t lock_new_credits_offered;
	int new_credits_offered;

	
	int receive_credit_max;
	int send_credit_target;
	int max_send_size;
	int max_fragmented_recv_size;
	int max_fragmented_send_size;
	int max_receive_size;
	int keep_alive_interval;
	int max_readwrite_size;
	enum keep_alive_status keep_alive_requested;
	int protocol;
	atomic_t send_credits;
	atomic_t receive_credits;
	int receive_credit_target;
	int fragment_reassembly_remaining;

	
	
	int responder_resources;
	
	int max_frmr_depth;
	
	int rdma_readwrite_threshold;
	enum ib_mr_type mr_type;
	struct list_head mr_list;
	spinlock_t mr_list_lock;
	
	atomic_t mr_ready_count;
	atomic_t mr_used_count;
	wait_queue_head_t wait_mr;
	struct work_struct mr_recovery_work;
	
	wait_queue_head_t wait_for_mr_cleanup;

	
	atomic_t send_pending;
	wait_queue_head_t wait_send_pending;
	wait_queue_head_t wait_post_send;

	
	struct list_head receive_queue;
	int count_receive_queue;
	spinlock_t receive_queue_lock;

	struct list_head empty_packet_queue;
	int count_empty_packet_queue;
	spinlock_t empty_packet_queue_lock;

	wait_queue_head_t wait_receive_queues;

	
	struct list_head reassembly_queue;
	spinlock_t reassembly_queue_lock;
	wait_queue_head_t wait_reassembly_queue;

	
	int reassembly_data_length;
	int reassembly_queue_length;
	
	int first_entry_offset;

	bool send_immediate;

	wait_queue_head_t wait_send_queue;

	
	bool full_packet_received;

	struct workqueue_struct *workqueue;
	struct delayed_work idle_timer_work;

	
	
	struct kmem_cache *request_cache;
	mempool_t *request_mempool;

	
	struct kmem_cache *response_cache;
	mempool_t *response_mempool;

	
	unsigned int count_get_receive_buffer;
	unsigned int count_put_receive_buffer;
	unsigned int count_reassembly_queue;
	unsigned int count_enqueue_reassembly_queue;
	unsigned int count_dequeue_reassembly_queue;
	unsigned int count_send_empty;
};

enum smbd_message_type {
	SMBD_NEGOTIATE_RESP,
	SMBD_TRANSFER_DATA,
};

#define SMB_DIRECT_RESPONSE_REQUESTED 0x0001


struct smbd_negotiate_req {
	__le16 min_version;
	__le16 max_version;
	__le16 reserved;
	__le16 credits_requested;
	__le32 preferred_send_size;
	__le32 max_receive_size;
	__le32 max_fragmented_size;
} __packed;


struct smbd_negotiate_resp {
	__le16 min_version;
	__le16 max_version;
	__le16 negotiated_version;
	__le16 reserved;
	__le16 credits_requested;
	__le16 credits_granted;
	__le32 status;
	__le32 max_readwrite_size;
	__le32 preferred_send_size;
	__le32 max_receive_size;
	__le32 max_fragmented_size;
} __packed;


struct smbd_data_transfer {
	__le16 credits_requested;
	__le16 credits_granted;
	__le16 flags;
	__le16 reserved;
	__le32 remaining_data_length;
	__le32 data_offset;
	__le32 data_length;
	__le32 padding;
	__u8 buffer[];
} __packed;


struct smbd_buffer_descriptor_v1 {
	__le64 offset;
	__le32 token;
	__le32 length;
} __packed;


#define SMBDIRECT_MAX_SEND_SGE	6


struct smbd_request {
	struct smbd_connection *info;
	struct ib_cqe cqe;

	
	struct ib_sge sge[SMBDIRECT_MAX_SEND_SGE];
	int num_sge;

	
	u8 packet[];
};


#define SMBDIRECT_MAX_RECV_SGE	1


struct smbd_response {
	struct smbd_connection *info;
	struct ib_cqe cqe;
	struct ib_sge sge;

	enum smbd_message_type type;

	
	struct list_head list;

	
	bool first_segment;

	
	u8 packet[];
};


struct smbd_connection *smbd_get_connection(
	struct TCP_Server_Info *server, struct sockaddr *dstaddr);


int smbd_reconnect(struct TCP_Server_Info *server);

void smbd_destroy(struct TCP_Server_Info *server);


int smbd_recv(struct smbd_connection *info, struct msghdr *msg);
int smbd_send(struct TCP_Server_Info *server,
	int num_rqst, struct smb_rqst *rqst);

enum mr_state {
	MR_READY,
	MR_REGISTERED,
	MR_INVALIDATED,
	MR_ERROR
};

struct smbd_mr {
	struct smbd_connection	*conn;
	struct list_head	list;
	enum mr_state		state;
	struct ib_mr		*mr;
	struct sg_table		sgt;
	enum dma_data_direction	dir;
	union {
		struct ib_reg_wr	wr;
		struct ib_send_wr	inv_wr;
	};
	struct ib_cqe		cqe;
	bool			need_invalidate;
	struct completion	invalidate_done;
};


struct smbd_mr *smbd_register_mr(
	struct smbd_connection *info, struct iov_iter *iter,
	bool writing, bool need_invalidate);
int smbd_deregister_mr(struct smbd_mr *mr);

#else
#define cifs_rdma_enabled(server)	0
struct smbd_connection {};
static inline void *smbd_get_connection(
	struct TCP_Server_Info *server, struct sockaddr *dstaddr) {return NULL;}
static inline int smbd_reconnect(struct TCP_Server_Info *server) {return -1; }
static inline void smbd_destroy(struct TCP_Server_Info *server) {}
static inline int smbd_recv(struct smbd_connection *info, struct msghdr *msg) {return -1; }
static inline int smbd_send(struct TCP_Server_Info *server, int num_rqst, struct smb_rqst *rqst) {return -1; }
#endif

#endif
