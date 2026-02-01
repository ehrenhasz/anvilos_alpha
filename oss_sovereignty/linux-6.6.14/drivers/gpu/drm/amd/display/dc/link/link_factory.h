 
#ifndef __LINK_FACTORY_H__
#define __LINK_FACTORY_H__
#include "link.h"
struct dc_link *link_create(const struct link_init_data *init_params);
void link_destroy(struct dc_link **link);

#endif  
