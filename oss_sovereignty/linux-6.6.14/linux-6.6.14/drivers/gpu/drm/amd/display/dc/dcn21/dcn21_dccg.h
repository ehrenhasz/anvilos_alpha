#ifndef __DCN21_DCCG_H__
#define __DCN21_DCCG_H__
struct dccg *dccg21_create(
	struct dc_context *ctx,
	const struct dccg_registers *regs,
	const struct dccg_shift *dccg_shift,
	const struct dccg_mask *dccg_mask);
#endif  
