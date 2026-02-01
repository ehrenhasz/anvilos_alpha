 

#include "nouveau_drv.h"
#include "hw.h"

#include <subdev/bios/pll.h>
#include <nvif/timer.h>

#define CHIPSET_NFORCE 0x01a0
#define CHIPSET_NFORCE2 0x01f0

 

void
NVWriteVgaSeq(struct drm_device *dev, int head, uint8_t index, uint8_t value)
{
	NVWritePRMVIO(dev, head, NV_PRMVIO_SRX, index);
	NVWritePRMVIO(dev, head, NV_PRMVIO_SR, value);
}

uint8_t
NVReadVgaSeq(struct drm_device *dev, int head, uint8_t index)
{
	NVWritePRMVIO(dev, head, NV_PRMVIO_SRX, index);
	return NVReadPRMVIO(dev, head, NV_PRMVIO_SR);
}

void
NVWriteVgaGr(struct drm_device *dev, int head, uint8_t index, uint8_t value)
{
	NVWritePRMVIO(dev, head, NV_PRMVIO_GRX, index);
	NVWritePRMVIO(dev, head, NV_PRMVIO_GX, value);
}

uint8_t
NVReadVgaGr(struct drm_device *dev, int head, uint8_t index)
{
	NVWritePRMVIO(dev, head, NV_PRMVIO_GRX, index);
	return NVReadPRMVIO(dev, head, NV_PRMVIO_GX);
}

 
void
NVSetOwner(struct drm_device *dev, int owner)
{
	struct nouveau_drm *drm = nouveau_drm(dev);

	if (owner == 1)
		owner *= 3;

	if (drm->client.device.info.chipset == 0x11) {
		 
		NVReadVgaCrtc(dev, 0, NV_CIO_SR_LOCK_INDEX);
		NVReadVgaCrtc(dev, 1, NV_CIO_SR_LOCK_INDEX);
	}

	 
	NVWriteVgaCrtc(dev, 0, NV_CIO_CRE_44, owner);

	if (drm->client.device.info.chipset == 0x11) {	 
		NVWriteVgaCrtc(dev, 0, NV_CIO_CRE_2E, owner);
		NVWriteVgaCrtc(dev, 0, NV_CIO_CRE_2E, owner);
	}
}

void
NVBlankScreen(struct drm_device *dev, int head, bool blank)
{
	unsigned char seq1;

	if (nv_two_heads(dev))
		NVSetOwner(dev, head);

	seq1 = NVReadVgaSeq(dev, head, NV_VIO_SR_CLOCK_INDEX);

	NVVgaSeqReset(dev, head, true);
	if (blank)
		NVWriteVgaSeq(dev, head, NV_VIO_SR_CLOCK_INDEX, seq1 | 0x20);
	else
		NVWriteVgaSeq(dev, head, NV_VIO_SR_CLOCK_INDEX, seq1 & ~0x20);
	NVVgaSeqReset(dev, head, false);
}

 

static void
nouveau_hw_decode_pll(struct drm_device *dev, uint32_t reg1, uint32_t pll1,
		      uint32_t pll2, struct nvkm_pll_vals *pllvals)
{
	struct nouveau_drm *drm = nouveau_drm(dev);

	 

	 
	pllvals->log2P = (pll1 >> 16) & 0x7;
	pllvals->N2 = pllvals->M2 = 1;

	if (reg1 <= 0x405c) {
		pllvals->NM1 = pll2 & 0xffff;
		 
		if (!(pll1 & 0x1100))
			pllvals->NM2 = pll2 >> 16;
	} else {
		pllvals->NM1 = pll1 & 0xffff;
		if (nv_two_reg_pll(dev) && pll2 & NV31_RAMDAC_ENABLE_VCO2)
			pllvals->NM2 = pll2 & 0xffff;
		else if (drm->client.device.info.chipset == 0x30 || drm->client.device.info.chipset == 0x35) {
			pllvals->M1 &= 0xf;  
			if (pll1 & NV30_RAMDAC_ENABLE_VCO2) {
				pllvals->M2 = (pll1 >> 4) & 0x7;
				pllvals->N2 = ((pll1 >> 21) & 0x18) |
					      ((pll1 >> 19) & 0x7);
			}
		}
	}
}

int
nouveau_hw_get_pllvals(struct drm_device *dev, enum nvbios_pll_type plltype,
		       struct nvkm_pll_vals *pllvals)
{
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvif_object *device = &drm->client.device.object;
	struct nvkm_bios *bios = nvxx_bios(&drm->client.device);
	uint32_t reg1, pll1, pll2 = 0;
	struct nvbios_pll pll_lim;
	int ret;

	ret = nvbios_pll_parse(bios, plltype, &pll_lim);
	if (ret || !(reg1 = pll_lim.reg))
		return -ENOENT;

	pll1 = nvif_rd32(device, reg1);
	if (reg1 <= 0x405c)
		pll2 = nvif_rd32(device, reg1 + 4);
	else if (nv_two_reg_pll(dev)) {
		uint32_t reg2 = reg1 + (reg1 == NV_RAMDAC_VPLL2 ? 0x5c : 0x70);

		pll2 = nvif_rd32(device, reg2);
	}

	if (drm->client.device.info.family == NV_DEVICE_INFO_V0_CELSIUS && reg1 >= NV_PRAMDAC_VPLL_COEFF) {
		uint32_t ramdac580 = NVReadRAMDAC(dev, 0, NV_PRAMDAC_580);

		 
		if (reg1 == NV_PRAMDAC_VPLL_COEFF) {
			if (ramdac580 & NV_RAMDAC_580_VPLL1_ACTIVE)
				pll2 = 0;
		} else
			if (ramdac580 & NV_RAMDAC_580_VPLL2_ACTIVE)
				pll2 = 0;
	}

	nouveau_hw_decode_pll(dev, reg1, pll1, pll2, pllvals);
	pllvals->refclk = pll_lim.refclk;
	return 0;
}

int
nouveau_hw_pllvals_to_clk(struct nvkm_pll_vals *pv)
{
	 
	if (!pv->M1 || !pv->M2)
		return 0;

	return pv->N1 * pv->N2 * pv->refclk / (pv->M1 * pv->M2) >> pv->log2P;
}

int
nouveau_hw_get_clock(struct drm_device *dev, enum nvbios_pll_type plltype)
{
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	struct nvkm_pll_vals pllvals;
	int ret;
	int domain;

	domain = pci_domain_nr(pdev->bus);

	if (plltype == PLL_MEMORY &&
	    (pdev->device & 0x0ff0) == CHIPSET_NFORCE) {
		uint32_t mpllP;
		pci_read_config_dword(pci_get_domain_bus_and_slot(domain, 0, 3),
				      0x6c, &mpllP);
		mpllP = (mpllP >> 8) & 0xf;
		if (!mpllP)
			mpllP = 4;

		return 400000 / mpllP;
	} else
	if (plltype == PLL_MEMORY &&
	    (pdev->device & 0xff0) == CHIPSET_NFORCE2) {
		uint32_t clock;

		pci_read_config_dword(pci_get_domain_bus_and_slot(domain, 0, 5),
				      0x4c, &clock);
		return clock / 1000;
	}

	ret = nouveau_hw_get_pllvals(dev, plltype, &pllvals);
	if (ret)
		return ret;

	return nouveau_hw_pllvals_to_clk(&pllvals);
}

static void
nouveau_hw_fix_bad_vpll(struct drm_device *dev, int head)
{
	 

	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvif_device *device = &drm->client.device;
	struct nvkm_clk *clk = nvxx_clk(device);
	struct nvkm_bios *bios = nvxx_bios(device);
	struct nvbios_pll pll_lim;
	struct nvkm_pll_vals pv;
	enum nvbios_pll_type pll = head ? PLL_VPLL1 : PLL_VPLL0;

	if (nvbios_pll_parse(bios, pll, &pll_lim))
		return;
	nouveau_hw_get_pllvals(dev, pll, &pv);

	if (pv.M1 >= pll_lim.vco1.min_m && pv.M1 <= pll_lim.vco1.max_m &&
	    pv.N1 >= pll_lim.vco1.min_n && pv.N1 <= pll_lim.vco1.max_n &&
	    pv.log2P <= pll_lim.max_p)
		return;

	NV_WARN(drm, "VPLL %d outwith limits, attempting to fix\n", head + 1);

	 
	pv.M1 = pll_lim.vco1.max_m;
	pv.N1 = pll_lim.vco1.min_n;
	pv.log2P = pll_lim.max_p_usable;
	clk->pll_prog(clk, pll_lim.reg, &pv);
}

 

static void nouveau_vga_font_io(struct drm_device *dev,
				void __iomem *iovram,
				bool save, unsigned plane)
{
	unsigned i;

	NVWriteVgaSeq(dev, 0, NV_VIO_SR_PLANE_MASK_INDEX, 1 << plane);
	NVWriteVgaGr(dev, 0, NV_VIO_GX_READ_MAP_INDEX, plane);
	for (i = 0; i < 16384; i++) {
		if (save) {
			nv04_display(dev)->saved_vga_font[plane][i] =
					ioread32_native(iovram + i * 4);
		} else {
			iowrite32_native(nv04_display(dev)->saved_vga_font[plane][i],
							iovram + i * 4);
		}
	}
}

void
nouveau_hw_save_vga_fonts(struct drm_device *dev, bool save)
{
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct pci_dev *pdev = to_pci_dev(dev->dev);
	uint8_t misc, gr4, gr5, gr6, seq2, seq4;
	bool graphicsmode;
	unsigned plane;
	void __iomem *iovram;

	if (nv_two_heads(dev))
		NVSetOwner(dev, 0);

	NVSetEnablePalette(dev, 0, true);
	graphicsmode = NVReadVgaAttr(dev, 0, NV_CIO_AR_MODE_INDEX) & 1;
	NVSetEnablePalette(dev, 0, false);

	if (graphicsmode)  
		return;

	NV_INFO(drm, "%sing VGA fonts\n", save ? "Sav" : "Restor");

	 
	iovram = ioremap(pci_resource_start(pdev, 1), 65536);
	if (!iovram) {
		NV_ERROR(drm, "Failed to map VRAM, "
					"cannot save/restore VGA fonts.\n");
		return;
	}

	if (nv_two_heads(dev))
		NVBlankScreen(dev, 1, true);
	NVBlankScreen(dev, 0, true);

	 
	misc = NVReadPRMVIO(dev, 0, NV_PRMVIO_MISC__READ);
	seq2 = NVReadVgaSeq(dev, 0, NV_VIO_SR_PLANE_MASK_INDEX);
	seq4 = NVReadVgaSeq(dev, 0, NV_VIO_SR_MEM_MODE_INDEX);
	gr4 = NVReadVgaGr(dev, 0, NV_VIO_GX_READ_MAP_INDEX);
	gr5 = NVReadVgaGr(dev, 0, NV_VIO_GX_MODE_INDEX);
	gr6 = NVReadVgaGr(dev, 0, NV_VIO_GX_MISC_INDEX);

	NVWritePRMVIO(dev, 0, NV_PRMVIO_MISC__WRITE, 0x67);
	NVWriteVgaSeq(dev, 0, NV_VIO_SR_MEM_MODE_INDEX, 0x6);
	NVWriteVgaGr(dev, 0, NV_VIO_GX_MODE_INDEX, 0x0);
	NVWriteVgaGr(dev, 0, NV_VIO_GX_MISC_INDEX, 0x5);

	 
	for (plane = 0; plane < 4; plane++)
		nouveau_vga_font_io(dev, iovram, save, plane);

	 
	NVWritePRMVIO(dev, 0, NV_PRMVIO_MISC__WRITE, misc);
	NVWriteVgaGr(dev, 0, NV_VIO_GX_READ_MAP_INDEX, gr4);
	NVWriteVgaGr(dev, 0, NV_VIO_GX_MODE_INDEX, gr5);
	NVWriteVgaGr(dev, 0, NV_VIO_GX_MISC_INDEX, gr6);
	NVWriteVgaSeq(dev, 0, NV_VIO_SR_PLANE_MASK_INDEX, seq2);
	NVWriteVgaSeq(dev, 0, NV_VIO_SR_MEM_MODE_INDEX, seq4);

	if (nv_two_heads(dev))
		NVBlankScreen(dev, 1, false);
	NVBlankScreen(dev, 0, false);

	iounmap(iovram);
}

 

static void
rd_cio_state(struct drm_device *dev, int head,
	     struct nv04_crtc_reg *crtcstate, int index)
{
	crtcstate->CRTC[index] = NVReadVgaCrtc(dev, head, index);
}

static void
wr_cio_state(struct drm_device *dev, int head,
	     struct nv04_crtc_reg *crtcstate, int index)
{
	NVWriteVgaCrtc(dev, head, index, crtcstate->CRTC[index]);
}

static void
nv_save_state_ramdac(struct drm_device *dev, int head,
		     struct nv04_mode_state *state)
{
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nv04_crtc_reg *regp = &state->crtc_reg[head];
	int i;

	if (drm->client.device.info.family >= NV_DEVICE_INFO_V0_CELSIUS)
		regp->nv10_cursync = NVReadRAMDAC(dev, head, NV_RAMDAC_NV10_CURSYNC);

	nouveau_hw_get_pllvals(dev, head ? PLL_VPLL1 : PLL_VPLL0, &regp->pllvals);
	state->pllsel = NVReadRAMDAC(dev, 0, NV_PRAMDAC_PLL_COEFF_SELECT);
	if (nv_two_heads(dev))
		state->sel_clk = NVReadRAMDAC(dev, 0, NV_PRAMDAC_SEL_CLK);
	if (drm->client.device.info.chipset == 0x11)
		regp->dither = NVReadRAMDAC(dev, head, NV_RAMDAC_DITHER_NV11);

	regp->ramdac_gen_ctrl = NVReadRAMDAC(dev, head, NV_PRAMDAC_GENERAL_CONTROL);

	if (nv_gf4_disp_arch(dev))
		regp->ramdac_630 = NVReadRAMDAC(dev, head, NV_PRAMDAC_630);
	if (drm->client.device.info.chipset >= 0x30)
		regp->ramdac_634 = NVReadRAMDAC(dev, head, NV_PRAMDAC_634);

	regp->tv_setup = NVReadRAMDAC(dev, head, NV_PRAMDAC_TV_SETUP);
	regp->tv_vtotal = NVReadRAMDAC(dev, head, NV_PRAMDAC_TV_VTOTAL);
	regp->tv_vskew = NVReadRAMDAC(dev, head, NV_PRAMDAC_TV_VSKEW);
	regp->tv_vsync_delay = NVReadRAMDAC(dev, head, NV_PRAMDAC_TV_VSYNC_DELAY);
	regp->tv_htotal = NVReadRAMDAC(dev, head, NV_PRAMDAC_TV_HTOTAL);
	regp->tv_hskew = NVReadRAMDAC(dev, head, NV_PRAMDAC_TV_HSKEW);
	regp->tv_hsync_delay = NVReadRAMDAC(dev, head, NV_PRAMDAC_TV_HSYNC_DELAY);
	regp->tv_hsync_delay2 = NVReadRAMDAC(dev, head, NV_PRAMDAC_TV_HSYNC_DELAY2);

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_PRAMDAC_FP_VDISPLAY_END + (i * 4);
		regp->fp_vert_regs[i] = NVReadRAMDAC(dev, head, ramdac_reg);
		regp->fp_horiz_regs[i] = NVReadRAMDAC(dev, head, ramdac_reg + 0x20);
	}

	if (nv_gf4_disp_arch(dev)) {
		regp->dither = NVReadRAMDAC(dev, head, NV_RAMDAC_FP_DITHER);
		for (i = 0; i < 3; i++) {
			regp->dither_regs[i] = NVReadRAMDAC(dev, head, NV_PRAMDAC_850 + i * 4);
			regp->dither_regs[i + 3] = NVReadRAMDAC(dev, head, NV_PRAMDAC_85C + i * 4);
		}
	}

	regp->fp_control = NVReadRAMDAC(dev, head, NV_PRAMDAC_FP_TG_CONTROL);
	regp->fp_debug_0 = NVReadRAMDAC(dev, head, NV_PRAMDAC_FP_DEBUG_0);
	if (!nv_gf4_disp_arch(dev) && head == 0) {
		 
		NVWriteRAMDAC(dev, 0, NV_PRAMDAC_FP_DEBUG_0, regp->fp_debug_0 &
			      ~NV_PRAMDAC_FP_DEBUG_0_PWRDOWN_FPCLK);
	}
	regp->fp_debug_1 = NVReadRAMDAC(dev, head, NV_PRAMDAC_FP_DEBUG_1);
	regp->fp_debug_2 = NVReadRAMDAC(dev, head, NV_PRAMDAC_FP_DEBUG_2);

	regp->fp_margin_color = NVReadRAMDAC(dev, head, NV_PRAMDAC_FP_MARGIN_COLOR);

	if (nv_gf4_disp_arch(dev))
		regp->ramdac_8c0 = NVReadRAMDAC(dev, head, NV_PRAMDAC_8C0);

	if (drm->client.device.info.family == NV_DEVICE_INFO_V0_CURIE) {
		regp->ramdac_a20 = NVReadRAMDAC(dev, head, NV_PRAMDAC_A20);
		regp->ramdac_a24 = NVReadRAMDAC(dev, head, NV_PRAMDAC_A24);
		regp->ramdac_a34 = NVReadRAMDAC(dev, head, NV_PRAMDAC_A34);

		for (i = 0; i < 38; i++)
			regp->ctv_regs[i] = NVReadRAMDAC(dev, head,
							 NV_PRAMDAC_CTV + 4*i);
	}
}

static void
nv_load_state_ramdac(struct drm_device *dev, int head,
		     struct nv04_mode_state *state)
{
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvkm_clk *clk = nvxx_clk(&drm->client.device);
	struct nv04_crtc_reg *regp = &state->crtc_reg[head];
	uint32_t pllreg = head ? NV_RAMDAC_VPLL2 : NV_PRAMDAC_VPLL_COEFF;
	int i;

	if (drm->client.device.info.family >= NV_DEVICE_INFO_V0_CELSIUS)
		NVWriteRAMDAC(dev, head, NV_RAMDAC_NV10_CURSYNC, regp->nv10_cursync);

	clk->pll_prog(clk, pllreg, &regp->pllvals);
	NVWriteRAMDAC(dev, 0, NV_PRAMDAC_PLL_COEFF_SELECT, state->pllsel);
	if (nv_two_heads(dev))
		NVWriteRAMDAC(dev, 0, NV_PRAMDAC_SEL_CLK, state->sel_clk);
	if (drm->client.device.info.chipset == 0x11)
		NVWriteRAMDAC(dev, head, NV_RAMDAC_DITHER_NV11, regp->dither);

	NVWriteRAMDAC(dev, head, NV_PRAMDAC_GENERAL_CONTROL, regp->ramdac_gen_ctrl);

	if (nv_gf4_disp_arch(dev))
		NVWriteRAMDAC(dev, head, NV_PRAMDAC_630, regp->ramdac_630);
	if (drm->client.device.info.chipset >= 0x30)
		NVWriteRAMDAC(dev, head, NV_PRAMDAC_634, regp->ramdac_634);

	NVWriteRAMDAC(dev, head, NV_PRAMDAC_TV_SETUP, regp->tv_setup);
	NVWriteRAMDAC(dev, head, NV_PRAMDAC_TV_VTOTAL, regp->tv_vtotal);
	NVWriteRAMDAC(dev, head, NV_PRAMDAC_TV_VSKEW, regp->tv_vskew);
	NVWriteRAMDAC(dev, head, NV_PRAMDAC_TV_VSYNC_DELAY, regp->tv_vsync_delay);
	NVWriteRAMDAC(dev, head, NV_PRAMDAC_TV_HTOTAL, regp->tv_htotal);
	NVWriteRAMDAC(dev, head, NV_PRAMDAC_TV_HSKEW, regp->tv_hskew);
	NVWriteRAMDAC(dev, head, NV_PRAMDAC_TV_HSYNC_DELAY, regp->tv_hsync_delay);
	NVWriteRAMDAC(dev, head, NV_PRAMDAC_TV_HSYNC_DELAY2, regp->tv_hsync_delay2);

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_PRAMDAC_FP_VDISPLAY_END + (i * 4);

		NVWriteRAMDAC(dev, head, ramdac_reg, regp->fp_vert_regs[i]);
		NVWriteRAMDAC(dev, head, ramdac_reg + 0x20, regp->fp_horiz_regs[i]);
	}

	if (nv_gf4_disp_arch(dev)) {
		NVWriteRAMDAC(dev, head, NV_RAMDAC_FP_DITHER, regp->dither);
		for (i = 0; i < 3; i++) {
			NVWriteRAMDAC(dev, head, NV_PRAMDAC_850 + i * 4, regp->dither_regs[i]);
			NVWriteRAMDAC(dev, head, NV_PRAMDAC_85C + i * 4, regp->dither_regs[i + 3]);
		}
	}

	NVWriteRAMDAC(dev, head, NV_PRAMDAC_FP_TG_CONTROL, regp->fp_control);
	NVWriteRAMDAC(dev, head, NV_PRAMDAC_FP_DEBUG_0, regp->fp_debug_0);
	NVWriteRAMDAC(dev, head, NV_PRAMDAC_FP_DEBUG_1, regp->fp_debug_1);
	NVWriteRAMDAC(dev, head, NV_PRAMDAC_FP_DEBUG_2, regp->fp_debug_2);

	NVWriteRAMDAC(dev, head, NV_PRAMDAC_FP_MARGIN_COLOR, regp->fp_margin_color);

	if (nv_gf4_disp_arch(dev))
		NVWriteRAMDAC(dev, head, NV_PRAMDAC_8C0, regp->ramdac_8c0);

	if (drm->client.device.info.family == NV_DEVICE_INFO_V0_CURIE) {
		NVWriteRAMDAC(dev, head, NV_PRAMDAC_A20, regp->ramdac_a20);
		NVWriteRAMDAC(dev, head, NV_PRAMDAC_A24, regp->ramdac_a24);
		NVWriteRAMDAC(dev, head, NV_PRAMDAC_A34, regp->ramdac_a34);

		for (i = 0; i < 38; i++)
			NVWriteRAMDAC(dev, head,
				      NV_PRAMDAC_CTV + 4*i, regp->ctv_regs[i]);
	}
}

static void
nv_save_state_vga(struct drm_device *dev, int head,
		  struct nv04_mode_state *state)
{
	struct nv04_crtc_reg *regp = &state->crtc_reg[head];
	int i;

	regp->MiscOutReg = NVReadPRMVIO(dev, head, NV_PRMVIO_MISC__READ);

	for (i = 0; i < 25; i++)
		rd_cio_state(dev, head, regp, i);

	NVSetEnablePalette(dev, head, true);
	for (i = 0; i < 21; i++)
		regp->Attribute[i] = NVReadVgaAttr(dev, head, i);
	NVSetEnablePalette(dev, head, false);

	for (i = 0; i < 9; i++)
		regp->Graphics[i] = NVReadVgaGr(dev, head, i);

	for (i = 0; i < 5; i++)
		regp->Sequencer[i] = NVReadVgaSeq(dev, head, i);
}

static void
nv_load_state_vga(struct drm_device *dev, int head,
		  struct nv04_mode_state *state)
{
	struct nv04_crtc_reg *regp = &state->crtc_reg[head];
	int i;

	NVWritePRMVIO(dev, head, NV_PRMVIO_MISC__WRITE, regp->MiscOutReg);

	for (i = 0; i < 5; i++)
		NVWriteVgaSeq(dev, head, i, regp->Sequencer[i]);

	nv_lock_vga_crtc_base(dev, head, false);
	for (i = 0; i < 25; i++)
		wr_cio_state(dev, head, regp, i);
	nv_lock_vga_crtc_base(dev, head, true);

	for (i = 0; i < 9; i++)
		NVWriteVgaGr(dev, head, i, regp->Graphics[i]);

	NVSetEnablePalette(dev, head, true);
	for (i = 0; i < 21; i++)
		NVWriteVgaAttr(dev, head, i, regp->Attribute[i]);
	NVSetEnablePalette(dev, head, false);
}

static void
nv_save_state_ext(struct drm_device *dev, int head,
		  struct nv04_mode_state *state)
{
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nv04_crtc_reg *regp = &state->crtc_reg[head];
	int i;

	rd_cio_state(dev, head, regp, NV_CIO_CRE_LCD__INDEX);
	rd_cio_state(dev, head, regp, NV_CIO_CRE_RPC0_INDEX);
	rd_cio_state(dev, head, regp, NV_CIO_CRE_RPC1_INDEX);
	rd_cio_state(dev, head, regp, NV_CIO_CRE_LSR_INDEX);
	rd_cio_state(dev, head, regp, NV_CIO_CRE_PIXEL_INDEX);
	rd_cio_state(dev, head, regp, NV_CIO_CRE_HEB__INDEX);
	rd_cio_state(dev, head, regp, NV_CIO_CRE_ENH_INDEX);

	rd_cio_state(dev, head, regp, NV_CIO_CRE_FF_INDEX);
	rd_cio_state(dev, head, regp, NV_CIO_CRE_FFLWM__INDEX);
	rd_cio_state(dev, head, regp, NV_CIO_CRE_21);

	if (drm->client.device.info.family >= NV_DEVICE_INFO_V0_KELVIN)
		rd_cio_state(dev, head, regp, NV_CIO_CRE_47);

	if (drm->client.device.info.family >= NV_DEVICE_INFO_V0_RANKINE)
		rd_cio_state(dev, head, regp, 0x9f);

	rd_cio_state(dev, head, regp, NV_CIO_CRE_49);
	rd_cio_state(dev, head, regp, NV_CIO_CRE_HCUR_ADDR0_INDEX);
	rd_cio_state(dev, head, regp, NV_CIO_CRE_HCUR_ADDR1_INDEX);
	rd_cio_state(dev, head, regp, NV_CIO_CRE_HCUR_ADDR2_INDEX);
	rd_cio_state(dev, head, regp, NV_CIO_CRE_ILACE__INDEX);

	if (drm->client.device.info.family >= NV_DEVICE_INFO_V0_CELSIUS) {
		regp->crtc_830 = NVReadCRTC(dev, head, NV_PCRTC_830);
		regp->crtc_834 = NVReadCRTC(dev, head, NV_PCRTC_834);

		if (drm->client.device.info.family >= NV_DEVICE_INFO_V0_RANKINE)
			regp->gpio_ext = NVReadCRTC(dev, head, NV_PCRTC_GPIO_EXT);

		if (drm->client.device.info.family == NV_DEVICE_INFO_V0_CURIE)
			regp->crtc_850 = NVReadCRTC(dev, head, NV_PCRTC_850);

		if (nv_two_heads(dev))
			regp->crtc_eng_ctrl = NVReadCRTC(dev, head, NV_PCRTC_ENGINE_CTRL);
		regp->cursor_cfg = NVReadCRTC(dev, head, NV_PCRTC_CURSOR_CONFIG);
	}

	regp->crtc_cfg = NVReadCRTC(dev, head, NV_PCRTC_CONFIG);

	rd_cio_state(dev, head, regp, NV_CIO_CRE_SCRATCH3__INDEX);
	rd_cio_state(dev, head, regp, NV_CIO_CRE_SCRATCH4__INDEX);
	if (drm->client.device.info.family >= NV_DEVICE_INFO_V0_CELSIUS) {
		rd_cio_state(dev, head, regp, NV_CIO_CRE_EBR_INDEX);
		rd_cio_state(dev, head, regp, NV_CIO_CRE_CSB);
		rd_cio_state(dev, head, regp, NV_CIO_CRE_4B);
		rd_cio_state(dev, head, regp, NV_CIO_CRE_TVOUT_LATENCY);
	}
	 
	if (nv_gf4_disp_arch(dev)) {
		rd_cio_state(dev, head, regp, NV_CIO_CRE_42);
		rd_cio_state(dev, head, regp, NV_CIO_CRE_53);
		rd_cio_state(dev, head, regp, NV_CIO_CRE_54);

		for (i = 0; i < 0x10; i++)
			regp->CR58[i] = NVReadVgaCrtc5758(dev, head, i);
		rd_cio_state(dev, head, regp, NV_CIO_CRE_59);
		rd_cio_state(dev, head, regp, NV_CIO_CRE_5B);

		rd_cio_state(dev, head, regp, NV_CIO_CRE_85);
		rd_cio_state(dev, head, regp, NV_CIO_CRE_86);
	}

	regp->fb_start = NVReadCRTC(dev, head, NV_PCRTC_START);
}

static void
nv_load_state_ext(struct drm_device *dev, int head,
		  struct nv04_mode_state *state)
{
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct nvif_object *device = &drm->client.device.object;
	struct nv04_crtc_reg *regp = &state->crtc_reg[head];
	uint32_t reg900;
	int i;

	if (drm->client.device.info.family >= NV_DEVICE_INFO_V0_CELSIUS) {
		if (nv_two_heads(dev))
			 
			NVWriteCRTC(dev, head, NV_PCRTC_ENGINE_CTRL, regp->crtc_eng_ctrl);

		nvif_wr32(device, NV_PVIDEO_STOP, 1);
		nvif_wr32(device, NV_PVIDEO_INTR_EN, 0);
		nvif_wr32(device, NV_PVIDEO_OFFSET_BUFF(0), 0);
		nvif_wr32(device, NV_PVIDEO_OFFSET_BUFF(1), 0);
		nvif_wr32(device, NV_PVIDEO_LIMIT(0), drm->client.device.info.ram_size - 1);
		nvif_wr32(device, NV_PVIDEO_LIMIT(1), drm->client.device.info.ram_size - 1);
		nvif_wr32(device, NV_PVIDEO_UVPLANE_LIMIT(0), drm->client.device.info.ram_size - 1);
		nvif_wr32(device, NV_PVIDEO_UVPLANE_LIMIT(1), drm->client.device.info.ram_size - 1);
		nvif_wr32(device, NV_PBUS_POWERCTRL_2, 0);

		NVWriteCRTC(dev, head, NV_PCRTC_CURSOR_CONFIG, regp->cursor_cfg);
		NVWriteCRTC(dev, head, NV_PCRTC_830, regp->crtc_830);
		NVWriteCRTC(dev, head, NV_PCRTC_834, regp->crtc_834);

		if (drm->client.device.info.family >= NV_DEVICE_INFO_V0_RANKINE)
			NVWriteCRTC(dev, head, NV_PCRTC_GPIO_EXT, regp->gpio_ext);

		if (drm->client.device.info.family == NV_DEVICE_INFO_V0_CURIE) {
			NVWriteCRTC(dev, head, NV_PCRTC_850, regp->crtc_850);

			reg900 = NVReadRAMDAC(dev, head, NV_PRAMDAC_900);
			if (regp->crtc_cfg == NV10_PCRTC_CONFIG_START_ADDRESS_HSYNC)
				NVWriteRAMDAC(dev, head, NV_PRAMDAC_900, reg900 | 0x10000);
			else
				NVWriteRAMDAC(dev, head, NV_PRAMDAC_900, reg900 & ~0x10000);
		}
	}

	NVWriteCRTC(dev, head, NV_PCRTC_CONFIG, regp->crtc_cfg);

	wr_cio_state(dev, head, regp, NV_CIO_CRE_RPC0_INDEX);
	wr_cio_state(dev, head, regp, NV_CIO_CRE_RPC1_INDEX);
	wr_cio_state(dev, head, regp, NV_CIO_CRE_LSR_INDEX);
	wr_cio_state(dev, head, regp, NV_CIO_CRE_PIXEL_INDEX);
	wr_cio_state(dev, head, regp, NV_CIO_CRE_LCD__INDEX);
	wr_cio_state(dev, head, regp, NV_CIO_CRE_HEB__INDEX);
	wr_cio_state(dev, head, regp, NV_CIO_CRE_ENH_INDEX);
	wr_cio_state(dev, head, regp, NV_CIO_CRE_FF_INDEX);
	wr_cio_state(dev, head, regp, NV_CIO_CRE_FFLWM__INDEX);

	if (drm->client.device.info.family >= NV_DEVICE_INFO_V0_KELVIN)
		wr_cio_state(dev, head, regp, NV_CIO_CRE_47);

	if (drm->client.device.info.family >= NV_DEVICE_INFO_V0_RANKINE)
		wr_cio_state(dev, head, regp, 0x9f);

	wr_cio_state(dev, head, regp, NV_CIO_CRE_49);
	wr_cio_state(dev, head, regp, NV_CIO_CRE_HCUR_ADDR0_INDEX);
	wr_cio_state(dev, head, regp, NV_CIO_CRE_HCUR_ADDR1_INDEX);
	wr_cio_state(dev, head, regp, NV_CIO_CRE_HCUR_ADDR2_INDEX);
	if (drm->client.device.info.family == NV_DEVICE_INFO_V0_CURIE)
		nv_fix_nv40_hw_cursor(dev, head);
	wr_cio_state(dev, head, regp, NV_CIO_CRE_ILACE__INDEX);

	wr_cio_state(dev, head, regp, NV_CIO_CRE_SCRATCH3__INDEX);
	wr_cio_state(dev, head, regp, NV_CIO_CRE_SCRATCH4__INDEX);
	if (drm->client.device.info.family >= NV_DEVICE_INFO_V0_CELSIUS) {
		wr_cio_state(dev, head, regp, NV_CIO_CRE_EBR_INDEX);
		wr_cio_state(dev, head, regp, NV_CIO_CRE_CSB);
		wr_cio_state(dev, head, regp, NV_CIO_CRE_4B);
		wr_cio_state(dev, head, regp, NV_CIO_CRE_TVOUT_LATENCY);
	}
	 
	if (nv_gf4_disp_arch(dev)) {
		if (drm->client.device.info.family < NV_DEVICE_INFO_V0_KELVIN) {
			 
			nvif_msec(&drm->client.device, 650,
				if ( (nvif_rd32(device, NV_PRMCIO_INP0__COLOR) & 8))
					break;
			);
			nvif_msec(&drm->client.device, 650,
				if (!(nvif_rd32(device, NV_PRMCIO_INP0__COLOR) & 8))
					break;
			);
		}

		wr_cio_state(dev, head, regp, NV_CIO_CRE_42);
		wr_cio_state(dev, head, regp, NV_CIO_CRE_53);
		wr_cio_state(dev, head, regp, NV_CIO_CRE_54);

		for (i = 0; i < 0x10; i++)
			NVWriteVgaCrtc5758(dev, head, i, regp->CR58[i]);
		wr_cio_state(dev, head, regp, NV_CIO_CRE_59);
		wr_cio_state(dev, head, regp, NV_CIO_CRE_5B);

		wr_cio_state(dev, head, regp, NV_CIO_CRE_85);
		wr_cio_state(dev, head, regp, NV_CIO_CRE_86);
	}

	NVWriteCRTC(dev, head, NV_PCRTC_START, regp->fb_start);
}

static void
nv_save_state_palette(struct drm_device *dev, int head,
		      struct nv04_mode_state *state)
{
	struct nvif_object *device = &nouveau_drm(dev)->client.device.object;
	int head_offset = head * NV_PRMDIO_SIZE, i;

	nvif_wr08(device, NV_PRMDIO_PIXEL_MASK + head_offset,
				NV_PRMDIO_PIXEL_MASK_MASK);
	nvif_wr08(device, NV_PRMDIO_READ_MODE_ADDRESS + head_offset, 0x0);

	for (i = 0; i < 768; i++) {
		state->crtc_reg[head].DAC[i] = nvif_rd08(device,
				NV_PRMDIO_PALETTE_DATA + head_offset);
	}

	NVSetEnablePalette(dev, head, false);
}

void
nouveau_hw_load_state_palette(struct drm_device *dev, int head,
			      struct nv04_mode_state *state)
{
	struct nvif_object *device = &nouveau_drm(dev)->client.device.object;
	int head_offset = head * NV_PRMDIO_SIZE, i;

	nvif_wr08(device, NV_PRMDIO_PIXEL_MASK + head_offset,
				NV_PRMDIO_PIXEL_MASK_MASK);
	nvif_wr08(device, NV_PRMDIO_WRITE_MODE_ADDRESS + head_offset, 0x0);

	for (i = 0; i < 768; i++) {
		nvif_wr08(device, NV_PRMDIO_PALETTE_DATA + head_offset,
				state->crtc_reg[head].DAC[i]);
	}

	NVSetEnablePalette(dev, head, false);
}

void nouveau_hw_save_state(struct drm_device *dev, int head,
			   struct nv04_mode_state *state)
{
	struct nouveau_drm *drm = nouveau_drm(dev);

	if (drm->client.device.info.chipset == 0x11)
		 
		nouveau_hw_fix_bad_vpll(dev, head);
	nv_save_state_ramdac(dev, head, state);
	nv_save_state_vga(dev, head, state);
	nv_save_state_palette(dev, head, state);
	nv_save_state_ext(dev, head, state);
}

void nouveau_hw_load_state(struct drm_device *dev, int head,
			   struct nv04_mode_state *state)
{
	NVVgaProtect(dev, head, true);
	nv_load_state_ramdac(dev, head, state);
	nv_load_state_ext(dev, head, state);
	nouveau_hw_load_state_palette(dev, head, state);
	nv_load_state_vga(dev, head, state);
	NVVgaProtect(dev, head, false);
}
