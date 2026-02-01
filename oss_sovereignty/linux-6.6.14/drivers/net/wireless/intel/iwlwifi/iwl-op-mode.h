 
 
#ifndef __iwl_op_mode_h__
#define __iwl_op_mode_h__

#include <linux/netdevice.h>
#include <linux/debugfs.h>
#include "iwl-dbg-tlv.h"

struct iwl_op_mode;
struct iwl_trans;
struct sk_buff;
struct iwl_device_cmd;
struct iwl_rx_cmd_buffer;
struct iwl_fw;
struct iwl_cfg;

 

 

 
struct iwl_op_mode_ops {
	struct iwl_op_mode *(*start)(struct iwl_trans *trans,
				     const struct iwl_cfg *cfg,
				     const struct iwl_fw *fw,
				     struct dentry *dbgfs_dir);
	void (*stop)(struct iwl_op_mode *op_mode);
	void (*rx)(struct iwl_op_mode *op_mode, struct napi_struct *napi,
		   struct iwl_rx_cmd_buffer *rxb);
	void (*rx_rss)(struct iwl_op_mode *op_mode, struct napi_struct *napi,
		       struct iwl_rx_cmd_buffer *rxb, unsigned int queue);
	void (*async_cb)(struct iwl_op_mode *op_mode,
			 const struct iwl_device_cmd *cmd);
	void (*queue_full)(struct iwl_op_mode *op_mode, int queue);
	void (*queue_not_full)(struct iwl_op_mode *op_mode, int queue);
	bool (*hw_rf_kill)(struct iwl_op_mode *op_mode, bool state);
	void (*free_skb)(struct iwl_op_mode *op_mode, struct sk_buff *skb);
	void (*nic_error)(struct iwl_op_mode *op_mode, bool sync);
	void (*cmd_queue_full)(struct iwl_op_mode *op_mode);
	void (*nic_config)(struct iwl_op_mode *op_mode);
	void (*wimax_active)(struct iwl_op_mode *op_mode);
	void (*time_point)(struct iwl_op_mode *op_mode,
			   enum iwl_fw_ini_time_point tp_id,
			   union iwl_dbg_tlv_tp_data *tp_data);
};

int iwl_opmode_register(const char *name, const struct iwl_op_mode_ops *ops);
void iwl_opmode_deregister(const char *name);

 
struct iwl_op_mode {
	const struct iwl_op_mode_ops *ops;

	char op_mode_specific[] __aligned(sizeof(void *));
};

static inline void iwl_op_mode_stop(struct iwl_op_mode *op_mode)
{
	might_sleep();
	op_mode->ops->stop(op_mode);
}

static inline void iwl_op_mode_rx(struct iwl_op_mode *op_mode,
				  struct napi_struct *napi,
				  struct iwl_rx_cmd_buffer *rxb)
{
	return op_mode->ops->rx(op_mode, napi, rxb);
}

static inline void iwl_op_mode_rx_rss(struct iwl_op_mode *op_mode,
				      struct napi_struct *napi,
				      struct iwl_rx_cmd_buffer *rxb,
				      unsigned int queue)
{
	op_mode->ops->rx_rss(op_mode, napi, rxb, queue);
}

static inline void iwl_op_mode_async_cb(struct iwl_op_mode *op_mode,
					const struct iwl_device_cmd *cmd)
{
	if (op_mode->ops->async_cb)
		op_mode->ops->async_cb(op_mode, cmd);
}

static inline void iwl_op_mode_queue_full(struct iwl_op_mode *op_mode,
					  int queue)
{
	op_mode->ops->queue_full(op_mode, queue);
}

static inline void iwl_op_mode_queue_not_full(struct iwl_op_mode *op_mode,
					      int queue)
{
	op_mode->ops->queue_not_full(op_mode, queue);
}

static inline bool __must_check
iwl_op_mode_hw_rf_kill(struct iwl_op_mode *op_mode, bool state)
{
	might_sleep();
	return op_mode->ops->hw_rf_kill(op_mode, state);
}

static inline void iwl_op_mode_free_skb(struct iwl_op_mode *op_mode,
					struct sk_buff *skb)
{
	if (WARN_ON_ONCE(!op_mode))
		return;
	op_mode->ops->free_skb(op_mode, skb);
}

static inline void iwl_op_mode_nic_error(struct iwl_op_mode *op_mode, bool sync)
{
	op_mode->ops->nic_error(op_mode, sync);
}

static inline void iwl_op_mode_cmd_queue_full(struct iwl_op_mode *op_mode)
{
	op_mode->ops->cmd_queue_full(op_mode);
}

static inline void iwl_op_mode_nic_config(struct iwl_op_mode *op_mode)
{
	might_sleep();
	op_mode->ops->nic_config(op_mode);
}

static inline void iwl_op_mode_wimax_active(struct iwl_op_mode *op_mode)
{
	might_sleep();
	op_mode->ops->wimax_active(op_mode);
}

static inline void iwl_op_mode_time_point(struct iwl_op_mode *op_mode,
					  enum iwl_fw_ini_time_point tp_id,
					  union iwl_dbg_tlv_tp_data *tp_data)
{
	if (!op_mode || !op_mode->ops || !op_mode->ops->time_point)
		return;
	op_mode->ops->time_point(op_mode, tp_id, tp_data);
}

#endif  
