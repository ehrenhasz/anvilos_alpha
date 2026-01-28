#ifndef __DC_VIRTUAL_STREAM_ENCODER_H__
#define __DC_VIRTUAL_STREAM_ENCODER_H__
#include "stream_encoder.h"
struct stream_encoder *virtual_stream_encoder_create(
	struct dc_context *ctx, struct dc_bios *bp);
bool virtual_stream_encoder_construct(
	struct stream_encoder *enc,
	struct dc_context *ctx,
	struct dc_bios *bp);
#endif  
