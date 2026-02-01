 
#include "ctxgf100.h"
#include "gf100.h"

#include <subdev/mc.h>

static void
gk20a_grctx_generate_main(struct gf100_gr_chan *chan)
{
	struct gf100_gr *gr = chan->gr;
	struct nvkm_device *device = gr->base.engine.subdev.device;
	const struct gf100_grctx_func *grctx = gr->func->grctx;
	u32 idle_timeout;
	int i;

	gf100_gr_mmio(gr, gr->sw_ctx);

	gf100_gr_wait_idle(gr);

	idle_timeout = nvkm_mask(device, 0x404154, 0xffffffff, 0x00000000);

	grctx->attrib_cb(chan, chan->attrib_cb->addr, grctx->attrib_cb_size(gr));
	grctx->attrib(chan);

	grctx->unkn(gr);

	gf100_grctx_generate_floorsweep(gr);

	for (i = 0; i < 8; i++)
		nvkm_wr32(device, 0x4064d0 + (i * 0x04), 0x00000000);

	nvkm_wr32(device, 0x405b00, (gr->tpc_total << 8) | gr->gpc_nr);

	nvkm_mask(device, 0x5044b0, 0x08000000, 0x08000000);

	gf100_gr_wait_idle(gr);

	nvkm_wr32(device, 0x404154, idle_timeout);
	gf100_gr_wait_idle(gr);

	gf100_gr_mthd(gr, gr->method);
	gf100_gr_wait_idle(gr);

	gf100_gr_icmd(gr, gr->bundle);
	grctx->pagepool(chan, chan->pagepool->addr);
	grctx->bundle(chan, chan->bundle_cb->addr, grctx->bundle_size);
}

const struct gf100_grctx_func
gk20a_grctx = {
	.main  = gk20a_grctx_generate_main,
	.unkn  = gk104_grctx_generate_unkn,
	.bundle = gk104_grctx_generate_bundle,
	.bundle_size = 0x1800,
	.bundle_min_gpm_fifo_depth = 0x62,
	.bundle_token_limit = 0x100,
	.pagepool = gk104_grctx_generate_pagepool,
	.pagepool_size = 0x8000,
	.attrib_cb_size = gf100_grctx_generate_attrib_cb_size,
	.attrib_cb = gf100_grctx_generate_attrib_cb,
	.attrib = gf117_grctx_generate_attrib,
	.attrib_nr_max = 0x240,
	.attrib_nr = 0x240,
	.alpha_nr_max = 0x648 + (0x648 / 2),
	.alpha_nr = 0x648,
	.sm_id = gf100_grctx_generate_sm_id,
	.tpc_nr = gf100_grctx_generate_tpc_nr,
	.rop_mapping = gf117_grctx_generate_rop_mapping,
	.alpha_beta_tables = gk104_grctx_generate_alpha_beta_tables,
};
