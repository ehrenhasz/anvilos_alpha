#ifndef __RC_CALC_FPU_H__
#define __RC_CALC_FPU_H__
#include "os_types.h"
#include <drm/display/drm_dsc.h>
#define QP_SET_SIZE 15
typedef int qp_set[QP_SET_SIZE];
struct rc_params {
	int      rc_quant_incr_limit0;
	int      rc_quant_incr_limit1;
	int      initial_fullness_offset;
	int      initial_xmit_delay;
	int      first_line_bpg_offset;
	int      second_line_bpg_offset;
	int      flatness_min_qp;
	int      flatness_max_qp;
	int      flatness_det_thresh;
	qp_set   qp_min;
	qp_set   qp_max;
	qp_set   ofs;
	int      rc_model_size;
	int      rc_edge_factor;
	int      rc_tgt_offset_hi;
	int      rc_tgt_offset_lo;
	int      rc_buf_thresh[QP_SET_SIZE - 1];
};
enum colour_mode {
	CM_RGB,    
	CM_444,    
	CM_422,    
	CM_420     
};
enum bits_per_comp {
	BPC_8  =  8,
	BPC_10 = 10,
	BPC_12 = 12
};
enum max_min {
	DAL_MM_MIN = 0,
	DAL_MM_MAX = 1
};
struct qp_entry {
	float         bpp;
	const qp_set  qps;
};
typedef struct qp_entry qp_table[];
void _do_calc_rc_params(struct rc_params *rc,
		enum colour_mode cm,
		enum bits_per_comp bpc,
		u16 drm_bpp,
		bool is_navite_422_or_420,
		int slice_width,
		int slice_height,
		int minor_version);
#endif
