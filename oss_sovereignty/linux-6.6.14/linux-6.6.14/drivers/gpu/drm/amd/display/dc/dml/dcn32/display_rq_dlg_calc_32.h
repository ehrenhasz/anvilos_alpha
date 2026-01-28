#ifndef __DML32_DISPLAY_RQ_DLG_CALC_H__
#define __DML32_DISPLAY_RQ_DLG_CALC_H__
#include "../display_rq_dlg_helpers.h"
struct display_mode_lib;
void dml32_rq_dlg_get_rq_reg(display_rq_regs_st *rq_regs,
		struct display_mode_lib *mode_lib,
		const display_e2e_pipe_params_st *e2e_pipe_param,
		const unsigned int num_pipes,
		const unsigned int pipe_idx);
void dml32_rq_dlg_get_dlg_reg(struct display_mode_lib *mode_lib,
		display_dlg_regs_st *dlg_regs,
		display_ttu_regs_st *ttu_regs,
		display_e2e_pipe_params_st *e2e_pipe_param,
		const unsigned int num_pipes,
		const unsigned int pipe_idx);
#endif
