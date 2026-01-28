#ifndef __DC_LINK_HPD_H__
#define __DC_LINK_HPD_H__
#include "link.h"
enum hpd_source_id get_hpd_line(struct dc_link *link);
bool program_hpd_filter(const struct dc_link *link);
bool dpia_query_hpd_status(struct dc_link *link);
bool query_hpd_status(struct dc_link *link, uint32_t *is_hpd_high);
bool link_get_hpd_state(struct dc_link *link);
struct gpio *link_get_hpd_gpio(struct dc_bios *dcb,
		struct graphics_object_id link_id,
		struct gpio_service *gpio_service);
void link_enable_hpd(const struct dc_link *link);
void link_disable_hpd(const struct dc_link *link);
void link_enable_hpd_filter(struct dc_link *link, bool enable);
#endif  
