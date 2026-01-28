#ifndef __DML20V2_DISPLAY_RQ_DLG_CALC_H__
#define __DML20V2_DISPLAY_RQ_DLG_CALC_H__
#include "../display_rq_dlg_helpers.h"
struct display_mode_lib;
void dml20v2_rq_dlg_get_rq_reg(
		struct display_mode_lib *mode_lib,
		display_rq_regs_st *rq_regs,
		const display_pipe_params_st *pipe_param);
void dml20v2_rq_dlg_get_dlg_reg(
		struct display_mode_lib *mode_lib,
		display_dlg_regs_st *dlg_regs,
		display_ttu_regs_st *ttu_regs,
		const display_e2e_pipe_params_st *e2e_pipe_param,
		const unsigned int num_pipes,
		const unsigned int pipe_idx,
		const bool cstate_en,
		const bool pstate_en,
		const bool vm_en,
		const bool ignore_viewport_pos,
		const bool immediate_flip_support);
#endif
