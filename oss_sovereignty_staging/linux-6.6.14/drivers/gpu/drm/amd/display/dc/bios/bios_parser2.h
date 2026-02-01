 

#ifndef __DAL_BIOS_PARSER2_H__
#define __DAL_BIOS_PARSER2_H__

struct dc_bios *firmware_parser_create(
	struct bp_init_data *init,
	enum dce_version dce_version);

#endif
