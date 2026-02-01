 

#ifndef __DAL_TRANSFORM_V_DCE110_H__
#define __DAL_TRANSFORM_V_DCE110_H__

#include "../dce/dce_transform.h"

#define LB_TOTAL_NUMBER_OF_ENTRIES 1712
#define LB_BITS_PER_ENTRY 144

bool dce110_transform_v_construct(
	struct dce_transform *xfm110,
	struct dc_context *ctx);

void dce110_opp_v_set_csc_default(
	struct transform *xfm,
	const struct default_adjustment *default_adjust);

void dce110_opp_v_set_csc_adjustment(
		struct transform *xfm,
	const struct out_csc_color_matrix *tbl_entry);


void dce110_opp_program_regamma_pwl_v(
	struct transform *xfm,
	const struct pwl_params *params);

void dce110_opp_power_on_regamma_lut_v(
	struct transform *xfm,
	bool power_on);

void dce110_opp_set_regamma_mode_v(
	struct transform *xfm,
	enum opp_regamma mode);

#endif
