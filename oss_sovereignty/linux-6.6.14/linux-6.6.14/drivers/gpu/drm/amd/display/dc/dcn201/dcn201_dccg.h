#ifndef __DCN201_DCCG_H__
#define __DCN201_DCCG_H__
#include "dcn20/dcn20_dccg.h"
struct dccg *dccg201_create(
	struct dc_context *ctx,
	const struct dccg_registers *regs,
	const struct dccg_shift *dccg_shift,
	const struct dccg_mask *dccg_mask);
#endif  
