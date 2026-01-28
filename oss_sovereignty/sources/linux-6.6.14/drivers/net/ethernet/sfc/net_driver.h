




#ifndef EFX_NET_DRIVER_H
#define EFX_NET_DRIVER_H

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/timer.h>
#include <linux/mdio.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/vmalloc.h>
#include <linux/mtd/mtd.h>
#include <net/busy_poll.h>
#include <net/xdp.h>
#include <net/netevent.h>

#include "enum.h"
#include "bitfield.h"
#include "filter.h"



#ifdef DEBUG
#define EFX_WARN_ON_ONCE_PARANOID(x) WARN_ON_ONCE(x)
#define EFX_WARN_ON_PARANOID(x) WARN_ON(x)
#else
#define EFX_WARN_ON_ONCE_PARANOID(x) do {} while (0)
#define EFX_WARN_ON_PARANOID(x) do {} while (0)
#endif



#define EFX_MAX_CHANNELS 32U
#define EFX_MAX_RX_QUEUES EFX_MAX_CHANNELS
#define EFX_EXTRA_CHANNEL_IOV	0
#define EFX_EXTRA_CHANNEL_PTP	1
#define EFX_EXTRA_CHANNEL_TC	2
#define EFX_MAX_EXTRA_CHANNELS	3U


#define EFX_MAX_TX_TC		2
#define EFX_MAX_CORE_TX_QUEUES	(EFX_MAX_TX_TC * EFX_MAX_CHANNELS)
#define EFX_TXQ_TYPE_OUTER_CSUM	1	
#define EFX_TXQ_TYPE_INNER_CSUM	2	
#define EFX_TXQ_TYPES		4
#define EFX_MAX_TXQ_PER_CHANNEL	4
#define EFX_MAX_TX_QUEUES	(EFX_MAX_TXQ_PER_CHANNEL * EFX_MAX_CHANNELS)


#define EFX_MAX_MTU (9 * 1024)


#define EFX_MIN_MTU 68


#define EFX_TSO2_MAX_HDRLEN	208


#define EFX_RX_USR_BUF_SIZE	(2048 - 256)


#if NET_IP_ALIGN == 0
#define EFX_RX_BUF_ALIGNMENT	L1_CACHE_BYTES
#else
#define EFX_RX_BUF_ALIGNMENT	4
#endif


#define EFX_XDP_HEADROOM	128
#define EFX_XDP_TAILROOM	SKB_DATA_ALIGN(sizeof(struct skb_shared_info))


struct efx_ptp_data;
struct hwtstamp_config;

struct efx_self_tests;


struct efx_buffer {
	void *addr;
	dma_addr_t dma_addr;
	unsigned int len;
};


struct efx_tx_buffer {
	union {
		const struct sk_buff *skb;
		struct xdp_frame *xdpf;
	};
	union {
		efx_qword_t option;    
		dma_addr_t dma_addr;
	};
	unsigned short flags;
	unsigned short len;
	unsigned short unmap_len;
	unsigned short dma_offset;
};
#define EFX_TX_BUF_CONT		1	
#define EFX_TX_BUF_SKB		2	
#define EFX_TX_BUF_MAP_SINGLE	8	
#define EFX_TX_BUF_OPTION	0x10	
#define EFX_TX_BUF_XDP		0x20	
#define EFX_TX_BUF_TSO_V3	0x40	
#define EFX_TX_BUF_EFV		0x100	


struct efx_tx_queue {
	
	struct efx_nic *efx ____cacheline_aligned_in_smp;
	unsigned int queue;
	unsigned int label;
	unsigned int type;
	unsigned int tso_version;
	bool tso_encap;
	struct efx_channel *channel;
	struct netdev_queue *core_txq;
	struct efx_tx_buffer *buffer;
	struct efx_buffer *cb_page;
	struct efx_buffer txd;
	unsigned int ptr_mask;
	void __iomem *piobuf;
	unsigned int piobuf_offset;
	bool initialised;
	bool timestamping;
	bool xdp_tx;

	
	unsigned int read_count ____cacheline_aligned_in_smp;
	unsigned int old_write_count;
	unsigned int merge_events;
	unsigned int bytes_compl;
	unsigned int pkts_compl;
	u32 completed_timestamp_major;
	u32 completed_timestamp_minor;

	
	unsigned int insert_count ____cacheline_aligned_in_smp;
	unsigned int write_count;
	unsigned int packet_write_count;
	unsigned int old_read_count;
	unsigned int tso_bursts;
	unsigned int tso_long_headers;
	unsigned int tso_packets;
	unsigned int tso_fallbacks;
	unsigned int pushes;
	unsigned int pio_packets;
	bool xmit_pending;
	unsigned int cb_packets;
	unsigned int notify_count;
	
	unsigned long tx_packets;

	
	unsigned int empty_read_count ____cacheline_aligned_in_smp;
#define EFX_EMPTY_COUNT_VALID 0x80000000
	atomic_t flush_outstanding;
};

#define EFX_TX_CB_ORDER	7
#define EFX_TX_CB_SIZE	(1 << EFX_TX_CB_ORDER) - NET_IP_ALIGN


struct efx_rx_buffer {
	dma_addr_t dma_addr;
	struct page *page;
	u16 page_offset;
	u16 len;
	u16 flags;
};
#define EFX_RX_BUF_LAST_IN_PAGE	0x0001
#define EFX_RX_PKT_CSUMMED	0x0002
#define EFX_RX_PKT_DISCARD	0x0004
#define EFX_RX_PKT_TCP		0x0040
#define EFX_RX_PKT_PREFIX_LEN	0x0080	
#define EFX_RX_PKT_CSUM_LEVEL	0x0200


struct efx_rx_page_state {
	dma_addr_t dma_addr;

	unsigned int __pad[] ____cacheline_aligned;
};


struct efx_rx_queue {
	struct efx_nic *efx;
	int core_index;
	struct efx_rx_buffer *buffer;
	struct efx_buffer rxd;
	unsigned int ptr_mask;
	bool refill_enabled;
	bool flush_pending;
	bool grant_credits;

	unsigned int added_count;
	unsigned int notified_count;
	unsigned int granted_count;
	unsigned int removed_count;
	unsigned int scatter_n;
	unsigned int scatter_len;
	struct page **page_ring;
	unsigned int page_add;
	unsigned int page_remove;
	unsigned int page_recycle_count;
	unsigned int page_recycle_failed;
	unsigned int page_recycle_full;
	unsigned int page_ptr_mask;
	unsigned int max_fill;
	unsigned int fast_fill_trigger;
	unsigned int min_fill;
	unsigned int min_overfill;
	unsigned int recycle_count;
	struct timer_list slow_fill;
	unsigned int slow_fill_count;
	struct work_struct grant_work;
	
	unsigned long rx_packets;
	struct xdp_rxq_info xdp_rxq_info;
	bool xdp_rxq_info_valid;
};

enum efx_sync_events_state {
	SYNC_EVENTS_DISABLED = 0,
	SYNC_EVENTS_QUIESCENT,
	SYNC_EVENTS_REQUESTED,
	SYNC_EVENTS_VALID,
};


struct efx_channel {
	struct efx_nic *efx;
	int channel;
	const struct efx_channel_type *type;
	bool eventq_init;
	bool enabled;
	int irq;
	unsigned int irq_moderation_us;
	struct net_device *napi_dev;
	struct napi_struct napi_str;
#ifdef CONFIG_NET_RX_BUSY_POLL
	unsigned long busy_poll_state;
#endif
	struct efx_buffer eventq;
	unsigned int eventq_mask;
	unsigned int eventq_read_ptr;
	int event_test_cpu;

	unsigned int irq_count;
	unsigned int irq_mod_score;
#ifdef CONFIG_RFS_ACCEL
	unsigned int rfs_filter_count;
	unsigned int rfs_last_expiry;
	unsigned int rfs_expire_index;
	unsigned int n_rfs_succeeded;
	unsigned int n_rfs_failed;
	struct delayed_work filter_work;
#define RPS_FLOW_ID_INVALID 0xFFFFFFFF
	u32 *rps_flow_id;
#endif

	unsigned int n_rx_tobe_disc;
	unsigned int n_rx_ip_hdr_chksum_err;
	unsigned int n_rx_tcp_udp_chksum_err;
	unsigned int n_rx_outer_ip_hdr_chksum_err;
	unsigned int n_rx_outer_tcp_udp_chksum_err;
	unsigned int n_rx_inner_ip_hdr_chksum_err;
	unsigned int n_rx_inner_tcp_udp_chksum_err;
	unsigned int n_rx_eth_crc_err;
	unsigned int n_rx_mcast_mismatch;
	unsigned int n_rx_frm_trunc;
	unsigned int n_rx_overlength;
	unsigned int n_skbuff_leaks;
	unsigned int n_rx_nodesc_trunc;
	unsigned int n_rx_merge_events;
	unsigned int n_rx_merge_packets;
	unsigned int n_rx_xdp_drops;
	unsigned int n_rx_xdp_bad_drops;
	unsigned int n_rx_xdp_tx;
	unsigned int n_rx_xdp_redirect;
	unsigned int n_rx_mport_bad;

	unsigned int rx_pkt_n_frags;
	unsigned int rx_pkt_index;

	struct list_head *rx_list;

	struct efx_rx_queue rx_queue;
	struct efx_tx_queue tx_queue[EFX_MAX_TXQ_PER_CHANNEL];
	struct efx_tx_queue *tx_queue_by_type[EFX_TXQ_TYPES];

	enum efx_sync_events_state sync_events_state;
	u32 sync_timestamp_major;
	u32 sync_timestamp_minor;
};


struct efx_msi_context {
	struct efx_nic *efx;
	unsigned int index;
	char name[IFNAMSIZ + 6];
};


struct efx_channel_type {
	void (*handle_no_channel)(struct efx_nic *);
	int (*pre_probe)(struct efx_channel *);
	int (*start)(struct efx_channel *);
	void (*stop)(struct efx_channel *);
	void (*post_remove)(struct efx_channel *);
	void (*get_name)(struct efx_channel *, char *buf, size_t len);
	struct efx_channel *(*copy)(const struct efx_channel *);
	bool (*receive_skb)(struct efx_channel *, struct sk_buff *);
	bool (*receive_raw)(struct efx_rx_queue *, u32);
	bool (*want_txqs)(struct efx_channel *);
	bool keep_eventq;
	bool want_pio;
};

enum efx_led_mode {
	EFX_LED_OFF	= 0,
	EFX_LED_ON	= 1,
	EFX_LED_DEFAULT	= 2
};

#define STRING_TABLE_LOOKUP(val, member) \
	((val) < member ## _max) ? member ## _names[val] : "(invalid)"

extern const char *const efx_loopback_mode_names[];
extern const unsigned int efx_loopback_mode_max;
#define LOOPBACK_MODE(efx) \
	STRING_TABLE_LOOKUP((efx)->loopback_mode, efx_loopback_mode)

enum efx_int_mode {
	
	EFX_INT_MODE_MSIX = 0,
	EFX_INT_MODE_MSI = 1,
	EFX_INT_MODE_LEGACY = 2,
	EFX_INT_MODE_MAX	
};
#define EFX_INT_MODE_USE_MSI(x) (((x)->interrupt_mode) <= EFX_INT_MODE_MSI)

enum nic_state {
	STATE_UNINIT = 0,	
	STATE_PROBED,		
	STATE_NET_DOWN,		
	STATE_NET_UP,		
	STATE_DISABLED,		

	STATE_RECOVERY = 0x100,
	STATE_FROZEN = 0x200,	
};

static inline bool efx_net_active(enum nic_state state)
{
	return state == STATE_NET_DOWN || state == STATE_NET_UP;
}

static inline bool efx_frozen(enum nic_state state)
{
	return state & STATE_FROZEN;
}

static inline bool efx_recovering(enum nic_state state)
{
	return state & STATE_RECOVERY;
}

static inline enum nic_state efx_freeze(enum nic_state state)
{
	WARN_ON(!efx_net_active(state));
	return state | STATE_FROZEN;
}

static inline enum nic_state efx_thaw(enum nic_state state)
{
	WARN_ON(!efx_frozen(state));
	return state & ~STATE_FROZEN;
}

static inline enum nic_state efx_recover(enum nic_state state)
{
	WARN_ON(!efx_net_active(state));
	return state | STATE_RECOVERY;
}

static inline enum nic_state efx_recovered(enum nic_state state)
{
	WARN_ON(!efx_recovering(state));
	return state & ~STATE_RECOVERY;
}


struct efx_nic;


#define EFX_FC_RX	FLOW_CTRL_RX
#define EFX_FC_TX	FLOW_CTRL_TX
#define EFX_FC_AUTO	4


struct efx_link_state {
	bool up;
	bool fd;
	u8 fc;
	unsigned int speed;
};

static inline bool efx_link_state_equal(const struct efx_link_state *left,
					const struct efx_link_state *right)
{
	return left->up == right->up && left->fd == right->fd &&
		left->fc == right->fc && left->speed == right->speed;
}


enum efx_phy_mode {
	PHY_MODE_NORMAL		= 0,
	PHY_MODE_TX_DISABLED	= 1,
	PHY_MODE_LOW_POWER	= 2,
	PHY_MODE_OFF		= 4,
	PHY_MODE_SPECIAL	= 8,
};

static inline bool efx_phy_mode_disabled(enum efx_phy_mode mode)
{
	return !!(mode & ~PHY_MODE_TX_DISABLED);
}


struct efx_hw_stat_desc {
	const char *name;
	u16 dma_width;
	u16 offset;
};

struct vfdi_status;


#define EFX_MCDI_RSS_CONTEXT_INVALID	0xffffffff

struct efx_rss_context {
	struct list_head list;
	u32 context_id;
	u32 user_id;
	bool rx_hash_udp_4tuple;
	u8 rx_hash_key[40];
	u32 rx_indir_table[128];
};

#ifdef CONFIG_RFS_ACCEL

#define EFX_ARFS_FILTER_ID_PENDING	-1
#define EFX_ARFS_FILTER_ID_ERROR	-2
#define EFX_ARFS_FILTER_ID_REMOVING	-3

struct efx_arfs_rule {
	struct hlist_node node;
	struct efx_filter_spec spec;
	u16 rxq_index;
	u16 arfs_id;
	s32 filter_id;
};


#define EFX_ARFS_HASH_TABLE_SIZE	512


struct efx_async_filter_insertion {
	struct net_device *net_dev;
	struct efx_filter_spec spec;
	struct work_struct work;
	u16 rxq_index;
	u32 flow_id;
};


#define EFX_RPS_MAX_IN_FLIGHT	8
#endif 

enum efx_xdp_tx_queues_mode {
	EFX_XDP_TX_QUEUES_DEDICATED,	
	EFX_XDP_TX_QUEUES_SHARED,	
	EFX_XDP_TX_QUEUES_BORROWED	
};

struct efx_mae;


struct efx_nic {
	

	char name[IFNAMSIZ];
	struct list_head node;
	struct efx_nic *primary;
	struct list_head secondary_list;
	struct pci_dev *pci_dev;
	unsigned int port_num;
	const struct efx_nic_type *type;
	int legacy_irq;
	bool eeh_disabled_legacy_irq;
	struct workqueue_struct *workqueue;
	char workqueue_name[16];
	struct work_struct reset_work;
	resource_size_t membase_phys;
	void __iomem *membase;

	unsigned int vi_stride;

	enum efx_int_mode interrupt_mode;
	unsigned int timer_quantum_ns;
	unsigned int timer_max_ns;
	bool irq_rx_adaptive;
	bool irqs_hooked;
	unsigned int irq_mod_step_us;
	unsigned int irq_rx_moderation_us;
	u32 msg_enable;

	enum nic_state state;
	unsigned long reset_pending;

	struct efx_channel *channel[EFX_MAX_CHANNELS];
	struct efx_msi_context msi_context[EFX_MAX_CHANNELS];
	const struct efx_channel_type *
	extra_channel_type[EFX_MAX_EXTRA_CHANNELS];
	struct efx_mae *mae;

	unsigned int xdp_tx_queue_count;
	struct efx_tx_queue **xdp_tx_queues;
	enum efx_xdp_tx_queues_mode xdp_txq_queues_mode;

	unsigned rxq_entries;
	unsigned txq_entries;
	unsigned int txq_stop_thresh;
	unsigned int txq_wake_thresh;

	unsigned tx_dc_base;
	unsigned rx_dc_base;
	unsigned sram_lim_qw;

	unsigned int max_channels;
	unsigned int max_vis;
	unsigned int max_tx_channels;
	unsigned n_channels;
	unsigned n_rx_channels;
	unsigned rss_spread;
	unsigned tx_channel_offset;
	unsigned n_tx_channels;
	unsigned n_extra_tx_channels;
	unsigned int tx_queues_per_channel;
	unsigned int n_xdp_channels;
	unsigned int xdp_channel_offset;
	unsigned int xdp_tx_per_channel;
	unsigned int rx_ip_align;
	unsigned int rx_dma_len;
	unsigned int rx_buffer_order;
	unsigned int rx_buffer_truesize;
	unsigned int rx_page_buf_step;
	unsigned int rx_bufs_per_page;
	unsigned int rx_pages_per_batch;
	unsigned int rx_prefix_size;
	int rx_packet_hash_offset;
	int rx_packet_len_offset;
	int rx_packet_ts_offset;
	bool rx_scatter;
	struct efx_rss_context rss_context;
	struct mutex rss_lock;
	u32 vport_id;

	unsigned int_error_count;
	unsigned long int_error_expire;

	bool must_realloc_vis;
	bool irq_soft_enabled;
	struct efx_buffer irq_status;
	unsigned irq_zero_count;
	unsigned irq_level;
	struct delayed_work selftest_work;

#ifdef CONFIG_SFC_MTD
	struct list_head mtd_list;
#endif

	void *nic_data;
	struct efx_mcdi_data *mcdi;

	struct mutex mac_lock;
	struct work_struct mac_work;
	bool port_enabled;

	bool mc_bist_for_other_fn;
	bool port_initialized;
	struct net_device *net_dev;

	netdev_features_t fixed_features;

	u16 num_mac_stats;
	struct efx_buffer stats_buffer;
	u64 rx_nodesc_drops_total;
	u64 rx_nodesc_drops_while_down;
	bool rx_nodesc_drops_prev_state;

	unsigned int phy_type;
	void *phy_data;
	struct mdio_if_info mdio;
	unsigned int mdio_bus;
	enum efx_phy_mode phy_mode;

	__ETHTOOL_DECLARE_LINK_MODE_MASK(link_advertising);
	u32 fec_config;
	struct efx_link_state link_state;
	unsigned int n_link_state_changes;

	u8 wanted_fc;
	unsigned fc_disable;

	atomic_t rx_reset;
	enum efx_loopback_mode loopback_mode;
	u64 loopback_modes;

	void *loopback_selftest;
	
	struct bpf_prog __rcu *xdp_prog;

	struct rw_semaphore filter_sem;
	void *filter_state;
#ifdef CONFIG_RFS_ACCEL
	struct mutex rps_mutex;
	unsigned long rps_slot_map;
	struct efx_async_filter_insertion rps_slot[EFX_RPS_MAX_IN_FLIGHT];
	spinlock_t rps_hash_lock;
	struct hlist_head *rps_hash_table;
	u32 rps_next_id;
#endif

	atomic_t active_queues;
	atomic_t rxq_flush_pending;
	atomic_t rxq_flush_outstanding;
	wait_queue_head_t flush_wq;

#ifdef CONFIG_SFC_SRIOV
	unsigned vf_count;
	unsigned vf_init_count;
	unsigned vi_scale;
#endif
	spinlock_t vf_reps_lock;
	struct list_head vf_reps;

	struct efx_ptp_data *ptp_data;
	bool ptp_warned;

	char *vpd_sn;
	bool xdp_rxq_info_failed;

	struct notifier_block netdev_notifier;
	struct notifier_block netevent_notifier;
	struct efx_tc_state *tc;

	struct devlink *devlink;
	struct devlink_port *dl_port;
	unsigned int mem_bar;
	u32 reg_base;

	

	struct delayed_work monitor_work ____cacheline_aligned_in_smp;
	spinlock_t biu_lock;
	int last_irq_cpu;
	spinlock_t stats_lock;
	atomic_t n_rx_noskb_drops;
};


struct efx_probe_data {
	struct pci_dev *pci_dev;
	struct efx_nic efx;
};

static inline struct efx_nic *efx_netdev_priv(struct net_device *dev)
{
	struct efx_probe_data **probe_ptr = netdev_priv(dev);
	struct efx_probe_data *probe_data = *probe_ptr;

	return &probe_data->efx;
}

static inline int efx_dev_registered(struct efx_nic *efx)
{
	return efx->net_dev->reg_state == NETREG_REGISTERED;
}

static inline unsigned int efx_port_num(struct efx_nic *efx)
{
	return efx->port_num;
}

struct efx_mtd_partition {
	struct list_head node;
	struct mtd_info mtd;
	const char *dev_type_name;
	const char *type_name;
	char name[IFNAMSIZ + 20];
};

struct efx_udp_tunnel {
#define TUNNEL_ENCAP_UDP_PORT_ENTRY_INVALID	0xffff
	u16 type; 
	__be16 port;
};


struct efx_nic_type {
	bool is_vf;
	unsigned int (*mem_bar)(struct efx_nic *efx);
	unsigned int (*mem_map_size)(struct efx_nic *efx);
	int (*probe)(struct efx_nic *efx);
	void (*remove)(struct efx_nic *efx);
	int (*init)(struct efx_nic *efx);
	int (*dimension_resources)(struct efx_nic *efx);
	void (*fini)(struct efx_nic *efx);
	void (*monitor)(struct efx_nic *efx);
	enum reset_type (*map_reset_reason)(enum reset_type reason);
	int (*map_reset_flags)(u32 *flags);
	int (*reset)(struct efx_nic *efx, enum reset_type method);
	int (*probe_port)(struct efx_nic *efx);
	void (*remove_port)(struct efx_nic *efx);
	bool (*handle_global_event)(struct efx_channel *channel, efx_qword_t *);
	int (*fini_dmaq)(struct efx_nic *efx);
	void (*prepare_flr)(struct efx_nic *efx);
	void (*finish_flr)(struct efx_nic *efx);
	size_t (*describe_stats)(struct efx_nic *efx, u8 *names);
	size_t (*update_stats)(struct efx_nic *efx, u64 *full_stats,
			       struct rtnl_link_stats64 *core_stats);
	size_t (*update_stats_atomic)(struct efx_nic *efx, u64 *full_stats,
				      struct rtnl_link_stats64 *core_stats);
	void (*start_stats)(struct efx_nic *efx);
	void (*pull_stats)(struct efx_nic *efx);
	void (*stop_stats)(struct efx_nic *efx);
	void (*push_irq_moderation)(struct efx_channel *channel);
	int (*reconfigure_port)(struct efx_nic *efx);
	void (*prepare_enable_fc_tx)(struct efx_nic *efx);
	int (*reconfigure_mac)(struct efx_nic *efx, bool mtu_only);
	bool (*check_mac_fault)(struct efx_nic *efx);
	void (*get_wol)(struct efx_nic *efx, struct ethtool_wolinfo *wol);
	int (*set_wol)(struct efx_nic *efx, u32 type);
	void (*resume_wol)(struct efx_nic *efx);
	void (*get_fec_stats)(struct efx_nic *efx,
			      struct ethtool_fec_stats *fec_stats);
	unsigned int (*check_caps)(const struct efx_nic *efx,
				   u8 flag,
				   u32 offset);
	int (*test_chip)(struct efx_nic *efx, struct efx_self_tests *tests);
	int (*test_nvram)(struct efx_nic *efx);
	void (*mcdi_request)(struct efx_nic *efx,
			     const efx_dword_t *hdr, size_t hdr_len,
			     const efx_dword_t *sdu, size_t sdu_len);
	bool (*mcdi_poll_response)(struct efx_nic *efx);
	void (*mcdi_read_response)(struct efx_nic *efx, efx_dword_t *pdu,
				   size_t pdu_offset, size_t pdu_len);
	int (*mcdi_poll_reboot)(struct efx_nic *efx);
	void (*mcdi_reboot_detected)(struct efx_nic *efx);
	void (*irq_enable_master)(struct efx_nic *efx);
	int (*irq_test_generate)(struct efx_nic *efx);
	void (*irq_disable_non_ev)(struct efx_nic *efx);
	irqreturn_t (*irq_handle_msi)(int irq, void *dev_id);
	irqreturn_t (*irq_handle_legacy)(int irq, void *dev_id);
	int (*tx_probe)(struct efx_tx_queue *tx_queue);
	void (*tx_init)(struct efx_tx_queue *tx_queue);
	void (*tx_remove)(struct efx_tx_queue *tx_queue);
	void (*tx_write)(struct efx_tx_queue *tx_queue);
	netdev_tx_t (*tx_enqueue)(struct efx_tx_queue *tx_queue, struct sk_buff *skb);
	unsigned int (*tx_limit_len)(struct efx_tx_queue *tx_queue,
				     dma_addr_t dma_addr, unsigned int len);
	int (*rx_push_rss_config)(struct efx_nic *efx, bool user,
				  const u32 *rx_indir_table, const u8 *key);
	int (*rx_pull_rss_config)(struct efx_nic *efx);
	int (*rx_push_rss_context_config)(struct efx_nic *efx,
					  struct efx_rss_context *ctx,
					  const u32 *rx_indir_table,
					  const u8 *key);
	int (*rx_pull_rss_context_config)(struct efx_nic *efx,
					  struct efx_rss_context *ctx);
	void (*rx_restore_rss_contexts)(struct efx_nic *efx);
	int (*rx_probe)(struct efx_rx_queue *rx_queue);
	void (*rx_init)(struct efx_rx_queue *rx_queue);
	void (*rx_remove)(struct efx_rx_queue *rx_queue);
	void (*rx_write)(struct efx_rx_queue *rx_queue);
	void (*rx_defer_refill)(struct efx_rx_queue *rx_queue);
	void (*rx_packet)(struct efx_channel *channel);
	bool (*rx_buf_hash_valid)(const u8 *prefix);
	int (*ev_probe)(struct efx_channel *channel);
	int (*ev_init)(struct efx_channel *channel);
	void (*ev_fini)(struct efx_channel *channel);
	void (*ev_remove)(struct efx_channel *channel);
	int (*ev_process)(struct efx_channel *channel, int quota);
	void (*ev_read_ack)(struct efx_channel *channel);
	void (*ev_test_generate)(struct efx_channel *channel);
	int (*filter_table_probe)(struct efx_nic *efx);
	void (*filter_table_restore)(struct efx_nic *efx);
	void (*filter_table_remove)(struct efx_nic *efx);
	void (*filter_update_rx_scatter)(struct efx_nic *efx);
	s32 (*filter_insert)(struct efx_nic *efx,
			     struct efx_filter_spec *spec, bool replace);
	int (*filter_remove_safe)(struct efx_nic *efx,
				  enum efx_filter_priority priority,
				  u32 filter_id);
	int (*filter_get_safe)(struct efx_nic *efx,
			       enum efx_filter_priority priority,
			       u32 filter_id, struct efx_filter_spec *);
	int (*filter_clear_rx)(struct efx_nic *efx,
			       enum efx_filter_priority priority);
	u32 (*filter_count_rx_used)(struct efx_nic *efx,
				    enum efx_filter_priority priority);
	u32 (*filter_get_rx_id_limit)(struct efx_nic *efx);
	s32 (*filter_get_rx_ids)(struct efx_nic *efx,
				 enum efx_filter_priority priority,
				 u32 *buf, u32 size);
#ifdef CONFIG_RFS_ACCEL
	bool (*filter_rfs_expire_one)(struct efx_nic *efx, u32 flow_id,
				      unsigned int index);
#endif
#ifdef CONFIG_SFC_MTD
	int (*mtd_probe)(struct efx_nic *efx);
	void (*mtd_rename)(struct efx_mtd_partition *part);
	int (*mtd_read)(struct mtd_info *mtd, loff_t start, size_t len,
			size_t *retlen, u8 *buffer);
	int (*mtd_erase)(struct mtd_info *mtd, loff_t start, size_t len);
	int (*mtd_write)(struct mtd_info *mtd, loff_t start, size_t len,
			 size_t *retlen, const u8 *buffer);
	int (*mtd_sync)(struct mtd_info *mtd);
#endif
	void (*ptp_write_host_time)(struct efx_nic *efx, u32 host_time);
	int (*ptp_set_ts_sync_events)(struct efx_nic *efx, bool en, bool temp);
	int (*ptp_set_ts_config)(struct efx_nic *efx,
				 struct hwtstamp_config *init);
	int (*sriov_configure)(struct efx_nic *efx, int num_vfs);
	int (*vlan_rx_add_vid)(struct efx_nic *efx, __be16 proto, u16 vid);
	int (*vlan_rx_kill_vid)(struct efx_nic *efx, __be16 proto, u16 vid);
	int (*get_phys_port_id)(struct efx_nic *efx,
				struct netdev_phys_item_id *ppid);
	int (*sriov_init)(struct efx_nic *efx);
	void (*sriov_fini)(struct efx_nic *efx);
	bool (*sriov_wanted)(struct efx_nic *efx);
	int (*sriov_set_vf_mac)(struct efx_nic *efx, int vf_i, const u8 *mac);
	int (*sriov_set_vf_vlan)(struct efx_nic *efx, int vf_i, u16 vlan,
				 u8 qos);
	int (*sriov_set_vf_spoofchk)(struct efx_nic *efx, int vf_i,
				     bool spoofchk);
	int (*sriov_get_vf_config)(struct efx_nic *efx, int vf_i,
				   struct ifla_vf_info *ivi);
	int (*sriov_set_vf_link_state)(struct efx_nic *efx, int vf_i,
				       int link_state);
	int (*vswitching_probe)(struct efx_nic *efx);
	int (*vswitching_restore)(struct efx_nic *efx);
	void (*vswitching_remove)(struct efx_nic *efx);
	int (*get_mac_address)(struct efx_nic *efx, unsigned char *perm_addr);
	int (*set_mac_address)(struct efx_nic *efx);
	u32 (*tso_versions)(struct efx_nic *efx);
	int (*udp_tnl_push_ports)(struct efx_nic *efx);
	bool (*udp_tnl_has_port)(struct efx_nic *efx, __be16 port);
	size_t (*print_additional_fwver)(struct efx_nic *efx, char *buf,
					 size_t len);
	void (*sensor_event)(struct efx_nic *efx, efx_qword_t *ev);
	unsigned int (*rx_recycle_ring_size)(const struct efx_nic *efx);

	int revision;
	unsigned int txd_ptr_tbl_base;
	unsigned int rxd_ptr_tbl_base;
	unsigned int buf_tbl_base;
	unsigned int evq_ptr_tbl_base;
	unsigned int evq_rptr_tbl_base;
	u64 max_dma_mask;
	unsigned int rx_prefix_size;
	unsigned int rx_hash_offset;
	unsigned int rx_ts_offset;
	unsigned int rx_buffer_padding;
	bool can_rx_scatter;
	bool always_rx_scatter;
	bool option_descriptors;
	unsigned int min_interrupt_mode;
	unsigned int timer_period_max;
	netdev_features_t offload_features;
	int mcdi_max_ver;
	unsigned int max_rx_ip_filters;
	u32 hwtstamp_filters;
	unsigned int rx_hash_key_size;
};



static inline struct efx_channel *
efx_get_channel(struct efx_nic *efx, unsigned index)
{
	EFX_WARN_ON_ONCE_PARANOID(index >= efx->n_channels);
	return efx->channel[index];
}


#define efx_for_each_channel(_channel, _efx)				\
	for (_channel = (_efx)->channel[0];				\
	     _channel;							\
	     _channel = (_channel->channel + 1 < (_efx)->n_channels) ?	\
		     (_efx)->channel[_channel->channel + 1] : NULL)


#define efx_for_each_channel_rev(_channel, _efx)			\
	for (_channel = (_efx)->channel[(_efx)->n_channels - 1];	\
	     _channel;							\
	     _channel = _channel->channel ?				\
		     (_efx)->channel[_channel->channel - 1] : NULL)

static inline struct efx_channel *
efx_get_tx_channel(struct efx_nic *efx, unsigned int index)
{
	EFX_WARN_ON_ONCE_PARANOID(index >= efx->n_tx_channels);
	return efx->channel[efx->tx_channel_offset + index];
}

static inline struct efx_channel *
efx_get_xdp_channel(struct efx_nic *efx, unsigned int index)
{
	EFX_WARN_ON_ONCE_PARANOID(index >= efx->n_xdp_channels);
	return efx->channel[efx->xdp_channel_offset + index];
}

static inline bool efx_channel_is_xdp_tx(struct efx_channel *channel)
{
	return channel->channel - channel->efx->xdp_channel_offset <
	       channel->efx->n_xdp_channels;
}

static inline bool efx_channel_has_tx_queues(struct efx_channel *channel)
{
	return channel && channel->channel >= channel->efx->tx_channel_offset;
}

static inline unsigned int efx_channel_num_tx_queues(struct efx_channel *channel)
{
	if (efx_channel_is_xdp_tx(channel))
		return channel->efx->xdp_tx_per_channel;
	return channel->efx->tx_queues_per_channel;
}

static inline struct efx_tx_queue *
efx_channel_get_tx_queue(struct efx_channel *channel, unsigned int type)
{
	EFX_WARN_ON_ONCE_PARANOID(type >= EFX_TXQ_TYPES);
	return channel->tx_queue_by_type[type];
}

static inline struct efx_tx_queue *
efx_get_tx_queue(struct efx_nic *efx, unsigned int index, unsigned int type)
{
	struct efx_channel *channel = efx_get_tx_channel(efx, index);

	return efx_channel_get_tx_queue(channel, type);
}


#define efx_for_each_channel_tx_queue(_tx_queue, _channel)		\
	if (!efx_channel_has_tx_queues(_channel))			\
		;							\
	else								\
		for (_tx_queue = (_channel)->tx_queue;			\
		     _tx_queue < (_channel)->tx_queue +			\
				 efx_channel_num_tx_queues(_channel);		\
		     _tx_queue++)

static inline bool efx_channel_has_rx_queue(struct efx_channel *channel)
{
	return channel->rx_queue.core_index >= 0;
}

static inline struct efx_rx_queue *
efx_channel_get_rx_queue(struct efx_channel *channel)
{
	EFX_WARN_ON_ONCE_PARANOID(!efx_channel_has_rx_queue(channel));
	return &channel->rx_queue;
}


#define efx_for_each_channel_rx_queue(_rx_queue, _channel)		\
	if (!efx_channel_has_rx_queue(_channel))			\
		;							\
	else								\
		for (_rx_queue = &(_channel)->rx_queue;			\
		     _rx_queue;						\
		     _rx_queue = NULL)

static inline struct efx_channel *
efx_rx_queue_channel(struct efx_rx_queue *rx_queue)
{
	return container_of(rx_queue, struct efx_channel, rx_queue);
}

static inline int efx_rx_queue_index(struct efx_rx_queue *rx_queue)
{
	return efx_rx_queue_channel(rx_queue)->channel;
}


static inline struct efx_rx_buffer *efx_rx_buffer(struct efx_rx_queue *rx_queue,
						  unsigned int index)
{
	return &rx_queue->buffer[index];
}

static inline struct efx_rx_buffer *
efx_rx_buf_next(struct efx_rx_queue *rx_queue, struct efx_rx_buffer *rx_buf)
{
	if (unlikely(rx_buf == efx_rx_buffer(rx_queue, rx_queue->ptr_mask)))
		return efx_rx_buffer(rx_queue, 0);
	else
		return rx_buf + 1;
}


#define EFX_FRAME_PAD	16
#define EFX_MAX_FRAME_LEN(mtu) \
	(ALIGN(((mtu) + ETH_HLEN + VLAN_HLEN + ETH_FCS_LEN + EFX_FRAME_PAD), 8))

static inline bool efx_xmit_with_hwtstamp(struct sk_buff *skb)
{
	return skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP;
}
static inline void efx_xmit_hwtstamp_pending(struct sk_buff *skb)
{
	skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
}


static inline unsigned int
efx_channel_tx_fill_level(struct efx_channel *channel)
{
	struct efx_tx_queue *tx_queue;
	unsigned int fill_level = 0;

	efx_for_each_channel_tx_queue(tx_queue, channel)
		fill_level = max(fill_level,
				 tx_queue->insert_count - tx_queue->read_count);

	return fill_level;
}


static inline unsigned int
efx_channel_tx_old_fill_level(struct efx_channel *channel)
{
	struct efx_tx_queue *tx_queue;
	unsigned int fill_level = 0;

	efx_for_each_channel_tx_queue(tx_queue, channel)
		fill_level = max(fill_level,
				 tx_queue->insert_count - tx_queue->old_read_count);

	return fill_level;
}


static inline netdev_features_t efx_supported_features(const struct efx_nic *efx)
{
	const struct net_device *net_dev = efx->net_dev;

	return net_dev->features | net_dev->hw_features;
}


static inline unsigned int
efx_tx_queue_get_insert_index(const struct efx_tx_queue *tx_queue)
{
	return tx_queue->insert_count & tx_queue->ptr_mask;
}


static inline struct efx_tx_buffer *
__efx_tx_queue_get_insert_buffer(const struct efx_tx_queue *tx_queue)
{
	return &tx_queue->buffer[efx_tx_queue_get_insert_index(tx_queue)];
}


static inline struct efx_tx_buffer *
efx_tx_queue_get_insert_buffer(const struct efx_tx_queue *tx_queue)
{
	struct efx_tx_buffer *buffer =
		__efx_tx_queue_get_insert_buffer(tx_queue);

	EFX_WARN_ON_ONCE_PARANOID(buffer->len);
	EFX_WARN_ON_ONCE_PARANOID(buffer->flags);
	EFX_WARN_ON_ONCE_PARANOID(buffer->unmap_len);

	return buffer;
}

#endif 
