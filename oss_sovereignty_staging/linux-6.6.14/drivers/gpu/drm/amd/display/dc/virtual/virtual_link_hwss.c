 

#include "virtual_link_hwss.h"

void virtual_setup_stream_encoder(struct pipe_ctx *pipe_ctx)
{
}

void virtual_setup_stream_attribute(struct pipe_ctx *pipe_ctx)
{
}

void virtual_reset_stream_encoder(struct pipe_ctx *pipe_ctx)
{
}

static void virtual_disable_link_output(struct dc_link *link,
	const struct link_resource *link_res,
	enum signal_type signal)
{
}

static const struct link_hwss virtual_link_hwss = {
	.setup_stream_encoder = virtual_setup_stream_encoder,
	.reset_stream_encoder = virtual_reset_stream_encoder,
	.setup_stream_attribute = virtual_setup_stream_attribute,
	.disable_link_output = virtual_disable_link_output,
};

const struct link_hwss *get_virtual_link_hwss(void)
{
	return &virtual_link_hwss;
}
