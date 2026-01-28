#ifndef _HERMES_DLD_H
#define _HERMES_DLD_H
#include "hermes.h"
int hermesi_program_init(struct hermes *hw, u32 offset);
int hermesi_program_end(struct hermes *hw);
int hermes_program(struct hermes *hw, const char *first_block, const void *end);
int hermes_read_pda(struct hermes *hw,
		    __le16 *pda,
		    u32 pda_addr,
		    u16 pda_len,
		    int use_eeprom);
int hermes_apply_pda(struct hermes *hw,
		     const char *first_pdr,
		     const void *pdr_end,
		     const __le16 *pda,
		     const void *pda_end);
int hermes_apply_pda_with_defaults(struct hermes *hw,
				   const char *first_pdr,
				   const void *pdr_end,
				   const __le16 *pda,
				   const void *pda_end);
size_t hermes_blocks_length(const char *first_block, const void *end);
#endif  
