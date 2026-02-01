 

#include "dc/dc_stat.h"
#include "dmub/dmub_srv_stat.h"
#include "dc_dmub_srv.h"

 

 
void dc_stat_get_dmub_notification(const struct dc *dc, struct dmub_notification *notify)
{
	 
	struct dmub_srv *dmub = dc->ctx->dmub_srv->dmub;
	enum dmub_status status;

	status = dmub_srv_stat_get_notification(dmub, notify);
	ASSERT(status == DMUB_STATUS_OK);

	 
	if (notify->type == DMUB_NOTIFICATION_HPD ||
	    notify->type == DMUB_NOTIFICATION_HPD_IRQ ||
		notify->type == DMUB_NOTIFICATION_DPIA_NOTIFICATION ||
	    notify->type == DMUB_NOTIFICATION_SET_CONFIG_REPLY) {
		notify->link_index =
			get_link_index_from_dpia_port_index(dc, notify->link_index);
	}
}

 
void dc_stat_get_dmub_dataout(const struct dc *dc, uint32_t *dataout)
{
	struct dmub_srv *dmub = dc->ctx->dmub_srv->dmub;
	enum dmub_status status;

	status = dmub_srv_get_gpint_dataout(dmub, dataout);
	ASSERT(status == DMUB_STATUS_OK);
}
