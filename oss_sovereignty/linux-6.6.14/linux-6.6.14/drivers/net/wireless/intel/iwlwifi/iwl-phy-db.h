#ifndef __IWL_PHYDB_H__
#define __IWL_PHYDB_H__
#include <linux/types.h>
#include "iwl-op-mode.h"
#include "iwl-trans.h"
struct iwl_phy_db *iwl_phy_db_init(struct iwl_trans *trans);
void iwl_phy_db_free(struct iwl_phy_db *phy_db);
int iwl_phy_db_set_section(struct iwl_phy_db *phy_db,
			   struct iwl_rx_packet *pkt);
int iwl_send_phy_db_data(struct iwl_phy_db *phy_db);
#endif  
