#ifndef DAL_DC_DCN201_DCN201_HUBBUB_H_
#define DAL_DC_DCN201_DCN201_HUBBUB_H_
#include "dcn20/dcn20_hubbub.h"
#define HUBBUB_REG_LIST_DCN201(id)\
	HUBBUB_REG_LIST_DCN_COMMON(), \
	HUBBUB_VM_REG_LIST(), \
	SR(DCHUBBUB_CRC_CTRL)
#define HUBBUB_MASK_SH_LIST_DCN201(mask_sh)\
	HUBBUB_MASK_SH_LIST_DCN_COMMON(mask_sh), \
	HUBBUB_SF(DCHUBBUB_GLOBAL_TIMER_CNTL, DCHUBBUB_GLOBAL_TIMER_REFDIV, mask_sh)
void hubbub201_construct(struct dcn20_hubbub *hubbub,
	struct dc_context *ctx,
	const struct dcn_hubbub_registers *hubbub_regs,
	const struct dcn_hubbub_shift *hubbub_shift,
	const struct dcn_hubbub_mask *hubbub_mask);
#endif  
