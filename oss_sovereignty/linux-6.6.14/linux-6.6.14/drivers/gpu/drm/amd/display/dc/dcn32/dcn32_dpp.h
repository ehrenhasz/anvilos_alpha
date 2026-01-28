#ifndef __DCN32_DPP_H__
#define __DCN32_DPP_H__
#include "dcn20/dcn20_dpp.h"
#include "dcn30/dcn30_dpp.h"
bool dpp32_construct(struct dcn3_dpp *dpp3,
	struct dc_context *ctx,
	uint32_t inst,
	const struct dcn3_dpp_registers *tf_regs,
	const struct dcn3_dpp_shift *tf_shift,
	const struct dcn3_dpp_mask *tf_mask);
#endif  
