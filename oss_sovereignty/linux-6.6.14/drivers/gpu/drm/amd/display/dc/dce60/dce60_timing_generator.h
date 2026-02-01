 

#ifndef __DC_TIMING_GENERATOR_DCE60_H__
#define __DC_TIMING_GENERATOR_DCE60_H__

#include "timing_generator.h"
#include "../include/grph_object_id.h"

 
void dce60_timing_generator_construct(
	struct dce110_timing_generator *tg,
	struct dc_context *ctx,
	uint32_t instance,
	const struct dce110_timing_generator_offsets *offsets);

#endif  
