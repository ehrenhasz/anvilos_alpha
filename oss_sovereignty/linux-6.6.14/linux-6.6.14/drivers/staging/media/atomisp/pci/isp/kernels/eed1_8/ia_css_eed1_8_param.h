#ifndef __IA_CSS_EED1_8_PARAM_H
#define __IA_CSS_EED1_8_PARAM_H
#include "type_support.h"
#include "vmem.h"  
#include "ia_css_eed1_8_types.h"  
#define EED1_8_FC_ENABLE_MEDIAN		1
#define EED1_8_CORINGTHMIN	1
#define NUM_PLANES	4
#define EED1_8_STATE_INPUT_BUFFER_HEIGHT	(5 * NUM_PLANES)
#define EED1_8_STATE_INPUT_BUFFER_WIDTH	CEIL_DIV(MAX_FRAME_SIMDWIDTH, 2)
#define EED1_8_STATE_LD_H_HEIGHT	(1 * NUM_PLANES)
#define EED1_8_STATE_LD_H_WIDTH		CEIL_DIV(MAX_FRAME_SIMDWIDTH, 2)
#define EED1_8_STATE_LD_V_HEIGHT	(1 * NUM_PLANES)
#define EED1_8_STATE_LD_V_WIDTH		CEIL_DIV(MAX_FRAME_SIMDWIDTH, 2)
#define EED1_8_STATE_D_HR_HEIGHT	1
#define EED1_8_STATE_D_HR_WIDTH		CEIL_DIV(MAX_FRAME_SIMDWIDTH, 2)
#define EED1_8_STATE_D_HB_HEIGHT	1
#define EED1_8_STATE_D_HB_WIDTH		CEIL_DIV(MAX_FRAME_SIMDWIDTH, 2)
#define EED1_8_STATE_D_VR_HEIGHT	2
#define EED1_8_STATE_D_VR_WIDTH		CEIL_DIV(MAX_FRAME_SIMDWIDTH, 2)
#define EED1_8_STATE_D_VB_HEIGHT	2
#define EED1_8_STATE_D_VB_WIDTH		CEIL_DIV(MAX_FRAME_SIMDWIDTH, 2)
#define EED1_8_STATE_RB_ZIPPED_HEIGHT	(2 * 2)
#define EED1_8_STATE_RB_ZIPPED_WIDTH	CEIL_DIV(MAX_FRAME_SIMDWIDTH, 2)
#if EED1_8_FC_ENABLE_MEDIAN
#define EED1_8_STATE_YC_HEIGHT	1
#define EED1_8_STATE_YC_WIDTH	MAX_FRAME_SIMDWIDTH
#define EED1_8_STATE_CG_HEIGHT	(1 * NUM_PLANES)
#define EED1_8_STATE_CG_WIDTH	CEIL_DIV(MAX_FRAME_SIMDWIDTH, 2)
#define EED1_8_STATE_CO_HEIGHT	(1 * NUM_PLANES)
#define EED1_8_STATE_CO_WIDTH	CEIL_DIV(MAX_FRAME_SIMDWIDTH, 2)
#define EED1_8_STATE_ABSK_HEIGHT	1
#define EED1_8_STATE_ABSK_WIDTH		MAX_FRAME_SIMDWIDTH
#endif
struct eed1_8_vmem_params {
	VMEM_ARRAY(e_dew_enh_x, ISP_VEC_NELEMS);
	VMEM_ARRAY(e_dew_enh_y, ISP_VEC_NELEMS);
	VMEM_ARRAY(e_dew_enh_a, ISP_VEC_NELEMS);
	VMEM_ARRAY(e_dew_enh_f, ISP_VEC_NELEMS);
	VMEM_ARRAY(chgrinv_x, ISP_VEC_NELEMS);
	VMEM_ARRAY(chgrinv_a, ISP_VEC_NELEMS);
	VMEM_ARRAY(chgrinv_b, ISP_VEC_NELEMS);
	VMEM_ARRAY(chgrinv_c, ISP_VEC_NELEMS);
	VMEM_ARRAY(fcinv_x, ISP_VEC_NELEMS);
	VMEM_ARRAY(fcinv_a, ISP_VEC_NELEMS);
	VMEM_ARRAY(fcinv_b, ISP_VEC_NELEMS);
	VMEM_ARRAY(fcinv_c, ISP_VEC_NELEMS);
	VMEM_ARRAY(tcinv_x, ISP_VEC_NELEMS);
	VMEM_ARRAY(tcinv_a, ISP_VEC_NELEMS);
	VMEM_ARRAY(tcinv_b, ISP_VEC_NELEMS);
	VMEM_ARRAY(tcinv_c, ISP_VEC_NELEMS);
};
struct eed1_8_dmem_params {
	s32 rbzp_strength;
	s32 fcstrength;
	s32 fcthres_0;
	s32 fc_sat_coef;
	s32 fc_coring_prm;
	s32 fc_slope;
	s32 aerel_thres0;
	s32 aerel_gain0;
	s32 aerel_thres_diff;
	s32 aerel_gain_diff;
	s32 derel_thres0;
	s32 derel_gain0;
	s32 derel_thres_diff;
	s32 derel_gain_diff;
	s32 coring_pos0;
	s32 coring_pos_diff;
	s32 coring_neg0;
	s32 coring_neg_diff;
	s32 gain_exp;
	s32 gain_pos0;
	s32 gain_pos_diff;
	s32 gain_neg0;
	s32 gain_neg_diff;
	s32 margin_pos0;
	s32 margin_pos_diff;
	s32 margin_neg0;
	s32 margin_neg_diff;
	s32 e_dew_enh_asr;
	s32 dedgew_max;
};
#endif  
