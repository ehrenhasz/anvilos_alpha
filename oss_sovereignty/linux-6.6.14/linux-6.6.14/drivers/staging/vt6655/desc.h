#ifndef __DESC_H__
#define __DESC_H__
#include <linux/types.h>
#include <linux/mm.h>
#include "linux/ieee80211.h"
#define B_OWNED_BY_CHIP     1
#define B_OWNED_BY_HOST     0
#define RSR_ADDRBROAD       0x80
#define RSR_ADDRMULTI       0x40
#define RSR_ADDRUNI         0x00
#define RSR_IVLDTYP         0x20
#define RSR_IVLDLEN         0x10         
#define RSR_BSSIDOK         0x08
#define RSR_CRCOK           0x04
#define RSR_BCNSSIDOK       0x02
#define RSR_ADDROK          0x01
#define NEWRSR_DECRYPTOK    0x10
#define NEWRSR_CFPIND       0x08
#define NEWRSR_HWUTSF       0x04
#define NEWRSR_BCNHITAID    0x02
#define NEWRSR_BCNHITAID0   0x01
#define TSR0_PWRSTS1_2      0xC0
#define TSR0_PWRSTS7        0x20
#define TSR0_NCR            0x1F
#define TSR1_TERR           0x80
#define TSR1_PWRSTS4_6      0x70
#define TSR1_RETRYTMO       0x08
#define TSR1_TMO            0x04
#define TSR1_PWRSTS3        0x02
#define ACK_DATA            0x01
#define EDMSDU              0x04         
#define TCR_EDP             0x02         
#define TCR_STP             0x01         
#define CB_MAX_BUF_SIZE     2900U
#define CB_MAX_TX_BUF_SIZE          CB_MAX_BUF_SIZE
#define CB_MAX_RX_BUF_SIZE_NORMAL   CB_MAX_BUF_SIZE
#define CB_BEACON_BUF_SIZE  512U
#define CB_MAX_RX_DESC      128
#define CB_MIN_RX_DESC      16
#define CB_MAX_TX_DESC      64
#define CB_MIN_TX_DESC      16
#define CB_MAX_RECEIVED_PACKETS     16
#define CB_EXTRA_RD_NUM     32
#define CB_RD_NUM           32
#define CB_TD_NUM           32
#define CB_MAX_SEGMENT      4
#define CB_MIN_MAP_REG_NUM  4
#define CB_MAX_MAP_REG_NUM  CB_MAX_TX_DESC
#define CB_PROTOCOL_RESERVED_SECTION    16
#define CB_MAX_TX_ABORT_RETRY   3
#define FIFOCTL_AUTO_FB_1   0x1000
#define FIFOCTL_AUTO_FB_0   0x0800
#define FIFOCTL_GRPACK      0x0400
#define FIFOCTL_11GA        0x0300
#define FIFOCTL_11GB        0x0200
#define FIFOCTL_11B         0x0100
#define FIFOCTL_11A         0x0000
#define FIFOCTL_RTS         0x0080
#define FIFOCTL_ISDMA0      0x0040
#define FIFOCTL_GENINT      0x0020
#define FIFOCTL_TMOEN       0x0010
#define FIFOCTL_LRETRY      0x0008
#define FIFOCTL_CRCDIS      0x0004
#define FIFOCTL_NEEDACK     0x0002
#define FIFOCTL_LHEAD       0x0001
#define FRAGCTL_AES         0x0300
#define FRAGCTL_TKIP        0x0200
#define FRAGCTL_LEGACY      0x0100
#define FRAGCTL_NONENCRYPT  0x0000
#define FRAGCTL_ENDFRAG     0x0003
#define FRAGCTL_MIDFRAG     0x0002
#define FRAGCTL_STAFRAG     0x0001
#define FRAGCTL_NONFRAG     0x0000
#define TYPE_TXDMA0     0
#define TYPE_AC0DMA     1
#define TYPE_ATIMDMA    2
#define TYPE_SYNCDMA    3
#define TYPE_MAXTD      2
#define TYPE_BEACONDMA  4
#define TYPE_RXDMA0     0
#define TYPE_RXDMA1     1
#define TYPE_MAXRD      2
#define TD_FLAGS_NETIF_SKB      0x01     
#define TD_FLAGS_PRIV_SKB       0x02
#define TD_FLAGS_PS_RETRY       0x04     
struct vnt_rd_info {
	struct sk_buff *skb;
	dma_addr_t  skb_dma;
};
struct vnt_rdes0 {
	volatile __le16 res_count;
#ifdef __BIG_ENDIAN
	union {
		volatile u16 f15_reserved;
		struct {
			volatile u8 f8_reserved1;
			volatile u8 owner:1;
			volatile u8 f7_reserved:7;
		} __packed;
	} __packed;
#else
	u16 f15_reserved:15;
	u16 owner:1;
#endif
} __packed;
struct vnt_rdes1 {
	__le16 req_count;
	u16 reserved;
} __packed;
struct vnt_rx_desc {
	volatile struct vnt_rdes0 rd0;
	volatile struct vnt_rdes1 rd1;
	volatile __le32 buff_addr;
	volatile __le32 next_desc;
	struct vnt_rx_desc *next __aligned(8);
	struct vnt_rd_info *rd_info __aligned(8);
} __packed;
struct vnt_tdes0 {
	volatile u8 tsr0;
	volatile u8 tsr1;
#ifdef __BIG_ENDIAN
	union {
		volatile u16 f15_txtime;
		struct {
			volatile u8 f8_reserved;
			volatile u8 owner:1;
			volatile u8 f7_reserved:7;
		} __packed;
	} __packed;
#else
	volatile u16 f15_txtime:15;
	volatile u16 owner:1;
#endif
} __packed;
struct vnt_tdes1 {
	volatile __le16 req_count;
	volatile u8 tcr;
	volatile u8 reserved;
} __packed;
struct vnt_td_info {
	void *mic_hdr;
	struct sk_buff *skb;
	unsigned char *buf;
	dma_addr_t buf_dma;
	u16 req_count;
	u8 flags;
};
struct vnt_tx_desc {
	volatile struct vnt_tdes0 td0;
	volatile struct vnt_tdes1 td1;
	volatile __le32 buff_addr;
	volatile __le32 next_desc;
	struct vnt_tx_desc *next __aligned(8);
	struct vnt_td_info *td_info __aligned(8);
} __packed;
struct vnt_phy_field {
	u8 signal;
	u8 service;
	__le16 len;
} __packed;
union vnt_phy_field_swap {
	struct vnt_phy_field field_read;
	u16 swap[2];
	u32 field_write;
};
#endif  
