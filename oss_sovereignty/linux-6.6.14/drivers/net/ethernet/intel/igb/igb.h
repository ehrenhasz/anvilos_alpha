#ifndef _IGB_H_
#define _IGB_H_
#include "e1000_mac.h"
#include "e1000_82575.h"
#include <linux/timecounter.h>
#include <linux/net_tstamp.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/bitops.h>
#include <linux/if_vlan.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/pci.h>
#include <linux/mdio.h>
#include <net/xdp.h>
struct igb_adapter;
#define E1000_PCS_CFG_IGN_SD	1
#define IGB_START_ITR		648  
#define IGB_4K_ITR		980
#define IGB_20K_ITR		196
#define IGB_70K_ITR		56
#define IGB_DEFAULT_TXD		256
#define IGB_DEFAULT_TX_WORK	128
#define IGB_MIN_TXD		64
#define IGB_MAX_TXD		4096
#define IGB_DEFAULT_RXD		256
#define IGB_MIN_RXD		64
#define IGB_MAX_RXD		4096
#define IGB_DEFAULT_ITR		3  
#define IGB_MAX_ITR_USECS	10000
#define IGB_MIN_ITR_USECS	10
#define NON_Q_VECTORS		1
#define MAX_Q_VECTORS		8
#define MAX_MSIX_ENTRIES	10
#define IGB_MAX_RX_QUEUES	8
#define IGB_MAX_RX_QUEUES_82575	4
#define IGB_MAX_RX_QUEUES_I211	2
#define IGB_MAX_TX_QUEUES	8
#define IGB_MAX_VF_MC_ENTRIES	30
#define IGB_MAX_VF_FUNCTIONS	8
#define IGB_MAX_VFTA_ENTRIES	128
#define IGB_82576_VF_DEV_ID	0x10CA
#define IGB_I350_VF_DEV_ID	0x1520
#define IGB_MAJOR_MASK		0xF000
#define IGB_MINOR_MASK		0x0FF0
#define IGB_BUILD_MASK		0x000F
#define IGB_COMB_VER_MASK	0x00FF
#define IGB_MAJOR_SHIFT		12
#define IGB_MINOR_SHIFT		4
#define IGB_COMB_VER_SHFT	8
#define IGB_NVM_VER_INVALID	0xFFFF
#define IGB_ETRACK_SHIFT	16
#define NVM_ETRACK_WORD		0x0042
#define NVM_COMB_VER_OFF	0x0083
#define NVM_COMB_VER_PTR	0x003d
#define IGB_I210_TX_LATENCY_10		9542
#define IGB_I210_TX_LATENCY_100		1024
#define IGB_I210_TX_LATENCY_1000	178
#define IGB_I210_RX_LATENCY_10		20662
#define IGB_I210_RX_LATENCY_100		2213
#define IGB_I210_RX_LATENCY_1000	448
#define IGB_XDP_PASS		0
#define IGB_XDP_CONSUMED	BIT(0)
#define IGB_XDP_TX		BIT(1)
#define IGB_XDP_REDIR		BIT(2)
struct vf_data_storage {
	unsigned char vf_mac_addresses[ETH_ALEN];
	u16 vf_mc_hashes[IGB_MAX_VF_MC_ENTRIES];
	u16 num_vf_mc_hashes;
	u32 flags;
	unsigned long last_nack;
	u16 pf_vlan;  
	u16 pf_qos;
	u16 tx_rate;
	bool spoofchk_enabled;
	bool trusted;
};
#define IGB_PF_MAC_FILTERS_RESERVED	3
struct vf_mac_filter {
	struct list_head l;
	int vf;
	bool free;
	u8 vf_mac[ETH_ALEN];
};
#define IGB_VF_FLAG_CTS            0x00000001  
#define IGB_VF_FLAG_UNI_PROMISC    0x00000002  
#define IGB_VF_FLAG_MULTI_PROMISC  0x00000004  
#define IGB_VF_FLAG_PF_SET_MAC     0x00000008  
#define IGB_RX_PTHRESH	((hw->mac.type == e1000_i354) ? 12 : 8)
#define IGB_RX_HTHRESH	8
#define IGB_TX_PTHRESH	((hw->mac.type == e1000_i354) ? 20 : 8)
#define IGB_TX_HTHRESH	1
#define IGB_RX_WTHRESH	((hw->mac.type == e1000_82576 && \
			  (adapter->flags & IGB_FLAG_HAS_MSIX)) ? 1 : 4)
#define IGB_TX_WTHRESH	((hw->mac.type == e1000_82576 && \
			  (adapter->flags & IGB_FLAG_HAS_MSIX)) ? 1 : 16)
#define MAXIMUM_ETHERNET_VLAN_SIZE 1522
#define IGB_ETH_PKT_HDR_PAD	(ETH_HLEN + ETH_FCS_LEN + (VLAN_HLEN * 2))
#define IGB_RXBUFFER_256	256
#define IGB_RXBUFFER_1536	1536
#define IGB_RXBUFFER_2048	2048
#define IGB_RXBUFFER_3072	3072
#define IGB_RX_HDR_LEN		IGB_RXBUFFER_256
#define IGB_TS_HDR_LEN		16
#if (PAGE_SIZE < 8192)
#define IGB_MAX_FRAME_BUILD_SKB (IGB_RXBUFFER_1536 - NET_IP_ALIGN)
#define IGB_2K_TOO_SMALL_WITH_PADDING \
((NET_SKB_PAD + IGB_TS_HDR_LEN + IGB_RXBUFFER_1536) > SKB_WITH_OVERHEAD(IGB_RXBUFFER_2048))
static inline int igb_compute_pad(int rx_buf_len)
{
	int page_size, pad_size;
	page_size = ALIGN(rx_buf_len, PAGE_SIZE / 2);
	pad_size = SKB_WITH_OVERHEAD(page_size) - rx_buf_len;
	return pad_size;
}
static inline int igb_skb_pad(void)
{
	int rx_buf_len;
	if (IGB_2K_TOO_SMALL_WITH_PADDING)
		rx_buf_len = IGB_RXBUFFER_3072 + SKB_DATA_ALIGN(NET_IP_ALIGN);
	else
		rx_buf_len = IGB_RXBUFFER_1536;
	rx_buf_len -= NET_IP_ALIGN;
	return igb_compute_pad(rx_buf_len);
}
#define IGB_SKB_PAD	igb_skb_pad()
#else
#define IGB_SKB_PAD	(NET_SKB_PAD + NET_IP_ALIGN)
#endif
#define IGB_RX_BUFFER_WRITE	16  
#define IGB_RX_DMA_ATTR \
	(DMA_ATTR_SKIP_CPU_SYNC | DMA_ATTR_WEAK_ORDERING)
#define AUTO_ALL_MODES		0
#define IGB_EEPROM_APME		0x0400
#ifndef IGB_MASTER_SLAVE
#define IGB_MASTER_SLAVE	e1000_ms_hw_default
#endif
#define IGB_MNG_VLAN_NONE	-1
enum igb_tx_flags {
	IGB_TX_FLAGS_VLAN	= 0x01,
	IGB_TX_FLAGS_TSO	= 0x02,
	IGB_TX_FLAGS_TSTAMP	= 0x04,
	IGB_TX_FLAGS_IPV4	= 0x10,
	IGB_TX_FLAGS_CSUM	= 0x20,
};
#define IGB_TX_FLAGS_VLAN_MASK	0xffff0000
#define IGB_TX_FLAGS_VLAN_SHIFT	16
#define IGB_MAX_TXD_PWR	15
#define IGB_MAX_DATA_PER_TXD	(1u << IGB_MAX_TXD_PWR)
#define TXD_USE_COUNT(S) DIV_ROUND_UP((S), IGB_MAX_DATA_PER_TXD)
#define DESC_NEEDED (MAX_SKB_FRAGS + 4)
#define IGB_SFF_8472_SWAP		0x5C
#define IGB_SFF_8472_COMP		0x5E
#define IGB_SFF_ADDRESSING_MODE		0x4
#define IGB_SFF_8472_UNSUP		0x00
enum igb_tx_buf_type {
	IGB_TYPE_SKB = 0,
	IGB_TYPE_XDP,
};
struct igb_tx_buffer {
	union e1000_adv_tx_desc *next_to_watch;
	unsigned long time_stamp;
	enum igb_tx_buf_type type;
	union {
		struct sk_buff *skb;
		struct xdp_frame *xdpf;
	};
	unsigned int bytecount;
	u16 gso_segs;
	__be16 protocol;
	DEFINE_DMA_UNMAP_ADDR(dma);
	DEFINE_DMA_UNMAP_LEN(len);
	u32 tx_flags;
};
struct igb_rx_buffer {
	dma_addr_t dma;
	struct page *page;
#if (BITS_PER_LONG > 32) || (PAGE_SIZE >= 65536)
	__u32 page_offset;
#else
	__u16 page_offset;
#endif
	__u16 pagecnt_bias;
};
struct igb_tx_queue_stats {
	u64 packets;
	u64 bytes;
	u64 restart_queue;
	u64 restart_queue2;
};
struct igb_rx_queue_stats {
	u64 packets;
	u64 bytes;
	u64 drops;
	u64 csum_err;
	u64 alloc_failed;
};
struct igb_ring_container {
	struct igb_ring *ring;		 
	unsigned int total_bytes;	 
	unsigned int total_packets;	 
	u16 work_limit;			 
	u8 count;			 
	u8 itr;				 
};
struct igb_ring {
	struct igb_q_vector *q_vector;	 
	struct net_device *netdev;	 
	struct bpf_prog *xdp_prog;
	struct device *dev;		 
	union {				 
		struct igb_tx_buffer *tx_buffer_info;
		struct igb_rx_buffer *rx_buffer_info;
	};
	void *desc;			 
	unsigned long flags;		 
	void __iomem *tail;		 
	dma_addr_t dma;			 
	unsigned int  size;		 
	u16 count;			 
	u8 queue_index;			 
	u8 reg_idx;			 
	bool launchtime_enable;		 
	bool cbs_enable;		 
	s32 idleslope;			 
	s32 sendslope;			 
	s32 hicredit;			 
	s32 locredit;			 
	u16 next_to_clean;
	u16 next_to_use;
	u16 next_to_alloc;
	union {
		struct {
			struct igb_tx_queue_stats tx_stats;
			struct u64_stats_sync tx_syncp;
			struct u64_stats_sync tx_syncp2;
		};
		struct {
			struct sk_buff *skb;
			struct igb_rx_queue_stats rx_stats;
			struct u64_stats_sync rx_syncp;
		};
	};
	struct xdp_rxq_info xdp_rxq;
} ____cacheline_internodealigned_in_smp;
struct igb_q_vector {
	struct igb_adapter *adapter;	 
	int cpu;			 
	u32 eims_value;			 
	u16 itr_val;
	u8 set_itr;
	void __iomem *itr_register;
	struct igb_ring_container rx, tx;
	struct napi_struct napi;
	struct rcu_head rcu;	 
	char name[IFNAMSIZ + 9];
	struct igb_ring ring[] ____cacheline_internodealigned_in_smp;
};
enum e1000_ring_flags_t {
	IGB_RING_FLAG_RX_3K_BUFFER,
	IGB_RING_FLAG_RX_BUILD_SKB_ENABLED,
	IGB_RING_FLAG_RX_SCTP_CSUM,
	IGB_RING_FLAG_RX_LB_VLAN_BSWAP,
	IGB_RING_FLAG_TX_CTX_IDX,
	IGB_RING_FLAG_TX_DETECT_HANG
};
#define ring_uses_large_buffer(ring) \
	test_bit(IGB_RING_FLAG_RX_3K_BUFFER, &(ring)->flags)
#define set_ring_uses_large_buffer(ring) \
	set_bit(IGB_RING_FLAG_RX_3K_BUFFER, &(ring)->flags)
#define clear_ring_uses_large_buffer(ring) \
	clear_bit(IGB_RING_FLAG_RX_3K_BUFFER, &(ring)->flags)
#define ring_uses_build_skb(ring) \
	test_bit(IGB_RING_FLAG_RX_BUILD_SKB_ENABLED, &(ring)->flags)
#define set_ring_build_skb_enabled(ring) \
	set_bit(IGB_RING_FLAG_RX_BUILD_SKB_ENABLED, &(ring)->flags)
#define clear_ring_build_skb_enabled(ring) \
	clear_bit(IGB_RING_FLAG_RX_BUILD_SKB_ENABLED, &(ring)->flags)
static inline unsigned int igb_rx_bufsz(struct igb_ring *ring)
{
#if (PAGE_SIZE < 8192)
	if (ring_uses_large_buffer(ring))
		return IGB_RXBUFFER_3072;
	if (ring_uses_build_skb(ring))
		return IGB_MAX_FRAME_BUILD_SKB;
#endif
	return IGB_RXBUFFER_2048;
}
static inline unsigned int igb_rx_pg_order(struct igb_ring *ring)
{
#if (PAGE_SIZE < 8192)
	if (ring_uses_large_buffer(ring))
		return 1;
#endif
	return 0;
}
#define igb_rx_pg_size(_ring) (PAGE_SIZE << igb_rx_pg_order(_ring))
#define IGB_TXD_DCMD (E1000_ADVTXD_DCMD_EOP | E1000_ADVTXD_DCMD_RS)
#define IGB_RX_DESC(R, i)	\
	(&(((union e1000_adv_rx_desc *)((R)->desc))[i]))
#define IGB_TX_DESC(R, i)	\
	(&(((union e1000_adv_tx_desc *)((R)->desc))[i]))
#define IGB_TX_CTXTDESC(R, i)	\
	(&(((struct e1000_adv_tx_context_desc *)((R)->desc))[i]))
static inline __le32 igb_test_staterr(union e1000_adv_rx_desc *rx_desc,
				      const u32 stat_err_bits)
{
	return rx_desc->wb.upper.status_error & cpu_to_le32(stat_err_bits);
}
static inline int igb_desc_unused(struct igb_ring *ring)
{
	if (ring->next_to_clean > ring->next_to_use)
		return ring->next_to_clean - ring->next_to_use - 1;
	return ring->count + ring->next_to_clean - ring->next_to_use - 1;
}
#ifdef CONFIG_IGB_HWMON
#define IGB_HWMON_TYPE_LOC	0
#define IGB_HWMON_TYPE_TEMP	1
#define IGB_HWMON_TYPE_CAUTION	2
#define IGB_HWMON_TYPE_MAX	3
struct hwmon_attr {
	struct device_attribute dev_attr;
	struct e1000_hw *hw;
	struct e1000_thermal_diode_data *sensor;
	char name[12];
	};
struct hwmon_buff {
	struct attribute_group group;
	const struct attribute_group *groups[2];
	struct attribute *attrs[E1000_MAX_SENSORS * 4 + 1];
	struct hwmon_attr hwmon_list[E1000_MAX_SENSORS * 4];
	unsigned int n_hwmon;
	};
#endif
#define MAX_ETYPE_FILTER	(4 - 1)
#define IGB_ETQF_FILTER_1588	3
#define IGB_N_EXTTS	2
#define IGB_N_PEROUT	2
#define IGB_N_SDP	4
#define IGB_RETA_SIZE	128
enum igb_filter_match_flags {
	IGB_FILTER_FLAG_ETHER_TYPE = 0x1,
	IGB_FILTER_FLAG_VLAN_TCI   = 0x2,
	IGB_FILTER_FLAG_SRC_MAC_ADDR   = 0x4,
	IGB_FILTER_FLAG_DST_MAC_ADDR   = 0x8,
};
#define IGB_MAX_RXNFC_FILTERS 16
struct igb_nfc_input {
	u8 match_flags;
	__be16 etype;
	__be16 vlan_tci;
	u8 src_addr[ETH_ALEN];
	u8 dst_addr[ETH_ALEN];
};
struct igb_nfc_filter {
	struct hlist_node nfc_node;
	struct igb_nfc_input filter;
	unsigned long cookie;
	u16 etype_reg_index;
	u16 sw_idx;
	u16 action;
};
struct igb_mac_addr {
	u8 addr[ETH_ALEN];
	u8 queue;
	u8 state;  
};
#define IGB_MAC_STATE_DEFAULT	0x1
#define IGB_MAC_STATE_IN_USE	0x2
#define IGB_MAC_STATE_SRC_ADDR	0x4
#define IGB_MAC_STATE_QUEUE_STEERING 0x8
struct igb_adapter {
	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];
	struct net_device *netdev;
	struct bpf_prog *xdp_prog;
	unsigned long state;
	unsigned int flags;
	unsigned int num_q_vectors;
	struct msix_entry msix_entries[MAX_MSIX_ENTRIES];
	u32 rx_itr_setting;
	u32 tx_itr_setting;
	u16 tx_itr;
	u16 rx_itr;
	u16 tx_work_limit;
	u32 tx_timeout_count;
	int num_tx_queues;
	struct igb_ring *tx_ring[16];
	int num_rx_queues;
	struct igb_ring *rx_ring[16];
	u32 max_frame_size;
	u32 min_frame_size;
	struct timer_list watchdog_timer;
	struct timer_list phy_info_timer;
	u16 mng_vlan_id;
	u32 bd_number;
	u32 wol;
	u32 en_mng_pt;
	u16 link_speed;
	u16 link_duplex;
	u8 __iomem *io_addr;  
	struct work_struct reset_task;
	struct work_struct watchdog_task;
	bool fc_autoneg;
	u8  tx_timeout_factor;
	struct timer_list blink_timer;
	unsigned long led_status;
	struct pci_dev *pdev;
	spinlock_t stats64_lock;
	struct rtnl_link_stats64 stats64;
	struct e1000_hw hw;
	struct e1000_hw_stats stats;
	struct e1000_phy_info phy_info;
	u32 test_icr;
	struct igb_ring test_tx_ring;
	struct igb_ring test_rx_ring;
	int msg_enable;
	struct igb_q_vector *q_vector[MAX_Q_VECTORS];
	u32 eims_enable_mask;
	u32 eims_other;
	u16 tx_ring_count;
	u16 rx_ring_count;
	unsigned int vfs_allocated_count;
	struct vf_data_storage *vf_data;
	int vf_rate_link_speed;
	u32 rss_queues;
	u32 wvbr;
	u32 *shadow_vfta;
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_caps;
	struct delayed_work ptp_overflow_work;
	struct work_struct ptp_tx_work;
	struct sk_buff *ptp_tx_skb;
	struct hwtstamp_config tstamp_config;
	unsigned long ptp_tx_start;
	unsigned long last_rx_ptp_check;
	unsigned long last_rx_timestamp;
	unsigned int ptp_flags;
	spinlock_t tmreg_lock;
	struct cyclecounter cc;
	struct timecounter tc;
	u32 tx_hwtstamp_timeouts;
	u32 tx_hwtstamp_skipped;
	u32 rx_hwtstamp_cleared;
	bool pps_sys_wrap_on;
	struct ptp_pin_desc sdp_config[IGB_N_SDP];
	struct {
		struct timespec64 start;
		struct timespec64 period;
	} perout[IGB_N_PEROUT];
	char fw_version[32];
#ifdef CONFIG_IGB_HWMON
	struct hwmon_buff *igb_hwmon_buff;
	bool ets;
#endif
	struct i2c_algo_bit_data i2c_algo;
	struct i2c_adapter i2c_adap;
	struct i2c_client *i2c_client;
	u32 rss_indir_tbl_init;
	u8 rss_indir_tbl[IGB_RETA_SIZE];
	unsigned long link_check_timeout;
	int copper_tries;
	struct e1000_info ei;
	u16 eee_advert;
	struct hlist_head nfc_filter_list;
	struct hlist_head cls_flower_list;
	unsigned int nfc_filter_count;
	spinlock_t nfc_lock;
	bool etype_bitmap[MAX_ETYPE_FILTER];
	struct igb_mac_addr *mac_table;
	struct vf_mac_filter vf_macs;
	struct vf_mac_filter *vf_mac_list;
	spinlock_t vfs_lock;
};
#define IGB_PTP_ENABLED		BIT(0)
#define IGB_PTP_OVERFLOW_CHECK	BIT(1)
#define IGB_FLAG_HAS_MSI		BIT(0)
#define IGB_FLAG_DCA_ENABLED		BIT(1)
#define IGB_FLAG_QUAD_PORT_A		BIT(2)
#define IGB_FLAG_QUEUE_PAIRS		BIT(3)
#define IGB_FLAG_DMAC			BIT(4)
#define IGB_FLAG_RSS_FIELD_IPV4_UDP	BIT(6)
#define IGB_FLAG_RSS_FIELD_IPV6_UDP	BIT(7)
#define IGB_FLAG_WOL_SUPPORTED		BIT(8)
#define IGB_FLAG_NEED_LINK_UPDATE	BIT(9)
#define IGB_FLAG_MEDIA_RESET		BIT(10)
#define IGB_FLAG_MAS_CAPABLE		BIT(11)
#define IGB_FLAG_MAS_ENABLE		BIT(12)
#define IGB_FLAG_HAS_MSIX		BIT(13)
#define IGB_FLAG_EEE			BIT(14)
#define IGB_FLAG_VLAN_PROMISC		BIT(15)
#define IGB_FLAG_RX_LEGACY		BIT(16)
#define IGB_FLAG_FQTSS			BIT(17)
#define IGB_MAS_ENABLE_0		0X0001
#define IGB_MAS_ENABLE_1		0X0002
#define IGB_MAS_ENABLE_2		0X0004
#define IGB_MAS_ENABLE_3		0X0008
#define IGB_MIN_TXPBSIZE	20408
#define IGB_TX_BUF_4096		4096
#define IGB_DMCTLX_DCFLUSH_DIS	0x80000000   
#define IGB_82576_TSYNC_SHIFT	19
enum e1000_state_t {
	__IGB_TESTING,
	__IGB_RESETTING,
	__IGB_DOWN,
	__IGB_PTP_TX_IN_PROGRESS,
};
enum igb_boards {
	board_82575,
};
extern char igb_driver_name[];
int igb_xmit_xdp_ring(struct igb_adapter *adapter,
		      struct igb_ring *ring,
		      struct xdp_frame *xdpf);
int igb_open(struct net_device *netdev);
int igb_close(struct net_device *netdev);
int igb_up(struct igb_adapter *);
void igb_down(struct igb_adapter *);
void igb_reinit_locked(struct igb_adapter *);
void igb_reset(struct igb_adapter *);
int igb_reinit_queues(struct igb_adapter *);
void igb_write_rss_indir_tbl(struct igb_adapter *);
int igb_set_spd_dplx(struct igb_adapter *, u32, u8);
int igb_setup_tx_resources(struct igb_ring *);
int igb_setup_rx_resources(struct igb_ring *);
void igb_free_tx_resources(struct igb_ring *);
void igb_free_rx_resources(struct igb_ring *);
void igb_configure_tx_ring(struct igb_adapter *, struct igb_ring *);
void igb_configure_rx_ring(struct igb_adapter *, struct igb_ring *);
void igb_setup_tctl(struct igb_adapter *);
void igb_setup_rctl(struct igb_adapter *);
void igb_setup_srrctl(struct igb_adapter *, struct igb_ring *);
netdev_tx_t igb_xmit_frame_ring(struct sk_buff *, struct igb_ring *);
void igb_alloc_rx_buffers(struct igb_ring *, u16);
void igb_update_stats(struct igb_adapter *);
bool igb_has_link(struct igb_adapter *adapter);
void igb_set_ethtool_ops(struct net_device *);
void igb_power_up_link(struct igb_adapter *);
void igb_set_fw_version(struct igb_adapter *);
void igb_ptp_init(struct igb_adapter *adapter);
void igb_ptp_stop(struct igb_adapter *adapter);
void igb_ptp_reset(struct igb_adapter *adapter);
void igb_ptp_suspend(struct igb_adapter *adapter);
void igb_ptp_rx_hang(struct igb_adapter *adapter);
void igb_ptp_tx_hang(struct igb_adapter *adapter);
void igb_ptp_rx_rgtstamp(struct igb_q_vector *q_vector, struct sk_buff *skb);
int igb_ptp_rx_pktstamp(struct igb_q_vector *q_vector, void *va,
			ktime_t *timestamp);
int igb_ptp_set_ts_config(struct net_device *netdev, struct ifreq *ifr);
int igb_ptp_get_ts_config(struct net_device *netdev, struct ifreq *ifr);
void igb_set_flag_queue_pairs(struct igb_adapter *, const u32);
unsigned int igb_get_max_rss_queues(struct igb_adapter *);
#ifdef CONFIG_IGB_HWMON
void igb_sysfs_exit(struct igb_adapter *adapter);
int igb_sysfs_init(struct igb_adapter *adapter);
#endif
static inline s32 igb_reset_phy(struct e1000_hw *hw)
{
	if (hw->phy.ops.reset)
		return hw->phy.ops.reset(hw);
	return 0;
}
static inline s32 igb_read_phy_reg(struct e1000_hw *hw, u32 offset, u16 *data)
{
	if (hw->phy.ops.read_reg)
		return hw->phy.ops.read_reg(hw, offset, data);
	return 0;
}
static inline s32 igb_write_phy_reg(struct e1000_hw *hw, u32 offset, u16 data)
{
	if (hw->phy.ops.write_reg)
		return hw->phy.ops.write_reg(hw, offset, data);
	return 0;
}
static inline s32 igb_get_phy_info(struct e1000_hw *hw)
{
	if (hw->phy.ops.get_phy_info)
		return hw->phy.ops.get_phy_info(hw);
	return 0;
}
static inline struct netdev_queue *txring_txq(const struct igb_ring *tx_ring)
{
	return netdev_get_tx_queue(tx_ring->netdev, tx_ring->queue_index);
}
int igb_add_filter(struct igb_adapter *adapter,
		   struct igb_nfc_filter *input);
int igb_erase_filter(struct igb_adapter *adapter,
		     struct igb_nfc_filter *input);
int igb_add_mac_steering_filter(struct igb_adapter *adapter,
				const u8 *addr, u8 queue, u8 flags);
int igb_del_mac_steering_filter(struct igb_adapter *adapter,
				const u8 *addr, u8 queue, u8 flags);
#endif  
