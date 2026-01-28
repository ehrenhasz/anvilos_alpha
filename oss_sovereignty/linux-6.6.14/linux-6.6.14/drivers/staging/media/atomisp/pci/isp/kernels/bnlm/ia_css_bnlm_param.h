#ifndef __IA_CSS_BNLM_PARAM_H
#define __IA_CSS_BNLM_PARAM_H
#include "type_support.h"
#include "vmem.h"  
struct bnlm_lut {
	VMEM_ARRAY(thr, ISP_VEC_NELEMS);  
	VMEM_ARRAY(val, ISP_VEC_NELEMS);  
};
struct bnlm_vmem_params {
	VMEM_ARRAY(nl_th, ISP_VEC_NELEMS);
	VMEM_ARRAY(match_quality_max_idx, ISP_VEC_NELEMS);
	struct bnlm_lut mu_root_lut;
	struct bnlm_lut sad_norm_lut;
	struct bnlm_lut sig_detail_lut;
	struct bnlm_lut sig_rad_lut;
	struct bnlm_lut rad_pow_lut;
	struct bnlm_lut nl_0_lut;
	struct bnlm_lut nl_1_lut;
	struct bnlm_lut nl_2_lut;
	struct bnlm_lut nl_3_lut;
	struct bnlm_lut div_lut;
	VMEM_ARRAY(div_lut_intercepts, ISP_VEC_NELEMS);
	VMEM_ARRAY(power_of_2, ISP_VEC_NELEMS);
};
struct bnlm_dmem_params {
	bool rad_enable;
	s32 rad_x_origin;
	s32 rad_y_origin;
	s32 avg_min_th;
	s32 max_min_th;
	s32 exp_coeff_a;
	u32 exp_coeff_b;
	s32 exp_coeff_c;
	u32 exp_exponent;
};
#endif  
