


#ifndef _I40E_DIAG_H_
#define _I40E_DIAG_H_

#include "i40e_type.h"

enum i40e_lb_mode {
	I40E_LB_MODE_NONE       = 0x0,
	I40E_LB_MODE_PHY_LOCAL  = I40E_AQ_LB_PHY_LOCAL,
	I40E_LB_MODE_PHY_REMOTE = I40E_AQ_LB_PHY_REMOTE,
	I40E_LB_MODE_MAC_LOCAL  = I40E_AQ_LB_MAC_LOCAL,
};

struct i40e_diag_reg_test_info {
	u32 offset;	
	u32 mask;	
	u32 elements;	
	u32 stride;	
};

extern const struct i40e_diag_reg_test_info i40e_reg_list[];

int i40e_diag_reg_test(struct i40e_hw *hw);
int i40e_diag_eeprom_test(struct i40e_hw *hw);

#endif 
