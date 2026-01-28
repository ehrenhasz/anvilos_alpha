#ifndef __OCTEON_DROQ_H__
#define __OCTEON_DROQ_H__
#define MAX_PACKET_BUDGET 0xFFFFFFFF
struct octeon_droq_desc {
	u64 buffer_ptr;
	u64 info_ptr;
};
#define OCT_DROQ_DESC_SIZE    (sizeof(struct octeon_droq_desc))
struct octeon_droq_info {
	u64 length;
	union octeon_rh rh;
};
#define OCT_DROQ_INFO_SIZE   (sizeof(struct octeon_droq_info))
struct octeon_skb_page_info {
	dma_addr_t dma;
	struct page *page;
	unsigned int page_offset;
};
struct octeon_recv_buffer {
	void *buffer;
	u8 *data;
	struct octeon_skb_page_info pg_info;
};
#define OCT_DROQ_RECVBUF_SIZE    (sizeof(struct octeon_recv_buffer))
struct oct_droq_stats {
	u64 pkts_received;
	u64 bytes_received;
	u64 dropped_nodispatch;
	u64 dropped_nomem;
	u64 dropped_toomany;
	u64 rx_pkts_received;
	u64 rx_bytes_received;
	u64 rx_dropped;
	u64 rx_vxlan;
	u64 rx_alloc_failure;
};
#define    MAX_RECV_BUFS    64
struct octeon_recv_pkt {
	u16 buffer_count;
	u16 octeon_id;
	u32 length;
	union octeon_rh rh;
	void *buffer_ptr[MAX_RECV_BUFS];
	u32 buffer_size[MAX_RECV_BUFS];
};
#define OCT_RECV_PKT_SIZE    (sizeof(struct octeon_recv_pkt))
struct octeon_recv_info {
	void *rsvd;
	struct octeon_recv_pkt *recv_pkt;
};
#define  OCT_RECV_INFO_SIZE    (sizeof(struct octeon_recv_info))
static inline struct octeon_recv_info *octeon_alloc_recv_info(int extra_bytes)
{
	struct octeon_recv_info *recv_info;
	u8 *buf;
	buf = kmalloc(OCT_RECV_PKT_SIZE + OCT_RECV_INFO_SIZE +
		      extra_bytes, GFP_ATOMIC);
	if (!buf)
		return NULL;
	recv_info = (struct octeon_recv_info *)buf;
	recv_info->recv_pkt =
		(struct octeon_recv_pkt *)(buf + OCT_RECV_INFO_SIZE);
	recv_info->rsvd = NULL;
	if (extra_bytes)
		recv_info->rsvd = buf + OCT_RECV_INFO_SIZE + OCT_RECV_PKT_SIZE;
	return recv_info;
}
static inline void octeon_free_recv_info(struct octeon_recv_info *recv_info)
{
	kfree(recv_info);
}
typedef int (*octeon_dispatch_fn_t)(struct octeon_recv_info *, void *);
struct octeon_droq_ops {
	void (*fptr)(u32, void *, u32, union octeon_rh *, void *, void *);
	void *farg;
	void (*napi_fn)(void *);
	u32 poll_mode;
	u32 drop_on_max;
};
struct octeon_droq {
	u32 q_no;
	u32 pkt_count;
	struct octeon_droq_ops ops;
	struct octeon_device *oct_dev;
	struct octeon_droq_desc *desc_ring;
	u32 read_idx;
	u32 write_idx;
	u32 refill_idx;
	atomic_t pkts_pending;
	u32 max_count;
	u32 refill_count;
	u32 pkts_per_intr;
	u32 refill_threshold;
	u32 max_empty_descs;
	struct octeon_recv_buffer *recv_buf_list;
	u32 buffer_size;
	void  __iomem *pkts_credit_reg;
	void __iomem *pkts_sent_reg;
	struct list_head dispatch_list;
	struct oct_droq_stats stats;
	size_t desc_ring_dma;
	void *app_ctx;
	struct napi_struct napi;
	u32 cpu_id;
	call_single_data_t csd;
};
#define OCT_DROQ_SIZE   (sizeof(struct octeon_droq))
int octeon_init_droq(struct octeon_device *oct_dev,
		     u32 q_no,
		     u32 num_descs,
		     u32 desc_size,
		     void *app_ctx);
int octeon_delete_droq(struct octeon_device *oct_dev, u32 q_no);
int
octeon_register_droq_ops(struct octeon_device *oct,
			 u32 q_no,
			 struct octeon_droq_ops *ops);
int octeon_unregister_droq_ops(struct octeon_device *oct, u32 q_no);
int octeon_register_dispatch_fn(struct octeon_device *oct,
				u16 opcode,
				u16 subcode,
				octeon_dispatch_fn_t fn, void *fn_arg);
void *octeon_get_dispatch_arg(struct octeon_device *oct,
			      u16 opcode, u16 subcode);
void octeon_droq_print_stats(void);
u32 octeon_droq_check_hw_for_pkts(struct octeon_droq *droq);
int octeon_create_droq(struct octeon_device *oct, u32 q_no,
		       u32 num_descs, u32 desc_size, void *app_ctx);
int octeon_droq_process_packets(struct octeon_device *oct,
				struct octeon_droq *droq,
				u32 budget);
int octeon_droq_process_poll_pkts(struct octeon_device *oct,
				  struct octeon_droq *droq, u32 budget);
int octeon_enable_irq(struct octeon_device *oct, u32 q_no);
int octeon_retry_droq_refill(struct octeon_droq *droq);
#endif	 
