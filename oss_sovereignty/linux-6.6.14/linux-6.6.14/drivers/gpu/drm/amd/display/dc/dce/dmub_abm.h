#ifndef __DMUB_ABM_H__
#define __DMUB_ABM_H__
#include "abm.h"
#include "dce_abm.h"
struct abm *dmub_abm_create(
	struct dc_context *ctx,
	const struct dce_abm_registers *regs,
	const struct dce_abm_shift *abm_shift,
	const struct dce_abm_mask *abm_mask);
void dmub_abm_destroy(struct abm **abm);
#endif
