 

#ifndef __DAL_BIOS_PARSER_INTERFACE_H__
#define __DAL_BIOS_PARSER_INTERFACE_H__

#include "dc_bios_types.h"

struct bios_parser;

struct bp_init_data {
	struct dc_context *ctx;
	uint8_t *bios;
};

struct dc_bios *dal_bios_parser_create(
	struct bp_init_data *init,
	enum dce_version dce_version);

void dal_bios_parser_destroy(struct dc_bios **dcb);

#endif  
