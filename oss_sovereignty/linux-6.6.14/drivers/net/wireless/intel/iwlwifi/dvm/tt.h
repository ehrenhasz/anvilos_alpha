 
 
#ifndef __iwl_tt_setting_h__
#define __iwl_tt_setting_h__

#include "commands.h"

#define IWL_ABSOLUTE_ZERO		0
#define IWL_ABSOLUTE_MAX		0xFFFFFFFF
#define IWL_TT_INCREASE_MARGIN	5
#define IWL_TT_CT_KILL_MARGIN	3

enum iwl_antenna_ok {
	IWL_ANT_OK_NONE,
	IWL_ANT_OK_SINGLE,
	IWL_ANT_OK_MULTI,
};

 
enum  iwl_tt_state {
	IWL_TI_0,	 
	IWL_TI_1,	 
	IWL_TI_2,	 
	IWL_TI_CT_KILL,  
	IWL_TI_STATE_MAX
};

 
struct iwl_tt_restriction {
	enum iwl_antenna_ok tx_stream;
	enum iwl_antenna_ok rx_stream;
	bool is_ht;
};

 
struct iwl_tt_trans {
	enum iwl_tt_state next_state;
	u32 tt_low;
	u32 tt_high;
};

 
struct iwl_tt_mgmt {
	enum iwl_tt_state state;
	bool advanced_tt;
	u8 tt_power_mode;
	bool ct_kill_toggle;
#ifdef CONFIG_IWLWIFI_DEBUG
	s32 tt_previous_temp;
#endif
	struct iwl_tt_restriction *restriction;
	struct iwl_tt_trans *transaction;
	struct timer_list ct_kill_exit_tm;
	struct timer_list ct_kill_waiting_tm;
};

u8 iwl_tt_current_power_mode(struct iwl_priv *priv);
bool iwl_tt_is_low_power_state(struct iwl_priv *priv);
bool iwl_ht_enabled(struct iwl_priv *priv);
enum iwl_antenna_ok iwl_tx_ant_restriction(struct iwl_priv *priv);
enum iwl_antenna_ok iwl_rx_ant_restriction(struct iwl_priv *priv);
void iwl_tt_enter_ct_kill(struct iwl_priv *priv);
void iwl_tt_exit_ct_kill(struct iwl_priv *priv);
void iwl_tt_handler(struct iwl_priv *priv);
void iwl_tt_initialize(struct iwl_priv *priv);
void iwl_tt_exit(struct iwl_priv *priv);

#endif   
