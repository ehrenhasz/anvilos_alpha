#ifndef _GVE_DESC_H_
#define _GVE_DESC_H_
#include <linux/build_bug.h>
struct gve_tx_pkt_desc {
	u8	type_flags;   
	u8	l4_csum_offset;   
	u8	l4_hdr_offset;   
	u8	desc_cnt;   
	__be16	len;   
	__be16	seg_len;   
	__be64	seg_addr;   
} __packed;
struct gve_tx_mtd_desc {
	u8      type_flags;      
	u8      path_state;      
	__be16  reserved0;
	__be32  path_hash;
	__be64  reserved1;
} __packed;
struct gve_tx_seg_desc {
	u8	type_flags;	 
	u8	l3_offset;	 
	__be16	reserved;
	__be16	mss;		 
	__be16	seg_len;
	__be64	seg_addr;
} __packed;
#define	GVE_TXD_STD		(0x0 << 4)  
#define	GVE_TXD_TSO		(0x1 << 4)  
#define	GVE_TXD_SEG		(0x2 << 4)  
#define	GVE_TXD_MTD		(0x3 << 4)  
#define	GVE_TXF_L4CSUM	BIT(0)	 
#define	GVE_TXF_TSTAMP	BIT(2)	 
#define	GVE_TXSF_IPV6	BIT(1)	 
#define GVE_MTD_SUBTYPE_PATH		0
#define GVE_MTD_PATH_STATE_DEFAULT	0
#define GVE_MTD_PATH_STATE_TIMEOUT	1
#define GVE_MTD_PATH_STATE_CONGESTION	2
#define GVE_MTD_PATH_STATE_RETRANSMIT	3
#define GVE_MTD_PATH_HASH_NONE         (0x0 << 4)
#define GVE_MTD_PATH_HASH_L4           (0x1 << 4)
#define GVE_RX_PAD 2
struct gve_rx_desc {
	u8	padding[48];
	__be32	rss_hash;   
	__be16	mss;
	__be16	reserved;   
	u8	hdr_len;   
	u8	hdr_off;   
	__sum16	csum;   
	__be16	len;   
	__be16	flags_seq;   
} __packed;
static_assert(sizeof(struct gve_rx_desc) == 64);
union gve_rx_data_slot {
	__be64 qpl_offset;
	__be64 addr;
};
#define GVE_SEQNO(x) (be16_to_cpu(x) & 0x7)
#define GVE_RXFLG(x)	cpu_to_be16(1 << (3 + (x)))
#define	GVE_RXF_FRAG		GVE_RXFLG(3)	 
#define	GVE_RXF_IPV4		GVE_RXFLG(4)	 
#define	GVE_RXF_IPV6		GVE_RXFLG(5)	 
#define	GVE_RXF_TCP		GVE_RXFLG(6)	 
#define	GVE_RXF_UDP		GVE_RXFLG(7)	 
#define	GVE_RXF_ERR		GVE_RXFLG(8)	 
#define	GVE_RXF_PKT_CONT	GVE_RXFLG(10)	 
#define GVE_IRQ_ACK	BIT(31)
#define GVE_IRQ_MASK	BIT(30)
#define GVE_IRQ_EVENT	BIT(29)
static inline bool gve_needs_rss(__be16 flag)
{
	if (flag & GVE_RXF_FRAG)
		return false;
	if (flag & (GVE_RXF_IPV4 | GVE_RXF_IPV6))
		return true;
	return false;
}
static inline u8 gve_next_seqno(u8 seq)
{
	return (seq + 1) == 8 ? 1 : seq + 1;
}
#endif  
