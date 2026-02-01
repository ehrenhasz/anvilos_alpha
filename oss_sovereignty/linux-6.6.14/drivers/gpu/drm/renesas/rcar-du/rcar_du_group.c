
 

 

#include <linux/clk.h>
#include <linux/io.h>

#include "rcar_du_drv.h"
#include "rcar_du_group.h"
#include "rcar_du_regs.h"

u32 rcar_du_group_read(struct rcar_du_group *rgrp, u32 reg)
{
	return rcar_du_read(rgrp->dev, rgrp->mmio_offset + reg);
}

void rcar_du_group_write(struct rcar_du_group *rgrp, u32 reg, u32 data)
{
	rcar_du_write(rgrp->dev, rgrp->mmio_offset + reg, data);
}

static void rcar_du_group_setup_pins(struct rcar_du_group *rgrp)
{
	u32 defr6 = DEFR6_CODE;

	if (rgrp->channels_mask & BIT(0))
		defr6 |= DEFR6_ODPM02_DISP;

	if (rgrp->channels_mask & BIT(1))
		defr6 |= DEFR6_ODPM12_DISP;

	rcar_du_group_write(rgrp, DEFR6, defr6);
}

static void rcar_du_group_setup_defr8(struct rcar_du_group *rgrp)
{
	struct rcar_du_device *rcdu = rgrp->dev;
	u32 defr8 = DEFR8_CODE;

	if (rcdu->info->gen < 3) {
		defr8 |= DEFR8_DEFE8;

		 
		if (rgrp->index == 0) {
			defr8 |= DEFR8_DRGBS_DU(rcdu->dpad0_source);
			if (rgrp->dev->vspd1_sink == 2)
				defr8 |= DEFR8_VSCS;
		}
	} else {
		 
		if (rgrp->index == rcdu->dpad0_source / 2)
			defr8 |= DEFR8_DRGBS_DU(rcdu->dpad0_source);
	}

	rcar_du_group_write(rgrp, DEFR8, defr8);
}

static void rcar_du_group_setup_didsr(struct rcar_du_group *rgrp)
{
	struct rcar_du_device *rcdu = rgrp->dev;
	struct rcar_du_crtc *rcrtc;
	unsigned int num_crtcs = 0;
	unsigned int i;
	u32 didsr;

	 
	if (rcdu->info->gen < 3 && rgrp->index == 0) {
		 
		rcrtc = rcdu->crtcs;
		num_crtcs = rcdu->num_crtcs;
	} else if (rcdu->info->gen >= 3 && rgrp->num_crtcs > 1) {
		 
		rcrtc = &rcdu->crtcs[rgrp->index * 2];
		num_crtcs = rgrp->num_crtcs;
	}

	if (!num_crtcs)
		return;

	didsr = DIDSR_CODE;
	for (i = 0; i < num_crtcs; ++i, ++rcrtc) {
		if (rcdu->info->lvds_clk_mask & BIT(rcrtc->index))
			didsr |= DIDSR_LDCS_LVDS0(i)
			      |  DIDSR_PDCS_CLK(i, 0);
		else if (rcdu->info->dsi_clk_mask & BIT(rcrtc->index))
			didsr |= DIDSR_LDCS_DSI(i);
		else
			didsr |= DIDSR_LDCS_DCLKIN(i)
			      |  DIDSR_PDCS_CLK(i, 0);
	}

	rcar_du_group_write(rgrp, DIDSR, didsr);
}

static void rcar_du_group_setup(struct rcar_du_group *rgrp)
{
	struct rcar_du_device *rcdu = rgrp->dev;
	u32 defr7 = DEFR7_CODE;
	u32 dorcr;

	 
	rcar_du_group_write(rgrp, DEFR, DEFR_CODE | DEFR_DEFE);
	if (rcdu->info->gen < 3) {
		rcar_du_group_write(rgrp, DEFR2, DEFR2_CODE | DEFR2_DEFE2G);
		rcar_du_group_write(rgrp, DEFR3, DEFR3_CODE | DEFR3_DEFE3);
		rcar_du_group_write(rgrp, DEFR4, DEFR4_CODE);
	}
	rcar_du_group_write(rgrp, DEFR5, DEFR5_CODE | DEFR5_DEFE5);

	if (rcdu->info->gen < 4)
		rcar_du_group_setup_pins(rgrp);

	if (rcdu->info->gen < 4) {
		 
		defr7 |= (rgrp->cmms_mask & BIT(1) ? DEFR7_CMME1 : 0) |
			 (rgrp->cmms_mask & BIT(0) ? DEFR7_CMME0 : 0);
		rcar_du_group_write(rgrp, DEFR7, defr7);
	}

	if (rcdu->info->gen >= 2) {
		if (rcdu->info->gen < 4)
			rcar_du_group_setup_defr8(rgrp);
		rcar_du_group_setup_didsr(rgrp);
	}

	if (rcdu->info->gen >= 3)
		rcar_du_group_write(rgrp, DEFR10, DEFR10_CODE | DEFR10_DEFE10);

	 
	dorcr = DORCR_PG0D_DS0 | DORCR_DPRS;
	if (rcdu->info->gen >= 3 && rgrp->num_crtcs == 1)
		dorcr |= DORCR_PG1T | DORCR_DK1S | DORCR_PG1D_DS1;
	rcar_du_group_write(rgrp, DORCR, dorcr);

	 
	mutex_lock(&rgrp->lock);
	rcar_du_group_write(rgrp, DPTSR, (rgrp->dptsr_planes << 16) |
			    rgrp->dptsr_planes);
	mutex_unlock(&rgrp->lock);
}

 
int rcar_du_group_get(struct rcar_du_group *rgrp)
{
	if (rgrp->use_count)
		goto done;

	rcar_du_group_setup(rgrp);

done:
	rgrp->use_count++;
	return 0;
}

 
void rcar_du_group_put(struct rcar_du_group *rgrp)
{
	--rgrp->use_count;
}

static void __rcar_du_group_start_stop(struct rcar_du_group *rgrp, bool start)
{
	struct rcar_du_device *rcdu = rgrp->dev;

	 
	if (rcdu->info->channels_mask & BIT(rgrp->index * 2)) {
		struct rcar_du_crtc *rcrtc = &rgrp->dev->crtcs[rgrp->index * 2];

		rcar_du_crtc_dsysr_clr_set(rcrtc, DSYSR_DRES | DSYSR_DEN,
					   start ? DSYSR_DEN : DSYSR_DRES);
	} else {
		rcar_du_group_write(rgrp, DSYSR,
				    start ? DSYSR_DEN : DSYSR_DRES);
	}
}

void rcar_du_group_start_stop(struct rcar_du_group *rgrp, bool start)
{
	 
	if (start) {
		if (rgrp->used_crtcs++ != 0)
			__rcar_du_group_start_stop(rgrp, false);
		__rcar_du_group_start_stop(rgrp, true);
	} else {
		if (--rgrp->used_crtcs == 0)
			__rcar_du_group_start_stop(rgrp, false);
	}
}

void rcar_du_group_restart(struct rcar_du_group *rgrp)
{
	rgrp->need_restart = false;

	__rcar_du_group_start_stop(rgrp, false);
	__rcar_du_group_start_stop(rgrp, true);
}

int rcar_du_set_dpad0_vsp1_routing(struct rcar_du_device *rcdu)
{
	struct rcar_du_group *rgrp;
	struct rcar_du_crtc *crtc;
	unsigned int index;
	int ret;

	if (rcdu->info->gen < 2)
		return 0;

	 
	index = rcdu->info->gen < 3 ? 0 : DIV_ROUND_UP(rcdu->num_crtcs, 2) - 1;
	rgrp = &rcdu->groups[index];
	crtc = &rcdu->crtcs[index * 2];

	ret = clk_prepare_enable(crtc->clock);
	if (ret < 0)
		return ret;

	rcar_du_group_setup_defr8(rgrp);

	clk_disable_unprepare(crtc->clock);

	return 0;
}

static void rcar_du_group_set_dpad_levels(struct rcar_du_group *rgrp)
{
	static const u32 doflr_values[2] = {
		DOFLR_HSYCFL0 | DOFLR_VSYCFL0 | DOFLR_ODDFL0 |
		DOFLR_DISPFL0 | DOFLR_CDEFL0  | DOFLR_RGBFL0,
		DOFLR_HSYCFL1 | DOFLR_VSYCFL1 | DOFLR_ODDFL1 |
		DOFLR_DISPFL1 | DOFLR_CDEFL1  | DOFLR_RGBFL1,
	};
	static const u32 dpad_mask = BIT(RCAR_DU_OUTPUT_DPAD1)
				   | BIT(RCAR_DU_OUTPUT_DPAD0);
	struct rcar_du_device *rcdu = rgrp->dev;
	u32 doflr = DOFLR_CODE;
	unsigned int i;

	if (rcdu->info->gen < 2)
		return;

	 

	for (i = 0; i < rgrp->num_crtcs; ++i) {
		struct rcar_du_crtc_state *rstate;
		struct rcar_du_crtc *rcrtc;

		rcrtc = &rcdu->crtcs[rgrp->index * 2 + i];
		rstate = to_rcar_crtc_state(rcrtc->crtc.state);

		if (!(rstate->outputs & dpad_mask))
			doflr |= doflr_values[i];
	}

	rcar_du_group_write(rgrp, DOFLR, doflr);
}

int rcar_du_group_set_routing(struct rcar_du_group *rgrp)
{
	struct rcar_du_device *rcdu = rgrp->dev;
	u32 dorcr = rcar_du_group_read(rgrp, DORCR);

	dorcr &= ~(DORCR_PG1T | DORCR_DK1S | DORCR_PG1D_MASK);

	 
	if (rcdu->dpad1_source == rgrp->index * 2)
		dorcr |= DORCR_PG1D_DS0;
	else
		dorcr |= DORCR_PG1T | DORCR_DK1S | DORCR_PG1D_DS1;

	rcar_du_group_write(rgrp, DORCR, dorcr);

	rcar_du_group_set_dpad_levels(rgrp);

	return rcar_du_set_dpad0_vsp1_routing(rgrp->dev);
}
