 

#ifndef __DISPLAY_RQ_DLG_CALC_H__
#define __DISPLAY_RQ_DLG_CALC_H__

struct display_mode_lib;

#include "display_rq_dlg_helpers.h"

void dml1_extract_rq_regs(
		struct display_mode_lib *mode_lib,
		struct _vcs_dpi_display_rq_regs_st *rq_regs,
		const struct _vcs_dpi_display_rq_params_st *rq_param);
 
void dml1_rq_dlg_get_rq_params(
		struct display_mode_lib *mode_lib,
		struct _vcs_dpi_display_rq_params_st *rq_param,
		const struct _vcs_dpi_display_pipe_source_params_st *pipe_src_param);


 
void dml1_rq_dlg_get_dlg_params(
		struct display_mode_lib *mode_lib,
		struct _vcs_dpi_display_dlg_regs_st *dlg_regs,
		struct _vcs_dpi_display_ttu_regs_st *ttu_regs,
		const struct _vcs_dpi_display_rq_dlg_params_st *rq_dlg_param,
		const struct _vcs_dpi_display_dlg_sys_params_st *dlg_sys_param,
		const struct _vcs_dpi_display_e2e_pipe_params_st *e2e_pipe_param,
		const bool cstate_en,
		const bool pstate_en,
		const bool vm_en,
		const bool iflip_en);

#endif
