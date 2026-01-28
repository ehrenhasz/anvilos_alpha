#ifndef __DC_RESOURCE_DCE80_H__
#define __DC_RESOURCE_DCE80_H__
#include "core_types.h"
struct dc;
struct resource_pool;
struct resource_pool *dce80_create_resource_pool(
	uint8_t num_virtual_links,
	struct dc *dc);
struct resource_pool *dce81_create_resource_pool(
	uint8_t num_virtual_links,
	struct dc *dc);
struct resource_pool *dce83_create_resource_pool(
	uint8_t num_virtual_links,
	struct dc *dc);
#endif  
