 

#include "dm_services.h"

 
#include "dce/dce_11_0_d.h"
#include "dce/dce_11_0_sh_mask.h"

#include "dce/dce_opp.h"
#include "dce110_opp_v.h"

 
 
 

static const struct opp_funcs funcs = {
		.opp_set_dyn_expansion = dce110_opp_set_dyn_expansion,
		.opp_destroy = dce110_opp_destroy,
		.opp_program_fmt = dce110_opp_program_fmt,
		.opp_program_bit_depth_reduction =
				dce110_opp_program_bit_depth_reduction
};

void dce110_opp_v_construct(struct dce110_opp *opp110,
	struct dc_context *ctx)
{
	opp110->base.funcs = &funcs;

	opp110->base.ctx = ctx;
}

