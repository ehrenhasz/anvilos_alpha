 

#include "dm_services.h"
#include "dcn10_ipp.h"
#include "reg_helper.h"

#define REG(reg) \
	(ippn10->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	ippn10->ipp_shift->field_name, ippn10->ipp_mask->field_name

#define CTX \
	ippn10->base.ctx

 
 
 

static void dcn10_ipp_destroy(struct input_pixel_processor **ipp)
{
	kfree(TO_DCN10_IPP(*ipp));
	*ipp = NULL;
}

static const struct ipp_funcs dcn10_ipp_funcs = {
	.ipp_destroy			= dcn10_ipp_destroy
};

static const struct ipp_funcs dcn20_ipp_funcs = {
	.ipp_destroy			= dcn10_ipp_destroy
};

void dcn10_ipp_construct(
	struct dcn10_ipp *ippn10,
	struct dc_context *ctx,
	int inst,
	const struct dcn10_ipp_registers *regs,
	const struct dcn10_ipp_shift *ipp_shift,
	const struct dcn10_ipp_mask *ipp_mask)
{
	ippn10->base.ctx = ctx;
	ippn10->base.inst = inst;
	ippn10->base.funcs = &dcn10_ipp_funcs;

	ippn10->regs = regs;
	ippn10->ipp_shift = ipp_shift;
	ippn10->ipp_mask = ipp_mask;
}

void dcn20_ipp_construct(
	struct dcn10_ipp *ippn10,
	struct dc_context *ctx,
	int inst,
	const struct dcn10_ipp_registers *regs,
	const struct dcn10_ipp_shift *ipp_shift,
	const struct dcn10_ipp_mask *ipp_mask)
{
	ippn10->base.ctx = ctx;
	ippn10->base.inst = inst;
	ippn10->base.funcs = &dcn20_ipp_funcs;

	ippn10->regs = regs;
	ippn10->ipp_shift = ipp_shift;
	ippn10->ipp_mask = ipp_mask;
}
