 
 
#ifndef __iwl_dbg_tlv_h__
#define __iwl_dbg_tlv_h__

#include <linux/device.h>
#include <linux/types.h>
#include <fw/file.h>
#include <fw/api/dbg-tlv.h>

#define IWL_DBG_TLV_MAX_PRESET	15
#define ENABLE_INI		(IWL_DBG_TLV_MAX_PRESET + 1)

 
struct iwl_dbg_tlv_node {
	struct list_head list;
	struct iwl_ucode_tlv tlv;
};

 
union iwl_dbg_tlv_tp_data {
	struct iwl_rx_packet *fw_pkt;
};

 
struct iwl_dbg_tlv_time_point_data {
	struct list_head trig_list;
	struct list_head active_trig_list;
	struct list_head hcmd_list;
	struct list_head config_list;
};

struct iwl_trans;
struct iwl_fw_runtime;

void iwl_dbg_tlv_load_bin(struct device *dev, struct iwl_trans *trans);
void iwl_dbg_tlv_free(struct iwl_trans *trans);
void iwl_dbg_tlv_alloc(struct iwl_trans *trans, const struct iwl_ucode_tlv *tlv,
		       bool ext);
void iwl_dbg_tlv_init(struct iwl_trans *trans);
void _iwl_dbg_tlv_time_point(struct iwl_fw_runtime *fwrt,
			     enum iwl_fw_ini_time_point tp_id,
			     union iwl_dbg_tlv_tp_data *tp_data,
			     bool sync);

static inline void iwl_dbg_tlv_time_point(struct iwl_fw_runtime *fwrt,
					  enum iwl_fw_ini_time_point tp_id,
					  union iwl_dbg_tlv_tp_data *tp_data)
{
	_iwl_dbg_tlv_time_point(fwrt, tp_id, tp_data, false);
}

static inline void iwl_dbg_tlv_time_point_sync(struct iwl_fw_runtime *fwrt,
					       enum iwl_fw_ini_time_point tp_id,
					       union iwl_dbg_tlv_tp_data *tp_data)
{
	_iwl_dbg_tlv_time_point(fwrt, tp_id, tp_data, true);
}

void iwl_dbg_tlv_del_timers(struct iwl_trans *trans);

#endif  
