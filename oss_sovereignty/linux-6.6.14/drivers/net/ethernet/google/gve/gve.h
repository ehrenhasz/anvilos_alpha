 

#ifndef _GVE_H_
#define _GVE_H_

#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/u64_stats_sync.h>
#include <net/xdp.h>

#include "gve_desc.h"
#include "gve_desc_dqo.h"

#ifndef PCI_VENDOR_ID_GOOGLE
#define PCI_VENDOR_ID_GOOGLE	0x1ae0
#endif

#define PCI_DEV_ID_GVNIC	0x0042

#define GVE_REGISTER_BAR	0
#define GVE_DOORBELL_BAR	2

 
#define GVE_TX_MAX_IOVEC	4
 
#define GVE_MIN_MSIX 3

 
#define GVE_TX_STATS_REPORT_NUM	6
#define GVE_RX_STATS_REPORT_NUM	2

 
#define GVE_STATS_REPORT_TIMER_PERIOD	20000

 
#define NIC_TX_STATS_REPORT_NUM	0
#define NIC_RX_STATS_REPORT_NUM	4

#define GVE_DATA_SLOT_ADDR_PAGE_MASK (~(PAGE_SIZE - 1))

 
#define GVE_NUM_PTYPES	1024

#define GVE_RX_BUFFER_SIZE_DQO 2048

#define GVE_XDP_ACTIONS 5

#define GVE_GQ_TX_MIN_PKT_DESC_BYTES 182

#define DQO_QPL_DEFAULT_TX_PAGES 512
#define DQO_QPL_DEFAULT_RX_PAGES 2048

 
#define GVE_DQO_TX_MAX	0x3FFFF

#define GVE_TX_BUF_SHIFT_DQO 11

 
#define GVE_TX_BUF_SIZE_DQO BIT(GVE_TX_BUF_SHIFT_DQO)
#define GVE_TX_BUFS_PER_PAGE_DQO (PAGE_SIZE >> GVE_TX_BUF_SHIFT_DQO)
#define GVE_MAX_TX_BUFS_PER_PKT (DIV_ROUND_UP(GVE_DQO_TX_MAX, GVE_TX_BUF_SIZE_DQO))

 
#define GVE_DQO_QPL_ONDEMAND_ALLOC_THRESHOLD 96

 
struct gve_rx_desc_queue {
	struct gve_rx_desc *desc_ring;  
	dma_addr_t bus;  
	u8 seqno;  
};

 
struct gve_rx_slot_page_info {
	struct page *page;
	void *page_address;
	u32 page_offset;  
	int pagecnt_bias;  
	u16 pad;  
	u8 can_flip;  
};

 
struct gve_queue_page_list {
	u32 id;  
	u32 num_entries;
	struct page **pages;  
	dma_addr_t *page_buses;  
};

 
struct gve_rx_data_queue {
	union gve_rx_data_slot *data_ring;  
	dma_addr_t data_bus;  
	struct gve_rx_slot_page_info *page_info;  
	struct gve_queue_page_list *qpl;  
	u8 raw_addressing;  
};

struct gve_priv;

 
struct gve_rx_buf_queue_dqo {
	struct gve_rx_desc_dqo *desc_ring;
	dma_addr_t bus;
	u32 head;  
	u32 tail;  
	u32 mask;  
};

 
struct gve_rx_compl_queue_dqo {
	struct gve_rx_compl_desc_dqo *desc_ring;
	dma_addr_t bus;

	 
	int num_free_slots;

	 
	u8 cur_gen_bit;

	 
	u32 head;
	u32 mask;  
};

 
struct gve_rx_buf_state_dqo {
	 
	struct gve_rx_slot_page_info page_info;

	 
	dma_addr_t addr;

	 
	u32 last_single_ref_offset;

	 
	s16 next;
};

 
struct gve_index_list {
	s16 head;
	s16 tail;
};

 
struct gve_rx_ctx {
	 
	struct sk_buff *skb_head;
	struct sk_buff *skb_tail;
	u32 total_size;
	u8 frag_cnt;
	bool drop_pkt;
};

struct gve_rx_cnts {
	u32 ok_pkt_bytes;
	u16 ok_pkt_cnt;
	u16 total_pkt_cnt;
	u16 cont_pkt_cnt;
	u16 desc_err_pkt_cnt;
};

 
struct gve_rx_ring {
	struct gve_priv *gve;
	union {
		 
		struct {
			struct gve_rx_desc_queue desc;
			struct gve_rx_data_queue data;

			 
			u32 db_threshold;
			u16 packet_buffer_size;

			u32 qpl_copy_pool_mask;
			u32 qpl_copy_pool_head;
			struct gve_rx_slot_page_info *qpl_copy_pool;
		};

		 
		struct {
			struct gve_rx_buf_queue_dqo bufq;
			struct gve_rx_compl_queue_dqo complq;

			struct gve_rx_buf_state_dqo *buf_states;
			u16 num_buf_states;

			 
			s16 free_buf_states;

			 
			struct gve_index_list recycled_buf_states;

			 
			struct gve_index_list used_buf_states;

			 
			struct gve_queue_page_list *qpl;

			 
			u32 next_qpl_page_idx;

			 
			u16 used_buf_states_cnt;
		} dqo;
	};

	u64 rbytes;  
	u64 rpackets;  
	u32 cnt;  
	u32 fill_cnt;  
	u32 mask;  
	u64 rx_copybreak_pkt;  
	u64 rx_copied_pkt;  
	u64 rx_skb_alloc_fail;  
	u64 rx_buf_alloc_fail;  
	u64 rx_desc_err_dropped_pkt;  
	u64 rx_cont_packet_cnt;  
	u64 rx_frag_flip_cnt;  
	u64 rx_frag_copy_cnt;  
	u64 rx_frag_alloc_cnt;  
	u64 xdp_tx_errors;
	u64 xdp_redirect_errors;
	u64 xdp_alloc_fails;
	u64 xdp_actions[GVE_XDP_ACTIONS];
	u32 q_num;  
	u32 ntfy_id;  
	struct gve_queue_resources *q_resources;  
	dma_addr_t q_resources_bus;  
	struct u64_stats_sync statss;  

	struct gve_rx_ctx ctx;  

	 
	struct xdp_rxq_info xdp_rxq;
	struct xdp_rxq_info xsk_rxq;
	struct xsk_buff_pool *xsk_pool;
	struct page_frag_cache page_cache;  
};

 
union gve_tx_desc {
	struct gve_tx_pkt_desc pkt;  
	struct gve_tx_mtd_desc mtd;  
	struct gve_tx_seg_desc seg;  
};

 
struct gve_tx_iovec {
	u32 iov_offset;  
	u32 iov_len;  
	u32 iov_padding;  
};

 
struct gve_tx_buffer_state {
	union {
		struct sk_buff *skb;  
		struct xdp_frame *xdp_frame;  
	};
	struct {
		u16 size;  
		u8 is_xsk;  
	} xdp;
	union {
		struct gve_tx_iovec iov[GVE_TX_MAX_IOVEC];  
		struct {
			DEFINE_DMA_UNMAP_ADDR(dma);
			DEFINE_DMA_UNMAP_LEN(len);
		};
	};
};

 
struct gve_tx_fifo {
	void *base;  
	u32 size;  
	atomic_t available;  
	u32 head;  
	struct gve_queue_page_list *qpl;  
};

 
union gve_tx_desc_dqo {
	struct gve_tx_pkt_desc_dqo pkt;
	struct gve_tx_tso_context_desc_dqo tso_ctx;
	struct gve_tx_general_context_desc_dqo general_ctx;
};

enum gve_packet_state {
	 
	GVE_PACKET_STATE_UNALLOCATED,
	 
	GVE_PACKET_STATE_PENDING_DATA_COMPL,
	 
	GVE_PACKET_STATE_PENDING_REINJECT_COMPL,
	 
	GVE_PACKET_STATE_TIMED_OUT_COMPL,
};

struct gve_tx_pending_packet_dqo {
	struct sk_buff *skb;  

	 
	union {
		struct {
			DEFINE_DMA_UNMAP_ADDR(dma[MAX_SKB_FRAGS + 1]);
			DEFINE_DMA_UNMAP_LEN(len[MAX_SKB_FRAGS + 1]);
		};
		s16 tx_qpl_buf_ids[GVE_MAX_TX_BUFS_PER_PKT];
	};

	u16 num_bufs;

	 
	s16 next;

	 
	s16 prev;

	 
	u8 state;

	 
	unsigned long timeout_jiffies;
};

 
struct gve_tx_ring {
	 
	union {
		 
		struct {
			struct gve_tx_fifo tx_fifo;
			u32 req;  
			u32 done;  
		};

		 
		struct {
			 
			s16 free_pending_packets;

			 
			u32 head;
			u32 tail;  

			 
			u32 last_re_idx;

			 
			u16 posted_packet_desc_cnt;
			 
			u16 completed_packet_desc_cnt;

			 
			struct {
			        
				s16 free_tx_qpl_buf_head;

			        
				u32 alloc_tx_qpl_buf_cnt;

				 
				u32 free_tx_qpl_buf_cnt;
			};
		} dqo_tx;
	};

	 
	union {
		 
		struct {
			 
			spinlock_t clean_lock;
			 
			spinlock_t xdp_lock;
		};

		 
		struct {
			u32 head;  

			 
			u8 cur_gen_bit;

			 
			atomic_t free_pending_packets;

			 
			atomic_t hw_tx_head;

			 
			struct gve_index_list miss_completions;

			 
			struct gve_index_list timed_out_completions;

			 
			struct {
				 
				atomic_t free_tx_qpl_buf_head;

				 
				atomic_t free_tx_qpl_buf_cnt;
			};
		} dqo_compl;
	} ____cacheline_aligned;
	u64 pkt_done;  
	u64 bytes_done;  
	u64 dropped_pkt;  
	u64 dma_mapping_error;  

	 
	union {
		 
		struct {
			union gve_tx_desc *desc;

			 
			struct gve_tx_buffer_state *info;
		};

		 
		struct {
			union gve_tx_desc_dqo *tx_ring;
			struct gve_tx_compl_desc *compl_ring;

			struct gve_tx_pending_packet_dqo *pending_packets;
			s16 num_pending_packets;

			u32 complq_mask;  

			 
			struct {
				 
				struct gve_queue_page_list *qpl;

				 
				s16 *tx_qpl_buf_next;
				u32 num_tx_qpl_bufs;
			};
		} dqo;
	} ____cacheline_aligned;
	struct netdev_queue *netdev_txq;
	struct gve_queue_resources *q_resources;  
	struct device *dev;
	u32 mask;  
	u8 raw_addressing;  

	 
	u32 q_num ____cacheline_aligned;  
	u32 stop_queue;  
	u32 wake_queue;  
	u32 queue_timeout;  
	u32 ntfy_id;  
	u32 last_kick_msec;  
	dma_addr_t bus;  
	dma_addr_t q_resources_bus;  
	dma_addr_t complq_bus_dqo;  
	struct u64_stats_sync statss;  
	struct xsk_buff_pool *xsk_pool;
	u32 xdp_xsk_wakeup;
	u32 xdp_xsk_done;
	u64 xdp_xsk_sent;
	u64 xdp_xmit;
	u64 xdp_xmit_errors;
} ____cacheline_aligned;

 
struct gve_notify_block {
	__be32 *irq_db_index;  
	char name[IFNAMSIZ + 16];  
	struct napi_struct napi;  
	struct gve_priv *priv;
	struct gve_tx_ring *tx;  
	struct gve_rx_ring *rx;  
};

 
struct gve_queue_config {
	u16 max_queues;
	u16 num_queues;  
};

 
struct gve_qpl_config {
	u32 qpl_map_size;  
	unsigned long *qpl_id_map;  
};

struct gve_options_dqo_rda {
	u16 tx_comp_ring_entries;  
	u16 rx_buff_ring_entries;  
};

struct gve_irq_db {
	__be32 index;
} ____cacheline_aligned;

struct gve_ptype {
	u8 l3_type;   
	u8 l4_type;   
};

struct gve_ptype_lut {
	struct gve_ptype ptypes[GVE_NUM_PTYPES];
};

 
enum gve_queue_format {
	GVE_QUEUE_FORMAT_UNSPECIFIED	= 0x0,
	GVE_GQI_RDA_FORMAT		= 0x1,
	GVE_GQI_QPL_FORMAT		= 0x2,
	GVE_DQO_RDA_FORMAT		= 0x3,
	GVE_DQO_QPL_FORMAT		= 0x4,
};

struct gve_priv {
	struct net_device *dev;
	struct gve_tx_ring *tx;  
	struct gve_rx_ring *rx;  
	struct gve_queue_page_list *qpls;  
	struct gve_notify_block *ntfy_blocks;  
	struct gve_irq_db *irq_db_indices;  
	dma_addr_t irq_db_indices_bus;
	struct msix_entry *msix_vectors;  
	char mgmt_msix_name[IFNAMSIZ + 16];
	u32 mgmt_msix_idx;
	__be32 *counter_array;  
	dma_addr_t counter_array_bus;

	u16 num_event_counters;
	u16 tx_desc_cnt;  
	u16 rx_desc_cnt;  
	u16 tx_pages_per_qpl;  
	u16 rx_pages_per_qpl;  
	u16 rx_data_slot_cnt;  
	u64 max_registered_pages;
	u64 num_registered_pages;  
	struct bpf_prog *xdp_prog;  
	u32 rx_copybreak;  
	u16 default_num_queues;  

	u16 num_xdp_queues;
	struct gve_queue_config tx_cfg;
	struct gve_queue_config rx_cfg;
	struct gve_qpl_config qpl_cfg;  
	u32 num_ntfy_blks;  

	struct gve_registers __iomem *reg_bar0;  
	__be32 __iomem *db_bar2;  
	u32 msg_enable;	 
	struct pci_dev *pdev;

	 
	u32 tx_timeo_cnt;

	 
	union gve_adminq_command *adminq;
	dma_addr_t adminq_bus_addr;
	u32 adminq_mask;  
	u32 adminq_prod_cnt;  
	u32 adminq_cmd_fail;  
	u32 adminq_timeouts;  
	 
	u32 adminq_describe_device_cnt;
	u32 adminq_cfg_device_resources_cnt;
	u32 adminq_register_page_list_cnt;
	u32 adminq_unregister_page_list_cnt;
	u32 adminq_create_tx_queue_cnt;
	u32 adminq_create_rx_queue_cnt;
	u32 adminq_destroy_tx_queue_cnt;
	u32 adminq_destroy_rx_queue_cnt;
	u32 adminq_dcfg_device_resources_cnt;
	u32 adminq_set_driver_parameter_cnt;
	u32 adminq_report_stats_cnt;
	u32 adminq_report_link_speed_cnt;
	u32 adminq_get_ptype_map_cnt;
	u32 adminq_verify_driver_compatibility_cnt;

	 
	u32 interface_up_cnt;  
	u32 interface_down_cnt;  
	u32 reset_cnt;  
	u32 page_alloc_fail;  
	u32 dma_mapping_error;  
	u32 stats_report_trigger_cnt;  
	u32 suspend_cnt;  
	u32 resume_cnt;  
	struct workqueue_struct *gve_wq;
	struct work_struct service_task;
	struct work_struct stats_report_task;
	unsigned long service_task_flags;
	unsigned long state_flags;

	struct gve_stats_report *stats_report;
	u64 stats_report_len;
	dma_addr_t stats_report_bus;  
	unsigned long ethtool_flags;

	unsigned long stats_report_timer_period;
	struct timer_list stats_report_timer;

	 
	u64 link_speed;
	bool up_before_suspend;  

	struct gve_options_dqo_rda options_dqo_rda;
	struct gve_ptype_lut *ptype_lut_dqo;

	 
	int data_buffer_size_dqo;

	enum gve_queue_format queue_format;

	 
	u32 tx_coalesce_usecs;
	u32 rx_coalesce_usecs;
};

enum gve_service_task_flags_bit {
	GVE_PRIV_FLAGS_DO_RESET			= 1,
	GVE_PRIV_FLAGS_RESET_IN_PROGRESS	= 2,
	GVE_PRIV_FLAGS_PROBE_IN_PROGRESS	= 3,
	GVE_PRIV_FLAGS_DO_REPORT_STATS = 4,
};

enum gve_state_flags_bit {
	GVE_PRIV_FLAGS_ADMIN_QUEUE_OK		= 1,
	GVE_PRIV_FLAGS_DEVICE_RESOURCES_OK	= 2,
	GVE_PRIV_FLAGS_DEVICE_RINGS_OK		= 3,
	GVE_PRIV_FLAGS_NAPI_ENABLED		= 4,
};

enum gve_ethtool_flags_bit {
	GVE_PRIV_FLAGS_REPORT_STATS		= 0,
};

static inline bool gve_get_do_reset(struct gve_priv *priv)
{
	return test_bit(GVE_PRIV_FLAGS_DO_RESET, &priv->service_task_flags);
}

static inline void gve_set_do_reset(struct gve_priv *priv)
{
	set_bit(GVE_PRIV_FLAGS_DO_RESET, &priv->service_task_flags);
}

static inline void gve_clear_do_reset(struct gve_priv *priv)
{
	clear_bit(GVE_PRIV_FLAGS_DO_RESET, &priv->service_task_flags);
}

static inline bool gve_get_reset_in_progress(struct gve_priv *priv)
{
	return test_bit(GVE_PRIV_FLAGS_RESET_IN_PROGRESS,
			&priv->service_task_flags);
}

static inline void gve_set_reset_in_progress(struct gve_priv *priv)
{
	set_bit(GVE_PRIV_FLAGS_RESET_IN_PROGRESS, &priv->service_task_flags);
}

static inline void gve_clear_reset_in_progress(struct gve_priv *priv)
{
	clear_bit(GVE_PRIV_FLAGS_RESET_IN_PROGRESS, &priv->service_task_flags);
}

static inline bool gve_get_probe_in_progress(struct gve_priv *priv)
{
	return test_bit(GVE_PRIV_FLAGS_PROBE_IN_PROGRESS,
			&priv->service_task_flags);
}

static inline void gve_set_probe_in_progress(struct gve_priv *priv)
{
	set_bit(GVE_PRIV_FLAGS_PROBE_IN_PROGRESS, &priv->service_task_flags);
}

static inline void gve_clear_probe_in_progress(struct gve_priv *priv)
{
	clear_bit(GVE_PRIV_FLAGS_PROBE_IN_PROGRESS, &priv->service_task_flags);
}

static inline bool gve_get_do_report_stats(struct gve_priv *priv)
{
	return test_bit(GVE_PRIV_FLAGS_DO_REPORT_STATS,
			&priv->service_task_flags);
}

static inline void gve_set_do_report_stats(struct gve_priv *priv)
{
	set_bit(GVE_PRIV_FLAGS_DO_REPORT_STATS, &priv->service_task_flags);
}

static inline void gve_clear_do_report_stats(struct gve_priv *priv)
{
	clear_bit(GVE_PRIV_FLAGS_DO_REPORT_STATS, &priv->service_task_flags);
}

static inline bool gve_get_admin_queue_ok(struct gve_priv *priv)
{
	return test_bit(GVE_PRIV_FLAGS_ADMIN_QUEUE_OK, &priv->state_flags);
}

static inline void gve_set_admin_queue_ok(struct gve_priv *priv)
{
	set_bit(GVE_PRIV_FLAGS_ADMIN_QUEUE_OK, &priv->state_flags);
}

static inline void gve_clear_admin_queue_ok(struct gve_priv *priv)
{
	clear_bit(GVE_PRIV_FLAGS_ADMIN_QUEUE_OK, &priv->state_flags);
}

static inline bool gve_get_device_resources_ok(struct gve_priv *priv)
{
	return test_bit(GVE_PRIV_FLAGS_DEVICE_RESOURCES_OK, &priv->state_flags);
}

static inline void gve_set_device_resources_ok(struct gve_priv *priv)
{
	set_bit(GVE_PRIV_FLAGS_DEVICE_RESOURCES_OK, &priv->state_flags);
}

static inline void gve_clear_device_resources_ok(struct gve_priv *priv)
{
	clear_bit(GVE_PRIV_FLAGS_DEVICE_RESOURCES_OK, &priv->state_flags);
}

static inline bool gve_get_device_rings_ok(struct gve_priv *priv)
{
	return test_bit(GVE_PRIV_FLAGS_DEVICE_RINGS_OK, &priv->state_flags);
}

static inline void gve_set_device_rings_ok(struct gve_priv *priv)
{
	set_bit(GVE_PRIV_FLAGS_DEVICE_RINGS_OK, &priv->state_flags);
}

static inline void gve_clear_device_rings_ok(struct gve_priv *priv)
{
	clear_bit(GVE_PRIV_FLAGS_DEVICE_RINGS_OK, &priv->state_flags);
}

static inline bool gve_get_napi_enabled(struct gve_priv *priv)
{
	return test_bit(GVE_PRIV_FLAGS_NAPI_ENABLED, &priv->state_flags);
}

static inline void gve_set_napi_enabled(struct gve_priv *priv)
{
	set_bit(GVE_PRIV_FLAGS_NAPI_ENABLED, &priv->state_flags);
}

static inline void gve_clear_napi_enabled(struct gve_priv *priv)
{
	clear_bit(GVE_PRIV_FLAGS_NAPI_ENABLED, &priv->state_flags);
}

static inline bool gve_get_report_stats(struct gve_priv *priv)
{
	return test_bit(GVE_PRIV_FLAGS_REPORT_STATS, &priv->ethtool_flags);
}

static inline void gve_clear_report_stats(struct gve_priv *priv)
{
	clear_bit(GVE_PRIV_FLAGS_REPORT_STATS, &priv->ethtool_flags);
}

 
static inline __be32 __iomem *gve_irq_doorbell(struct gve_priv *priv,
					       struct gve_notify_block *block)
{
	return &priv->db_bar2[be32_to_cpu(*block->irq_db_index)];
}

 
static inline u32 gve_tx_idx_to_ntfy(struct gve_priv *priv, u32 queue_idx)
{
	return queue_idx;
}

 
static inline u32 gve_rx_idx_to_ntfy(struct gve_priv *priv, u32 queue_idx)
{
	return (priv->num_ntfy_blks / 2) + queue_idx;
}

static inline bool gve_is_qpl(struct gve_priv *priv)
{
	return priv->queue_format == GVE_GQI_QPL_FORMAT ||
		priv->queue_format == GVE_DQO_QPL_FORMAT;
}

 
static inline u32 gve_num_tx_qpls(struct gve_priv *priv)
{
	if (!gve_is_qpl(priv))
		return 0;

	return priv->tx_cfg.num_queues + priv->num_xdp_queues;
}

 
static inline u32 gve_num_xdp_qpls(struct gve_priv *priv)
{
	if (priv->queue_format != GVE_GQI_QPL_FORMAT)
		return 0;

	return priv->num_xdp_queues;
}

 
static inline u32 gve_num_rx_qpls(struct gve_priv *priv)
{
	if (!gve_is_qpl(priv))
		return 0;

	return priv->rx_cfg.num_queues;
}

static inline u32 gve_tx_qpl_id(struct gve_priv *priv, int tx_qid)
{
	return tx_qid;
}

static inline u32 gve_rx_qpl_id(struct gve_priv *priv, int rx_qid)
{
	return priv->tx_cfg.max_queues + rx_qid;
}

static inline u32 gve_tx_start_qpl_id(struct gve_priv *priv)
{
	return gve_tx_qpl_id(priv, 0);
}

static inline u32 gve_rx_start_qpl_id(struct gve_priv *priv)
{
	return gve_rx_qpl_id(priv, 0);
}

 
static inline
struct gve_queue_page_list *gve_assign_tx_qpl(struct gve_priv *priv, int tx_qid)
{
	int id = gve_tx_qpl_id(priv, tx_qid);

	 
	if (test_bit(id, priv->qpl_cfg.qpl_id_map))
		return NULL;

	set_bit(id, priv->qpl_cfg.qpl_id_map);
	return &priv->qpls[id];
}

 
static inline
struct gve_queue_page_list *gve_assign_rx_qpl(struct gve_priv *priv, int rx_qid)
{
	int id = gve_rx_qpl_id(priv, rx_qid);

	 
	if (test_bit(id, priv->qpl_cfg.qpl_id_map))
		return NULL;

	set_bit(id, priv->qpl_cfg.qpl_id_map);
	return &priv->qpls[id];
}

 
static inline void gve_unassign_qpl(struct gve_priv *priv, int id)
{
	clear_bit(id, priv->qpl_cfg.qpl_id_map);
}

 
static inline enum dma_data_direction gve_qpl_dma_dir(struct gve_priv *priv,
						      int id)
{
	if (id < gve_rx_start_qpl_id(priv))
		return DMA_TO_DEVICE;
	else
		return DMA_FROM_DEVICE;
}

static inline bool gve_is_gqi(struct gve_priv *priv)
{
	return priv->queue_format == GVE_GQI_RDA_FORMAT ||
		priv->queue_format == GVE_GQI_QPL_FORMAT;
}

static inline u32 gve_num_tx_queues(struct gve_priv *priv)
{
	return priv->tx_cfg.num_queues + priv->num_xdp_queues;
}

static inline u32 gve_xdp_tx_queue_id(struct gve_priv *priv, u32 queue_id)
{
	return priv->tx_cfg.num_queues + queue_id;
}

static inline u32 gve_xdp_tx_start_queue_id(struct gve_priv *priv)
{
	return gve_xdp_tx_queue_id(priv, 0);
}

 
int gve_alloc_page(struct gve_priv *priv, struct device *dev,
		   struct page **page, dma_addr_t *dma,
		   enum dma_data_direction, gfp_t gfp_flags);
void gve_free_page(struct device *dev, struct page *page, dma_addr_t dma,
		   enum dma_data_direction);
 
netdev_tx_t gve_tx(struct sk_buff *skb, struct net_device *dev);
int gve_xdp_xmit(struct net_device *dev, int n, struct xdp_frame **frames,
		 u32 flags);
int gve_xdp_xmit_one(struct gve_priv *priv, struct gve_tx_ring *tx,
		     void *data, int len, void *frame_p);
void gve_xdp_tx_flush(struct gve_priv *priv, u32 xdp_qid);
bool gve_tx_poll(struct gve_notify_block *block, int budget);
bool gve_xdp_poll(struct gve_notify_block *block, int budget);
int gve_tx_alloc_rings(struct gve_priv *priv, int start_id, int num_rings);
void gve_tx_free_rings_gqi(struct gve_priv *priv, int start_id, int num_rings);
u32 gve_tx_load_event_counter(struct gve_priv *priv,
			      struct gve_tx_ring *tx);
bool gve_tx_clean_pending(struct gve_priv *priv, struct gve_tx_ring *tx);
 
void gve_rx_write_doorbell(struct gve_priv *priv, struct gve_rx_ring *rx);
int gve_rx_poll(struct gve_notify_block *block, int budget);
bool gve_rx_work_pending(struct gve_rx_ring *rx);
int gve_rx_alloc_rings(struct gve_priv *priv);
void gve_rx_free_rings_gqi(struct gve_priv *priv);
 
void gve_schedule_reset(struct gve_priv *priv);
int gve_reset(struct gve_priv *priv, bool attempt_teardown);
int gve_adjust_queues(struct gve_priv *priv,
		      struct gve_queue_config new_rx_config,
		      struct gve_queue_config new_tx_config);
 
void gve_handle_report_stats(struct gve_priv *priv);
 
extern const struct ethtool_ops gve_ethtool_ops;
 
extern char gve_driver_name[];
extern const char gve_version_str[];
#endif  
