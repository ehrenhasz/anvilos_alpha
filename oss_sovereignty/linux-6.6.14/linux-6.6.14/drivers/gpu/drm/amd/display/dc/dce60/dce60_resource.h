#ifndef __DC_RESOURCE_DCE60_H__
#define __DC_RESOURCE_DCE60_H__
#include "core_types.h"
struct dc;
struct resource_pool;
struct resource_pool *dce60_create_resource_pool(
	uint8_t num_virtual_links,
	struct dc *dc);
struct resource_pool *dce61_create_resource_pool(
	uint8_t num_virtual_links,
	struct dc *dc);
struct resource_pool *dce64_create_resource_pool(
	uint8_t num_virtual_links,
	struct dc *dc);
#endif  
