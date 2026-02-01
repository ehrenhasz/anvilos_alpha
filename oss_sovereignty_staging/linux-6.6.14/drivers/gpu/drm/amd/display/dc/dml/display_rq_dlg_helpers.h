 

#ifndef __DISPLAY_RQ_DLG_HELPERS_H__
#define __DISPLAY_RQ_DLG_HELPERS_H__

#include "display_mode_lib.h"

 
void print__rq_params_st(struct display_mode_lib *mode_lib, const struct _vcs_dpi_display_rq_params_st *rq_param);
void print__data_rq_sizing_params_st(struct display_mode_lib *mode_lib, const struct _vcs_dpi_display_data_rq_sizing_params_st *rq_sizing);
void print__data_rq_dlg_params_st(struct display_mode_lib *mode_lib, const struct _vcs_dpi_display_data_rq_dlg_params_st *rq_dlg_param);
void print__data_rq_misc_params_st(struct display_mode_lib *mode_lib, const struct _vcs_dpi_display_data_rq_misc_params_st *rq_misc_param);
void print__rq_dlg_params_st(struct display_mode_lib *mode_lib, const struct _vcs_dpi_display_rq_dlg_params_st *rq_dlg_param);
void print__dlg_sys_params_st(struct display_mode_lib *mode_lib, const struct _vcs_dpi_display_dlg_sys_params_st *dlg_sys_param);

void print__data_rq_regs_st(struct display_mode_lib *mode_lib, const struct _vcs_dpi_display_data_rq_regs_st *rq_regs);
void print__rq_regs_st(struct display_mode_lib *mode_lib, const struct _vcs_dpi_display_rq_regs_st *rq_regs);
void print__dlg_regs_st(struct display_mode_lib *mode_lib, const struct _vcs_dpi_display_dlg_regs_st *dlg_regs);
void print__ttu_regs_st(struct display_mode_lib *mode_lib, const struct _vcs_dpi_display_ttu_regs_st *ttu_regs);

#endif
