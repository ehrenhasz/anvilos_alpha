#ifndef _PIO_H
#define _PIO_H
#define SC_KERNEL 0
#define SC_VL15   1
#define SC_ACK    2
#define SC_USER   3	 
#define SC_MAX    4	 
#define INVALID_SCI 0xff
typedef void (*pio_release_cb)(void *arg, int code);
#define PRC_OK		0	 
#define PRC_STATUS_ERR	0x01	 
#define PRC_PBC		0x02	 
#define PRC_THRESHOLD	0x04	 
#define PRC_FILL_ERR	0x08	 
#define PRC_FORCE	0x10	 
#define PRC_SC_DISABLE	0x20	 
union mix {
	u64 val64;
	u32 val32[2];
	u8  val8[8];
};
struct pio_buf {
	struct send_context *sc; 
	pio_release_cb cb;	 
	void *arg;		 
	void __iomem *start;	 
	void __iomem *end;	 
	unsigned long sent_at;	 
	union mix carry;	 
	u16 qw_written;		 
	u8 carry_bytes;	 
};
union pio_shadow_ring {
	struct pio_buf pbuf;
} ____cacheline_aligned;
struct send_context {
	struct hfi1_devdata *dd;		 
	union pio_shadow_ring *sr;	 
	void __iomem *base_addr;	 
	u32 __percpu *buffers_allocated; 
	u32 size;			 
	int node;			 
	u32 sr_size;			 
	u16 flags;			 
	u8  type;			 
	u8  sw_index;			 
	u8  hw_context;			 
	u8  group;			 
	spinlock_t alloc_lock ____cacheline_aligned_in_smp;
	u32 sr_head;			 
	unsigned long fill;		 
	unsigned long alloc_free;	 
	u32 fill_wrap;			 
	u32 credits;			 
	spinlock_t release_lock ____cacheline_aligned_in_smp;
	u32 sr_tail;			 
	unsigned long free;		 
	volatile __le64 *hw_free;	 
	struct list_head piowait  ____cacheline_aligned_in_smp;
	seqlock_t waitlock;
	spinlock_t credit_ctrl_lock ____cacheline_aligned_in_smp;
	u32 credit_intr_count;		 
	u64 credit_ctrl;		 
	wait_queue_head_t halt_wait;     
	struct work_struct halt_work;	 
};
#define SCF_ENABLED 0x01
#define SCF_IN_FREE 0x02
#define SCF_HALTED  0x04
#define SCF_FROZEN  0x08
#define SCF_LINK_DOWN 0x10
struct send_context_info {
	struct send_context *sc;	 
	u16 allocated;			 
	u16 type;			 
	u16 base;			 
	u16 credits;			 
};
struct credit_return {
	volatile __le64 cr[8];
};
struct credit_return_base {
	struct credit_return *va;
	dma_addr_t dma;
};
struct sc_config_sizes {
	short int size;
	short int count;
};
#define INIT_SC_PER_VL 2
struct pio_map_elem {
	u32 mask;
	struct send_context *ksc[];
};
struct pio_vl_map {
	struct rcu_head list;
	u32 mask;
	u8 actual_vls;
	u8 vls;
	struct pio_map_elem *map[];
};
int pio_map_init(struct hfi1_devdata *dd, u8 port, u8 num_vls,
		 u8 *vl_scontexts);
void free_pio_map(struct hfi1_devdata *dd);
struct send_context *pio_select_send_context_vl(struct hfi1_devdata *dd,
						u32 selector, u8 vl);
struct send_context *pio_select_send_context_sc(struct hfi1_devdata *dd,
						u32 selector, u8 sc5);
int init_credit_return(struct hfi1_devdata *dd);
void free_credit_return(struct hfi1_devdata *dd);
int init_sc_pools_and_sizes(struct hfi1_devdata *dd);
int init_send_contexts(struct hfi1_devdata *dd);
int init_pervl_scs(struct hfi1_devdata *dd);
struct send_context *sc_alloc(struct hfi1_devdata *dd, int type,
			      uint hdrqentsize, int numa);
void sc_free(struct send_context *sc);
int sc_enable(struct send_context *sc);
void sc_disable(struct send_context *sc);
int sc_restart(struct send_context *sc);
void sc_return_credits(struct send_context *sc);
void sc_flush(struct send_context *sc);
void sc_drop(struct send_context *sc);
void sc_stop(struct send_context *sc, int bit);
struct pio_buf *sc_buffer_alloc(struct send_context *sc, u32 dw_len,
				pio_release_cb cb, void *arg);
void sc_release_update(struct send_context *sc);
void sc_group_release_update(struct hfi1_devdata *dd, u32 hw_context);
void sc_add_credit_return_intr(struct send_context *sc);
void sc_del_credit_return_intr(struct send_context *sc);
void sc_set_cr_threshold(struct send_context *sc, u32 new_threshold);
u32 sc_percent_to_threshold(struct send_context *sc, u32 percent);
u32 sc_mtu_to_threshold(struct send_context *sc, u32 mtu, u32 hdrqentsize);
void hfi1_sc_wantpiobuf_intr(struct send_context *sc, u32 needint);
void sc_wait(struct hfi1_devdata *dd);
void set_pio_integrity(struct send_context *sc);
void pio_reset_all(struct hfi1_devdata *dd);
void pio_freeze(struct hfi1_devdata *dd);
void pio_kernel_unfreeze(struct hfi1_devdata *dd);
void pio_kernel_linkup(struct hfi1_devdata *dd);
#define PSC_GLOBAL_ENABLE 0
#define PSC_GLOBAL_DISABLE 1
#define PSC_GLOBAL_VLARB_ENABLE 2
#define PSC_GLOBAL_VLARB_DISABLE 3
#define PSC_CM_RESET 4
#define PSC_DATA_VL_ENABLE 5
#define PSC_DATA_VL_DISABLE 6
void __cm_reset(struct hfi1_devdata *dd, u64 sendctrl);
void pio_send_control(struct hfi1_devdata *dd, int op);
void pio_copy(struct hfi1_devdata *dd, struct pio_buf *pbuf, u64 pbc,
	      const void *from, size_t count);
void seg_pio_copy_start(struct pio_buf *pbuf, u64 pbc,
			const void *from, size_t nbytes);
void seg_pio_copy_mid(struct pio_buf *pbuf, const void *from, size_t nbytes);
void seg_pio_copy_end(struct pio_buf *pbuf);
void seqfile_dump_sci(struct seq_file *s, u32 i,
		      struct send_context_info *sci);
#endif  
