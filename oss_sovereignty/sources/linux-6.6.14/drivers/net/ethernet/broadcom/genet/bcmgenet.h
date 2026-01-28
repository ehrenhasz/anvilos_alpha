


#ifndef __BCMGENET_H__
#define __BCMGENET_H__

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <linux/phy.h>
#include <linux/dim.h>
#include <linux/ethtool.h>

#include "../unimac.h"


#define TOTAL_DESC				256


#define DESC_INDEX				16


#define ENET_BRCM_TAG_LEN	6
#define ENET_PAD		8
#define ENET_MAX_MTU_SIZE	(ETH_DATA_LEN + ETH_HLEN + VLAN_HLEN + \
				 ENET_BRCM_TAG_LEN + ETH_FCS_LEN + ENET_PAD)
#define DMA_MAX_BURST_LENGTH    0x10


#define MAX_NUM_OF_FS_RULES		16
#define CLEAR_ALL_HFB			0xFF
#define DMA_FC_THRESH_HI		(TOTAL_DESC >> 4)
#define DMA_FC_THRESH_LO		5


struct status_64 {
	u32	length_status;		
	u32	ext_status;		
	u32	rx_csum;		
	u32	unused1[9];		
	u32	tx_csum_info;		
	u32	unused2[3];		
};


#define STATUS_RX_EXT_MASK		0x1FFFFF
#define STATUS_RX_CSUM_MASK		0xFFFF
#define STATUS_RX_CSUM_OK		0x10000
#define STATUS_RX_CSUM_FR		0x20000
#define STATUS_RX_PROTO_TCP		0
#define STATUS_RX_PROTO_UDP		1
#define STATUS_RX_PROTO_ICMP		2
#define STATUS_RX_PROTO_OTHER		3
#define STATUS_RX_PROTO_MASK		3
#define STATUS_RX_PROTO_SHIFT		18
#define STATUS_FILTER_INDEX_MASK	0xFFFF

#define STATUS_TX_CSUM_START_MASK	0X7FFF
#define STATUS_TX_CSUM_START_SHIFT	16
#define STATUS_TX_CSUM_PROTO_UDP	0x8000
#define STATUS_TX_CSUM_OFFSET_MASK	0x7FFF
#define STATUS_TX_CSUM_LV		0x80000000


#define DMA_DESC_LENGTH_STATUS	0x00	
#define DMA_DESC_ADDRESS_LO	0x04	
#define DMA_DESC_ADDRESS_HI	0x08	


struct bcmgenet_pkt_counters {
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


struct bcmgenet_rx_counters {
	struct  bcmgenet_pkt_counters pkt_cnt;
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


struct bcmgenet_tx_counters {
	struct bcmgenet_pkt_counters pkt_cnt;
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

struct bcmgenet_mib_counters {
	struct bcmgenet_rx_counters rx;
	struct bcmgenet_tx_counters tx;
	u32	rx_runt_cnt;
	u32	rx_runt_fcs;
	u32	rx_runt_fcs_align;
	u32	rx_runt_bytes;
	u32	rbuf_ovflow_cnt;
	u32	rbuf_err_cnt;
	u32	mdf_err_cnt;
	u32	alloc_rx_buff_failed;
	u32	rx_dma_failed;
	u32	tx_dma_failed;
	u32	tx_realloc_tsb;
	u32	tx_realloc_tsb_failed;
};

#define UMAC_MIB_START			0x400

#define UMAC_MDIO_CMD			0x614
#define  MDIO_START_BUSY		(1 << 29)
#define  MDIO_READ_FAIL			(1 << 28)
#define  MDIO_RD			(2 << 26)
#define  MDIO_WR			(1 << 26)
#define  MDIO_PMD_SHIFT			21
#define  MDIO_PMD_MASK			0x1F
#define  MDIO_REG_SHIFT			16
#define  MDIO_REG_MASK			0x1F

#define UMAC_RBUF_OVFL_CNT_V1		0x61C
#define RBUF_OVFL_CNT_V2		0x80
#define RBUF_OVFL_CNT_V3PLUS		0x94

#define UMAC_MPD_CTRL			0x620
#define  MPD_EN				(1 << 0)
#define  MPD_PW_EN			(1 << 27)
#define  MPD_MSEQ_LEN_SHIFT		16
#define  MPD_MSEQ_LEN_MASK		0xFF

#define UMAC_MPD_PW_MS			0x624
#define UMAC_MPD_PW_LS			0x628
#define UMAC_RBUF_ERR_CNT_V1		0x634
#define RBUF_ERR_CNT_V2			0x84
#define RBUF_ERR_CNT_V3PLUS		0x98
#define UMAC_MDF_ERR_CNT		0x638
#define UMAC_MDF_CTRL			0x650
#define UMAC_MDF_ADDR			0x654
#define UMAC_MIB_CTRL			0x580
#define  MIB_RESET_RX			(1 << 0)
#define  MIB_RESET_RUNT			(1 << 1)
#define  MIB_RESET_TX			(1 << 2)

#define RBUF_CTRL			0x00
#define  RBUF_64B_EN			(1 << 0)
#define  RBUF_ALIGN_2B			(1 << 1)
#define  RBUF_BAD_DIS			(1 << 2)

#define RBUF_STATUS			0x0C
#define  RBUF_STATUS_WOL		(1 << 0)
#define  RBUF_STATUS_MPD_INTR_ACTIVE	(1 << 1)
#define  RBUF_STATUS_ACPI_INTR_ACTIVE	(1 << 2)

#define RBUF_CHK_CTRL			0x14
#define  RBUF_RXCHK_EN			(1 << 0)
#define  RBUF_SKIP_FCS			(1 << 4)
#define  RBUF_L3_PARSE_DIS		(1 << 5)

#define RBUF_ENERGY_CTRL		0x9c
#define  RBUF_EEE_EN			(1 << 0)
#define  RBUF_PM_EN			(1 << 1)

#define RBUF_TBUF_SIZE_CTRL		0xb4

#define RBUF_HFB_CTRL_V1		0x38
#define  RBUF_HFB_FILTER_EN_SHIFT	16
#define  RBUF_HFB_FILTER_EN_MASK	0xffff0000
#define  RBUF_HFB_EN			(1 << 0)
#define  RBUF_HFB_256B			(1 << 1)
#define  RBUF_ACPI_EN			(1 << 2)

#define RBUF_HFB_LEN_V1			0x3C
#define  RBUF_FLTR_LEN_MASK		0xFF
#define  RBUF_FLTR_LEN_SHIFT		8

#define TBUF_CTRL			0x00
#define  TBUF_64B_EN			(1 << 0)
#define TBUF_BP_MC			0x0C
#define TBUF_ENERGY_CTRL		0x14
#define  TBUF_EEE_EN			(1 << 0)
#define  TBUF_PM_EN			(1 << 1)

#define TBUF_CTRL_V1			0x80
#define TBUF_BP_MC_V1			0xA0

#define HFB_CTRL			0x00
#define HFB_FLT_ENABLE_V3PLUS		0x04
#define HFB_FLT_LEN_V2			0x04
#define HFB_FLT_LEN_V3PLUS		0x1C


#define INTRL2_CPU_STAT			0x00
#define INTRL2_CPU_SET			0x04
#define INTRL2_CPU_CLEAR		0x08
#define INTRL2_CPU_MASK_STATUS		0x0C
#define INTRL2_CPU_MASK_SET		0x10
#define INTRL2_CPU_MASK_CLEAR		0x14


#define UMAC_IRQ_SCB			(1 << 0)
#define UMAC_IRQ_EPHY			(1 << 1)
#define UMAC_IRQ_PHY_DET_R		(1 << 2)
#define UMAC_IRQ_PHY_DET_F		(1 << 3)
#define UMAC_IRQ_LINK_UP		(1 << 4)
#define UMAC_IRQ_LINK_DOWN		(1 << 5)
#define UMAC_IRQ_LINK_EVENT		(UMAC_IRQ_LINK_UP | UMAC_IRQ_LINK_DOWN)
#define UMAC_IRQ_UMAC			(1 << 6)
#define UMAC_IRQ_UMAC_TSV		(1 << 7)
#define UMAC_IRQ_TBUF_UNDERRUN		(1 << 8)
#define UMAC_IRQ_RBUF_OVERFLOW		(1 << 9)
#define UMAC_IRQ_HFB_SM			(1 << 10)
#define UMAC_IRQ_HFB_MM			(1 << 11)
#define UMAC_IRQ_MPD_R			(1 << 12)
#define UMAC_IRQ_WAKE_EVENT		(UMAC_IRQ_HFB_SM | UMAC_IRQ_HFB_MM | \
					 UMAC_IRQ_MPD_R)
#define UMAC_IRQ_RXDMA_MBDONE		(1 << 13)
#define UMAC_IRQ_RXDMA_PDONE		(1 << 14)
#define UMAC_IRQ_RXDMA_BDONE		(1 << 15)
#define UMAC_IRQ_RXDMA_DONE		UMAC_IRQ_RXDMA_MBDONE
#define UMAC_IRQ_TXDMA_MBDONE		(1 << 16)
#define UMAC_IRQ_TXDMA_PDONE		(1 << 17)
#define UMAC_IRQ_TXDMA_BDONE		(1 << 18)
#define UMAC_IRQ_TXDMA_DONE		UMAC_IRQ_TXDMA_MBDONE


#define UMAC_IRQ_MDIO_DONE		(1 << 23)
#define UMAC_IRQ_MDIO_ERROR		(1 << 24)


#define UMAC_IRQ1_TX_INTR_MASK		0xFFFF
#define UMAC_IRQ1_RX_INTR_MASK		0xFFFF
#define UMAC_IRQ1_RX_INTR_SHIFT		16


#define GENET_SYS_OFF			0x0000
#define GENET_GR_BRIDGE_OFF		0x0040
#define GENET_EXT_OFF			0x0080
#define GENET_INTRL2_0_OFF		0x0200
#define GENET_INTRL2_1_OFF		0x0240
#define GENET_RBUF_OFF			0x0300
#define GENET_UMAC_OFF			0x0800


#define SYS_REV_CTRL			0x00
#define SYS_PORT_CTRL			0x04
#define  PORT_MODE_INT_EPHY		0
#define  PORT_MODE_INT_GPHY		1
#define  PORT_MODE_EXT_EPHY		2
#define  PORT_MODE_EXT_GPHY		3
#define  PORT_MODE_EXT_RVMII_25		(4 | BIT(4))
#define  PORT_MODE_EXT_RVMII_50		4
#define  LED_ACT_SOURCE_MAC		(1 << 9)

#define SYS_RBUF_FLUSH_CTRL		0x08
#define SYS_TBUF_FLUSH_CTRL		0x0C
#define RBUF_FLUSH_CTRL_V1		0x04


#define EXT_EXT_PWR_MGMT		0x00
#define  EXT_PWR_DOWN_BIAS		(1 << 0)
#define  EXT_PWR_DOWN_DLL		(1 << 1)
#define  EXT_PWR_DOWN_PHY		(1 << 2)
#define  EXT_PWR_DN_EN_LD		(1 << 3)
#define  EXT_ENERGY_DET			(1 << 4)
#define  EXT_IDDQ_FROM_PHY		(1 << 5)
#define  EXT_IDDQ_GLBL_PWR		(1 << 7)
#define  EXT_PHY_RESET			(1 << 8)
#define  EXT_ENERGY_DET_MASK		(1 << 12)
#define  EXT_PWR_DOWN_PHY_TX		(1 << 16)
#define  EXT_PWR_DOWN_PHY_RX		(1 << 17)
#define  EXT_PWR_DOWN_PHY_SD		(1 << 18)
#define  EXT_PWR_DOWN_PHY_RD		(1 << 19)
#define  EXT_PWR_DOWN_PHY_EN		(1 << 20)

#define EXT_RGMII_OOB_CTRL		0x0C
#define  RGMII_MODE_EN_V123		(1 << 0)
#define  RGMII_LINK			(1 << 4)
#define  OOB_DISABLE			(1 << 5)
#define  RGMII_MODE_EN			(1 << 6)
#define  ID_MODE_DIS			(1 << 16)

#define EXT_GPHY_CTRL			0x1C
#define  EXT_CFG_IDDQ_BIAS		(1 << 0)
#define  EXT_CFG_PWR_DOWN		(1 << 1)
#define  EXT_CK25_DIS			(1 << 4)
#define  EXT_CFG_IDDQ_GLOBAL_PWR	(1 << 3)
#define  EXT_GPHY_RESET			(1 << 5)


#define DMA_RING_SIZE			(0x40)
#define DMA_RINGS_SIZE			(DMA_RING_SIZE * (DESC_INDEX + 1))


#define DMA_RW_POINTER_MASK		0x1FF
#define DMA_P_INDEX_DISCARD_CNT_MASK	0xFFFF
#define DMA_P_INDEX_DISCARD_CNT_SHIFT	16
#define DMA_BUFFER_DONE_CNT_MASK	0xFFFF
#define DMA_BUFFER_DONE_CNT_SHIFT	16
#define DMA_P_INDEX_MASK		0xFFFF
#define DMA_C_INDEX_MASK		0xFFFF


#define DMA_RING_SIZE_MASK		0xFFFF
#define DMA_RING_SIZE_SHIFT		16
#define DMA_RING_BUFFER_SIZE_MASK	0xFFFF


#define DMA_INTR_THRESHOLD_MASK		0x01FF


#define DMA_XON_THREHOLD_MASK		0xFFFF
#define DMA_XOFF_THRESHOLD_MASK		0xFFFF
#define DMA_XOFF_THRESHOLD_SHIFT	16


#define DMA_FLOW_PERIOD_MASK		0xFFFF
#define DMA_MAX_PKT_SIZE_MASK		0xFFFF
#define DMA_MAX_PKT_SIZE_SHIFT		16



#define DMA_EN				(1 << 0)
#define DMA_RING_BUF_EN_SHIFT		0x01
#define DMA_RING_BUF_EN_MASK		0xFFFF
#define DMA_TSB_SWAP_EN			(1 << 20)


#define DMA_DISABLED			(1 << 0)
#define DMA_DESC_RAM_INIT_BUSY		(1 << 1)


#define DMA_SCB_BURST_SIZE_MASK		0x1F


#define DMA_ACTIVITY_VECTOR_MASK	0x1FFFF


#define DMA_BACKPRESSURE_MASK		0x1FFFF
#define DMA_PFC_ENABLE			(1 << 31)


#define DMA_BACKPRESSURE_STATUS_MASK	0x1FFFF


#define DMA_LITTLE_ENDIAN_MODE		(1 << 0)
#define DMA_REGISTER_MODE		(1 << 1)


#define DMA_TIMEOUT_MASK		0xFFFF
#define DMA_TIMEOUT_VAL			5000	


#define DMA_RATE_LIMIT_EN_MASK		0xFFFF


#define DMA_ARBITER_MODE_MASK		0x03
#define DMA_RING_BUF_PRIORITY_MASK	0x1F
#define DMA_RING_BUF_PRIORITY_SHIFT	5
#define DMA_PRIO_REG_INDEX(q)		((q) / 6)
#define DMA_PRIO_REG_SHIFT(q)		(((q) % 6) * DMA_RING_BUF_PRIORITY_SHIFT)
#define DMA_RATE_ADJ_MASK		0xFF


#define DMA_BUFLENGTH_MASK		0x0fff
#define DMA_BUFLENGTH_SHIFT		16
#define DMA_OWN				0x8000
#define DMA_EOP				0x4000
#define DMA_SOP				0x2000
#define DMA_WRAP			0x1000

#define DMA_TX_UNDERRUN			0x0200
#define DMA_TX_APPEND_CRC		0x0040
#define DMA_TX_OW_CRC			0x0020
#define DMA_TX_DO_CSUM			0x0010
#define DMA_TX_QTAG_SHIFT		7


#define DMA_RX_CHK_V3PLUS		0x8000
#define DMA_RX_CHK_V12			0x1000
#define DMA_RX_BRDCAST			0x0040
#define DMA_RX_MULT			0x0020
#define DMA_RX_LG			0x0010
#define DMA_RX_NO			0x0008
#define DMA_RX_RXER			0x0004
#define DMA_RX_CRC_ERROR		0x0002
#define DMA_RX_OV			0x0001
#define DMA_RX_FI_MASK			0x001F
#define DMA_RX_FI_SHIFT			0x0007
#define DMA_DESC_ALLOC_MASK		0x00FF

#define DMA_ARBITER_RR			0x00
#define DMA_ARBITER_WRR			0x01
#define DMA_ARBITER_SP			0x02

struct enet_cb {
	struct sk_buff      *skb;
	void __iomem *bd_addr;
	DEFINE_DMA_UNMAP_ADDR(dma_addr);
	DEFINE_DMA_UNMAP_LEN(dma_len);
};


enum bcmgenet_power_mode {
	GENET_POWER_CABLE_SENSE = 0,
	GENET_POWER_PASSIVE,
	GENET_POWER_WOL_MAGIC,
};

struct bcmgenet_priv;


enum bcmgenet_version {
	GENET_V1 = 1,
	GENET_V2,
	GENET_V3,
	GENET_V4,
	GENET_V5
};

#define GENET_IS_V1(p)	((p)->version == GENET_V1)
#define GENET_IS_V2(p)	((p)->version == GENET_V2)
#define GENET_IS_V3(p)	((p)->version == GENET_V3)
#define GENET_IS_V4(p)	((p)->version == GENET_V4)
#define GENET_IS_V5(p)	((p)->version == GENET_V5)


#define GENET_HAS_40BITS	(1 << 0)
#define GENET_HAS_EXT		(1 << 1)
#define GENET_HAS_MDIO_INTR	(1 << 2)
#define GENET_HAS_MOCA_LINK_DET	(1 << 3)


struct bcmgenet_hw_params {
	u8		tx_queues;
	u8		tx_bds_per_q;
	u8		rx_queues;
	u8		rx_bds_per_q;
	u8		bp_in_en_shift;
	u32		bp_in_mask;
	u8		hfb_filter_cnt;
	u8		hfb_filter_size;
	u8		qtag_mask;
	u16		tbuf_offset;
	u32		hfb_offset;
	u32		hfb_reg_offset;
	u32		rdma_offset;
	u32		tdma_offset;
	u32		words_per_bd;
	u32		flags;
};

struct bcmgenet_skb_cb {
	struct enet_cb *first_cb;	
	struct enet_cb *last_cb;	
	unsigned int bytes_sent;	
};

#define GENET_CB(skb)	((struct bcmgenet_skb_cb *)((skb)->cb))

struct bcmgenet_tx_ring {
	spinlock_t	lock;		
	struct napi_struct napi;	
	unsigned long	packets;
	unsigned long	bytes;
	unsigned int	index;		
	unsigned int	queue;		
	struct enet_cb	*cbs;		
	unsigned int	size;		
	unsigned int    clean_ptr;      
	unsigned int	c_index;	
	unsigned int	free_bds;	
	unsigned int	write_ptr;	
	unsigned int	prod_index;	
	unsigned int	cb_ptr;		
	unsigned int	end_ptr;	
	void (*int_enable)(struct bcmgenet_tx_ring *);
	void (*int_disable)(struct bcmgenet_tx_ring *);
	struct bcmgenet_priv *priv;
};

struct bcmgenet_net_dim {
	u16		use_dim;
	u16		event_ctr;
	unsigned long	packets;
	unsigned long	bytes;
	struct dim	dim;
};

struct bcmgenet_rx_ring {
	struct napi_struct napi;	
	unsigned long	bytes;
	unsigned long	packets;
	unsigned long	errors;
	unsigned long	dropped;
	unsigned int	index;		
	struct enet_cb	*cbs;		
	unsigned int	size;		
	unsigned int	c_index;	
	unsigned int	read_ptr;	
	unsigned int	cb_ptr;		
	unsigned int	end_ptr;	
	unsigned int	old_discards;
	struct bcmgenet_net_dim dim;
	u32		rx_max_coalesced_frames;
	u32		rx_coalesce_usecs;
	void (*int_enable)(struct bcmgenet_rx_ring *);
	void (*int_disable)(struct bcmgenet_rx_ring *);
	struct bcmgenet_priv *priv;
};

enum bcmgenet_rxnfc_state {
	BCMGENET_RXNFC_STATE_UNUSED = 0,
	BCMGENET_RXNFC_STATE_DISABLED,
	BCMGENET_RXNFC_STATE_ENABLED
};

struct bcmgenet_rxnfc_rule {
	struct	list_head list;
	struct ethtool_rx_flow_spec	fs;
	enum bcmgenet_rxnfc_state state;
};


struct bcmgenet_priv {
	void __iomem *base;
	enum bcmgenet_version version;
	struct net_device *dev;

	
	void __iomem *tx_bds;
	struct enet_cb *tx_cbs;
	unsigned int num_tx_bds;

	struct bcmgenet_tx_ring tx_rings[DESC_INDEX + 1];

	
	void __iomem *rx_bds;
	struct enet_cb *rx_cbs;
	unsigned int num_rx_bds;
	unsigned int rx_buf_len;
	struct bcmgenet_rxnfc_rule rxnfc_rules[MAX_NUM_OF_FS_RULES];
	struct list_head rxnfc_list;

	struct bcmgenet_rx_ring rx_rings[DESC_INDEX + 1];

	
	struct bcmgenet_hw_params *hw_params;
	unsigned autoneg_pause:1;
	unsigned tx_pause:1;
	unsigned rx_pause:1;

	
	wait_queue_head_t wq;
	bool internal_phy;
	struct device_node *phy_dn;
	struct device_node *mdio_dn;
	struct mii_bus *mii_bus;
	u16 gphy_rev;
	struct clk *clk_eee;
	bool clk_eee_enabled;

	
	phy_interface_t phy_interface;
	int phy_addr;
	int ext_phy;
	bool ephy_16nm;

	
	struct work_struct bcmgenet_irq_work;
	int irq0;
	int irq1;
	int wol_irq;
	bool wol_irq_disabled;

	
	spinlock_t lock;
	unsigned int irq0_stat;

	
	bool crc_fwd_en;

	u32 dma_max_burst_length;

	u32 msg_enable;

	struct clk *clk;
	struct platform_device *pdev;
	struct platform_device *mii_pdev;

	
	struct clk *clk_wol;
	u32 wolopts;
	u8 sopass[SOPASS_MAX];
	bool wol_active;

	struct bcmgenet_mib_counters mib;

	struct ethtool_eee eee;
};

#define GENET_IO_MACRO(name, offset)					\
static inline u32 bcmgenet_##name##_readl(struct bcmgenet_priv *priv,	\
					u32 off)			\
{									\
									\
	if (IS_ENABLED(CONFIG_MIPS) && IS_ENABLED(CONFIG_CPU_BIG_ENDIAN)) \
		return __raw_readl(priv->base + offset + off);		\
	else								\
		return readl_relaxed(priv->base + offset + off);	\
}									\
static inline void bcmgenet_##name##_writel(struct bcmgenet_priv *priv,	\
					u32 val, u32 off)		\
{									\
	if (IS_ENABLED(CONFIG_MIPS) && IS_ENABLED(CONFIG_CPU_BIG_ENDIAN)) \
		__raw_writel(val, priv->base + offset + off);		\
	else								\
		writel_relaxed(val, priv->base + offset + off);		\
}

GENET_IO_MACRO(ext, GENET_EXT_OFF);
GENET_IO_MACRO(umac, GENET_UMAC_OFF);
GENET_IO_MACRO(sys, GENET_SYS_OFF);


GENET_IO_MACRO(intrl2_0, GENET_INTRL2_0_OFF);
GENET_IO_MACRO(intrl2_1, GENET_INTRL2_1_OFF);


GENET_IO_MACRO(hfb, priv->hw_params->hfb_offset);


GENET_IO_MACRO(hfb_reg, priv->hw_params->hfb_reg_offset);


GENET_IO_MACRO(rbuf, GENET_RBUF_OFF);


int bcmgenet_mii_init(struct net_device *dev);
int bcmgenet_mii_config(struct net_device *dev, bool init);
int bcmgenet_mii_probe(struct net_device *dev);
void bcmgenet_mii_exit(struct net_device *dev);
void bcmgenet_phy_pause_set(struct net_device *dev, bool rx, bool tx);
void bcmgenet_phy_power_set(struct net_device *dev, bool enable);
void bcmgenet_mii_setup(struct net_device *dev);


void bcmgenet_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol);
int bcmgenet_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol);
int bcmgenet_wol_power_down_cfg(struct bcmgenet_priv *priv,
				enum bcmgenet_power_mode mode);
void bcmgenet_wol_power_up_cfg(struct bcmgenet_priv *priv,
			       enum bcmgenet_power_mode mode);

void bcmgenet_eee_enable_set(struct net_device *dev, bool enable,
			     bool tx_lpi_enabled);

#endif 
