 

#include "reg_helper.h"
#include "core_types.h"
#include "dcn301_dccg.h"

#define TO_DCN_DCCG(dccg)\
	container_of(dccg, struct dcn_dccg, base)

#define REG(reg) \
	(dccg_dcn->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	dccg_dcn->dccg_shift->field_name, dccg_dcn->dccg_mask->field_name

#define CTX \
	dccg_dcn->base.ctx
#define DC_LOGGER \
	dccg->ctx->logger

static const struct dccg_funcs dccg301_funcs = {
	.update_dpp_dto = dccg2_update_dpp_dto,
	.get_dccg_ref_freq = dccg2_get_dccg_ref_freq,
	.set_fifo_errdet_ovr_en = dccg2_set_fifo_errdet_ovr_en,
	.otg_add_pixel = dccg2_otg_add_pixel,
	.otg_drop_pixel = dccg2_otg_drop_pixel,
	.dccg_init = dccg2_init
};

struct dccg *dccg301_create(
	struct dc_context *ctx,
	const struct dccg_registers *regs,
	const struct dccg_shift *dccg_shift,
	const struct dccg_mask *dccg_mask)
{
	struct dcn_dccg *dccg_dcn = kzalloc(sizeof(*dccg_dcn), GFP_KERNEL);
	struct dccg *base;

	if (dccg_dcn == NULL) {
		BREAK_TO_DEBUGGER();
		return NULL;
	}

	base = &dccg_dcn->base;
	base->ctx = ctx;
	base->funcs = &dccg301_funcs;

	dccg_dcn->regs = regs;
	dccg_dcn->dccg_shift = dccg_shift;
	dccg_dcn->dccg_mask = dccg_mask;

	return &dccg_dcn->base;
}

