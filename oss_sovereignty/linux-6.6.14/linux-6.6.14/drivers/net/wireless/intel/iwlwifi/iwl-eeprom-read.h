#ifndef __iwl_eeprom_h__
#define __iwl_eeprom_h__
#include "iwl-trans.h"
int iwl_read_eeprom(struct iwl_trans *trans, u8 **eeprom, size_t *eeprom_size);
#endif   
