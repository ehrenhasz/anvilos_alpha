#ifndef _IGC_H_
#define _IGC_H_
#include <linux/kobject.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/ethtool.h>
#include <linux/sctp.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/timecounter.h>
#include <linux/net_tstamp.h>
#include <linux/bitfield.h>
#include <linux/hrtimer.h>
#include <net/xdp.h>
#include "igc_hw.h"
void igc_ethtool_set_ops(struct net_device *);
#define IGC_MAX_RX_QUEUES		4
#define IGC_MAX_TX_QUEUES		4
#define MAX_Q_VECTORS			8
#define MAX_STD_JUMBO_FRAME_SIZE	9216
#define MAX_ETYPE_FILTER		8
#define IGC_RETA_SIZE			128
#define IGC_N_EXTTS	2
#define IGC_N_PEROUT	2
#define IGC_N_SDP	4
#define MAX_FLEX_FILTER			32
#define IGC_MAX_TX_TSTAMP_REGS		4
enum igc_mac_filter_type {
	IGC_MAC_FILTER_TYPE_DST = 0,
	IGC_MAC_FILTER_TYPE_SRC
};
struct igc_tx_queue_stats {
	u64 packets;
	u64 bytes;
	u64 restart_queue;
	u64 restart_queue2;
};
struct igc_rx_queue_stats {
	u64 packets;
	u64 bytes;
	u64 drops;
	u64 csum_err;
	u64 alloc_failed;
};
struct igc_rx_packet_stats {
	u64 ipv4_packets;       
	u64 ipv4e_packets;      
	u64 ipv6_packets;       
	u64 ipv6e_packets;      
	u64 tcp_packets;        
	u64 udp_packets;        
	u64 sctp_packets;       
	u64 nfs_packets;        
	u64 other_packets;
};
struct igc_tx_timestamp_request {
	struct sk_buff *skb;    
	unsigned long start;    
	u32 mask;               
	u32 regl;               
	u32 regh;               
	u32 flags;              
};
struct igc_ring_container {
	struct igc_ring *ring;           
	unsigned int total_bytes;        
	unsigned int total_packets;      
	u16 work_limit;                  
	u8 count;                        
	u8 itr;                          
};
struct igc_ring {
	struct igc_q_vector *q_vector;   
	struct net_device *netdev;       
	struct device *dev;              
	union {                          
		struct igc_tx_buffer *tx_buffer_info;
		struct igc_rx_buffer *rx_buffer_info;
	};
	void *desc;                      
	unsigned long flags;             
	void __iomem *tail;              
	dma_addr_t dma;                  
	unsigned int size;               
	u16 count;                       
	u8 queue_index;                  
	u8 reg_idx;                      
	bool launchtime_enable;          
	ktime_t last_tx_cycle;           
	ktime_t last_ff_cycle;           
	u32 start_time;
	u32 end_time;
	u32 max_sdu;
	bool oper_gate_closed;		 
	bool admin_gate_closed;		 
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
			struct igc_tx_queue_stats tx_stats;
			struct u64_stats_sync tx_syncp;
			struct u64_stats_sync tx_syncp2;
		};
		struct {
			struct igc_rx_queue_stats rx_stats;
			struct igc_rx_packet_stats pkt_stats;
			struct u64_stats_sync rx_syncp;
			struct sk_buff *skb;
		};
	};
	struct xdp_rxq_info xdp_rxq;
	struct xsk_buff_pool *xsk_pool;
} ____cacheline_internodealigned_in_smp;
struct igc_adapter {
	struct net_device *netdev;
	struct ethtool_eee eee;
	u16 eee_advert;
	unsigned long state;
	unsigned int flags;
	unsigned int num_q_vectors;
	struct msix_entry *msix_entries;
	u16 tx_work_limit;
	u32 tx_timeout_count;
	int num_tx_queues;
	struct igc_ring *tx_ring[IGC_MAX_TX_QUEUES];
	int num_rx_queues;
	struct igc_ring *rx_ring[IGC_MAX_RX_QUEUES];
	struct timer_list watchdog_timer;
	struct timer_list dma_err_timer;
	struct timer_list phy_info_timer;
	struct hrtimer hrtimer;
	u32 wol;
	u32 en_mng_pt;
	u16 link_speed;
	u16 link_duplex;
	u8 port_num;
	u8 __iomem *io_addr;
	u32 rx_itr_setting;
	u32 tx_itr_setting;
	struct work_struct reset_task;
	struct work_struct watchdog_task;
	struct work_struct dma_err_task;
	bool fc_autoneg;
	u8 tx_timeout_factor;
	int msg_enable;
	u32 max_frame_size;
	u32 min_frame_size;
	int tc_setup_type;
	ktime_t base_time;
	ktime_t cycle_time;
	bool taprio_offload_enable;
	u32 qbv_config_change_errors;
	bool qbv_transition;
	unsigned int qbv_count;
	spinlock_t qbv_tx_lock;
	struct pci_dev *pdev;
	spinlock_t stats64_lock;
	struct rtnl_link_stats64 stats64;
	struct igc_hw hw;
	struct igc_hw_stats stats;
	struct igc_q_vector *q_vector[MAX_Q_VECTORS];
	u32 eims_enable_mask;
	u32 eims_other;
	u16 tx_ring_count;
	u16 rx_ring_count;
	u32 tx_hwtstamp_timeouts;
	u32 tx_hwtstamp_skipped;
	u32 rx_hwtstamp_cleared;
	u32 rss_queues;
	u32 rss_indir_tbl_init;
	struct mutex nfc_rule_lock;
	struct list_head nfc_rule_list;
	unsigned int nfc_rule_count;
	u8 rss_indir_tbl[IGC_RETA_SIZE];
	unsigned long link_check_timeout;
	struct igc_info ei;
	u32 test_icr;
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_caps;
	spinlock_t ptp_tx_lock;
	struct igc_tx_timestamp_request tx_tstamp[IGC_MAX_TX_TSTAMP_REGS];
	struct hwtstamp_config tstamp_config;
	unsigned int ptp_flags;
	spinlock_t tmreg_lock;
	struct cyclecounter cc;
	struct timecounter tc;
	struct timespec64 prev_ptp_time;  
	ktime_t ptp_reset_start;  
	struct system_time_snapshot snapshot;
	char fw_version[32];
	struct bpf_prog *xdp_prog;
	bool pps_sys_wrap_on;
	struct ptp_pin_desc sdp_config[IGC_N_SDP];
	struct {
		struct timespec64 start;
		struct timespec64 period;
	} perout[IGC_N_PEROUT];
};
void igc_up(struct igc_adapter *adapter);
void igc_down(struct igc_adapter *adapter);
int igc_open(struct net_device *netdev);
int igc_close(struct net_device *netdev);
int igc_setup_tx_resources(struct igc_ring *ring);
int igc_setup_rx_resources(struct igc_ring *ring);
void igc_free_tx_resources(struct igc_ring *ring);
void igc_free_rx_resources(struct igc_ring *ring);
unsigned int igc_get_max_rss_queues(struct igc_adapter *adapter);
void igc_set_flag_queue_pairs(struct igc_adapter *adapter,
			      const u32 max_rss_queues);
int igc_reinit_queues(struct igc_adapter *adapter);
void igc_write_rss_indir_tbl(struct igc_adapter *adapter);
bool igc_has_link(struct igc_adapter *adapter);
void igc_reset(struct igc_adapter *adapter);
void igc_update_stats(struct igc_adapter *adapter);
void igc_disable_rx_ring(struct igc_ring *ring);
void igc_enable_rx_ring(struct igc_ring *ring);
void igc_disable_tx_ring(struct igc_ring *ring);
void igc_enable_tx_ring(struct igc_ring *ring);
int igc_xsk_wakeup(struct net_device *dev, u32 queue_id, u32 flags);
void igc_rings_dump(struct igc_adapter *adapter);
void igc_regs_dump(struct igc_adapter *adapter);
extern char igc_driver_name[];
#define IGC_REGS_LEN			740
#define IGC_PTP_ENABLED		BIT(0)
#define IGC_FLAG_HAS_MSI		BIT(0)
#define IGC_FLAG_QUEUE_PAIRS		BIT(3)
#define IGC_FLAG_DMAC			BIT(4)
#define IGC_FLAG_PTP			BIT(8)
#define IGC_FLAG_WOL_SUPPORTED		BIT(8)
#define IGC_FLAG_NEED_LINK_UPDATE	BIT(9)
#define IGC_FLAG_HAS_MSIX		BIT(13)
#define IGC_FLAG_EEE			BIT(14)
#define IGC_FLAG_VLAN_PROMISC		BIT(15)
#define IGC_FLAG_RX_LEGACY		BIT(16)
#define IGC_FLAG_TSN_QBV_ENABLED	BIT(17)
#define IGC_FLAG_TSN_QAV_ENABLED	BIT(18)
#define IGC_FLAG_TSN_ANY_ENABLED \
	(IGC_FLAG_TSN_QBV_ENABLED | IGC_FLAG_TSN_QAV_ENABLED)
#define IGC_FLAG_RSS_FIELD_IPV4_UDP	BIT(6)
#define IGC_FLAG_RSS_FIELD_IPV6_UDP	BIT(7)
#define IGC_MRQC_ENABLE_RSS_MQ		0x00000002
#define IGC_MRQC_RSS_FIELD_IPV4_UDP	0x00400000
#define IGC_MRQC_RSS_FIELD_IPV6_UDP	0x00800000
enum igc_rss_type_num {
	IGC_RSS_TYPE_NO_HASH		= 0,
	IGC_RSS_TYPE_HASH_TCP_IPV4	= 1,
	IGC_RSS_TYPE_HASH_IPV4		= 2,
	IGC_RSS_TYPE_HASH_TCP_IPV6	= 3,
	IGC_RSS_TYPE_HASH_IPV6_EX	= 4,
	IGC_RSS_TYPE_HASH_IPV6		= 5,
	IGC_RSS_TYPE_HASH_TCP_IPV6_EX	= 6,
	IGC_RSS_TYPE_HASH_UDP_IPV4	= 7,
	IGC_RSS_TYPE_HASH_UDP_IPV6	= 8,
	IGC_RSS_TYPE_HASH_UDP_IPV6_EX	= 9,
	IGC_RSS_TYPE_MAX		= 10,
};
#define IGC_RSS_TYPE_MAX_TABLE		16
#define IGC_RSS_TYPE_MASK		GENMASK(3,0)  
static inline u32 igc_rss_type(const union igc_adv_rx_desc *rx_desc)
{
	return le32_get_bits(rx_desc->wb.lower.lo_dword.data, IGC_RSS_TYPE_MASK);
}
#define IGC_START_ITR			648  
#define IGC_4K_ITR			980
#define IGC_20K_ITR			196
#define IGC_70K_ITR			56
#define IGC_DEFAULT_ITR		3  
#define IGC_MAX_ITR_USECS	10000
#define IGC_MIN_ITR_USECS	10
#define NON_Q_VECTORS		1
#define MAX_MSIX_ENTRIES	10
#define IGC_DEFAULT_TXD		256
#define IGC_DEFAULT_TX_WORK	128
#define IGC_MIN_TXD		64
#define IGC_MAX_TXD		4096
#define IGC_DEFAULT_RXD		256
#define IGC_MIN_RXD		64
#define IGC_MAX_RXD		4096
#define IGC_RXBUFFER_256		256
#define IGC_RXBUFFER_2048		2048
#define IGC_RXBUFFER_3072		3072
#define AUTO_ALL_MODES		0
#define IGC_RX_HDR_LEN			IGC_RXBUFFER_256
#define IGC_I225_TX_LATENCY_10		240
#define IGC_I225_TX_LATENCY_100		58
#define IGC_I225_TX_LATENCY_1000	80
#define IGC_I225_TX_LATENCY_2500	1325
#define IGC_I225_RX_LATENCY_10		6450
#define IGC_I225_RX_LATENCY_100		185
#define IGC_I225_RX_LATENCY_1000	300
#define IGC_I225_RX_LATENCY_2500	1485
#define IGC_RX_PTHRESH			8
#define IGC_RX_HTHRESH			8
#define IGC_TX_PTHRESH			8
#define IGC_TX_HTHRESH			1
#define IGC_RX_WTHRESH			4
#define IGC_TX_WTHRESH			16
#define IGC_RX_DMA_ATTR \
	(DMA_ATTR_SKIP_CPU_SYNC | DMA_ATTR_WEAK_ORDERING)
#define IGC_TS_HDR_LEN			16
#define IGC_SKB_PAD			(NET_SKB_PAD + NET_IP_ALIGN)
#if (PAGE_SIZE < 8192)
#define IGC_MAX_FRAME_BUILD_SKB \
	(SKB_WITH_OVERHEAD(IGC_RXBUFFER_2048) - IGC_SKB_PAD - IGC_TS_HDR_LEN)
#else
#define IGC_MAX_FRAME_BUILD_SKB (IGC_RXBUFFER_2048 - IGC_TS_HDR_LEN)
#endif
#define IGC_RX_BUFFER_WRITE	16  
#define IGC_TX_FLAGS_VLAN_MASK	0xffff0000
#define IGC_TX_FLAGS_VLAN_SHIFT	16
static inline __le32 igc_test_staterr(union igc_adv_rx_desc *rx_desc,
				      const u32 stat_err_bits)
{
	return rx_desc->wb.upper.status_error & cpu_to_le32(stat_err_bits);
}
enum igc_state_t {
	__IGC_TESTING,
	__IGC_RESETTING,
	__IGC_DOWN,
};
enum igc_tx_flags {
	IGC_TX_FLAGS_VLAN	= 0x01,
	IGC_TX_FLAGS_TSO	= 0x02,
	IGC_TX_FLAGS_TSTAMP	= 0x04,
	IGC_TX_FLAGS_IPV4	= 0x10,
	IGC_TX_FLAGS_CSUM	= 0x20,
	IGC_TX_FLAGS_TSTAMP_1	= 0x100,
	IGC_TX_FLAGS_TSTAMP_2	= 0x200,
	IGC_TX_FLAGS_TSTAMP_3	= 0x400,
};
enum igc_boards {
	board_base,
};
#define IGC_MAX_TXD_PWR		15
#define IGC_MAX_DATA_PER_TXD	BIT(IGC_MAX_TXD_PWR)
#define TXD_USE_COUNT(S)	DIV_ROUND_UP((S), IGC_MAX_DATA_PER_TXD)
#define DESC_NEEDED	(MAX_SKB_FRAGS + 4)
enum igc_tx_buffer_type {
	IGC_TX_BUFFER_TYPE_SKB,
	IGC_TX_BUFFER_TYPE_XDP,
	IGC_TX_BUFFER_TYPE_XSK,
};
struct igc_tx_buffer {
	union igc_adv_tx_desc *next_to_watch;
	unsigned long time_stamp;
	enum igc_tx_buffer_type type;
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
struct igc_rx_buffer {
	union {
		struct {
			dma_addr_t dma;
			struct page *page;
#if (BITS_PER_LONG > 32) || (PAGE_SIZE >= 65536)
			__u32 page_offset;
#else
			__u16 page_offset;
#endif
			__u16 pagecnt_bias;
		};
		struct xdp_buff *xdp;
	};
};
struct igc_xdp_buff {
	struct xdp_buff xdp;
	union igc_adv_rx_desc *rx_desc;
	ktime_t rx_ts;  
};
struct igc_q_vector {
	struct igc_adapter *adapter;     
	void __iomem *itr_register;
	u32 eims_value;                  
	u16 itr_val;
	u8 set_itr;
	struct igc_ring_container rx, tx;
	struct napi_struct napi;
	struct rcu_head rcu;     
	char name[IFNAMSIZ + 9];
	struct net_device poll_dev;
	struct igc_ring ring[] ____cacheline_internodealigned_in_smp;
};
enum igc_filter_match_flags {
	IGC_FILTER_FLAG_ETHER_TYPE =	BIT(0),
	IGC_FILTER_FLAG_VLAN_TCI   =	BIT(1),
	IGC_FILTER_FLAG_SRC_MAC_ADDR =	BIT(2),
	IGC_FILTER_FLAG_DST_MAC_ADDR =	BIT(3),
	IGC_FILTER_FLAG_USER_DATA =	BIT(4),
	IGC_FILTER_FLAG_VLAN_ETYPE =	BIT(5),
};
struct igc_nfc_filter {
	u8 match_flags;
	u16 etype;
	__be16 vlan_etype;
	u16 vlan_tci;
	u16 vlan_tci_mask;
	u8 src_addr[ETH_ALEN];
	u8 dst_addr[ETH_ALEN];
	u8 user_data[8];
	u8 user_mask[8];
	u8 flex_index;
	u8 rx_queue;
	u8 prio;
	u8 immediate_irq;
	u8 drop;
};
struct igc_nfc_rule {
	struct list_head list;
	struct igc_nfc_filter filter;
	u32 location;
	u16 action;
	bool flex;
};
#define IGC_MAX_RXNFC_RULES		64
struct igc_flex_filter {
	u8 index;
	u8 data[128];
	u8 mask[16];
	u8 length;
	u8 rx_queue;
	u8 prio;
	u8 immediate_irq;
	u8 drop;
};
static inline u16 igc_desc_unused(const struct igc_ring *ring)
{
	u16 ntc = ring->next_to_clean;
	u16 ntu = ring->next_to_use;
	return ((ntc > ntu) ? 0 : ring->count) + ntc - ntu - 1;
}
static inline s32 igc_get_phy_info(struct igc_hw *hw)
{
	if (hw->phy.ops.get_phy_info)
		return hw->phy.ops.get_phy_info(hw);
	return 0;
}
static inline s32 igc_reset_phy(struct igc_hw *hw)
{
	if (hw->phy.ops.reset)
		return hw->phy.ops.reset(hw);
	return 0;
}
static inline struct netdev_queue *txring_txq(const struct igc_ring *tx_ring)
{
	return netdev_get_tx_queue(tx_ring->netdev, tx_ring->queue_index);
}
enum igc_ring_flags_t {
	IGC_RING_FLAG_RX_3K_BUFFER,
	IGC_RING_FLAG_RX_BUILD_SKB_ENABLED,
	IGC_RING_FLAG_RX_SCTP_CSUM,
	IGC_RING_FLAG_RX_LB_VLAN_BSWAP,
	IGC_RING_FLAG_TX_CTX_IDX,
	IGC_RING_FLAG_TX_DETECT_HANG,
	IGC_RING_FLAG_AF_XDP_ZC,
	IGC_RING_FLAG_TX_HWTSTAMP,
};
#define ring_uses_large_buffer(ring) \
	test_bit(IGC_RING_FLAG_RX_3K_BUFFER, &(ring)->flags)
#define set_ring_uses_large_buffer(ring) \
	set_bit(IGC_RING_FLAG_RX_3K_BUFFER, &(ring)->flags)
#define clear_ring_uses_large_buffer(ring) \
	clear_bit(IGC_RING_FLAG_RX_3K_BUFFER, &(ring)->flags)
#define ring_uses_build_skb(ring) \
	test_bit(IGC_RING_FLAG_RX_BUILD_SKB_ENABLED, &(ring)->flags)
static inline unsigned int igc_rx_bufsz(struct igc_ring *ring)
{
#if (PAGE_SIZE < 8192)
	if (ring_uses_large_buffer(ring))
		return IGC_RXBUFFER_3072;
	if (ring_uses_build_skb(ring))
		return IGC_MAX_FRAME_BUILD_SKB + IGC_TS_HDR_LEN;
#endif
	return IGC_RXBUFFER_2048;
}
static inline unsigned int igc_rx_pg_order(struct igc_ring *ring)
{
#if (PAGE_SIZE < 8192)
	if (ring_uses_large_buffer(ring))
		return 1;
#endif
	return 0;
}
static inline s32 igc_read_phy_reg(struct igc_hw *hw, u32 offset, u16 *data)
{
	if (hw->phy.ops.read_reg)
		return hw->phy.ops.read_reg(hw, offset, data);
	return -EOPNOTSUPP;
}
void igc_reinit_locked(struct igc_adapter *);
struct igc_nfc_rule *igc_get_nfc_rule(struct igc_adapter *adapter,
				      u32 location);
int igc_add_nfc_rule(struct igc_adapter *adapter, struct igc_nfc_rule *rule);
void igc_del_nfc_rule(struct igc_adapter *adapter, struct igc_nfc_rule *rule);
void igc_ptp_init(struct igc_adapter *adapter);
void igc_ptp_reset(struct igc_adapter *adapter);
void igc_ptp_suspend(struct igc_adapter *adapter);
void igc_ptp_stop(struct igc_adapter *adapter);
ktime_t igc_ptp_rx_pktstamp(struct igc_adapter *adapter, __le32 *buf);
int igc_ptp_set_ts_config(struct net_device *netdev, struct ifreq *ifr);
int igc_ptp_get_ts_config(struct net_device *netdev, struct ifreq *ifr);
void igc_ptp_tx_hang(struct igc_adapter *adapter);
void igc_ptp_read(struct igc_adapter *adapter, struct timespec64 *ts);
void igc_ptp_tx_tstamp_event(struct igc_adapter *adapter);
#define igc_rx_pg_size(_ring) (PAGE_SIZE << igc_rx_pg_order(_ring))
#define IGC_TXD_DCMD	(IGC_ADVTXD_DCMD_EOP | IGC_ADVTXD_DCMD_RS)
#define IGC_RX_DESC(R, i)       \
	(&(((union igc_adv_rx_desc *)((R)->desc))[i]))
#define IGC_TX_DESC(R, i)       \
	(&(((union igc_adv_tx_desc *)((R)->desc))[i]))
#define IGC_TX_CTXTDESC(R, i)   \
	(&(((struct igc_adv_tx_context_desc *)((R)->desc))[i]))
#endif  
