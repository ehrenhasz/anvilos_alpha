 
 

#ifndef _I40E_TXRX_H_
#define _I40E_TXRX_H_

#include <net/xdp.h>

 
#define I40E_DEFAULT_IRQ_WORK      256

 
#define I40E_ITR_DYNAMIC	0x8000	 
#define I40E_ITR_MASK		0x1FFE	 
#define I40E_MIN_ITR		     2	 
#define I40E_ITR_20K		    50
#define I40E_ITR_8K		   122
#define I40E_MAX_ITR		  8160	 
#define ITR_TO_REG(setting) ((setting) & ~I40E_ITR_DYNAMIC)
#define ITR_REG_ALIGN(setting) __ALIGN_MASK(setting, ~I40E_ITR_MASK)
#define ITR_IS_DYNAMIC(setting) (!!((setting) & I40E_ITR_DYNAMIC))

#define I40E_ITR_RX_DEF		(I40E_ITR_20K | I40E_ITR_DYNAMIC)
#define I40E_ITR_TX_DEF		(I40E_ITR_20K | I40E_ITR_DYNAMIC)

 
#define INTRL_ENA                  BIT(6)
#define I40E_MAX_INTRL             0x3B     
#define INTRL_REG_TO_USEC(intrl) ((intrl & ~INTRL_ENA) << 2)

 
static inline u16 i40e_intrl_usec_to_reg(int intrl)
{
	if (intrl >> 2)
		return ((intrl >> 2) | INTRL_ENA);
	else
		return 0;
}

#define I40E_QUEUE_END_OF_LIST 0x7FF

 
enum i40e_dyn_idx_t {
	I40E_IDX_ITR0 = 0,
	I40E_IDX_ITR1 = 1,
	I40E_IDX_ITR2 = 2,
	I40E_ITR_NONE = 3	 
};

 
#define I40E_RX_ITR    I40E_IDX_ITR0
#define I40E_TX_ITR    I40E_IDX_ITR1

 
#define I40E_DEFAULT_RSS_HENA ( \
	BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_UDP) | \
	BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_SCTP) | \
	BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_TCP) | \
	BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_OTHER) | \
	BIT_ULL(I40E_FILTER_PCTYPE_FRAG_IPV4) | \
	BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_UDP) | \
	BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_TCP) | \
	BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_SCTP) | \
	BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_OTHER) | \
	BIT_ULL(I40E_FILTER_PCTYPE_FRAG_IPV6) | \
	BIT_ULL(I40E_FILTER_PCTYPE_L2_PAYLOAD))

#define I40E_DEFAULT_RSS_HENA_EXPANDED (I40E_DEFAULT_RSS_HENA | \
	BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK) | \
	BIT_ULL(I40E_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP) | \
	BIT_ULL(I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP) | \
	BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK) | \
	BIT_ULL(I40E_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP) | \
	BIT_ULL(I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP))

#define i40e_pf_get_default_rss_hena(pf) \
	(((pf)->hw_features & I40E_HW_MULTIPLE_TCP_UDP_RSS_PCTYPE) ? \
	  I40E_DEFAULT_RSS_HENA_EXPANDED : I40E_DEFAULT_RSS_HENA)

 
#define I40E_RXBUFFER_256   256
#define I40E_RXBUFFER_1536  1536   
#define I40E_RXBUFFER_2048  2048
#define I40E_RXBUFFER_3072  3072   
#define I40E_MAX_RXBUFFER   9728   

 
#define I40E_RX_HDR_SIZE I40E_RXBUFFER_256
#define I40E_PACKET_HDR_PAD (ETH_HLEN + ETH_FCS_LEN + (VLAN_HLEN * 2))
#define i40e_rx_desc i40e_16byte_rx_desc

#define I40E_RX_DMA_ATTR \
	(DMA_ATTR_SKIP_CPU_SYNC | DMA_ATTR_WEAK_ORDERING)

 
#if (PAGE_SIZE < 8192)
#define I40E_2K_TOO_SMALL_WITH_PADDING \
((NET_SKB_PAD + I40E_RXBUFFER_1536) > SKB_WITH_OVERHEAD(I40E_RXBUFFER_2048))

static inline int i40e_compute_pad(int rx_buf_len)
{
	int page_size, pad_size;

	page_size = ALIGN(rx_buf_len, PAGE_SIZE / 2);
	pad_size = SKB_WITH_OVERHEAD(page_size) - rx_buf_len;

	return pad_size;
}

static inline int i40e_skb_pad(void)
{
	int rx_buf_len;

	 
	if (I40E_2K_TOO_SMALL_WITH_PADDING)
		rx_buf_len = I40E_RXBUFFER_3072 + SKB_DATA_ALIGN(NET_IP_ALIGN);
	else
		rx_buf_len = I40E_RXBUFFER_1536;

	 
	rx_buf_len -= NET_IP_ALIGN;

	return i40e_compute_pad(rx_buf_len);
}

#define I40E_SKB_PAD i40e_skb_pad()
#else
#define I40E_2K_TOO_SMALL_WITH_PADDING false
#define I40E_SKB_PAD (NET_SKB_PAD + NET_IP_ALIGN)
#endif

 
static inline bool i40e_test_staterr(union i40e_rx_desc *rx_desc,
				     const u64 stat_err_bits)
{
	return !!(rx_desc->wb.qword1.status_error_len &
		  cpu_to_le64(stat_err_bits));
}

 
#define I40E_RX_BUFFER_WRITE	32	 

#define I40E_RX_NEXT_DESC(r, i, n)		\
	do {					\
		(i)++;				\
		if ((i) == (r)->count)		\
			i = 0;			\
		(n) = I40E_RX_DESC((r), (i));	\
	} while (0)


#define I40E_MAX_BUFFER_TXD	8
#define I40E_MIN_TX_LEN		17

 
#define I40E_MAX_READ_REQ_SIZE		4096
#define I40E_MAX_DATA_PER_TXD		(16 * 1024 - 1)
#define I40E_MAX_DATA_PER_TXD_ALIGNED \
	(I40E_MAX_DATA_PER_TXD & ~(I40E_MAX_READ_REQ_SIZE - 1))

 
static inline unsigned int i40e_txd_use_count(unsigned int size)
{
	return ((size * 85) >> 20) + 1;
}

 
#define DESC_NEEDED (MAX_SKB_FRAGS + 6)

#define I40E_TX_FLAGS_HW_VLAN		BIT(1)
#define I40E_TX_FLAGS_SW_VLAN		BIT(2)
#define I40E_TX_FLAGS_TSO		BIT(3)
#define I40E_TX_FLAGS_IPV4		BIT(4)
#define I40E_TX_FLAGS_IPV6		BIT(5)
#define I40E_TX_FLAGS_TSYN		BIT(8)
#define I40E_TX_FLAGS_FD_SB		BIT(9)
#define I40E_TX_FLAGS_UDP_TUNNEL	BIT(10)
#define I40E_TX_FLAGS_VLAN_MASK		0xffff0000
#define I40E_TX_FLAGS_VLAN_PRIO_MASK	0xe0000000
#define I40E_TX_FLAGS_VLAN_PRIO_SHIFT	29
#define I40E_TX_FLAGS_VLAN_SHIFT	16

struct i40e_tx_buffer {
	struct i40e_tx_desc *next_to_watch;
	union {
		struct xdp_frame *xdpf;
		struct sk_buff *skb;
		void *raw_buf;
	};
	unsigned int bytecount;
	unsigned short gso_segs;

	DEFINE_DMA_UNMAP_ADDR(dma);
	DEFINE_DMA_UNMAP_LEN(len);
	u32 tx_flags;
};

struct i40e_rx_buffer {
	dma_addr_t dma;
	struct page *page;
	__u32 page_offset;
	__u16 pagecnt_bias;
	__u32 page_count;
};

struct i40e_queue_stats {
	u64 packets;
	u64 bytes;
};

struct i40e_tx_queue_stats {
	u64 restart_queue;
	u64 tx_busy;
	u64 tx_done_old;
	u64 tx_linearize;
	u64 tx_force_wb;
	u64 tx_stopped;
	int prev_pkt_ctr;
};

struct i40e_rx_queue_stats {
	u64 non_eop_descs;
	u64 alloc_page_failed;
	u64 alloc_buff_failed;
	u64 page_reuse_count;
	u64 page_alloc_count;
	u64 page_waive_count;
	u64 page_busy_count;
};

enum i40e_ring_state_t {
	__I40E_TX_FDIR_INIT_DONE,
	__I40E_TX_XPS_INIT_DONE,
	__I40E_RING_STATE_NBITS  
};

 
#define I40E_RX_DTYPE_HEADER_SPLIT  1
#define I40E_RX_SPLIT_L2      0x1
#define I40E_RX_SPLIT_IP      0x2
#define I40E_RX_SPLIT_TCP_UDP 0x4
#define I40E_RX_SPLIT_SCTP    0x8

 
struct i40e_ring {
	struct i40e_ring *next;		 
	void *desc;			 
	struct device *dev;		 
	struct net_device *netdev;	 
	struct bpf_prog *xdp_prog;
	union {
		struct i40e_tx_buffer *tx_bi;
		struct i40e_rx_buffer *rx_bi;
		struct xdp_buff **rx_bi_zc;
	};
	DECLARE_BITMAP(state, __I40E_RING_STATE_NBITS);
	u16 queue_index;		 
	u8 dcb_tc;			 
	u8 __iomem *tail;

	 
	struct xdp_buff xdp;

	 
	u16 next_to_process;
	 
	u16 itr_setting;

	u16 count;			 
	u16 reg_idx;			 
	u16 rx_buf_len;

	 
	u16 next_to_use;
	u16 next_to_clean;
	u16 xdp_tx_active;

	u8 atr_sample_rate;
	u8 atr_count;

	bool ring_active;		 
	bool arm_wb;		 
	u8 packet_stride;

	u16 flags;
#define I40E_TXR_FLAGS_WB_ON_ITR		BIT(0)
#define I40E_RXR_FLAGS_BUILD_SKB_ENABLED	BIT(1)
#define I40E_TXR_FLAGS_XDP			BIT(2)

	 
	struct i40e_queue_stats	stats;
	struct u64_stats_sync syncp;
	union {
		struct i40e_tx_queue_stats tx_stats;
		struct i40e_rx_queue_stats rx_stats;
	};

	unsigned int size;		 
	dma_addr_t dma;			 

	struct i40e_vsi *vsi;		 
	struct i40e_q_vector *q_vector;	 

	struct rcu_head rcu;		 
	u16 next_to_alloc;

	struct i40e_channel *ch;
	u16 rx_offset;
	struct xdp_rxq_info xdp_rxq;
	struct xsk_buff_pool *xsk_pool;
} ____cacheline_internodealigned_in_smp;

static inline bool ring_uses_build_skb(struct i40e_ring *ring)
{
	return !!(ring->flags & I40E_RXR_FLAGS_BUILD_SKB_ENABLED);
}

static inline void set_ring_build_skb_enabled(struct i40e_ring *ring)
{
	ring->flags |= I40E_RXR_FLAGS_BUILD_SKB_ENABLED;
}

static inline void clear_ring_build_skb_enabled(struct i40e_ring *ring)
{
	ring->flags &= ~I40E_RXR_FLAGS_BUILD_SKB_ENABLED;
}

static inline bool ring_is_xdp(struct i40e_ring *ring)
{
	return !!(ring->flags & I40E_TXR_FLAGS_XDP);
}

static inline void set_ring_xdp(struct i40e_ring *ring)
{
	ring->flags |= I40E_TXR_FLAGS_XDP;
}

#define I40E_ITR_ADAPTIVE_MIN_INC	0x0002
#define I40E_ITR_ADAPTIVE_MIN_USECS	0x0002
#define I40E_ITR_ADAPTIVE_MAX_USECS	0x007e
#define I40E_ITR_ADAPTIVE_LATENCY	0x8000
#define I40E_ITR_ADAPTIVE_BULK		0x0000

struct i40e_ring_container {
	struct i40e_ring *ring;		 
	unsigned long next_update;	 
	unsigned int total_bytes;	 
	unsigned int total_packets;	 
	u16 count;
	u16 target_itr;			 
	u16 current_itr;		 
};

 
#define i40e_for_each_ring(pos, head) \
	for (pos = (head).ring; pos != NULL; pos = pos->next)

static inline unsigned int i40e_rx_pg_order(struct i40e_ring *ring)
{
#if (PAGE_SIZE < 8192)
	if (ring->rx_buf_len > (PAGE_SIZE / 2))
		return 1;
#endif
	return 0;
}

#define i40e_rx_pg_size(_ring) (PAGE_SIZE << i40e_rx_pg_order(_ring))

bool i40e_alloc_rx_buffers(struct i40e_ring *rxr, u16 cleaned_count);
netdev_tx_t i40e_lan_xmit_frame(struct sk_buff *skb, struct net_device *netdev);
u16 i40e_lan_select_queue(struct net_device *netdev, struct sk_buff *skb,
			  struct net_device *sb_dev);
void i40e_clean_tx_ring(struct i40e_ring *tx_ring);
void i40e_clean_rx_ring(struct i40e_ring *rx_ring);
int i40e_setup_tx_descriptors(struct i40e_ring *tx_ring);
int i40e_setup_rx_descriptors(struct i40e_ring *rx_ring);
void i40e_free_tx_resources(struct i40e_ring *tx_ring);
void i40e_free_rx_resources(struct i40e_ring *rx_ring);
int i40e_napi_poll(struct napi_struct *napi, int budget);
void i40e_force_wb(struct i40e_vsi *vsi, struct i40e_q_vector *q_vector);
u32 i40e_get_tx_pending(struct i40e_ring *ring, bool in_sw);
void i40e_detect_recover_hung(struct i40e_vsi *vsi);
int __i40e_maybe_stop_tx(struct i40e_ring *tx_ring, int size);
bool __i40e_chk_linearize(struct sk_buff *skb);
int i40e_xdp_xmit(struct net_device *dev, int n, struct xdp_frame **frames,
		  u32 flags);
bool i40e_is_non_eop(struct i40e_ring *rx_ring,
		     union i40e_rx_desc *rx_desc);

 
static inline u32 i40e_get_head(struct i40e_ring *tx_ring)
{
	void *head = (struct i40e_tx_desc *)tx_ring->desc + tx_ring->count;

	return le32_to_cpu(*(volatile __le32 *)head);
}

 
static inline int i40e_xmit_descriptor_count(struct sk_buff *skb)
{
	const skb_frag_t *frag = &skb_shinfo(skb)->frags[0];
	unsigned int nr_frags = skb_shinfo(skb)->nr_frags;
	int count = 0, size = skb_headlen(skb);

	for (;;) {
		count += i40e_txd_use_count(size);

		if (!nr_frags--)
			break;

		size = skb_frag_size(frag++);
	}

	return count;
}

 
static inline int i40e_maybe_stop_tx(struct i40e_ring *tx_ring, int size)
{
	if (likely(I40E_DESC_UNUSED(tx_ring) >= size))
		return 0;
	return __i40e_maybe_stop_tx(tx_ring, size);
}

 
static inline bool i40e_chk_linearize(struct sk_buff *skb, int count)
{
	 
	if (likely(count < I40E_MAX_BUFFER_TXD))
		return false;

	if (skb_is_gso(skb))
		return __i40e_chk_linearize(skb);

	 
	return count != I40E_MAX_BUFFER_TXD;
}

 
static inline struct netdev_queue *txring_txq(const struct i40e_ring *ring)
{
	return netdev_get_tx_queue(ring->netdev, ring->queue_index);
}
#endif  
