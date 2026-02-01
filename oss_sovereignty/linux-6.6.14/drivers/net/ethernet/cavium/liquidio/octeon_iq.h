 
 

#ifndef __OCTEON_IQ_H__
#define  __OCTEON_IQ_H__

#define IQ_STATUS_RUNNING   1

#define IQ_SEND_OK          0
#define IQ_SEND_STOP        1
#define IQ_SEND_FAILED     -1

 

 

#define REQTYPE_NONE                 0
#define REQTYPE_NORESP_NET           1
#define REQTYPE_NORESP_NET_SG        2
#define REQTYPE_RESP_NET             3
#define REQTYPE_RESP_NET_SG          4
#define REQTYPE_SOFT_COMMAND         5
#define REQTYPE_LAST                 5

struct octeon_request_list {
	u32 reqtype;
	void *buf;
};

 

 
struct oct_iq_stats {
	u64 instr_posted;  
	u64 instr_processed;  
	u64 instr_dropped;  
	u64 bytes_sent;   
	u64 sgentry_sent; 
	u64 tx_done; 
	u64 tx_iq_busy; 
	u64 tx_dropped; 
	u64 tx_tot_bytes; 
	u64 tx_gso;   
	u64 tx_vxlan;  
	u64 tx_dmamap_fail;  
	u64 tx_restart;  
};

#define OCT_IQ_STATS_SIZE   (sizeof(struct oct_iq_stats))

 
struct octeon_instr_queue {
	struct octeon_device *oct_dev;

	 
	spinlock_t lock;

	 
	spinlock_t post_lock;

	 
	bool allow_soft_cmds;

	u32 pkt_in_done;

	u32 pkts_processed;

	 
	spinlock_t iq_flush_running_lock;

	 
	u32 iqcmd_64B:1;

	 
	union oct_txpciq txpciq;

	u32 rsvd:17;

	 
	u32 do_auto_flush:1;

	u32 status:8;

	 
	u32 max_count;

	 
	u32 host_write_index;

	 
	u32 octeon_read_index;

	 
	u32 flush_index;

	 
	atomic_t instr_pending;

	u32 reset_instr_cnt;

	 
	u8 *base_addr;

	struct octeon_request_list *request_list;

	 
	void __iomem *doorbell_reg;

	 
	void __iomem *inst_cnt_reg;

	 
	u32 fill_cnt;

	 
	u32 fill_threshold;

	 
	u64 last_db_time;

	 
	u32 db_timeout;

	 
	struct oct_iq_stats stats;

	 
	dma_addr_t base_addr_dma;

	 
	void *app_ctx;

	 
	int q_index;

	 
	int ifidx;

};

 

 
struct octeon_instr_32B {
	 
	u64 dptr;

	 
	u64 ih;

	 
	u64 rptr;

	 
	u64 irh;

};

#define OCT_32B_INSTR_SIZE     (sizeof(struct octeon_instr_32B))

 
struct octeon_instr2_64B {
	 
	u64 dptr;

	 
	u64 ih2;

	 
	u64 irh;

	 
	u64 ossp[2];

	 
	u64 rdp;

	 
	u64 rptr;

	u64 reserved;
};

struct octeon_instr3_64B {
	 
	u64 dptr;

	 
	u64 ih3;

	 
	u64 pki_ih3;

	 
	u64 irh;

	 
	u64 ossp[2];

	 
	u64 rdp;

	 
	u64 rptr;

};

union octeon_instr_64B {
	struct octeon_instr2_64B cmd2;
	struct octeon_instr3_64B cmd3;
};

#define OCT_64B_INSTR_SIZE     (sizeof(union octeon_instr_64B))

 
#define  SOFT_COMMAND_BUFFER_SIZE	2048

struct octeon_soft_command {
	 
	struct list_head node;
	u64 dma_addr;
	u32 size;

	 
	union octeon_instr_64B cmd;

#define COMPLETION_WORD_INIT    0xffffffffffffffffULL
	u64 *status_word;

	 
	void *virtdptr;
	u64 dmadptr;
	u32 datasize;

	 
	void *virtrptr;
	u64 dmarptr;
	u32 rdatasize;

	 
	void *ctxptr;
	u32  ctxsize;

	 
	size_t expiry_time;
	u32 iq_no;
	void (*callback)(struct octeon_device *, u32, void *);
	void *callback_arg;

	int caller_is_done;
	u32 sc_status;
	struct completion complete;
};

 
#define LIO_SC_MAX_TMO_MS       60000

 
#define  MAX_SOFT_COMMAND_BUFFERS	256

 
struct octeon_sc_buffer_pool {
	 
	struct list_head head;

	 
	spinlock_t lock;

	atomic_t alloc_buf_count;
};

#define INCR_INSTRQUEUE_PKT_COUNT(octeon_dev_ptr, iq_no, field, count)  \
		(((octeon_dev_ptr)->instr_queue[iq_no]->stats.field) += count)

int octeon_setup_sc_buffer_pool(struct octeon_device *oct);
int octeon_free_sc_done_list(struct octeon_device *oct);
int octeon_free_sc_zombie_list(struct octeon_device *oct);
int octeon_free_sc_buffer_pool(struct octeon_device *oct);
struct octeon_soft_command *
	octeon_alloc_soft_command(struct octeon_device *oct,
				  u32 datasize, u32 rdatasize,
				  u32 ctxsize);
void octeon_free_soft_command(struct octeon_device *oct,
			      struct octeon_soft_command *sc);

 
int octeon_init_instr_queue(struct octeon_device *octeon_dev,
			    union oct_txpciq txpciq,
			    u32 num_descs);

 
int octeon_delete_instr_queue(struct octeon_device *octeon_dev, u32 iq_no);

int lio_wait_for_instr_fetch(struct octeon_device *oct);

void
octeon_ring_doorbell_locked(struct octeon_device *oct, u32 iq_no);

int
octeon_register_reqtype_free_fn(struct octeon_device *oct, int reqtype,
				void (*fn)(void *));

int
lio_process_iq_request_list(struct octeon_device *oct,
			    struct octeon_instr_queue *iq, u32 napi_budget);

int octeon_send_command(struct octeon_device *oct, u32 iq_no,
			u32 force_db, void *cmd, void *buf,
			u32 datasize, u32 reqtype);

void octeon_dump_soft_command(struct octeon_device *oct,
			      struct octeon_soft_command *sc);

void octeon_prepare_soft_command(struct octeon_device *oct,
				 struct octeon_soft_command *sc,
				 u8 opcode, u8 subcode,
				 u32 irh_ossp, u64 ossp0,
				 u64 ossp1);

int octeon_send_soft_command(struct octeon_device *oct,
			     struct octeon_soft_command *sc);

int octeon_setup_iq(struct octeon_device *oct, int ifidx,
		    int q_index, union oct_txpciq iq_no, u32 num_descs,
		    void *app_ctx);
int
octeon_flush_iq(struct octeon_device *oct, struct octeon_instr_queue *iq,
		u32 napi_budget);
#endif				 
