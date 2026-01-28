





#ifndef FEC_H
#define	FEC_H


#include <linux/clocksource.h>
#include <linux/net_tstamp.h>
#include <linux/pm_qos.h>
#include <linux/bpf.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/timecounter.h>
#include <dt-bindings/firmware/imx/rsrc.h>
#include <linux/firmware/imx/sci.h>
#include <net/xdp.h>

#if defined(CONFIG_M523x) || defined(CONFIG_M527x) || defined(CONFIG_M528x) || \
    defined(CONFIG_M520x) || defined(CONFIG_M532x) || defined(CONFIG_ARM) || \
    defined(CONFIG_ARM64) || defined(CONFIG_COMPILE_TEST)

#define FEC_IEVENT		0x004 
#define FEC_IMASK		0x008 
#define FEC_R_DES_ACTIVE_0	0x010 
#define FEC_X_DES_ACTIVE_0	0x014 
#define FEC_ECNTRL		0x024 
#define FEC_MII_DATA		0x040 
#define FEC_MII_SPEED		0x044 
#define FEC_MIB_CTRLSTAT	0x064 
#define FEC_R_CNTRL		0x084 
#define FEC_X_CNTRL		0x0c4 
#define FEC_ADDR_LOW		0x0e4 
#define FEC_ADDR_HIGH		0x0e8 
#define FEC_OPD			0x0ec 
#define FEC_TXIC0		0x0f0 
#define FEC_TXIC1		0x0f4 
#define FEC_TXIC2		0x0f8 
#define FEC_RXIC0		0x100 
#define FEC_RXIC1		0x104 
#define FEC_RXIC2		0x108 
#define FEC_HASH_TABLE_HIGH	0x118 
#define FEC_HASH_TABLE_LOW	0x11c 
#define FEC_GRP_HASH_TABLE_HIGH	0x120 
#define FEC_GRP_HASH_TABLE_LOW	0x124 
#define FEC_X_WMRK		0x144 
#define FEC_R_BOUND		0x14c 
#define FEC_R_FSTART		0x150 
#define FEC_R_DES_START_1	0x160 
#define FEC_X_DES_START_1	0x164 
#define FEC_R_BUFF_SIZE_1	0x168 
#define FEC_R_DES_START_2	0x16c 
#define FEC_X_DES_START_2	0x170 
#define FEC_R_BUFF_SIZE_2	0x174 
#define FEC_R_DES_START_0	0x180 
#define FEC_X_DES_START_0	0x184 
#define FEC_R_BUFF_SIZE_0	0x188 
#define FEC_R_FIFO_RSFL		0x190 
#define FEC_R_FIFO_RSEM		0x194 
#define FEC_R_FIFO_RAEM		0x198 
#define FEC_R_FIFO_RAFL		0x19c 
#define FEC_FTRL		0x1b0 
#define FEC_RACC		0x1c4 
#define FEC_RCMR_1		0x1c8 
#define FEC_RCMR_2		0x1cc 
#define FEC_DMA_CFG_1		0x1d8 
#define FEC_DMA_CFG_2		0x1dc 
#define FEC_R_DES_ACTIVE_1	0x1e0 
#define FEC_X_DES_ACTIVE_1	0x1e4 
#define FEC_R_DES_ACTIVE_2	0x1e8 
#define FEC_X_DES_ACTIVE_2	0x1ec 
#define FEC_QOS_SCHEME		0x1f0 
#define FEC_LPI_SLEEP		0x1f4 
#define FEC_LPI_WAKE		0x1f8 
#define FEC_MIIGSK_CFGR		0x300 
#define FEC_MIIGSK_ENR		0x308 

#define BM_MIIGSK_CFGR_MII		0x00
#define BM_MIIGSK_CFGR_RMII		0x01
#define BM_MIIGSK_CFGR_FRCONT_10M	0x40

#define RMON_T_DROP		0x200 
#define RMON_T_PACKETS		0x204 
#define RMON_T_BC_PKT		0x208 
#define RMON_T_MC_PKT		0x20c 
#define RMON_T_CRC_ALIGN	0x210 
#define RMON_T_UNDERSIZE	0x214 
#define RMON_T_OVERSIZE		0x218 
#define RMON_T_FRAG		0x21c 
#define RMON_T_JAB		0x220 
#define RMON_T_COL		0x224 
#define RMON_T_P64		0x228 
#define RMON_T_P65TO127		0x22c 
#define RMON_T_P128TO255	0x230 
#define RMON_T_P256TO511	0x234 
#define RMON_T_P512TO1023	0x238 
#define RMON_T_P1024TO2047	0x23c 
#define RMON_T_P_GTE2048	0x240 
#define RMON_T_OCTETS		0x244 
#define IEEE_T_DROP		0x248 
#define IEEE_T_FRAME_OK		0x24c 
#define IEEE_T_1COL		0x250 
#define IEEE_T_MCOL		0x254 
#define IEEE_T_DEF		0x258 
#define IEEE_T_LCOL		0x25c 
#define IEEE_T_EXCOL		0x260 
#define IEEE_T_MACERR		0x264 
#define IEEE_T_CSERR		0x268 
#define IEEE_T_SQE		0x26c 
#define IEEE_T_FDXFC		0x270 
#define IEEE_T_OCTETS_OK	0x274 
#define RMON_R_PACKETS		0x284 
#define RMON_R_BC_PKT		0x288 
#define RMON_R_MC_PKT		0x28c 
#define RMON_R_CRC_ALIGN	0x290 
#define RMON_R_UNDERSIZE	0x294 
#define RMON_R_OVERSIZE		0x298 
#define RMON_R_FRAG		0x29c 
#define RMON_R_JAB		0x2a0 
#define RMON_R_RESVD_O		0x2a4 
#define RMON_R_P64		0x2a8 
#define RMON_R_P65TO127		0x2ac 
#define RMON_R_P128TO255	0x2b0 
#define RMON_R_P256TO511	0x2b4 
#define RMON_R_P512TO1023	0x2b8 
#define RMON_R_P1024TO2047	0x2bc 
#define RMON_R_P_GTE2048	0x2c0 
#define RMON_R_OCTETS		0x2c4 
#define IEEE_R_DROP		0x2c8 
#define IEEE_R_FRAME_OK		0x2cc 
#define IEEE_R_CRC		0x2d0 
#define IEEE_R_ALIGN		0x2d4 
#define IEEE_R_MACERR		0x2d8 
#define IEEE_R_FDXFC		0x2dc 
#define IEEE_R_OCTETS_OK	0x2e0 

#else

#define FEC_ECNTRL		0x000 
#define FEC_IEVENT		0x004 
#define FEC_IMASK		0x008 
#define FEC_IVEC		0x00c 
#define FEC_R_DES_ACTIVE_0	0x010 
#define FEC_R_DES_ACTIVE_1	FEC_R_DES_ACTIVE_0
#define FEC_R_DES_ACTIVE_2	FEC_R_DES_ACTIVE_0
#define FEC_X_DES_ACTIVE_0	0x014 
#define FEC_X_DES_ACTIVE_1	FEC_X_DES_ACTIVE_0
#define FEC_X_DES_ACTIVE_2	FEC_X_DES_ACTIVE_0
#define FEC_MII_DATA		0x040 
#define FEC_MII_SPEED		0x044 
#define FEC_R_BOUND		0x08c 
#define FEC_R_FSTART		0x090 
#define FEC_X_WMRK		0x0a4 
#define FEC_X_FSTART		0x0ac 
#define FEC_R_CNTRL		0x104 
#define FEC_MAX_FRM_LEN		0x108 
#define FEC_X_CNTRL		0x144 
#define FEC_ADDR_LOW		0x3c0 
#define FEC_ADDR_HIGH		0x3c4 
#define FEC_GRP_HASH_TABLE_HIGH	0x3c8 
#define FEC_GRP_HASH_TABLE_LOW	0x3cc 
#define FEC_R_DES_START_0	0x3d0 
#define FEC_R_DES_START_1	FEC_R_DES_START_0
#define FEC_R_DES_START_2	FEC_R_DES_START_0
#define FEC_X_DES_START_0	0x3d4 
#define FEC_X_DES_START_1	FEC_X_DES_START_0
#define FEC_X_DES_START_2	FEC_X_DES_START_0
#define FEC_R_BUFF_SIZE_0	0x3d8 
#define FEC_R_BUFF_SIZE_1	FEC_R_BUFF_SIZE_0
#define FEC_R_BUFF_SIZE_2	FEC_R_BUFF_SIZE_0
#define FEC_FIFO_RAM		0x400 

#define FEC_RCMR_1		0xfff
#define FEC_RCMR_2		0xfff
#define FEC_DMA_CFG_1		0xfff
#define FEC_DMA_CFG_2		0xfff
#define FEC_TXIC0		0xfff
#define FEC_TXIC1		0xfff
#define FEC_TXIC2		0xfff
#define FEC_RXIC0		0xfff
#define FEC_RXIC1		0xfff
#define FEC_RXIC2		0xfff
#define FEC_LPI_SLEEP		0xfff
#define FEC_LPI_WAKE		0xfff
#endif 



#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
#define fec32_to_cpu le32_to_cpu
#define fec16_to_cpu le16_to_cpu
#define cpu_to_fec32 cpu_to_le32
#define cpu_to_fec16 cpu_to_le16
#define __fec32 __le32
#define __fec16 __le16

struct bufdesc {
	__fec16 cbd_datlen;	
	__fec16 cbd_sc;		
	__fec32 cbd_bufaddr;	
};
#else
#define fec32_to_cpu be32_to_cpu
#define fec16_to_cpu be16_to_cpu
#define cpu_to_fec32 cpu_to_be32
#define cpu_to_fec16 cpu_to_be16
#define __fec32 __be32
#define __fec16 __be16

struct bufdesc {
	__fec16	cbd_sc;		
	__fec16	cbd_datlen;	
	__fec32	cbd_bufaddr;	
};
#endif

struct bufdesc_ex {
	struct bufdesc desc;
	__fec32 cbd_esc;
	__fec32 cbd_prot;
	__fec32 cbd_bdu;
	__fec32 ts;
	__fec16 res0[4];
};


#define BD_SC_EMPTY	((ushort)0x8000)	
#define BD_SC_READY	((ushort)0x8000)	
#define BD_SC_WRAP	((ushort)0x2000)	
#define BD_SC_INTRPT	((ushort)0x1000)	
#define BD_SC_CM	((ushort)0x0200)	
#define BD_SC_ID	((ushort)0x0100)	
#define BD_SC_P		((ushort)0x0100)	
#define BD_SC_BR	((ushort)0x0020)	
#define BD_SC_FR	((ushort)0x0010)	
#define BD_SC_PR	((ushort)0x0008)	
#define BD_SC_OV	((ushort)0x0002)	
#define BD_SC_CD	((ushort)0x0001)	


#define BD_ENET_RX_EMPTY	((ushort)0x8000)
#define BD_ENET_RX_WRAP		((ushort)0x2000)
#define BD_ENET_RX_INTR		((ushort)0x1000)
#define BD_ENET_RX_LAST		((ushort)0x0800)
#define BD_ENET_RX_FIRST	((ushort)0x0400)
#define BD_ENET_RX_MISS		((ushort)0x0100)
#define BD_ENET_RX_LG		((ushort)0x0020)
#define BD_ENET_RX_NO		((ushort)0x0010)
#define BD_ENET_RX_SH		((ushort)0x0008)
#define BD_ENET_RX_CR		((ushort)0x0004)
#define BD_ENET_RX_OV		((ushort)0x0002)
#define BD_ENET_RX_CL		((ushort)0x0001)
#define BD_ENET_RX_STATS	((ushort)0x013f)	


#define BD_ENET_RX_VLAN		0x00000004


#define BD_ENET_TX_READY	((ushort)0x8000)
#define BD_ENET_TX_PAD		((ushort)0x4000)
#define BD_ENET_TX_WRAP		((ushort)0x2000)
#define BD_ENET_TX_INTR		((ushort)0x1000)
#define BD_ENET_TX_LAST		((ushort)0x0800)
#define BD_ENET_TX_TC		((ushort)0x0400)
#define BD_ENET_TX_DEF		((ushort)0x0200)
#define BD_ENET_TX_HB		((ushort)0x0100)
#define BD_ENET_TX_LC		((ushort)0x0080)
#define BD_ENET_TX_RL		((ushort)0x0040)
#define BD_ENET_TX_RCMASK	((ushort)0x003c)
#define BD_ENET_TX_UN		((ushort)0x0002)
#define BD_ENET_TX_CSL		((ushort)0x0001)
#define BD_ENET_TX_STATS	((ushort)0x0fff)	


#define BD_ENET_TX_INT		0x40000000
#define BD_ENET_TX_TS		0x20000000
#define BD_ENET_TX_PINS		0x10000000
#define BD_ENET_TX_IINS		0x08000000



#define FEC_IRQ_NUM		3


#define FEC_ENET_MAX_TX_QS	3
#define FEC_ENET_MAX_RX_QS	3

#define FEC_R_DES_START(X)	(((X) == 1) ? FEC_R_DES_START_1 : \
				(((X) == 2) ? \
					FEC_R_DES_START_2 : FEC_R_DES_START_0))
#define FEC_X_DES_START(X)	(((X) == 1) ? FEC_X_DES_START_1 : \
				(((X) == 2) ? \
					FEC_X_DES_START_2 : FEC_X_DES_START_0))
#define FEC_R_BUFF_SIZE(X)	(((X) == 1) ? FEC_R_BUFF_SIZE_1 : \
				(((X) == 2) ? \
					FEC_R_BUFF_SIZE_2 : FEC_R_BUFF_SIZE_0))

#define FEC_DMA_CFG(X)		(((X) == 2) ? FEC_DMA_CFG_2 : FEC_DMA_CFG_1)

#define DMA_CLASS_EN		(1 << 16)
#define FEC_RCMR(X)		(((X) == 2) ? FEC_RCMR_2 : FEC_RCMR_1)
#define IDLE_SLOPE_MASK		0xffff
#define IDLE_SLOPE_1		0x200 
#define IDLE_SLOPE_2		0x200 
#define IDLE_SLOPE(X)		(((X) == 1) ?				\
				(IDLE_SLOPE_1 & IDLE_SLOPE_MASK) :	\
				(IDLE_SLOPE_2 & IDLE_SLOPE_MASK))
#define RCMR_MATCHEN		(0x1 << 16)
#define RCMR_CMP_CFG(v, n)	(((v) & 0x7) <<  (n << 2))
#define RCMR_CMP_1		(RCMR_CMP_CFG(0, 0) | RCMR_CMP_CFG(1, 1) | \
				RCMR_CMP_CFG(2, 2) | RCMR_CMP_CFG(3, 3))
#define RCMR_CMP_2		(RCMR_CMP_CFG(4, 0) | RCMR_CMP_CFG(5, 1) | \
				RCMR_CMP_CFG(6, 2) | RCMR_CMP_CFG(7, 3))
#define RCMR_CMP(X)		(((X) == 1) ? RCMR_CMP_1 : RCMR_CMP_2)
#define FEC_TX_BD_FTYPE(X)	(((X) & 0xf) << 20)



#define FEC_ENET_XDP_HEADROOM	(XDP_PACKET_HEADROOM)
#define FEC_ENET_RX_PAGES	256
#define FEC_ENET_RX_FRSIZE	(PAGE_SIZE - FEC_ENET_XDP_HEADROOM \
		- SKB_DATA_ALIGN(sizeof(struct skb_shared_info)))
#define FEC_ENET_RX_FRPPG	(PAGE_SIZE / FEC_ENET_RX_FRSIZE)
#define RX_RING_SIZE		(FEC_ENET_RX_FRPPG * FEC_ENET_RX_PAGES)
#define FEC_ENET_TX_FRSIZE	2048
#define FEC_ENET_TX_FRPPG	(PAGE_SIZE / FEC_ENET_TX_FRSIZE)
#define TX_RING_SIZE		1024	
#define TX_RING_MOD_MASK	511	

#define BD_ENET_RX_INT		0x00800000
#define BD_ENET_RX_PTP		((ushort)0x0400)
#define BD_ENET_RX_ICE		0x00000020
#define BD_ENET_RX_PCR		0x00000010
#define FLAG_RX_CSUM_ENABLED	(BD_ENET_RX_ICE | BD_ENET_RX_PCR)
#define FLAG_RX_CSUM_ERROR	(BD_ENET_RX_ICE | BD_ENET_RX_PCR)


#define FEC_ENET_HBERR  ((uint)0x80000000)      
#define FEC_ENET_BABR   ((uint)0x40000000)      
#define FEC_ENET_BABT   ((uint)0x20000000)      
#define FEC_ENET_GRA    ((uint)0x10000000)      
#define FEC_ENET_TXF_0	((uint)0x08000000)	
#define FEC_ENET_TXF_1	((uint)0x00000008)	
#define FEC_ENET_TXF_2	((uint)0x00000080)	
#define FEC_ENET_TXB    ((uint)0x04000000)      
#define FEC_ENET_RXF_0	((uint)0x02000000)	
#define FEC_ENET_RXF_1	((uint)0x00000002)	
#define FEC_ENET_RXF_2	((uint)0x00000020)	
#define FEC_ENET_RXB    ((uint)0x01000000)      
#define FEC_ENET_MII    ((uint)0x00800000)      
#define FEC_ENET_EBERR  ((uint)0x00400000)      
#define FEC_ENET_WAKEUP	((uint)0x00020000)	
#define FEC_ENET_TXF	(FEC_ENET_TXF_0 | FEC_ENET_TXF_1 | FEC_ENET_TXF_2)
#define FEC_ENET_RXF	(FEC_ENET_RXF_0 | FEC_ENET_RXF_1 | FEC_ENET_RXF_2)
#define FEC_ENET_RXF_GET(X)	(((X) == 0) ? FEC_ENET_RXF_0 :	\
				(((X) == 1) ? FEC_ENET_RXF_1 :	\
				FEC_ENET_RXF_2))
#define FEC_ENET_TS_AVAIL       ((uint)0x00010000)
#define FEC_ENET_TS_TIMER       ((uint)0x00008000)

#define FEC_DEFAULT_IMASK (FEC_ENET_TXF | FEC_ENET_RXF)
#define FEC_RX_DISABLED_IMASK (FEC_DEFAULT_IMASK & (~FEC_ENET_RXF))

#define FEC_ENET_TXC_DLY	((uint)0x00010000)
#define FEC_ENET_RXC_DLY	((uint)0x00020000)


#define FEC_ITR_CLK_SEL		(0x1 << 30)
#define FEC_ITR_EN		(0x1 << 31)
#define FEC_ITR_ICFT(X)		(((X) & 0xff) << 20)
#define FEC_ITR_ICTT(X)		((X) & 0xffff)
#define FEC_ITR_ICFT_DEFAULT	200  
#define FEC_ITR_ICTT_DEFAULT	1000 

#define FEC_VLAN_TAG_LEN	0x04
#define FEC_ETHTYPE_LEN		0x02


#define FEC_QUIRK_ENET_MAC		(1 << 0)

#define FEC_QUIRK_SWAP_FRAME		(1 << 1)

#define FEC_QUIRK_USE_GASKET		(1 << 2)

#define FEC_QUIRK_HAS_GBIT		(1 << 3)

#define FEC_QUIRK_HAS_BUFDESC_EX	(1 << 4)

#define FEC_QUIRK_HAS_CSUM		(1 << 5)

#define FEC_QUIRK_HAS_VLAN		(1 << 6)

#define FEC_QUIRK_ERR006358		(1 << 7)

#define FEC_QUIRK_HAS_AVB		(1 << 8)

#define FEC_QUIRK_ERR007885		(1 << 9)

#define FEC_QUIRK_BUG_CAPTURE		(1 << 10)

#define FEC_QUIRK_SINGLE_MDIO		(1 << 11)

#define FEC_QUIRK_HAS_RACC		(1 << 12)

#define FEC_QUIRK_HAS_COALESCE		(1 << 13)

#define FEC_QUIRK_ERR006687		(1 << 14)

#define FEC_QUIRK_MIB_CLEAR		(1 << 15)

#define FEC_QUIRK_HAS_FRREG		(1 << 16)


#define FEC_QUIRK_CLEAR_SETUP_MII	(1 << 17)


#define FEC_QUIRK_NO_HARD_RESET		(1 << 18)


#define FEC_QUIRK_HAS_MULTI_QUEUES	(1 << 19)


#define FEC_QUIRK_HAS_EEE		(1 << 20)


#define FEC_QUIRK_DELAYED_CLKS_SUPPORT	(1 << 21)


#define FEC_QUIRK_WAKEUP_FROM_INT2	(1 << 22)


#define FEC_QUIRK_HAS_PMQOS			BIT(23)


#define FEC_QUIRK_HAS_MDIO_C45		BIT(24)

struct bufdesc_prop {
	int qid;
	
	struct bufdesc	*base;
	struct bufdesc	*last;
	struct bufdesc	*cur;
	void __iomem	*reg_desc_active;
	dma_addr_t	dma;
	unsigned short ring_size;
	unsigned char dsize;
	unsigned char dsize_log2;
};

struct fec_enet_priv_txrx_info {
	int	offset;
	struct	page *page;
	struct  sk_buff *skb;
};

enum {
	RX_XDP_REDIRECT = 0,
	RX_XDP_PASS,
	RX_XDP_DROP,
	RX_XDP_TX,
	RX_XDP_TX_ERRORS,
	TX_XDP_XMIT,
	TX_XDP_XMIT_ERRORS,

	
	XDP_STATS_TOTAL,
};

enum fec_txbuf_type {
	FEC_TXBUF_T_SKB,
	FEC_TXBUF_T_XDP_NDO,
	FEC_TXBUF_T_XDP_TX,
};

struct fec_tx_buffer {
	void *buf_p;
	enum fec_txbuf_type type;
};

struct fec_enet_priv_tx_q {
	struct bufdesc_prop bd;
	unsigned char *tx_bounce[TX_RING_SIZE];
	struct fec_tx_buffer tx_buf[TX_RING_SIZE];

	unsigned short tx_stop_threshold;
	unsigned short tx_wake_threshold;

	struct bufdesc	*dirty_tx;
	char *tso_hdrs;
	dma_addr_t tso_hdrs_dma;
};

struct fec_enet_priv_rx_q {
	struct bufdesc_prop bd;
	struct  fec_enet_priv_txrx_info rx_skb_info[RX_RING_SIZE];

	
	struct page_pool *page_pool;
	struct xdp_rxq_info xdp_rxq;
	u32 stats[XDP_STATS_TOTAL];

	
	u8 id;
};

struct fec_stop_mode_gpr {
	struct regmap *gpr;
	u8 reg;
	u8 bit;
};


struct fec_enet_private {
	
	void __iomem *hwp;

	struct net_device *netdev;

	struct clk *clk_ipg;
	struct clk *clk_ahb;
	struct clk *clk_ref;
	struct clk *clk_enet_out;
	struct clk *clk_ptp;
	struct clk *clk_2x_txclk;

	bool ptp_clk_on;
	struct mutex ptp_clk_mutex;
	unsigned int num_tx_queues;
	unsigned int num_rx_queues;

	
	struct fec_enet_priv_tx_q *tx_queue[FEC_ENET_MAX_TX_QS];
	struct fec_enet_priv_rx_q *rx_queue[FEC_ENET_MAX_RX_QS];

	unsigned int total_tx_ring_size;
	unsigned int total_rx_ring_size;

	struct	platform_device *pdev;

	int	dev_id;

	
	struct	mii_bus *mii_bus;
	uint	phy_speed;
	phy_interface_t	phy_interface;
	struct device_node *phy_node;
	bool	rgmii_txc_dly;
	bool	rgmii_rxc_dly;
	bool	rpm_active;
	int	link;
	int	full_duplex;
	int	speed;
	int	irq[FEC_IRQ_NUM];
	bool	bufdesc_ex;
	int	pause_flag;
	int	wol_flag;
	int	wake_irq;
	u32	quirks;

	struct	napi_struct napi;
	int	csum_flags;

	struct work_struct tx_timeout_work;

	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_caps;
	spinlock_t tmreg_lock;
	struct cyclecounter cc;
	struct timecounter tc;
	u32 cycle_speed;
	int hwts_rx_en;
	int hwts_tx_en;
	struct delayed_work time_keep;
	struct regulator *reg_phy;
	struct fec_stop_mode_gpr stop_gpr;
	struct pm_qos_request pm_qos_req;

	unsigned int tx_align;
	unsigned int rx_align;

	
	unsigned int rx_pkts_itr;
	unsigned int rx_time_itr;
	unsigned int tx_pkts_itr;
	unsigned int tx_time_itr;
	unsigned int itr_clk_rate;

	
	struct ethtool_eee eee;
	unsigned int clk_ref_rate;

	
	unsigned int ptp_inc;

	
	int pps_channel;
	unsigned int reload_period;
	int pps_enable;
	unsigned int next_counter;
	struct hrtimer perout_timer;
	u64 perout_stime;

	struct imx_sc_ipc *ipc_handle;

	
	struct bpf_prog *xdp_prog;

	u64 ethtool_stats[];
};

void fec_ptp_init(struct platform_device *pdev, int irq_idx);
void fec_ptp_stop(struct platform_device *pdev);
void fec_ptp_start_cyclecounter(struct net_device *ndev);
int fec_ptp_set(struct net_device *ndev, struct kernel_hwtstamp_config *config,
		struct netlink_ext_ack *extack);
void fec_ptp_get(struct net_device *ndev, struct kernel_hwtstamp_config *config);


#endif 
