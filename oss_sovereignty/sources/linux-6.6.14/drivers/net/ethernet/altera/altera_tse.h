


#ifndef __ALTERA_TSE_H__
#define __ALTERA_TSE_H__

#define ALTERA_TSE_RESOURCE_NAME	"altera_tse"

#include <linux/bitops.h>
#include <linux/if_vlan.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/phy.h>
#include <linux/phylink.h>

#define ALTERA_TSE_SW_RESET_WATCHDOG_CNTR	10000
#define ALTERA_TSE_MAC_FIFO_WIDTH		4	

#define ALTERA_TSE_RX_SECTION_EMPTY	16
#define ALTERA_TSE_RX_SECTION_FULL	0
#define ALTERA_TSE_RX_ALMOST_EMPTY	8
#define ALTERA_TSE_RX_ALMOST_FULL	8


#define ALTERA_TSE_TX_SECTION_EMPTY	16
#define ALTERA_TSE_TX_SECTION_FULL	0
#define ALTERA_TSE_TX_ALMOST_EMPTY	8
#define ALTERA_TSE_TX_ALMOST_FULL	3


#define ALTERA_TSE_TX_IPG_LENGTH	12

#define ALTERA_TSE_PAUSE_QUANTA		0xffff

#define GET_BIT_VALUE(v, bit)		(((v) >> (bit)) & 0x1)


#define MAC_CMDCFG_TX_ENA			BIT(0)
#define MAC_CMDCFG_RX_ENA			BIT(1)
#define MAC_CMDCFG_XON_GEN			BIT(2)
#define MAC_CMDCFG_ETH_SPEED			BIT(3)
#define MAC_CMDCFG_PROMIS_EN			BIT(4)
#define MAC_CMDCFG_PAD_EN			BIT(5)
#define MAC_CMDCFG_CRC_FWD			BIT(6)
#define MAC_CMDCFG_PAUSE_FWD			BIT(7)
#define MAC_CMDCFG_PAUSE_IGNORE			BIT(8)
#define MAC_CMDCFG_TX_ADDR_INS			BIT(9)
#define MAC_CMDCFG_HD_ENA			BIT(10)
#define MAC_CMDCFG_EXCESS_COL			BIT(11)
#define MAC_CMDCFG_LATE_COL			BIT(12)
#define MAC_CMDCFG_SW_RESET			BIT(13)
#define MAC_CMDCFG_MHASH_SEL			BIT(14)
#define MAC_CMDCFG_LOOP_ENA			BIT(15)
#define MAC_CMDCFG_TX_ADDR_SEL(v)		(((v) & 0x7) << 16)
#define MAC_CMDCFG_MAGIC_ENA			BIT(19)
#define MAC_CMDCFG_SLEEP			BIT(20)
#define MAC_CMDCFG_WAKEUP			BIT(21)
#define MAC_CMDCFG_XOFF_GEN			BIT(22)
#define MAC_CMDCFG_CNTL_FRM_ENA			BIT(23)
#define MAC_CMDCFG_NO_LGTH_CHECK		BIT(24)
#define MAC_CMDCFG_ENA_10			BIT(25)
#define MAC_CMDCFG_RX_ERR_DISC			BIT(26)
#define MAC_CMDCFG_DISABLE_READ_TIMEOUT		BIT(27)
#define MAC_CMDCFG_CNT_RESET			BIT(31)

#define MAC_CMDCFG_TX_ENA_GET(v)		GET_BIT_VALUE(v, 0)
#define MAC_CMDCFG_RX_ENA_GET(v)		GET_BIT_VALUE(v, 1)
#define MAC_CMDCFG_XON_GEN_GET(v)		GET_BIT_VALUE(v, 2)
#define MAC_CMDCFG_ETH_SPEED_GET(v)		GET_BIT_VALUE(v, 3)
#define MAC_CMDCFG_PROMIS_EN_GET(v)		GET_BIT_VALUE(v, 4)
#define MAC_CMDCFG_PAD_EN_GET(v)		GET_BIT_VALUE(v, 5)
#define MAC_CMDCFG_CRC_FWD_GET(v)		GET_BIT_VALUE(v, 6)
#define MAC_CMDCFG_PAUSE_FWD_GET(v)		GET_BIT_VALUE(v, 7)
#define MAC_CMDCFG_PAUSE_IGNORE_GET(v)		GET_BIT_VALUE(v, 8)
#define MAC_CMDCFG_TX_ADDR_INS_GET(v)		GET_BIT_VALUE(v, 9)
#define MAC_CMDCFG_HD_ENA_GET(v)		GET_BIT_VALUE(v, 10)
#define MAC_CMDCFG_EXCESS_COL_GET(v)		GET_BIT_VALUE(v, 11)
#define MAC_CMDCFG_LATE_COL_GET(v)		GET_BIT_VALUE(v, 12)
#define MAC_CMDCFG_SW_RESET_GET(v)		GET_BIT_VALUE(v, 13)
#define MAC_CMDCFG_MHASH_SEL_GET(v)		GET_BIT_VALUE(v, 14)
#define MAC_CMDCFG_LOOP_ENA_GET(v)		GET_BIT_VALUE(v, 15)
#define MAC_CMDCFG_TX_ADDR_SEL_GET(v)		(((v) >> 16) & 0x7)
#define MAC_CMDCFG_MAGIC_ENA_GET(v)		GET_BIT_VALUE(v, 19)
#define MAC_CMDCFG_SLEEP_GET(v)			GET_BIT_VALUE(v, 20)
#define MAC_CMDCFG_WAKEUP_GET(v)		GET_BIT_VALUE(v, 21)
#define MAC_CMDCFG_XOFF_GEN_GET(v)		GET_BIT_VALUE(v, 22)
#define MAC_CMDCFG_CNTL_FRM_ENA_GET(v)		GET_BIT_VALUE(v, 23)
#define MAC_CMDCFG_NO_LGTH_CHECK_GET(v)		GET_BIT_VALUE(v, 24)
#define MAC_CMDCFG_ENA_10_GET(v)		GET_BIT_VALUE(v, 25)
#define MAC_CMDCFG_RX_ERR_DISC_GET(v)		GET_BIT_VALUE(v, 26)
#define MAC_CMDCFG_DISABLE_READ_TIMEOUT_GET(v)	GET_BIT_VALUE(v, 27)
#define MAC_CMDCFG_CNT_RESET_GET(v)		GET_BIT_VALUE(v, 31)


struct altera_tse_mdio {
	u32 control;	
	u32 status;	
	u32 phy_id1;	
	u32 phy_id2;	
	u32 auto_negotiation_advertisement;	
	u32 remote_partner_base_page_ability;

	u32 reg6;
	u32 reg7;
	u32 reg8;
	u32 reg9;
	u32 rega;
	u32 regb;
	u32 regc;
	u32 regd;
	u32 rege;
	u32 regf;
	u32 reg10;
	u32 reg11;
	u32 reg12;
	u32 reg13;
	u32 reg14;
	u32 reg15;
	u32 reg16;
	u32 reg17;
	u32 reg18;
	u32 reg19;
	u32 reg1a;
	u32 reg1b;
	u32 reg1c;
	u32 reg1d;
	u32 reg1e;
	u32 reg1f;
};


struct altera_tse_mac {
	
	u32 megacore_revision;
	
	u32 scratch_pad;
	
	u32 command_config;
	
	u32 mac_addr_0;
	
	u32 mac_addr_1;
	
	u32 frm_length;
	
	u32 pause_quanta;
	
	u32 rx_section_empty;
	
	u32 rx_section_full;
	
	u32 tx_section_empty;
	
	u32 tx_section_full;
	
	u32 rx_almost_empty;
	
	u32 rx_almost_full;
	
	u32 tx_almost_empty;
	
	u32 tx_almost_full;
	
	u32 mdio_phy0_addr;
	
	u32 mdio_phy1_addr;

	
	u32 holdoff_quant;

	
	u32 reserved1[5];

	
	u32 tx_ipg_length;

	

	
	u32 mac_id_1;
	u32 mac_id_2;

	
	u32 frames_transmitted_ok;
	
	u32 frames_received_ok;
	
	u32 frames_check_sequence_errors;
	
	u32 alignment_errors;
	
	u32 octets_transmitted_ok;
	
	u32 octets_received_ok;

	

	
	u32 tx_pause_mac_ctrl_frames;
	
	u32 rx_pause_mac_ctrl_frames;

	

	
	u32 if_in_errors;
	
	u32 if_out_errors;
	
	u32 if_in_ucast_pkts;
	
	u32 if_in_multicast_pkts;
	
	u32 if_in_broadcast_pkts;
	u32 if_out_discards;
	
	u32 if_out_ucast_pkts;
	
	u32 if_out_multicast_pkts;
	u32 if_out_broadcast_pkts;

	

	
	u32 ether_stats_drop_events;
	
	u32 ether_stats_octets;
	
	u32 ether_stats_pkts;
	
	u32 ether_stats_undersize_pkts;
	
	u32 ether_stats_oversize_pkts;
	
	u32 ether_stats_pkts_64_octets;
	
	u32 ether_stats_pkts_65to127_octets;
	
	u32 ether_stats_pkts_128to255_octets;
	
	u32 ether_stats_pkts_256to511_octets;
	
	u32 ether_stats_pkts_512to1023_octets;
	
	u32 ether_stats_pkts_1024to1518_octets;

	
	u32 ether_stats_pkts_1519tox_octets;
	
	u32 ether_stats_jabbers;
	
	u32 ether_stats_fragments;

	u32 reserved2;

	
	u32 tx_cmd_stat;
	u32 rx_cmd_stat;

	
	u32 msb_octets_transmitted_ok;
	u32 msb_octets_received_ok;
	u32 msb_ether_stats_octets;

	u32 reserved3;

	
	u32 hash_table[64];

	
	struct altera_tse_mdio mdio_phy0;
	struct altera_tse_mdio mdio_phy1;

	
	u32 supp_mac_addr_0_0;
	u32 supp_mac_addr_0_1;
	u32 supp_mac_addr_1_0;
	u32 supp_mac_addr_1_1;
	u32 supp_mac_addr_2_0;
	u32 supp_mac_addr_2_1;
	u32 supp_mac_addr_3_0;
	u32 supp_mac_addr_3_1;

	u32 reserved4[8];

	
	u32 tx_period;
	u32 tx_adjust_fns;
	u32 tx_adjust_ns;
	u32 rx_period;
	u32 rx_adjust_fns;
	u32 rx_adjust_ns;

	u32 reserved5[42];
};

#define tse_csroffs(a) (offsetof(struct altera_tse_mac, a))


#define ALTERA_TSE_TX_CMD_STAT_OMIT_CRC		BIT(17)
#define ALTERA_TSE_TX_CMD_STAT_TX_SHIFT16	BIT(18)
#define ALTERA_TSE_RX_CMD_STAT_RX_SHIFT16	BIT(25)


struct tse_buffer {
	struct list_head lh;
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	u32 len;
	int mapped_as_page;
};

struct altera_tse_private;

#define ALTERA_DTYPE_SGDMA 1
#define ALTERA_DTYPE_MSGDMA 2


struct altera_dmaops {
	int altera_dtype;
	int dmamask;
	void (*reset_dma)(struct altera_tse_private *);
	void (*enable_txirq)(struct altera_tse_private *);
	void (*enable_rxirq)(struct altera_tse_private *);
	void (*disable_txirq)(struct altera_tse_private *);
	void (*disable_rxirq)(struct altera_tse_private *);
	void (*clear_txirq)(struct altera_tse_private *);
	void (*clear_rxirq)(struct altera_tse_private *);
	int (*tx_buffer)(struct altera_tse_private *, struct tse_buffer *);
	u32 (*tx_completions)(struct altera_tse_private *);
	void (*add_rx_desc)(struct altera_tse_private *, struct tse_buffer *);
	u32 (*get_rx_status)(struct altera_tse_private *);
	int (*init_dma)(struct altera_tse_private *);
	void (*uninit_dma)(struct altera_tse_private *);
	void (*start_rxdma)(struct altera_tse_private *);
};


struct altera_tse_private {
	struct net_device *dev;
	struct device *device;
	struct napi_struct napi;

	
	struct altera_tse_mac __iomem *mac_dev;

	
	u32	revision;

	
	void __iomem *rx_dma_csr;
	void __iomem *rx_dma_desc;
	void __iomem *rx_dma_resp;

	
	void __iomem *tx_dma_csr;
	void __iomem *tx_dma_desc;

	
	void __iomem *pcs_base;

	
	struct tse_buffer *rx_ring;
	u32 rx_cons;
	u32 rx_prod;
	u32 rx_ring_size;
	u32 rx_dma_buf_sz;

	
	struct tse_buffer *tx_ring;
	u32 tx_prod;
	u32 tx_cons;
	u32 tx_ring_size;

	
	u32 tx_irq;
	u32 rx_irq;

	
	u32 tx_fifo_depth;
	u32 rx_fifo_depth;

	
	u32 hash_filter;
	u32 added_unicast;

	
	u32 txdescmem;
	u32 rxdescmem;
	dma_addr_t rxdescmem_busaddr;
	dma_addr_t txdescmem_busaddr;
	u32 txctrlreg;
	u32 rxctrlreg;
	dma_addr_t rxdescphys;
	dma_addr_t txdescphys;

	struct list_head txlisthd;
	struct list_head rxlisthd;

	
	spinlock_t mac_cfg_lock;
	
	spinlock_t tx_lock;
	
	spinlock_t rxdma_irq_lock;

	
	int phy_addr;		
	phy_interface_t phy_iface;
	struct mii_bus *mdio;
	int oldspeed;
	int oldduplex;
	int oldlink;

	
	u32 msg_enable;

	struct altera_dmaops *dmaops;

	struct phylink *phylink;
	struct phylink_config phylink_config;
	struct phylink_pcs *pcs;
};


void altera_tse_set_ethtool_ops(struct net_device *);

static inline
u32 csrrd32(void __iomem *mac, size_t offs)
{
	void __iomem *paddr = (void __iomem *)((uintptr_t)mac + offs);
	return readl(paddr);
}

static inline
u16 csrrd16(void __iomem *mac, size_t offs)
{
	void __iomem *paddr = (void __iomem *)((uintptr_t)mac + offs);
	return readw(paddr);
}

static inline
u8 csrrd8(void __iomem *mac, size_t offs)
{
	void __iomem *paddr = (void __iomem *)((uintptr_t)mac + offs);
	return readb(paddr);
}

static inline
void csrwr32(u32 val, void __iomem *mac, size_t offs)
{
	void __iomem *paddr = (void __iomem *)((uintptr_t)mac + offs);

	writel(val, paddr);
}

static inline
void csrwr16(u16 val, void __iomem *mac, size_t offs)
{
	void __iomem *paddr = (void __iomem *)((uintptr_t)mac + offs);

	writew(val, paddr);
}

static inline
void csrwr8(u8 val, void __iomem *mac, size_t offs)
{
	void __iomem *paddr = (void __iomem *)((uintptr_t)mac + offs);

	writeb(val, paddr);
}

#endif 
