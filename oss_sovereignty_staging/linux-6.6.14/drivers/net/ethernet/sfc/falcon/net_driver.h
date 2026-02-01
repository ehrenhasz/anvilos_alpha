 
 

 

#ifndef EF4_NET_DRIVER_H
#define EF4_NET_DRIVER_H

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
#include <linux/i2c.h>
#include <linux/mtd/mtd.h>
#include <net/busy_poll.h>

#include "enum.h"
#include "bitfield.h"
#include "filter.h"

 

#define EF4_DRIVER_VERSION	"4.1"

#ifdef DEBUG
#define EF4_BUG_ON_PARANOID(x) BUG_ON(x)
#define EF4_WARN_ON_PARANOID(x) WARN_ON(x)
#else
#define EF4_BUG_ON_PARANOID(x) do {} while (0)
#define EF4_WARN_ON_PARANOID(x) do {} while (0)
#endif

 

#define EF4_MAX_CHANNELS 32U
#define EF4_MAX_RX_QUEUES EF4_MAX_CHANNELS
#define EF4_EXTRA_CHANNEL_IOV	0
#define EF4_EXTRA_CHANNEL_PTP	1
#define EF4_MAX_EXTRA_CHANNELS	2U

 
#define EF4_MAX_TX_TC		2
#define EF4_MAX_CORE_TX_QUEUES	(EF4_MAX_TX_TC * EF4_MAX_CHANNELS)
#define EF4_TXQ_TYPE_OFFLOAD	1	 
#define EF4_TXQ_TYPE_HIGHPRI	2	 
#define EF4_TXQ_TYPES		4
#define EF4_MAX_TX_QUEUES	(EF4_TXQ_TYPES * EF4_MAX_CHANNELS)

 
#define EF4_MAX_MTU (9 * 1024)

 
#define EF4_MIN_MTU 68

 
#define EF4_RX_USR_BUF_SIZE	(2048 - 256)

 
#if NET_IP_ALIGN == 0
#define EF4_RX_BUF_ALIGNMENT	L1_CACHE_BYTES
#else
#define EF4_RX_BUF_ALIGNMENT	4
#endif

struct ef4_self_tests;

 
struct ef4_buffer {
	void *addr;
	dma_addr_t dma_addr;
	unsigned int len;
};

 
struct ef4_special_buffer {
	struct ef4_buffer buf;
	unsigned int index;
	unsigned int entries;
};

 
struct ef4_tx_buffer {
	const struct sk_buff *skb;
	union {
		ef4_qword_t option;
		dma_addr_t dma_addr;
	};
	unsigned short flags;
	unsigned short len;
	unsigned short unmap_len;
	unsigned short dma_offset;
};
#define EF4_TX_BUF_CONT		1	 
#define EF4_TX_BUF_SKB		2	 
#define EF4_TX_BUF_MAP_SINGLE	8	 
#define EF4_TX_BUF_OPTION	0x10	 

 
struct ef4_tx_queue {
	 
	struct ef4_nic *efx ____cacheline_aligned_in_smp;
	unsigned queue;
	struct ef4_channel *channel;
	struct netdev_queue *core_txq;
	struct ef4_tx_buffer *buffer;
	struct ef4_buffer *cb_page;
	struct ef4_special_buffer txd;
	unsigned int ptr_mask;
	bool initialised;
	unsigned int tx_min_size;

	 
	int (*handle_tso)(struct ef4_tx_queue*, struct sk_buff*, bool *);

	 
	unsigned int read_count ____cacheline_aligned_in_smp;
	unsigned int old_write_count;
	unsigned int merge_events;
	unsigned int bytes_compl;
	unsigned int pkts_compl;

	 
	unsigned int insert_count ____cacheline_aligned_in_smp;
	unsigned int write_count;
	unsigned int old_read_count;
	unsigned int pushes;
	bool xmit_more_available;
	unsigned int cb_packets;
	 
	unsigned long tx_packets;

	 
	unsigned int empty_read_count ____cacheline_aligned_in_smp;
#define EF4_EMPTY_COUNT_VALID 0x80000000
	atomic_t flush_outstanding;
};

#define EF4_TX_CB_ORDER	7
#define EF4_TX_CB_SIZE	(1 << EF4_TX_CB_ORDER) - NET_IP_ALIGN

 
struct ef4_rx_buffer {
	dma_addr_t dma_addr;
	struct page *page;
	u16 page_offset;
	u16 len;
	u16 flags;
};
#define EF4_RX_BUF_LAST_IN_PAGE	0x0001
#define EF4_RX_PKT_CSUMMED	0x0002
#define EF4_RX_PKT_DISCARD	0x0004
#define EF4_RX_PKT_TCP		0x0040
#define EF4_RX_PKT_PREFIX_LEN	0x0080	 

 
struct ef4_rx_page_state {
	dma_addr_t dma_addr;

	unsigned int __pad[] ____cacheline_aligned;
};

 
struct ef4_rx_queue {
	struct ef4_nic *efx;
	int core_index;
	struct ef4_rx_buffer *buffer;
	struct ef4_special_buffer rxd;
	unsigned int ptr_mask;
	bool refill_enabled;
	bool flush_pending;

	unsigned int added_count;
	unsigned int notified_count;
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
	 
	unsigned long rx_packets;
};

 
struct ef4_channel {
	struct ef4_nic *efx;
	int channel;
	const struct ef4_channel_type *type;
	bool eventq_init;
	bool enabled;
	int irq;
	unsigned int irq_moderation_us;
	struct net_device *napi_dev;
	struct napi_struct napi_str;
#ifdef CONFIG_NET_RX_BUSY_POLL
	unsigned long busy_poll_state;
#endif
	struct ef4_special_buffer eventq;
	unsigned int eventq_mask;
	unsigned int eventq_read_ptr;
	int event_test_cpu;

	unsigned int irq_count;
	unsigned int irq_mod_score;
#ifdef CONFIG_RFS_ACCEL
	unsigned int rfs_filters_added;
#define RPS_FLOW_ID_INVALID 0xFFFFFFFF
	u32 *rps_flow_id;
#endif

	unsigned n_rx_tobe_disc;
	unsigned n_rx_ip_hdr_chksum_err;
	unsigned n_rx_tcp_udp_chksum_err;
	unsigned n_rx_mcast_mismatch;
	unsigned n_rx_frm_trunc;
	unsigned n_rx_overlength;
	unsigned n_skbuff_leaks;
	unsigned int n_rx_nodesc_trunc;
	unsigned int n_rx_merge_events;
	unsigned int n_rx_merge_packets;

	unsigned int rx_pkt_n_frags;
	unsigned int rx_pkt_index;

	struct ef4_rx_queue rx_queue;
	struct ef4_tx_queue tx_queue[EF4_TXQ_TYPES];
};

 
struct ef4_msi_context {
	struct ef4_nic *efx;
	unsigned int index;
	char name[IFNAMSIZ + 6];
};

 
struct ef4_channel_type {
	void (*handle_no_channel)(struct ef4_nic *);
	int (*pre_probe)(struct ef4_channel *);
	void (*post_remove)(struct ef4_channel *);
	void (*get_name)(struct ef4_channel *, char *buf, size_t len);
	struct ef4_channel *(*copy)(const struct ef4_channel *);
	bool (*receive_skb)(struct ef4_channel *, struct sk_buff *);
	bool keep_eventq;
};

enum ef4_led_mode {
	EF4_LED_OFF	= 0,
	EF4_LED_ON	= 1,
	EF4_LED_DEFAULT	= 2
};

#define STRING_TABLE_LOOKUP(val, member) \
	((val) < member ## _max) ? member ## _names[val] : "(invalid)"

extern const char *const ef4_loopback_mode_names[];
extern const unsigned int ef4_loopback_mode_max;
#define LOOPBACK_MODE(efx) \
	STRING_TABLE_LOOKUP((efx)->loopback_mode, ef4_loopback_mode)

extern const char *const ef4_reset_type_names[];
extern const unsigned int ef4_reset_type_max;
#define RESET_TYPE(type) \
	STRING_TABLE_LOOKUP(type, ef4_reset_type)

enum ef4_int_mode {
	 
	EF4_INT_MODE_MSIX = 0,
	EF4_INT_MODE_MSI = 1,
	EF4_INT_MODE_LEGACY = 2,
	EF4_INT_MODE_MAX	 
};
#define EF4_INT_MODE_USE_MSI(x) (((x)->interrupt_mode) <= EF4_INT_MODE_MSI)

enum nic_state {
	STATE_UNINIT = 0,	 
	STATE_READY = 1,	 
	STATE_DISABLED = 2,	 
	STATE_RECOVERY = 3,	 
};

 
struct ef4_nic;

 
#define EF4_FC_RX	FLOW_CTRL_RX
#define EF4_FC_TX	FLOW_CTRL_TX
#define EF4_FC_AUTO	4

 
struct ef4_link_state {
	bool up;
	bool fd;
	u8 fc;
	unsigned int speed;
};

static inline bool ef4_link_state_equal(const struct ef4_link_state *left,
					const struct ef4_link_state *right)
{
	return left->up == right->up && left->fd == right->fd &&
		left->fc == right->fc && left->speed == right->speed;
}

 
struct ef4_phy_operations {
	int (*probe) (struct ef4_nic *efx);
	int (*init) (struct ef4_nic *efx);
	void (*fini) (struct ef4_nic *efx);
	void (*remove) (struct ef4_nic *efx);
	int (*reconfigure) (struct ef4_nic *efx);
	bool (*poll) (struct ef4_nic *efx);
	void (*get_link_ksettings)(struct ef4_nic *efx,
				   struct ethtool_link_ksettings *cmd);
	int (*set_link_ksettings)(struct ef4_nic *efx,
				  const struct ethtool_link_ksettings *cmd);
	void (*set_npage_adv) (struct ef4_nic *efx, u32);
	int (*test_alive) (struct ef4_nic *efx);
	const char *(*test_name) (struct ef4_nic *efx, unsigned int index);
	int (*run_tests) (struct ef4_nic *efx, int *results, unsigned flags);
	int (*get_module_eeprom) (struct ef4_nic *efx,
			       struct ethtool_eeprom *ee,
			       u8 *data);
	int (*get_module_info) (struct ef4_nic *efx,
				struct ethtool_modinfo *modinfo);
};

 
enum ef4_phy_mode {
	PHY_MODE_NORMAL		= 0,
	PHY_MODE_TX_DISABLED	= 1,
	PHY_MODE_LOW_POWER	= 2,
	PHY_MODE_OFF		= 4,
	PHY_MODE_SPECIAL	= 8,
};

static inline bool ef4_phy_mode_disabled(enum ef4_phy_mode mode)
{
	return !!(mode & ~PHY_MODE_TX_DISABLED);
}

 
struct ef4_hw_stat_desc {
	const char *name;
	u16 dma_width;
	u16 offset;
};

 
#define EF4_MCAST_HASH_BITS 8

 
#define EF4_MCAST_HASH_ENTRIES (1 << EF4_MCAST_HASH_BITS)

 
union ef4_multicast_hash {
	u8 byte[EF4_MCAST_HASH_ENTRIES / 8];
	ef4_oword_t oword[EF4_MCAST_HASH_ENTRIES / sizeof(ef4_oword_t) / 8];
};

 
struct ef4_nic {
	 

	char name[IFNAMSIZ];
	struct list_head node;
	struct ef4_nic *primary;
	struct list_head secondary_list;
	struct pci_dev *pci_dev;
	unsigned int port_num;
	const struct ef4_nic_type *type;
	int legacy_irq;
	bool eeh_disabled_legacy_irq;
	struct workqueue_struct *workqueue;
	char workqueue_name[16];
	struct work_struct reset_work;
	resource_size_t membase_phys;
	void __iomem *membase;

	enum ef4_int_mode interrupt_mode;
	unsigned int timer_quantum_ns;
	unsigned int timer_max_ns;
	bool irq_rx_adaptive;
	unsigned int irq_mod_step_us;
	unsigned int irq_rx_moderation_us;
	u32 msg_enable;

	enum nic_state state;
	unsigned long reset_pending;

	struct ef4_channel *channel[EF4_MAX_CHANNELS];
	struct ef4_msi_context msi_context[EF4_MAX_CHANNELS];
	const struct ef4_channel_type *
	extra_channel_type[EF4_MAX_EXTRA_CHANNELS];

	unsigned rxq_entries;
	unsigned txq_entries;
	unsigned int txq_stop_thresh;
	unsigned int txq_wake_thresh;

	unsigned tx_dc_base;
	unsigned rx_dc_base;
	unsigned sram_lim_qw;
	unsigned next_buffer_table;

	unsigned int max_channels;
	unsigned int max_tx_channels;
	unsigned n_channels;
	unsigned n_rx_channels;
	unsigned rss_spread;
	unsigned tx_channel_offset;
	unsigned n_tx_channels;
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
	u8 rx_hash_key[40];
	u32 rx_indir_table[128];
	bool rx_scatter;

	unsigned int_error_count;
	unsigned long int_error_expire;

	bool irq_soft_enabled;
	struct ef4_buffer irq_status;
	unsigned irq_zero_count;
	unsigned irq_level;
	struct delayed_work selftest_work;

#ifdef CONFIG_SFC_FALCON_MTD
	struct list_head mtd_list;
#endif

	void *nic_data;

	struct mutex mac_lock;
	struct work_struct mac_work;
	bool port_enabled;

	bool mc_bist_for_other_fn;
	bool port_initialized;
	struct net_device *net_dev;

	netdev_features_t fixed_features;

	struct ef4_buffer stats_buffer;
	u64 rx_nodesc_drops_total;
	u64 rx_nodesc_drops_while_down;
	bool rx_nodesc_drops_prev_state;

	unsigned int phy_type;
	const struct ef4_phy_operations *phy_op;
	void *phy_data;
	struct mdio_if_info mdio;
	enum ef4_phy_mode phy_mode;

	u32 link_advertising;
	struct ef4_link_state link_state;
	unsigned int n_link_state_changes;

	bool unicast_filter;
	union ef4_multicast_hash multicast_hash;
	u8 wanted_fc;
	unsigned fc_disable;

	atomic_t rx_reset;
	enum ef4_loopback_mode loopback_mode;
	u64 loopback_modes;

	void *loopback_selftest;

	struct rw_semaphore filter_sem;
	spinlock_t filter_lock;
	void *filter_state;
#ifdef CONFIG_RFS_ACCEL
	unsigned int rps_expire_channel;
	unsigned int rps_expire_index;
#endif

	atomic_t active_queues;
	atomic_t rxq_flush_pending;
	atomic_t rxq_flush_outstanding;
	wait_queue_head_t flush_wq;

	char *vpd_sn;

	 

	struct delayed_work monitor_work ____cacheline_aligned_in_smp;
	spinlock_t biu_lock;
	int last_irq_cpu;
	spinlock_t stats_lock;
	atomic_t n_rx_noskb_drops;
};

static inline int ef4_dev_registered(struct ef4_nic *efx)
{
	return efx->net_dev->reg_state == NETREG_REGISTERED;
}

static inline unsigned int ef4_port_num(struct ef4_nic *efx)
{
	return efx->port_num;
}

struct ef4_mtd_partition {
	struct list_head node;
	struct mtd_info mtd;
	const char *dev_type_name;
	const char *type_name;
	char name[IFNAMSIZ + 20];
};

 
struct ef4_nic_type {
	unsigned int mem_bar;
	unsigned int (*mem_map_size)(struct ef4_nic *efx);
	int (*probe)(struct ef4_nic *efx);
	void (*remove)(struct ef4_nic *efx);
	int (*init)(struct ef4_nic *efx);
	int (*dimension_resources)(struct ef4_nic *efx);
	void (*fini)(struct ef4_nic *efx);
	void (*monitor)(struct ef4_nic *efx);
	enum reset_type (*map_reset_reason)(enum reset_type reason);
	int (*map_reset_flags)(u32 *flags);
	int (*reset)(struct ef4_nic *efx, enum reset_type method);
	int (*probe_port)(struct ef4_nic *efx);
	void (*remove_port)(struct ef4_nic *efx);
	bool (*handle_global_event)(struct ef4_channel *channel, ef4_qword_t *);
	int (*fini_dmaq)(struct ef4_nic *efx);
	void (*prepare_flush)(struct ef4_nic *efx);
	void (*finish_flush)(struct ef4_nic *efx);
	void (*prepare_flr)(struct ef4_nic *efx);
	void (*finish_flr)(struct ef4_nic *efx);
	size_t (*describe_stats)(struct ef4_nic *efx, u8 *names);
	size_t (*update_stats)(struct ef4_nic *efx, u64 *full_stats,
			       struct rtnl_link_stats64 *core_stats);
	void (*start_stats)(struct ef4_nic *efx);
	void (*pull_stats)(struct ef4_nic *efx);
	void (*stop_stats)(struct ef4_nic *efx);
	void (*set_id_led)(struct ef4_nic *efx, enum ef4_led_mode mode);
	void (*push_irq_moderation)(struct ef4_channel *channel);
	int (*reconfigure_port)(struct ef4_nic *efx);
	void (*prepare_enable_fc_tx)(struct ef4_nic *efx);
	int (*reconfigure_mac)(struct ef4_nic *efx);
	bool (*check_mac_fault)(struct ef4_nic *efx);
	void (*get_wol)(struct ef4_nic *efx, struct ethtool_wolinfo *wol);
	int (*set_wol)(struct ef4_nic *efx, u32 type);
	void (*resume_wol)(struct ef4_nic *efx);
	int (*test_chip)(struct ef4_nic *efx, struct ef4_self_tests *tests);
	int (*test_nvram)(struct ef4_nic *efx);
	void (*irq_enable_master)(struct ef4_nic *efx);
	int (*irq_test_generate)(struct ef4_nic *efx);
	void (*irq_disable_non_ev)(struct ef4_nic *efx);
	irqreturn_t (*irq_handle_msi)(int irq, void *dev_id);
	irqreturn_t (*irq_handle_legacy)(int irq, void *dev_id);
	int (*tx_probe)(struct ef4_tx_queue *tx_queue);
	void (*tx_init)(struct ef4_tx_queue *tx_queue);
	void (*tx_remove)(struct ef4_tx_queue *tx_queue);
	void (*tx_write)(struct ef4_tx_queue *tx_queue);
	unsigned int (*tx_limit_len)(struct ef4_tx_queue *tx_queue,
				     dma_addr_t dma_addr, unsigned int len);
	int (*rx_push_rss_config)(struct ef4_nic *efx, bool user,
				  const u32 *rx_indir_table);
	int (*rx_probe)(struct ef4_rx_queue *rx_queue);
	void (*rx_init)(struct ef4_rx_queue *rx_queue);
	void (*rx_remove)(struct ef4_rx_queue *rx_queue);
	void (*rx_write)(struct ef4_rx_queue *rx_queue);
	void (*rx_defer_refill)(struct ef4_rx_queue *rx_queue);
	int (*ev_probe)(struct ef4_channel *channel);
	int (*ev_init)(struct ef4_channel *channel);
	void (*ev_fini)(struct ef4_channel *channel);
	void (*ev_remove)(struct ef4_channel *channel);
	int (*ev_process)(struct ef4_channel *channel, int quota);
	void (*ev_read_ack)(struct ef4_channel *channel);
	void (*ev_test_generate)(struct ef4_channel *channel);
	int (*filter_table_probe)(struct ef4_nic *efx);
	void (*filter_table_restore)(struct ef4_nic *efx);
	void (*filter_table_remove)(struct ef4_nic *efx);
	void (*filter_update_rx_scatter)(struct ef4_nic *efx);
	s32 (*filter_insert)(struct ef4_nic *efx,
			     struct ef4_filter_spec *spec, bool replace);
	int (*filter_remove_safe)(struct ef4_nic *efx,
				  enum ef4_filter_priority priority,
				  u32 filter_id);
	int (*filter_get_safe)(struct ef4_nic *efx,
			       enum ef4_filter_priority priority,
			       u32 filter_id, struct ef4_filter_spec *);
	int (*filter_clear_rx)(struct ef4_nic *efx,
			       enum ef4_filter_priority priority);
	u32 (*filter_count_rx_used)(struct ef4_nic *efx,
				    enum ef4_filter_priority priority);
	u32 (*filter_get_rx_id_limit)(struct ef4_nic *efx);
	s32 (*filter_get_rx_ids)(struct ef4_nic *efx,
				 enum ef4_filter_priority priority,
				 u32 *buf, u32 size);
#ifdef CONFIG_RFS_ACCEL
	s32 (*filter_rfs_insert)(struct ef4_nic *efx,
				 struct ef4_filter_spec *spec);
	bool (*filter_rfs_expire_one)(struct ef4_nic *efx, u32 flow_id,
				      unsigned int index);
#endif
#ifdef CONFIG_SFC_FALCON_MTD
	int (*mtd_probe)(struct ef4_nic *efx);
	void (*mtd_rename)(struct ef4_mtd_partition *part);
	int (*mtd_read)(struct mtd_info *mtd, loff_t start, size_t len,
			size_t *retlen, u8 *buffer);
	int (*mtd_erase)(struct mtd_info *mtd, loff_t start, size_t len);
	int (*mtd_write)(struct mtd_info *mtd, loff_t start, size_t len,
			 size_t *retlen, const u8 *buffer);
	int (*mtd_sync)(struct mtd_info *mtd);
#endif
	int (*get_mac_address)(struct ef4_nic *efx, unsigned char *perm_addr);
	int (*set_mac_address)(struct ef4_nic *efx);

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
	unsigned int max_interrupt_mode;
	unsigned int timer_period_max;
	netdev_features_t offload_features;
	unsigned int max_rx_ip_filters;
};

 

static inline struct ef4_channel *
ef4_get_channel(struct ef4_nic *efx, unsigned index)
{
	EF4_BUG_ON_PARANOID(index >= efx->n_channels);
	return efx->channel[index];
}

 
#define ef4_for_each_channel(_channel, _efx)				\
	for (_channel = (_efx)->channel[0];				\
	     _channel;							\
	     _channel = (_channel->channel + 1 < (_efx)->n_channels) ?	\
		     (_efx)->channel[_channel->channel + 1] : NULL)

 
#define ef4_for_each_channel_rev(_channel, _efx)			\
	for (_channel = (_efx)->channel[(_efx)->n_channels - 1];	\
	     _channel;							\
	     _channel = _channel->channel ?				\
		     (_efx)->channel[_channel->channel - 1] : NULL)

static inline struct ef4_tx_queue *
ef4_get_tx_queue(struct ef4_nic *efx, unsigned index, unsigned type)
{
	EF4_BUG_ON_PARANOID(index >= efx->n_tx_channels ||
			    type >= EF4_TXQ_TYPES);
	return &efx->channel[efx->tx_channel_offset + index]->tx_queue[type];
}

static inline bool ef4_channel_has_tx_queues(struct ef4_channel *channel)
{
	return channel->channel - channel->efx->tx_channel_offset <
		channel->efx->n_tx_channels;
}

static inline struct ef4_tx_queue *
ef4_channel_get_tx_queue(struct ef4_channel *channel, unsigned type)
{
	EF4_BUG_ON_PARANOID(!ef4_channel_has_tx_queues(channel) ||
			    type >= EF4_TXQ_TYPES);
	return &channel->tx_queue[type];
}

static inline bool ef4_tx_queue_used(struct ef4_tx_queue *tx_queue)
{
	return !(tx_queue->efx->net_dev->num_tc < 2 &&
		 tx_queue->queue & EF4_TXQ_TYPE_HIGHPRI);
}

 
#define ef4_for_each_channel_tx_queue(_tx_queue, _channel)		\
	if (!ef4_channel_has_tx_queues(_channel))			\
		;							\
	else								\
		for (_tx_queue = (_channel)->tx_queue;			\
		     _tx_queue < (_channel)->tx_queue + EF4_TXQ_TYPES && \
			     ef4_tx_queue_used(_tx_queue);		\
		     _tx_queue++)

 
#define ef4_for_each_possible_channel_tx_queue(_tx_queue, _channel)	\
	if (!ef4_channel_has_tx_queues(_channel))			\
		;							\
	else								\
		for (_tx_queue = (_channel)->tx_queue;			\
		     _tx_queue < (_channel)->tx_queue + EF4_TXQ_TYPES;	\
		     _tx_queue++)

static inline bool ef4_channel_has_rx_queue(struct ef4_channel *channel)
{
	return channel->rx_queue.core_index >= 0;
}

static inline struct ef4_rx_queue *
ef4_channel_get_rx_queue(struct ef4_channel *channel)
{
	EF4_BUG_ON_PARANOID(!ef4_channel_has_rx_queue(channel));
	return &channel->rx_queue;
}

 
#define ef4_for_each_channel_rx_queue(_rx_queue, _channel)		\
	if (!ef4_channel_has_rx_queue(_channel))			\
		;							\
	else								\
		for (_rx_queue = &(_channel)->rx_queue;			\
		     _rx_queue;						\
		     _rx_queue = NULL)

static inline struct ef4_channel *
ef4_rx_queue_channel(struct ef4_rx_queue *rx_queue)
{
	return container_of(rx_queue, struct ef4_channel, rx_queue);
}

static inline int ef4_rx_queue_index(struct ef4_rx_queue *rx_queue)
{
	return ef4_rx_queue_channel(rx_queue)->channel;
}

 
static inline struct ef4_rx_buffer *ef4_rx_buffer(struct ef4_rx_queue *rx_queue,
						  unsigned int index)
{
	return &rx_queue->buffer[index];
}

 
#define EF4_FRAME_PAD	16
#define EF4_MAX_FRAME_LEN(mtu) \
	(ALIGN(((mtu) + ETH_HLEN + VLAN_HLEN + ETH_FCS_LEN + EF4_FRAME_PAD), 8))

 
static inline netdev_features_t ef4_supported_features(const struct ef4_nic *efx)
{
	const struct net_device *net_dev = efx->net_dev;

	return net_dev->features | net_dev->hw_features;
}

 
static inline unsigned int
ef4_tx_queue_get_insert_index(const struct ef4_tx_queue *tx_queue)
{
	return tx_queue->insert_count & tx_queue->ptr_mask;
}

 
static inline struct ef4_tx_buffer *
__ef4_tx_queue_get_insert_buffer(const struct ef4_tx_queue *tx_queue)
{
	return &tx_queue->buffer[ef4_tx_queue_get_insert_index(tx_queue)];
}

 
static inline struct ef4_tx_buffer *
ef4_tx_queue_get_insert_buffer(const struct ef4_tx_queue *tx_queue)
{
	struct ef4_tx_buffer *buffer =
		__ef4_tx_queue_get_insert_buffer(tx_queue);

	EF4_BUG_ON_PARANOID(buffer->len);
	EF4_BUG_ON_PARANOID(buffer->flags);
	EF4_BUG_ON_PARANOID(buffer->unmap_len);

	return buffer;
}

#endif  
