#ifndef __BCM_SYSPORT_H
#define __BCM_SYSPORT_H
#include <linux/bitmap.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/dim.h>
#include "unimac.h"
#define DESC_ADDR_HI_STATUS_LEN	0x00
#define  DESC_ADDR_HI_SHIFT	0
#define  DESC_ADDR_HI_MASK	0xff
#define  DESC_STATUS_SHIFT	8
#define  DESC_STATUS_MASK	0x3ff
#define  DESC_LEN_SHIFT		18
#define  DESC_LEN_MASK		0x7fff
#define DESC_ADDR_LO		0x04
#define DESC_SIZE		(WORDS_PER_DESC * sizeof(u32))
#define RX_BUF_LENGTH		2048
#define ENET_BRCM_TAG_LEN	4
#define ENET_PAD		10
#define UMAC_MAX_MTU_SIZE	(ETH_DATA_LEN + ETH_HLEN + VLAN_HLEN + \
				 ENET_BRCM_TAG_LEN + ETH_FCS_LEN + ENET_PAD)
struct bcm_tsb {
	u32 pcp_dei_vid;
#define PCP_DEI_MASK		0xf
#define VID_SHIFT		4
#define VID_MASK		0xfff
	u32 l4_ptr_dest_map;
#define L4_CSUM_PTR_MASK	0x1ff
#define L4_PTR_SHIFT		9
#define L4_PTR_MASK		0x1ff
#define L4_UDP			(1 << 18)
#define L4_LENGTH_VALID		(1 << 19)
#define DEST_MAP_SHIFT		20
#define DEST_MAP_MASK		0x1ff
};
struct bcm_rsb {
	u32 rx_status_len;
	u32 brcm_egress_tag;
};
#define DESC_L4_CSUM		(1 << 7)
#define DESC_SOP		(1 << 8)
#define DESC_EOP		(1 << 9)
#define RX_STATUS_UCAST			0
#define RX_STATUS_BCAST			0x04
#define RX_STATUS_MCAST			0x08
#define RX_STATUS_L2_MCAST		0x0c
#define RX_STATUS_ERR			(1 << 4)
#define RX_STATUS_OVFLOW		(1 << 5)
#define RX_STATUS_PARSE_FAIL		(1 << 6)
#define TX_STATUS_VLAN_NO_ACT		0x00
#define TX_STATUS_VLAN_PCP_TSB		0x01
#define TX_STATUS_VLAN_QUEUE		0x02
#define TX_STATUS_VLAN_VID_TSB		0x03
#define TX_STATUS_OWR_CRC		(1 << 2)
#define TX_STATUS_APP_CRC		(1 << 3)
#define TX_STATUS_BRCM_TAG_NO_ACT	0
#define TX_STATUS_BRCM_TAG_ZERO		0x10
#define TX_STATUS_BRCM_TAG_ONE_QUEUE	0x20
#define TX_STATUS_BRCM_TAG_ONE_TSB	0x30
#define TX_STATUS_SKIP_BYTES		(1 << 6)
#define SYS_PORT_TOPCTRL_OFFSET		0
#define REV_CNTL			0x00
#define  REV_MASK			0xffff
#define RX_FLUSH_CNTL			0x04
#define  RX_FLUSH			(1 << 0)
#define TX_FLUSH_CNTL			0x08
#define  TX_FLUSH			(1 << 0)
#define MISC_CNTL			0x0c
#define  SYS_CLK_SEL			(1 << 0)
#define  TDMA_EOP_SEL			(1 << 1)
#define SYS_PORT_INTRL2_0_OFFSET	0x200
#define SYS_PORT_INTRL2_1_OFFSET	0x240
#define INTRL2_CPU_STATUS		0x00
#define INTRL2_CPU_SET			0x04
#define INTRL2_CPU_CLEAR		0x08
#define INTRL2_CPU_MASK_STATUS		0x0c
#define INTRL2_CPU_MASK_SET		0x10
#define INTRL2_CPU_MASK_CLEAR		0x14
#define INTRL2_0_GISB_ERR		(1 << 0)
#define INTRL2_0_RBUF_OVFLOW		(1 << 1)
#define INTRL2_0_TBUF_UNDFLOW		(1 << 2)
#define INTRL2_0_MPD			(1 << 3)
#define INTRL2_0_BRCM_MATCH_TAG		(1 << 4)
#define INTRL2_0_RDMA_MBDONE		(1 << 5)
#define INTRL2_0_OVER_MAX_THRESH	(1 << 6)
#define INTRL2_0_BELOW_HYST_THRESH	(1 << 7)
#define INTRL2_0_FREE_LIST_EMPTY	(1 << 8)
#define INTRL2_0_TX_RING_FULL		(1 << 9)
#define INTRL2_0_DESC_ALLOC_ERR		(1 << 10)
#define INTRL2_0_UNEXP_PKTSIZE_ACK	(1 << 11)
#define INTRL2_0_TDMA_MBDONE_SHIFT	12
#define INTRL2_0_TDMA_MBDONE_MASK	(0xffff << INTRL2_0_TDMA_MBDONE_SHIFT)
#define SYS_PORT_RXCHK_OFFSET		0x300
#define RXCHK_CONTROL			0x00
#define  RXCHK_EN			(1 << 0)
#define  RXCHK_SKIP_FCS			(1 << 1)
#define  RXCHK_BAD_CSUM_DIS		(1 << 2)
#define  RXCHK_BRCM_TAG_EN		(1 << 3)
#define  RXCHK_BRCM_TAG_MATCH_SHIFT	4
#define  RXCHK_BRCM_TAG_MATCH_MASK	0xff
#define  RXCHK_PARSE_TNL		(1 << 12)
#define  RXCHK_VIOL_EN			(1 << 13)
#define  RXCHK_VIOL_DIS			(1 << 14)
#define  RXCHK_INCOM_PKT		(1 << 15)
#define  RXCHK_V6_DUPEXT_EN		(1 << 16)
#define  RXCHK_V6_DUPEXT_DIS		(1 << 17)
#define  RXCHK_ETHERTYPE_DIS		(1 << 18)
#define  RXCHK_L2_HDR_DIS		(1 << 19)
#define  RXCHK_L3_HDR_DIS		(1 << 20)
#define  RXCHK_MAC_RX_ERR_DIS		(1 << 21)
#define  RXCHK_PARSE_AUTH		(1 << 22)
#define RXCHK_BRCM_TAG0			0x04
#define RXCHK_BRCM_TAG(i)		((i) * 0x4 + RXCHK_BRCM_TAG0)
#define RXCHK_BRCM_TAG0_MASK		0x24
#define RXCHK_BRCM_TAG_MASK(i)		((i) * 0x4 + RXCHK_BRCM_TAG0_MASK)
#define RXCHK_BRCM_TAG_MATCH_STATUS	0x44
#define RXCHK_ETHERTYPE			0x48
#define RXCHK_BAD_CSUM_CNTR		0x4C
#define RXCHK_OTHER_DISC_CNTR		0x50
#define RXCHK_BRCM_TAG_MAX		8
#define RXCHK_BRCM_TAG_CID_SHIFT	16
#define RXCHK_BRCM_TAG_CID_MASK		0xff
#define SYS_PORT_TXCHK_OFFSET		0x380
#define TXCHK_PKT_RDY_THRESH		0x00
#define SYS_PORT_RBUF_OFFSET		0x400
#define RBUF_CONTROL			0x00
#define  RBUF_RSB_EN			(1 << 0)
#define  RBUF_4B_ALGN			(1 << 1)
#define  RBUF_BRCM_TAG_STRIP		(1 << 2)
#define  RBUF_BAD_PKT_DISC		(1 << 3)
#define  RBUF_RESUME_THRESH_SHIFT	4
#define  RBUF_RESUME_THRESH_MASK	0xff
#define  RBUF_OK_TO_SEND_SHIFT		12
#define  RBUF_OK_TO_SEND_MASK		0xff
#define  RBUF_CRC_REPLACE		(1 << 20)
#define  RBUF_OK_TO_SEND_MODE		(1 << 21)
#define  RBUF_RSB_SWAP0			(1 << 22)
#define  RBUF_RSB_SWAP1			(1 << 23)
#define  RBUF_ACPI_EN			(1 << 23)
#define  RBUF_ACPI_EN_LITE		(1 << 24)
#define RBUF_PKT_RDY_THRESH		0x04
#define RBUF_STATUS			0x08
#define  RBUF_WOL_MODE			(1 << 0)
#define  RBUF_MPD			(1 << 1)
#define  RBUF_ACPI			(1 << 2)
#define RBUF_OVFL_DISC_CNTR		0x0c
#define RBUF_ERR_PKT_CNTR		0x10
#define SYS_PORT_TBUF_OFFSET		0x600
#define TBUF_CONTROL			0x00
#define  TBUF_BP_EN			(1 << 0)
#define  TBUF_MAX_PKT_THRESH_SHIFT	1
#define  TBUF_MAX_PKT_THRESH_MASK	0x1f
#define  TBUF_FULL_THRESH_SHIFT		8
#define  TBUF_FULL_THRESH_MASK		0x1f
#define SYS_PORT_UMAC_OFFSET		0x800
#define UMAC_MIB_START			0x400
#define UMAC_MIB_STAT_OFFSET		0xc
#define UMAC_MIB_CTRL			0x580
#define  MIB_RX_CNT_RST			(1 << 0)
#define  MIB_RUNT_CNT_RST		(1 << 1)
#define  MIB_TX_CNT_RST			(1 << 2)
#define UMAC_MPD_CTRL			0x620
#define  MPD_EN				(1 << 0)
#define  MSEQ_LEN_SHIFT			16
#define  MSEQ_LEN_MASK			0xff
#define  PSW_EN				(1 << 27)
#define UMAC_PSW_MS			0x624
#define UMAC_PSW_LS			0x628
#define UMAC_MDF_CTRL			0x650
#define UMAC_MDF_ADDR			0x654
#define SYS_PORT_GIB_OFFSET		0x1000
#define GIB_CONTROL			0x00
#define  GIB_TX_EN			(1 << 0)
#define  GIB_RX_EN			(1 << 1)
#define  GIB_TX_FLUSH			(1 << 2)
#define  GIB_RX_FLUSH			(1 << 3)
#define  GIB_GTX_CLK_SEL_SHIFT		4
#define  GIB_GTX_CLK_EXT_CLK		(0 << GIB_GTX_CLK_SEL_SHIFT)
#define  GIB_GTX_CLK_125MHZ		(1 << GIB_GTX_CLK_SEL_SHIFT)
#define  GIB_GTX_CLK_250MHZ		(2 << GIB_GTX_CLK_SEL_SHIFT)
#define  GIB_FCS_STRIP_SHIFT		6
#define  GIB_FCS_STRIP			(1 << GIB_FCS_STRIP_SHIFT)
#define  GIB_LCL_LOOP_EN		(1 << 7)
#define  GIB_LCL_LOOP_TXEN		(1 << 8)
#define  GIB_RMT_LOOP_EN		(1 << 9)
#define  GIB_RMT_LOOP_RXEN		(1 << 10)
#define  GIB_RX_PAUSE_EN		(1 << 11)
#define  GIB_PREAMBLE_LEN_SHIFT		12
#define  GIB_PREAMBLE_LEN_MASK		0xf
#define  GIB_IPG_LEN_SHIFT		16
#define  GIB_IPG_LEN_MASK		0x3f
#define  GIB_PAD_EXTENSION_SHIFT	22
#define  GIB_PAD_EXTENSION_MASK		0x3f
#define GIB_MAC1			0x08
#define GIB_MAC0			0x0c
#define SYS_PORT_RDMA_OFFSET		0x2000
#define RDMA_CONTROL			0x1000
#define  RDMA_EN			(1 << 0)
#define  RDMA_RING_CFG			(1 << 1)
#define  RDMA_DISC_EN			(1 << 2)
#define  RDMA_BUF_DATA_OFFSET_SHIFT	4
#define  RDMA_BUF_DATA_OFFSET_MASK	0x3ff
#define RDMA_STATUS			0x1004
#define  RDMA_DISABLED			(1 << 0)
#define  RDMA_DESC_RAM_INIT_BUSY	(1 << 1)
#define  RDMA_BP_STATUS			(1 << 2)
#define RDMA_SCB_BURST_SIZE		0x1008
#define RDMA_RING_BUF_SIZE		0x100c
#define  RDMA_RING_SIZE_SHIFT		16
#define RDMA_WRITE_PTR_HI		0x1010
#define RDMA_WRITE_PTR_LO		0x1014
#define RDMA_OVFL_DISC_CNTR		0x1018
#define RDMA_PROD_INDEX			0x1018
#define  RDMA_PROD_INDEX_MASK		0xffff
#define RDMA_CONS_INDEX			0x101c
#define  RDMA_CONS_INDEX_MASK		0xffff
#define RDMA_START_ADDR_HI		0x1020
#define RDMA_START_ADDR_LO		0x1024
#define RDMA_END_ADDR_HI		0x1028
#define RDMA_END_ADDR_LO		0x102c
#define RDMA_MBDONE_INTR		0x1030
#define  RDMA_INTR_THRESH_MASK		0x1ff
#define  RDMA_TIMEOUT_SHIFT		16
#define  RDMA_TIMEOUT_MASK		0xffff
#define RDMA_XON_XOFF_THRESH		0x1034
#define  RDMA_XON_XOFF_THRESH_MASK	0xffff
#define  RDMA_XOFF_THRESH_SHIFT		16
#define RDMA_READ_PTR_HI		0x1038
#define RDMA_READ_PTR_LO		0x103c
#define RDMA_OVERRIDE			0x1040
#define  RDMA_LE_MODE			(1 << 0)
#define  RDMA_REG_MODE			(1 << 1)
#define RDMA_TEST			0x1044
#define  RDMA_TP_OUT_SEL		(1 << 0)
#define  RDMA_MEM_SEL			(1 << 1)
#define RDMA_DEBUG			0x1048
#define TDMA_NUM_RINGS			32	 
#define TDMA_PORT_SIZE			DESC_SIZE  
#define SYS_PORT_TDMA_OFFSET		0x4000
#define TDMA_WRITE_PORT_OFFSET		0x0000
#define TDMA_WRITE_PORT_HI(i)		(TDMA_WRITE_PORT_OFFSET + \
					(i) * TDMA_PORT_SIZE)
#define TDMA_WRITE_PORT_LO(i)		(TDMA_WRITE_PORT_OFFSET + \
					sizeof(u32) + (i) * TDMA_PORT_SIZE)
#define TDMA_READ_PORT_OFFSET		(TDMA_WRITE_PORT_OFFSET + \
					(TDMA_NUM_RINGS * TDMA_PORT_SIZE))
#define TDMA_READ_PORT_HI(i)		(TDMA_READ_PORT_OFFSET + \
					(i) * TDMA_PORT_SIZE)
#define TDMA_READ_PORT_LO(i)		(TDMA_READ_PORT_OFFSET + \
					sizeof(u32) + (i) * TDMA_PORT_SIZE)
#define TDMA_READ_PORT_CMD_OFFSET	(TDMA_READ_PORT_OFFSET + \
					(TDMA_NUM_RINGS * TDMA_PORT_SIZE))
#define TDMA_READ_PORT_CMD(i)		(TDMA_READ_PORT_CMD_OFFSET + \
					(i) * sizeof(u32))
#define TDMA_DESC_RING_00_BASE		(TDMA_READ_PORT_CMD_OFFSET + \
					(TDMA_NUM_RINGS * sizeof(u32)))
#define RING_HEAD_TAIL_PTR		0x00
#define  RING_HEAD_MASK			0x7ff
#define  RING_TAIL_SHIFT		11
#define  RING_TAIL_MASK			0x7ff
#define  RING_FLUSH			(1 << 24)
#define  RING_EN			(1 << 25)
#define RING_COUNT			0x04
#define  RING_COUNT_MASK		0x7ff
#define  RING_BUFF_DONE_SHIFT		11
#define  RING_BUFF_DONE_MASK		0x7ff
#define RING_MAX_HYST			0x08
#define  RING_MAX_THRESH_MASK		0x7ff
#define  RING_HYST_THRESH_SHIFT		11
#define  RING_HYST_THRESH_MASK		0x7ff
#define RING_INTR_CONTROL		0x0c
#define  RING_INTR_THRESH_MASK		0x7ff
#define  RING_EMPTY_INTR_EN		(1 << 15)
#define  RING_TIMEOUT_SHIFT		16
#define  RING_TIMEOUT_MASK		0xffff
#define RING_PROD_CONS_INDEX		0x10
#define  RING_PROD_INDEX_MASK		0xffff
#define  RING_CONS_INDEX_SHIFT		16
#define  RING_CONS_INDEX_MASK		0xffff
#define RING_MAPPING			0x14
#define  RING_QID_MASK			0x7
#define  RING_PORT_ID_SHIFT		3
#define  RING_PORT_ID_MASK		0x7
#define  RING_IGNORE_STATUS		(1 << 6)
#define  RING_FAILOVER_EN		(1 << 7)
#define  RING_CREDIT_SHIFT		8
#define  RING_CREDIT_MASK		0xffff
#define RING_PCP_DEI_VID		0x18
#define  RING_VID_MASK			0x7ff
#define  RING_DEI			(1 << 12)
#define  RING_PCP_SHIFT			13
#define  RING_PCP_MASK			0x7
#define  RING_PKT_SIZE_ADJ_SHIFT	16
#define  RING_PKT_SIZE_ADJ_MASK		0xf
#define TDMA_DESC_RING_SIZE		28
#define TDMA_DESC_RING_BASE(i)		(TDMA_DESC_RING_00_BASE + \
					((i) * TDMA_DESC_RING_SIZE))
#define TDMA_DESC_RING_HEAD_TAIL_PTR(i)	(TDMA_DESC_RING_BASE(i) + \
					RING_HEAD_TAIL_PTR)
#define TDMA_DESC_RING_COUNT(i)		(TDMA_DESC_RING_BASE(i) + \
					RING_COUNT)
#define TDMA_DESC_RING_MAX_HYST(i)	(TDMA_DESC_RING_BASE(i) + \
					RING_MAX_HYST)
#define TDMA_DESC_RING_INTR_CONTROL(i)	(TDMA_DESC_RING_BASE(i) + \
					RING_INTR_CONTROL)
#define TDMA_DESC_RING_PROD_CONS_INDEX(i) \
					(TDMA_DESC_RING_BASE(i) + \
					RING_PROD_CONS_INDEX)
#define TDMA_DESC_RING_MAPPING(i)	(TDMA_DESC_RING_BASE(i) + \
					RING_MAPPING)
#define TDMA_DESC_RING_PCP_DEI_VID(i)	(TDMA_DESC_RING_BASE(i) + \
					RING_PCP_DEI_VID)
#define TDMA_CONTROL			0x600
#define  TDMA_EN			0
#define  TSB_EN				1
#define  TSB_SWAP0			2
#define  TSB_SWAP1			3
#define  ACB_ALGO			3
#define  BUF_DATA_OFFSET_SHIFT		4
#define  BUF_DATA_OFFSET_MASK		0x3ff
#define  VLAN_EN			14
#define  SW_BRCM_TAG			15
#define  WNC_KPT_SIZE_UPDATE		16
#define  SYNC_PKT_SIZE			17
#define  ACH_TXDONE_DELAY_SHIFT		18
#define  ACH_TXDONE_DELAY_MASK		0xff
#define TDMA_STATUS			0x604
#define  TDMA_DISABLED			(1 << 0)
#define  TDMA_LL_RAM_INIT_BUSY		(1 << 1)
#define TDMA_SCB_BURST_SIZE		0x608
#define TDMA_OVER_MAX_THRESH_STATUS	0x60c
#define TDMA_OVER_HYST_THRESH_STATUS	0x610
#define TDMA_TPID			0x614
#define TDMA_FREE_LIST_HEAD_TAIL_PTR	0x618
#define  TDMA_FREE_HEAD_MASK		0x7ff
#define  TDMA_FREE_TAIL_SHIFT		11
#define  TDMA_FREE_TAIL_MASK		0x7ff
#define TDMA_FREE_LIST_COUNT		0x61c
#define  TDMA_FREE_LIST_COUNT_MASK	0x7ff
#define TDMA_TIER2_ARB_CTRL		0x620
#define  TDMA_ARB_MODE_RR		0
#define  TDMA_ARB_MODE_WEIGHT_RR	0x1
#define  TDMA_ARB_MODE_STRICT		0x2
#define  TDMA_ARB_MODE_DEFICIT_RR	0x3
#define  TDMA_CREDIT_SHIFT		4
#define  TDMA_CREDIT_MASK		0xffff
#define TDMA_TIER1_ARB_0_CTRL		0x624
#define  TDMA_ARB_EN			(1 << 0)
#define TDMA_TIER1_ARB_0_QUEUE_EN	0x628
#define TDMA_TIER1_ARB_1_CTRL		0x62c
#define TDMA_TIER1_ARB_1_QUEUE_EN	0x630
#define TDMA_TIER1_ARB_2_CTRL		0x634
#define TDMA_TIER1_ARB_2_QUEUE_EN	0x638
#define TDMA_TIER1_ARB_3_CTRL		0x63c
#define TDMA_TIER1_ARB_3_QUEUE_EN	0x640
#define TDMA_SCB_ENDIAN_OVERRIDE	0x644
#define  TDMA_LE_MODE			(1 << 0)
#define  TDMA_REG_MODE			(1 << 1)
#define TDMA_TEST			0x648
#define  TDMA_TP_OUT_SEL		(1 << 0)
#define  TDMA_MEM_TM			(1 << 1)
#define TDMA_DEBUG			0x64c
#define SP_NUM_HW_RX_DESC_WORDS		1024
#define SP_LT_NUM_HW_RX_DESC_WORDS	512
#define SP_NUM_TX_DESC			1536
#define SP_LT_NUM_TX_DESC		256
#define WORDS_PER_DESC			2
struct bcm_sysport_pkt_counters {
	u32	cnt_64;		 
	u32	cnt_127;	 
	u32	cnt_255;	 
	u32	cnt_511;	 
	u32	cnt_1023;	 
	u32	cnt_1518;	 
	u32	cnt_mgv;	 
	u32	cnt_2047;	 
	u32	cnt_4095;	 
	u32	cnt_9216;	 
};
struct bcm_sysport_rx_counters {
	struct  bcm_sysport_pkt_counters pkt_cnt;
	u32	pkt;		 
	u32	bytes;		 
	u32	mca;		 
	u32	bca;		 
	u32	fcs;		 
	u32	cf;		 
	u32	pf;		 
	u32	uo;		 
	u32	aln;		 
	u32	flr;		 
	u32	cde;		 
	u32	fcr;		 
	u32	ovr;		 
	u32	jbr;		 
	u32	mtue;		 
	u32	pok;		 
	u32	uc;		 
	u32	ppp;		 
	u32	rcrc;		 
};
struct bcm_sysport_tx_counters {
	struct bcm_sysport_pkt_counters pkt_cnt;
	u32	pkts;		 
	u32	mca;		 
	u32	bca;		 
	u32	pf;		 
	u32	cf;		 
	u32	fcs;		 
	u32	ovr;		 
	u32	drf;		 
	u32	edf;		 
	u32	scl;		 
	u32	mcl;		 
	u32	lcl;		 
	u32	ecl;		 
	u32	frg;		 
	u32	ncl;		 
	u32	jbr;		 
	u32	bytes;		 
	u32	pok;		 
	u32	uc;		 
};
struct bcm_sysport_mib {
	struct bcm_sysport_rx_counters rx;
	struct bcm_sysport_tx_counters tx;
	u32 rx_runt_cnt;
	u32 rx_runt_fcs;
	u32 rx_runt_fcs_align;
	u32 rx_runt_bytes;
	u32 rxchk_bad_csum;
	u32 rxchk_other_pkt_disc;
	u32 rbuf_ovflow_cnt;
	u32 rbuf_err_cnt;
	u32 rdma_ovflow_cnt;
	u32 alloc_rx_buff_failed;
	u32 rx_dma_failed;
	u32 tx_dma_failed;
	u32 tx_realloc_tsb;
	u32 tx_realloc_tsb_failed;
};
enum bcm_sysport_stat_type {
	BCM_SYSPORT_STAT_NETDEV = -1,
	BCM_SYSPORT_STAT_NETDEV64,
	BCM_SYSPORT_STAT_MIB_RX,
	BCM_SYSPORT_STAT_MIB_TX,
	BCM_SYSPORT_STAT_RUNT,
	BCM_SYSPORT_STAT_RXCHK,
	BCM_SYSPORT_STAT_RBUF,
	BCM_SYSPORT_STAT_RDMA,
	BCM_SYSPORT_STAT_SOFT,
};
#define STAT_NETDEV(m) { \
	.stat_string = __stringify(m), \
	.stat_sizeof = sizeof(((struct net_device_stats *)0)->m), \
	.stat_offset = offsetof(struct net_device_stats, m), \
	.type = BCM_SYSPORT_STAT_NETDEV, \
}
#define STAT_NETDEV64(m) { \
	.stat_string = __stringify(m), \
	.stat_sizeof = sizeof(((struct bcm_sysport_stats64 *)0)->m), \
	.stat_offset = offsetof(struct bcm_sysport_stats64, m), \
	.type = BCM_SYSPORT_STAT_NETDEV64, \
}
#define STAT_MIB(str, m, _type) { \
	.stat_string = str, \
	.stat_sizeof = sizeof(((struct bcm_sysport_priv *)0)->m), \
	.stat_offset = offsetof(struct bcm_sysport_priv, m), \
	.type = _type, \
}
#define STAT_MIB_RX(str, m) STAT_MIB(str, m, BCM_SYSPORT_STAT_MIB_RX)
#define STAT_MIB_TX(str, m) STAT_MIB(str, m, BCM_SYSPORT_STAT_MIB_TX)
#define STAT_RUNT(str, m) STAT_MIB(str, m, BCM_SYSPORT_STAT_RUNT)
#define STAT_MIB_SOFT(str, m) STAT_MIB(str, m, BCM_SYSPORT_STAT_SOFT)
#define STAT_RXCHK(str, m, ofs) { \
	.stat_string = str, \
	.stat_sizeof = sizeof(((struct bcm_sysport_priv *)0)->m), \
	.stat_offset = offsetof(struct bcm_sysport_priv, m), \
	.type = BCM_SYSPORT_STAT_RXCHK, \
	.reg_offset = ofs, \
}
#define STAT_RBUF(str, m, ofs) { \
	.stat_string = str, \
	.stat_sizeof = sizeof(((struct bcm_sysport_priv *)0)->m), \
	.stat_offset = offsetof(struct bcm_sysport_priv, m), \
	.type = BCM_SYSPORT_STAT_RBUF, \
	.reg_offset = ofs, \
}
#define STAT_RDMA(str, m, ofs) { \
	.stat_string = str, \
	.stat_sizeof = sizeof(((struct bcm_sysport_priv *)0)->m), \
	.stat_offset = offsetof(struct bcm_sysport_priv, m), \
	.type = BCM_SYSPORT_STAT_RDMA, \
	.reg_offset = ofs, \
}
#define NUM_SYSPORT_TXQ_STAT	2
struct bcm_sysport_stats {
	char stat_string[ETH_GSTRING_LEN];
	int stat_sizeof;
	int stat_offset;
	enum bcm_sysport_stat_type type;
	u16 reg_offset;
};
struct bcm_sysport_stats64 {
	u64	rx_packets;
	u64	rx_bytes;
	u64	tx_packets;
	u64	tx_bytes;
};
struct bcm_sysport_cb {
	struct sk_buff	*skb;		 
	void __iomem	*bd_addr;	 
	DEFINE_DMA_UNMAP_ADDR(dma_addr);
	DEFINE_DMA_UNMAP_LEN(dma_len);
};
enum bcm_sysport_type {
	SYSTEMPORT = 0,
	SYSTEMPORT_LITE,
};
struct bcm_sysport_hw_params {
	bool		is_lite;
	unsigned int	num_rx_desc_words;
};
struct bcm_sysport_net_dim {
	u16			use_dim;
	u16			event_ctr;
	unsigned long		packets;
	unsigned long		bytes;
	struct dim		dim;
};
struct bcm_sysport_tx_ring {
	spinlock_t	lock;		 
	struct napi_struct napi;	 
	unsigned int	index;		 
	unsigned int	size;		 
	unsigned int	alloc_size;	 
	unsigned int	desc_count;	 
	unsigned int	curr_desc;	 
	unsigned int	c_index;	 
	unsigned int	clean_index;	 
	struct bcm_sysport_cb *cbs;	 
	struct bcm_sysport_priv *priv;	 
	unsigned long	packets;	 
	unsigned long	bytes;		 
	unsigned int	switch_queue;	 
	unsigned int	switch_port;	 
	bool		inspect;	 
};
struct bcm_sysport_priv {
	void __iomem		*base;
	u32			irq0_stat;
	u32			irq0_mask;
	u32			irq1_stat;
	u32			irq1_mask;
	bool			is_lite;
	unsigned int		num_rx_desc_words;
	struct napi_struct	napi ____cacheline_aligned;
	struct net_device	*netdev;
	struct platform_device	*pdev;
	int			irq0;
	int			irq1;
	int			wol_irq;
	spinlock_t		desc_lock;
	struct bcm_sysport_tx_ring *tx_rings;
	void __iomem		*rx_bds;
	struct bcm_sysport_cb	*rx_cbs;
	unsigned int		num_rx_bds;
	unsigned int		rx_read_ptr;
	unsigned int		rx_c_index;
	struct bcm_sysport_net_dim	dim;
	u32			rx_max_coalesced_frames;
	u32			rx_coalesce_usecs;
	struct device_node	*phy_dn;
	phy_interface_t		phy_interface;
	int			old_pause;
	int			old_link;
	int			old_duplex;
	unsigned int		rx_chk_en:1;
	unsigned int		tsb_en:1;
	unsigned int		crc_fwd:1;
	u16			rev;
	u32			wolopts;
	u8			sopass[SOPASS_MAX];
	unsigned int		wol_irq_disabled:1;
	struct clk		*clk;
	struct clk		*wol_clk;
	struct bcm_sysport_mib	mib;
	u32			msg_enable;
	DECLARE_BITMAP(filters, RXCHK_BRCM_TAG_MAX);
	u32			filters_loc[RXCHK_BRCM_TAG_MAX];
	struct bcm_sysport_stats64	stats64;
	struct u64_stats_sync	syncp;
	struct notifier_block	netdev_notifier;
	unsigned int		per_port_num_tx_queues;
	struct bcm_sysport_tx_ring *ring_map[DSA_MAX_PORTS * 8];
};
#endif  
