 
#ifndef __DC_VIRTUAL_LINK_HWSS_H__
#define __DC_VIRTUAL_LINK_HWSS_H__

#include "core_types.h"

void virtual_setup_stream_encoder(struct pipe_ctx *pipe_ctx);
void virtual_setup_stream_attribute(struct pipe_ctx *pipe_ctx);
void virtual_reset_stream_encoder(struct pipe_ctx *pipe_ctx);
const struct link_hwss *get_virtual_link_hwss(void);

#endif  
