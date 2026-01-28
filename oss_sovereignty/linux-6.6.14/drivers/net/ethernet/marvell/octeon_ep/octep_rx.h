#ifndef _OCTEP_RX_H_
#define _OCTEP_RX_H_
struct octep_oq_desc_hw {
	dma_addr_t buffer_ptr;
	u64 info_ptr;
};
#define OCTEP_OQ_DESC_SIZE    (sizeof(struct octep_oq_desc_hw))
#define OCTEP_CSUM_L4_VERIFIED 0x1
#define OCTEP_CSUM_IP_VERIFIED 0x2
#define OCTEP_CSUM_VERIFIED (OCTEP_CSUM_L4_VERIFIED | OCTEP_CSUM_IP_VERIFIED)
struct octep_oq_resp_hw_ext {
	u64 reserved:62;
	u64 csum_verified:2;
};
#define  OCTEP_OQ_RESP_HW_EXT_SIZE   (sizeof(struct octep_oq_resp_hw_ext))
struct octep_oq_resp_hw {
	__be64 length;
};
#define OCTEP_OQ_RESP_HW_SIZE   (sizeof(struct octep_oq_resp_hw))
struct octep_rx_buffer {
	struct page *page;
	u64 len;
};
#define OCTEP_OQ_RECVBUF_SIZE    (sizeof(struct octep_rx_buffer))
struct octep_oq_stats {
	u64 packets;
	u64 bytes;
	u64 alloc_failures;
};
#define OCTEP_OQ_STATS_SIZE   (sizeof(struct octep_oq_stats))
struct octep_iface_rx_stats {
	u64 pkts;
	u64 octets;
	u64 pause_pkts;
	u64 pause_octets;
	u64 dmac0_pkts;
	u64 dmac0_octets;
	u64 dropped_pkts_fifo_full;
	u64 dropped_octets_fifo_full;
	u64 err_pkts;
	u64 dmac1_pkts;
	u64 dmac1_octets;
	u64 ncsi_dropped_pkts;
	u64 ncsi_dropped_octets;
	u64 mcast_pkts;
	u64 bcast_pkts;
};
struct octep_oq {
	u32 q_no;
	struct octep_device *octep_dev;
	struct net_device *netdev;
	struct device *dev;
	struct napi_struct *napi;
	struct octep_rx_buffer *buff_info;
	u8 __iomem *pkts_credit_reg;
	u8 __iomem *pkts_sent_reg;
	struct octep_oq_stats stats;
	u32 pkts_pending;
	u32 last_pkt_count;
	u32 host_read_idx;
	u32 max_count;
	u32 ring_size_mask;
	u32 refill_count;
	u32 host_refill_idx;
	u32 refill_threshold;
	u32 buffer_size;
	u32 max_single_buffer_size;
	struct octep_oq_desc_hw *desc_ring;
	dma_addr_t desc_ring_dma;
};
#define OCTEP_OQ_SIZE   (sizeof(struct octep_oq))
#endif	 
