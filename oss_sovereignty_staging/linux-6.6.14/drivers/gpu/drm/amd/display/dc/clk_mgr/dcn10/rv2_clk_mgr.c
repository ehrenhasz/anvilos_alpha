 

#include "core_types.h"
#include "clk_mgr_internal.h"
#include "rv1_clk_mgr.h"
#include "rv2_clk_mgr.h"
#include "dce112/dce112_clk_mgr.h"

static struct clk_mgr_internal_funcs rv2_clk_internal_funcs = {
	.set_dispclk = dce112_set_dispclk,
	.set_dprefclk = dce112_set_dprefclk
};

void rv2_clk_mgr_construct(struct dc_context *ctx, struct clk_mgr_internal *clk_mgr, struct pp_smu_funcs *pp_smu)

{
	rv1_clk_mgr_construct(ctx, clk_mgr, pp_smu);

	clk_mgr->funcs = &rv2_clk_internal_funcs;
}
