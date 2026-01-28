#ifndef __iwl_drv_h__
#define __iwl_drv_h__
#include <linux/export.h>
#define DRV_NAME        "iwlwifi"
#define NVM_RF_CFG_DASH_MSK(x)   (x & 0x3)          
#define NVM_RF_CFG_STEP_MSK(x)   ((x >> 2)  & 0x3)  
#define NVM_RF_CFG_TYPE_MSK(x)   ((x >> 4)  & 0x3)  
#define NVM_RF_CFG_PNUM_MSK(x)   ((x >> 6)  & 0x3)  
#define NVM_RF_CFG_TX_ANT_MSK(x) ((x >> 8)  & 0xF)  
#define NVM_RF_CFG_RX_ANT_MSK(x) ((x >> 12) & 0xF)  
#define EXT_NVM_RF_CFG_FLAVOR_MSK(x)   ((x) & 0xF)
#define EXT_NVM_RF_CFG_DASH_MSK(x)   (((x) >> 4) & 0xF)
#define EXT_NVM_RF_CFG_STEP_MSK(x)   (((x) >> 8) & 0xF)
#define EXT_NVM_RF_CFG_TYPE_MSK(x)   (((x) >> 12) & 0xFFF)
#define EXT_NVM_RF_CFG_TX_ANT_MSK(x) (((x) >> 24) & 0xF)
#define EXT_NVM_RF_CFG_RX_ANT_MSK(x) (((x) >> 28) & 0xF)
struct iwl_drv;
struct iwl_trans;
struct iwl_cfg;
struct iwl_drv *iwl_drv_start(struct iwl_trans *trans);
void iwl_drv_stop(struct iwl_drv *drv);
#ifdef CONFIG_IWLWIFI_OPMODE_MODULAR
#define IWL_EXPORT_SYMBOL(sym)	EXPORT_SYMBOL_NS_GPL(sym, IWLWIFI)
#else
#define IWL_EXPORT_SYMBOL(sym)
#endif
#define IWL_MAX_INIT_RETRY 2
#define FW_NAME_PRE_BUFSIZE	64
struct iwl_trans;
const char *iwl_drv_get_fwname_pre(struct iwl_trans *trans, char *buf);
#endif  
