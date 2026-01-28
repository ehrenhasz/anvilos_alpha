#ifndef __DC_LINK_DP_IRQ_HANDLER_H__
#define __DC_LINK_DP_IRQ_HANDLER_H__
#include "link.h"
bool dp_parse_link_loss_status(
	struct dc_link *link,
	union hpd_irq_data *hpd_irq_dpcd_data);
bool dp_should_allow_hpd_rx_irq(const struct dc_link *link);
void dp_handle_link_loss(struct dc_link *link);
enum dc_status dp_read_hpd_rx_irq_data(
	struct dc_link *link,
	union hpd_irq_data *irq_data);
bool dp_handle_hpd_rx_irq(struct dc_link *link,
		union hpd_irq_data *out_hpd_irq_dpcd_data, bool *out_link_loss,
		bool defer_handling, bool *has_left_work);
#endif  
