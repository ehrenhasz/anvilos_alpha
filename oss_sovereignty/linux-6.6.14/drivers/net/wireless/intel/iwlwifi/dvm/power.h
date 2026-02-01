 
 
#ifndef __iwl_power_setting_h__
#define __iwl_power_setting_h__

#include "commands.h"

struct iwl_power_mgr {
	struct iwl_powertable_cmd sleep_cmd;
	struct iwl_powertable_cmd sleep_cmd_next;
	int debug_sleep_level_override;
	bool bus_pm;
};

int iwl_power_set_mode(struct iwl_priv *priv, struct iwl_powertable_cmd *cmd,
		       bool force);
int iwl_power_update_mode(struct iwl_priv *priv, bool force);
void iwl_power_initialize(struct iwl_priv *priv);

extern bool no_sleep_autoadjust;

#endif   
