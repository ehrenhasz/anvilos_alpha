 

#ifndef __DC_HUBP_DCN32_H__
#define __DC_HUBP_DCN32_H__

#include "dcn20/dcn20_hubp.h"
#include "dcn21/dcn21_hubp.h"
#include "dcn30/dcn30_hubp.h"
#include "dcn31/dcn31_hubp.h"

#define HUBP_MASK_SH_LIST_DCN32(mask_sh)\
	HUBP_MASK_SH_LIST_DCN31(mask_sh),\
	HUBP_SF(HUBP0_DCHUBP_MALL_CONFIG, USE_MALL_SEL, mask_sh),\
	HUBP_SF(HUBP0_DCHUBP_MALL_CONFIG, USE_MALL_FOR_CURSOR, mask_sh),\
	HUBP_SF(HUBP0_DCHUBP_VMPG_CONFIG, VMPG_SIZE, mask_sh),\
	HUBP_SF(HUBP0_DCHUBP_VMPG_CONFIG, PTE_BUFFER_MODE, mask_sh),\
	HUBP_SF(HUBP0_DCHUBP_VMPG_CONFIG, BIGK_FRAGMENT_SIZE, mask_sh),\
	HUBP_SF(HUBP0_DCHUBP_VMPG_CONFIG, FORCE_ONE_ROW_FOR_FRAME, mask_sh),\
	HUBP_SF(HUBPREQ0_UCLK_PSTATE_FORCE, DATA_UCLK_PSTATE_FORCE_EN, mask_sh),\
	HUBP_SF(HUBPREQ0_UCLK_PSTATE_FORCE, DATA_UCLK_PSTATE_FORCE_VALUE, mask_sh),\
	HUBP_SF(HUBPREQ0_UCLK_PSTATE_FORCE, CURSOR_UCLK_PSTATE_FORCE_EN, mask_sh),\
	HUBP_SF(HUBPREQ0_UCLK_PSTATE_FORCE, CURSOR_UCLK_PSTATE_FORCE_VALUE, mask_sh)

void hubp32_update_force_pstate_disallow(struct hubp *hubp, bool pstate_disallow);

void hubp32_update_force_cursor_pstate_disallow(struct hubp *hubp, bool pstate_disallow);

void hubp32_update_mall_sel(struct hubp *hubp, uint32_t mall_sel, bool c_cursor);

void hubp32_prepare_subvp_buffering(struct hubp *hubp, bool enable);

void hubp32_phantom_hubp_post_enable(struct hubp *hubp);

void hubp32_cursor_set_attributes(struct hubp *hubp,
		const struct dc_cursor_attributes *attr);

void hubp32_init(struct hubp *hubp);

bool hubp32_construct(
	struct dcn20_hubp *hubp2,
	struct dc_context *ctx,
	uint32_t inst,
	const struct dcn_hubp2_registers *hubp_regs,
	const struct dcn_hubp2_shift *hubp_shift,
	const struct dcn_hubp2_mask *hubp_mask);

#endif  
