 

#ifndef __DC_HWSS_DCN10_DEBUG_H__
#define __DC_HWSS_DCN10_DEBUG_H__

#include "core_types.h"

struct dc;

void dcn10_clear_status_bits(struct dc *dc, unsigned int mask);

void dcn10_log_hw_state(struct dc *dc,
		struct dc_log_buffer_ctx *log_ctx);

void dcn10_get_hw_state(struct dc *dc,
		char *pBuf,
		unsigned int bufSize,
		unsigned int mask);

#endif  
