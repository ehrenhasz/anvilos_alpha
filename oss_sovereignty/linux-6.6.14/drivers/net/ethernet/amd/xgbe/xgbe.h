 

#ifndef __XGBE_H__
#define __XGBE_H__

#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include <linux/phy.h>
#include <linux/if_vlan.h>
#include <linux/bitops.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/timecounter.h>
#include <linux/net_tstamp.h>
#include <net/dcbnl.h>
#include <linux/completion.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <linux/dcache.h>
#include <linux/ethtool.h>
#include <linux/list.h>

#define XGBE_DRV_NAME		"amd-xgbe"
#define XGBE_DRV_DESC		"AMD 10 Gigabit Ethernet Driver"

 
#define XGBE_TX_DESC_CNT	512
#define XGBE_TX_DESC_MIN_FREE	(XGBE_TX_DESC_CNT >> 3)
#define XGBE_TX_DESC_MAX_PROC	(XGBE_TX_DESC_CNT >> 1)
#define XGBE_RX_DESC_CNT	512

#define XGBE_TX_DESC_CNT_MIN	64
#define XGBE_TX_DESC_CNT_MAX	4096
#define XGBE_RX_DESC_CNT_MIN	64
#define XGBE_RX_DESC_CNT_MAX	4096

#define XGBE_TX_MAX_BUF_SIZE	(0x3fff & ~(64 - 1))

 
#define XGBE_TX_MAX_SPLIT	\
	((GSO_LEGACY_MAX_SIZE / XGBE_TX_MAX_BUF_SIZE) + 1)

 
#define XGBE_TX_MAX_DESCS	(MAX_SKB_FRAGS + XGBE_TX_MAX_SPLIT + 2)

#define XGBE_RX_MIN_BUF_SIZE	(ETH_FRAME_LEN + ETH_FCS_LEN + VLAN_HLEN)
#define XGBE_RX_BUF_ALIGN	64
#define XGBE_SKB_ALLOC_SIZE	256
#define XGBE_SPH_HDSMS_SIZE	2	 

#define XGBE_MAX_DMA_CHANNELS	16
#define XGBE_MAX_QUEUES		16
#define XGBE_PRIORITY_QUEUES	8
#define XGBE_DMA_STOP_TIMEOUT	1

 
#define XGBE_DMA_OS_ARCR	0x002b2b2b
#define XGBE_DMA_OS_AWCR	0x2f2f2f2f

 
#define XGBE_DMA_SYS_ARCR	0x00303030
#define XGBE_DMA_SYS_AWCR	0x30303030

 
#define XGBE_DMA_PCI_ARCR	0x000f0f0f
#define XGBE_DMA_PCI_AWCR	0x0f0f0f0f
#define XGBE_DMA_PCI_AWARCR	0x00000f0f

 
#define XGBE_IRQ_MODE_EDGE	0
#define XGBE_IRQ_MODE_LEVEL	1

#define XGMAC_MIN_PACKET	60
#define XGMAC_STD_PACKET_MTU	1500
#define XGMAC_MAX_STD_PACKET	1518
#define XGMAC_JUMBO_PACKET_MTU	9000
#define XGMAC_MAX_JUMBO_PACKET	9018
#define XGMAC_ETH_PREAMBLE	(12 + 8)	 

#define XGMAC_PFC_DATA_LEN	46
#define XGMAC_PFC_DELAYS	14000

#define XGMAC_PRIO_QUEUES(_cnt)					\
	min_t(unsigned int, IEEE_8021QAZ_MAX_TCS, (_cnt))

 
#define XGBE_MAC_ADDR_PROPERTY	"mac-address"
#define XGBE_PHY_MODE_PROPERTY	"phy-mode"
#define XGBE_DMA_IRQS_PROPERTY	"amd,per-channel-interrupt"
#define XGBE_SPEEDSET_PROPERTY	"amd,speed-set"

 
#define XGBE_DMA_CLOCK		"dma_clk"
#define XGBE_PTP_CLOCK		"ptp_clk"

 
#define XGBE_ACPI_DMA_FREQ	"amd,dma-freq"
#define XGBE_ACPI_PTP_FREQ	"amd,ptp-freq"

 
#define XGBE_XGMAC_BAR		0
#define XGBE_XPCS_BAR		1
#define XGBE_MAC_PROP_OFFSET	0x1d000
#define XGBE_I2C_CTRL_OFFSET	0x1e000

 
#define XGBE_MSI_BASE_COUNT	4
#define XGBE_MSI_MIN_COUNT	(XGBE_MSI_BASE_COUNT + 1)

 
#define XGBE_V2_DMA_CLOCK_FREQ	500000000	 
#define XGBE_V2_PTP_CLOCK_FREQ	125000000	 

 
#define XGBE_TSTAMP_SSINC	20
#define XGBE_TSTAMP_SNSINC	0

 
#define XGMAC_DRIVER_CONTEXT	1
#define XGMAC_IOCTL_CONTEXT	2

#define XGMAC_FIFO_MIN_ALLOC	2048
#define XGMAC_FIFO_UNIT		256
#define XGMAC_FIFO_ALIGN(_x)				\
	(((_x) + XGMAC_FIFO_UNIT - 1) & ~(XGMAC_FIFO_UNIT - 1))
#define XGMAC_FIFO_FC_OFF	2048
#define XGMAC_FIFO_FC_MIN	4096

#define XGBE_TC_MIN_QUANTUM	10

 
#define XGBE_GET_DESC_DATA(_ring, _idx)				\
	((_ring)->rdata +					\
	 ((_idx) & ((_ring)->rdesc_count - 1)))

 
#define XGMAC_INIT_DMA_TX_USECS		1000
#define XGMAC_INIT_DMA_TX_FRAMES	25

#define XGMAC_MAX_DMA_RIWT		0xff
#define XGMAC_INIT_DMA_RX_USECS		30
#define XGMAC_INIT_DMA_RX_FRAMES	25

 
#define XGMAC_MAX_FLOW_CONTROL_QUEUES	8

 
#define XGMAC_FLOW_CONTROL_UNIT		512
#define XGMAC_FLOW_CONTROL_ALIGN(_x)				\
	(((_x) + XGMAC_FLOW_CONTROL_UNIT - 1) & ~(XGMAC_FLOW_CONTROL_UNIT - 1))
#define XGMAC_FLOW_CONTROL_VALUE(_x)				\
	(((_x) < 1024) ? 0 : ((_x) / XGMAC_FLOW_CONTROL_UNIT) - 2)
#define XGMAC_FLOW_CONTROL_MAX		33280

 
#define XGBE_MAC_HASH_TABLE_SIZE	8

 
#define XGBE_RSS_HASH_KEY_SIZE		40
#define XGBE_RSS_MAX_TABLE_SIZE		256
#define XGBE_RSS_LOOKUP_TABLE_TYPE	0
#define XGBE_RSS_HASH_KEY_TYPE		1

 
#define XGBE_AN_MS_TIMEOUT		500
#define XGBE_LINK_TIMEOUT		5
#define XGBE_KR_TRAINING_WAIT_ITER	50

#define XGBE_SGMII_AN_LINK_STATUS	BIT(1)
#define XGBE_SGMII_AN_LINK_SPEED	(BIT(2) | BIT(3))
#define XGBE_SGMII_AN_LINK_SPEED_10	0x00
#define XGBE_SGMII_AN_LINK_SPEED_100	0x04
#define XGBE_SGMII_AN_LINK_SPEED_1000	0x08
#define XGBE_SGMII_AN_LINK_DUPLEX	BIT(4)

 
#define XGBE_ECC_LIMIT			60

 
#define XGMAC_MAX_C22_PORT		3

 
#define XGBE_ZERO_SUP(_ls)		\
	ethtool_link_ksettings_zero_link_mode((_ls), supported)

#define XGBE_SET_SUP(_ls, _mode)	\
	ethtool_link_ksettings_add_link_mode((_ls), supported, _mode)

#define XGBE_CLR_SUP(_ls, _mode)	\
	ethtool_link_ksettings_del_link_mode((_ls), supported, _mode)

#define XGBE_IS_SUP(_ls, _mode)	\
	ethtool_link_ksettings_test_link_mode((_ls), supported, _mode)

#define XGBE_ZERO_ADV(_ls)		\
	ethtool_link_ksettings_zero_link_mode((_ls), advertising)

#define XGBE_SET_ADV(_ls, _mode)	\
	ethtool_link_ksettings_add_link_mode((_ls), advertising, _mode)

#define XGBE_CLR_ADV(_ls, _mode)	\
	ethtool_link_ksettings_del_link_mode((_ls), advertising, _mode)

#define XGBE_ADV(_ls, _mode)		\
	ethtool_link_ksettings_test_link_mode((_ls), advertising, _mode)

#define XGBE_ZERO_LP_ADV(_ls)		\
	ethtool_link_ksettings_zero_link_mode((_ls), lp_advertising)

#define XGBE_SET_LP_ADV(_ls, _mode)	\
	ethtool_link_ksettings_add_link_mode((_ls), lp_advertising, _mode)

#define XGBE_CLR_LP_ADV(_ls, _mode)	\
	ethtool_link_ksettings_del_link_mode((_ls), lp_advertising, _mode)

#define XGBE_LP_ADV(_ls, _mode)		\
	ethtool_link_ksettings_test_link_mode((_ls), lp_advertising, _mode)

#define XGBE_LM_COPY(_dst, _dname, _src, _sname)	\
	bitmap_copy((_dst)->link_modes._dname,		\
		    (_src)->link_modes._sname,		\
		    __ETHTOOL_LINK_MODE_MASK_NBITS)

struct xgbe_prv_data;

struct xgbe_packet_data {
	struct sk_buff *skb;

	unsigned int attributes;

	unsigned int errors;

	unsigned int rdesc_count;
	unsigned int length;

	unsigned int header_len;
	unsigned int tcp_header_len;
	unsigned int tcp_payload_len;
	unsigned short mss;

	unsigned short vlan_ctag;

	u64 rx_tstamp;

	u32 rss_hash;
	enum pkt_hash_types rss_hash_type;

	unsigned int tx_packets;
	unsigned int tx_bytes;
};

 
struct xgbe_ring_desc {
	__le32 desc0;
	__le32 desc1;
	__le32 desc2;
	__le32 desc3;
};

 
struct xgbe_page_alloc {
	struct page *pages;
	unsigned int pages_len;
	unsigned int pages_offset;

	dma_addr_t pages_dma;
};

 
struct xgbe_buffer_data {
	struct xgbe_page_alloc pa;
	struct xgbe_page_alloc pa_unmap;

	dma_addr_t dma_base;
	unsigned long dma_off;
	unsigned int dma_len;
};

 
struct xgbe_tx_ring_data {
	unsigned int packets;		 
	unsigned int bytes;		 
};

 
struct xgbe_rx_ring_data {
	struct xgbe_buffer_data hdr;	 
	struct xgbe_buffer_data buf;	 

	unsigned short hdr_len;		 
	unsigned short len;		 
};

 
struct xgbe_ring_data {
	struct xgbe_ring_desc *rdesc;	 
	dma_addr_t rdesc_dma;		 

	struct sk_buff *skb;		 
	dma_addr_t skb_dma;		 
	unsigned int skb_dma_len;	 

	struct xgbe_tx_ring_data tx;	 
	struct xgbe_rx_ring_data rx;	 

	unsigned int mapped_as_page;

	 
	unsigned int state_saved;
	struct {
		struct sk_buff *skb;
		unsigned int len;
		unsigned int error;
	} state;
};

struct xgbe_ring {
	 
	spinlock_t lock;

	 
	struct xgbe_packet_data packet_data;

	 
	struct xgbe_ring_desc *rdesc;
	dma_addr_t rdesc_dma;
	unsigned int rdesc_count;

	 
	struct xgbe_ring_data *rdata;

	 
	struct xgbe_page_alloc rx_hdr_pa;
	struct xgbe_page_alloc rx_buf_pa;
	int node;

	 
	unsigned int cur;
	unsigned int dirty;

	 
	unsigned int coalesce_count;

	union {
		struct {
			unsigned int queue_stopped;
			unsigned int xmit_more;
			unsigned short cur_mss;
			unsigned short cur_vlan_ctag;
		} tx;
	};
} ____cacheline_aligned;

 
struct xgbe_channel {
	char name[16];

	 
	struct xgbe_prv_data *pdata;

	 
	unsigned int queue_index;
	void __iomem *dma_regs;

	 
	int dma_irq;
	char dma_irq_name[IFNAMSIZ + 32];

	 
	struct napi_struct napi;

	 
	unsigned int curr_ier;
	unsigned int saved_ier;

	unsigned int tx_timer_active;
	struct timer_list tx_timer;

	struct xgbe_ring *tx_ring;
	struct xgbe_ring *rx_ring;

	int node;
	cpumask_t affinity_mask;
} ____cacheline_aligned;

enum xgbe_state {
	XGBE_DOWN,
	XGBE_LINK_INIT,
	XGBE_LINK_ERR,
	XGBE_STOPPED,
};

enum xgbe_int {
	XGMAC_INT_DMA_CH_SR_TI,
	XGMAC_INT_DMA_CH_SR_TPS,
	XGMAC_INT_DMA_CH_SR_TBU,
	XGMAC_INT_DMA_CH_SR_RI,
	XGMAC_INT_DMA_CH_SR_RBU,
	XGMAC_INT_DMA_CH_SR_RPS,
	XGMAC_INT_DMA_CH_SR_TI_RI,
	XGMAC_INT_DMA_CH_SR_FBE,
	XGMAC_INT_DMA_ALL,
};

enum xgbe_int_state {
	XGMAC_INT_STATE_SAVE,
	XGMAC_INT_STATE_RESTORE,
};

enum xgbe_ecc_sec {
	XGBE_ECC_SEC_TX,
	XGBE_ECC_SEC_RX,
	XGBE_ECC_SEC_DESC,
};

enum xgbe_speed {
	XGBE_SPEED_1000 = 0,
	XGBE_SPEED_2500,
	XGBE_SPEED_10000,
	XGBE_SPEEDS,
};

enum xgbe_xpcs_access {
	XGBE_XPCS_ACCESS_V1 = 0,
	XGBE_XPCS_ACCESS_V2,
};

enum xgbe_an_mode {
	XGBE_AN_MODE_CL73 = 0,
	XGBE_AN_MODE_CL73_REDRV,
	XGBE_AN_MODE_CL37,
	XGBE_AN_MODE_CL37_SGMII,
	XGBE_AN_MODE_NONE,
};

enum xgbe_an {
	XGBE_AN_READY = 0,
	XGBE_AN_PAGE_RECEIVED,
	XGBE_AN_INCOMPAT_LINK,
	XGBE_AN_COMPLETE,
	XGBE_AN_NO_LINK,
	XGBE_AN_ERROR,
};

enum xgbe_rx {
	XGBE_RX_BPA = 0,
	XGBE_RX_XNP,
	XGBE_RX_COMPLETE,
	XGBE_RX_ERROR,
};

enum xgbe_mode {
	XGBE_MODE_KX_1000 = 0,
	XGBE_MODE_KX_2500,
	XGBE_MODE_KR,
	XGBE_MODE_X,
	XGBE_MODE_SGMII_10,
	XGBE_MODE_SGMII_100,
	XGBE_MODE_SGMII_1000,
	XGBE_MODE_SFI,
	XGBE_MODE_UNKNOWN,
};

enum xgbe_speedset {
	XGBE_SPEEDSET_1000_10000 = 0,
	XGBE_SPEEDSET_2500_10000,
};

enum xgbe_mdio_mode {
	XGBE_MDIO_MODE_NONE = 0,
	XGBE_MDIO_MODE_CL22,
	XGBE_MDIO_MODE_CL45,
};

enum xgbe_mb_cmd {
	XGBE_MB_CMD_POWER_OFF = 0,
	XGBE_MB_CMD_SET_1G,
	XGBE_MB_CMD_SET_2_5G,
	XGBE_MB_CMD_SET_10G_SFI,
	XGBE_MB_CMD_SET_10G_KR,
	XGBE_MB_CMD_RRC
};

enum xgbe_mb_subcmd {
	XGBE_MB_SUBCMD_NONE = 0,
	XGBE_MB_SUBCMD_RX_ADAP,

	 
	XGBE_MB_SUBCMD_ACTIVE = 0,
	XGBE_MB_SUBCMD_PASSIVE_1M,
	XGBE_MB_SUBCMD_PASSIVE_3M,
	XGBE_MB_SUBCMD_PASSIVE_OTHER,

	 
	XGBE_MB_SUBCMD_10MBITS = 0,
	XGBE_MB_SUBCMD_100MBITS,
	XGBE_MB_SUBCMD_1G_SGMII,
	XGBE_MB_SUBCMD_1G_KX
};

struct xgbe_phy {
	struct ethtool_link_ksettings lks;

	int address;

	int autoneg;
	int speed;
	int duplex;

	int link;

	int pause_autoneg;
	int tx_pause;
	int rx_pause;
};

enum xgbe_i2c_cmd {
	XGBE_I2C_CMD_READ = 0,
	XGBE_I2C_CMD_WRITE,
};

struct xgbe_i2c_op {
	enum xgbe_i2c_cmd cmd;

	unsigned int target;

	void *buf;
	unsigned int len;
};

struct xgbe_i2c_op_state {
	struct xgbe_i2c_op *op;

	unsigned int tx_len;
	unsigned char *tx_buf;

	unsigned int rx_len;
	unsigned char *rx_buf;

	unsigned int tx_abort_source;

	int ret;
};

struct xgbe_i2c {
	unsigned int started;
	unsigned int max_speed_mode;
	unsigned int rx_fifo_size;
	unsigned int tx_fifo_size;

	struct xgbe_i2c_op_state op_state;
};

struct xgbe_mmc_stats {
	 
	u64 txoctetcount_gb;
	u64 txframecount_gb;
	u64 txbroadcastframes_g;
	u64 txmulticastframes_g;
	u64 tx64octets_gb;
	u64 tx65to127octets_gb;
	u64 tx128to255octets_gb;
	u64 tx256to511octets_gb;
	u64 tx512to1023octets_gb;
	u64 tx1024tomaxoctets_gb;
	u64 txunicastframes_gb;
	u64 txmulticastframes_gb;
	u64 txbroadcastframes_gb;
	u64 txunderflowerror;
	u64 txoctetcount_g;
	u64 txframecount_g;
	u64 txpauseframes;
	u64 txvlanframes_g;

	 
	u64 rxframecount_gb;
	u64 rxoctetcount_gb;
	u64 rxoctetcount_g;
	u64 rxbroadcastframes_g;
	u64 rxmulticastframes_g;
	u64 rxcrcerror;
	u64 rxrunterror;
	u64 rxjabbererror;
	u64 rxundersize_g;
	u64 rxoversize_g;
	u64 rx64octets_gb;
	u64 rx65to127octets_gb;
	u64 rx128to255octets_gb;
	u64 rx256to511octets_gb;
	u64 rx512to1023octets_gb;
	u64 rx1024tomaxoctets_gb;
	u64 rxunicastframes_g;
	u64 rxlengtherror;
	u64 rxoutofrangetype;
	u64 rxpauseframes;
	u64 rxfifooverflow;
	u64 rxvlanframes_gb;
	u64 rxwatchdogerror;
};

struct xgbe_ext_stats {
	u64 tx_tso_packets;
	u64 rx_split_header_packets;
	u64 rx_buffer_unavailable;

	u64 txq_packets[XGBE_MAX_DMA_CHANNELS];
	u64 txq_bytes[XGBE_MAX_DMA_CHANNELS];
	u64 rxq_packets[XGBE_MAX_DMA_CHANNELS];
	u64 rxq_bytes[XGBE_MAX_DMA_CHANNELS];

	u64 tx_vxlan_packets;
	u64 rx_vxlan_packets;
	u64 rx_csum_errors;
	u64 rx_vxlan_csum_errors;
};

struct xgbe_hw_if {
	int (*tx_complete)(struct xgbe_ring_desc *);

	int (*set_mac_address)(struct xgbe_prv_data *, const u8 *addr);
	int (*config_rx_mode)(struct xgbe_prv_data *);

	int (*enable_rx_csum)(struct xgbe_prv_data *);
	int (*disable_rx_csum)(struct xgbe_prv_data *);

	int (*enable_rx_vlan_stripping)(struct xgbe_prv_data *);
	int (*disable_rx_vlan_stripping)(struct xgbe_prv_data *);
	int (*enable_rx_vlan_filtering)(struct xgbe_prv_data *);
	int (*disable_rx_vlan_filtering)(struct xgbe_prv_data *);
	int (*update_vlan_hash_table)(struct xgbe_prv_data *);

	int (*read_mmd_regs)(struct xgbe_prv_data *, int, int);
	void (*write_mmd_regs)(struct xgbe_prv_data *, int, int, int);
	int (*set_speed)(struct xgbe_prv_data *, int);

	int (*set_ext_mii_mode)(struct xgbe_prv_data *, unsigned int,
				enum xgbe_mdio_mode);
	int (*read_ext_mii_regs_c22)(struct xgbe_prv_data *, int, int);
	int (*write_ext_mii_regs_c22)(struct xgbe_prv_data *, int, int, u16);
	int (*read_ext_mii_regs_c45)(struct xgbe_prv_data *, int, int, int);
	int (*write_ext_mii_regs_c45)(struct xgbe_prv_data *, int, int, int,
				      u16);

	int (*set_gpio)(struct xgbe_prv_data *, unsigned int);
	int (*clr_gpio)(struct xgbe_prv_data *, unsigned int);

	void (*enable_tx)(struct xgbe_prv_data *);
	void (*disable_tx)(struct xgbe_prv_data *);
	void (*enable_rx)(struct xgbe_prv_data *);
	void (*disable_rx)(struct xgbe_prv_data *);

	void (*powerup_tx)(struct xgbe_prv_data *);
	void (*powerdown_tx)(struct xgbe_prv_data *);
	void (*powerup_rx)(struct xgbe_prv_data *);
	void (*powerdown_rx)(struct xgbe_prv_data *);

	int (*init)(struct xgbe_prv_data *);
	int (*exit)(struct xgbe_prv_data *);

	int (*enable_int)(struct xgbe_channel *, enum xgbe_int);
	int (*disable_int)(struct xgbe_channel *, enum xgbe_int);
	void (*dev_xmit)(struct xgbe_channel *);
	int (*dev_read)(struct xgbe_channel *);
	void (*tx_desc_init)(struct xgbe_channel *);
	void (*rx_desc_init)(struct xgbe_channel *);
	void (*tx_desc_reset)(struct xgbe_ring_data *);
	void (*rx_desc_reset)(struct xgbe_prv_data *, struct xgbe_ring_data *,
			      unsigned int);
	int (*is_last_desc)(struct xgbe_ring_desc *);
	int (*is_context_desc)(struct xgbe_ring_desc *);
	void (*tx_start_xmit)(struct xgbe_channel *, struct xgbe_ring *);

	 
	int (*config_tx_flow_control)(struct xgbe_prv_data *);
	int (*config_rx_flow_control)(struct xgbe_prv_data *);

	 
	int (*config_rx_coalesce)(struct xgbe_prv_data *);
	int (*config_tx_coalesce)(struct xgbe_prv_data *);
	unsigned int (*usec_to_riwt)(struct xgbe_prv_data *, unsigned int);
	unsigned int (*riwt_to_usec)(struct xgbe_prv_data *, unsigned int);

	 
	int (*config_rx_threshold)(struct xgbe_prv_data *, unsigned int);
	int (*config_tx_threshold)(struct xgbe_prv_data *, unsigned int);

	 
	int (*config_rsf_mode)(struct xgbe_prv_data *, unsigned int);
	int (*config_tsf_mode)(struct xgbe_prv_data *, unsigned int);

	 
	int (*config_osp_mode)(struct xgbe_prv_data *);

	 
	void (*rx_mmc_int)(struct xgbe_prv_data *);
	void (*tx_mmc_int)(struct xgbe_prv_data *);
	void (*read_mmc_stats)(struct xgbe_prv_data *);

	 
	int (*config_tstamp)(struct xgbe_prv_data *, unsigned int);
	void (*update_tstamp_addend)(struct xgbe_prv_data *, unsigned int);
	void (*set_tstamp_time)(struct xgbe_prv_data *, unsigned int sec,
				unsigned int nsec);
	u64 (*get_tstamp_time)(struct xgbe_prv_data *);
	u64 (*get_tx_tstamp)(struct xgbe_prv_data *);

	 
	void (*config_tc)(struct xgbe_prv_data *);
	void (*config_dcb_tc)(struct xgbe_prv_data *);
	void (*config_dcb_pfc)(struct xgbe_prv_data *);

	 
	int (*enable_rss)(struct xgbe_prv_data *);
	int (*disable_rss)(struct xgbe_prv_data *);
	int (*set_rss_hash_key)(struct xgbe_prv_data *, const u8 *);
	int (*set_rss_lookup_table)(struct xgbe_prv_data *, const u32 *);

	 
	void (*disable_ecc_ded)(struct xgbe_prv_data *);
	void (*disable_ecc_sec)(struct xgbe_prv_data *, enum xgbe_ecc_sec);

	 
	void (*enable_vxlan)(struct xgbe_prv_data *);
	void (*disable_vxlan)(struct xgbe_prv_data *);
	void (*set_vxlan_id)(struct xgbe_prv_data *);
};

 
struct xgbe_phy_impl_if {
	 
	int (*init)(struct xgbe_prv_data *);
	void (*exit)(struct xgbe_prv_data *);

	 
	int (*reset)(struct xgbe_prv_data *);
	int (*start)(struct xgbe_prv_data *);
	void (*stop)(struct xgbe_prv_data *);

	 
	int (*link_status)(struct xgbe_prv_data *, int *);

	 
	bool (*valid_speed)(struct xgbe_prv_data *, int);

	 
	bool (*use_mode)(struct xgbe_prv_data *, enum xgbe_mode);
	 
	void (*set_mode)(struct xgbe_prv_data *, enum xgbe_mode);
	 
	enum xgbe_mode (*get_mode)(struct xgbe_prv_data *, int);
	 
	enum xgbe_mode (*switch_mode)(struct xgbe_prv_data *);
	 
	enum xgbe_mode (*cur_mode)(struct xgbe_prv_data *);

	 
	enum xgbe_an_mode (*an_mode)(struct xgbe_prv_data *);

	 
	int (*an_config)(struct xgbe_prv_data *);

	 
	void (*an_advertising)(struct xgbe_prv_data *,
			       struct ethtool_link_ksettings *);

	 
	enum xgbe_mode (*an_outcome)(struct xgbe_prv_data *);

	 
	void (*an_pre)(struct xgbe_prv_data *);
	void (*an_post)(struct xgbe_prv_data *);

	 
	void (*kr_training_pre)(struct xgbe_prv_data *);
	void (*kr_training_post)(struct xgbe_prv_data *);

	 
	int (*module_info)(struct xgbe_prv_data *pdata,
			   struct ethtool_modinfo *modinfo);
	int (*module_eeprom)(struct xgbe_prv_data *pdata,
			     struct ethtool_eeprom *eeprom, u8 *data);
};

struct xgbe_phy_if {
	 
	int (*phy_init)(struct xgbe_prv_data *);
	void (*phy_exit)(struct xgbe_prv_data *);

	 
	int (*phy_reset)(struct xgbe_prv_data *);
	int (*phy_start)(struct xgbe_prv_data *);
	void (*phy_stop)(struct xgbe_prv_data *);

	 
	void (*phy_status)(struct xgbe_prv_data *);
	int (*phy_config_aneg)(struct xgbe_prv_data *);

	 
	bool (*phy_valid_speed)(struct xgbe_prv_data *, int);

	 
	irqreturn_t (*an_isr)(struct xgbe_prv_data *);

	 
	int (*module_info)(struct xgbe_prv_data *pdata,
			   struct ethtool_modinfo *modinfo);
	int (*module_eeprom)(struct xgbe_prv_data *pdata,
			     struct ethtool_eeprom *eeprom, u8 *data);

	 
	struct xgbe_phy_impl_if phy_impl;
};

struct xgbe_i2c_if {
	 
	int (*i2c_init)(struct xgbe_prv_data *);

	 
	int (*i2c_start)(struct xgbe_prv_data *);
	void (*i2c_stop)(struct xgbe_prv_data *);

	 
	int (*i2c_xfer)(struct xgbe_prv_data *, struct xgbe_i2c_op *);

	 
	irqreturn_t (*i2c_isr)(struct xgbe_prv_data *);
};

struct xgbe_desc_if {
	int (*alloc_ring_resources)(struct xgbe_prv_data *);
	void (*free_ring_resources)(struct xgbe_prv_data *);
	int (*map_tx_skb)(struct xgbe_channel *, struct sk_buff *);
	int (*map_rx_buffer)(struct xgbe_prv_data *, struct xgbe_ring *,
			     struct xgbe_ring_data *);
	void (*unmap_rdata)(struct xgbe_prv_data *, struct xgbe_ring_data *);
	void (*wrapper_tx_desc_init)(struct xgbe_prv_data *);
	void (*wrapper_rx_desc_init)(struct xgbe_prv_data *);
};

 
struct xgbe_hw_features {
	 
	unsigned int version;

	 
	unsigned int gmii;		 
	unsigned int vlhash;		 
	unsigned int sma;		 
	unsigned int rwk;		 
	unsigned int mgk;		 
	unsigned int mmc;		 
	unsigned int aoe;		 
	unsigned int ts;		 
	unsigned int eee;		 
	unsigned int tx_coe;		 
	unsigned int rx_coe;		 
	unsigned int addn_mac;		 
	unsigned int ts_src;		 
	unsigned int sa_vlan_ins;	 
	unsigned int vxn;		 

	 
	unsigned int rx_fifo_size;	 
	unsigned int tx_fifo_size;	 
	unsigned int adv_ts_hi;		 
	unsigned int dma_width;		 
	unsigned int dcb;		 
	unsigned int sph;		 
	unsigned int tso;		 
	unsigned int dma_debug;		 
	unsigned int rss;		 
	unsigned int tc_cnt;		 
	unsigned int hash_table_size;	 
	unsigned int l3l4_filter_num;	 

	 
	unsigned int rx_q_cnt;		 
	unsigned int tx_q_cnt;		 
	unsigned int rx_ch_cnt;		 
	unsigned int tx_ch_cnt;		 
	unsigned int pps_out_num;	 
	unsigned int aux_snap_num;	 
};

struct xgbe_version_data {
	void (*init_function_ptrs_phy_impl)(struct xgbe_phy_if *);
	enum xgbe_xpcs_access xpcs_access;
	unsigned int mmc_64bit;
	unsigned int tx_max_fifo_size;
	unsigned int rx_max_fifo_size;
	unsigned int tx_tstamp_workaround;
	unsigned int ecc_support;
	unsigned int i2c_support;
	unsigned int irq_reissue_support;
	unsigned int tx_desc_prefetch;
	unsigned int rx_desc_prefetch;
	unsigned int an_cdr_workaround;
	unsigned int enable_rrc;
};

struct xgbe_prv_data {
	struct net_device *netdev;
	struct pci_dev *pcidev;
	struct platform_device *platdev;
	struct acpi_device *adev;
	struct device *dev;
	struct platform_device *phy_platdev;
	struct device *phy_dev;

	 
	struct xgbe_version_data *vdata;

	 
	unsigned int use_acpi;

	 
	void __iomem *xgmac_regs;	 
	void __iomem *xpcs_regs;	 
	void __iomem *rxtx_regs;	 
	void __iomem *sir0_regs;	 
	void __iomem *sir1_regs;	 
	void __iomem *xprop_regs;	 
	void __iomem *xi2c_regs;	 

	 
	unsigned int pp0;
	unsigned int pp1;
	unsigned int pp2;
	unsigned int pp3;
	unsigned int pp4;

	 
	spinlock_t lock;

	 
	spinlock_t xpcs_lock;
	unsigned int xpcs_window_def_reg;
	unsigned int xpcs_window_sel_reg;
	unsigned int xpcs_window;
	unsigned int xpcs_window_size;
	unsigned int xpcs_window_mask;

	 
	struct mutex rss_mutex;

	 
	unsigned long dev_state;

	 
	unsigned long tx_sec_period;
	unsigned long tx_ded_period;
	unsigned long rx_sec_period;
	unsigned long rx_ded_period;
	unsigned long desc_sec_period;
	unsigned long desc_ded_period;

	unsigned int tx_sec_count;
	unsigned int tx_ded_count;
	unsigned int rx_sec_count;
	unsigned int rx_ded_count;
	unsigned int desc_ded_count;
	unsigned int desc_sec_count;

	int dev_irq;
	int ecc_irq;
	int i2c_irq;
	int channel_irq[XGBE_MAX_DMA_CHANNELS];

	unsigned int per_channel_irq;
	unsigned int irq_count;
	unsigned int channel_irq_count;
	unsigned int channel_irq_mode;

	char ecc_name[IFNAMSIZ + 32];

	struct xgbe_hw_if hw_if;
	struct xgbe_phy_if phy_if;
	struct xgbe_desc_if desc_if;
	struct xgbe_i2c_if i2c_if;

	 
	unsigned int coherent;
	unsigned int arcr;
	unsigned int awcr;
	unsigned int awarcr;

	 
	struct workqueue_struct *dev_workqueue;
	struct work_struct service_work;
	struct timer_list service_timer;

	 
	struct xgbe_channel *channel[XGBE_MAX_DMA_CHANNELS];
	unsigned int tx_max_channel_count;
	unsigned int rx_max_channel_count;
	unsigned int channel_count;
	unsigned int tx_ring_count;
	unsigned int tx_desc_count;
	unsigned int rx_ring_count;
	unsigned int rx_desc_count;

	unsigned int new_tx_ring_count;
	unsigned int new_rx_ring_count;

	unsigned int tx_max_q_count;
	unsigned int rx_max_q_count;
	unsigned int tx_q_count;
	unsigned int rx_q_count;

	 
	unsigned int blen;
	unsigned int pbl;
	unsigned int aal;
	unsigned int rd_osr_limit;
	unsigned int wr_osr_limit;

	 
	unsigned int tx_sf_mode;
	unsigned int tx_threshold;
	unsigned int tx_osp_mode;
	unsigned int tx_max_fifo_size;

	 
	unsigned int rx_sf_mode;
	unsigned int rx_threshold;
	unsigned int rx_max_fifo_size;

	 
	unsigned int tx_usecs;
	unsigned int tx_frames;

	 
	unsigned int rx_riwt;
	unsigned int rx_usecs;
	unsigned int rx_frames;

	 
	unsigned int rx_buf_size;

	 
	unsigned int pause_autoneg;
	unsigned int tx_pause;
	unsigned int rx_pause;
	unsigned int rx_rfa[XGBE_MAX_QUEUES];
	unsigned int rx_rfd[XGBE_MAX_QUEUES];

	 
	u8 rss_key[XGBE_RSS_HASH_KEY_SIZE];
	u32 rss_table[XGBE_RSS_MAX_TABLE_SIZE];
	u32 rss_options;

	 
	u16 vxlan_port;

	 
	unsigned char mac_addr[ETH_ALEN];
	netdev_features_t netdev_features;
	struct napi_struct napi;
	struct xgbe_mmc_stats mmc_stats;
	struct xgbe_ext_stats ext_stats;

	 
	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];

	 
	struct clk *sysclk;
	unsigned long sysclk_rate;
	struct clk *ptpclk;
	unsigned long ptpclk_rate;

	 
	spinlock_t tstamp_lock;
	struct ptp_clock_info ptp_clock_info;
	struct ptp_clock *ptp_clock;
	struct hwtstamp_config tstamp_config;
	struct cyclecounter tstamp_cc;
	struct timecounter tstamp_tc;
	unsigned int tstamp_addend;
	struct work_struct tx_tstamp_work;
	struct sk_buff *tx_tstamp_skb;
	u64 tx_tstamp;

	 
	struct ieee_ets *ets;
	struct ieee_pfc *pfc;
	unsigned int q2tc_map[XGBE_MAX_QUEUES];
	unsigned int prio2q_map[IEEE_8021QAZ_MAX_TCS];
	unsigned int pfcq[XGBE_MAX_QUEUES];
	unsigned int pfc_rfa;
	u8 num_tcs;

	 
	struct xgbe_hw_features hw_feat;

	 
	struct work_struct restart_work;
	struct work_struct stopdev_work;

	 
	unsigned int power_down;

	 
	u32 msg_enable;

	 
	phy_interface_t phy_mode;
	int phy_link;
	int phy_speed;

	 
	unsigned int phy_started;
	void *phy_data;
	struct xgbe_phy phy;
	int mdio_mmd;
	unsigned long link_check;
	struct completion mdio_complete;

	unsigned int kr_redrv;

	char an_name[IFNAMSIZ + 32];
	struct workqueue_struct *an_workqueue;

	int an_irq;
	struct work_struct an_irq_work;

	 
	unsigned int an_int;
	unsigned int an_status;
	struct mutex an_mutex;
	enum xgbe_an an_result;
	enum xgbe_an an_state;
	enum xgbe_rx kr_state;
	enum xgbe_rx kx_state;
	struct work_struct an_work;
	unsigned int an_again;
	unsigned int an_supported;
	unsigned int parallel_detect;
	unsigned int fec_ability;
	unsigned long an_start;
	unsigned long kr_start_time;
	enum xgbe_an_mode an_mode;

	 
	struct xgbe_i2c i2c;
	struct mutex i2c_mutex;
	struct completion i2c_complete;
	char i2c_name[IFNAMSIZ + 32];

	unsigned int lpm_ctrl;		 

	unsigned int isr_as_tasklet;
	struct tasklet_struct tasklet_dev;
	struct tasklet_struct tasklet_ecc;
	struct tasklet_struct tasklet_i2c;
	struct tasklet_struct tasklet_an;

	struct dentry *xgbe_debugfs;

	unsigned int debugfs_xgmac_reg;

	unsigned int debugfs_xpcs_mmd;
	unsigned int debugfs_xpcs_reg;

	unsigned int debugfs_xprop_reg;

	unsigned int debugfs_xi2c_reg;

	bool debugfs_an_cdr_workaround;
	bool debugfs_an_cdr_track_early;
	bool en_rx_adap;
	int rx_adapt_retries;
	bool rx_adapt_done;
	bool mode_set;
};

 
struct xgbe_prv_data *xgbe_alloc_pdata(struct device *);
void xgbe_free_pdata(struct xgbe_prv_data *);
void xgbe_set_counts(struct xgbe_prv_data *);
int xgbe_config_netdev(struct xgbe_prv_data *);
void xgbe_deconfig_netdev(struct xgbe_prv_data *);

int xgbe_platform_init(void);
void xgbe_platform_exit(void);
#ifdef CONFIG_PCI
int xgbe_pci_init(void);
void xgbe_pci_exit(void);
#else
static inline int xgbe_pci_init(void) { return 0; }
static inline void xgbe_pci_exit(void) { }
#endif

void xgbe_init_function_ptrs_dev(struct xgbe_hw_if *);
void xgbe_init_function_ptrs_phy(struct xgbe_phy_if *);
void xgbe_init_function_ptrs_phy_v1(struct xgbe_phy_if *);
void xgbe_init_function_ptrs_phy_v2(struct xgbe_phy_if *);
void xgbe_init_function_ptrs_desc(struct xgbe_desc_if *);
void xgbe_init_function_ptrs_i2c(struct xgbe_i2c_if *);
const struct net_device_ops *xgbe_get_netdev_ops(void);
const struct ethtool_ops *xgbe_get_ethtool_ops(void);
const struct udp_tunnel_nic_info *xgbe_get_udp_tunnel_info(void);

#ifdef CONFIG_AMD_XGBE_DCB
const struct dcbnl_rtnl_ops *xgbe_get_dcbnl_ops(void);
#endif

void xgbe_ptp_register(struct xgbe_prv_data *);
void xgbe_ptp_unregister(struct xgbe_prv_data *);
void xgbe_dump_tx_desc(struct xgbe_prv_data *, struct xgbe_ring *,
		       unsigned int, unsigned int, unsigned int);
void xgbe_dump_rx_desc(struct xgbe_prv_data *, struct xgbe_ring *,
		       unsigned int);
void xgbe_print_pkt(struct net_device *, struct sk_buff *, bool);
void xgbe_get_all_hw_features(struct xgbe_prv_data *);
int xgbe_powerup(struct net_device *, unsigned int);
int xgbe_powerdown(struct net_device *, unsigned int);
void xgbe_init_rx_coalesce(struct xgbe_prv_data *);
void xgbe_init_tx_coalesce(struct xgbe_prv_data *);
void xgbe_restart_dev(struct xgbe_prv_data *pdata);
void xgbe_full_restart_dev(struct xgbe_prv_data *pdata);

#ifdef CONFIG_DEBUG_FS
void xgbe_debugfs_init(struct xgbe_prv_data *);
void xgbe_debugfs_exit(struct xgbe_prv_data *);
void xgbe_debugfs_rename(struct xgbe_prv_data *pdata);
#else
static inline void xgbe_debugfs_init(struct xgbe_prv_data *pdata) {}
static inline void xgbe_debugfs_exit(struct xgbe_prv_data *pdata) {}
static inline void xgbe_debugfs_rename(struct xgbe_prv_data *pdata) {}
#endif  

 
#if 0
#define YDEBUG
#define YDEBUG_MDIO
#endif

 
#ifdef YDEBUG
#define DBGPR(x...) pr_alert(x)
#else
#define DBGPR(x...) do { } while (0)
#endif

#ifdef YDEBUG_MDIO
#define DBGPR_MDIO(x...) pr_alert(x)
#else
#define DBGPR_MDIO(x...) do { } while (0)
#endif

#endif
