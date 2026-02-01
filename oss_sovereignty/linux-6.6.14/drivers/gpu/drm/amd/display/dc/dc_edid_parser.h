 

#ifndef _DC_EDID_PARSER_H_
#define _DC_EDID_PARSER_H_

#include "core_types.h"

bool dc_edid_parser_send_cea(struct dc *dc,
		int offset,
		int total_length,
		uint8_t *data,
		int length);

bool dc_edid_parser_recv_cea_ack(struct dc *dc, int *offset);

bool dc_edid_parser_recv_amd_vsdb(struct dc *dc,
		int *version,
		int *min_frame_rate,
		int *max_frame_rate);

#endif  
