 

#ifndef __DC_TIMING_GENERATOR_DCE120_H__
#define __DC_TIMING_GENERATOR_DCE120_H__

#include "timing_generator.h"
#include "../include/grph_object_id.h"
#include "dce110/dce110_timing_generator.h"


void dce120_timing_generator_construct(
	struct dce110_timing_generator *tg110,
	struct dc_context *ctx,
	uint32_t instance,
	const struct dce110_timing_generator_offsets *offsets);

#endif  
