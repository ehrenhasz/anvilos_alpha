 

#ifndef __DC_RESOURCE_DCE120_H__
#define __DC_RESOURCE_DCE120_H__

#include "core_types.h"

struct dc;
struct resource_pool;

struct resource_pool *dce120_create_resource_pool(
	uint8_t num_virtual_links,
	struct dc *dc);

#endif  

