#ifndef ENA_COM
#define ENA_COM
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/gfp.h>
#include <linux/io.h>
#include <linux/prefetch.h>
#include <linux/sched.h>
#include <linux/sizes.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/netdevice.h>
#include "ena_common_defs.h"
#include "ena_admin_defs.h"
#include "ena_eth_io_defs.h"
#include "ena_regs_defs.h"
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#define ENA_MAX_NUM_IO_QUEUES 128U
#define ENA_TOTAL_NUM_QUEUES (2 * (ENA_MAX_NUM_IO_QUEUES))
#define ENA_MAX_HANDLERS 256
#define ENA_MAX_PHYS_ADDR_SIZE_BITS 48
#define ENA_REG_READ_TIMEOUT 200000
#define ADMIN_SQ_SIZE(depth)	((depth) * sizeof(struct ena_admin_aq_entry))
#define ADMIN_CQ_SIZE(depth)	((depth) * sizeof(struct ena_admin_acq_entry))
#define ADMIN_AENQ_SIZE(depth)	((depth) * sizeof(struct ena_admin_aenq_entry))
#define ENA_INTR_INITIAL_TX_INTERVAL_USECS 64
#define ENA_INTR_INITIAL_RX_INTERVAL_USECS 0
#define ENA_DEFAULT_INTR_DELAY_RESOLUTION 1
#define ENA_HASH_KEY_SIZE 40
#define ENA_HW_HINTS_NO_TIMEOUT	0xFFFF
#define ENA_FEATURE_MAX_QUEUE_EXT_VER 1
struct ena_llq_configurations {
	enum ena_admin_llq_header_location llq_header_location;
	enum ena_admin_llq_ring_entry_size llq_ring_entry_size;
	enum ena_admin_llq_stride_ctrl  llq_stride_ctrl;
	enum ena_admin_llq_num_descs_before_header llq_num_decs_before_header;
	u16 llq_ring_entry_size_value;
};
enum queue_direction {
	ENA_COM_IO_QUEUE_DIRECTION_TX,
	ENA_COM_IO_QUEUE_DIRECTION_RX
};
struct ena_com_buf {
	dma_addr_t paddr;  
	u16 len;  
};
struct ena_com_rx_buf_info {
	u16 len;
	u16 req_id;
};
struct ena_com_io_desc_addr {
	u8 __iomem *pbuf_dev_addr;  
	u8 *virt_addr;
	dma_addr_t phys_addr;
};
struct ena_com_tx_meta {
	u16 mss;
	u16 l3_hdr_len;
	u16 l3_hdr_offset;
	u16 l4_hdr_len;  
};
struct ena_com_llq_info {
	u16 header_location_ctrl;
	u16 desc_stride_ctrl;
	u16 desc_list_entry_size_ctrl;
	u16 desc_list_entry_size;
	u16 descs_num_before_header;
	u16 descs_per_entry;
	u16 max_entries_in_tx_burst;
	bool disable_meta_caching;
};
struct ena_com_io_cq {
	struct ena_com_io_desc_addr cdesc_addr;
	u32 __iomem *unmask_reg;
	u32 __iomem *cq_head_db_reg;
	u32 __iomem *numa_node_cfg_reg;
	u32 msix_vector;
	enum queue_direction direction;
	u16 cur_rx_pkt_cdesc_count;
	u16 cur_rx_pkt_cdesc_start_idx;
	u16 q_depth;
	u16 qid;
	u16 idx;
	u16 head;
	u16 last_head_update;
	u8 phase;
	u8 cdesc_entry_size_in_bytes;
} ____cacheline_aligned;
struct ena_com_io_bounce_buffer_control {
	u8 *base_buffer;
	u16 next_to_use;
	u16 buffer_size;
	u16 buffers_num;   
};
struct ena_com_llq_pkt_ctrl {
	u8 *curr_bounce_buf;
	u16 idx;
	u16 descs_left_in_line;
};
struct ena_com_io_sq {
	struct ena_com_io_desc_addr desc_addr;
	u32 __iomem *db_addr;
	u8 __iomem *header_addr;
	enum queue_direction direction;
	enum ena_admin_placement_policy_type mem_queue_type;
	bool disable_meta_caching;
	u32 msix_vector;
	struct ena_com_tx_meta cached_tx_meta;
	struct ena_com_llq_info llq_info;
	struct ena_com_llq_pkt_ctrl llq_buf_ctrl;
	struct ena_com_io_bounce_buffer_control bounce_buf_ctrl;
	u16 q_depth;
	u16 qid;
	u16 idx;
	u16 tail;
	u16 next_to_comp;
	u16 llq_last_copy_tail;
	u32 tx_max_header_size;
	u8 phase;
	u8 desc_entry_size;
	u8 dma_addr_bits;
	u16 entries_in_tx_burst_left;
} ____cacheline_aligned;
struct ena_com_admin_cq {
	struct ena_admin_acq_entry *entries;
	dma_addr_t dma_addr;
	u16 head;
	u8 phase;
};
struct ena_com_admin_sq {
	struct ena_admin_aq_entry *entries;
	dma_addr_t dma_addr;
	u32 __iomem *db_addr;
	u16 head;
	u16 tail;
	u8 phase;
};
struct ena_com_stats_admin {
	u64 aborted_cmd;
	u64 submitted_cmd;
	u64 completed_cmd;
	u64 out_of_space;
	u64 no_completion;
};
struct ena_com_admin_queue {
	void *q_dmadev;
	struct ena_com_dev *ena_dev;
	spinlock_t q_lock;  
	struct ena_comp_ctx *comp_ctx;
	u32 completion_timeout;
	u16 q_depth;
	struct ena_com_admin_cq cq;
	struct ena_com_admin_sq sq;
	bool polling;
	bool auto_polling;
	u16 curr_cmd_id;
	bool running_state;
	atomic_t outstanding_cmds;
	struct ena_com_stats_admin stats;
};
struct ena_aenq_handlers;
struct ena_com_aenq {
	u16 head;
	u8 phase;
	struct ena_admin_aenq_entry *entries;
	dma_addr_t dma_addr;
	u16 q_depth;
	struct ena_aenq_handlers *aenq_handlers;
};
struct ena_com_mmio_read {
	struct ena_admin_ena_mmio_req_read_less_resp *read_resp;
	dma_addr_t read_resp_dma_addr;
	u32 reg_read_to;  
	u16 seq_num;
	bool readless_supported;
	spinlock_t lock;
};
struct ena_rss {
	u16 *host_rss_ind_tbl;
	struct ena_admin_rss_ind_table_entry *rss_ind_tbl;
	dma_addr_t rss_ind_tbl_dma_addr;
	u16 tbl_log_size;
	enum ena_admin_hash_functions hash_func;
	struct ena_admin_feature_rss_flow_hash_control *hash_key;
	dma_addr_t hash_key_dma_addr;
	u32 hash_init_val;
	struct ena_admin_feature_rss_hash_control *hash_ctrl;
	dma_addr_t hash_ctrl_dma_addr;
};
struct ena_host_attribute {
	u8 *debug_area_virt_addr;
	dma_addr_t debug_area_dma_addr;
	u32 debug_area_size;
	struct ena_admin_host_info *host_info;
	dma_addr_t host_info_dma_addr;
};
struct ena_com_dev {
	struct ena_com_admin_queue admin_queue;
	struct ena_com_aenq aenq;
	struct ena_com_io_cq io_cq_queues[ENA_TOTAL_NUM_QUEUES];
	struct ena_com_io_sq io_sq_queues[ENA_TOTAL_NUM_QUEUES];
	u8 __iomem *reg_bar;
	void __iomem *mem_bar;
	void *dmadev;
	struct net_device *net_device;
	enum ena_admin_placement_policy_type tx_mem_queue_type;
	u32 tx_max_header_size;
	u16 stats_func;  
	u16 stats_queue;  
	struct ena_com_mmio_read mmio_read;
	struct ena_rss rss;
	u32 supported_features;
	u32 capabilities;
	u32 dma_addr_bits;
	struct ena_host_attribute host_attr;
	bool adaptive_coalescing;
	u16 intr_delay_resolution;
	u32 intr_moder_tx_interval;
	u32 intr_moder_rx_interval;
	struct ena_intr_moder_entry *intr_moder_tbl;
	struct ena_com_llq_info llq_info;
	u32 ena_min_poll_delay_us;
};
struct ena_com_dev_get_features_ctx {
	struct ena_admin_queue_feature_desc max_queues;
	struct ena_admin_queue_ext_feature_desc max_queue_ext;
	struct ena_admin_device_attr_feature_desc dev_attr;
	struct ena_admin_feature_aenq_desc aenq;
	struct ena_admin_feature_offload_desc offload;
	struct ena_admin_ena_hw_hints hw_hints;
	struct ena_admin_feature_llq_desc llq;
};
struct ena_com_create_io_ctx {
	enum ena_admin_placement_policy_type mem_queue_type;
	enum queue_direction direction;
	int numa_node;
	u32 msix_vector;
	u16 queue_size;
	u16 qid;
};
typedef void (*ena_aenq_handler)(void *data,
	struct ena_admin_aenq_entry *aenq_e);
struct ena_aenq_handlers {
	ena_aenq_handler handlers[ENA_MAX_HANDLERS];
	ena_aenq_handler unimplemented_handler;
};
int ena_com_mmio_reg_read_request_init(struct ena_com_dev *ena_dev);
void ena_com_set_mmio_read_mode(struct ena_com_dev *ena_dev,
				bool readless_supported);
void ena_com_mmio_reg_read_request_write_dev_addr(struct ena_com_dev *ena_dev);
void ena_com_mmio_reg_read_request_destroy(struct ena_com_dev *ena_dev);
int ena_com_admin_init(struct ena_com_dev *ena_dev,
		       struct ena_aenq_handlers *aenq_handlers);
void ena_com_admin_destroy(struct ena_com_dev *ena_dev);
int ena_com_dev_reset(struct ena_com_dev *ena_dev,
		      enum ena_regs_reset_reason_types reset_reason);
int ena_com_create_io_queue(struct ena_com_dev *ena_dev,
			    struct ena_com_create_io_ctx *ctx);
void ena_com_destroy_io_queue(struct ena_com_dev *ena_dev, u16 qid);
int ena_com_get_io_handlers(struct ena_com_dev *ena_dev, u16 qid,
			    struct ena_com_io_sq **io_sq,
			    struct ena_com_io_cq **io_cq);
void ena_com_admin_aenq_enable(struct ena_com_dev *ena_dev);
void ena_com_set_admin_running_state(struct ena_com_dev *ena_dev, bool state);
bool ena_com_get_admin_running_state(struct ena_com_dev *ena_dev);
void ena_com_set_admin_polling_mode(struct ena_com_dev *ena_dev, bool polling);
void ena_com_set_admin_auto_polling_mode(struct ena_com_dev *ena_dev,
					 bool polling);
void ena_com_admin_q_comp_intr_handler(struct ena_com_dev *ena_dev);
void ena_com_aenq_intr_handler(struct ena_com_dev *ena_dev, void *data);
void ena_com_abort_admin_commands(struct ena_com_dev *ena_dev);
void ena_com_wait_for_abort_completion(struct ena_com_dev *ena_dev);
int ena_com_validate_version(struct ena_com_dev *ena_dev);
int ena_com_get_link_params(struct ena_com_dev *ena_dev,
			    struct ena_admin_get_feat_resp *resp);
int ena_com_get_dma_width(struct ena_com_dev *ena_dev);
int ena_com_set_aenq_config(struct ena_com_dev *ena_dev, u32 groups_flag);
int ena_com_get_dev_attr_feat(struct ena_com_dev *ena_dev,
			      struct ena_com_dev_get_features_ctx *get_feat_ctx);
int ena_com_get_dev_basic_stats(struct ena_com_dev *ena_dev,
				struct ena_admin_basic_stats *stats);
int ena_com_get_eni_stats(struct ena_com_dev *ena_dev,
			  struct ena_admin_eni_stats *stats);
int ena_com_set_dev_mtu(struct ena_com_dev *ena_dev, u32 mtu);
int ena_com_get_offload_settings(struct ena_com_dev *ena_dev,
				 struct ena_admin_feature_offload_desc *offload);
int ena_com_rss_init(struct ena_com_dev *ena_dev, u16 log_size);
void ena_com_rss_destroy(struct ena_com_dev *ena_dev);
int ena_com_get_current_hash_function(struct ena_com_dev *ena_dev);
int ena_com_fill_hash_function(struct ena_com_dev *ena_dev,
			       enum ena_admin_hash_functions func,
			       const u8 *key, u16 key_len, u32 init_val);
int ena_com_set_hash_function(struct ena_com_dev *ena_dev);
int ena_com_get_hash_function(struct ena_com_dev *ena_dev,
			      enum ena_admin_hash_functions *func);
int ena_com_get_hash_key(struct ena_com_dev *ena_dev, u8 *key);
int ena_com_fill_hash_ctrl(struct ena_com_dev *ena_dev,
			   enum ena_admin_flow_hash_proto proto,
			   u16 hash_fields);
int ena_com_set_hash_ctrl(struct ena_com_dev *ena_dev);
int ena_com_get_hash_ctrl(struct ena_com_dev *ena_dev,
			  enum ena_admin_flow_hash_proto proto,
			  u16 *fields);
int ena_com_set_default_hash_ctrl(struct ena_com_dev *ena_dev);
int ena_com_indirect_table_fill_entry(struct ena_com_dev *ena_dev,
				      u16 entry_idx, u16 entry_value);
int ena_com_indirect_table_set(struct ena_com_dev *ena_dev);
int ena_com_indirect_table_get(struct ena_com_dev *ena_dev, u32 *ind_tbl);
int ena_com_allocate_host_info(struct ena_com_dev *ena_dev);
int ena_com_allocate_debug_area(struct ena_com_dev *ena_dev,
				u32 debug_area_size);
void ena_com_delete_debug_area(struct ena_com_dev *ena_dev);
void ena_com_delete_host_info(struct ena_com_dev *ena_dev);
int ena_com_set_host_attributes(struct ena_com_dev *ena_dev);
int ena_com_create_io_cq(struct ena_com_dev *ena_dev,
			 struct ena_com_io_cq *io_cq);
int ena_com_destroy_io_cq(struct ena_com_dev *ena_dev,
			  struct ena_com_io_cq *io_cq);
int ena_com_execute_admin_command(struct ena_com_admin_queue *admin_queue,
				  struct ena_admin_aq_entry *cmd,
				  size_t cmd_size,
				  struct ena_admin_acq_entry *cmd_comp,
				  size_t cmd_comp_size);
int ena_com_init_interrupt_moderation(struct ena_com_dev *ena_dev);
bool ena_com_interrupt_moderation_supported(struct ena_com_dev *ena_dev);
int ena_com_update_nonadaptive_moderation_interval_tx(struct ena_com_dev *ena_dev,
						      u32 tx_coalesce_usecs);
int ena_com_update_nonadaptive_moderation_interval_rx(struct ena_com_dev *ena_dev,
						      u32 rx_coalesce_usecs);
unsigned int ena_com_get_nonadaptive_moderation_interval_tx(struct ena_com_dev *ena_dev);
unsigned int ena_com_get_nonadaptive_moderation_interval_rx(struct ena_com_dev *ena_dev);
int ena_com_config_dev_mode(struct ena_com_dev *ena_dev,
			    struct ena_admin_feature_llq_desc *llq_features,
			    struct ena_llq_configurations *llq_default_config);
static inline struct ena_com_dev *ena_com_io_sq_to_ena_dev(struct ena_com_io_sq *io_sq)
{
	return container_of(io_sq, struct ena_com_dev, io_sq_queues[io_sq->qid]);
}
static inline struct ena_com_dev *ena_com_io_cq_to_ena_dev(struct ena_com_io_cq *io_cq)
{
	return container_of(io_cq, struct ena_com_dev, io_cq_queues[io_cq->qid]);
}
static inline bool ena_com_get_adaptive_moderation_enabled(struct ena_com_dev *ena_dev)
{
	return ena_dev->adaptive_coalescing;
}
static inline void ena_com_enable_adaptive_moderation(struct ena_com_dev *ena_dev)
{
	ena_dev->adaptive_coalescing = true;
}
static inline void ena_com_disable_adaptive_moderation(struct ena_com_dev *ena_dev)
{
	ena_dev->adaptive_coalescing = false;
}
static inline bool ena_com_get_cap(struct ena_com_dev *ena_dev,
				   enum ena_admin_aq_caps_id cap_id)
{
	return !!(ena_dev->capabilities & BIT(cap_id));
}
static inline void ena_com_update_intr_reg(struct ena_eth_io_intr_reg *intr_reg,
					   u32 rx_delay_interval,
					   u32 tx_delay_interval,
					   bool unmask)
{
	intr_reg->intr_control = 0;
	intr_reg->intr_control |= rx_delay_interval &
		ENA_ETH_IO_INTR_REG_RX_INTR_DELAY_MASK;
	intr_reg->intr_control |=
		(tx_delay_interval << ENA_ETH_IO_INTR_REG_TX_INTR_DELAY_SHIFT)
		& ENA_ETH_IO_INTR_REG_TX_INTR_DELAY_MASK;
	if (unmask)
		intr_reg->intr_control |= ENA_ETH_IO_INTR_REG_INTR_UNMASK_MASK;
}
static inline u8 *ena_com_get_next_bounce_buffer(struct ena_com_io_bounce_buffer_control *bounce_buf_ctrl)
{
	u16 size, buffers_num;
	u8 *buf;
	size = bounce_buf_ctrl->buffer_size;
	buffers_num = bounce_buf_ctrl->buffers_num;
	buf = bounce_buf_ctrl->base_buffer +
		(bounce_buf_ctrl->next_to_use++ & (buffers_num - 1)) * size;
	prefetchw(bounce_buf_ctrl->base_buffer +
		(bounce_buf_ctrl->next_to_use & (buffers_num - 1)) * size);
	return buf;
}
#endif  
