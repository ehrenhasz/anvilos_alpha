#ifndef _IAVF_TXRX_H_
#define _IAVF_TXRX_H_
#define IAVF_DEFAULT_IRQ_WORK      256
#define IAVF_ITR_DYNAMIC	0x8000	 
#define IAVF_ITR_MASK		0x1FFE	 
#define IAVF_ITR_100K		    10	 
#define IAVF_ITR_50K		    20
#define IAVF_ITR_20K		    50
#define IAVF_ITR_18K		    60
#define IAVF_ITR_8K		   122
#define IAVF_MAX_ITR		  8160	 
#define ITR_TO_REG(setting) ((setting) & ~IAVF_ITR_DYNAMIC)
#define ITR_REG_ALIGN(setting) __ALIGN_MASK(setting, ~IAVF_ITR_MASK)
#define ITR_IS_DYNAMIC(setting) (!!((setting) & IAVF_ITR_DYNAMIC))
#define IAVF_ITR_RX_DEF		(IAVF_ITR_20K | IAVF_ITR_DYNAMIC)
#define IAVF_ITR_TX_DEF		(IAVF_ITR_20K | IAVF_ITR_DYNAMIC)
#define INTRL_ENA                  BIT(6)
#define IAVF_MAX_INTRL             0x3B     
#define INTRL_REG_TO_USEC(intrl) ((intrl & ~INTRL_ENA) << 2)
#define INTRL_USEC_TO_REG(set) ((set) ? ((set) >> 2) | INTRL_ENA : 0)
#define IAVF_INTRL_8K              125      
#define IAVF_INTRL_62K             16       
#define IAVF_INTRL_83K             12       
#define IAVF_QUEUE_END_OF_LIST 0x7FF
enum iavf_dyn_idx_t {
	IAVF_IDX_ITR0 = 0,
	IAVF_IDX_ITR1 = 1,
	IAVF_IDX_ITR2 = 2,
	IAVF_ITR_NONE = 3	 
};
#define IAVF_RX_ITR    IAVF_IDX_ITR0
#define IAVF_TX_ITR    IAVF_IDX_ITR1
#define IAVF_PE_ITR    IAVF_IDX_ITR2
#define IAVF_DEFAULT_RSS_HENA ( \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV4_UDP) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV4_SCTP) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV4_TCP) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV4_OTHER) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_FRAG_IPV4) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV6_UDP) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV6_TCP) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV6_SCTP) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV6_OTHER) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_FRAG_IPV6) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_L2_PAYLOAD))
#define IAVF_DEFAULT_RSS_HENA_EXPANDED (IAVF_DEFAULT_RSS_HENA | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP) | \
	BIT_ULL(IAVF_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP))
#define IAVF_RXBUFFER_256   256
#define IAVF_RXBUFFER_1536  1536   
#define IAVF_RXBUFFER_2048  2048
#define IAVF_RXBUFFER_3072  3072   
#define IAVF_MAX_RXBUFFER   9728   
#define IAVF_RX_HDR_SIZE IAVF_RXBUFFER_256
#define IAVF_PACKET_HDR_PAD (ETH_HLEN + ETH_FCS_LEN + (VLAN_HLEN * 2))
#define iavf_rx_desc iavf_32byte_rx_desc
#define IAVF_RX_DMA_ATTR \
	(DMA_ATTR_SKIP_CPU_SYNC | DMA_ATTR_WEAK_ORDERING)
#if (PAGE_SIZE < 8192)
#define IAVF_2K_TOO_SMALL_WITH_PADDING \
((NET_SKB_PAD + IAVF_RXBUFFER_1536) > SKB_WITH_OVERHEAD(IAVF_RXBUFFER_2048))
static inline int iavf_compute_pad(int rx_buf_len)
{
	int page_size, pad_size;
	page_size = ALIGN(rx_buf_len, PAGE_SIZE / 2);
	pad_size = SKB_WITH_OVERHEAD(page_size) - rx_buf_len;
	return pad_size;
}
static inline int iavf_skb_pad(void)
{
	int rx_buf_len;
	if (IAVF_2K_TOO_SMALL_WITH_PADDING)
		rx_buf_len = IAVF_RXBUFFER_3072 + SKB_DATA_ALIGN(NET_IP_ALIGN);
	else
		rx_buf_len = IAVF_RXBUFFER_1536;
	rx_buf_len -= NET_IP_ALIGN;
	return iavf_compute_pad(rx_buf_len);
}
#define IAVF_SKB_PAD iavf_skb_pad()
#else
#define IAVF_2K_TOO_SMALL_WITH_PADDING false
#define IAVF_SKB_PAD (NET_SKB_PAD + NET_IP_ALIGN)
#endif
static inline bool iavf_test_staterr(union iavf_rx_desc *rx_desc,
				     const u64 stat_err_bits)
{
	return !!(rx_desc->wb.qword1.status_error_len &
		  cpu_to_le64(stat_err_bits));
}
#define IAVF_RX_INCREMENT(r, i) \
	do {					\
		(i)++;				\
		if ((i) == (r)->count)		\
			i = 0;			\
		r->next_to_clean = i;		\
	} while (0)
#define IAVF_RX_NEXT_DESC(r, i, n)		\
	do {					\
		(i)++;				\
		if ((i) == (r)->count)		\
			i = 0;			\
		(n) = IAVF_RX_DESC((r), (i));	\
	} while (0)
#define IAVF_RX_NEXT_DESC_PREFETCH(r, i, n)		\
	do {						\
		IAVF_RX_NEXT_DESC((r), (i), (n));	\
		prefetch((n));				\
	} while (0)
#define IAVF_MAX_BUFFER_TXD	8
#define IAVF_MIN_TX_LEN		17
#define IAVF_MAX_READ_REQ_SIZE		4096
#define IAVF_MAX_DATA_PER_TXD		(16 * 1024 - 1)
#define IAVF_MAX_DATA_PER_TXD_ALIGNED \
	(IAVF_MAX_DATA_PER_TXD & ~(IAVF_MAX_READ_REQ_SIZE - 1))
static inline unsigned int iavf_txd_use_count(unsigned int size)
{
	return ((size * 85) >> 20) + 1;
}
#define DESC_NEEDED (MAX_SKB_FRAGS + 6)
#define IAVF_MIN_DESC_PENDING	4
#define IAVF_TX_FLAGS_HW_VLAN			BIT(1)
#define IAVF_TX_FLAGS_SW_VLAN			BIT(2)
#define IAVF_TX_FLAGS_TSO			BIT(3)
#define IAVF_TX_FLAGS_IPV4			BIT(4)
#define IAVF_TX_FLAGS_IPV6			BIT(5)
#define IAVF_TX_FLAGS_FCCRC			BIT(6)
#define IAVF_TX_FLAGS_FSO			BIT(7)
#define IAVF_TX_FLAGS_FD_SB			BIT(9)
#define IAVF_TX_FLAGS_VXLAN_TUNNEL		BIT(10)
#define IAVF_TX_FLAGS_HW_OUTER_SINGLE_VLAN	BIT(11)
#define IAVF_TX_FLAGS_VLAN_MASK			0xffff0000
#define IAVF_TX_FLAGS_VLAN_PRIO_MASK		0xe0000000
#define IAVF_TX_FLAGS_VLAN_PRIO_SHIFT		29
#define IAVF_TX_FLAGS_VLAN_SHIFT		16
struct iavf_tx_buffer {
	struct iavf_tx_desc *next_to_watch;
	union {
		struct sk_buff *skb;
		void *raw_buf;
	};
	unsigned int bytecount;
	unsigned short gso_segs;
	DEFINE_DMA_UNMAP_ADDR(dma);
	DEFINE_DMA_UNMAP_LEN(len);
	u32 tx_flags;
};
struct iavf_rx_buffer {
	dma_addr_t dma;
	struct page *page;
#if (BITS_PER_LONG > 32) || (PAGE_SIZE >= 65536)
	__u32 page_offset;
#else
	__u16 page_offset;
#endif
	__u16 pagecnt_bias;
};
struct iavf_queue_stats {
	u64 packets;
	u64 bytes;
};
struct iavf_tx_queue_stats {
	u64 restart_queue;
	u64 tx_busy;
	u64 tx_done_old;
	u64 tx_linearize;
	u64 tx_force_wb;
	int prev_pkt_ctr;
	u64 tx_lost_interrupt;
};
struct iavf_rx_queue_stats {
	u64 non_eop_descs;
	u64 alloc_page_failed;
	u64 alloc_buff_failed;
	u64 page_reuse_count;
	u64 realloc_count;
};
enum iavf_ring_state_t {
	__IAVF_TX_FDIR_INIT_DONE,
	__IAVF_TX_XPS_INIT_DONE,
	__IAVF_RING_STATE_NBITS  
};
#define IAVF_RX_DTYPE_NO_SPLIT      0
#define IAVF_RX_DTYPE_HEADER_SPLIT  1
#define IAVF_RX_DTYPE_SPLIT_ALWAYS  2
#define IAVF_RX_SPLIT_L2      0x1
#define IAVF_RX_SPLIT_IP      0x2
#define IAVF_RX_SPLIT_TCP_UDP 0x4
#define IAVF_RX_SPLIT_SCTP    0x8
struct iavf_ring {
	struct iavf_ring *next;		 
	void *desc;			 
	struct device *dev;		 
	struct net_device *netdev;	 
	union {
		struct iavf_tx_buffer *tx_bi;
		struct iavf_rx_buffer *rx_bi;
	};
	DECLARE_BITMAP(state, __IAVF_RING_STATE_NBITS);
	u16 queue_index;		 
	u8 dcb_tc;			 
	u8 __iomem *tail;
	u16 itr_setting;
	u16 count;			 
	u16 reg_idx;			 
	u16 rx_buf_len;
	u16 next_to_use;
	u16 next_to_clean;
	u8 atr_sample_rate;
	u8 atr_count;
	bool ring_active;		 
	bool arm_wb;		 
	u8 packet_stride;
	u16 flags;
#define IAVF_TXR_FLAGS_WB_ON_ITR		BIT(0)
#define IAVF_RXR_FLAGS_BUILD_SKB_ENABLED	BIT(1)
#define IAVF_TXRX_FLAGS_VLAN_TAG_LOC_L2TAG1	BIT(3)
#define IAVF_TXR_FLAGS_VLAN_TAG_LOC_L2TAG2	BIT(4)
#define IAVF_RXR_FLAGS_VLAN_TAG_LOC_L2TAG2_2	BIT(5)
	struct iavf_queue_stats	stats;
	struct u64_stats_sync syncp;
	union {
		struct iavf_tx_queue_stats tx_stats;
		struct iavf_rx_queue_stats rx_stats;
	};
	unsigned int size;		 
	dma_addr_t dma;			 
	struct iavf_vsi *vsi;		 
	struct iavf_q_vector *q_vector;	 
	struct rcu_head rcu;		 
	u16 next_to_alloc;
	struct sk_buff *skb;		 
} ____cacheline_internodealigned_in_smp;
static inline bool ring_uses_build_skb(struct iavf_ring *ring)
{
	return !!(ring->flags & IAVF_RXR_FLAGS_BUILD_SKB_ENABLED);
}
static inline void set_ring_build_skb_enabled(struct iavf_ring *ring)
{
	ring->flags |= IAVF_RXR_FLAGS_BUILD_SKB_ENABLED;
}
static inline void clear_ring_build_skb_enabled(struct iavf_ring *ring)
{
	ring->flags &= ~IAVF_RXR_FLAGS_BUILD_SKB_ENABLED;
}
#define IAVF_ITR_ADAPTIVE_MIN_INC	0x0002
#define IAVF_ITR_ADAPTIVE_MIN_USECS	0x0002
#define IAVF_ITR_ADAPTIVE_MAX_USECS	0x007e
#define IAVF_ITR_ADAPTIVE_LATENCY	0x8000
#define IAVF_ITR_ADAPTIVE_BULK		0x0000
#define ITR_IS_BULK(x) (!((x) & IAVF_ITR_ADAPTIVE_LATENCY))
struct iavf_ring_container {
	struct iavf_ring *ring;		 
	unsigned long next_update;	 
	unsigned int total_bytes;	 
	unsigned int total_packets;	 
	u16 count;
	u16 target_itr;			 
	u16 current_itr;		 
};
#define iavf_for_each_ring(pos, head) \
	for (pos = (head).ring; pos != NULL; pos = pos->next)
static inline unsigned int iavf_rx_pg_order(struct iavf_ring *ring)
{
#if (PAGE_SIZE < 8192)
	if (ring->rx_buf_len > (PAGE_SIZE / 2))
		return 1;
#endif
	return 0;
}
#define iavf_rx_pg_size(_ring) (PAGE_SIZE << iavf_rx_pg_order(_ring))
bool iavf_alloc_rx_buffers(struct iavf_ring *rxr, u16 cleaned_count);
netdev_tx_t iavf_xmit_frame(struct sk_buff *skb, struct net_device *netdev);
int iavf_setup_tx_descriptors(struct iavf_ring *tx_ring);
int iavf_setup_rx_descriptors(struct iavf_ring *rx_ring);
void iavf_free_tx_resources(struct iavf_ring *tx_ring);
void iavf_free_rx_resources(struct iavf_ring *rx_ring);
int iavf_napi_poll(struct napi_struct *napi, int budget);
void iavf_detect_recover_hung(struct iavf_vsi *vsi);
int __iavf_maybe_stop_tx(struct iavf_ring *tx_ring, int size);
bool __iavf_chk_linearize(struct sk_buff *skb);
static inline int iavf_xmit_descriptor_count(struct sk_buff *skb)
{
	const skb_frag_t *frag = &skb_shinfo(skb)->frags[0];
	unsigned int nr_frags = skb_shinfo(skb)->nr_frags;
	int count = 0, size = skb_headlen(skb);
	for (;;) {
		count += iavf_txd_use_count(size);
		if (!nr_frags--)
			break;
		size = skb_frag_size(frag++);
	}
	return count;
}
static inline int iavf_maybe_stop_tx(struct iavf_ring *tx_ring, int size)
{
	if (likely(IAVF_DESC_UNUSED(tx_ring) >= size))
		return 0;
	return __iavf_maybe_stop_tx(tx_ring, size);
}
static inline bool iavf_chk_linearize(struct sk_buff *skb, int count)
{
	if (likely(count < IAVF_MAX_BUFFER_TXD))
		return false;
	if (skb_is_gso(skb))
		return __iavf_chk_linearize(skb);
	return count != IAVF_MAX_BUFFER_TXD;
}
static inline struct netdev_queue *txring_txq(const struct iavf_ring *ring)
{
	return netdev_get_tx_queue(ring->netdev, ring->queue_index);
}
#endif  
