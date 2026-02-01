 
#include "priv.h"
#include "chan.h"
#include "head.h"
#include "ior.h"

#include <nvif/class.h>

static const struct nvkm_ior_func
gp100_sor = {
	.route = {
		.get = gm200_sor_route_get,
		.set = gm200_sor_route_set,
	},
	.state = gf119_sor_state,
	.power = nv50_sor_power,
	.clock = gf119_sor_clock,
	.hdmi = &gm200_sor_hdmi,
	.dp = &gm200_sor_dp,
	.hda = &gf119_sor_hda,
};

int
gp100_sor_new(struct nvkm_disp *disp, int id)
{
	struct nvkm_device *device = disp->engine.subdev.device;
	u32 hda;

	if (!((hda = nvkm_rd32(device, 0x08a15c)) & 0x40000000))
		hda = nvkm_rd32(device, 0x10ebb0) >> 8;

	return nvkm_ior_new_(&gp100_sor, disp, SOR, id, hda & BIT(id));
}

static const struct nvkm_disp_func
gp100_disp = {
	.oneinit = nv50_disp_oneinit,
	.init = gf119_disp_init,
	.fini = gf119_disp_fini,
	.intr = gf119_disp_intr,
	.intr_error = gf119_disp_intr_error,
	.super = gf119_disp_super,
	.uevent = &gf119_disp_chan_uevent,
	.head = { .cnt = gf119_head_cnt, .new = gf119_head_new },
	.sor = { .cnt = gf119_sor_cnt, .new = gp100_sor_new },
	.root = { 0,0,GP100_DISP },
	.user = {
		{{0,0,GK104_DISP_CURSOR             }, nvkm_disp_chan_new, &gf119_disp_curs },
		{{0,0,GK104_DISP_OVERLAY            }, nvkm_disp_chan_new, &gf119_disp_oimm },
		{{0,0,GK110_DISP_BASE_CHANNEL_DMA   }, nvkm_disp_chan_new, &gf119_disp_base },
		{{0,0,GP100_DISP_CORE_CHANNEL_DMA   }, nvkm_disp_core_new, &gk104_disp_core },
		{{0,0,GK104_DISP_OVERLAY_CONTROL_DMA}, nvkm_disp_chan_new, &gk104_disp_ovly },
		{}
	},
};

int
gp100_disp_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	       struct nvkm_disp **pdisp)
{
	return nvkm_disp_new_(&gp100_disp, device, type, inst, pdisp);
}
