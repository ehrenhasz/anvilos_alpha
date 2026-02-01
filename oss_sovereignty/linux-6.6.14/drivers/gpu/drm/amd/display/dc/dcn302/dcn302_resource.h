 

#ifndef _DCN302_RESOURCE_H_
#define _DCN302_RESOURCE_H_

#include "core_types.h"

extern struct _vcs_dpi_ip_params_st dcn3_02_ip;
extern struct _vcs_dpi_soc_bounding_box_st dcn3_02_soc;

struct resource_pool *dcn302_create_resource_pool(const struct dc_init_data *init_data, struct dc *dc);

void dcn302_update_bw_bounding_box(struct dc *dc, struct clk_bw_params *bw_params);

#endif  
