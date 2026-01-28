#ifndef WIL6210_TXRX_EDMA_H
#define WIL6210_TXRX_EDMA_H
#include "wil6210.h"
#define WIL_SRING_SIZE_ORDER_MIN	(WIL_RING_SIZE_ORDER_MIN)
#define WIL_SRING_SIZE_ORDER_MAX	(WIL_RING_SIZE_ORDER_MAX)
#define WIL_RX_SRING_SIZE_ORDER_DEFAULT	(12)
#define WIL_TX_SRING_SIZE_ORDER_DEFAULT	(14)
#define WIL_RX_BUFF_ARR_SIZE_DEFAULT (2600)
#define WIL_DEFAULT_RX_STATUS_RING_ID 0
#define WIL_RX_DESC_RING_ID 0
#define WIL_RX_STATUS_IRQ_IDX 0
#define WIL_TX_STATUS_IRQ_IDX 1
#define WIL_EDMA_AGG_WATERMARK (0xffff)
#define WIL_EDMA_AGG_WATERMARK_POS (16)
#define WIL_EDMA_IDLE_TIME_LIMIT_USEC (50)
#define WIL_EDMA_TIME_UNIT_CLK_CYCLES (330)  
#define WIL_RX_EDMA_ERROR_MIC	(1)
#define WIL_RX_EDMA_ERROR_KEY	(2)  
#define WIL_RX_EDMA_ERROR_REPLAY	(3)
#define WIL_RX_EDMA_ERROR_AMSDU	(4)
#define WIL_RX_EDMA_ERROR_FCS	(7)
#define WIL_RX_EDMA_ERROR_L3_ERR	(BIT(0) | BIT(1))
#define WIL_RX_EDMA_ERROR_L4_ERR	(BIT(0) | BIT(1))
#define WIL_RX_EDMA_DLPF_LU_MISS_BIT		BIT(11)
#define WIL_RX_EDMA_DLPF_LU_MISS_CID_TID_MASK	0x7
#define WIL_RX_EDMA_DLPF_LU_HIT_CID_TID_MASK	0xf
#define WIL_RX_EDMA_DLPF_LU_MISS_CID_POS	2
#define WIL_RX_EDMA_DLPF_LU_HIT_CID_POS		4
#define WIL_RX_EDMA_DLPF_LU_MISS_TID_POS	5
#define WIL_RX_EDMA_MID_VALID_BIT		BIT(20)
#define WIL_EDMA_DESC_TX_MAC_CFG_0_QID_POS 16
#define WIL_EDMA_DESC_TX_MAC_CFG_0_QID_LEN 6
#define WIL_EDMA_DESC_TX_CFG_EOP_POS 0
#define WIL_EDMA_DESC_TX_CFG_EOP_LEN 1
#define WIL_EDMA_DESC_TX_CFG_TSO_DESC_TYPE_POS 3
#define WIL_EDMA_DESC_TX_CFG_TSO_DESC_TYPE_LEN 2
#define WIL_EDMA_DESC_TX_CFG_SEG_EN_POS 5
#define WIL_EDMA_DESC_TX_CFG_SEG_EN_LEN 1
#define WIL_EDMA_DESC_TX_CFG_INSERT_IP_CHKSUM_POS 6
#define WIL_EDMA_DESC_TX_CFG_INSERT_IP_CHKSUM_LEN 1
#define WIL_EDMA_DESC_TX_CFG_INSERT_TCP_CHKSUM_POS 7
#define WIL_EDMA_DESC_TX_CFG_INSERT_TCP_CHKSUM_LEN 1
#define WIL_EDMA_DESC_TX_CFG_L4_TYPE_POS 15
#define WIL_EDMA_DESC_TX_CFG_L4_TYPE_LEN 1
#define WIL_EDMA_DESC_TX_CFG_PSEUDO_HEADER_CALC_EN_POS 5
#define WIL_EDMA_DESC_TX_CFG_PSEUDO_HEADER_CALC_EN_LEN 1
struct wil_ring_rx_enhanced_mac {
	u32 d[3];
	__le16 buff_id;
	u16 reserved;
} __packed;
struct wil_ring_rx_enhanced_dma {
	u32 d0;
	struct wil_ring_dma_addr addr;
	u16 w5;
	__le16 addr_high_high;
	__le16 length;
} __packed;
struct wil_rx_enhanced_desc {
	struct wil_ring_rx_enhanced_mac mac;
	struct wil_ring_rx_enhanced_dma dma;
} __packed;
struct wil_ring_tx_enhanced_dma {
	u8 l4_hdr_len;
	u8 cmd;
	u16 w1;
	struct wil_ring_dma_addr addr;
	u8  ip_length;
	u8  b11;        
	__le16 addr_high_high;
	__le16 length;
} __packed;
struct wil_ring_tx_enhanced_mac {
	u32 d[3];
	__le16 tso_mss;
	u16 scratchpad;
} __packed;
struct wil_tx_enhanced_desc {
	struct wil_ring_tx_enhanced_mac mac;
	struct wil_ring_tx_enhanced_dma dma;
} __packed;
#define TX_STATUS_DESC_READY_POS 7
struct wil_ring_tx_status {
	u8 num_descriptors;
	u8 ring_id;
	u8 status;
	u8 desc_ready;  
	u32 timestamp;
	u32 d2;
	u16 seq_number;  
	u16 w7;
} __packed;
struct wil_rx_status_compressed {
	u32 d0;
	u32 d1;
	__le16 buff_id;
	__le16 length;
	u32 timestamp;
} __packed;
struct wil_rx_status_extension {
	u32 d0;
	u32 d1;
	__le16 seq_num;  
	struct_group_attr(pn, __packed,
		u16 pn_15_0;
		u32 pn_47_16;
	);
} __packed;
struct wil_rx_status_extended {
	struct wil_rx_status_compressed comp;
	struct wil_rx_status_extension ext;
} __packed;
static inline void *wil_skb_rxstatus(struct sk_buff *skb)
{
	return (void *)skb->cb;
}
static inline __le16 wil_rx_status_get_length(void *msg)
{
	return ((struct wil_rx_status_compressed *)msg)->length;
}
static inline u8 wil_rx_status_get_mcs(void *msg)
{
	return WIL_GET_BITS(((struct wil_rx_status_compressed *)msg)->d1,
			    16, 21);
}
static inline u8 wil_rx_status_get_cb_mode(void *msg)
{
	return WIL_GET_BITS(((struct wil_rx_status_compressed *)msg)->d1,
			    22, 23);
}
static inline u16 wil_rx_status_get_flow_id(void *msg)
{
	return WIL_GET_BITS(((struct wil_rx_status_compressed *)msg)->d0,
			    8, 19);
}
static inline u8 wil_rx_status_get_mcast(void *msg)
{
	return WIL_GET_BITS(((struct wil_rx_status_compressed *)msg)->d0,
			    26, 26);
}
static inline u8 wil_rx_status_get_cid(void *msg)
{
	u16 val = wil_rx_status_get_flow_id(msg);
	if (val & WIL_RX_EDMA_DLPF_LU_MISS_BIT)
		return (val >> WIL_RX_EDMA_DLPF_LU_MISS_CID_POS) &
			WIL_RX_EDMA_DLPF_LU_MISS_CID_TID_MASK;
	else
		return (val >> WIL_RX_EDMA_DLPF_LU_HIT_CID_POS) &
			WIL_RX_EDMA_DLPF_LU_HIT_CID_TID_MASK;
}
static inline u8 wil_rx_status_get_tid(void *msg)
{
	u16 val = wil_rx_status_get_flow_id(msg);
	if (val & WIL_RX_EDMA_DLPF_LU_MISS_BIT)
		return (val >> WIL_RX_EDMA_DLPF_LU_MISS_TID_POS) &
			WIL_RX_EDMA_DLPF_LU_MISS_CID_TID_MASK;
	else
		return val & WIL_RX_EDMA_DLPF_LU_MISS_CID_TID_MASK;
}
static inline int wil_rx_status_get_eop(void *msg)  
{
	return WIL_GET_BITS(((struct wil_rx_status_compressed *)msg)->d0,
			    30, 30);
}
static inline void wil_rx_status_reset_buff_id(struct wil_status_ring *s)
{
	((struct wil_rx_status_compressed *)
		(s->va + (s->elem_size * s->swhead)))->buff_id = 0;
}
static inline __le16 wil_rx_status_get_buff_id(void *msg)
{
	return ((struct wil_rx_status_compressed *)msg)->buff_id;
}
static inline u8 wil_rx_status_get_data_offset(void *msg)
{
	u8 val = WIL_GET_BITS(((struct wil_rx_status_compressed *)msg)->d1,
			      24, 27);
	switch (val) {
	case 0: return 0;
	case 3: return 2;
	default: return 0xFF;
	}
}
static inline int wil_rx_status_get_frame_type(struct wil6210_priv *wil,
					       void *msg)
{
	if (wil->use_compressed_rx_status)
		return IEEE80211_FTYPE_DATA;
	return WIL_GET_BITS(((struct wil_rx_status_extended *)msg)->ext.d1,
			    0, 1) << 2;
}
static inline int wil_rx_status_get_fc1(struct wil6210_priv *wil, void *msg)
{
	if (wil->use_compressed_rx_status)
		return 0;
	return WIL_GET_BITS(((struct wil_rx_status_extended *)msg)->ext.d1,
			    0, 5) << 2;
}
static inline __le16 wil_rx_status_get_seq(struct wil6210_priv *wil, void *msg)
{
	if (wil->use_compressed_rx_status)
		return 0;
	return ((struct wil_rx_status_extended *)msg)->ext.seq_num;
}
static inline u8 wil_rx_status_get_retry(void *msg)
{
	return 1;
}
static inline int wil_rx_status_get_mid(void *msg)
{
	if (!(((struct wil_rx_status_compressed *)msg)->d0 &
	    WIL_RX_EDMA_MID_VALID_BIT))
		return 0;  
	return WIL_GET_BITS(((struct wil_rx_status_compressed *)msg)->d0,
			    21, 22);
}
static inline int wil_rx_status_get_error(void *msg)
{
	return WIL_GET_BITS(((struct wil_rx_status_compressed *)msg)->d0,
			    29, 29);
}
static inline int wil_rx_status_get_l2_rx_status(void *msg)
{
	return WIL_GET_BITS(((struct wil_rx_status_compressed *)msg)->d0,
			    0, 2);
}
static inline int wil_rx_status_get_l3_rx_status(void *msg)
{
	return WIL_GET_BITS(((struct wil_rx_status_compressed *)msg)->d0,
			    3, 4);
}
static inline int wil_rx_status_get_l4_rx_status(void *msg)
{
	return WIL_GET_BITS(((struct wil_rx_status_compressed *)msg)->d0,
			    5, 6);
}
static inline int wil_rx_status_get_checksum(void *msg,
					     struct wil_net_stats *stats)
{
	int l3_rx_status = wil_rx_status_get_l3_rx_status(msg);
	int l4_rx_status = wil_rx_status_get_l4_rx_status(msg);
	if (l4_rx_status == 1)
		return CHECKSUM_UNNECESSARY;
	if (l4_rx_status == 0 && l3_rx_status == 1)
		return CHECKSUM_UNNECESSARY;
	if (l3_rx_status == 0 && l4_rx_status == 0)
		return CHECKSUM_NONE;
	stats->rx_csum_err++;
	return CHECKSUM_NONE;
}
static inline int wil_rx_status_get_security(void *msg)
{
	return WIL_GET_BITS(((struct wil_rx_status_compressed *)msg)->d0,
			    28, 28);
}
static inline u8 wil_rx_status_get_key_id(void *msg)
{
	return WIL_GET_BITS(((struct wil_rx_status_compressed *)msg)->d1,
			    31, 31);
}
static inline u8 wil_tx_status_get_mcs(struct wil_ring_tx_status *msg)
{
	return WIL_GET_BITS(msg->d2, 0, 4);
}
static inline u32 wil_ring_next_head(struct wil_ring *ring)
{
	return (ring->swhead + 1) % ring->size;
}
static inline void wil_desc_set_addr_edma(struct wil_ring_dma_addr *addr,
					  __le16 *addr_high_high,
					  dma_addr_t pa)
{
	addr->addr_low = cpu_to_le32(lower_32_bits(pa));
	addr->addr_high = cpu_to_le16((u16)upper_32_bits(pa));
	*addr_high_high = cpu_to_le16((u16)(upper_32_bits(pa) >> 16));
}
static inline
dma_addr_t wil_tx_desc_get_addr_edma(struct wil_ring_tx_enhanced_dma *dma)
{
	return le32_to_cpu(dma->addr.addr_low) |
			   ((u64)le16_to_cpu(dma->addr.addr_high) << 32) |
			   ((u64)le16_to_cpu(dma->addr_high_high) << 48);
}
static inline
dma_addr_t wil_rx_desc_get_addr_edma(struct wil_ring_rx_enhanced_dma *dma)
{
	return le32_to_cpu(dma->addr.addr_low) |
			   ((u64)le16_to_cpu(dma->addr.addr_high) << 32) |
			   ((u64)le16_to_cpu(dma->addr_high_high) << 48);
}
void wil_configure_interrupt_moderation_edma(struct wil6210_priv *wil);
int wil_tx_sring_handler(struct wil6210_priv *wil,
			 struct wil_status_ring *sring);
void wil_rx_handle_edma(struct wil6210_priv *wil, int *quota);
void wil_init_txrx_ops_edma(struct wil6210_priv *wil);
#endif  
