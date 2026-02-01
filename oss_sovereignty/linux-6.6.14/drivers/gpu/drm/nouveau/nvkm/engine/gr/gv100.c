 
#include "gf100.h"
#include "ctxgf100.h"

#include <nvif/class.h>

static void
gv100_gr_trap_sm(struct gf100_gr *gr, int gpc, int tpc, int sm)
{
	struct nvkm_subdev *subdev = &gr->base.engine.subdev;
	struct nvkm_device *device = subdev->device;
	u32 werr = nvkm_rd32(device, TPC_UNIT(gpc, tpc, 0x730 + (sm * 0x80)));
	u32 gerr = nvkm_rd32(device, TPC_UNIT(gpc, tpc, 0x734 + (sm * 0x80)));
	const struct nvkm_enum *warp;
	char glob[128];

	nvkm_snprintbf(glob, sizeof(glob), gf100_mp_global_error, gerr);
	warp = nvkm_enum_find(gf100_mp_warp_error, werr & 0xffff);

	nvkm_error(subdev, "GPC%i/TPC%i/SM%d trap: "
			   "global %08x [%s] warp %04x [%s]\n",
		   gpc, tpc, sm, gerr, glob, werr, warp ? warp->name : "");

	nvkm_wr32(device, TPC_UNIT(gpc, tpc, 0x730 + sm * 0x80), 0x00000000);
	nvkm_wr32(device, TPC_UNIT(gpc, tpc, 0x734 + sm * 0x80), gerr);
}

void
gv100_gr_trap_mp(struct gf100_gr *gr, int gpc, int tpc)
{
	gv100_gr_trap_sm(gr, gpc, tpc, 0);
	gv100_gr_trap_sm(gr, gpc, tpc, 1);
}

void
gv100_gr_init_4188a4(struct gf100_gr *gr)
{
	struct nvkm_device *device = gr->base.engine.subdev.device;

	nvkm_mask(device, 0x4188a4, 0x03000000, 0x03000000);
}

void
gv100_gr_init_shader_exceptions(struct gf100_gr *gr, int gpc, int tpc)
{
	struct nvkm_device *device = gr->base.engine.subdev.device;
	int sm;
	for (sm = 0; sm < 0x100; sm += 0x80) {
		nvkm_wr32(device, TPC_UNIT(gpc, tpc, 0x610), 0x00000001);
		nvkm_wr32(device, TPC_UNIT(gpc, tpc, 0x72c + sm), 0x00000004);
	}
}

void
gv100_gr_init_504430(struct gf100_gr *gr, int gpc, int tpc)
{
	struct nvkm_device *device = gr->base.engine.subdev.device;
	nvkm_wr32(device, TPC_UNIT(gpc, tpc, 0x430), 0x403f0000);
}

void
gv100_gr_init_419bd8(struct gf100_gr *gr)
{
	struct nvkm_device *device = gr->base.engine.subdev.device;
	nvkm_mask(device, 0x419bd8, 0x00000700, 0x00000000);
}

u32
gv100_gr_nonpes_aware_tpc(struct gf100_gr *gr, u32 gpc, u32 tpc)
{
	u32 pes, temp, tpc_new = 0;

	for (pes = 0; pes < gr->ppc_nr[gpc]; pes++) {
		if (gr->ppc_tpc_mask[gpc][pes] & BIT(tpc))
			break;

		tpc_new += gr->ppc_tpc_nr[gpc][pes];
	}

	temp = (BIT(tpc) - 1) & gr->ppc_tpc_mask[gpc][pes];
	temp = hweight32(temp);
	return tpc_new + temp;
}

static int
gv100_gr_scg_estimate_perf(struct gf100_gr *gr, unsigned long *gpc_tpc_mask,
			   u32 disable_gpc, u32 disable_tpc, int *perf)
{
	const u32 scale_factor = 512UL;		 
	const u32 pix_scale = 1024*1024UL;	 
	const u32 world_scale = 1024UL;		 
	const u32 tpc_scale = 1;		 
	u32 scg_num_pes = 0;
	u32 min_scg_gpc_pix_perf = scale_factor;  
	u32 average_tpcs = 0;  
	u32 deviation;  
	u32 norm_tpc_deviation;	 
	u32 tpc_balance;
	u32 scg_gpc_pix_perf;
	u32 scg_world_perf;
	u32 gpc;
	u32 pes;
	int diff;
	bool tpc_removed_gpc = false;
	bool tpc_removed_pes = false;
	u32 max_tpc_gpc = 0;
	u32 num_tpc_mask;
	u32 *num_tpc_gpc;
	int ret = -EINVAL;

	if (!(num_tpc_gpc = kcalloc(gr->gpc_nr, sizeof(*num_tpc_gpc), GFP_KERNEL)))
		return -ENOMEM;

	 
	for (gpc = 0; gpc < gr->gpc_nr; gpc++) {
		num_tpc_mask = gpc_tpc_mask[gpc];

		if ((gpc == disable_gpc) && num_tpc_mask & BIT(disable_tpc)) {
			 
			if (WARN_ON(tpc_removed_gpc))
				goto done;

			 
			num_tpc_mask &= ~BIT(disable_tpc);
			tpc_removed_gpc = true;
		}

		 
		num_tpc_gpc[gpc] = hweight32(num_tpc_mask);
		average_tpcs += num_tpc_gpc[gpc];

		 
		max_tpc_gpc = num_tpc_gpc[gpc] > max_tpc_gpc ? num_tpc_gpc[gpc] : max_tpc_gpc;

		 
		scg_gpc_pix_perf = scale_factor * num_tpc_gpc[gpc] / gr->tpc_nr[gpc];
		if (min_scg_gpc_pix_perf > scg_gpc_pix_perf)
			min_scg_gpc_pix_perf = scg_gpc_pix_perf;

		 
		for (pes = 0; pes < gr->ppc_nr[gpc]; pes++) {
			 
			num_tpc_mask = gr->ppc_tpc_mask[gpc][pes] & gpc_tpc_mask[gpc];

			if ((gpc == disable_gpc) && (num_tpc_mask & BIT(disable_tpc))) {
				if (WARN_ON(tpc_removed_pes))
					goto done;

				num_tpc_mask &= ~BIT(disable_tpc);
				tpc_removed_pes = true;
			}

			if (hweight32(num_tpc_mask))
				scg_num_pes++;
		}
	}

	if (WARN_ON(!tpc_removed_gpc || !tpc_removed_pes))
		goto done;

	if (max_tpc_gpc == 0) {
		*perf = 0;
		goto done_ok;
	}

	 
	scg_world_perf = (scale_factor * scg_num_pes) / gr->ppc_total;
	deviation = 0;
	average_tpcs = scale_factor * average_tpcs / gr->gpc_nr;
	for (gpc = 0; gpc < gr->gpc_nr; gpc++) {
		diff = average_tpcs - scale_factor * num_tpc_gpc[gpc];
		if (diff < 0)
			diff = -diff;

		deviation += diff;
	}

	deviation /= gr->gpc_nr;

	norm_tpc_deviation = deviation / max_tpc_gpc;

	tpc_balance = scale_factor - norm_tpc_deviation;

	if ((tpc_balance > scale_factor)          ||
	    (scg_world_perf > scale_factor)       ||
	    (min_scg_gpc_pix_perf > scale_factor) ||
	    (norm_tpc_deviation > scale_factor)) {
		WARN_ON(1);
		goto done;
	}

	*perf = (pix_scale * min_scg_gpc_pix_perf) +
		(world_scale * scg_world_perf) +
		(tpc_scale * tpc_balance);
done_ok:
	ret = 0;
done:
	kfree(num_tpc_gpc);
	return ret;
}

int
gv100_gr_oneinit_sm_id(struct gf100_gr *gr)
{
	unsigned long *gpc_tpc_mask;
	u32 *tpc_table, *gpc_table;
	u32 gpc, tpc, pes, gtpc;
	int perf, maxperf, ret = 0;

	gpc_tpc_mask = kcalloc(gr->gpc_nr, sizeof(*gpc_tpc_mask), GFP_KERNEL);
	gpc_table = kcalloc(gr->tpc_total, sizeof(*gpc_table), GFP_KERNEL);
	tpc_table = kcalloc(gr->tpc_total, sizeof(*tpc_table), GFP_KERNEL);
	if (!gpc_table || !tpc_table || !gpc_tpc_mask) {
		ret = -ENOMEM;
		goto done;
	}

	for (gpc = 0; gpc < gr->gpc_nr; gpc++) {
		for (pes = 0; pes < gr->ppc_nr[gpc]; pes++)
			gpc_tpc_mask[gpc] |= gr->ppc_tpc_mask[gpc][pes];
	}

	for (gtpc = 0; gtpc < gr->tpc_total; gtpc++) {
		for (maxperf = -1, gpc = 0; gpc < gr->gpc_nr; gpc++) {
			for_each_set_bit(tpc, &gpc_tpc_mask[gpc], gr->tpc_nr[gpc]) {
				ret = gv100_gr_scg_estimate_perf(gr, gpc_tpc_mask, gpc, tpc, &perf);
				if (ret)
					goto done;

				 
				if (perf > maxperf) {
					maxperf = perf;
					gpc_table[gtpc] = gpc;
					tpc_table[gtpc] = tpc;
				}
			}
		}

		gpc_tpc_mask[gpc_table[gtpc]] &= ~BIT(tpc_table[gtpc]);
	}

	 
	for (gtpc = 0; gtpc < gr->tpc_total; gtpc++) {
		gr->sm[gtpc].gpc = gpc_table[gtpc];
		gr->sm[gtpc].tpc = tpc_table[gtpc];
		gr->sm_nr++;
	}

done:
	kfree(gpc_table);
	kfree(tpc_table);
	kfree(gpc_tpc_mask);
	return ret;
}

static const struct gf100_gr_func
gv100_gr = {
	.oneinit_tiles = gm200_gr_oneinit_tiles,
	.oneinit_sm_id = gv100_gr_oneinit_sm_id,
	.init = gf100_gr_init,
	.init_419bd8 = gv100_gr_init_419bd8,
	.init_gpc_mmu = gm200_gr_init_gpc_mmu,
	.init_vsc_stream_master = gk104_gr_init_vsc_stream_master,
	.init_zcull = gf117_gr_init_zcull,
	.init_num_active_ltcs = gm200_gr_init_num_active_ltcs,
	.init_rop_active_fbps = gp100_gr_init_rop_active_fbps,
	.init_swdx_pes_mask = gp102_gr_init_swdx_pes_mask,
	.init_fecs_exceptions = gp100_gr_init_fecs_exceptions,
	.init_ds_hww_esr_2 = gm200_gr_init_ds_hww_esr_2,
	.init_sked_hww_esr = gk104_gr_init_sked_hww_esr,
	.init_ppc_exceptions = gk104_gr_init_ppc_exceptions,
	.init_504430 = gv100_gr_init_504430,
	.init_shader_exceptions = gv100_gr_init_shader_exceptions,
	.init_rop_exceptions = gf100_gr_init_rop_exceptions,
	.init_exception2 = gf100_gr_init_exception2,
	.init_4188a4 = gv100_gr_init_4188a4,
	.trap_mp = gv100_gr_trap_mp,
	.fecs.reset = gf100_gr_fecs_reset,
	.rops = gm200_gr_rops,
	.gpc_nr = 6,
	.tpc_nr = 7,
	.ppc_nr = 3,
	.grctx = &gv100_grctx,
	.zbc = &gp102_gr_zbc,
	.sclass = {
		{ -1, -1, FERMI_TWOD_A },
		{ -1, -1, KEPLER_INLINE_TO_MEMORY_B },
		{ -1, -1, VOLTA_A, &gf100_fermi },
		{ -1, -1, VOLTA_COMPUTE_A },
		{}
	}
};

MODULE_FIRMWARE("nvidia/gv100/gr/fecs_bl.bin");
MODULE_FIRMWARE("nvidia/gv100/gr/fecs_inst.bin");
MODULE_FIRMWARE("nvidia/gv100/gr/fecs_data.bin");
MODULE_FIRMWARE("nvidia/gv100/gr/fecs_sig.bin");
MODULE_FIRMWARE("nvidia/gv100/gr/gpccs_bl.bin");
MODULE_FIRMWARE("nvidia/gv100/gr/gpccs_inst.bin");
MODULE_FIRMWARE("nvidia/gv100/gr/gpccs_data.bin");
MODULE_FIRMWARE("nvidia/gv100/gr/gpccs_sig.bin");
MODULE_FIRMWARE("nvidia/gv100/gr/sw_ctx.bin");
MODULE_FIRMWARE("nvidia/gv100/gr/sw_nonctx.bin");
MODULE_FIRMWARE("nvidia/gv100/gr/sw_bundle_init.bin");
MODULE_FIRMWARE("nvidia/gv100/gr/sw_method_init.bin");

static const struct gf100_gr_fwif
gv100_gr_fwif[] = {
	{  0, gm200_gr_load, &gv100_gr, &gp108_gr_fecs_acr, &gp108_gr_gpccs_acr },
	{ -1, gm200_gr_nofw },
	{}
};

int
gv100_gr_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_gr **pgr)
{
	return gf100_gr_new_(gv100_gr_fwif, device, type, inst, pgr);
}
