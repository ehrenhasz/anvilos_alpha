 
#include "ctxgf100.h"

#include <subdev/fb.h>

 

const struct gf100_grctx_func
gp107_grctx = {
	.main = gf100_grctx_generate_main,
	.unkn = gk104_grctx_generate_unkn,
	.bundle = gm107_grctx_generate_bundle,
	.bundle_size = 0x3000,
	.bundle_min_gpm_fifo_depth = 0x180,
	.bundle_token_limit = 0x300,
	.pagepool = gp100_grctx_generate_pagepool,
	.pagepool_size = 0x20000,
	.attrib_cb_size = gp102_grctx_generate_attrib_cb_size,
	.attrib_cb = gp100_grctx_generate_attrib_cb,
	.attrib = gp102_grctx_generate_attrib,
	.attrib_nr_max = 0x15de,
	.attrib_nr = 0x540,
	.alpha_nr_max = 0xc00,
	.alpha_nr = 0x800,
	.gfxp_nr = 0xe94,
	.sm_id = gm107_grctx_generate_sm_id,
	.rop_mapping = gf117_grctx_generate_rop_mapping,
	.dist_skip_table = gm200_grctx_generate_dist_skip_table,
	.r406500 = gm200_grctx_generate_r406500,
	.gpc_tpc_nr = gk104_grctx_generate_gpc_tpc_nr,
	.tpc_mask = gm200_grctx_generate_tpc_mask,
	.smid_config = gp100_grctx_generate_smid_config,
	.r419a3c = gm200_grctx_generate_r419a3c,
};
