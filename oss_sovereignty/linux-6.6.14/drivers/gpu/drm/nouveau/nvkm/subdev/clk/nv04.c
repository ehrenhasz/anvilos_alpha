 
#include "priv.h"
#include "pll.h"

#include <subdev/bios.h>
#include <subdev/bios/pll.h>
#include <subdev/devinit/nv04.h>

int
nv04_clk_pll_calc(struct nvkm_clk *clock, struct nvbios_pll *info,
		  int clk, struct nvkm_pll_vals *pv)
{
	int N1, M1, N2, M2, P;
	int ret = nv04_pll_calc(&clock->subdev, info, clk, &N1, &M1, &N2, &M2, &P);
	if (ret) {
		pv->refclk = info->refclk;
		pv->N1 = N1;
		pv->M1 = M1;
		pv->N2 = N2;
		pv->M2 = M2;
		pv->log2P = P;
	}
	return ret;
}

int
nv04_clk_pll_prog(struct nvkm_clk *clk, u32 reg1, struct nvkm_pll_vals *pv)
{
	struct nvkm_device *device = clk->subdev.device;
	struct nvkm_devinit *devinit = device->devinit;
	int cv = device->bios->version.chip;

	if (cv == 0x30 || cv == 0x31 || cv == 0x35 || cv == 0x36 ||
	    cv >= 0x40) {
		if (reg1 > 0x405c)
			setPLL_double_highregs(devinit, reg1, pv);
		else
			setPLL_double_lowregs(devinit, reg1, pv);
	} else
		setPLL_single(devinit, reg1, pv);

	return 0;
}

static const struct nvkm_clk_func
nv04_clk = {
	.domains = {
		{ nv_clk_src_max }
	}
};

int
nv04_clk_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	     struct nvkm_clk **pclk)
{
	int ret = nvkm_clk_new_(&nv04_clk, device, type, inst, false, pclk);
	if (ret == 0) {
		(*pclk)->pll_calc = nv04_clk_pll_calc;
		(*pclk)->pll_prog = nv04_clk_pll_prog;
	}
	return ret;
}
