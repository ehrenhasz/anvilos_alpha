 

#include "dc.h"
#include "dc_dmub_srv.h"
#include "dmub_outbox.h"
#include "dmub/inc/dmub_cmd.h"

 
void dmub_enable_outbox_notification(struct dc_dmub_srv *dmub_srv)
{
	union dmub_rb_cmd cmd;

	memset(&cmd, 0x0, sizeof(cmd));
	cmd.outbox1_enable.header.type = DMUB_CMD__OUTBOX1_ENABLE;
	cmd.outbox1_enable.header.sub_type = 0;
	cmd.outbox1_enable.header.payload_bytes =
		sizeof(cmd.outbox1_enable) -
		sizeof(cmd.outbox1_enable.header);
	cmd.outbox1_enable.enable = true;

	dm_execute_dmub_cmd(dmub_srv->ctx, &cmd, DM_DMUB_WAIT_TYPE_WAIT);
}
