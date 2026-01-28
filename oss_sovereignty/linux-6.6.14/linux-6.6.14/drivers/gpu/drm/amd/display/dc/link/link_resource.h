#ifndef __LINK_RESOURCE_H__
#define __LINK_RESOURCE_H__
#include "link.h"
void link_get_cur_res_map(const struct dc *dc, uint32_t *map);
void link_restore_res_map(const struct dc *dc, uint32_t *map);
void link_get_cur_link_res(const struct dc_link *link,
		struct link_resource *link_res);
#endif  
