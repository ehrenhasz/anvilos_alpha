#ifndef __iwl_trans_h__
#define __iwl_trans_h__
#include <linux/ieee80211.h>
#include <linux/mm.h>  
#include <linux/lockdep.h>
#include <linux/kernel.h>
#include "iwl-debug.h"
#include "iwl-config.h"
#include "fw/img.h"
#include "iwl-op-mode.h"
#include <linux/firmware.h>
#include "fw/api/cmdhdr.h"
#include "fw/api/txq.h"
#include "fw/api/dbg-tlv.h"
#include "iwl-dbg-tlv.h"
#define IWL_FW_DBG_DOMAIN_POS	16
#define IWL_FW_DBG_DOMAIN	BIT(IWL_FW_DBG_DOMAIN_POS)
#define IWL_TRANS_FW_DBG_DOMAIN(trans)	IWL_FW_INI_DOMAIN_ALWAYS_ON
#define FH_RSCSR_FRAME_SIZE_MSK		0x00003FFF	 
#define FH_RSCSR_FRAME_INVALID		0x55550000
#define FH_RSCSR_FRAME_ALIGN		0x40
#define FH_RSCSR_RPA_EN			BIT(25)
#define FH_RSCSR_RADA_EN		BIT(26)
#define FH_RSCSR_RXQ_POS		16
#define FH_RSCSR_RXQ_MASK		0x3F0000
struct iwl_rx_packet {
	__le32 len_n_flags;
	struct iwl_cmd_header hdr;
	u8 data[];
} __packed;
static inline u32 iwl_rx_packet_len(const struct iwl_rx_packet *pkt)
{
	return le32_to_cpu(pkt->len_n_flags) & FH_RSCSR_FRAME_SIZE_MSK;
}
static inline u32 iwl_rx_packet_payload_len(const struct iwl_rx_packet *pkt)
{
	return iwl_rx_packet_len(pkt) - sizeof(pkt->hdr);
}
enum CMD_MODE {
	CMD_ASYNC		= BIT(0),
	CMD_WANT_SKB		= BIT(1),
	CMD_SEND_IN_RFKILL	= BIT(2),
	CMD_WANT_ASYNC_CALLBACK	= BIT(3),
	CMD_SEND_IN_D3          = BIT(4),
};
#define DEF_CMD_PAYLOAD_SIZE 320
struct iwl_device_cmd {
	union {
		struct {
			struct iwl_cmd_header hdr;	 
			u8 payload[DEF_CMD_PAYLOAD_SIZE];
		};
		struct {
			struct iwl_cmd_header_wide hdr_wide;
			u8 payload_wide[DEF_CMD_PAYLOAD_SIZE -
					sizeof(struct iwl_cmd_header_wide) +
					sizeof(struct iwl_cmd_header)];
		};
	};
} __packed;
struct iwl_device_tx_cmd {
	struct iwl_cmd_header hdr;
	u8 payload[];
} __packed;
#define TFD_MAX_PAYLOAD_SIZE (sizeof(struct iwl_device_cmd))
#define IWL_MAX_CMD_TBS_PER_TFD	2
#define IWL_TRANS_MAX_FRAGS(trans) ((trans)->txqs.tfd.max_tbs - 3)
enum iwl_hcmd_dataflag {
	IWL_HCMD_DFL_NOCOPY	= BIT(0),
	IWL_HCMD_DFL_DUP	= BIT(1),
};
enum iwl_error_event_table_status {
	IWL_ERROR_EVENT_TABLE_LMAC1 = BIT(0),
	IWL_ERROR_EVENT_TABLE_LMAC2 = BIT(1),
	IWL_ERROR_EVENT_TABLE_UMAC = BIT(2),
	IWL_ERROR_EVENT_TABLE_TCM1 = BIT(3),
	IWL_ERROR_EVENT_TABLE_TCM2 = BIT(4),
	IWL_ERROR_EVENT_TABLE_RCM1 = BIT(5),
	IWL_ERROR_EVENT_TABLE_RCM2 = BIT(6),
};
struct iwl_host_cmd {
	const void *data[IWL_MAX_CMD_TBS_PER_TFD];
	struct iwl_rx_packet *resp_pkt;
	unsigned long _rx_page_addr;
	u32 _rx_page_order;
	u32 flags;
	u32 id;
	u16 len[IWL_MAX_CMD_TBS_PER_TFD];
	u8 dataflags[IWL_MAX_CMD_TBS_PER_TFD];
};
static inline void iwl_free_resp(struct iwl_host_cmd *cmd)
{
	free_pages(cmd->_rx_page_addr, cmd->_rx_page_order);
}
struct iwl_rx_cmd_buffer {
	struct page *_page;
	int _offset;
	bool _page_stolen;
	u32 _rx_page_order;
	unsigned int truesize;
};
static inline void *rxb_addr(struct iwl_rx_cmd_buffer *r)
{
	return (void *)((unsigned long)page_address(r->_page) + r->_offset);
}
static inline int rxb_offset(struct iwl_rx_cmd_buffer *r)
{
	return r->_offset;
}
static inline struct page *rxb_steal_page(struct iwl_rx_cmd_buffer *r)
{
	r->_page_stolen = true;
	get_page(r->_page);
	return r->_page;
}
static inline void iwl_free_rxb(struct iwl_rx_cmd_buffer *r)
{
	__free_pages(r->_page, r->_rx_page_order);
}
#define MAX_NO_RECLAIM_CMDS	6
#define IWL_MASK(lo, hi) ((1 << (hi)) | ((1 << (hi)) - (1 << (lo))))
#define IWL_MAX_HW_QUEUES		32
#define IWL_MAX_TVQM_QUEUES		512
#define IWL_MAX_TID_COUNT	8
#define IWL_MGMT_TID		15
#define IWL_FRAME_LIMIT	64
#define IWL_MAX_RX_HW_QUEUES	16
#define IWL_9000_MAX_RX_HW_QUEUES	6
enum iwl_d3_status {
	IWL_D3_STATUS_ALIVE,
	IWL_D3_STATUS_RESET,
};
enum iwl_trans_status {
	STATUS_SYNC_HCMD_ACTIVE,
	STATUS_DEVICE_ENABLED,
	STATUS_TPOWER_PMI,
	STATUS_INT_ENABLED,
	STATUS_RFKILL_HW,
	STATUS_RFKILL_OPMODE,
	STATUS_FW_ERROR,
	STATUS_TRANS_GOING_IDLE,
	STATUS_TRANS_IDLE,
	STATUS_TRANS_DEAD,
	STATUS_SUPPRESS_CMD_ERROR_ONCE,
};
static inline int
iwl_trans_get_rb_size_order(enum iwl_amsdu_size rb_size)
{
	switch (rb_size) {
	case IWL_AMSDU_2K:
		return get_order(2 * 1024);
	case IWL_AMSDU_4K:
		return get_order(4 * 1024);
	case IWL_AMSDU_8K:
		return get_order(8 * 1024);
	case IWL_AMSDU_12K:
		return get_order(16 * 1024);
	default:
		WARN_ON(1);
		return -1;
	}
}
static inline int
iwl_trans_get_rb_size(enum iwl_amsdu_size rb_size)
{
	switch (rb_size) {
	case IWL_AMSDU_2K:
		return 2 * 1024;
	case IWL_AMSDU_4K:
		return 4 * 1024;
	case IWL_AMSDU_8K:
		return 8 * 1024;
	case IWL_AMSDU_12K:
		return 16 * 1024;
	default:
		WARN_ON(1);
		return 0;
	}
}
struct iwl_hcmd_names {
	u8 cmd_id;
	const char *const cmd_name;
};
#define HCMD_NAME(x)	\
	{ .cmd_id = x, .cmd_name = #x }
struct iwl_hcmd_arr {
	const struct iwl_hcmd_names *arr;
	int size;
};
#define HCMD_ARR(x)	\
	{ .arr = x, .size = ARRAY_SIZE(x) }
struct iwl_dump_sanitize_ops {
	void (*frob_txf)(void *ctx, void *buf, size_t buflen);
	void (*frob_hcmd)(void *ctx, void *hcmd, size_t buflen);
	void (*frob_mem)(void *ctx, u32 mem_addr, void *mem, size_t buflen);
};
struct iwl_trans_config {
	struct iwl_op_mode *op_mode;
	u8 cmd_queue;
	u8 cmd_fifo;
	unsigned int cmd_q_wdg_timeout;
	const u8 *no_reclaim_cmds;
	unsigned int n_no_reclaim_cmds;
	enum iwl_amsdu_size rx_buf_size;
	bool bc_table_dword;
	bool scd_set_active;
	const struct iwl_hcmd_arr *command_groups;
	int command_groups_size;
	u8 cb_data_offs;
	bool fw_reset_handshake;
	u8 queue_alloc_cmd_ver;
};
struct iwl_trans_dump_data {
	u32 len;
	u8 data[];
};
struct iwl_trans;
struct iwl_trans_txq_scd_cfg {
	u8 fifo;
	u8 sta_id;
	u8 tid;
	bool aggregate;
	int frame_limit;
};
struct iwl_trans_rxq_dma_data {
	u64 fr_bd_cb;
	u32 fr_bd_wid;
	u64 urbd_stts_wrptr;
	u64 ur_bd_cb;
};
#define IPC_DRAM_MAP_ENTRY_NUM_MAX 64
struct iwl_pnvm_image {
	struct {
		const void *data;
		u32 len;
	} chunks[IPC_DRAM_MAP_ENTRY_NUM_MAX];
	u32 n_chunks;
	u32 version;
};
struct iwl_trans_ops {
	int (*start_hw)(struct iwl_trans *iwl_trans);
	void (*op_mode_leave)(struct iwl_trans *iwl_trans);
	int (*start_fw)(struct iwl_trans *trans, const struct fw_img *fw,
			bool run_in_rfkill);
	void (*fw_alive)(struct iwl_trans *trans, u32 scd_addr);
	void (*stop_device)(struct iwl_trans *trans);
	int (*d3_suspend)(struct iwl_trans *trans, bool test, bool reset);
	int (*d3_resume)(struct iwl_trans *trans, enum iwl_d3_status *status,
			 bool test, bool reset);
	int (*send_cmd)(struct iwl_trans *trans, struct iwl_host_cmd *cmd);
	int (*tx)(struct iwl_trans *trans, struct sk_buff *skb,
		  struct iwl_device_tx_cmd *dev_cmd, int queue);
	void (*reclaim)(struct iwl_trans *trans, int queue, int ssn,
			struct sk_buff_head *skbs, bool is_flush);
	void (*set_q_ptrs)(struct iwl_trans *trans, int queue, int ptr);
	bool (*txq_enable)(struct iwl_trans *trans, int queue, u16 ssn,
			   const struct iwl_trans_txq_scd_cfg *cfg,
			   unsigned int queue_wdg_timeout);
	void (*txq_disable)(struct iwl_trans *trans, int queue,
			    bool configure_scd);
	int (*txq_alloc)(struct iwl_trans *trans, u32 flags,
			 u32 sta_mask, u8 tid,
			 int size, unsigned int queue_wdg_timeout);
	void (*txq_free)(struct iwl_trans *trans, int queue);
	int (*rxq_dma_data)(struct iwl_trans *trans, int queue,
			    struct iwl_trans_rxq_dma_data *data);
	void (*txq_set_shared_mode)(struct iwl_trans *trans, u32 txq_id,
				    bool shared);
	int (*wait_tx_queues_empty)(struct iwl_trans *trans, u32 txq_bm);
	int (*wait_txq_empty)(struct iwl_trans *trans, int queue);
	void (*freeze_txq_timer)(struct iwl_trans *trans, unsigned long txqs,
				 bool freeze);
	void (*block_txq_ptrs)(struct iwl_trans *trans, bool block);
	void (*write8)(struct iwl_trans *trans, u32 ofs, u8 val);
	void (*write32)(struct iwl_trans *trans, u32 ofs, u32 val);
	u32 (*read32)(struct iwl_trans *trans, u32 ofs);
	u32 (*read_prph)(struct iwl_trans *trans, u32 ofs);
	void (*write_prph)(struct iwl_trans *trans, u32 ofs, u32 val);
	int (*read_mem)(struct iwl_trans *trans, u32 addr,
			void *buf, int dwords);
	int (*write_mem)(struct iwl_trans *trans, u32 addr,
			 const void *buf, int dwords);
	int (*read_config32)(struct iwl_trans *trans, u32 ofs, u32 *val);
	void (*configure)(struct iwl_trans *trans,
			  const struct iwl_trans_config *trans_cfg);
	void (*set_pmi)(struct iwl_trans *trans, bool state);
	int (*sw_reset)(struct iwl_trans *trans, bool retake_ownership);
	bool (*grab_nic_access)(struct iwl_trans *trans);
	void (*release_nic_access)(struct iwl_trans *trans);
	void (*set_bits_mask)(struct iwl_trans *trans, u32 reg, u32 mask,
			      u32 value);
	struct iwl_trans_dump_data *(*dump_data)(struct iwl_trans *trans,
						 u32 dump_mask,
						 const struct iwl_dump_sanitize_ops *sanitize_ops,
						 void *sanitize_ctx);
	void (*debugfs_cleanup)(struct iwl_trans *trans);
	void (*sync_nmi)(struct iwl_trans *trans);
	int (*load_pnvm)(struct iwl_trans *trans,
			 const struct iwl_pnvm_image *pnvm_payloads,
			 const struct iwl_ucode_capabilities *capa);
	void (*set_pnvm)(struct iwl_trans *trans,
			 const struct iwl_ucode_capabilities *capa);
	int (*load_reduce_power)(struct iwl_trans *trans,
				 const struct iwl_pnvm_image *payloads,
				 const struct iwl_ucode_capabilities *capa);
	void (*set_reduce_power)(struct iwl_trans *trans,
				 const struct iwl_ucode_capabilities *capa);
	void (*interrupts)(struct iwl_trans *trans, bool enable);
	int (*imr_dma_data)(struct iwl_trans *trans,
			    u32 dst_addr, u64 src_addr,
			    u32 byte_cnt);
};
enum iwl_trans_state {
	IWL_TRANS_NO_FW,
	IWL_TRANS_FW_STARTED,
	IWL_TRANS_FW_ALIVE,
};
enum iwl_plat_pm_mode {
	IWL_PLAT_PM_MODE_DISABLED,
	IWL_PLAT_PM_MODE_D3,
};
enum iwl_ini_cfg_state {
	IWL_INI_CFG_STATE_NOT_LOADED,
	IWL_INI_CFG_STATE_LOADED,
	IWL_INI_CFG_STATE_CORRUPTED,
};
#define IWL_TRANS_NMI_TIMEOUT (HZ / 4)
struct iwl_dram_data {
	dma_addr_t physical;
	void *block;
	int size;
};
struct iwl_dram_regions {
	struct iwl_dram_data drams[IPC_DRAM_MAP_ENTRY_NUM_MAX];
	struct iwl_dram_data prph_scratch_mem_desc;
	u8 n_regions;
};
struct iwl_fw_mon {
	u32 num_frags;
	struct iwl_dram_data *frags;
};
struct iwl_self_init_dram {
	struct iwl_dram_data *fw;
	int fw_cnt;
	struct iwl_dram_data *paging;
	int paging_cnt;
};
struct iwl_imr_data {
	u32 imr_enable;
	u32 imr_size;
	u32 sram_addr;
	u32 sram_size;
	u32 imr2sram_remainbyte;
	u64 imr_curr_addr;
	__le64 imr_base_addr;
};
#define IWL_TRANS_CURRENT_PC_NAME_MAX_BYTES      32
struct iwl_pc_data {
	u8  pc_name[IWL_TRANS_CURRENT_PC_NAME_MAX_BYTES];
	u32 pc_address;
};
struct iwl_trans_debug {
	u8 n_dest_reg;
	bool rec_on;
	const struct iwl_fw_dbg_dest_tlv_v1 *dest_tlv;
	const struct iwl_fw_dbg_conf_tlv *conf_tlv[FW_DBG_CONF_MAX];
	struct iwl_fw_dbg_trigger_tlv * const *trigger_tlv;
	u32 lmac_error_event_table[2];
	u32 umac_error_event_table;
	u32 tcm_error_event_table[2];
	u32 rcm_error_event_table[2];
	unsigned int error_event_table_tlv_status;
	enum iwl_ini_cfg_state internal_ini_cfg;
	enum iwl_ini_cfg_state external_ini_cfg;
	struct iwl_fw_ini_allocation_tlv fw_mon_cfg[IWL_FW_INI_ALLOCATION_NUM];
	struct iwl_fw_mon fw_mon_ini[IWL_FW_INI_ALLOCATION_NUM];
	struct iwl_dram_data fw_mon;
	bool hw_error;
	enum iwl_fw_ini_buffer_location ini_dest;
	u64 unsupported_region_msk;
	struct iwl_ucode_tlv *active_regions[IWL_FW_INI_MAX_REGION_ID];
	struct list_head debug_info_tlv_list;
	struct iwl_dbg_tlv_time_point_data
		time_point[IWL_FW_INI_TIME_POINT_NUM];
	struct list_head periodic_trig_list;
	u32 domains_bitmap;
	u32 ucode_preset;
	bool restart_required;
	u32 last_tp_resetfw;
	struct iwl_imr_data imr_data;
	u8 dump_file_name_ext[IWL_FW_INI_MAX_NAME];
	bool dump_file_name_ext_valid;
	u32 num_pc;
	struct iwl_pc_data *pc_data;
};
struct iwl_dma_ptr {
	dma_addr_t dma;
	void *addr;
	size_t size;
};
struct iwl_cmd_meta {
	struct iwl_host_cmd *source;
	u32 flags;
	u32 tbs;
};
#define IWL_FIRST_TB_SIZE	20
#define IWL_FIRST_TB_SIZE_ALIGN ALIGN(IWL_FIRST_TB_SIZE, 64)
struct iwl_pcie_txq_entry {
	void *cmd;
	struct sk_buff *skb;
	const void *free_buf;
	struct iwl_cmd_meta meta;
};
struct iwl_pcie_first_tb_buf {
	u8 buf[IWL_FIRST_TB_SIZE_ALIGN];
};
struct iwl_txq {
	void *tfds;
	struct iwl_pcie_first_tb_buf *first_tb_bufs;
	dma_addr_t first_tb_dma;
	struct iwl_pcie_txq_entry *entries;
	spinlock_t lock;
	unsigned long frozen_expiry_remainder;
	struct timer_list stuck_timer;
	struct iwl_trans *trans;
	bool need_update;
	bool frozen;
	bool ampdu;
	int block;
	unsigned long wd_timeout;
	struct sk_buff_head overflow_q;
	struct iwl_dma_ptr bc_tbl;
	int write_ptr;
	int read_ptr;
	dma_addr_t dma_addr;
	int n_window;
	u32 id;
	int low_mark;
	int high_mark;
	bool overflow_tx;
};
struct iwl_trans_txqs {
	unsigned long queue_used[BITS_TO_LONGS(IWL_MAX_TVQM_QUEUES)];
	unsigned long queue_stopped[BITS_TO_LONGS(IWL_MAX_TVQM_QUEUES)];
	struct iwl_txq *txq[IWL_MAX_TVQM_QUEUES];
	struct dma_pool *bc_pool;
	size_t bc_tbl_size;
	bool bc_table_dword;
	u8 page_offs;
	u8 dev_cmd_offs;
	struct iwl_tso_hdr_page __percpu *tso_hdr_page;
	struct {
		u8 fifo;
		u8 q_id;
		unsigned int wdg_timeout;
	} cmd;
	struct {
		u8 max_tbs;
		u16 size;
		u8 addr_size;
	} tfd;
	struct iwl_dma_ptr scd_bc_tbls;
	u8 queue_alloc_cmd_ver;
};
struct iwl_trans {
	bool csme_own;
	const struct iwl_trans_ops *ops;
	struct iwl_op_mode *op_mode;
	const struct iwl_cfg_trans_params *trans_cfg;
	const struct iwl_cfg *cfg;
	struct iwl_drv *drv;
	enum iwl_trans_state state;
	unsigned long status;
	struct device *dev;
	u32 max_skb_frags;
	u32 hw_rev;
	u32 hw_rev_step;
	u32 hw_rf_id;
	u32 hw_crf_id;
	u32 hw_cnv_id;
	u32 hw_wfpm_id;
	u32 hw_id;
	char hw_id_str[52];
	u32 sku_id[3];
	u8 rx_mpdu_cmd, rx_mpdu_cmd_hdr_size;
	bool pm_support;
	bool ltr_enabled;
	u8 pnvm_loaded:1;
	u8 fail_to_parse_pnvm_image:1;
	u8 reduce_power_loaded:1;
	u8 failed_to_load_reduce_power_image:1;
	const struct iwl_hcmd_arr *command_groups;
	int command_groups_size;
	bool wide_cmd_header;
	wait_queue_head_t wait_command_queue;
	u8 num_rx_queues;
	size_t iml_len;
	u8 *iml;
	struct kmem_cache *dev_cmd_pool;
	char dev_cmd_pool_name[50];
	struct dentry *dbgfs_dir;
#ifdef CONFIG_LOCKDEP
	struct lockdep_map sync_cmd_lockdep_map;
#endif
	struct iwl_trans_debug dbg;
	struct iwl_self_init_dram init_dram;
	enum iwl_plat_pm_mode system_pm_mode;
	const char *name;
	struct iwl_trans_txqs txqs;
	u32 mbx_addr_0_step;
	u32 mbx_addr_1_step;
	u8 pcie_link_speed;
	struct iwl_dma_ptr invalid_tx_cmd;
	char trans_specific[] __aligned(sizeof(void *));
};
const char *iwl_get_cmd_string(struct iwl_trans *trans, u32 id);
int iwl_cmd_groups_verify_sorted(const struct iwl_trans_config *trans);
static inline void iwl_trans_configure(struct iwl_trans *trans,
				       const struct iwl_trans_config *trans_cfg)
{
	trans->op_mode = trans_cfg->op_mode;
	trans->ops->configure(trans, trans_cfg);
	WARN_ON(iwl_cmd_groups_verify_sorted(trans_cfg));
}
static inline int iwl_trans_start_hw(struct iwl_trans *trans)
{
	might_sleep();
	return trans->ops->start_hw(trans);
}
static inline void iwl_trans_op_mode_leave(struct iwl_trans *trans)
{
	might_sleep();
	if (trans->ops->op_mode_leave)
		trans->ops->op_mode_leave(trans);
	trans->op_mode = NULL;
	trans->state = IWL_TRANS_NO_FW;
}
static inline void iwl_trans_fw_alive(struct iwl_trans *trans, u32 scd_addr)
{
	might_sleep();
	trans->state = IWL_TRANS_FW_ALIVE;
	trans->ops->fw_alive(trans, scd_addr);
}
static inline int iwl_trans_start_fw(struct iwl_trans *trans,
				     const struct fw_img *fw,
				     bool run_in_rfkill)
{
	int ret;
	might_sleep();
	WARN_ON_ONCE(!trans->rx_mpdu_cmd);
	clear_bit(STATUS_FW_ERROR, &trans->status);
	ret = trans->ops->start_fw(trans, fw, run_in_rfkill);
	if (ret == 0)
		trans->state = IWL_TRANS_FW_STARTED;
	return ret;
}
static inline void iwl_trans_stop_device(struct iwl_trans *trans)
{
	might_sleep();
	trans->ops->stop_device(trans);
	trans->state = IWL_TRANS_NO_FW;
}
static inline int iwl_trans_d3_suspend(struct iwl_trans *trans, bool test,
				       bool reset)
{
	might_sleep();
	if (!trans->ops->d3_suspend)
		return -EOPNOTSUPP;
	return trans->ops->d3_suspend(trans, test, reset);
}
static inline int iwl_trans_d3_resume(struct iwl_trans *trans,
				      enum iwl_d3_status *status,
				      bool test, bool reset)
{
	might_sleep();
	if (!trans->ops->d3_resume)
		return -EOPNOTSUPP;
	return trans->ops->d3_resume(trans, status, test, reset);
}
static inline struct iwl_trans_dump_data *
iwl_trans_dump_data(struct iwl_trans *trans, u32 dump_mask,
		    const struct iwl_dump_sanitize_ops *sanitize_ops,
		    void *sanitize_ctx)
{
	if (!trans->ops->dump_data)
		return NULL;
	return trans->ops->dump_data(trans, dump_mask,
				     sanitize_ops, sanitize_ctx);
}
static inline struct iwl_device_tx_cmd *
iwl_trans_alloc_tx_cmd(struct iwl_trans *trans)
{
	return kmem_cache_zalloc(trans->dev_cmd_pool, GFP_ATOMIC);
}
int iwl_trans_send_cmd(struct iwl_trans *trans, struct iwl_host_cmd *cmd);
static inline void iwl_trans_free_tx_cmd(struct iwl_trans *trans,
					 struct iwl_device_tx_cmd *dev_cmd)
{
	kmem_cache_free(trans->dev_cmd_pool, dev_cmd);
}
static inline int iwl_trans_tx(struct iwl_trans *trans, struct sk_buff *skb,
			       struct iwl_device_tx_cmd *dev_cmd, int queue)
{
	if (unlikely(test_bit(STATUS_FW_ERROR, &trans->status)))
		return -EIO;
	if (WARN_ON_ONCE(trans->state != IWL_TRANS_FW_ALIVE)) {
		IWL_ERR(trans, "%s bad state = %d\n", __func__, trans->state);
		return -EIO;
	}
	return trans->ops->tx(trans, skb, dev_cmd, queue);
}
static inline void iwl_trans_reclaim(struct iwl_trans *trans, int queue,
				     int ssn, struct sk_buff_head *skbs,
				     bool is_flush)
{
	if (WARN_ON_ONCE(trans->state != IWL_TRANS_FW_ALIVE)) {
		IWL_ERR(trans, "%s bad state = %d\n", __func__, trans->state);
		return;
	}
	trans->ops->reclaim(trans, queue, ssn, skbs, is_flush);
}
static inline void iwl_trans_set_q_ptrs(struct iwl_trans *trans, int queue,
					int ptr)
{
	if (WARN_ON_ONCE(trans->state != IWL_TRANS_FW_ALIVE)) {
		IWL_ERR(trans, "%s bad state = %d\n", __func__, trans->state);
		return;
	}
	trans->ops->set_q_ptrs(trans, queue, ptr);
}
static inline void iwl_trans_txq_disable(struct iwl_trans *trans, int queue,
					 bool configure_scd)
{
	trans->ops->txq_disable(trans, queue, configure_scd);
}
static inline bool
iwl_trans_txq_enable_cfg(struct iwl_trans *trans, int queue, u16 ssn,
			 const struct iwl_trans_txq_scd_cfg *cfg,
			 unsigned int queue_wdg_timeout)
{
	might_sleep();
	if (WARN_ON_ONCE(trans->state != IWL_TRANS_FW_ALIVE)) {
		IWL_ERR(trans, "%s bad state = %d\n", __func__, trans->state);
		return false;
	}
	return trans->ops->txq_enable(trans, queue, ssn,
				      cfg, queue_wdg_timeout);
}
static inline int
iwl_trans_get_rxq_dma_data(struct iwl_trans *trans, int queue,
			   struct iwl_trans_rxq_dma_data *data)
{
	if (WARN_ON_ONCE(!trans->ops->rxq_dma_data))
		return -ENOTSUPP;
	return trans->ops->rxq_dma_data(trans, queue, data);
}
static inline void
iwl_trans_txq_free(struct iwl_trans *trans, int queue)
{
	if (WARN_ON_ONCE(!trans->ops->txq_free))
		return;
	trans->ops->txq_free(trans, queue);
}
static inline int
iwl_trans_txq_alloc(struct iwl_trans *trans,
		    u32 flags, u32 sta_mask, u8 tid,
		    int size, unsigned int wdg_timeout)
{
	might_sleep();
	if (WARN_ON_ONCE(!trans->ops->txq_alloc))
		return -ENOTSUPP;
	if (WARN_ON_ONCE(trans->state != IWL_TRANS_FW_ALIVE)) {
		IWL_ERR(trans, "%s bad state = %d\n", __func__, trans->state);
		return -EIO;
	}
	return trans->ops->txq_alloc(trans, flags, sta_mask, tid,
				     size, wdg_timeout);
}
static inline void iwl_trans_txq_set_shared_mode(struct iwl_trans *trans,
						 int queue, bool shared_mode)
{
	if (trans->ops->txq_set_shared_mode)
		trans->ops->txq_set_shared_mode(trans, queue, shared_mode);
}
static inline void iwl_trans_txq_enable(struct iwl_trans *trans, int queue,
					int fifo, int sta_id, int tid,
					int frame_limit, u16 ssn,
					unsigned int queue_wdg_timeout)
{
	struct iwl_trans_txq_scd_cfg cfg = {
		.fifo = fifo,
		.sta_id = sta_id,
		.tid = tid,
		.frame_limit = frame_limit,
		.aggregate = sta_id >= 0,
	};
	iwl_trans_txq_enable_cfg(trans, queue, ssn, &cfg, queue_wdg_timeout);
}
static inline
void iwl_trans_ac_txq_enable(struct iwl_trans *trans, int queue, int fifo,
			     unsigned int queue_wdg_timeout)
{
	struct iwl_trans_txq_scd_cfg cfg = {
		.fifo = fifo,
		.sta_id = -1,
		.tid = IWL_MAX_TID_COUNT,
		.frame_limit = IWL_FRAME_LIMIT,
		.aggregate = false,
	};
	iwl_trans_txq_enable_cfg(trans, queue, 0, &cfg, queue_wdg_timeout);
}
static inline void iwl_trans_freeze_txq_timer(struct iwl_trans *trans,
					      unsigned long txqs,
					      bool freeze)
{
	if (WARN_ON_ONCE(trans->state != IWL_TRANS_FW_ALIVE)) {
		IWL_ERR(trans, "%s bad state = %d\n", __func__, trans->state);
		return;
	}
	if (trans->ops->freeze_txq_timer)
		trans->ops->freeze_txq_timer(trans, txqs, freeze);
}
static inline void iwl_trans_block_txq_ptrs(struct iwl_trans *trans,
					    bool block)
{
	if (WARN_ON_ONCE(trans->state != IWL_TRANS_FW_ALIVE)) {
		IWL_ERR(trans, "%s bad state = %d\n", __func__, trans->state);
		return;
	}
	if (trans->ops->block_txq_ptrs)
		trans->ops->block_txq_ptrs(trans, block);
}
static inline int iwl_trans_wait_tx_queues_empty(struct iwl_trans *trans,
						 u32 txqs)
{
	if (WARN_ON_ONCE(!trans->ops->wait_tx_queues_empty))
		return -ENOTSUPP;
	if (trans->state != IWL_TRANS_FW_ALIVE) {
		IWL_ERR(trans, "%s bad state = %d\n", __func__, trans->state);
		return -EIO;
	}
	return trans->ops->wait_tx_queues_empty(trans, txqs);
}
static inline int iwl_trans_wait_txq_empty(struct iwl_trans *trans, int queue)
{
	if (WARN_ON_ONCE(!trans->ops->wait_txq_empty))
		return -ENOTSUPP;
	if (WARN_ON_ONCE(trans->state != IWL_TRANS_FW_ALIVE)) {
		IWL_ERR(trans, "%s bad state = %d\n", __func__, trans->state);
		return -EIO;
	}
	return trans->ops->wait_txq_empty(trans, queue);
}
static inline void iwl_trans_write8(struct iwl_trans *trans, u32 ofs, u8 val)
{
	trans->ops->write8(trans, ofs, val);
}
static inline void iwl_trans_write32(struct iwl_trans *trans, u32 ofs, u32 val)
{
	trans->ops->write32(trans, ofs, val);
}
static inline u32 iwl_trans_read32(struct iwl_trans *trans, u32 ofs)
{
	return trans->ops->read32(trans, ofs);
}
static inline u32 iwl_trans_read_prph(struct iwl_trans *trans, u32 ofs)
{
	return trans->ops->read_prph(trans, ofs);
}
static inline void iwl_trans_write_prph(struct iwl_trans *trans, u32 ofs,
					u32 val)
{
	return trans->ops->write_prph(trans, ofs, val);
}
static inline int iwl_trans_read_mem(struct iwl_trans *trans, u32 addr,
				     void *buf, int dwords)
{
	return trans->ops->read_mem(trans, addr, buf, dwords);
}
#define iwl_trans_read_mem_bytes(trans, addr, buf, bufsize)		      \
	do {								      \
		if (__builtin_constant_p(bufsize))			      \
			BUILD_BUG_ON((bufsize) % sizeof(u32));		      \
		iwl_trans_read_mem(trans, addr, buf, (bufsize) / sizeof(u32));\
	} while (0)
static inline int iwl_trans_write_imr_mem(struct iwl_trans *trans,
					  u32 dst_addr, u64 src_addr,
					  u32 byte_cnt)
{
	if (trans->ops->imr_dma_data)
		return trans->ops->imr_dma_data(trans, dst_addr, src_addr, byte_cnt);
	return 0;
}
static inline u32 iwl_trans_read_mem32(struct iwl_trans *trans, u32 addr)
{
	u32 value;
	if (iwl_trans_read_mem(trans, addr, &value, 1))
		return 0xa5a5a5a5;
	return value;
}
static inline int iwl_trans_write_mem(struct iwl_trans *trans, u32 addr,
				      const void *buf, int dwords)
{
	return trans->ops->write_mem(trans, addr, buf, dwords);
}
static inline u32 iwl_trans_write_mem32(struct iwl_trans *trans, u32 addr,
					u32 val)
{
	return iwl_trans_write_mem(trans, addr, &val, 1);
}
static inline void iwl_trans_set_pmi(struct iwl_trans *trans, bool state)
{
	if (trans->ops->set_pmi)
		trans->ops->set_pmi(trans, state);
}
static inline int iwl_trans_sw_reset(struct iwl_trans *trans,
				     bool retake_ownership)
{
	if (trans->ops->sw_reset)
		return trans->ops->sw_reset(trans, retake_ownership);
	return 0;
}
static inline void
iwl_trans_set_bits_mask(struct iwl_trans *trans, u32 reg, u32 mask, u32 value)
{
	trans->ops->set_bits_mask(trans, reg, mask, value);
}
#define iwl_trans_grab_nic_access(trans)		\
	__cond_lock(nic_access,				\
		    likely((trans)->ops->grab_nic_access(trans)))
static inline void __releases(nic_access)
iwl_trans_release_nic_access(struct iwl_trans *trans)
{
	trans->ops->release_nic_access(trans);
	__release(nic_access);
}
static inline void iwl_trans_fw_error(struct iwl_trans *trans, bool sync)
{
	if (WARN_ON_ONCE(!trans->op_mode))
		return;
	if (!test_and_set_bit(STATUS_FW_ERROR, &trans->status)) {
		iwl_op_mode_nic_error(trans->op_mode, sync);
		trans->state = IWL_TRANS_NO_FW;
	}
}
static inline bool iwl_trans_fw_running(struct iwl_trans *trans)
{
	return trans->state == IWL_TRANS_FW_ALIVE;
}
static inline void iwl_trans_sync_nmi(struct iwl_trans *trans)
{
	if (trans->ops->sync_nmi)
		trans->ops->sync_nmi(trans);
}
void iwl_trans_sync_nmi_with_addr(struct iwl_trans *trans, u32 inta_addr,
				  u32 sw_err_bit);
static inline int iwl_trans_load_pnvm(struct iwl_trans *trans,
				      const struct iwl_pnvm_image *pnvm_data,
				      const struct iwl_ucode_capabilities *capa)
{
	return trans->ops->load_pnvm(trans, pnvm_data, capa);
}
static inline void iwl_trans_set_pnvm(struct iwl_trans *trans,
				      const struct iwl_ucode_capabilities *capa)
{
	if (trans->ops->set_pnvm)
		trans->ops->set_pnvm(trans, capa);
}
static inline int iwl_trans_load_reduce_power
				(struct iwl_trans *trans,
				 const struct iwl_pnvm_image *payloads,
				 const struct iwl_ucode_capabilities *capa)
{
	return trans->ops->load_reduce_power(trans, payloads, capa);
}
static inline void
iwl_trans_set_reduce_power(struct iwl_trans *trans,
			   const struct iwl_ucode_capabilities *capa)
{
	if (trans->ops->set_reduce_power)
		trans->ops->set_reduce_power(trans, capa);
}
static inline bool iwl_trans_dbg_ini_valid(struct iwl_trans *trans)
{
	return trans->dbg.internal_ini_cfg != IWL_INI_CFG_STATE_NOT_LOADED ||
		trans->dbg.external_ini_cfg != IWL_INI_CFG_STATE_NOT_LOADED;
}
static inline void iwl_trans_interrupts(struct iwl_trans *trans, bool enable)
{
	if (trans->ops->interrupts)
		trans->ops->interrupts(trans, enable);
}
struct iwl_trans *iwl_trans_alloc(unsigned int priv_size,
			  struct device *dev,
			  const struct iwl_trans_ops *ops,
			  const struct iwl_cfg_trans_params *cfg_trans);
int iwl_trans_init(struct iwl_trans *trans);
void iwl_trans_free(struct iwl_trans *trans);
static inline bool iwl_trans_is_hw_error_value(u32 val)
{
	return ((val & ~0xf) == 0xa5a5a5a0) || ((val & ~0xf) == 0x5a5a5a50);
}
int __must_check iwl_pci_register_driver(void);
void iwl_pci_unregister_driver(void);
void iwl_trans_pcie_remove(struct iwl_trans *trans, bool rescan);
#endif  
