 

#include "dce/dce_dmcu.h"
#include "dc_edid_parser.h"

bool dc_edid_parser_send_cea(struct dc *dc,
		int offset,
		int total_length,
		uint8_t *data,
		int length)
{
	struct dmcu *dmcu = dc->res_pool->dmcu;

	if (dmcu &&
	    dmcu->funcs->is_dmcu_initialized(dmcu) &&
	    dmcu->funcs->send_edid_cea) {
		return dmcu->funcs->send_edid_cea(dmcu,
				offset,
				total_length,
				data,
				length);
	}

	return false;
}

bool dc_edid_parser_recv_cea_ack(struct dc *dc, int *offset)
{
	struct dmcu *dmcu = dc->res_pool->dmcu;

	if (dmcu &&
	    dmcu->funcs->is_dmcu_initialized(dmcu) &&
	    dmcu->funcs->recv_edid_cea_ack) {
		return dmcu->funcs->recv_edid_cea_ack(dmcu, offset);
	}

	return false;
}

bool dc_edid_parser_recv_amd_vsdb(struct dc *dc,
		int *version,
		int *min_frame_rate,
		int *max_frame_rate)
{
	struct dmcu *dmcu = dc->res_pool->dmcu;

	if (dmcu &&
	    dmcu->funcs->is_dmcu_initialized(dmcu) &&
	    dmcu->funcs->recv_amd_vsdb) {
		return dmcu->funcs->recv_amd_vsdb(dmcu,
				version,
				min_frame_rate,
				max_frame_rate);
	}

	return false;
}
